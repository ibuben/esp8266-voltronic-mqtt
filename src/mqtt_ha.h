#pragma once

#include "inverter.h"

void mqttSetup();
void mqttLoop();
bool mqttConnected();
void mqttForceReconnect();
bool mqttPublishTelemetry(const Telemetry &t);
void mqttPublishOnline();
