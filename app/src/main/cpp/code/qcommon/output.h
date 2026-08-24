
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
