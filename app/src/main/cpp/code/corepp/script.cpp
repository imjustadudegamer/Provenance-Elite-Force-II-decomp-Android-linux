/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "../corepp/script.h"

#if defined(GAME_DLL)

#    include "../fgame/g_local.h"

#    define FILE_FS_FreeFile       gi.FS_FreeFile
#    define FILE_FS_ReadFile(a, b) gi.FS_ReadFile(a, b, true)
#    define FILE_Malloc            gi.Malloc
#    define FILE_Free              gi.Free
#    define FILE_Error             gi.Error

#elif defined(CGAME_DLL)

#    define FILE_FS_FreeFile       cgi.FS_FreeFile
#    define FILE_FS_ReadFile(a, b) cgi.FS_ReadFile(a, b, qtrue)
#    define FILE_Malloc            cgi.Malloc
#    define FILE_Free              cgi.Free
#    define FILE_Error             cgi.Error

#else

#    include "../qcommon/qcommon.h"

#    define FILE_FS_FreeFile       FS_FreeFile
#    define FILE_FS_ReadFile(a, b) FS_ReadFile(a, b)
#    define FILE_Malloc            Z_Malloc
#    define FILE_Free              Z_Free
#    define FILE_Error             Com_Error

#endif

typedef unsigned char byte;

CLASS_DECLARATION(Class, Script, NULL) {
    {NULL, NULL}
};

Script::~Script()
{
    Close();
}

Script::Script(const char *name )
{
    buffer        = NULL;
    script_p      = NULL;
    end_p         = NULL;
    line          = 0;
    length        = 0;
    releaseBuffer = false;
    tokenready    = false;
    token[0]      = 0;

    if (name != 0) {
        LoadFile(name);
    }
}

Script::Script()
{
    buffer        = NULL;
    script_p      = NULL;
    end_p         = NULL;
    line          = 0;
    length        = 0;
    releaseBuffer = false;
    tokenready    = false;
    token[0]      = 0;
}

void Script::Close(void)
{
    if (releaseBuffer && buffer) {
        FILE_Free((void *)buffer);
    }

    buffer        = NULL;
    script_p      = NULL;
    end_p         = NULL;
    line          = 0;
    releaseBuffer = false;
    tokenready    = false;
    token[0]      = 0;

    for (int i = 1; i <= macrolist.NumObjects(); i++) {
        if (macrolist.ObjectAt(i)) {
            delete macrolist.ObjectAt(i);
            macrolist.ObjectAt(i) = 0;
        }
    }
}


const char *Script::Filename(void)
{
    return filename.c_str();
}


int Script::GetLineNumber(void)
{
    return line;
}


void Script::Reset(void)
{
    script_p   = buffer;
    line       = 1;
    tokenready = false;
    hasError   = false;
}


void Script::MarkPosition(scriptmarker_t *mark)
{
    assert(mark);

    mark->tokenready = tokenready;
    mark->offset     = script_p - buffer;
    mark->line       = line;
    Q_strncpyz(mark->token, token, sizeof(mark->token));
}


void Script::RestorePosition(const scriptmarker_t *mark)
{
    assert(mark);

    tokenready = mark->tokenready;
    script_p   = buffer + mark->offset;
    line       = mark->line;
    Q_strncpyz(token, mark->token, sizeof(token));

    assert(script_p <= end_p);
    if (script_p > end_p) {
        script_p = end_p;
    }
}


qboolean Script::SkipToEOL(void)
{
    if (script_p >= end_p) {
        return true;
    }

    while (*script_p != TOKENEOL) {
        if (script_p >= end_p) {
            return true;
        }
        script_p++;
    }
    return false;
}


void Script::CheckOverflow(void)
{
    if (script_p >= end_p) {
        FILE_Error(ERR_DROP, "End of token file reached prematurely reading %s\n", filename.c_str());
    }
}


void Script::SkipWhiteSpace(qboolean crossline)
{
    CheckOverflow();

    while (*script_p <= TOKENSPACE || *script_p == TOKENCOMMA) {
        if (*script_p++ == TOKENEOL) {
            if (!crossline) {
                FILE_Error(ERR_DROP, "Line %i is incomplete in file %s\n", line, filename.c_str());
            }

            line++;
        }
        CheckOverflow();
    }
}

qboolean Script::AtComment(void)
{
    if (script_p >= end_p) {
        return false;
    }

    if (*script_p == TOKENCOMMENT) {
        return true;
    }

    // Retail does NOT accept '#' here.  Script::TokenAvailable @0x001CEC80 (which
#if 0
    if (*script_p == TOKENCOMMENT2) {
        return true;
    }
#endif

    if ((script_p + 1) >= end_p) {
        return false;
    }

    if ((*script_p == '/') && (*(script_p + 1) == '/')) {
        return true;
    }

    return false;
}


void Script::SkipNonToken(qboolean crossline)
{
    SkipWhiteSpace(crossline);
    while (AtComment()) {
        SkipToEOL();
        SkipWhiteSpace(crossline);
    }
}



qboolean Script::TokenAvailable(qboolean crossline)
{
    if (script_p >= end_p) {
        return false;
    }

    while (1) {
        while (*script_p <= TOKENSPACE) {
            if (*script_p == TOKENEOL) {
                if (!crossline) {
                    return (false);
                }
                line++;
            }

            script_p++;
            if (script_p >= end_p) {
                return false;
            }
        }

        if (AtComment()) {
            qboolean done;

            done = SkipToEOL();
            if (done) {
                return false;
            }
        } else {
            break;
        }
    }

    return true;
}


qboolean Script::CommentAvailable(qboolean crossline)
{
    const char *searchptr;

    searchptr = script_p;

    if (searchptr >= end_p) {
        return false;
    }

    while (*searchptr <= TOKENSPACE) {
        if ((*searchptr == TOKENEOL) && (!crossline)) {
            return false;
        }
        searchptr++;
        if (searchptr >= end_p) {
            return false;
        }
    }

    return true;
}


void Script::UnGetToken(void)
{
    tokenready = true;
}

qboolean Script::AtString(qboolean crossline)
{
    SkipNonToken(crossline);

    return (*script_p == '"');
}

qboolean Script::AtOpenParen(qboolean crossline)
{
    SkipNonToken(crossline);

    return (*script_p == '(');
}

qboolean Script::AtCloseParen(qboolean crossline)
{
    SkipNonToken(crossline);

    return (*script_p == ')');
}

qboolean Script::AtComma(qboolean crossline)
{
    SkipNonToken(crossline);

    return (*script_p == ',');
}

qboolean Script::AtDot(qboolean crossline)
{
    SkipNonToken(crossline);

    return (*script_p == '.');
}

qboolean Script::AtAssignment(qboolean crossline)
{
    SkipNonToken(crossline);

    return (*script_p == '=') || ((*script_p == '+') && (*(script_p + 1) == '='))
        || ((*script_p == '-') && (*(script_p + 1) == '=')) || ((*script_p == '*') && (*(script_p + 1) == '='))
        || ((*script_p == '/') && (*(script_p + 1) == '='));
}


const char *Script::GetToken(qboolean crossline)
{
    const char *token_p  = token;
    qboolean    is_Macro = false;

    if (tokenready) {
        tokenready = false;
        return token;
    }

    is_Macro = isMacro();

    token_p = GrabNextToken(crossline);

    if (is_Macro && (strcmp(token_p, "$include") != 0)) {
        while ((!strcmp(token_p, "$define")) || (!strcmp(token_p, "$Define"))) {
            AddMacroDefinition(crossline);
            is_Macro = isMacro();
            token_p = GrabNextToken(crossline);
        }

        if (is_Macro && (strcmp(token_p, "$include") != 0) && (token_p[strlen(token_p) - 1] == '$')) {
            return GetMacroString(token_p);
        }
    }

    return token;
}

const char *Script::GrabNextToken(qboolean crossline)
{
    char *token_p;

    SkipNonToken(crossline);

    if (*script_p == '"') {
        return GetString(crossline);
    }

    token_p = token;
    while ((*script_p > TOKENSPACE) && (*script_p != TOKENCOMMA) && !AtComment()) {
        if ((*script_p == '\\') && (script_p < (end_p - 1))) {
            script_p++;
            switch (*script_p) {
            case 'n':
                *token_p++ = '\n';
                break;
            case 'r':
                *token_p++ = '\n';
                break;
            case '\'':
                *token_p++ = '\'';
                break;
            case '\"':
                *token_p++ = '\"';
                break;
            case '\\':
                *token_p++ = '\\';
                break;
            default:
                *token_p++ = *script_p;
                break;
            }
            script_p++;
        } else {
            *token_p++ = *script_p++;
        }

        if (token_p == &token[SCRIPT_MAXTOKEN]) {
            FILE_Error(ERR_DROP, "Token too large on line %i in file %s\n", line, filename.c_str());
        }

        if (script_p == end_p) {
            break;
        }
    }

    *token_p = 0;

    return token;
}

void Script::AddMacroDefinition(qboolean crossline)
{
    macro *theMacro;

    theMacro = new macro;

    theMacro->macroName = "$";
    theMacro->macroName.append(GrabNextToken(crossline));
    theMacro->macroName.append("$"); 

    str tmpstr;
    tmpstr = GrabNextToken(crossline);
    if ((tmpstr != "$include") && (tmpstr[tmpstr.length() - 1] == '$')) {
        theMacro->macroText = GetMacroString(tmpstr);
    } else {
        theMacro->macroText = tmpstr;
    }

    macrolist.AddObject(theMacro);
}

const char *Script::GetMacroString(const char *theMacroName)
{
    macro *theMacro = 0; 

    for (int i = 1; i <= macrolist.NumObjects(); i++) {
        theMacro = macrolist.ObjectAt(i);

        if (!theMacro->macroName.cmp(theMacro->macroName.c_str(), theMacroName)) {
            const char *text = theMacro->macroText.c_str();

            if (text[0] == '$') {
                return EvaluateMacroString(text);
            } else {
                return text;
            }
        }
    }

    return "";
}

void Script::AddMacro(const char *name, const char *value) {}

char *Script::EvaluateMacroString(const char *theMacroString)
{
    static char evalText[255];
    char        macroBuffer[255], *bufferptr = macroBuffer, oper = '+', newoper = '+';
    bool        haveoper = false;
    int         i;
    float       value = 0.0f, val = 0.0f;
    memset(macroBuffer, 0, 255);

    // NOTE: '<=' is deliberate - the final iteration lands on the NUL terminator
    for (i = 0; i <= (int)strlen(theMacroString); i++) {
        if (theMacroString[i] == '+') {
            haveoper = true;
            newoper  = '+';
        }
        if (theMacroString[i] == '-') {
            haveoper = true;
            newoper  = '-';
        }
        if (theMacroString[i] == '*') {
            haveoper = true;
            newoper  = '*';
        }
        if (theMacroString[i] == '/') {
            haveoper = true;
            newoper  = '/';
        }
        if (theMacroString[i] == 0) {
            haveoper = true;
        }

        if (haveoper) {
            if (macroBuffer[0] == '$') {
                val = atof(GetMacroString(macroBuffer));
            } else {
                val = atof(macroBuffer);
            }

            value = EvaluateMacroMath(value, val, oper);
            oper  = newoper;

            haveoper = false;
            memset(macroBuffer, 0, 255);
            bufferptr = macroBuffer;
            continue;
        }

        *bufferptr = theMacroString[i];
        bufferptr++;
    }

    Com_sprintf(evalText, sizeof(evalText), "%f", value);
    return evalText;
}

float Script::EvaluateMacroMath(float value, float newval, char oper)
{
    switch (oper) {
    case '+':
        value += newval;
        break;
    case '-':
        value -= newval;
        break;
    case '*':
        value *= newval;
        break;
    case '/':
        value /= newval;
        break;
    }

    return value;
}

qboolean Script::isMacro(void)
{
    if (!TokenAvailable(true)) {
        return false;
    }

    SkipNonToken(true);
    if (*script_p == TOKENSPECIAL) {
        return true;
    }

    return false;
}


const char *Script::GetLine(qboolean crossline)
{
    const char *start;
    int         size;

    if (tokenready) {
        tokenready = false;
        return token;
    }

    SkipNonToken(crossline);

    start = script_p;
    SkipToEOL();
    size = script_p - start;
    if (size < (SCRIPT_MAXTOKEN - 1)) {
        memcpy(token, start, size);
        token[size] = '\0';
    } else {
        FILE_Error(ERR_DROP, "Token too large on line %i in file %s\n", line, filename.c_str());
    }

    return token;
}


const char *Script::GetRaw(void)
{
    const char *start;
    int         size;

    SkipWhiteSpace(true);

    start = script_p;
    SkipToEOL();
    size = script_p - start;
    if (size < (SCRIPT_MAXTOKEN - 1)) {
        memset(token, 0, sizeof(token));
        memcpy(token, start, size);
    } else {
        FILE_Error(ERR_DROP, "Token too large on line %i in file %s\n", line, filename.c_str());
    }

    return token;
}


const char *Script::GetString(qboolean crossline)
{
    int   startline;
    char *token_p;

    if (tokenready) {
        tokenready = false;
        return token;
    }

    SkipNonToken(crossline);

    if (*script_p != '"') {
        FILE_Error(ERR_DROP, "Expecting string on line %i in file %s\n", line, filename.c_str());
    }

    script_p++;

    startline = line;
    token_p   = token;
    while (*script_p != '"') {
        if (*script_p == TOKENEOL) {
            FILE_Error(ERR_DROP, "Line %i is incomplete while reading string in file %s\n", line, filename.c_str());
        }

        if ((*script_p == '\\') && (script_p < (end_p - 1))) {
            script_p++;
            switch (*script_p) {
            case 'n':
                *token_p++ = '\n';
                break;
            case 'r':
                *token_p++ = '\n';
                break;
            case '\'':
                *token_p++ = '\'';
                break;
            case '\"':
                *token_p++ = '\"';
                break;
            case '\\':
                *token_p++ = '\\';
                break;
            default:
                *token_p++ = *script_p;
                break;
            }
            script_p++;
        } else {
            *token_p++ = *script_p++;
        }

        if (script_p >= end_p) {
            FILE_Error(
                ERR_DROP,
                "End of token file reached prematurely while reading "
                "string on\n"
                "line %d in file %s\n",
                startline,
                filename.c_str()
            );
        }

        if (token_p == &token[SCRIPT_MAXTOKEN]) {
            FILE_Error(ERR_DROP, "String too large on line %i in file %s\n", line, filename.c_str());
        }
    }

    *token_p = 0;

    script_p++;

    return token;
}


qboolean Script::GetSpecific(const char *string)
{
    do {
        if (!TokenAvailable(true)) {
            return false;
        }
        GetToken(true);
    } while (strcmp(token, string));

    return true;
}

qboolean Script::GetBoolean(qboolean crossline)
{
    GetToken(crossline);
    if (Q_stricmp(token, "true") == 0) {
        return true;
    } else if (Q_stricmp(token, "1") == 0) {
        return true;
    }

    return false;
}


int Script::GetInteger(qboolean crossline)
{
    GetToken(crossline);
    return atoi(token);
}


double Script::GetDouble(qboolean crossline)
{
    GetToken(crossline);
    return atof(token);
}


float Script::GetFloat(qboolean crossline)
{
    return (float)GetDouble(crossline);
}


Vector Script::GetVector(qboolean crossline)
{
    float x = GetFloat(crossline);
    float y = GetFloat(crossline);
    float z = GetFloat(crossline);
    return Vector(x, y, z);
}

int Script::LinesInFile(void)
{
    qboolean    temp_tokenready;
    const char *temp_script_p;
    int         temp_line;
    char        temp_token[SCRIPT_MAXTOKEN];
    int         numentries;

    temp_tokenready = tokenready;
    temp_script_p   = script_p;
    temp_line       = line;
    Q_strncpyz(temp_token, token, sizeof(temp_token));

    numentries = 0;

    Reset();
    while (TokenAvailable(true)) {
        GetLine(true);
        numentries++;
    }

    tokenready = temp_tokenready;
    script_p   = temp_script_p;
    line       = temp_line;
    Q_strncpyz(token, temp_token, sizeof(token));

    return numentries;
}


void Script::Parse(const char *data, size_t len, const char *name)
{
    Close();

    buffer = data;
    Reset();
    this->length = len;
    end_p        = script_p + length;
    filename     = name;
}


void Script::LoadFile(const char *name)
{
    int         filelen;
    byte       *filebuf;
    byte       *tempbuf;
    const char *const_buffer;

    Close();

    filelen = FILE_FS_ReadFile(name, (void **)&tempbuf);
    if (filelen == -1) {
        error("LoadFile", "Couldn't load %s\n", name);
        return;
    }

    hasError = false;

    if (filelen < 0) {
        hasError = true;
        return;
    }

    filebuf = (byte *)FILE_Malloc(filelen + 1);
    memcpy(filebuf, tempbuf, filelen);
    filebuf[filelen] = 0;
    FILE_FS_FreeFile(tempbuf);

    const_buffer = (char *)filebuf;

    Parse(const_buffer, filelen, name);
    releaseBuffer = true;
}


void Script::LoadFile(const char *name, int len, const char *buf)
{
    Close();

    this->buffer = (const char *)FILE_Malloc(len);
    this->length = len;
    memcpy((void *)this->buffer, buf, len);

    Parse(buffer, this->length, name);
    releaseBuffer = true;
}

qboolean Script::isValid()
{
    return !hasError;
}

qboolean Script::EndOfFile(void)
{
    return script_p >= end_p;
}

const char *Script::Token(void)
{
    return token;
}

#if !defined(CGAME_DLL)
#include "../qcommon/qcommon.h"
#include "../qcommon/stringresource.hpp"
#include "../qcommon/stringresource.h"

StringResource *StringResource::_instance = NULL;

StringResource::StringResource()
{
	memset(_stringHashTable, 0, sizeof(_stringHashTable));
	_notFoundString      = "";
	_loadedLevel         = "";
	_globalLoaded        = false;
	_overwrite           = false;
	_languageCvar        = Cvar_Get("local_language", "Eng", 1);
}

StringResource::~StringResource()
{
}

void StringResource::createInstance(void)
{
	if (!_instance) {
		_instance = new StringResource();
	}
}

void StringResource::deleteInstance(void)
{
	if (_instance) {
		_instance->clearAllStrings();
		delete _instance;
		_instance = NULL;
	}
}

int StringResource::getHashValue(const str &stringId)
{
	int    index = 0;
	short  hash  = 0;

	while (1) {
		if (index >= (int)stringId.length()) {
			break;
		}
		char ch = (char)tolower(stringId[index]);
		if (ch == '%') {
			++index;
			continue;
		}
		if (ch == '.') {
			return hash & 0x3FF;
		}
		if (ch == '\\') {
			ch = '/';
		}
		short v7 = ch * (index++ + 119);
		hash += v7;
		++index;
	}
	return hash & 0x3FF;
}

str &StringResource::getStringResource(const str &stringId)
{
	str stripped;

	for (int i = 2; i < (int)stringId.length() - 2; ++i) {
		stripped += stringId[i];
	}

	StringEntry *entry = _stringHashTable[getHashValue(stripped)];
	while (entry) {
		if (entry->getStringId().length() == stripped.length()
			&& !strcmp(entry->getStringId().c_str(), stripped.c_str())) {
			break;
		}
		entry = entry->getNext();
	}

	if (!entry) {
		_notFoundString = stringId;
		return _notFoundString;
	}
	return entry->getString();
}


void StringResource::parseStringValue(str &stringValue)
{
	str  decoded;
	int  len = (int)stringValue.length();

	for (int i = 0; i < len; ++i) {
		char ch = stringValue[i];
		if (ch == '\\' && i + 1 < len) {
			char nx = stringValue[i + 1];
			if (nx == 'n') {
				decoded += '\n';
				++i;
				continue;
			}
			if (nx == 't') {
				decoded += '\t';
				++i;
				continue;
			}
		}
		decoded += ch;
	}

	stringValue = decoded;
}

bool StringResource::addStringEntry(StringEntry *stringEntry)
{
	StringEntry **head = &_stringHashTable[getHashValue(stringEntry->getStringId())];

	if (!*head) {
		*head = stringEntry;
		stringEntry->setNext(NULL);
		return true;
	}

	StringEntry *prev  = *head;
	StringEntry *entry = *head;

	while (entry) {
		if (entry->getStringId().length() == stringEntry->getStringId().length()
			&& !strcmp(entry->getStringId().c_str(), stringEntry->getStringId().c_str())) {
			break;
		}
		prev = entry;
		entry = entry->getNext();
	}

	if (!entry) {
		prev->setNext(stringEntry);
		stringEntry->setNext(NULL);
		return true;
	}

	if (_overwrite) {
		entry->setString(stringEntry->getString());
		entry->setType(stringEntry->getType());
	} else {
		Com_WPrintf("Identical string id: %s\n", stringEntry->getStringId().c_str());
	}
	return false;
}

bool StringResource::loadStrings(const str &fileName, StringType stringType)
{
	char *buffer = NULL;

	// ever diverge from retail, never help.
	FS_ReadFileEx(fileName.c_str(), (void **)&buffer, qtrue);
	if (!buffer) {
		return false;
	}

	char *data = buffer;

	// Both strs live across the loop, as in retail.  keyString MUST be copied
	str keyString;
	str valueString;

	while (data) {
		const char *key = COM_ParseExt(&data, qtrue);
		if (!key[0]) {
			break;
		}
		keyString = key;

		const char *value = COM_ParseExt(&data, qfalse);
		if (!value[0]) {
			Com_WPrintf("Invalid file %s, line number %d\n", fileName.c_str(), COM_GetCurrentParseLine());
			break;
		}

		valueString = value;
		parseStringValue(valueString);

		StringEntry *entry = new StringEntry();
		entry->setString(valueString);
		entry->setStringId(keyString);
		entry->setType(stringType);
		if (!addStringEntry(entry)) {
			delete entry;
		}
	}

	FS_FreeFile(buffer);
	return true;
}

void StringResource::loadGlobalStrings(void)
{
	if (_globalLoaded) {
		return;
	}

	_overwrite = true;

	if (_languageCvar) {
		const char *lang = _languageCvar->string;

		str first = va("loc/%s/stringresource/globalstrings.loc", lang);
		Com_Printf("Loading global strings\n");
		if (!loadStrings(first, GLOBAL)) {
			Com_WPrintf("Unable to load %s\n", first.c_str());
		}

		for (int i = 1; i < 10; ++i) {
			loadStrings(str(va("loc/%s/stringresource/globalstrings%d.loc", lang, i)), GLOBAL);
		}

		_overwrite  = false;
		_globalLoaded = true;
	} else {
		Com_WPrintf("No language defined. Unable to load global strings.\n");
	}
}

void StringResource::loadLevelStrings(const str &levelName)
{
	qboolean sameAsLoaded = levelName.length() == _loadedLevel.length()
		&& !strcmp(levelName.c_str(), _loadedLevel.c_str());

	if (sameAsLoaded || !levelName.length()) {
		return;
	}

	_overwrite = false; // retail: level duplicates never override silently

	if (_languageCvar) {
		const char *lang = _languageCvar->string;

		str path = va("loc/%s/stringresource/%s.loc", lang, levelName.c_str());
		clearLevelStrings();
		Com_Printf("Loading level strings for level: %s\n", levelName.c_str());
		loadStrings(str(path), LEVEL);
		_loadedLevel = levelName;
	} else {
		Com_WPrintf("No language defined. Unable to load strings for level %s.\n", levelName.c_str());
	}
}

void StringResource::clearLevelStrings(void)
{
	for (int i = 0; i < STRING_HASH_SIZE; ++i) {
		StringEntry **head     = &_stringHashTable[i];
		StringEntry  *prevKept = NULL;
		StringEntry  *entry    = *head;

		while (entry) {
			StringEntry *next = entry->getNext();
			if (entry->getType() == LEVEL) {
				if (*head == entry) {
					*head = next;
				} else if (prevKept) {
					prevKept->setNext(next);
				}
				delete entry;
			} else {
				prevKept = entry;
			}
			entry = next;
		}
		if (!prevKept) {
			*head = NULL;
		}
	}
}

void StringResource::clearAllStrings(void)
{
	for (int i = 0; i < STRING_HASH_SIZE; ++i) {
		StringEntry *entry = _stringHashTable[i];
		while (entry) {
			StringEntry *next = entry->getNext();
			delete entry;
			entry = next;
		}
	}

	memset(_stringHashTable, 0, sizeof(_stringHashTable));
	_globalLoaded = false;
	_loadedLevel  = "";
}

void StringResource::convertStringResourceIds(str &text)
{
	str    pending;  
	str    output;   
	int    inside = 0;
	char   prev   = 0;
	int    len    = (int)text.length();

	for (int i = 0; i < len; ++i) {
		char ch = text[i];
		pending += ch;

		if (ch == '$' && prev == '$') {
			if (inside) {
				output.CapLength(output.length() - 2);
				output += getStringResource(pending);
				pending = "";
				inside  = 0;
			} else {
				output  += pending;
				pending  = "$$";
				inside   = 1;
			}
		}
		prev = ch;
	}

	if (pending.length()) {
		output += pending;
	}
	text = output;
}


extern "C" qboolean isStringResource(const char *text)
{
	size_t len = strlen(text);

	if (len <= 4) {
		return qfalse;
	}
	if (text[0] == '$' && text[1] == '$' && text[len - 1] == '$' && text[len - 2] == '$') {
		return qtrue;
	}
	return qfalse;
}

extern "C" const char *getStringResource(const char *stringId)
{
	StringResource::createInstance();

	str id = stringId ? stringId : "";
	return StringResource::getInstance()->getStringResource(id).c_str();
}

extern "C" void initializeStringResource(void)
{
	static qboolean initialized = qfalse;

	if (initialized) {
		return;
	}
	initialized = qtrue;

	StringResource::createInstance();
	StringResource::getInstance()->loadGlobalStrings();
}

extern "C" void uninitializeStringResource(void)
{
	StringResource::deleteInstance();
}

extern "C" void loadGlobalStrings(void)
{
	StringResource::createInstance();
	StringResource::getInstance()->loadGlobalStrings();
}

extern "C" void clearAllStrings(void)
{
	StringResource::createInstance();
	StringResource::getInstance()->clearAllStrings();
}

extern "C" void loadLevelStrings(const char *levelName)
{
	StringResource::createInstance();

	str name = levelName ? levelName : "";
	StringResource::getInstance()->loadLevelStrings(name);
}

extern "C" void convertStringResourceIds(const char *text, char *out, int size)
{
    str buffer;

    if (text) {
        buffer = text;
    }

    StringResource::createInstance();
    StringResource::getInstance()->convertStringResourceIds(buffer);

    if (out && size > 0) {
        strncpy(out, buffer.c_str(), size);
    }
}

struct MissionObjectives_s {
	str					_name;              
	str					_description;       
	int					_retailUnused[12];  // +56 .. +103 (never accessed in retail)
	qboolean			_showObjective;     
	qboolean			_objectiveComplete; 
	qboolean			_objectiveFailed;   
};

struct MissionInformation_s {
	str					_name;            
	str					_data;            
	str					_shader;          
	qboolean			_showInformation; 
};

static Container<MissionObjectives_s*>	mo_objectives;
static Container<MissionInformation_s*>	mi_informations;
static str								mo_currentObjectivesFile;

static const char s_objectivesPool[] =
	"\0"                                 
	"ui_infoText\0"                      
	"ui_addhud objectivenotify\n\0"      
	"ui_addhud objectivenotifytext\n\0"  
	"global/objectiveslist.txt\0"        
	"objective\0"                        
	"information\0"                      
	"}\0"                                
	"name\0"                             
	"desc\0"                             
	"shader\0"                           
	"data\0"                             
	"ui_currentInfoIndex\0"              
	"1\0"                                
	"ui_lastInfoChangeTime\0";           

#define MO_POOL               (s_objectivesPool)
#define MO_CVAR_INFO          (MO_POOL + 1)
#define MO_HUD_NOTIFY         (MO_POOL + 13)
#define MO_HUD_NOTIFYTEXT     (MO_POOL + 40)
#define MO_LISTFILE           (MO_POOL + 71)
#define MO_KW_OBJECTIVE       (MO_POOL + 97)
#define MO_KW_INFORMATION     (MO_POOL + 107)
#define MO_KW_CLOSE           (MO_POOL + 119)
#define MO_KW_NAME            (MO_POOL + 121)
#define MO_KW_DESC            (MO_POOL + 126)
#define MO_KW_SHADER          (MO_POOL + 131)
#define MO_KW_DATA            (MO_POOL + 138)
#define MO_CVAR_INFOINDEX     (MO_POOL + 143)
#define MO_CVAR_INFOINDEX_DEF (MO_POOL + 163)
#define MO_CVAR_INFOTIME      (MO_POOL + 165)

static MissionInformation_s *MissionInformation_Find(const char *name)
{
	for (int i = 1; i <= mi_informations.NumObjects(); ++i) {
		MissionInformation_s *info = mi_informations.ObjectAt(i);
		if (info && !Q_stricmp(info->_name.c_str(), name)) {
			return info;
		}
	}
	return NULL;
}

extern "C" void *MissionInformation_NewInformation(const char *name)
{
	for (int i = 1; i <= mi_informations.NumObjects(); ++i) {
		MissionInformation_s *info = mi_informations.ObjectAt(i);
		if (info && !Q_stricmp(info->_name.c_str(), name)) {
			return info;
		}
	}

	MissionInformation_s *info = new MissionInformation_s;
	if (name) {
		info->_name = name;
	}
	mi_informations.AddObject(info);
	return info;
}

extern "C" void MissionInformation_ClearInformationList(void)
{
	for (int i = mi_informations.NumObjects(); i > 0; --i) {
		delete mi_informations.ObjectAt(i);
	}
	mi_informations.FreeObjectList();
}

extern "C" const char *MissionInformation_GetNameFromIndex(unsigned int index)
{
	if (!index || index > (unsigned)mi_informations.NumObjects()) {
		return MO_POOL;  
	}
	MissionInformation_s *info = mi_informations.ObjectAt(index);
	return info ? info->_name.c_str() : MO_POOL;
}

extern "C" int MissionInformation_GetIndexFromName(const char *name)
{
	for (int i = 1; i <= mi_informations.NumObjects(); ++i) {
		MissionInformation_s *info = mi_informations.ObjectAt(i);
		if (info && !Q_stricmp(info->_name.c_str(), name)) {
			return i;
		}
	}
	return 0;
}

extern "C" const char *MissionInformation_GetInformationData(const char *name)
{
	MissionInformation_s *info = MissionInformation_Find(name);
	return info ? info->_data.c_str() : NULL;
}

extern "C" int MissionInformation_NumObjects(void)
{
	return mi_informations.NumObjects();
}

extern "C" const char *MissionInformation_DataAtIndex(int index)
{
	if (index < 1 || index > mi_informations.NumObjects()) {
		return NULL;
	}
	return mi_informations.ObjectAt(index)->_data.c_str();
}

extern "C" void MissionInformation_SetInformationData(const char *name, const char *data)
{
	MissionInformation_s *info = MissionInformation_Find(name);
	if (info) {
		info->_data = data;
	}
}

extern "C" const char *MissionInformation_GetShader(const char *name)
{
	MissionInformation_s *info = MissionInformation_Find(name);
	return info ? info->_shader.c_str() : NULL;
}

extern "C" void MissionInformation_SetShader(const char *name, const char *shader)
{
	MissionInformation_s *info = MissionInformation_Find(name);
	if (info) {
		info->_shader = shader;
	}
}

extern "C" qboolean MissionInformation_GetShowInformation(const char *name)
{
	MissionInformation_s *info = MissionInformation_Find(name);
	return info ? info->_showInformation : 0;
}

extern "C" void MissionInformation_SetShowInformation(const char *name, qboolean show)
{
	MissionInformation_s *info = MissionInformation_Find(name);
	if (info) {
		info->_showInformation = show;
	}
}

static MissionObjectives_s *MissionObjective_Find(const char *name)
{
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && !Q_stricmp(obj->_name.c_str(), name)) {
			return obj;
		}
	}
	return NULL;
}

extern "C" void *MissionObjective_NewObjective(const char *name)
{
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && !Q_stricmp(obj->_name.c_str(), name)) {
			return obj;
		}
	}

	MissionObjectives_s *obj = new MissionObjectives_s;
	if (name) {
		obj->_name = name;
	}
	mo_objectives.AddObject(obj);
	return obj;
}

extern "C" void MissionObjective_ClearObjectiveList(void)
{
	for (int i = mo_objectives.NumObjects(); i > 0; --i) {
		delete mo_objectives.ObjectAt(i);
	}
	mo_objectives.FreeObjectList();
	MissionInformation_ClearInformationList();
	mo_currentObjectivesFile = MO_POOL;
	Cvar_Set(MO_CVAR_INFO, MO_POOL);
}

extern "C" const char *MissionObjective_GetDescription(const char *name)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	return obj ? obj->_description.c_str() : NULL;
}

extern "C" void MissionObjective_SetDescription(const char *name, const char *desc)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	if (obj) {
		obj->_description = desc;
	}
}

extern "C" qboolean MissionObjective_GetShowObjective(const char *name)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	return obj ? obj->_showObjective : 0;
}

extern "C" void MissionObjective_SetShowObjective(const char *name, qboolean show)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	if (obj) {
		obj->_showObjective = show;
		if (show) {
			Cbuf_ExecuteText(0, MO_HUD_NOTIFY);
			Cbuf_ExecuteText(0, MO_HUD_NOTIFYTEXT);
		}
	}
}

extern "C" qboolean MissionObjective_GetObjectiveComplete(const char *name)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	return obj ? obj->_objectiveComplete : 0;
}

extern "C" void MissionObjective_SetObjectiveComplete(const char *name, qboolean complete)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	if (obj) {
		obj->_objectiveComplete = complete;
	}
}

extern "C" qboolean MissionObjective_GetObjectiveFailed(const char *name)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	return obj ? obj->_objectiveFailed : 0;
}

extern "C" void MissionObjective_SetObjectiveFailed(const char *name, qboolean failed)
{
	MissionObjectives_s *obj = MissionObjective_Find(name);
	if (obj) {
		obj->_objectiveFailed = failed;
	}
}

extern "C" int MissionObjective_GetNumObjectives(void)
{
	return mo_objectives.NumObjects();
}

extern "C" int MissionObjective_GetNumActiveObjectives(void)
{
	int count = 0;
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && obj->_showObjective) {
			++count;
		}
	}
	return count;
}

extern "C" int MissionObjective_GetNumCompleteObjectives(void)
{
	int count = 0;
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && obj->_objectiveComplete) {
			++count;
		}
	}
	return count;
}

extern "C" int MissionObjective_GetNumFailedObjectives(void)
{
	int count = 0;
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && obj->_objectiveFailed) {
			++count;
		}
	}
	return count;
}

extern "C" int MissionObjective_GetNumIncompleteObjectives(void)
{
	int count = 0;
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && !obj->_objectiveFailed && !obj->_objectiveComplete && obj->_showObjective) {
			++count;
		}
	}
	return count;
}

extern "C" const char *MissionObjective_GetNameFromIndex(unsigned int index)
{
	if (!index || index > (unsigned)mo_objectives.NumObjects()) {
		return MO_POOL;
	}
	MissionObjectives_s *obj = mo_objectives.ObjectAt(index);
	return obj ? obj->_name.c_str() : MO_POOL;
}

extern "C" int MissionObjective_GetIndexFromName(const char *name)
{
	for (int i = 1; i <= mo_objectives.NumObjects(); ++i) {
		MissionObjectives_s *obj = mo_objectives.ObjectAt(i);
		if (obj && !Q_stricmp(obj->_name.c_str(), name)) {
			return i;
		}
	}
	return 0;
}


static void ReadInObjective(Script *script)
{
	str token;
	str name;
	str desc;

	while (Q_stricmp(token.c_str(), MO_KW_CLOSE)) {
		token = script->GetToken(true);

		if (!Q_stricmp(token.c_str(), MO_KW_NAME)) {
			name = script->GetToken(false);
		}

		if (!Q_stricmp(token.c_str(), MO_KW_DESC)) {
			str value = script->GetToken(false);
			if (isStringResource(value.c_str())) {
				desc = getStringResource(value.c_str());
			} else {
				desc = value;
			}
		}
	}

	if (name.length() > 0) {
		MissionObjective_NewObjective(name.c_str());
		MissionObjectives_s *obj = MissionObjective_Find(name.c_str());
		if (obj) {
			obj->_description = desc;
			obj->_objectiveComplete = 0;
			obj->_showObjective = 0;
			obj->_objectiveFailed = 0;
		}
	}
}

static void ReadInInformation(Script *script)
{
	str token;
	str name;
	str shader;
	str data;

	while (Q_stricmp(token.c_str(), MO_KW_CLOSE)) {
		token = script->GetToken(true);

		if (!Q_stricmp(token.c_str(), MO_KW_NAME)) {
			name = script->GetToken(false);
		}

		if (!Q_stricmp(token.c_str(), MO_KW_SHADER)) {
			shader = script->GetToken(false);
		}

		if (!Q_stricmp(token.c_str(), MO_KW_DATA)) {
			str value = script->GetToken(false);
			if (isStringResource(value.c_str())) {
				data = getStringResource(value.c_str());
			} else {
				data = value;
			}
		}
	}

	if (name.length() > 0) {
		MissionInformation_NewInformation(name.c_str());
		MissionInformation_s *info = (MissionInformation_s *)MissionInformation_Find(name.c_str());
		if (info) {
			info->_shader = shader;
			info->_data = data;
			info->_showInformation = 0;
		}
	}
}

extern "C" qboolean MissionObjectives_ParseObjectiveFile(const char *objectivesName)
{
	Script script;
	str    token;

	mo_currentObjectivesFile = MO_POOL;

	MissionObjective_ClearObjectiveList();
	Cvar_Set(MO_CVAR_INFO, mo_currentObjectivesFile.c_str());

	if (!FS_Exists(MO_LISTFILE)) {
		return false;
	}

	script.LoadFile(MO_LISTFILE);

	while (script.TokenAvailable(true)) {
		token = script.GetToken(false);

		if (!Q_stricmp(token.c_str(), objectivesName)) {
			while (Q_stricmp(token.c_str(), MO_KW_CLOSE)) {
				token = script.GetToken(true);

				if (!Q_stricmp(token.c_str(), MO_KW_OBJECTIVE)) {
					ReadInObjective(&script);
				}
				if (!Q_stricmp(token.c_str(), MO_KW_INFORMATION)) {
					ReadInInformation(&script);
				}
			}
		}
	}

	script.Close();
	return true;
}

extern "C" void MissionObjectives_Update(const char *objectivesName)
{
	if (Q_stricmp(mo_currentObjectivesFile.c_str(), objectivesName)) {
		MissionObjectives_ParseObjectiveFile(objectivesName);
		mo_currentObjectivesFile = objectivesName;
	}
}

#endif 
