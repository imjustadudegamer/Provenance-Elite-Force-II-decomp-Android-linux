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
#include "vk_firstlight.h"
#include "vk_local.h"

static vkHomePathFn_t vk_homePathFn = NULL;
static int vk_firstLightState = 0;

void VK_SetHomePathFn( vkHomePathFn_t fn )
{
	vk_homePathFn = fn;
}

const char *VK_HomePath( void )
{
	if ( vk_homePathFn ) {
		return vk_homePathFn();
	}

	return NULL;
}

static void VK_FirstLightRow( const char *status, const char *detail )
{
	VK_Guard_Row( "VK:firstlight",
				  "status=%s device=\"%s\" bindless=%d mdi=%d frames-in-flight=%d %s",
				  status,
				  vk.deviceProps.deviceName[0] ? vk.deviceProps.deviceName : "none",
				  vk.bindless ? 1 : 0,
				  vk.multiDrawIndirect ? 1 : 0,
				  VK_FRAMES_IN_FLIGHT,
				  detail ? detail : "" );
}

qboolean VK_BringUp( int windowWidth, int windowHeight,
					 const char *const *surfaceExtensions, int numSurfaceExtensions )
{
	qboolean priorCrash = VK_Sentinel_Present();
	vkSwapchainPlan_t plan;

	vk_firstLightState = 0;

	if ( priorCrash ) {
		VK_Guard_Row( "VK:guard", "site=sentinel detail=\"prior bring-up crash marker present\" result=success(0) n=1" );
	}

	VK_Sentinel_Arm();

	if ( !VK_Instance_Create( surfaceExtensions, numSurfaceExtensions ) ) {
		VK_FirstLightRow( "fail", "stage=instance" );
		return qfalse;
	}
	vk_firstLightState = 1;

	if ( vk.surface == VK_NULL_HANDLE && !VK_Surface_Create() ) {
		VK_FirstLightRow( "fail", "stage=surface" );
		VK_Sentinel_Clear();
		VK_BringDown();
		return qfalse;
	}
	vk_firstLightState = 2;

	if ( VK_Device_Select() < 0 ) {
		VK_FirstLightRow( "fail", "stage=device-select" );
		VK_Sentinel_Clear();
		VK_BringDown();
		return qfalse;
	}
	vk_firstLightState = 3;

	if ( !VK_Device_Create() ) {
		VK_FirstLightRow( "fail", "stage=device-create" );
		VK_Sentinel_Clear();
		VK_BringDown();
		return qfalse;
	}
	vk_firstLightState = 4;

	if ( vk.surface != VK_NULL_HANDLE ) {
		if ( !VK_Swapchain_Plan( &plan, windowWidth, windowHeight ) || !VK_Swapchain_Create( &plan ) ) {
			VK_FirstLightRow( "fail", "stage=swapchain" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}
	}
	vk_firstLightState = 5;

	if ( !VK_PipelineCache_Open() ) {
		VK_FirstLightRow( "fail", "stage=pipeline-cache" );
		VK_Sentinel_Clear();
		VK_BringDown();
		return qfalse;
	}

	if ( vk.surface != VK_NULL_HANDLE ) {
		if ( !VK_Target_Init() ) {
			VK_FirstLightRow( "fail", "stage=target" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Frame_Init() ) {
			VK_FirstLightRow( "fail", "stage=frame" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Descriptor_Init() ) {
			VK_FirstLightRow( "fail", "stage=descriptor" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Texture_Init() ) {
			VK_FirstLightRow( "fail", "stage=texture" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Draw_Init() ) {
			VK_FirstLightRow( "fail", "stage=draw" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Vertex_Init() ) {
			VK_FirstLightRow( "fail", "stage=vertex" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Pipeline_Init() ) {
			VK_FirstLightRow( "fail", "stage=pipeline" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		/* The shade layer's SPIR-V modules, pipeline cache and descriptor
		   layout: the twin's run in R_Init, these need the device. */
		if ( !VK_Shade_Descriptors_Init() ) {
			VK_FirstLightRow( "fail", "stage=shade-descriptors" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}

		if ( !VK_Shade_Init() ) {
			VK_FirstLightRow( "fail", "stage=shade" );
			VK_Sentinel_Clear();
			VK_BringDown();
			return qfalse;
		}
	}

	if ( vk.surface != VK_NULL_HANDLE ) {
		VK_PipelineCache_Save();
	}

	VK_Sentinel_Clear();
	vk_firstLightState = 6;

	VK_FirstLightRow( "pass", "" );

	return qtrue;
}

void VK_BringDown( void )
{
	if ( vk.device != VK_NULL_HANDLE && qvk.DeviceWaitIdle ) {
		qvk.DeviceWaitIdle( vk.device );
	}

	VK_Draw_Shutdown();
	VK_Shade_Shutdown();
	VK_Shade_Descriptors_Shutdown();
	VK_Texture_Shutdown();
	VK_Pipeline_Shutdown();
	VK_Vertex_Shutdown();
	VK_Descriptor_Shutdown();
	VK_Frame_Shutdown();
	VK_Target_Shutdown();
	VK_PipelineCache_Close();
	VK_Swapchain_Destroy();
	VK_Surface_Destroy();
	VK_Device_Destroy();
	VK_Instance_Destroy();
	vk_firstLightState = 0;
}

int VK_FirstLight_Ready( void )
{
	return vk_firstLightState;
}
