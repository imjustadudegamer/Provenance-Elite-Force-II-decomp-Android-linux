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


#define WT_BALANCE			1
#define MAX_WEIGHTS			128

typedef struct fuzzyseperator_s
{
	int index;
	int value;
	int type;
	float weight;
	float minweight;
	float maxweight;
	struct fuzzyseperator_s *child;
	struct fuzzyseperator_s *next;
} fuzzyseperator_t;

typedef struct weight_s
{
	char *name;
	struct fuzzyseperator_s *firstseperator;
} weight_t;

typedef struct weightconfig_s
{
	int numweights;
	weight_t weights[MAX_WEIGHTS];
	char		filename[MAX_QPATH];
} weightconfig_t;

weightconfig_t *ReadWeightConfig(char *filename);
void FreeWeightConfig(weightconfig_t *config);
qboolean WriteWeightConfig(char *filename, weightconfig_t *config);
int FindFuzzyWeight(weightconfig_t *wc, char *name);
float FuzzyWeight(int *inventory, weightconfig_t *wc, int weightnum);
float FuzzyWeightUndecided(int *inventory, weightconfig_t *wc, int weightnum);
void ScaleWeight(weightconfig_t *config, char *name, float scale);
void ScaleBalanceRange(weightconfig_t *config, float scale);
void EvolveWeightConfig(weightconfig_t *config);
void InterbreedWeightConfigs(weightconfig_t *config1, weightconfig_t *config2, weightconfig_t *configout);
void BotShutdownWeights(void);
