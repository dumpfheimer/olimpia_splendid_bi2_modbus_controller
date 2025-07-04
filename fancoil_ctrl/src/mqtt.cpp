#include "mqtt.h"
#include "wifi_mgr_eeprom.h"

#define MQTT_USER_BUFFER_SIZE 128
#define MQTT_PASS_BUFFER_SIZE 128
#define TOPIC_BUFFER_SIZE 128
#define MESSAGE_BUFFER_SIZE 2048

WiFiClient wifiClient;
PubSubClient client(wifiClient);

String clientId;
char *topicBuffer;
char *messageBuffer;

boolean stateChanged = false;
unsigned long lastSend = 0;

JsonDocument doc;

void notifyStateChanged() {
    stateChanged = true;
}

void publishHelper(String *publishTopic, String *publishMessage, bool retain) {
    publishTopic->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
    publishMessage->toCharArray(messageBuffer, MESSAGE_BUFFER_SIZE);
    client.publish(topicBuffer, messageBuffer, retain);
}

void publishHelper(String publishTopic, String publishMessage, bool retain) {
    publishHelper(&publishTopic, &publishMessage, retain);
}

void sendMessageBufferTo(String publishTopic, bool retain) {
    String *ptr = &publishTopic;
    ptr->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
    client.publish(topicBuffer, messageBuffer, retain);
}

void subscribeHelper(String *subscribeTopic) {
    subscribeTopic->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
    client.subscribe(topicBuffer);
}

void subscribeHelper(String subscribeTopic) {
    subscribeHelper(&subscribeTopic);
}

void sendFancoilState(Fancoil *fancoil) {
    String addr = String(fancoil->getAddress());
    String state;

    switch (fancoil->getSyncState()) {
        case SyncState::HAPPY:
        case SyncState::WRITING:
            if (fancoil->ambientTemperatureIsValid()) {
                state = "online";
            } else {
                state = "offline";
            }
            break;
        default:
            state = "offline";
            break;
    }

    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/state/state", state, false);

    switch (fancoil->getSpeed()) {
        case FanSpeed::MAX:
            state = "high";
            break;
        case FanSpeed::NIGHT:
            state = "night";
            break;
        case FanSpeed::MIN:
            state = "low";
            break;
        case FanSpeed::AUTOMATIC:
            state = "auto";
            break;
    }
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/state", state, false);

    if (!fancoil->isOn()) {
        state = "off";
    } else if (fancoil->getMode() == Mode::FAN_ONLY) {
        state = "fan_only";
    } else if (fancoil->getMode() == Mode::COOLING) {
        state = "cool";
    } else if (fancoil->getMode() == Mode::HEATING) {
        state = "heat";
    } else {
        state = "auto";
    }
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/mode/state", state, false);

    if (!fancoil->isOn()) {
        state = "off";
    } else if (fancoil->getMode() == Mode::FAN_ONLY) {
        state = "fan";
    } else if (fancoil->ev1On() && fancoil->getMode() == Mode::COOLING) {
        state = "cooling";
    } else if (fancoil->ev1On() && fancoil->getMode() == Mode::HEATING) {
        state = "heating";
    } else {
        state = "idle";
    }
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/action/state", state, false);

    state = fancoil->isOn() ? "ON" : "OFF";
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/on_off/state", state, false);

    state = fancoil->isSwingOn() ? "ON" : "OFF";
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/swing/state", state, false);

    state = String(fancoil->getSetpoint());
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/state", state, false);

    state = String(fancoil->getAmbient());
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature/state", state, false);

#ifdef LOAD_AMBIENT_TEMP
    state = String(fancoil->getAmbientTemp());
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ambient_sensor/state", state, false);
#endif

#ifdef LOAD_WATER_TEMP
    state = String(fancoil->getWaterTemp());
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/water_sensor/state", state, false);
#endif

    state = fancoil->boilerOn() || fancoil->chillerOn() ? "ON" : "OFF";
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/is_consuming/state", state, false);

    state = fancoil->ev1On() ? "ON" : "OFF";
    publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ev1/state", state, false);
}

void sendFancoilStates() {
    stateChanged = false;
    LinkedFancoilListElement *fancoilLinkedList = getFirstFancoilListElement();
    while (fancoilLinkedList != nullptr && fancoilLinkedList->fancoil != nullptr) {
        sendFancoilState(fancoilLinkedList->fancoil);
        fancoilLinkedList = fancoilLinkedList->next;
        yield();
    }
    lastSend = millis();
}

void unconfigureHomeAssistantDevice(String addr, bool onlyExtra) {
    // purge configuration
    publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/on_off/config", "", true);
    publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/swing/config", "", true);
    publishHelper("homeassistant/select/" + clientId + "-" + addr + "/mode/config", "", true);
    publishHelper("homeassistant/select/" + clientId + "-" + addr + "/fan_speed/config", "", true);
    publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/setpoint/config", "", true);
    publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/ambient_temperature/config", "", true);
    if (!onlyExtra) publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/water_sensor/config", "", true);
    if (!onlyExtra) publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/ambient_sensor/config", "", true);
    if (!onlyExtra) publishHelper("homeassistant/binary_sensor/" + clientId + "-" + addr + "/is_consuming/config", "", true);
    if (!onlyExtra) publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/state/config", "", true);
}

void sendHomeAssistantConfiguration() {
    if (!client.connected()) return;

    // online
    publishHelper("homeassistant/binary_sensor/" + clientId + "/online/config",
                  "{\"~\": \"fancoil_ctrl/" + clientId + "/online\", \"name\": \"Fancoil controller " + clientId +
                  " online\", \"unique_id\": \"fancoil_" + clientId +
                  "_online\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                  clientId + "\", \"name\": \"Fancoil controller " + clientId + "\"}}", true);
    // IP
    publishHelper("homeassistant/sensor/" + clientId + "/ip/config",
                  "{\"~\": \"fancoil_ctrl/" + clientId + "/ip\", \"name\": \"Fancoil controller " + clientId +
                  " IP Address\", \"unique_id\": \"fancoil_" + clientId +
                  "_ip\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                  clientId + "\", \"name\": \"Fancoil controller " + clientId + "\", \"cu\": \"http://" +
                  WiFi.localIP().toString() + "/\"}}", true);


    for (uint8_t addr_i = 1; addr_i <= 32; addr_i++) {
        String addr = String(addr_i);
        Fancoil *fancoil = getFancoilByAddress(addr_i);
        if (fancoil != nullptr) {
            bool sendExtra = wifiMgrGetBoolConfig("HA_XTRA", false);

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/on_off/set");
            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/swing/set");
            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/mode/set");
            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/set");
            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/set");
            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature/set");
            if (sendExtra) {
                // on / off
                publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/on_off/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/on_off\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " on_off\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_on_off\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);

                publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/swing/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/swing\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " swing\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_swing\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);

                //mode
                publishHelper("homeassistant/select/" + clientId + "-" + addr + "/mode/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/mode\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " mode\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_mode\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr +
                              "\"}, \"options\": [\"heat\", \"cool\", \"fan_only\", \"auto\", \"off\"]}", true);

                //action
                publishHelper("homeassistant/select/" + clientId + "-" + addr + "/action/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/action\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " mode\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_mode\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr +
                              "\"}, \"options\": [\"heat\", \"cool\", \"fan_only\", \"idle\", \"off\"]}", true);

                //fan speed: auto, night, low, high
                publishHelper("homeassistant/select/" + clientId + "-" + addr + "/fan_speed/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " fan speed\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_fan_speed\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr +
                              "\"}, \"options\": [\"auto\", \"low\", \"high\", \"night\"]}", true);

                // setpoint
                publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/setpoint/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/setpoint\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " setpoint\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_setpoint\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr +
                              "\"}, \"unit_of_meas\": \"°C\"}", true);

                // ambient temp
                publishHelper("homeassistant/number/" + clientId + "-" + addr + "/ambient_temperature/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr +
                              "/ambient_temperature\", \"name\": \"Fancoil " + clientId + "-" + addr +
                              " ambient temperature\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_ambient_temperature\", " + "\"cmd_t\": \"~/set\", " +
                              "\"stat_t\": \"~/state\", \"retain\": \"false\", \"min\": 5, \"max\": 50, \"precision\": 0.1, \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr +
                              "\"}, \"unit_of_meas\": \"°C\"}", true);

#ifdef LOAD_AMBIENT_TEMP
                // ambient temp sensor
                publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/ambient_sensor/config",
                "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/ambient_sensor\", \"name\": \"Fancoil " + clientId + "-" + addr + " ambient sensor\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_ambient_sensor\", " + "\"cmd_t\": \"~/set\", " + "\"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"unit_of_meas\": \"°C\"}", true);
#else
                publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/ambient_sensor/config", "", true);
#endif


                // is consuming water
                publishHelper("homeassistant/binary_sensor/" + clientId + "-" + addr + "/is_consuming/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/is_consuming\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " is consuming\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_is_consuming\", \"stat_t\": \"~/state\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);


                // state: info text
                publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/state/config",
                              "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/state\", \"name\": \"Fancoil " +
                              clientId + "-" + addr + " state\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                              "_state\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                              clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
            } else {
                unconfigureHomeAssistantDevice(addr, true);
            }

#ifdef LOAD_WATER_TEMP
            // water temp sensor
            publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/water_sensor/config",
                          "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/water_sensor\", \"name\": \"Fancoil " +
                          clientId + "-" + addr + " water sensor\", \"unique_id\": \"fancoil_" + clientId + "_" + addr +
                          "_water_sensor\", " + "\"cmd_t\": \"~/set\", " +
                          "\"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" +
                          clientId + "_" + addr + "\", \"name\": \"Fancoil " + clientId + "-" + addr +
                          "\"}, \"unit_of_meas\": \"°C\"}", true);
#else
            publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/water_sensor/config", "", true);
#endif
	    // hvac
            doc["name"] = "Fancoil " + clientId + ":" + addr + "";
            doc["icon"] = "mdi:home-thermometer-outline";
            doc["send_if_off"] = "true";
            doc["unique_id"] = "hvac_" + clientId + "_" + addr;
            doc["availability_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/state/state";
            doc["payload_available"] = "online";
            doc["payload_not_available"] = "offline";
            doc["mode_command_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/mode/set";
            doc["mode_state_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/mode/state";
            doc["action_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/action/state";
	        JsonArray modes = doc["modes"].to<JsonArray>();
	        modes.add("heat");
	        modes.add("cool");
	        modes.add("off");
            //doc["modes"] = ["heat", "cool", "off"];
            doc["min_temp"] = "15";
            doc["max_temp"] = "30";
            doc["precision"] = 0.1;
            doc["retain"] = "false";
            doc["current_temperature_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature/state";
            doc["temperature_command_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/set";
            doc["temperature_state_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/state";
            doc["temp_step"] = "0.5";
            doc["fan_mode_command_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/set";
            doc["fan_mode_state_topic"] = "fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/state";
	        JsonArray fanModes = doc["fan_modes"];
	        fanModes.add("auto");
	        fanModes.add("high");
	        fanModes.add("low");
	        fanModes.add("night");
            //doc["fan_modes"] = "auto, high, low, night";
            JsonObject device  = doc["device"].to<JsonObject>();
            device["name"] = "Fancoil " + clientId + "-" + addr + "";
            //device["via_device"] = "Fancoil CTRL";
            device["identifiers"] = "fancoil_" + clientId + "_" + addr + "";

            const char *manufacturer = wifiMgrGetConfig("HA_MAN");
            if (manufacturer != nullptr) device["manufacturer"] = manufacturer;
            const char *model = wifiMgrGetConfig("HA_MOD");
            if (model != nullptr) device["model"] = model;
            device["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";

            serializeJson(doc, messageBuffer, MESSAGE_BUFFER_SIZE);
	        sendMessageBufferTo("homeassistant/climate/" + clientId + "-" + addr + "/config", true);
	        doc.clear();

            sendFancoilState(fancoil);
        } else {
        }
    }
}

void mqttHandleMessage(char *topic, byte *payload, unsigned int length) {
    String t = String(topic);
    String idAndRest = t.substring(t.indexOf("/") + 1);
    String id = idAndRest.substring(0, idAndRest.indexOf("/"));

    String addressAndRest = idAndRest.substring(idAndRest.indexOf("/") + 1);
    String address = addressAndRest.substring(0, addressAndRest.indexOf("/"));

    String topicNameAndRest = addressAndRest.substring(addressAndRest.indexOf("/") + 1);
    String topicName = topicNameAndRest.substring(0, topicNameAndRest.indexOf("/"));
    String cmd = topicNameAndRest.substring(topicNameAndRest.indexOf("/") + 1);

    if (cmd == "set") {
        debugPrintln("Received set command for addr " + address + " property " + topicName);
        Fancoil *f = getFancoilByAddress((int) address.toDouble());
        if (f != nullptr) {
            char *msg_ba = (char *) malloc(sizeof(char) * (length + 1));
            memcpy(msg_ba, (char *) payload, length);
            msg_ba[length] = 0;
            String msg = String(msg_ba);
            debugPrintln("Received set command for addr " + address + " property " + topicName + ": " + msg);
            if (topicName == "on_off") {
                f->setOn(isTrue(msg));
            } else if (topicName == "swing") {
                f->setSwing(isTrue(msg));
            } else if (topicName == "mode") {
                if (msg == "fan_only") {
                    f->setOn(true);
                    f->setMode(Mode::FAN_ONLY);
                } else if (msg == "heat") {
                    f->setOn(true);
                    f->setMode(Mode::HEATING);
                } else if (msg == "cool") {
                    f->setOn(true);
                    f->setMode(Mode::COOLING);
                } else if (msg == "auto") {
                    f->setOn(true);
                    f->setMode(Mode::AUTO);
                } else if (msg == "off") {
                    f->setOn(false);
                }
            } else if (topicName == "fan_speed") {
                if (msg == "auto" || msg == "automatic") {
                    f->setSpeed(FanSpeed::AUTOMATIC);
                } else if (msg == "low" || msg == "min") {
                    f->setSpeed(FanSpeed::MIN);
                } else if (msg == "high" || msg == "max") {
                    f->setSpeed(FanSpeed::MAX);
                } else if (msg == "night") {
                    f->setSpeed(FanSpeed::NIGHT);
                }
            } else if (topicName == "setpoint") {
                f->setSetpoint(msg.toDouble());
            } else if (topicName == "ambient_temperature") {
                f->setAmbient(msg.toDouble());
            } else if (topicName == "temps") {
                double setpoint = msg.substring(0, msg.indexOf(":")).toDouble();
                double ambient = msg.substring(msg.indexOf(":") + 1).toDouble();
                f->setAmbient(ambient);
                f->setSetpoint(setpoint);
            } else {
                debugPrint("Unknown topicName: " + topicName);
            }
            free(msg_ba);
            f->notifyHasValidState();
            f->forceWrite(100); // wait for 100 before force write to give more mqtt messages time to be received
        } else {
            debugPrint("No fancoil with address ");
            debugPrintln(((int) address.toDouble()));
        }
    }
}

unsigned long lastConnectTry = 0;

void mqttReconnect() {
    if ((millis() - lastConnectTry) > 5000) {
        if (!WiFi.isConnected()) return;
        if (client.connected()) return;

        lastConnectTry = millis();
        debugPrint("Reconnecting...");
        String lastWillTopic = "fancoil_ctrl/" + clientId + "/online/state";
        lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
#ifdef MQTT_USER
        bool connected = client.connect(clientIdCharArray, MQTT_USER, MQTT_PASS, topicBuffer, true, true, "OFF");
#else
        const char* user = wifiMgrGetConfig("MQTT_USER");
        const char* pass = wifiMgrGetConfig("MQTT_PASS");
        if (user == nullptr || pass == nullptr) {
            debugPrintln("No mqtt user and password");
            return;
        } else {
            debugPrintln("MQTT user");
            debugPrintln(user);
            debugPrintln("MQTT pass");
            debugPrintln(pass);
        }

        bool connected = client.connect(WiFi.getHostname(), user, pass, topicBuffer, true, true, "OFF");
#endif
        if (!connected) {
            debugPrint("failed, rc=");
            debugPrint(client.state());
        } else {
            debugPrint("success");
            sendHomeAssistantConfiguration();
            lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
            client.publish(topicBuffer, "ON", true);

            lastWillTopic = "fancoil_ctrl/" + clientId + "/ip/state";
            lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
            String ip = WiFi.localIP().toString();
            ip.toCharArray(messageBuffer, MESSAGE_BUFFER_SIZE);
            client.publish(topicBuffer, messageBuffer, true);
        }
    }
}

void setupMqtt() {
#ifdef MQTT_HOST
    client.setServer(MQTT_HOST, 1883);
#else
    const char *host = wifiMgrGetConfig("MQTT_HOST");
    debugPrint("mqtt host: ");
    debugPrintln(host);
    if (host != nullptr) {
        client.setServer(host, 1883);
#endif
        client.setCallback(mqttHandleMessage);

        topicBuffer = (char *) malloc(sizeof(char) * TOPIC_BUFFER_SIZE);
        messageBuffer = (char *) malloc(sizeof(char) * MESSAGE_BUFFER_SIZE);

        clientId = WiFi.macAddress();
        clientId.replace(":", "-");
        client.setBufferSize(MESSAGE_BUFFER_SIZE);

#ifndef MQTT_HOST
    }
#endif
}

void loopMqtt() {
    mqttReconnect();
    if (client.connected()) {
        client.loop();
        if (stateChanged || (millis() - lastSend) > 30000) sendFancoilStates();
    }
}
