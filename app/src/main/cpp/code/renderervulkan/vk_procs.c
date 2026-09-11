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
#include "vk_procs.h"
#include "vk_guard.h"

#include <dlfcn.h>
#include <string.h>

vkProcs_t qvk;

#ifdef __ANDROID__
#define VK_LOADER_SONAME "libvulkan.so"
static const char *vk_loaderCandidates[] = { "libvulkan.so", NULL };
#else
static const char *vk_loaderCandidates[] = { "libvulkan.so.1", "libvulkan.so", NULL };
#endif

static void *vk_loaderHandle;
static const char *vk_loaderName = "none";

qboolean VK_Procs_Load( void )
{
	int i;

	if ( vk_loaderHandle ) {
		return qtrue;
	}

	for ( i = 0; vk_loaderCandidates[i]; i++ ) {
		vk_loaderHandle = dlopen( vk_loaderCandidates[i], RTLD_NOW | RTLD_LOCAL );
		if ( vk_loaderHandle ) {
			vk_loaderName = vk_loaderCandidates[i];
			break;
		}
	}

	if ( !vk_loaderHandle ) {
		VK_Guard_Hit( VK_GSITE_LOADER, "dlopen failed", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	qvk.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym( vk_loaderHandle, "vkGetInstanceProcAddr" );
	if ( !qvk.GetInstanceProcAddr ) {
		VK_Guard_Hit( VK_GSITE_LOADER, "vkGetInstanceProcAddr missing", VK_ERROR_INITIALIZATION_FAILED );
		VK_Procs_Unload();
		return qfalse;
	}

	VK_Procs_ResolveGlobal();

	if ( !qvk.CreateInstance ) {
		VK_Guard_Hit( VK_GSITE_LOADER, "vkCreateInstance missing", VK_ERROR_INITIALIZATION_FAILED );
		VK_Procs_Unload();
		return qfalse;
	}

	return qtrue;
}

void VK_Procs_Unload( void )
{
	if ( vk_loaderHandle ) {
		dlclose( vk_loaderHandle );
		vk_loaderHandle = NULL;
	}
	vk_loaderName = "none";
	memset( &qvk, 0, sizeof( qvk ) );
}

const char *VK_Procs_LoaderName( void )
{
	return vk_loaderName;
}

void VK_Procs_ResolveGlobal( void )
{
	qvk.CreateInstance = (PFN_vkCreateInstance)qvk.GetInstanceProcAddr( VK_NULL_HANDLE, "vkCreateInstance" );
	qvk.EnumerateInstanceVersion =
		(PFN_vkEnumerateInstanceVersion)qvk.GetInstanceProcAddr( VK_NULL_HANDLE, "vkEnumerateInstanceVersion" );
	qvk.EnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)
		qvk.GetInstanceProcAddr( VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties" );
	qvk.EnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)
		qvk.GetInstanceProcAddr( VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties" );
}

void VK_Procs_ResolveInstance( VkInstance instance )
{
#define VK_PROC( field, name ) \
	qvk.field = (PFN_##name)qvk.GetInstanceProcAddr( instance, #name );

	VK_PROC( CreateInstance, vkCreateInstance );
	VK_PROC( EnumerateInstanceExtensionProperties, vkEnumerateInstanceExtensionProperties );
	VK_PROC( EnumerateInstanceLayerProperties, vkEnumerateInstanceLayerProperties );
	VK_PROC( EnumerateInstanceVersion, vkEnumerateInstanceVersion );
	VK_PROC( EnumeratePhysicalDevices, vkEnumeratePhysicalDevices );
	VK_PROC( GetPhysicalDeviceProperties, vkGetPhysicalDeviceProperties );
	VK_PROC( GetPhysicalDeviceProperties2, vkGetPhysicalDeviceProperties2 );
	VK_PROC( GetPhysicalDeviceFeatures, vkGetPhysicalDeviceFeatures );
	VK_PROC( GetPhysicalDeviceFeatures2, vkGetPhysicalDeviceFeatures2 );
	VK_PROC( GetPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties );
	VK_PROC( GetPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceMemoryProperties );
	VK_PROC( GetPhysicalDeviceFormatProperties, vkGetPhysicalDeviceFormatProperties );
	VK_PROC( GetPhysicalDeviceSurfaceSupportKHR, vkGetPhysicalDeviceSurfaceSupportKHR );
	VK_PROC( GetPhysicalDeviceSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceCapabilitiesKHR );
	VK_PROC( GetPhysicalDeviceSurfaceFormatsKHR, vkGetPhysicalDeviceSurfaceFormatsKHR );
	VK_PROC( GetPhysicalDeviceSurfacePresentModesKHR, vkGetPhysicalDeviceSurfacePresentModesKHR );
	VK_PROC( EnumerateDeviceExtensionProperties, vkEnumerateDeviceExtensionProperties );
	VK_PROC( CreateDevice, vkCreateDevice );
	VK_PROC( DestroyDevice, vkDestroyDevice );
	VK_PROC( DestroySurfaceKHR, vkDestroySurfaceKHR );

	qvk.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)qvk.GetInstanceProcAddr( instance, "vkGetDeviceProcAddr" );
}

void VK_Procs_ResolveDevice( VkDevice device )
{
	PFN_vkGetDeviceProcAddr gpa = qvk.GetDeviceProcAddr;

	if ( !gpa ) {
		return;
	}

#define VK_DPROC( field, name ) \
	qvk.field = (PFN_##name)gpa( device, #name );

	VK_DPROC( GetDeviceQueue, vkGetDeviceQueue );
	VK_DPROC( QueueSubmit, vkQueueSubmit );
	VK_DPROC( QueueWaitIdle, vkQueueWaitIdle );
	VK_DPROC( DeviceWaitIdle, vkDeviceWaitIdle );
	VK_DPROC( CreateSwapchainKHR, vkCreateSwapchainKHR );
	VK_DPROC( DestroySwapchainKHR, vkDestroySwapchainKHR );
	VK_DPROC( GetSwapchainImagesKHR, vkGetSwapchainImagesKHR );
	VK_DPROC( AcquireNextImageKHR, vkAcquireNextImageKHR );
	VK_DPROC( QueuePresentKHR, vkQueuePresentKHR );
	VK_DPROC( CreatePipelineCache, vkCreatePipelineCache );
	VK_DPROC( DestroyPipelineCache, vkDestroyPipelineCache );
	VK_DPROC( GetPipelineCacheData, vkGetPipelineCacheData );
	VK_DPROC( MergePipelineCaches, vkMergePipelineCaches );
	VK_DPROC( CreateFence, vkCreateFence );
	VK_DPROC( DestroyFence, vkDestroyFence );
	VK_DPROC( ResetFences, vkResetFences );
	VK_DPROC( WaitForFences, vkWaitForFences );
	VK_DPROC( CreateSemaphore, vkCreateSemaphore );
	VK_DPROC( DestroySemaphore, vkDestroySemaphore );
	VK_DPROC( CreateCommandPool, vkCreateCommandPool );
	VK_DPROC( DestroyCommandPool, vkDestroyCommandPool );
	VK_DPROC( AllocateCommandBuffers, vkAllocateCommandBuffers );
	VK_DPROC( FreeCommandBuffers, vkFreeCommandBuffers );
	VK_DPROC( BeginCommandBuffer, vkBeginCommandBuffer );
	VK_DPROC( EndCommandBuffer, vkEndCommandBuffer );
	VK_DPROC( ResetCommandBuffer, vkResetCommandBuffer );
}
