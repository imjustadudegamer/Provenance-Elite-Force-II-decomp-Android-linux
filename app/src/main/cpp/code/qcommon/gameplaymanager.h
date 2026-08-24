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
class GameplayManager;

#ifndef __GAMEPLAYMANAGER_H__
#define __GAMEPLAYMANAGER_H__

#ifdef GAME_DLL
#include "../fgame/g_local.h"
#include "../fgame/actor.h"
#include "../fgame/weapon.h"
#include "../fgame/player.h"
#include "../fgame/gamecmds.h"
#include "gameplayformulamanager.h"
#endif 

#ifdef CGAME_DLL
#include "cg_local.h"
#endif 


#include "gameplaydatabase.h"

class GameplayManager : public Listener
{
	public:
		static			GameplayManager* getTheGameplayManager();
		static void		shutdown();
		static bool		isReady();
		static void		create();

		bool			hasProperty(const str& objname, const str& propname);
		bool			hasObject(const str& objname);
		bool			hasSubObject(const str& objname, const str& subobject);
		bool			isDefined(const str& propname);

		const str		getDefine(const str& propname);
		float			getFloatValue(const str& objname, const str& propname);
		const str		getStringValue(const str& objname, const str& propname);

		void			setFloatValue(const str& objname, const str& propname, float value, bool create = false);
		void			setStringValue(const str& objname, const str& propname, const str& valuestr, bool create = false);
		bool			setBase(const str& objname, const str& baseobj);
		bool			clearPropertyOverrides(const str& objname);

	protected:
						GameplayManager();
		virtual		   ~GameplayManager();

	private:
		static GameplayManager *_theGameplayManager;		
		GameplayDatabase		_gameplayDatabase;

#ifdef GAME_DLL
	public:
		bool			hasFormula(const str& formulaName);
		float			calculate(const str& formulaName, const GameplayFormulaData& formulaData, float multiplier = 1.0f);

		void			processPendingMessages();
		void			Archive(Archiver &arc);
	private:
		GameplayFormulaManager _gameplayFormulaManager;
#endif 
};


#ifdef __cplusplus
extern "C" {
#endif

	qboolean HasGameplayProperty(const char *objectName, const char *propertyName);
	void     GetGameplayStringProperty(const char *objectName, const char *propertyName, char *buffer, int length);
	void     SetGameplayStringProperty(const char *objectName, const char *propertyName, const char *stringValue, qboolean create);
	float    GetGameplayFloatProperty(const char *objectName, const char *propertyName);
	void     SetGameplayFloatProperty(const char *objectName, const char *propertyName, float value, qboolean create);

	void     CreateGameplayManager(void);
	void     ShutdownGameplayManager(void);

#ifdef __cplusplus
}
#endif

#endif
