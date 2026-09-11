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
#ifndef VK_VK_PROCS_H
#define VK_VK_PROCS_H

#include <vulkan/vulkan.h>

#include "../qcommon/q_shared.h"

typedef struct vkProcs_s {
	PFN_vkGetInstanceProcAddr	GetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr		GetDeviceProcAddr;

	PFN_vkCreateInstance		CreateInstance;
	PFN_vkEnumerateInstanceVersion			EnumerateInstanceVersion;
	PFN_vkEnumerateInstanceExtensionProperties	EnumerateInstanceExtensionProperties;
	PFN_vkEnumerateInstanceLayerProperties		EnumerateInstanceLayerProperties;
	PFN_vkDestroyInstance		DestroyInstance;
	PFN_vkEnumeratePhysicalDevices		EnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceProperties		GetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceProperties2		GetPhysicalDeviceProperties2;
	PFN_vkGetPhysicalDeviceFeatures			GetPhysicalDeviceFeatures;
	PFN_vkGetPhysicalDeviceFeatures2		GetPhysicalDeviceFeatures2;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties	GetPhysicalDeviceQueueFamilyProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties		GetPhysicalDeviceMemoryProperties;
	PFN_vkGetPhysicalDeviceFormatProperties		GetPhysicalDeviceFormatProperties;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR	GetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	GetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR	GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	GetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkEnumerateDeviceExtensionProperties	EnumerateDeviceExtensionProperties;
	PFN_vkCreateDevice			CreateDevice;
	PFN_vkDestroyDevice			DestroyDevice;
	PFN_vkDestroySurfaceKHR			DestroySurfaceKHR;

	PFN_vkCreateDebugUtilsMessengerEXT	CreateDebugUtilsMessengerEXT;
	PFN_vkDestroyDebugUtilsMessengerEXT	DestroyDebugUtilsMessengerEXT;
	PFN_vkGetDeviceQueue			GetDeviceQueue;
	PFN_vkQueueSubmit			QueueSubmit;
	PFN_vkQueueWaitIdle			QueueWaitIdle;
	PFN_vkDeviceWaitIdle			DeviceWaitIdle;
	PFN_vkCreateSwapchainKHR		CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR		DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR		GetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR		AcquireNextImageKHR;
	PFN_vkQueuePresentKHR			QueuePresentKHR;
	PFN_vkCreatePipelineCache		CreatePipelineCache;
	PFN_vkDestroyPipelineCache		DestroyPipelineCache;
	PFN_vkGetPipelineCacheData		GetPipelineCacheData;
	PFN_vkMergePipelineCaches		MergePipelineCaches;
	PFN_vkCreateFence			CreateFence;
	PFN_vkDestroyFence			DestroyFence;
	PFN_vkResetFences			ResetFences;
	PFN_vkWaitForFences			WaitForFences;
	PFN_vkCreateSemaphore			CreateSemaphore;
	PFN_vkDestroySemaphore			DestroySemaphore;
	PFN_vkCreateCommandPool			CreateCommandPool;
	PFN_vkDestroyCommandPool		DestroyCommandPool;
	PFN_vkAllocateCommandBuffers		AllocateCommandBuffers;
	PFN_vkFreeCommandBuffers		FreeCommandBuffers;
	PFN_vkCreateDescriptorPool		CreateDescriptorPool;
	PFN_vkDestroyDescriptorPool		DestroyDescriptorPool;
	PFN_vkCreateDescriptorSetLayout	CreateDescriptorSetLayout;
	PFN_vkDestroyDescriptorSetLayout	DestroyDescriptorSetLayout;
	PFN_vkCreateRenderPass			CreateRenderPass;
	PFN_vkDestroyRenderPass			DestroyRenderPass;
	PFN_vkCreateFramebuffer			CreateFramebuffer;
	PFN_vkDestroyFramebuffer		DestroyFramebuffer;
	PFN_vkCreateImageView			CreateImageView;
	PFN_vkDestroyImageView			DestroyImageView;
	PFN_vkCreateImage				CreateImage;
	PFN_vkDestroyImage				DestroyImage;
	PFN_vkAllocateMemory			AllocateMemory;
	PFN_vkFreeMemory				FreeMemory;
	PFN_vkMapMemory					MapMemory;
	PFN_vkUnmapMemory				UnmapMemory;
	PFN_vkInvalidateMappedMemoryRanges	InvalidateMappedMemoryRanges;
	PFN_vkGetImageMemoryRequirements	GetImageMemoryRequirements;
	PFN_vkGetBufferMemoryRequirements	GetBufferMemoryRequirements;
	PFN_vkBindImageMemory			BindImageMemory;
	PFN_vkBindBufferMemory			BindBufferMemory;
	PFN_vkCreateBuffer				CreateBuffer;
	PFN_vkDestroyBuffer				DestroyBuffer;
	PFN_vkCreateShaderModule		CreateShaderModule;
	PFN_vkDestroyShaderModule		DestroyShaderModule;
	PFN_vkCreateGraphicsPipelines	CreateGraphicsPipelines;
	PFN_vkDestroyPipeline			DestroyPipeline;
	PFN_vkCreatePipelineLayout		CreatePipelineLayout;
	PFN_vkDestroyPipelineLayout		DestroyPipelineLayout;
	PFN_vkCmdBeginRenderPass		CmdBeginRenderPass;
	PFN_vkCmdEndRenderPass			CmdEndRenderPass;
	PFN_vkCmdSetViewport			CmdSetViewport;
	PFN_vkCmdSetScissor				CmdSetScissor;
	PFN_vkCmdBindPipeline			CmdBindPipeline;
	PFN_vkCmdBindVertexBuffers		CmdBindVertexBuffers;
	PFN_vkCmdBindIndexBuffer		CmdBindIndexBuffer;
	PFN_vkCmdBindDescriptorSets		CmdBindDescriptorSets;
	PFN_vkCmdPushConstants			CmdPushConstants;
	PFN_vkCmdDraw					CmdDraw;
	PFN_vkCmdDrawIndexed			CmdDrawIndexed;
	PFN_vkCmdPipelineBarrier		CmdPipelineBarrier;
	PFN_vkCmdCopyBuffer				CmdCopyBuffer;
	PFN_vkCmdCopyBufferToImage		CmdCopyBufferToImage;
	PFN_vkCmdCopyImageToBuffer		CmdCopyImageToBuffer;
	PFN_vkCmdBlitImage				CmdBlitImage;
	PFN_vkBeginCommandBuffer		BeginCommandBuffer;
	PFN_vkEndCommandBuffer			EndCommandBuffer;
	PFN_vkResetCommandBuffer		ResetCommandBuffer;
	PFN_vkCreateSampler				CreateSampler;
	PFN_vkDestroySampler			DestroySampler;
	PFN_vkAllocateDescriptorSets	AllocateDescriptorSets;
	PFN_vkFreeDescriptorSets		FreeDescriptorSets;
	PFN_vkUpdateDescriptorSets		UpdateDescriptorSets;
	PFN_vkCreateBufferView			CreateBufferView;
} vkProcs_t;

extern vkProcs_t qvk;

qboolean VK_Procs_Load( void );
void VK_Procs_Unload( void );
void VK_Procs_ResolveGlobal( void );
void VK_Procs_ResolveInstance( VkInstance instance );
void VK_Procs_ResolveDevice( VkDevice device );
const char *VK_Procs_LoaderName( void );

#endif
