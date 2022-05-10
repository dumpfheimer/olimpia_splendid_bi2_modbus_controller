#ifdef MQTT_HOST
#include <PubSubClient.h>

#define TOPIC_BUFFER_SIZE 128
#define MESSAGE_BUFFER_SIZE 512

WiFiClient wifiClient;
PubSubClient client(wifiClient);

String clientId;
char* topicBuffer;
char* messageBuffer;


void setupMqttListenTopics() {
  
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

void sendHomeAssistantConfiguration() {
  if (!client.connected()) return;
  
  LinkedFancoilListElement *fancoilLinkedList = getFirstFancoilListElement();

  // online
  publishHelper("homeassistant/binary_sensor/" + clientId + "/online/config",
  "{\"~\": \"fancoil_ctrl/" + clientId + "/online\", \"name\": \"Fancoil controller " + clientId + " online\", \"unique_id\": \"fancoil_" + clientId + "_online\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "\", \"name\": \"Fancoil controller " + clientId + "\"}}", true);
  // IP
  publishHelper("homeassistant/sensor/" + clientId + "/ip/config",
  "{\"~\": \"fancoil_ctrl/" + clientId + "/ip\", \"name\": \"Fancoil controller " + clientId + " IP Address\", \"unique_id\": \"fancoil_" + clientId + "_ip\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "\", \"name\": \"Fancoil controller " + clientId + "\"}}", true);
  

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
      publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/state/config", "", true);
    }
  }

  fancoilLinkedList = getFirstFancoilListElement();
  while (fancoilLinkedList != NULL && fancoilLinkedList->fancoil != NULL) {
    String addr = String(fancoilLinkedList->fancoil->getAddress());

    // on / off
    publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/on_off/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/on_off\", \"name\": \"Fancoil " + clientId + "-" + addr + " on_off\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_on_off\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "-" + addr + "/on_off/set");
    
    publishHelper("homeassistant/switch/" + clientId + "-" + addr + "/swing/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/swing\", \"name\": \"Fancoil " + clientId + "-" + addr + " swing\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_swing\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "-" + addr + "/swing/set");
  
    //mode: heating cooling
    publishHelper("homeassistant/select/" + clientId + "-" + addr + "/mode/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/mode\", \"name\": \"Fancoil " + clientId + "-" + addr + " mode\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_mode\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"options\": [\"heating\", \"cooling\"]}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "-" + addr + "/mode/set");
    
    //fan speed: auto, night, min, max
    publishHelper("homeassistant/select/" + clientId + "-" + addr + "/fan_speed/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/fan_speed\", \"name\": \"Fancoil " + clientId + "-" + addr + " fan speed\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_fan_speed\", \"cmd_t\": \"~/set\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}, \"options\": [\"auto\", \"min\", \"max\", \"night\"]}", true);
    subscribeHelper("fancoil_ctrl/" + clientId + "-" + addr + "/fan_speed/set");
    
    
    // state: info text
    publishHelper("homeassistant/sensor/" + clientId + "-" + addr + "/state/config",
    "{\"~\": \"fancoil_ctrl/" + clientId + "/" + addr + "/state\", \"name\": \"Fan coil " + clientId + "-" + addr + " state\", \"unique_id\": \"fancoil_" + clientId + "_" + addr + "_state\", \"stat_t\": \"~/state\", \"retain\": \"true\", \"device\": {\"identifiers\": \"fancoil_" + clientId + "_" + addr +"\", \"name\": \"Fancoil " + clientId + "-" + addr + "\"}}", true);
    
    fancoilLinkedList = fancoilLinkedList->next;
  }
}

void mqttHandleMessage(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  String idAndRest = t.substring(t.indexOf("-"));
  String id = idAndRest.substring(0, t.indexOf("/"));
  
  String topicNameAndRest = idAndRest.substring(t.indexOf("/"));
  String topicName = topicNameAndRest.substring(0, t.indexOf("/"));
  String cmd = topicNameAndRest.substring(t.indexOf("/"));

  if (cmd == "set") {
    Fancoil* f = getFancoilByAddress((int) id.toDouble());
    if (f != NULL) {
      char* msg_ba = (char*) malloc(sizeof(char) * (length + 1));
      memcpy(msg_ba, (char*) payload, length);
      msg_ba[length] = 0;
      String msg = String(msg_ba);
      if (topicName == "on_off") {
        f->setOn(isTrue(msg));
      } else if (topicName == "swing") {
        f->setSwing(isTrue(msg));
      } else if (topicName == "mode") {
        if (msg == "heating") {
          f->setMode(Mode::HEATING);
        } else if (msg == "cooling") {
          f->setMode(Mode::COOLING);
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
      }
      free(msg_ba);
    }
  }
}

void mqttReconnect() {
    while (!client.connected()) {
        debugPrint("Reconnecting...");
        String lastWillTopic = "fancoil_ctrl/" + clientId + "/online/state";
        lastWillTopic.toCharArray(topicBuffer, TOPIC_BUFFER_SIZE);
        if (!client.connect(MQTT_USER, MQTT_USER, MQTT_PASS, topicBuffer, true, 1, "OFF")) {
            debugPrint("failed, rc=");
            debugPrint(client.state());
            debugPrintln(" retrying in 5 seconds");
            delay(5000);
        } else {
          debugPrint("success");
          setupMqttListenTopics();
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
  
  topicBuffer = (char*) malloc(sizeof(char) * TOPIC_BUFFER_SIZE);
  messageBuffer = (char*) malloc(sizeof(char) * MESSAGE_BUFFER_SIZE);
  
  clientId = WiFi.macAddress();
  clientId.replace(":", "-");

  sendHomeAssistantConfiguration();
}

void loopMqtt() {
  mqttReconnect();
  client.loop();
}

#endif
