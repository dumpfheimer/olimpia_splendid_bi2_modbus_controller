//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_MQTT_H
#define FANCOIL_CTRL_MQTT_H

#include "configuration.h"

#ifdef MQTT_HOST

#include "Arduino.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "fancoil.h"
#include "httpHandlers.h"
#include "fancoil_manager.h"

void setupMqtt();
void loopMqtt();
void sendHomeAssistantConfiguration();
void unconfigureHomeAssistantDevice(String addr);
void notifyStateChanged();

#endif

#endif //FANCOIL_CTRL_MQTT_H
