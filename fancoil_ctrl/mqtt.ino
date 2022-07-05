#ifdef MQTT_HOST
#include <PubSubClient.h>

#define MQTT_USER_BUFFER_SIZE 128
#define MQTT_PASS_BUFFER_SIZE 128
#define TOPIC_BUFFER_SIZE 128
#define MESSAGE_BUFFER_SIZE 512

WiFiClient wifiClient;
PubSubClient client(wifiClient);

String clientId;
char* topicBuffer;
char* messageBuffer;

char* clientIdCharArray;
char* mqttUserCharArray;
char* mqttPassCharArray;

boolean stateChanged = false;
unsigned long lastSend = 0;

void notifyStateChanged() {
  stateChanged = true;
}

void publishHelper(String* publishTopic, String* publishMessage, bool retain) {
  publishTopic->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
  publishMessage->toCharArray(messageBuffer, MESSAGE_BUFFER_SIZE);
  client.publish(topicBuffer, messageBuffer, retain);
}

void publishHelper(String publishTopic, String publishMessage, bool retain) {
  publishHelper(&publishTopic, &publishMessage, retain);
}

void subscribeHelper(String* subscribeTopic) {
  subscribeTopic->toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
  client.subscribe(topicBuffer);
}

void subscribeHelper(String subscribeTopic) {
  subscribeHelper(&subscribeTopic);
}

void sendFancoilState(Fancoil* fancoil) {
  String addr = String(fancoil->getAddress());
  String state;

  switch (fancoil->getSyncState()) {
    case SyncState::HAPPY:
    case SyncState::WRITING:
      if (fancoil->ambientTemperatureIsValid()) {
          state = "CONNECTED";
      } else {
        state = "INVALID_AMBIENT_TEMPERATURE";
      }
      break;
    default:
      state = "DISCONNECTED";
      break;
  }

  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/state/state", state, false);
    
  switch (fancoil->getSpeed()) {
    case FanSpeed::MAX:
      state = "max";
      break;
    case FanSpeed::NIGHT:
      state = "night";
      break;
    case FanSpeed::MIN:
      state = "min";
      break;
    case FanSpeed::AUTOMATIC:
      state = "auto";
      break;
  }
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/state", state, false);

  if (fancoil->isFanOnly()) {
      state = "fan_only";
  } else if (fancoil->getMode() == Mode::COOLING) {
      state = "cooling";
  } else {
      state = "heating";
  }
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/mode/state", state, false);

  state = fancoil->isOn() ? "ON" : "OFF";
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/on_off/state", state, false);

  state = fancoil->isSwingOn() ? "ON" : "OFF";
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/swing/state", state, false);
  
  state = String(fancoil->getSetpoint());
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/state", state, false);
  
  state = String(fancoil->getAmbient());
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature/state", state, false);
  
  state = fancoil->boilerOn() || fancoil->chillerOn() ? "ON" : "OFF";
  publishHelper("fancoil_ctrl/" + clientId + "/" + addr + "/is_consuming/state", state, false);
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
  
  LinkedFancoilListElement *fancoilLinkedList = getFirstFancoilListElement();

  // online
  publishHelper("homeassistant/binary_sensor/" + clientId + "/online/config",
  "{\"~\": \"fancoil_ctrl/" + clientId + "/online\", \"name\": \"Fancoil controller " + clientId + " online\", \"unique_id\": \"fancoil_" + clientId + "_online\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "\", \"name\": \"Fancoil controller " + clientId + "\"}}", true);
  // IP
  publishHelper("homeassistant/sensor/" + clientId + "/ip/config",
  "{\"~\": \"fancoil_ctrl/" + clientId + "/ip\", \"name\": \"Fancoil controller " + clientId + " IP Address\", \"unique_id\": \"fancoil_" + clientId + "_ip\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "\", \"name\": \"Fancoil controller " + clientId + "\", \"cu\": \"http://" + WiFi.localIP().toString() + "/\"}}", true);
  

  for (uint8_t addr_i = 0; addr_i <= 32; addr_i++) {
    bool hasAddr = false;
    String addr = String(addr_i);

    fancoilLinkedList = getFirstFancoilListElement();
    while (fancoilLinkedList != NULL && fancoilLinkedList->fancoil != NULL) {
      if (fancoilLinkedList->fancoil->getAddress() == addr_i) {
        hasAddr = true;
        break;
      }
      fancoilLinkedList = fancoilLinkedList->next;
    }
    if (!hasAddr) {
      publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/on_off/config", "", true);
      publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/swing/config", "", true);
      publishHelper("homeassistant/select/" + clientId + "-" + addr + "/mode/config", "", true);
      publishHelper("homeassistant/select/" + clientId + "-" + addr + "/fan_speed/config", "", true);
      publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/setpoint/config", "", true);
      publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/ambient_temperature/config", "", true);
      publishHelper("homeassistant/binary_sensor/" + clientId + "-" + addr + "/is_consuming/config", "", true);
      publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/state/config", "", true);
    }
  }

  fancoilLinkedList = getFirstFancoilListElement();
  while (fancoilLinkedList != NULL && fancoilLinkedList->fancoil != NULL) {
    String addr = String(fancoilLinkedList->fancoil->getAddress());

    // on / off
    publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/on_off/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/on_off\", \"name\": \"Fancoil " + clientId + "-" + addr + " on_off\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_on_off\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/on_off/set");
    
    publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/swing/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/swing\", \"name\": \"Fancoil " + clientId + "-" + addr + " swing\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_swing\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/swing/set");
  
    //mode: heating cooling
    publishHelper("homeassistant/select/" + clientId + "-" + addr + "/mode/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/mode\", \"name\": \"Fancoil " + clientId + "-" + addr + " mode\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_mode\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"options\": [\"heating\", \"cooling\", \"fan_only\"]}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/mode/set");
    
    //fan speed: auto, night, min, max
    publishHelper("homeassistant/select/" + clientId + "-" + addr + "/fan_speed/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed\", \"name\": \"Fancoil " + clientId + "-" + addr + " fan speed\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_fan_speed\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"options\": [\"auto\", \"min\", \"max\", \"night\"]}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed/set");
    
    // setpoint
    publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/setpoint/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/setpoint\", \"name\": \"Fancoil " + clientId + "-" + addr + " setpoint\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_setpoint\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"unit_of_meas\": \"°C\"}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/setpoint/set");
    
    // ambient temp
    publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/ambient_temperature/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature\", \"name\": \"Fancoil " + clientId + "-" + addr + " ambient temperature\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_ambient_temperature\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"unit_of_meas\": \"°C\"}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "/" + addr + "/ambient_temperature/set");
    
    // is consuming water
    publishHelper("homeassistant/binary_sensor/" + clientId + "-" + addr + "/is_consuming/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/is_consuming\", \"name\": \"Fancoil " + clientId + "-" + addr + " is consuming\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_is_consuming\", \"stat_t\": \"~/state\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
    
    
    // state: info text
    publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/state/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/state\", \"name\": \"Fancoil " + clientId + "-" + addr + " state\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_state\", \"stat_t\": \"~/state\", \"retain\": \"false\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);

    sendFancoilState(fancoilLinkedList->fancoil);
    fancoilLinkedList = fancoilLinkedList->next;
  }
}

void mqttHandleMessage(char* topic, byte* payload, unsigned int length) {
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
    Fancoil* f = getFancoilByAddress((int) id.toDouble());
    if (f != NULL) {
      char* msg_ba = (char*) malloc(sizeof(char) * (length + 1));
      memcpy(msg_ba, (char*) payload, length);
      msg_ba[length] = 0;
      String msg = String(msg_ba);
      debugPrintln("Received set command for addr " + address + " property " + topicName + ": " + msg);
      if (topicName == "on_off") {
        f->setOn(isTrue(msg));
      } else if (topicName == "swing") {
        f->setSwing(isTrue(msg));
      } else if (topicName == "mode") {
        if (msg == "heating") {
          f->setMode(Mode::HEATING);
          f->setFanOnly(false);
        } else if (msg == "cooling") {
          f->setMode(Mode::COOLING);
          f->setFanOnly(false);
        } else {
          f->setFanOnly(true);
        }
      } else if (topicName == "fan_speed") {
        if (msg == "auto") {
          f->setSpeed(FanSpeed::AUTOMATIC);
        } else if (msg == "min") {
          f->setSpeed(FanSpeed::MIN);
        } else if (msg == "max") {
          f->setSpeed(FanSpeed::MAX);
        } else if (msg == "night") {
          f->setSpeed(FanSpeed::NIGHT);
        }
      } else if (topicName == "setpoint") {
        f->setSetpoint(msg.toDouble());
      } else if (topicName == "ambient_temperature") {
        f->setAmbient(msg.toDouble());
      }
      free(msg_ba);
      notifyStateChanged();
    }
  }
}

void mqttReconnect() {
    while (!client.connected()) {
        debugPrint("Reconnecting...");
        String lastWillTopic = "fancoil_ctrl/" + clientId + "/online/state";
        lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
        if (!client.connect(clientIdCharArray, MQTT_USER, MQTT_PASS, topicBuffer, true, 1, "OFF")) {
            debugPrint("failed, rc=");
            debugPrint(client.state());
            debugPrintln(" retrying in 5 seconds");
            delay(5000);
        } else {
          debugPrint("success");
          sendHomeAssistantConfiguration();
          lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
          client.publish(topicBuffer, "ON");

          lastWillTopic = "fancoil_ctrl/" + clientId + "/ip/state";
          lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
          String ip = WiFi.localIP().toString();
          ip.toCharArray(messageBuffer, MESSAGE_BUFFER_SIZE);
          client.publish(topicBuffer, messageBuffer);
        }
    }
}

void setupMqtt() {
  client.setServer(MQTT_HOST, 1883);
  client.setCallback(mqttHandleMessage);
  
  topicBuffer = (char*) malloc(sizeof(char) * TOPIC_BUFFER_SIZE);
  messageBuffer = (char*) malloc(sizeof(char) * MESSAGE_BUFFER_SIZE);
  
  clientId = WiFi.macAddress();
  clientId.replace(":", "-");
  clientIdCharArray = (char*) malloc(sizeof(char) * (12+5));
  clientId.toCharArray(clientIdCharArray, (12+5));

  sendHomeAssistantConfiguration();
}

void loopMqtt() {
  mqttReconnect();
  if (client.connected()) {
    client.loop();
    if (stateChanged || (millis() - lastSend) > 30000) sendFancoilStates();
  }
}

#endif
