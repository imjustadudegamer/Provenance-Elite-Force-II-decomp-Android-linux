///////////////////////////////////////////////////////////////////////////////
#include "gvPS3Audio.h"
#include "gvPS3Headset.h"

#if !defined(_PS3)
#error This file should only be used with the PlayStation3
#endif

GVBool gviHardwareStartup(void)
{
	return gviPS3HeadsetStartup();
}

void gviHardwareCleanup(void)
{
	gviPS3HeadsetCleanup();
}

void gviHardwareThink(void)
{
	gviPS3HeadsetThink();
}

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	return gviPS3HeadsetListDevices(devices, maxDevices, types);
}

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	return gviPS3HeadsetNewDevice(deviceID, type);
}
