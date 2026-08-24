#include "gvPS2Audio.h"
#include "gvPS2Spu2.h"
#include "gvPS2Headset.h"
#include "gvPS2Eyetoy.h"

#if !defined(_PS2)
#error This file should only be used with the PlayStation2
#endif

GVBool gviHardwareStartup(void)
{
	#if !defined(GV_NO_PS2_SPU2)
	if(gviPS2Spu2Startup())
	#endif
	{
		#if !defined(GV_NO_PS2_HEADSET)
		if(gviPS2HeadsetStartup())
		#endif
		{
			#if !defined(GV_NO_PS2_EYETOY)
			if(gviPS2EyetoyStartup())
			#endif
			{
				return GVTrue;
			}
			#if !defined(GV_NO_PS2_HEADSET)
			gviPS2HeadsetCleanup();
			#endif
		}
		#if !defined(GV_NO_PS2_SPU2)
		gviPS2Spu2Cleanup();
		#endif
	}

	return GVFalse;
}

void gviHardwareCleanup(void)
{
	#if !defined(GV_NO_PS2_SPU2)
	gviPS2Spu2Cleanup();
	#endif
	#if !defined(GV_NO_PS2_HEADSET)
	gviPS2HeadsetCleanup();
	#endif
	#if !defined(GV_NO_PS2_EYETOY)
	gviPS2EyetoyCleanup();
	#endif
}

void gviHardwareThink(void)
{
	#if !defined(GV_NO_PS2_SPU2)
	gviPS2Spu2Think();
	#endif
	#if !defined(GV_NO_PS2_HEADSET)
	gviPS2HeadsetThink();
	#endif
}

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	int numDevices = 0;

	#if !defined(GV_NO_PS2_SPU2)
	numDevices += gviPS2Spu2ListDevices(devices + numDevices, maxDevices - numDevices, types);
	#endif
	#if !defined(GV_NO_PS2_HEADSET)
	numDevices += gviPS2HeadsetListDevices(devices + numDevices, maxDevices - numDevices, types);
	#endif
	#if !defined(GV_NO_PS2_EYETOY)
	numDevices += gviPS2EyetoyListDevices(devices + numDevices, maxDevices - numDevices, types);
	#endif

	return numDevices;
}

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	#if !defined(GV_NO_PS2_SPU2)
	if(deviceID == GVPS2Spu2DeviceID)
		return gviPS2Spu2NewDevice(deviceID, type);
	#endif

	#if !defined(GV_NO_PS2_EYETOY)
	if(deviceID & GVI_EYETOY_DEVICEID_BIT)
		return gviPS2EyetoyNewDevice(deviceID, type);
	#endif

	#if !defined(GV_NO_PS2_HEADSET)
	return gviPS2HeadsetNewDevice(deviceID, type);
	#endif

	return NULL;
}
