/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
#include "vk_local.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

vkContext_t vk;

static const uint32_t vk_apiVersions[] = {
	VK_API_VERSION_1_3,
	VK_API_VERSION_1_2,
	VK_API_VERSION_1_1,
	VK_API_VERSION_1_0
};

static const char *const vk_deviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

static const char *vk_requestedLayers[8];
static uint32_t vk_numRequestedLayers;
static VkDebugUtilsMessengerEXT vk_debugMessenger = VK_NULL_HANDLE;

static VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT types,
	const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData )
{
	const char *kind = "other";

	(void)userData;

	if ( types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) {
		kind = "validation";
	} else if ( types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ) {
		kind = "performance";
	}

	VK_Guard_Row( "VK:valid",
				  "kind=%s severity=%d code=%d \"%s\"",
				  kind,
				  (int)severity,
				  data->messageIdNumber,
				  data->pMessage ? data->pMessage : "" );

	return VK_FALSE;
}

void VK_Instance_SetLayers( const char *const *layers, int numLayers )
{
	int i;

	vk_numRequestedLayers = 0;

	for ( i = 0; layers && i < numLayers && i < 8; i++ ) {
		vk_requestedLayers[vk_numRequestedLayers++] = layers[i];
	}
}

qboolean VK_Instance_LayerAvailable( const char *name )
{
	VkLayerProperties *props;
	uint32_t count = 0;
	uint32_t i;
	qboolean found = qfalse;
	VkResult result;

	if ( !VK_Procs_Load() ) {
		return qfalse;
	}

	if ( !qvk.EnumerateInstanceLayerProperties ) {
		return qfalse;
	}

	result = qvk.EnumerateInstanceLayerProperties( &count, NULL );
	if ( result != VK_SUCCESS || count == 0 ) {
		return qfalse;
	}

	props = (VkLayerProperties *)malloc( sizeof( VkLayerProperties ) * count );
	if ( !props ) {
		return qfalse;
	}

	qvk.EnumerateInstanceLayerProperties( &count, props );

	for ( i = 0; i < count; i++ ) {
		if ( !strcmp( props[i].layerName, name ) ) {
			found = qtrue;
			break;
		}
	}

	free( props );

	return found;
}

void VK_Debug_Init( void )
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo;

	if ( !vk_numRequestedLayers || !qvk.CreateDebugUtilsMessengerEXT ) {
		return;
	}

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
								 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
							 | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
							 | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = VK_DebugCallback;

	if ( VK_Guard_Result( VK_GSITE_INSTANCE,
						  qvk.CreateDebugUtilsMessengerEXT( vk.instance, &createInfo, NULL, &vk_debugMessenger ),
						  "vkCreateDebugUtilsMessengerEXT" ) ) {
		VK_Guard_Row( "VK:valid", "messenger=armed layers=%u", vk_numRequestedLayers );
	}
}

void VK_Debug_Shutdown( void )
{
	if ( vk_debugMessenger != VK_NULL_HANDLE && qvk.DestroyDebugUtilsMessengerEXT ) {
		qvk.DestroyDebugUtilsMessengerEXT( vk.instance, vk_debugMessenger, NULL );
	}
	vk_debugMessenger = VK_NULL_HANDLE;
}

static void VK_ApiName( uint32_t v, char *out, size_t len )
{
	snprintf( out, len, "%u.%u.%u",
			  VK_API_VERSION_MAJOR( v ),
			  VK_API_VERSION_MINOR( v ),
			  VK_API_VERSION_PATCH( v ) );
}

qboolean VK_Instance_Create( const char *const *surfaceExtensions, int numSurfaceExtensions )
{
	VkApplicationInfo app;
	VkInstanceCreateInfo createInfo;
	const char *extensionNames[32];
	uint32_t numExtensions = 0;
	VkResult result = VK_ERROR_INITIALIZATION_FAILED;
	size_t i;
	size_t versionIndex;

	if ( !VK_Procs_Load() ) {
		return qfalse;
	}

	if ( qvk.EnumerateInstanceVersion ) {
		qvk.EnumerateInstanceVersion( &vk.instanceApiVersion );
		VK_Guard_Row( "VK:loader", "lib=%s runtime=%u.%u.%u",
					  VK_Procs_LoaderName(),
					  VK_API_VERSION_MAJOR( vk.instanceApiVersion ),
					  VK_API_VERSION_MINOR( vk.instanceApiVersion ),
					  VK_API_VERSION_PATCH( vk.instanceApiVersion ) );
	} else {
		vk.instanceApiVersion = VK_API_VERSION_1_0;
		VK_Guard_Row( "VK:loader", "lib=%s runtime=1.0.0 enumerate-version-absent",
					  VK_Procs_LoaderName() );
	}

	if ( surfaceExtensions ) {
		for ( i = 0; i < (size_t)numSurfaceExtensions && numExtensions < 30; i++ ) {
			extensionNames[numExtensions++] = surfaceExtensions[i];
		}
	}

	memset( &app, 0, sizeof( app ) );
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "provenanceef2";
	app.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
	app.pEngineName = "ef2";
	app.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
	app.apiVersion = VK_API_VERSION_1_0;

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &app;
	createInfo.enabledExtensionCount = numExtensions;
	createInfo.ppEnabledExtensionNames = extensionNames;
	createInfo.enabledLayerCount = vk_numRequestedLayers;
	createInfo.ppEnabledLayerNames = vk_requestedLayers;

	for ( versionIndex = 0; versionIndex < sizeof( vk_apiVersions ) / sizeof( vk_apiVersions[0] ); versionIndex++ ) {
		app.apiVersion = vk_apiVersions[versionIndex];
		result = qvk.CreateInstance( &createInfo, NULL, &vk.instance );
		if ( result == VK_SUCCESS ) {
			break;
		}
		VK_Guard_Hit( VK_GSITE_INSTANCE, "api-version request refused", result );
	}

	if ( result != VK_SUCCESS ) {
		return qfalse;
	}

	vk.instanceApiVersion = app.apiVersion;
	VK_Procs_ResolveInstance( vk.instance );

	{
		char apiName[32];
		VK_ApiName( vk.instanceApiVersion, apiName, sizeof( apiName ) );
		VK_Guard_Row( "VK:instance", "api=%s extensions=%u layers=%u", apiName, numExtensions,
					  vk_numRequestedLayers );
	}

	VK_Debug_Init();

	vk.initialized = qtrue;
	return qtrue;
}

void VK_Instance_Destroy( void )
{
	VK_Debug_Shutdown();

	if ( vk.instance && qvk.DestroyInstance ) {
		qvk.DestroyInstance( vk.instance, NULL );
	}
	vk.instance = VK_NULL_HANDLE;
	vk.initialized = qfalse;
	VK_Procs_Unload();
}

static qboolean VK_HasExtension( const vkPhysDevice_t *dev, const char *name )
{
	uint32_t i;

	for ( i = 0; i < dev->extensionCount; i++ ) {
		if ( !strcmp( dev->extensions[i].extensionName, name ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

static void VK_QueryDevice( vkPhysDevice_t *out )
{
	VkPhysicalDeviceProperties2 props2;
	VkPhysicalDeviceFeatures2 features2;
	VkExtensionProperties *extensions;
	VkPhysicalDeviceVulkan12Properties *p12;
	uint32_t count = 0;
	VkResult result;
	qboolean useCore12;
	qboolean hasExt;

	{
		VkPhysicalDevice device = out->device;

		memset( out, 0, sizeof( *out ) );
		out->device = device;
	}

	out->graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
	out->presentFamily = VK_QUEUE_FAMILY_IGNORED;

	if ( !qvk.GetPhysicalDeviceProperties ) {
		VK_Guard_Hit( VK_GSITE_DEVICE, "GetPhysicalDeviceProperties unresolved", VK_ERROR_INITIALIZATION_FAILED );
		return;
	}

	qvk.GetPhysicalDeviceProperties( out->device, &out->props );

	if ( vk.instanceApiVersion >= VK_API_VERSION_1_2 && qvk.GetPhysicalDeviceProperties2 ) {
		memset( &props2, 0, sizeof( props2 ) );
		props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		memset( &out->driverProps, 0, sizeof( out->driverProps ) );
		out->driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
		memset( &out->props12, 0, sizeof( out->props12 ) );
		out->props12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
		out->driverProps.pNext = &out->props12;
		props2.pNext = &out->driverProps;
		qvk.GetPhysicalDeviceProperties2( out->device, &props2 );
		p12 = out->driverProps.pNext;
		if ( props2.properties.apiVersion != 0 ) {
			out->props = props2.properties;
			out->hasDriverProps = qtrue;
			out->has12Props = ( p12 && p12->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES )
								  ? qtrue : qfalse;
		} else {
			memset( &out->driverProps, 0, sizeof( out->driverProps ) );
		}
	}

	memset( &features2, 0, sizeof( features2 ) );
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	useCore12 = ( out->props.apiVersion >= VK_API_VERSION_1_2 );
	hasExt = qfalse;

	if ( qvk.EnumerateDeviceExtensionProperties ) {
		result = qvk.EnumerateDeviceExtensionProperties( out->device, NULL, &count, NULL );
		if ( result == VK_SUCCESS && count > 0 ) {
			extensions = (VkExtensionProperties *)malloc( sizeof( VkExtensionProperties ) * count );
			if ( extensions ) {
				out->extensions = extensions;
				out->extensionCount = count;
				qvk.EnumerateDeviceExtensionProperties( out->device, NULL, &count, extensions );
				hasExt = VK_HasExtension( out, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME );
			}
		}
	}

	out->hasDescriptorIndexingExt = hasExt;

	if ( useCore12 && qvk.GetPhysicalDeviceFeatures2 ) {
		memset( &out->features12, 0, sizeof( out->features12 ) );
		out->features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features2.pNext = &out->features12;
		qvk.GetPhysicalDeviceFeatures2( out->device, &features2 );
		out->hasDescriptorIndexing = qtrue;

		out->diFeatures.descriptorBindingPartiallyBound = out->features12.descriptorBindingPartiallyBound;
		out->diFeatures.runtimeDescriptorArray = out->features12.runtimeDescriptorArray;
		out->diFeatures.shaderSampledImageArrayNonUniformIndexing =
			out->features12.shaderSampledImageArrayNonUniformIndexing;
	} else if ( hasExt && qvk.GetPhysicalDeviceFeatures2 ) {
		memset( &out->diFeatures, 0, sizeof( out->diFeatures ) );
		out->diFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		features2.pNext = &out->diFeatures;
		qvk.GetPhysicalDeviceFeatures2( out->device, &features2 );
		out->hasDescriptorIndexing = qtrue;
	}

	out->hasMultiDrawIndirect = ( features2.features.multiDrawIndirect != VK_FALSE ) ? qtrue : qfalse;
	out->hasUpdateAfterBind = ( out->diFeatures.descriptorBindingSampledImageUpdateAfterBind
								&& out->diFeatures.descriptorBindingVariableDescriptorCount )
								  ? qtrue : qfalse;
	out->hasVariableDescriptorCount =
		out->diFeatures.descriptorBindingVariableDescriptorCount ? qtrue : qfalse;

	out->supportsBindless = qfalse;
	if ( out->hasDescriptorIndexing ) {
		out->supportsBindless = ( out->diFeatures.descriptorBindingPartiallyBound
								  && out->diFeatures.runtimeDescriptorArray
								  && out->diFeatures.shaderSampledImageArrayNonUniformIndexing )
									? qtrue : qfalse;
	}
}

static int VK_DeviceScore( const vkPhysDevice_t *dev )
{
	if ( dev->graphicsFamily == VK_QUEUE_FAMILY_IGNORED ) {
		return -1;
	}

	if ( dev->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ) {
		return 1;
	}

	if ( dev->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
		return 3;
	}

	if ( dev->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
		return 4;
	}

	return 2;
}

static void VK_LogDevice( const vkPhysDevice_t *dev, int score, int chosen )
{
	char apiName[32];
	char driverInfo[96];

	VK_ApiName( dev->props.apiVersion, apiName, sizeof( apiName ) );

	if ( dev->hasDriverProps && dev->driverProps.driverInfo[0] ) {
		snprintf( driverInfo, sizeof( driverInfo ), "driver=%s/%s",
				  dev->driverProps.driverName, dev->driverProps.driverInfo );
	} else {
		snprintf( driverInfo, sizeof( driverInfo ), "driver=0x%x", dev->props.driverVersion );
	}

	VK_Guard_Row( "VK:device",
				  "%s name=\"%s\" type=%d api=%s %s mdi=%d di=%d bindless=%d score=%d",
				  chosen ? "chosen" : "cand",
				  dev->props.deviceName,
				  (int)dev->props.deviceType,
				  apiName,
				  driverInfo,
				  dev->hasMultiDrawIndirect ? 1 : 0,
				  dev->hasDescriptorIndexing ? 1 : 0,
				  dev->supportsBindless ? 1 : 0,
				  score );
}

static void VK_QueryFamilies( vkPhysDevice_t *dev )
{
	VkQueueFamilyProperties *families;
	VkBool32 supported = VK_FALSE;
	uint32_t count = 0;
	uint32_t family;

	dev->graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
	dev->presentFamily = VK_QUEUE_FAMILY_IGNORED;
	dev->hasPresent = qfalse;

	qvk.GetPhysicalDeviceQueueFamilyProperties( dev->device, &count, NULL );
	if ( count == 0 ) {
		return;
	}

	families = (VkQueueFamilyProperties *)malloc( sizeof( VkQueueFamilyProperties ) * count );
	if ( !families ) {
		return;
	}

	qvk.GetPhysicalDeviceQueueFamilyProperties( dev->device, &count, families );

	for ( family = 0; family < count; family++ ) {
		if ( dev->graphicsFamily == VK_QUEUE_FAMILY_IGNORED
			 && ( families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
			dev->graphicsFamily = family;
		}

		if ( vk.surface != VK_NULL_HANDLE && qvk.GetPhysicalDeviceSurfaceSupportKHR
			 && qvk.GetPhysicalDeviceSurfaceSupportKHR( dev->device, family, vk.surface, &supported ) == VK_SUCCESS
			 && supported == VK_TRUE ) {
			dev->hasPresent = qtrue;
			if ( dev->presentFamily == VK_QUEUE_FAMILY_IGNORED ) {
				dev->presentFamily = family;
			}
		}
	}

	free( families );

	if ( vk.surface == VK_NULL_HANDLE ) {
		dev->presentFamily = dev->graphicsFamily;
		dev->hasPresent = qfalse;
	}
}

int VK_Device_Select( void )
{
	VkPhysicalDevice *handles;
	vkPhysDevice_t *list;
	uint32_t numDevices = 0;
	uint32_t i;
	int best = -1;
	int bestScore = -1;
	int score;
	VkResult result;

	result = qvk.EnumeratePhysicalDevices( vk.instance, &numDevices, NULL );
	if ( result != VK_SUCCESS || numDevices == 0 ) {
		VK_Guard_Hit( VK_GSITE_DEVICE, "no physical device", result );
		return -1;
	}

	handles = (VkPhysicalDevice *)malloc( sizeof( VkPhysicalDevice ) * numDevices );
	list = (vkPhysDevice_t *)calloc( numDevices, sizeof( vkPhysDevice_t ) );
	if ( !handles || !list ) {
		free( handles );
		free( list );
		VK_Guard_Hit( VK_GSITE_DEVICE, "enumerate alloc failed", VK_ERROR_OUT_OF_HOST_MEMORY );
		return -1;
	}

	qvk.EnumeratePhysicalDevices( vk.instance, &numDevices, handles );

	for ( i = 0; i < numDevices; i++ ) {
		list[i].device = handles[i];
		VK_QueryDevice( &list[i] );
		VK_QueryFamilies( &list[i] );

		score = VK_DeviceScore( &list[i] );
		VK_LogDevice( &list[i], score, 0 );

		if ( score > bestScore ) {
			bestScore = score;
			best = (int)i;
		}
	}

	if ( best < 0 ) {
		VK_Guard_Hit( VK_GSITE_DEVICE,
					  "no capable device (graphics queue required; multiDrawIndirect never required)",
					  VK_ERROR_INITIALIZATION_FAILED );
		free( handles );
		free( list );
		return -1;
	}

	vk.physicalDevice = list[best].device;
	vk.deviceProps = list[best].props;
	vk.props12 = list[best].props12;
	vk.has12Props = list[best].has12Props;
	memset( &vk.memProps, 0, sizeof( vk.memProps ) );
	qvk.GetPhysicalDeviceMemoryProperties( vk.physicalDevice, &vk.memProps );
	vk.deviceApiVersion = list[best].props.apiVersion;
	vk.graphicsFamily = list[best].graphicsFamily;
	vk.presentFamily = list[best].presentFamily;
	vk.descriptorIndexing = list[best].hasDescriptorIndexing;
	vk.descriptorIndexingExt = list[best].hasDescriptorIndexingExt;
	vk.bindless = list[best].supportsBindless;
	vk.multiDrawIndirect = list[best].hasMultiDrawIndirect;
	vk.updateAfterBind = list[best].hasUpdateAfterBind;
	vk.variableDescriptorCount = list[best].hasVariableDescriptorCount;

	VK_LogDevice( &list[best], bestScore, 1 );

	if ( !vk.multiDrawIndirect ) {
		VK_Guard_Row( "VK:guard",
					  "site=quirk detail=\"multiDrawIndirect absent (Mali class) - not required, not enabled\" result=success(0) n=1" );
	}

	free( handles );
	free( list );

	return best;
}

qboolean VK_Device_Create( void )
{
	VkDeviceQueueCreateInfo queueInfos[2];
	VkDeviceCreateInfo createInfo;
	VkPhysicalDeviceFeatures2 features2;
	VkPhysicalDeviceDescriptorIndexingFeatures diFeatures;
	const char *enabled[8];
	uint32_t numEnabled = 0;
	float priority = 1.0f;
	uint32_t numQueues = 0;
	VkResult result;

	if ( vk.physicalDevice == VK_NULL_HANDLE ) {
		return qfalse;
	}

	memset( &queueInfos, 0, sizeof( queueInfos ) );
	queueInfos[numQueues].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfos[numQueues].queueFamilyIndex = vk.graphicsFamily;
	queueInfos[numQueues].queueCount = 1;
	queueInfos[numQueues].pQueuePriorities = &priority;
	numQueues++;

	if ( vk.surface != VK_NULL_HANDLE && vk.presentFamily != VK_QUEUE_FAMILY_IGNORED
		 && vk.presentFamily != vk.graphicsFamily ) {
		queueInfos[numQueues].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfos[numQueues].queueFamilyIndex = vk.presentFamily;
		queueInfos[numQueues].queueCount = 1;
		queueInfos[numQueues].pQueuePriorities = &priority;
		numQueues++;
	}

	enabled[numEnabled++] = vk_deviceExtensions[0];

	if ( vk.descriptorIndexingExt ) {
		enabled[numEnabled++] = vk_deviceExtensions[1];
	}

	memset( &diFeatures, 0, sizeof( diFeatures ) );
	diFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	diFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	diFeatures.runtimeDescriptorArray = VK_TRUE;
	diFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

	memset( &features2, 0, sizeof( features2 ) );
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	if ( vk.descriptorIndexing ) {
		features2.pNext = &diFeatures;
	}

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = &features2;
	createInfo.queueCreateInfoCount = numQueues;
	createInfo.pQueueCreateInfos = queueInfos;
	createInfo.enabledExtensionCount = numEnabled;
	createInfo.ppEnabledExtensionNames = enabled;

	result = qvk.CreateDevice( vk.physicalDevice, &createInfo, NULL, &vk.device );

	if ( result != VK_SUCCESS && vk.descriptorIndexing ) {
		VK_Guard_Row( "VK:guard", "site=device detail=\"descriptor-indexing chain refused, retrying without\" result=%s(%d) n=1",
					  VK_Guard_ResultName( result ), (int)result );

		features2.pNext = NULL;
		if ( vk.descriptorIndexingExt ) {
			numEnabled--;
		}
		createInfo.enabledExtensionCount = numEnabled;
		result = qvk.CreateDevice( vk.physicalDevice, &createInfo, NULL, &vk.device );
	}

	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_DEVICE, "vkCreateDevice", result );
		return qfalse;
	}

	VK_Procs_ResolveDevice( vk.device );

	qvk.GetDeviceQueue( vk.device, vk.graphicsFamily, 0, &vk.graphicsQueue );
	if ( vk.presentFamily != VK_QUEUE_FAMILY_IGNORED ) {
		qvk.GetDeviceQueue( vk.device, vk.presentFamily, 0, &vk.presentQueue );
	} else {
		vk.presentQueue = vk.graphicsQueue;
	}

	{
		char apiName[32];
		VK_ApiName( vk.deviceApiVersion, apiName, sizeof( apiName ) );
		VK_Guard_Row( "VK:device", "created name=\"%s\" api=%s graphics=%u present=%u queues=%u bindless=%d",
					  vk.deviceProps.deviceName,
					  apiName,
					  vk.graphicsFamily,
					  vk.presentFamily,
					  numQueues,
					  vk.bindless ? 1 : 0 );
	}

	return qtrue;
}

void VK_Device_Destroy( void )
{
	if ( vk.device && qvk.DestroyDevice ) {
		qvk.DeviceWaitIdle( vk.device );
		qvk.DestroyDevice( vk.device, NULL );
	}
	vk.device = VK_NULL_HANDLE;
}
