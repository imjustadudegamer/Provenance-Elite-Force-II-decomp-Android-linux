//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/uilib/console.h                                           $
// $Revision:: 2                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/10/01 10:29a                                                $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// String resource system
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef STRING_RESOURCE_HPP
#define STRING_RESOURCE_HPP


#include "../qcommon/q_shared.h"  
#include "../corepp/container.h" 
#include "../corepp/str.h" 


enum StringType { GLOBAL, LEVEL };
#define STRING_HASH_SIZE 1024


class StringEntry
{
	public:
		StringEntry()									{	}
		~StringEntry()									{	}
		
		str&			getString(void)					{ return _string;	}
		str&			getStringId(void)				{ return _stringId;	}
		StringType		getType(void)					{ return _type;		}
		StringEntry*	getNext(void)					{ return _next;		}

		void			setString(const str& string)	{ _string	= string;		}
		void			setStringId(const str& stringId){ _stringId = stringId;		}	
		void			setType(StringType type)		{ _type		= type;			}
		void			setNext(StringEntry* next)		{ _next		= next;			}
		
	private:
		str				_string;
		str				_stringId;
		StringType		_type;
		StringEntry*	_next;
};


class StringResource
{
	public:
		static void				createInstance(void);
		static void				deleteInstance(void);
		static StringResource*	getInstance(void)		{ if(_instance == 0) createInstance(); return _instance;	}

		str&					getStringResource(const str& stringId);

		void					convertStringResourceIds(str& string);
		void					parseStringId(str& stringId);
		void					loadGlobalStrings(void);
		void					loadLevelStrings(const str& levelName);
		void					clearAllStrings(void);
		void					clearLevelStrings(void);
		bool					addStringEntry(StringEntry* stringEntry);
		bool					loadStrings(const str& fileName, StringType stringType);
		void					parseStringValue(str& stringValue);

	protected:
		int						getHashValue(const str& stringId);

	private:
		StringResource();
		~StringResource();


		static StringResource*				_instance;
		StringEntry*						_stringHashTable[STRING_HASH_SIZE];
		str									_notFoundString;
		str									_loadedLevel;
		bool								_globalLoaded;
		bool								_overwrite;
		cvar_t*								_languageCvar;
};
#endif
