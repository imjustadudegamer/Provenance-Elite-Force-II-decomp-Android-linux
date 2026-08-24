
#include "_pch_cpp.h"
#include "script.h"
#include "../qcommon/gameplaymanager.h"

#define TOKENCOMMENT      (';')
#define TOKENCOMMENT2     ('#')
#define TOKENEOL          ('\n')
#define TOKENSPACE        (' ')
#define TOKENSPECIAL	     ('$')

CLASS_DECLARATION( Class, Script, NULL )
{
	{ NULL, NULL }
};

Script::~Script()
{
	Close();
}

Script::Script(const char* filename_shadow1 )
{
	buffer			= NULL;
	script_p		= NULL;
	end_p			= NULL;
	line			= 0;
	length			= 0;
	releaseBuffer	= false;
	tokenready		= false;
	memset( token, 0, sizeof( token ) );
	
	if ( filename_shadow1 != 0 )
		LoadFile(filename_shadow1);
}

void Script::Close( void )
{
	if ( releaseBuffer && buffer )
	{
		gi.Free( ( void * )buffer );
	}
	
	buffer			= NULL;
	script_p			= NULL;
	end_p				= NULL;
	line				= 0;
	releaseBuffer	= false;
	tokenready		= false;
	memset( token, 0, sizeof( token ) );
	
	for( int i = 1; i <= macrolist.NumObjects(); i++)
	{
		if (macrolist.ObjectAt( i ) )
		{
			delete macrolist.ObjectAt( i );
			macrolist.ObjectAt( i ) = 0;
		}	
	}
}


const char *Script::Filename( void )
{
	return filename.c_str();
}


int Script::GetLineNumber( void )
{
	return line;
}


void Script::Reset( void )
{
	script_p = buffer;
	line = 1;
	tokenready = false;
}


void Script::MarkPosition( scriptmarker_t *mark )
{
	assert( mark );
	
	mark->tokenready = tokenready;
	mark->offset     = script_p - buffer;
	mark->line		  = line;
	strcpy( mark->token, token );
}


void Script::RestorePosition( const scriptmarker_t *mark )
{
	assert( mark );
	
	tokenready	= mark->tokenready;
	script_p		= buffer + mark->offset;
	line			= mark->line;
	strcpy( token, mark->token );
	
	assert( script_p <= end_p );
	if ( script_p > end_p )
	{
		script_p = end_p;
	}
}


qboolean Script::SkipToEOL( void )
{
	if ( script_p >= end_p )
	{
		return true;
	}
	
	while( *script_p != TOKENEOL )
	{
		if ( script_p >= end_p )
		{
			return true;
		}
		script_p++;
	}
	return false;
}


void Script::CheckOverflow(	void )
{
	if ( script_p >= end_p )
	{
		gi.Error( ERR_DROP, "End of token file reached prematurely reading %s\n", filename.c_str() );
	}
}


void Script::SkipWhiteSpace( qboolean crossline )
{
	CheckOverflow();
	
	while( *script_p <= TOKENSPACE )
	{
		if ( *script_p++ == TOKENEOL )
		{
			if ( !crossline )
			{
				gi.Error( ERR_DROP, "Line %i is incomplete in file %s\n", line, filename.c_str() );
			}
			
			line++;
		}
		CheckOverflow();
	}
}

qboolean Script::AtComment( void )
{
	if ( script_p >= end_p )
	{
		return false;
	}
	
	if ( *script_p == TOKENCOMMENT )
	{
		return true;
	}
	
	if ( *script_p == TOKENCOMMENT2 )
	{
		return true;
	}
	
	if ( ( script_p + 1 ) >= end_p )
	{
		return false;
	}
	
	if ( ( *script_p == '/' ) && ( *( script_p + 1 ) == '/' ) )
	{
		return true;
	}
	
	return false;
}


void Script::SkipNonToken( qboolean crossline )
{
	SkipWhiteSpace( crossline );
	while( AtComment() )
	{
		SkipToEOL();
		SkipWhiteSpace( crossline );
	}
}



qboolean Script::TokenAvailable( qboolean crossline )
{
	if ( script_p >= end_p )
	{
		return false;
	}
	
	while ( 1 )
	{
		while ( *script_p <= TOKENSPACE )
		{
			if ( *script_p == TOKENEOL )
			{
				if ( !crossline )
				{
					return( false );
				}
				line++;
			}
			
			script_p++;
			if ( script_p >= end_p )
			{
				return false;
			}
		}
		
		if ( AtComment() )
		{
			qboolean done;
			
			done = SkipToEOL();
			if ( done )
			{
				return false;
			}
		}
		else
		{
			break;
		}
	}
	
	return true;
}


qboolean Script::CommentAvailable( qboolean crossline )
{
	const char *searchptr;
	
	searchptr = script_p;
	
	if ( searchptr >= end_p )
	{
		return false;
	}
	
	while ( *searchptr <= TOKENSPACE )
	{
		if ( ( *searchptr == TOKENEOL ) && ( !crossline ) )
		{
			return false;
		}
		searchptr++;
		if ( searchptr >= end_p )
		{
			return false;
		}
	}
	
	return true;
}


void Script::UnGetToken( void )
{
	tokenready = true;
}

qboolean Script::AtString( qboolean crossline )
{
	SkipNonToken( crossline );
	
	return ( *script_p == '"' );
}

qboolean Script::AtOpenParen( qboolean crossline )
{
	SkipNonToken( crossline );
	
	return ( *script_p == '(' );	
}

qboolean Script::AtCloseParen( qboolean crossline  )
{
	SkipNonToken( crossline );
	
	return ( *script_p == ')' );	
}


const char *Script::GetToken( qboolean crossline )
{
	
	str		 token_p = token;
	qboolean	 is_Macro = false;
	
	if ( tokenready )
	{
		tokenready = false;
		return token;
	}
	
	is_Macro = isMacro();
	
	token_p = GrabNextToken(crossline);
	
	if ( is_Macro && ( token_p != "$include" ) )
	{
		
		while ( ( token_p == "$define" ) || ( token_p == "$Define" ) )
		{
			AddMacroDefinition(crossline);
			is_Macro = isMacro();
			token_p = GrabNextToken(crossline);			
		}
		
		if( is_Macro && ( token_p != "$include" ) && ( token_p[token_p.length() - 1] == '$' ) )
		{
			return GetMacroString(token_p);
		}		
		
	}
		
	return token;
}

const char *Script::GrabNextToken( qboolean crossline )
{
	char *token_p;
	
	SkipNonToken( crossline );
	
	if ( *script_p == '"' )
	{
		return GetString( crossline );
	}
	
	token_p = token;
	while( ( *script_p > TOKENSPACE ) && !AtComment() )
	{
		if ( ( *script_p == '\\' ) && ( script_p < ( end_p - 1 ) ) )
		{
			script_p++;
			switch( *script_p )
			{
			case 'n' :	*token_p++ = '\n'; break;
			case 'r' :	*token_p++ = '\n'; break;
			case '\'' : *token_p++ = '\''; break;
			case '\"' : *token_p++ = '\"'; break;
			case '\\' : *token_p++ = '\\'; break;
			default:		*token_p++ = *script_p; break;
			}
			script_p++;
		}
		else
		{
			*token_p++ = *script_p++;
		}
		
		if ( token_p == &token[ MAXTOKEN ] )
		{
			gi.Error( ERR_DROP, "Token too large on line %i in file %s\n", line, filename.c_str() );
		}
		
		if ( script_p == end_p )
		{
			break;
		}
	}
	
	
	*token_p = 0;
	
	return token;
}

void Script::AddMacroDefinition( qboolean crossline )
{
	macro      *theMacro;
	
	theMacro = new macro;
	
	theMacro->macroName = '$';
	theMacro->macroName.append(GrabNextToken(crossline));
	theMacro->macroName.append('$'); 
	
	str tmpstr;
	tmpstr = GrabNextToken(crossline);
	if( ( tmpstr != "$include" ) && ( tmpstr[tmpstr.length() - 1] == '$' ) )
		theMacro->macroText = GetMacroString(tmpstr);
	else
		theMacro->macroText = tmpstr;
	
	macrolist.AddObject( theMacro );
	
}

const char *Script::GetMacroString( const char *theMacroName )
{
	
	macro *theMacro =0; 
	
	for( int i = 1; i <= macrolist.NumObjects(); i++)
	{
		theMacro = macrolist.ObjectAt( i );
		
		if(!theMacro->macroName.cmp(theMacro->macroName.c_str(), theMacroName))
		{
			const char *text = theMacro->macroText.c_str();
			
			if( text[0] == '$' )
				return EvaluateMacroString(text);
			else
				return text;
		}
		
	}
	
	char tmpstr[255], *sptr = tmpstr;
	strcpy(tmpstr, theMacroName);
	tmpstr[strlen(tmpstr)-1] = 0;
	sptr++;
	
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( gpm->isDefined(sptr) )
		return gpm->getDefine(sptr).c_str();
	
	gi.Error( ERR_DROP, "No Macro Text found for %s in file %s\n", theMacroName, filename.c_str() );
	return 0;
	
}

void Script::AddMacro(const char *name, const char *value)
{

}

char *Script::EvaluateMacroString( const char *theMacroString )
{
	static char evalText[255];
	char buffer_shadow2[255], *bufferptr = buffer_shadow2, oper = '+', newoper = '+';
	bool haveoper = false;
	int i;
	float value = 0.0f, val = 0.0f;
	memset(buffer_shadow2, 0, 255);
	
	for ( i=0;i<=( int )strlen(theMacroString);i++ )
	{
		if ( theMacroString[i] == '+' ) { haveoper = true; newoper = '+'; }
		if ( theMacroString[i] == '-' ) { haveoper = true; newoper = '-'; }
		if ( theMacroString[i] == '*' ) { haveoper = true; newoper = '*'; }
		if ( theMacroString[i] == '/' ) { haveoper = true; newoper = '/'; }
		if ( theMacroString[i] == 0 ) haveoper = true;
		
		if ( haveoper )
		{ 
			if ( buffer_shadow2[0] == '$' )
				val = atof(GetMacroString(buffer_shadow2));
			else
				val = atof(buffer_shadow2);
			
			value = EvaluateMacroMath(value, val, oper);
			oper = newoper;
			
			haveoper = false;
			memset(buffer_shadow2, 0, 255);
			bufferptr = buffer_shadow2;
			continue; 
		}
		
		*bufferptr = theMacroString[i];
		bufferptr++;
	}
	
	sprintf(evalText,"%f",value);
	return evalText;
}

float Script::EvaluateMacroMath(float value, float newval, char oper)
{
	switch ( oper )
		{
		case '+' : value += newval; break;
		case '-' : value -= newval; break;
		case '*' : value *= newval; break;
		case '/' : value /= newval; break;
		}

	return value;
}

qboolean Script::isMacro( void )
{
	if ( !TokenAvailable( true ) )
		return false;
	
	SkipNonToken( true );
	if ( *script_p == TOKENSPECIAL )
	{
		return true;
	}
	
	return false;
}


const char *Script::GetLine( qboolean crossline )
{
	const char	*start;
	int			size;
	
	if ( tokenready )
	{
		tokenready = false;
		return token;
	}
	
	SkipNonToken( crossline );
	
	start = script_p;
	SkipToEOL();
	size = script_p - start;
	if ( size < ( MAXTOKEN - 1 ) )
	{
		memcpy( token, start, size );
		token[ size ] = '\0';
	}
	else
	{
		gi.Error( ERR_DROP, "Token too large on line %i in file %s\n", line, filename.c_str() );
	}
	
	return token;
}


const char *Script::GetRaw( void )
{
	const char	*start;
	int			size;
	
	SkipWhiteSpace( true );
	
	start = script_p;
	SkipToEOL();
	size = script_p - start;
	if ( size < ( MAXTOKEN - 1 ) )
	{
		memset( token, 0, sizeof( token ) );
		memcpy( token, start, size );
	}
	else
	{
		gi.Error( ERR_DROP, "Token too large on line %i in file %s\n", line, filename.c_str() );
	}
	
	return token;
}


const char *Script::GetString( qboolean crossline )
{
	int startline;
	char *token_p;
	
	if ( tokenready )
	{
		tokenready = false;
		return token;
	}
	
	SkipNonToken( crossline );
	
	if ( *script_p != '"' )
	{
		gi.Error( ERR_DROP, "Expecting string on line %i in file %s\n", line, filename.c_str() );
	}
	
	script_p++;
	
	startline = line;
	token_p = token;
	while( *script_p != '"' )
	{
		if ( *script_p == TOKENEOL )
		{
			gi.Error( ERR_DROP, "Line %i is incomplete while reading string in file %s\n", line, filename.c_str() );
		}
		
		if ( ( *script_p == '\\' ) && ( script_p < ( end_p - 1 ) ) )
		{
			script_p++;
			switch( *script_p )
			{
			case 'n' :	*token_p++ = '\n'; break;
			case 'r' :	*token_p++ = '\n'; break;
			case '\'' : *token_p++ = '\''; break;
			case '\"' : *token_p++ = '\"'; break;
			case '\\' : *token_p++ = '\\'; break;
			default:		*token_p++ = *script_p; break;
			}
			script_p++;
		}
		else
		{
			*token_p++ = *script_p++;
		}
		
		if ( script_p >= end_p )
		{
			gi.Error( ERR_DROP, "End of token file reached prematurely while reading string on\n"
				"line %d in file %s\n", startline, filename.c_str() );
		}
		
		if ( token_p == &token[ MAXTOKEN ] )
		{
			gi.Error( ERR_DROP, "String too large on line %i in file %s\n", line, filename.c_str() );
		}
	}
	
	*token_p = 0;
	
	script_p++;
	
	return token;
}


qboolean Script::GetSpecific( const char *string )
{
	do
	{
		if ( !TokenAvailable( true ) )
		{
			return false;
		}
		GetToken( true );
	}
	while( strcmp( token, string ) );
	
	return true;
}

qboolean Script::GetBoolean( qboolean crossline )
{
	GetToken( crossline );
	if ( stricmp( token, "true" ) == 0 )
		return qtrue ;
	else if ( stricmp( token, "1" ) == 0 )
		return qtrue ;
	return qfalse ;
}


int Script::GetInteger( qboolean crossline )
{
	GetToken( crossline );
	return atoi( token );
}


double Script::GetDouble( qboolean crossline )
{
	GetToken( crossline );
	return atof( token );
}


float Script::GetFloat( qboolean crossline )
{
	return ( float )GetDouble( crossline );
}


Vector Script::GetVector( qboolean crossline )
{
	float	x = GetFloat( crossline );
	float	y = GetFloat( crossline );
	float	z = GetFloat( crossline );
	return Vector( x, y, z );
}

int Script::LinesInFile( void )
{
	qboolean		temp_tokenready;
	const char	*temp_script_p;
	int			temp_line;
	char			temp_token[ MAXTOKEN ];
	int			numentries;
	
	temp_tokenready = tokenready;
	temp_script_p	= script_p;
	temp_line		= line;
	strcpy( temp_token, token );
	
	numentries = 0;
	
	Reset();
	while( TokenAvailable( true ) )
	{
		GetLine( true );
		numentries++;
	}
	
	tokenready	= temp_tokenready;
	script_p	= temp_script_p;
	line		= temp_line;
	strcpy( token, temp_token );
	
	return numentries;
}


void Script::Parse( const char *data, int length_shadow2, const char *name )
{
	Close();
	
	buffer = data;
	Reset();
	end_p = script_p + length_shadow2;
	this->length = length_shadow2;
	filename = name;
}


void Script::LoadFile( const char *name )
{
	int			length_shadow1;
	byte        *buffer_shadow1;
	byte        *tempbuf;
	const char  *const_buffer;
	
	Close();
	
	length_shadow1 = gi.FS_ReadFile( name, ( void ** )&tempbuf, true );
	if ( length_shadow1 < 0 )
	{
		error( "LoadFile", "Couldn't load %s\n", name );
	}
	buffer_shadow1 = ( byte * )gi.Malloc( length_shadow1 );
	memcpy( buffer_shadow1, tempbuf, length_shadow1 );
	gi.FS_FreeFile( tempbuf );
	
	const_buffer = ( char * )buffer_shadow1;
	
	Parse( const_buffer, length_shadow1, name );
	releaseBuffer = true;
}

const char *Script::Token( void )
{
	return token;
}
