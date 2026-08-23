
#ifndef _GV_PS2_EYETOY_H_
#define _GV_PS2_EYETOY_H_

#include "gvMain.h"

#define GVI_EYETOY_DEVICEID_BIT              0x80000000

GVBool gviPS2EyetoyStartup(void);
void gviPS2EyetoyCleanup(void);

int gviPS2EyetoyListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types);

GVDevice gviPS2EyetoyNewDevice(GVDeviceID deviceID, GVDeviceType type);

#endif
