#include "mqtt.h"

#ifdef MQTT_HOST

#define MQTT_USER_BUFFER_SIZE 128
#define MQTT_PASS_BUFFER_SIZE 128
#define TOPIC_BUFFER_SIZE 128
#define MESSAGE_BUFFER_SIZE 2048

WiFiClient wifiClient;
PubSubClient client(wifiClient);

String clientId;
char *topicBuffer;
char *messageBuffer;

char *clientIdCharArray;
char *mqttUserCharArray;
char *mqttPassCharArray;

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
    while (fancoilLinkedList != NULL && fancoilLinkedList->fancoil != NULL) {
        sendFancoilState(fancoilLinkedList->fancoil);
        fancoilLinkedList = fancoilLinkedList->next;
    }
    lastSend = millis();
}

void sendHomeAssistantConfiguration() {
    if (!client.connected()) return;

    for (uint8_t addr_i = 1; addr_i <= 32; addr_i++) {
        String addr = String(addr_i);
        Fancoil *fancoil = getFancoilByAddress(addr_i);
        if (fancoil != NULL) {

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/on_off/set");

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/swing/set");

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/mode/set");

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/set");

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/set");

            subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature/set");

#ifdef LOAD_WATER_TEMP
            // water temp sensor
            publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/water_sensor/config",
                          "{\"unique_id\": \"fancoil_" + clientId + "_" + addr + "_water_sensor\",  \"state_topic\": \"fancoil_ctrl/" + clientId + "/" + addr + "/water_sensor/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"device_class\": \"temperature\", \"unit_of_meas\": \"°C\", \"suggested_display_precision\": \"0\"}",true);
#endif

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
        JsonObject device  = doc["device"].to<JsonObject>();
        device["name"] = "Fancoil " + clientId + "-" + addr + "";
        device["identifiers"] = "fancoil_" + clientId + "_" + addr + "";
	    #ifdef MODEL
        device["model"] = "Bi2 SL Smart 400";
        #else
        device["model"] = "Generic";
        #endif
        device["manufacturer"] = "Olimpia Splendid";
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
            } else {
                debugPrint("Unknown topicName: " + topicName);
            }
            free(msg_ba);
            notifyStateChanged();
            f->notifyHasValidState();
            f->forceWrite(100); // wait for 100 before force write to give more mqtt messages time to be received
        } else {
            debugPrint("No fancoil with address ");
            debugPrintln(((int) address.toDouble()));
        }
    }
}

void mqttReconnect() {
    while (!client.connected()) {
        if (!WiFi.isConnected()) return;
        debugPrint("Reconnecting...");
        String lastWillTopic = "fancoil_ctrl/" + clientId + "/online/state";
        lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
        if (!client.connect(clientIdCharArray, MQTT_USER, MQTT_PASS, topicBuffer, true, true, "OFF")) {
            debugPrint("failed, rc=");
            debugPrint(client.state());
            debugPrintln(" retrying in 5 seconds");
            delay(5000);
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
    client.setServer(MQTT_HOST, 1883);
    client.setCallback(mqttHandleMessage);

    topicBuffer = (char *) malloc(sizeof(char) * TOPIC_BUFFER_SIZE);
    messageBuffer = (char *) malloc(sizeof(char) * MESSAGE_BUFFER_SIZE);

    clientId = WiFi.macAddress();
    clientId.replace(":", "-");
    clientIdCharArray = (char *) malloc(sizeof(char) * (12 + 5));
    clientId.toCharArray(clientIdCharArray, (12 + 5));
    client.setBufferSize(MESSAGE_BUFFER_SIZE);
}

void loopMqtt() {
    mqttReconnect();
    if (client.connected()) {
        client.loop();
        if (stateChanged || (millis() - lastSend) > 30000) sendFancoilStates();
    }
}

#endif
