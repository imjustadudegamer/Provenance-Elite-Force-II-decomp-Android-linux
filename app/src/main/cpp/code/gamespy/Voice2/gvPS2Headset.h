
#ifndef _GV_PS2_HEADSET_H_
#define _GV_PS2_HEADSET_H_

#include "gvMain.h"

GVBool gviPS2HeadsetStartup(void);
void gviPS2HeadsetCleanup(void);
void gviPS2HeadsetThink(void);

int gviPS2HeadsetListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types);

GVDevice gviPS2HeadsetNewDevice(GVDeviceID deviceID, GVDeviceType type);

#endif
