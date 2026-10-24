#pragma once

#include "inverter.h"

void webSetup();
void webLoop();
void webStop();
void startWifiPortal(bool force);
bool wifiConnectOrPortal(bool forcePortal);

extern Telemetry telemetry;
