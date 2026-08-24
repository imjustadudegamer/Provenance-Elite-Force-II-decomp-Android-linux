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


#define PFL_WINDOWDAMAGE			1		
#define PFL_RETURN					2		
#define WFL_FIRERELEASED			1		
#define DAMAGETYPE_IMPACT			1		
#define DAMAGETYPE_RADIAL			2		
#define DAMAGETYPE_VISIBLE			4		

typedef struct projectileinfo_s
{
	char name[MAX_STRINGFIELD];
	char model[MAX_STRINGFIELD];
	int flags;
	float gravity;
	int damage;
	float radius;
	int visdamage;
	int damagetype;
	int healthinc;
	float push;
	float detonation;
	float bounce;
	float bouncefric;
	float bouncestop;
} projectileinfo_t;

typedef struct weaponinfo_s
{
	int valid;					
	int number;									
	char name[MAX_STRINGFIELD];
	char model[MAX_STRINGFIELD];
	int level;
	int weaponindex;
	int flags;
	char projectile[MAX_STRINGFIELD];
	int numprojectiles;
	float hspread;
	float vspread;
	float speed;
	float acceleration;
	vec3_t recoil;
	vec3_t offset;
	vec3_t angleoffset;
	float extrazvelocity;
	int ammoamount;
	int ammoindex;
	float activate;
	float reload;
	float spinup;
	float spindown;
	int	primarydangerous;		
	int	altdangerous;
	projectileinfo_t proj;						
} weaponinfo_t;

int BotSetupWeaponAI(void);
void BotShutdownWeaponAI(void);
int BotChooseBestFightWeapon(int weaponstate, int *inventory);
void BotGetWeaponInfo(int weaponstate, int weapon, weaponinfo_t *weaponinfo);
int BotLoadWeaponWeights(int weaponstate, char *filename);
int BotAllocWeaponState(void);
void BotFreeWeaponState(int weaponstate);
void BotResetWeaponState(int weaponstate);
