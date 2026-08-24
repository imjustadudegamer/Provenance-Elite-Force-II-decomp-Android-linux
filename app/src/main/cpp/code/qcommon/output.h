//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/Shared/qcommon/output.h                                  $
// $Revision:: 10                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class DocFileOutput;

#ifndef __DOCFILEOUTPUT_H__
#define __DOCFILEOUTPUT_H__

#if defined ( GAME_DLL )
#include "../fgame/g_local.h"
#else
#include "../fgame/listener.h"
class EventArgDef;
#include "../fgame/class.h"
#endif

class DocFileOutput 
	{
	protected:
		FILE *fileptr;
		int typeFlag;

		int classCount;
		int eventCount;

	public:
		DocFileOutput();
		virtual ~DocFileOutput();

		virtual void OutputClasses(ClassDef *classlist);
		virtual void OutputClass(ClassDef *in_class);
		virtual void OutputEvents(ClassDef *in_class);
		virtual void OutputEvent(Event *ev);
		virtual void OutputArguments(Event *ev);
		virtual void OutputArgument(EventArgDef *evarg);

		virtual const char *GetExtension() { return "txt"; }

		void Write(const char *filename, ClassDef *classlist, int ptypeFlag);
	};

class HTMLDocFileOutput : public DocFileOutput
	{
	public:
		HTMLDocFileOutput();
		~HTMLDocFileOutput();

		void OutputClass(ClassDef *in_class);
		void OutputEvent(Event *ev);
		void OutputArgument(EventArgDef *evarg);

		void OutputClasses(ClassDef *classlist);

		void OutputRange(EventArgDef *evarg);

		const char *GetExtension() { return "html"; }
	};

class ToolDocFileOutput : public DocFileOutput
	{
	private:
		void PrintSubClasses(ClassDef *in_class, ClassDef *classlist); 
		bool randFlag;
		bool colorFlag;

	public:
		ToolDocFileOutput();
		~ToolDocFileOutput();

		void OutputClass(ClassDef *in_class);
		void OutputEvent(Event *ev);
		void OutputArgument(EventArgDef *evarg);

		void OutputClasses(ClassDef *classlist);

		void OutputRange(EventArgDef *evarg);

		const char *GetExtension() { return "txt"; }
	};

#endif 
