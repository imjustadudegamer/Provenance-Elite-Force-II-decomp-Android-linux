//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __BOTLIB_SHIM_H__
#define __BOTLIB_SHIM_H__

#include "../fgame/botlib.h"

#define g_gametype        botlib_g_gametype
#define CRC_Init          botlib_CRC_Init
#define CRC_ProcessByte   botlib_CRC_ProcessByte
#define CRC_Value         botlib_CRC_Value
#define crctable          botlib_crctable

// dedicated "jumped on the previous frame" latch: retail reuses ACTION_CROUCH

#endif
