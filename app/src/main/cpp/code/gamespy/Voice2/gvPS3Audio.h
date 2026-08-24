/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef _GV_PS3_AUDIO_H_
#define _GV_PS3_AUDIO_H_

#include "gvMain.h"

GVBool gviHardwareStartup(void);
void gviHardwareCleanup(void);
void gviHardwareThink(void);

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types);

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type);

#endif 
