
#ifndef _GV_PS2_SPU2_H_
#define _GV_PS2_SPU2_H_

#include "gvMain.h"

GVBool gviPS2Spu2Startup(void);
void gviPS2Spu2Cleanup(void);
void gviPS2Spu2Think(void);

int gviPS2Spu2ListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types);

GVDevice gviPS2Spu2NewDevice(GVDeviceID deviceID, GVDeviceType type);

#endif
