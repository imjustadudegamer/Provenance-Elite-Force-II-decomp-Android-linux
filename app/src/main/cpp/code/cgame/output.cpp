


#include <cgame/cg_local.h>
#include "listener.h"
#include "class.h"
#include <qcommon/output.h>
#include "../corepp/str.h"

DocFileOutput::DocFileOutput()
{
	typeFlag = 0;
	fileptr = NULL;
	classCount = 0;
	eventCount = 0;
}


DocFileOutput::~DocFileOutput()
{
	
}


void DocFileOutput::OutputClasses(ClassDef *classlist)
{
	ClassDef *c;
	int num = 0;
	
	for( c = classlist->next; c != classlist; c = c->next )
	{
		if ( num < MAX_CLASSES )
		{
			OutputClass(c);
			num++;
		}
	}
}

void DocFileOutput::OutputClass(ClassDef *in_class)
{
	OutputEvents(in_class);
	classCount++;
}

void DocFileOutput::OutputEvents(ClassDef *in_class)
{
	ResponseDef<Class> *r;
	int			ev, i, num;
	Event       **events;
	
	num = Event::NumEventCommands();
	
	events = new Event *[num];
	memset( events, 0, sizeof( Event * ) * num );
	
	r = in_class->responses;
	if ( r )
	{
		for( i = 0; r[i].event != NULL; i++ )
		{
			ev = ( int )*r[i].event;
			if ( r[i].response )
			{
				events[ev] = r[i].event;
			}
		}
	}
	
	for( i = 1; i < num; i++ )
	{
		int index;
		
		index = Event::MapSortedEventIndex( i );
		if ( events[index] )
		{
			
			if ( events[index]->GetFlags() != 0 )
			{
				
				if ( ( events[index]->GetFlags() & EV_TIKIONLY ) && typeFlag == EVENT_SCRIPT_ONLY && !( events[index]->GetFlags() & EV_SCRIPTONLY ) )
					continue;
				
				if ( ( events[index]->GetFlags() & EV_SCRIPTONLY ) && typeFlag == EVENT_TIKI_ONLY && !( events[index]->GetFlags() & EV_TIKIONLY ) )
					continue;
				
				if ( ( events[index]->GetFlags() & EV_CODEONLY ) && ( typeFlag == EVENT_TIKI_ONLY || typeFlag == EVENT_SCRIPT_ONLY ) )
					continue;
				
				if ( ( events[index]->GetFlags() & EV_CHEAT ) && ( typeFlag == EVENT_TIKI_ONLY ) && !( events[index]->GetFlags() & EV_TIKIONLY ) )
					continue;
				
				if ( ( events[index]->GetFlags() & EV_CHEAT ) && ( typeFlag == EVENT_SCRIPT_ONLY ) && !( events[index]->GetFlags() & EV_SCRIPTONLY ) )
					continue;
				
				if ( ( events[index]->GetFlags() & EV_CONSOLE ) && ( typeFlag == EVENT_TIKI_ONLY || typeFlag == EVENT_SCRIPT_ONLY ) )
					continue;
			}
			
			OutputEvent( events[index] );
		}
	}
	
	delete[] events;
}

void DocFileOutput::OutputEvent(Event *ev)
{
	OutputArguments(ev);
	eventCount++;
}

void DocFileOutput::OutputArguments(Event *ev)
{
	int i;
	for( i = 1; i <= ev->getNumArgDefs(); i++ )
	{
		EventArgDef *evarg = ev->getArgDef(i);
		OutputArgument(evarg);
	}
}

void DocFileOutput::OutputArgument(EventArgDef *evarg)
{
}

void DocFileOutput::Write(const char *filename, ClassDef *classlist, int ptypeFlag)
{
	char realname[255];
	sprintf(realname,"%s.%s",filename,GetExtension());
	fileptr = fopen(realname,"w");
	if ( !fileptr )
		return;
	
	typeFlag = ptypeFlag;
	
	OutputClasses(classlist);
	
	fclose(fileptr);
}






HTMLDocFileOutput::HTMLDocFileOutput()
{
	
}

HTMLDocFileOutput::~HTMLDocFileOutput()
{
	
}

void HTMLDocFileOutput::OutputClasses(ClassDef *classlist)
{
	str class_title;
#if defined( GAME_DLL )
	class_title = "Game Module";
#elif defined( CGAME_DLL )
	class_title = "Client Game Module";
#else
	class_title = "Client Module";
#endif
	
	fprintf(fileptr, "<HTML>\n");
	fprintf(fileptr, "<HEAD>\n");
	fprintf(fileptr, "<Title>%s Classes</Title>\n", class_title.c_str() );
	fprintf(fileptr, "</HEAD>\n");
	fprintf(fileptr, "<BODY>\n");
	fprintf(fileptr, "<H1>\n");
	fprintf(fileptr, "<center>%s Classes</center>\n", class_title.c_str() );
	fprintf(fileptr, "</H1>\n");
	
#if defined( GAME_DLL )
	fprintf(fileptr, "<h2>" );
	fprintf(fileptr, "<a href=\"#Actor\">Actor</a>, " );
	fprintf(fileptr, "<a href=\"#Animate\">Animate</a>, " );
	fprintf(fileptr, "<a href=\"#Entity\">Entity</a>, " );
	fprintf(fileptr, "<a href=\"#ScriptSlave\">ScriptSlave</a>, " );
	fprintf(fileptr, "<a href=\"#Sentient\">Sentient</a>, " );
	fprintf(fileptr, "<a href=\"#Trigger\">Trigger</a>, " );
	fprintf(fileptr, "<a href=\"#World\">World</a>" );
	fprintf(fileptr, "</h2>" );
#endif
	
	DocFileOutput::OutputClasses(classlist);
	
	fprintf(fileptr, "<H2>\n");
	fprintf(fileptr, "%d %s Classes.<BR>%d Events.\n", classCount, class_title.c_str(), eventCount );
	fprintf(fileptr, "</H2>\n");
	fprintf(fileptr, "</BODY>\n");
	fprintf(fileptr, "</HTML>\n");
}

void HTMLDocFileOutput::OutputClass(ClassDef *in_class)
{
	ClassDef *savedClass;
	savedClass = in_class;
	
	fprintf(fileptr, "\n");
	if ( in_class->classID[ 0 ] )
	{
		fprintf(fileptr, "<h2> <a name=\"%s\">%s (<i>%s</i>)</a>", in_class->classname, in_class->classname, in_class->classID );
	}
	else
	{
		fprintf(fileptr, "<h2> <a name=\"%s\">%s</a>", in_class->classname, in_class->classname );
	}
	
	for( in_class = in_class->super; in_class != NULL; in_class = in_class->super )
	{
		fprintf(fileptr, " -> <a href=\"#%s\">%s</a>", in_class->classname, in_class->classname );
	}
	fprintf(fileptr, "</h2>\n");
	fprintf(fileptr, "<BLOCKQUOTE>\n");
	
	DocFileOutput::OutputClass(savedClass);
	   
	fprintf(fileptr, "</BLOCKQUOTE>\n");
}

void HTMLDocFileOutput::OutputEvent(Event *ev)
{
	int numargs;
	const char *text;
	
	fprintf(fileptr, "\n<P><tt><B>%s</B>", ev->getName() );
	
	numargs = ev->getNumArgDefs();
	
	if ( numargs )
	{
		fprintf(fileptr, "( <i>" );
		DocFileOutput::OutputEvent(ev);
		fprintf(fileptr, " </i>)</tt><BR>\n" );
	}
	else
	{
		fprintf(fileptr, "</tt><BR>\n" );
	}
	
	text = ev->GetDocumentation();
	
	if ( text )
	{
		char new_doc[1024];
		int old_index;
		int new_index = 0;
		
		for ( old_index = 0 ; old_index < ( int )strlen ( text ) ; old_index++ )
		{
			if ( text[old_index] == '\n' )
			{
				new_doc[new_index 	] = '<';
				new_doc[new_index + 1] = 'B';
				new_doc[new_index + 2] = 'R';
				new_doc[new_index + 3] = '>';
				new_doc[new_index + 4] = '\n';
				new_index += 5;
			}
			else
			{
				new_doc[new_index] = text[old_index];
				new_index++;
			}
		}
		
		new_doc[new_index] = 0;
		
		fprintf(fileptr, "<ul>%s</ul>\n", new_doc );
	}
}

void HTMLDocFileOutput::OutputArgument(EventArgDef *evarg)
{
	if ( evarg->isOptional() )
	{
		fprintf(fileptr, "[ " );
	}
	
	switch( evarg->getType() )
	{
	case IS_ENTITY:
		fprintf(fileptr, "Entity " );
		break;
	case IS_VECTOR:
		fprintf(fileptr, "Vector " );
		break;
	case IS_INTEGER:
		fprintf(fileptr, "Integer " );
		break;
	case IS_FLOAT:
		fprintf(fileptr, "Float " );
		break;
	case IS_STRING:
		fprintf(fileptr, "String " );
		break;
	case IS_BOOLEAN:
		fprintf(fileptr, "Boolean " );
		break;
	}
	fprintf(fileptr, "%s", evarg->getName() );
	
	OutputRange(evarg);
	
	if ( evarg->isOptional() )
	{
		fprintf(fileptr, " ]" );
	}
	
	DocFileOutput::OutputArgument(evarg);
}

void HTMLDocFileOutput::OutputRange(EventArgDef *evarg)
{
	qboolean integer, single;
	int numRanges, i;
	
	single = false;
	integer = true;
	numRanges = 1;
	switch( evarg->getType() )
	{
	case IS_VECTOR:
		integer = false;
		numRanges = 3;
		break;
	case IS_FLOAT:
		integer = false;
		break;
	case IS_STRING:
		single = true;
		break;
	}
	for( i = 0; i < numRanges; i++ )
	{
		if ( single )
		{
			if ( !evarg->GetMinRangeDefault(i) )
            {
				if ( integer )
				{
					fprintf(fileptr, "<%d>", ( int )evarg->GetMinRange(i) );
				}
				else
				{
					fprintf(fileptr, "<%.2f>", evarg->GetMinRange(i) );
				}
            }
		}
		else
		{
			if ( !evarg->GetMinRangeDefault(i) && !evarg->GetMaxRangeDefault(i) )
            {
				if ( integer )
				{
					fprintf(fileptr, "<%d...%d>", ( int )evarg->GetMinRange(i), ( int )evarg->GetMaxRange(i) );
				}
				else
				{
					fprintf(fileptr, "<%.2f...%.2f>", evarg->GetMinRange(i), evarg->GetMaxRange(i) );
				}
            }
			else if ( !evarg->GetMinRangeDefault(i) && evarg->GetMaxRangeDefault(i) )
            {
				if ( integer )
				{
					fprintf(fileptr, "<%d...max_integer>", ( int )evarg->GetMinRange(i) );
				}
				else
				{
					fprintf(fileptr, "<%.2f...max_float>", evarg->GetMinRange(i) );
				}
            }
			else if ( evarg->GetMinRangeDefault(i) && !evarg->GetMaxRangeDefault(i) )
            {
				if ( integer )
				{
					fprintf(fileptr, "<min_integer...%d>", ( int )evarg->GetMaxRange(i) );
				}
				else
				{
					fprintf(fileptr, "<min_float...%.2f>", evarg->GetMaxRange(i) );
				}
            }
		}
	}
}







ToolDocFileOutput::ToolDocFileOutput()
{
	randFlag = false;
	colorFlag = false;
}

ToolDocFileOutput::~ToolDocFileOutput()
{
	
}

void ToolDocFileOutput::OutputClass(ClassDef *in_class)
{
	ClassDef *savedClass;
	savedClass = in_class;
	
	fprintf(fileptr, "\n");
	fprintf(fileptr, "%s\n", in_class->classname );
	fprintf(fileptr, "{\n");
	
	DocFileOutput::OutputClass(savedClass);
	if ( in_class->super != NULL )
		fprintf(fileptr, "\n\tincludes %s\n",in_class->super->classname);
	
	fprintf(fileptr, "}\n");
}

void ToolDocFileOutput::OutputEvent(Event *ev)
{
	int numargs;
	const char *text;
	int i;
	
	fprintf(fileptr, "\n");
	fprintf(fileptr, "\t%s\n", ev->getName() );
	fprintf(fileptr, "\t{\n");
	
	numargs = ev->getNumArgDefs();
	
	if ( numargs )
	{
		DocFileOutput::OutputEvent(ev);
	}
	
	fprintf(fileptr, "\n\t\thelp\t\t\"");
	
	text = ev->GetDocumentation();
	for ( i=0; i<( int )strlen(text); i++ )
	{
		if ( text[i] == '\n' )
			fprintf(fileptr, "\n\t\t\t\t");
		else
			fprintf(fileptr, "%c", text[i]);
	}
	
	fprintf(fileptr, "\"\n");
	fprintf(fileptr, "\t}\n");
	
	text = ev->GetDocumentation();
}

void ToolDocFileOutput::OutputArgument(EventArgDef *evarg)
{
	if ( !strcmp(evarg->getName(), "[randomtype]") )
	{
		fprintf(fileptr, "\t\trandom");
		randFlag = true; 
		return;
	}
	
	if ( randFlag ) 
	{
		fprintf(fileptr, "\t\t%s\t\"%s\"\n",evarg->getName(), evarg->getName());
		randFlag = false;
		return;
	}
	
	if ( strstr(evarg->getName(), "color_") && evarg->getType() == IS_VECTOR )
	{
		fprintf(fileptr, "\t\trgb\t\t%s\t\"%s\"\n",evarg->getName(), evarg->getName());
		return;
	}
	
	if ( strstr(evarg->getName(), "color_red") && evarg->getType() != IS_VECTOR )
	{
		char tmpstr[25], *sptr;
		strcpy(tmpstr,evarg->getName());
		sptr = strtok(tmpstr,"_"); 
		sptr = strtok(NULL,"_"); 
		sptr = strtok(NULL,"_"); 
		
		colorFlag = true;
		if ( sptr == NULL )
			fprintf(fileptr, "\t\trgb\t\tcolor\t\"color\"\n");
		else
			fprintf(fileptr, "\t\trgb\t\t%s\t\"%s\"\n", sptr, sptr);
		return;
	}
	
	if ( colorFlag && strstr(evarg->getName(), "color_blue") )
	{
		colorFlag = false;
		return;
	}
	
	if ( colorFlag )
		return;
	
	if ( evarg->isOptional() )
		fprintf(fileptr, "\t\t|" );
	else
		fprintf(fileptr, "\t\t");
	
	switch( evarg->getType() )
	{
	case IS_ENTITY:
		fprintf(fileptr, "string\t\t" );
		break;
	case IS_VECTOR:
		fprintf(fileptr, "xyz\t\t" );
		break;
	case IS_INTEGER:
		fprintf(fileptr, "int\t\t" );
		break;
	case IS_FLOAT:
		fprintf(fileptr, "float\t\t" );
		break;
	case IS_STRING:
		fprintf(fileptr, "string\t\t" );
		break;
	case IS_BOOLEAN:
		fprintf(fileptr, "boolean\t\t" );
		break;
	}
	fprintf(fileptr, "%s\t", evarg->getName() );
	fprintf(fileptr, "\"%s\"\t", evarg->getName() );
	
	OutputRange(evarg);
	fprintf(fileptr, "\n");
	
	DocFileOutput::OutputArgument(evarg);
}

void ToolDocFileOutput::OutputRange(EventArgDef *evarg)
{
	int i;
	
	if ( evarg->getType() == IS_VECTOR )
	{
		for ( i=0; i<3; i++ )
		{
			if ( evarg->GetMinRangeDefault(i) || evarg->GetMaxRangeDefault(i) )
				return;
		}
		
		fprintf(fileptr, "%.2f %.2f %.2f %.2f %.2f %.2f\n", 
			evarg->GetMinRange(0), evarg->GetMinRange(1), evarg->GetMinRange(2), 
			evarg->GetMaxRange(0), evarg->GetMaxRange(1), evarg->GetMaxRange(2) );
		
		return;
	}
	
	if ( evarg->getType() == IS_STRING )
	{
		if ( !evarg->GetMinRangeDefault(0) )
		{
			fprintf(fileptr, "%d", ( int )evarg->GetMinRange(0) );
		}
		
		return;
	}
	
	if ( !evarg->GetMinRangeDefault(0) && !evarg->GetMaxRangeDefault(0) )
	{
		if ( evarg->getType() == IS_FLOAT )
			fprintf(fileptr, "%.2f %.2f", evarg->GetMinRange(0), evarg->GetMaxRange(0) );
		else
			fprintf(fileptr, "%d %d", ( int )evarg->GetMinRange(0), ( int )evarg->GetMaxRange(0) );
	}
}

void ToolDocFileOutput::PrintSubClasses(ClassDef *in_class, ClassDef *classlist)
{
	ClassDef *c;
	OutputClass(in_class);
	for( c = classlist->next; c != classlist; c = c->next )
	{
		if ( c->super == in_class )
			PrintSubClasses(c, classlist);
	}
}

void ToolDocFileOutput::OutputClasses(ClassDef *classlist)
{
	ClassDef *c;
	
	for( c = classlist->next; c != classlist; c = c->next )
	{
		if ( c->super == NULL )
			PrintSubClasses(c, classlist);
	}
}
