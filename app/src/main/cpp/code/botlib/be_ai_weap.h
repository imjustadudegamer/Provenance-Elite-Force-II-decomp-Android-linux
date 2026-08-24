/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


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
