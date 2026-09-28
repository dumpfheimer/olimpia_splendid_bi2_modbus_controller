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

// Discovery configs are retained by the broker, so they only need publishing
// once per boot (or when the fancoil registry changes). Re-publishing the
// full burst (~17 retained ~1KB messages per fancoil, faster than the broker
// ACKs them) on every reconnect piled up several KB of in-flight TCP data on
// the heap - measured as a 3.6KB free-heap low-water mark - exactly when
// reassociation needs memory most. Subscriptions, by contrast, die with the
// MQTT session and must be redone on every reconnect.
bool discoveryPending = true;

void mqttRequestDiscovery() {
    discoveryPending = true;
}

JsonDocument doc;

void notifyStateChanged() {
    stateChanged = true;
}

void publishHelper(String *publishTopic, String *publishMessage, bool retain) {
    // buffers exist only when MQTT is configured; without this guard an
    // unregister on an MQTT-less device crashes in toCharArray(nullptr,...)
    if (topicBuffer == nullptr || messageBuffer == nullptr) return;
    publishTopic->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
    publishMessage->toCharArray(messageBuffer, MESSAGE_BUFFER_SIZE);
    client.publish(topicBuffer, messageBuffer, retain);
}

void publishHelper(String publishTopic, String publishMessage, bool retain) {
    publishHelper(&publishTopic, &publishMessage, retain);
}

void sendMessageBufferTo(String publishTopic, bool retain) {
    if (topicBuffer == nullptr || messageBuffer == nullptr) return;
    String *ptr = &publishTopic;
    ptr->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
    client.publish(topicBuffer, messageBuffer, retain);
}

void subscribeHelper(String *subscribeTopic) {
    if (topicBuffer == nullptr) return;
    subscribeTopic->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
    client.subscribe(topicBuffer);
}

void subscribeHelper(String subscribeTopic) {
    subscribeHelper(&subscribeTopic);
}

// --- PROGMEM variants -------------------------------------------------------
//
// Two problems with building topics as String concatenations of literals:
// every literal is copied into RAM at boot (23KB of .rodata across the
// firmware, on a chip with 80KB), and every concatenation allocates
// temporaries on a heap whose low-water mark is already ~3.6KB - during the
// discovery burst, which is exactly when the WiFi stack also wants memory.
//
// These build straight into the buffers that already exist, from a format
// string that stays in flash. %s is clientId, %s is the fancoil address.
// Passing an unused second argument is harmless.
static void topicP(PGM_P fmt, const char *a1, const char *a2) {
    snprintf_P(topicBuffer, TOPIC_BUFFER_SIZE, fmt, a1, a2);
}

static void publishP(PGM_P topicFmt, const char *a1, const char *a2, const char *payload, bool retain) {
    if (topicBuffer == nullptr) return;
    topicP(topicFmt, a1, a2);
    client.publish(topicBuffer, payload, retain);
}

// publishes whatever sendHomeAssistantConfiguration() has already rendered
// into messageBuffer
static void publishBufferP(PGM_P topicFmt, const char *a1, const char *a2, bool retain) {
    if (topicBuffer == nullptr || messageBuffer == nullptr) return;
    topicP(topicFmt, a1, a2);
    client.publish(topicBuffer, messageBuffer, retain);
}

static void subscribeP(PGM_P topicFmt, const char *a1, const char *a2) {
    if (topicBuffer == nullptr) return;
    topicP(topicFmt, a1, a2);
    client.subscribe(topicBuffer);
}

void sendFancoilState(Fancoil *fancoil) {
    String addr = String(fancoil->getAddress());
    const char *cid = clientId.c_str();
    const char *ad = addr.c_str();
    String state;

    switch (fancoil->getSyncState()) {
        case SyncState::HAPPY:
        case SyncState::WRITING:
            // a unit that is not remote-enabled ACKs writes (sync looks
            // HAPPY) but ignores them - report it unavailable so HA shows
            // the truth. Deliberately self-clearing (unlike the latching
            // collision flag, which stays a badge so one past event cannot
            // pin the entity offline forever).
            if (fancoil->ambientTemperatureIsValid() && !fancoil->isLocalModeDetected()) {
                state = "online";
            } else {
                state = "offline";
            }
            break;
        default:
            state = "offline";
            break;
    }

    publishP(PSTR("fancoil_ctrl/%s/%s/state/state"), cid, ad, state.c_str(), false);

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
    publishP(PSTR("fancoil_ctrl/%s/%s/fan_speed/state"), cid, ad, state.c_str(), false);

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
    publishP(PSTR("fancoil_ctrl/%s/%s/mode/state"), cid, ad, state.c_str(), false);

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
    publishP(PSTR("fancoil_ctrl/%s/%s/action/state"), cid, ad, state.c_str(), false);

    state = fancoil->isOn() ? "ON" : "OFF";
    publishP(PSTR("fancoil_ctrl/%s/%s/on_off/state"), cid, ad, state.c_str(), false);

    state = fancoil->isSwingOn() ? "ON" : "OFF";
    publishP(PSTR("fancoil_ctrl/%s/%s/swing/state"), cid, ad, state.c_str(), false);

    state = String(fancoil->getSetpoint());
    publishP(PSTR("fancoil_ctrl/%s/%s/setpoint/state"), cid, ad, state.c_str(), false);

    state = String(fancoil->getAmbient());
    publishP(PSTR("fancoil_ctrl/%s/%s/ambient_temperature/state"), cid, ad, state.c_str(), false);

#ifdef LOAD_AMBIENT_TEMP
    state = String(fancoil->getAmbientTemp());
    publishP(PSTR("fancoil_ctrl/%s/%s/ambient_sensor/state"), cid, ad, state.c_str(), false);
#endif

#ifdef LOAD_WATER_TEMP
    state = String(fancoil->getWaterTemp());
    publishP(PSTR("fancoil_ctrl/%s/%s/water_sensor/state"), cid, ad, state.c_str(), false);
#endif

    state = fancoil->boilerOn() || fancoil->chillerOn() ? "ON" : "OFF";
    publishP(PSTR("fancoil_ctrl/%s/%s/is_consuming/state"), cid, ad, state.c_str(), false);

    state = fancoil->ev1On() ? "ON" : "OFF";
    publishP(PSTR("fancoil_ctrl/%s/%s/ev1/state"), cid, ad, state.c_str(), false);
}

void sendFancoilStates() {
    stateChanged = false;
    LinkedFancoilListElement *fancoilLinkedList = getFirstFancoilListElement();
    uint8_t walked = 0;
    while (fancoilLinkedList != nullptr && fancoilLinkedList->fancoil != nullptr && ++walked <= MAX_FANCOIL_LIST_WALK) {
        sendFancoilState(fancoilLinkedList->fancoil);
        fancoilLinkedList = fancoilLinkedList->next;
        yield();
    }
    lastSend = millis();
}

void unconfigureHomeAssistantDevice(String addr, bool onlyExtra) {
    const char *cid = clientId.c_str();
    const char *ad = addr.c_str();
    // purge configuration
    publishP(PSTR("homeassistant/switch/%s-%s/on_off/config"), cid, ad, "", true);
    publishP(PSTR("homeassistant/switch/%s-%s/swing/config"), cid, ad, "", true);
    publishP(PSTR("homeassistant/select/%s-%s/mode/config"), cid, ad, "", true);
    publishP(PSTR("homeassistant/select/%s-%s/fan_speed/config"), cid, ad, "", true);
    publishP(PSTR("homeassistant/sensor/%s-%s/setpoint/config"), cid, ad, "", true);
    publishP(PSTR("homeassistant/sensor/%s-%s/ambient_temperature/config"), cid, ad, "", true);
    if (!onlyExtra) publishP(PSTR("homeassistant/sensor/%s-%s/water_sensor/config"), cid, ad, "", true);
    if (!onlyExtra) publishP(PSTR("homeassistant/sensor/%s-%s/ambient_sensor/config"), cid, ad, "", true);
    if (!onlyExtra) publishP(PSTR("homeassistant/binary_sensor/%s-%s/is_consuming/config"), cid, ad, "", true);
    if (!onlyExtra) publishP(PSTR("homeassistant/sensor/%s-%s/state/config"), cid, ad, "", true);
}

// (re)subscribe to all fancoils' command topics - required on every MQTT
// (re)connect, since subscriptions are per-session
void subscribeFancoilTopics() {
    if (!client.connected()) return;
    LinkedFancoilListElement *e = getFirstFancoilListElement();
    uint8_t walked = 0;
    while (e != nullptr && e->fancoil != nullptr && ++walked <= MAX_FANCOIL_LIST_WALK) {
        String addr = String(e->fancoil->getAddress());
        const char *cid = clientId.c_str();
        const char *ad = addr.c_str();
        subscribeP(PSTR("fancoil_ctrl/%s/%s/on_off/set"), cid, ad);
        subscribeP(PSTR("fancoil_ctrl/%s/%s/swing/set"), cid, ad);
        subscribeP(PSTR("fancoil_ctrl/%s/%s/mode/set"), cid, ad);
        subscribeP(PSTR("fancoil_ctrl/%s/%s/fan_speed/set"), cid, ad);
        subscribeP(PSTR("fancoil_ctrl/%s/%s/setpoint/set"), cid, ad);
        subscribeP(PSTR("fancoil_ctrl/%s/%s/ambient_temperature/set"), cid, ad);
        e = e->next;
    }
}


// Every per-fancoil discovery config shares one skeleton; only the component,
// the topic leaf, the display suffix, whether it accepts commands and an
// optional tail differ. Rendering from a single PROGMEM template keeps one
// copy of that skeleton in flash instead of ten near-identical literals in
// RAM, and builds straight into messageBuffer - no String temporaries during
// the burst, which is when the heap bottoms out.
// `uid` is separate from `leaf` only because the "action" entity has always
// published unique_id "..._mode"; kept as-is so existing HA entities are not
// orphaned.
static void publishEntityConfigP(PGM_P component, PGM_P leaf, PGM_P uid, PGM_P suffix,
                                 const char *cid, const char *ad,
                                 bool commandable, PGM_P tail) {
    if (topicBuffer == nullptr || messageBuffer == nullptr) return;

    char comp[16], lf[24], ui[24], sfx[24], tl[112];
    strncpy_P(comp, component, sizeof(comp)); comp[sizeof(comp) - 1] = 0;
    strncpy_P(lf, leaf, sizeof(lf));          lf[sizeof(lf) - 1] = 0;
    strncpy_P(ui, uid, sizeof(ui));           ui[sizeof(ui) - 1] = 0;
    strncpy_P(sfx, suffix, sizeof(sfx));      sfx[sizeof(sfx) - 1] = 0;
    strncpy_P(tl, tail, sizeof(tl));          tl[sizeof(tl) - 1] = 0;

    snprintf_P(messageBuffer, MESSAGE_BUFFER_SIZE,
               PSTR("{\"~\": \"fancoil_ctrl/%s/%s/%s\", \"name\": \"Fancoil %s-%s %s\", "
                    "\"unique_id\": \"fancoil_%s_%s_%s\", %s\"stat_t\": \"~/state\", "
                    "\"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_%s_%s\", "
                    "\"name\": \"Fancoil %s-%s\"}%s}"),
               cid, ad, lf, cid, ad, sfx, cid, ad, ui,
               commandable ? "\"cmd_t\": \"~/set\", " : "",
               cid, ad, cid, ad, tl);

    snprintf_P(topicBuffer, TOPIC_BUFFER_SIZE, PSTR("homeassistant/%s/%s-%s/%s/config"), comp, cid, ad, lf);
    client.publish(topicBuffer, messageBuffer, true);
}

void sendHomeAssistantConfiguration() {
    if (!client.connected()) return;

    const char *cid0 = clientId.c_str();

    // online
    snprintf_P(messageBuffer, MESSAGE_BUFFER_SIZE,
               PSTR("{\"~\": \"fancoil_ctrl/%s/online\", \"name\": \"Fancoil controller %s online\", "
                    "\"unique_id\": \"fancoil_%s_online\", \"stat_t\": \"~/state\", \"retain\": \"false\", "
                    "\"device\": {\"identifiers\": \"fancoil_%s\", \"name\": \"Fancoil controller %s\"}}"),
               cid0, cid0, cid0, cid0, cid0);
    publishBufferP(PSTR("homeassistant/binary_sensor/%s/online/config"), cid0, "", true);

    // IP
    String ipStr = WiFi.localIP().toString();
    snprintf_P(messageBuffer, MESSAGE_BUFFER_SIZE,
               PSTR("{\"~\": \"fancoil_ctrl/%s/ip\", \"name\": \"Fancoil controller %s IP Address\", "
                    "\"unique_id\": \"fancoil_%s_ip\", \"stat_t\": \"~/state\", \"retain\": \"false\", "
                    "\"device\": {\"identifiers\": \"fancoil_%s\", \"name\": \"Fancoil controller %s\", "
                    "\"cu\": \"http://%s/\"}}"),
               cid0, cid0, cid0, cid0, cid0, ipStr.c_str());
    publishBufferP(PSTR("homeassistant/sensor/%s/ip/config"), cid0, "", true);


    for (uint8_t addr_i = 1; addr_i <= 32; addr_i++) {
        String addr = String(addr_i);
        const char *cid = clientId.c_str();
        const char *ad = addr.c_str();
        Fancoil *fancoil = getFancoilByAddress(addr_i);
        if (fancoil != nullptr) {
            bool sendExtra = wifiMgrGetBoolConfig("HA_XTRA", false);

            if (sendExtra) {
                publishEntityConfigP(PSTR("switch"), PSTR("on_off"), PSTR("on_off"), PSTR("on_off"),
                                     cid, ad, true, PSTR(""));
                publishEntityConfigP(PSTR("switch"), PSTR("swing"), PSTR("swing"), PSTR("swing"),
                                     cid, ad, true, PSTR(""));
                publishEntityConfigP(PSTR("select"), PSTR("mode"), PSTR("mode"), PSTR("mode"),
                                     cid, ad, true,
                                     PSTR(", \"options\": [\"heat\", \"cool\", \"fan_only\", \"auto\", \"off\"]"));
                publishEntityConfigP(PSTR("select"), PSTR("action"), PSTR("mode"), PSTR("mode"),
                                     cid, ad, false,
                                     PSTR(", \"options\": [\"heat\", \"cool\", \"fan_only\", \"idle\", \"off\"]"));
                publishEntityConfigP(PSTR("select"), PSTR("fan_speed"), PSTR("fan_speed"), PSTR("fan speed"),
                                     cid, ad, true,
                                     PSTR(", \"options\": [\"auto\", \"low\", \"high\", \"night\"]"));
                publishEntityConfigP(PSTR("sensor"), PSTR("setpoint"), PSTR("setpoint"), PSTR("setpoint"),
                                     cid, ad, true, PSTR(", \"unit_of_meas\": \"\u00b0C\""));
                publishEntityConfigP(PSTR("number"), PSTR("ambient_temperature"), PSTR("ambient_temperature"),
                                     PSTR("ambient temperature"), cid, ad, true,
                                     PSTR(", \"min\": 5, \"max\": 50, \"precision\": 0.1, \"unit_of_meas\": \"\u00b0C\""));
#ifdef LOAD_AMBIENT_TEMP
                publishEntityConfigP(PSTR("sensor"), PSTR("ambient_sensor"), PSTR("ambient_sensor"),
                                     PSTR("ambient sensor"), cid, ad, true, PSTR(", \"unit_of_meas\": \"\u00b0C\""));
#else
                publishP(PSTR("homeassistant/sensor/%s-%s/ambient_sensor/config"), cid, ad, "", true);
#endif
                publishEntityConfigP(PSTR("binary_sensor"), PSTR("is_consuming"), PSTR("is_consuming"),
                                     PSTR("is consuming"), cid, ad, false, PSTR(""));
                publishEntityConfigP(PSTR("sensor"), PSTR("state"), PSTR("state"), PSTR("state"),
                                     cid, ad, false, PSTR(""));
            } else {
                unconfigureHomeAssistantDevice(addr, true);
            }

#ifdef LOAD_WATER_TEMP
            publishEntityConfigP(PSTR("sensor"), PSTR("water_sensor"), PSTR("water_sensor"),
                                 PSTR("water sensor"), cid, ad, true, PSTR(", \"unit_of_meas\": \"\u00b0C\""));
#else
            publishP(PSTR("homeassistant/sensor/%s-%s/water_sensor/config"), cid, ad, "", true);
#endif
	    // hvac. Keys and literal values wrapped in F(): ArduinoJson copies
	    // from flash, whereas a bare literal would sit in RAM for the life of
	    // the firmware. Topic values are built with snprintf_P into a scratch
	    // buffer - declared char* (not const) so ArduinoJson copies it rather
	    // than storing a pointer into a buffer we immediately overwrite.
            char t[TOPIC_BUFFER_SIZE];
            #define TOPIC_P(fmt) (snprintf_P(t, sizeof(t), PSTR(fmt), cid, ad), (char *) t)

            doc[F("name")] = String(F("Fancoil ")) + clientId + F(":") + addr;
            doc[F("icon")] = F("mdi:home-thermometer-outline");
            doc[F("send_if_off")] = F("true");
            doc[F("unique_id")] = String(F("hvac_")) + clientId + F("_") + addr;
            doc[F("availability_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/state/state");
            doc[F("payload_available")] = F("online");
            doc[F("payload_not_available")] = F("offline");
            doc[F("mode_command_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/mode/set");
            doc[F("mode_state_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/mode/state");
            doc[F("action_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/action/state");
            JsonArray modes = doc[F("modes")].to<JsonArray>();
            modes.add(F("heat"));
            modes.add(F("cool"));
            modes.add(F("off"));
            doc[F("min_temp")] = F("15");
            doc[F("max_temp")] = F("30");
            doc[F("precision")] = 0.1;
            doc[F("retain")] = F("false");
            doc[F("current_temperature_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/ambient_temperature/state");
            doc[F("temperature_command_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/setpoint/set");
            doc[F("temperature_state_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/setpoint/state");
            doc[F("temp_step")] = F("0.5");
            doc[F("fan_mode_command_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/fan_speed/set");
            doc[F("fan_mode_state_topic")] = TOPIC_P("fancoil_ctrl/%s/%s/fan_speed/state");
            JsonArray fanModes = doc[F("fan_modes")].to<JsonArray>();
            fanModes.add(F("auto"));
            fanModes.add(F("high"));
            fanModes.add(F("low"));
            fanModes.add(F("night"));
            JsonObject device = doc[F("device")].to<JsonObject>();
            device[F("name")] = String(F("Fancoil ")) + clientId + F("-") + addr;
            device[F("identifiers")] = String(F("fancoil_")) + clientId + F("_") + addr;

            const char *manufacturer = wifiMgrGetConfig("HA_MAN");
            if (manufacturer != nullptr) device[F("manufacturer")] = manufacturer;
            const char *model = wifiMgrGetConfig("HA_MOD");
            if (model != nullptr) device[F("model")] = model;
            device[F("configuration_url")] = String(F("http://")) + WiFi.localIP().toString() + F("/");

            serializeJson(doc, messageBuffer, MESSAGE_BUFFER_SIZE);
            publishBufferP(PSTR("homeassistant/climate/%s-%s/config"), cid, ad, true);
            doc.clear();
            #undef TOPIC_P

            sendFancoilState(fancoil);

            // let the broker ACK the burst before the next fancoil's configs,
            // so in-flight TCP data doesn't accumulate on the heap
            client.loop();
            delay(20);
            client.loop();
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
            if (msg_ba == nullptr) return; // OOM: drop the message, not the device
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
            // Only messages that carry the on/off intent may validate the
            // desired state for writing. After a reboot the desired state is
            // a factory default (off); if a mere ambient/setpoint update could
            // mark it valid, the first write after boot would switch off a
            // running unit before the real mode/on_off command is parsed.
            if (topicName == "on_off" || topicName == "mode") {
                f->notifyHasValidState();
            }
            f->forceWrite(100); // wait for 100 before force write to give more mqtt messages time to be received
        } else {
            debugPrint("No fancoil with address ");
            debugPrintln(((int) address.toDouble()));
        }
    }
}

unsigned long lastConnectTry = 0;
// Retry interval, grown on each failure. client.connect() BLOCKS - up to the
// WiFiClient timeout for the TCP handshake, then up to the PubSubClient socket
// timeout waiting for CONNACK - and nothing else runs meanwhile: no web server,
// no fancoil loop. With the interval measured from the START of the attempt, a
// 5s connect already consumed the 5s gap, so a dead broker put the device back
// into connect() immediately and kept it there ~permanently. Timing the gap
// from the END of the attempt plus backing off keeps the controller responsive
// and the fancoils serviced while the broker is away.
unsigned long mqttRetryInterval = 5000;
#define MQTT_RETRY_MIN 5000
#define MQTT_RETRY_MAX 60000

void mqttReconnect() {
    if ((millis() - lastConnectTry) > mqttRetryInterval) {
        if (!WiFi.isConnected()) return;
        if (client.connected()) return;

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
            // no credentials is a configuration state, not a transient
            // failure: re-arm the timer so this does not re-run (and re-log)
            // on every single main loop pass
            lastConnectTry = millis();
            return;
        } else {
            debugPrintln("MQTT user");
            debugPrintln(user);
            // intentionally not logging the MQTT password
        }

        bool connected = client.connect(WiFi.getHostname(), user, pass, topicBuffer, true, true, "OFF");
#endif
        // measured from here: the attempt above blocked for an unknown time
        lastConnectTry = millis();

        if (!connected) {
            debugPrint("failed, rc=");
            debugPrint(client.state());
            mqttRetryInterval *= 2;
            if (mqttRetryInterval > MQTT_RETRY_MAX) mqttRetryInterval = MQTT_RETRY_MAX;
        } else {
            debugPrint("success");
            mqttRetryInterval = MQTT_RETRY_MIN;
            subscribeFancoilTopics();
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

bool mqttConfigured = false;

bool mqttIsConfigured() {
    return mqttConfigured;
}

bool mqttIsConnected() {
    return client.connected();
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
        mqttConfigured = true;
        client.setCallback(mqttHandleMessage);

        topicBuffer = (char *) malloc(sizeof(char) * TOPIC_BUFFER_SIZE);
        messageBuffer = (char *) malloc(sizeof(char) * MESSAGE_BUFFER_SIZE);

        if (topicBuffer == nullptr || messageBuffer == nullptr) {
            // without buffers every publish would crash; behave as if MQTT
            // were unconfigured (the helpers also guard against null)
            mqttConfigured = false;
        }

        clientId = WiFi.macAddress();
        clientId.replace(":", "-");
        client.setBufferSize(MESSAGE_BUFFER_SIZE);

        // Bound how long a failing connect can freeze the main loop. Defaults
        // are 5s for the TCP handshake (WiFiClient) and 15s waiting for
        // CONNACK (MQTT_SOCKET_TIMEOUT) - up to 20s per attempt during which
        // the web server and the fancoil state machines do not run at all. A
        // broker on the LAN answers in milliseconds; anything slower is a
        // failure worth reporting quickly rather than waiting out.
        wifiClient.setTimeout(1500);
        client.setSocketTimeout(3);

#ifndef MQTT_HOST
    }
#endif
}

void loopMqtt() {
    mqttReconnect();
    if (client.connected()) {
        // client.loop() dispatches at most one incoming packet per call, and
        // a full fancoil pass (with blocking modbus transactions) runs between
        // loopMqtt() calls. Drain the entire burst here so the fancoil state
        // machine never acts on a half-applied command group (e.g. mode
        // parsed, ambient still buffered). Bounded to keep a message flood
        // from starving the fancoil loop.
        uint8_t drained = 0;
        do {
            client.loop();
        } while (wifiClient.available() && ++drained < 32);
        if (discoveryPending) {
            subscribeFancoilTopics();
            sendHomeAssistantConfiguration();
            discoveryPending = false;
        }
        if (stateChanged || (millis() - lastSend) > 30000) sendFancoilStates();
    }
}
