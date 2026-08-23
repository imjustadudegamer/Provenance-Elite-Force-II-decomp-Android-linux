
#ifndef _GV_PSP_H_
#define _GV_PSP_H_

#include "gvMain.h"

GVBool gviHardwareStartup(void);
void gviHardwareCleanup(void);
void gviHardwareThink(void);

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types);

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type);

#endif
