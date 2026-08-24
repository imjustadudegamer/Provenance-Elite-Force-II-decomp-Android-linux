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


int BotLoadCharacter(char *charfile, float skill);
void BotFreeCharacter(int character);
float Characteristic_Float(int character, int index);
float Characteristic_BFloat(int character, int index, float min, float max);
int Characteristic_Integer(int character, int index);
int Characteristic_BInteger(int character, int index, int min, int max);
void Characteristic_String(int character, int index, char *buf, int size);
void BotShutdownCharacters(void);
