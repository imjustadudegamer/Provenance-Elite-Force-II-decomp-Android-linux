

#if defined( GAME_DLL )
#include "../fgame/script.h"
#else
#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include "../corepp/class.h"
#include "../corepp/vector.h"
#include "../corepp/str.h"

#if defined(ARCHIVE_SUPPORTED)
#    include "../fgame/archive.h"
#endif

#define TOKENCOMMENT  (';')
#define TOKENCOMMENT2 ('#')
#define TOKENEOL      ('\n')
#define TOKENSPACE      (' ')
#define TOKENSPECIAL    ('$')
#define TOKENCOMMA      (',')

#define SCRIPT_MAXTOKEN 512

typedef struct {
    qboolean tokenready;
    int      offset;
    int      line;
    char     token[SCRIPT_MAXTOKEN];
} scriptmarker_t;

typedef struct {
    str macroName;
    str macroText;
} macro;

class Script : public Class
{
protected:
    qboolean tokenready;

    str                filename;
    const char        *script_p;
    const char        *end_p;
    Container<macro *> macrolist;

    int  line;
    char token[SCRIPT_MAXTOKEN];

    qboolean releaseBuffer;
    qboolean hasError;

    qboolean AtComment(void);
    void     CheckOverflow(void);

public:
    const char *buffer;
    size_t      length;

    CLASS_PROTOTYPE(Script);

#if defined(ARCHIVE_SUPPORTED)
    void Archive(Archiver& arc) override;
#endif

    ~Script();
    Script(const char *filename);
    Script();

    void        Close(void);
    const char *Filename(void);
    int         GetLineNumber(void);
    void        Reset(void);
    void        MarkPosition(scriptmarker_t *mark);
    void        RestorePosition(const scriptmarker_t *mark);
    qboolean    SkipToEOL(void);
    void        SkipWhiteSpace(qboolean crossline);
    void        SkipNonToken(qboolean crossline);
    qboolean    TokenAvailable(qboolean crossline);
    qboolean    CommentAvailable(qboolean crossline);
    void        UnGetToken(void);
    qboolean    AtString(qboolean crossline);
    qboolean    AtOpenParen(qboolean crossline);
    qboolean    AtCloseParen(qboolean crossline);
    qboolean    AtComma(qboolean crossline);
    qboolean    AtDot(qboolean crossline);
    qboolean    AtAssignment(qboolean crossline);
    const char *GetToken(qboolean crossline);
    const char *GetLine(qboolean crossline);
    const char *GetRaw(void);
    const char *GetString(qboolean crossline);
    qboolean    GetSpecific(const char *string);
    qboolean    GetBoolean(qboolean crossline);
    int         GetInteger(qboolean crossline);
    double      GetDouble(qboolean crossline);
    float       GetFloat(qboolean crossline);
    Vector      GetVector(qboolean crossline);
    int         LinesInFile(void);
    void        Parse(const char *data, size_t length, const char *name);
    void        LoadFile(const char *name);
    void        LoadFile(const char *name, int length, const char *buf);
    const char *Token(void);
    void        AddMacroDefinition(qboolean crossline);
    const char *GetMacroString(const char *theMacroName);
    char       *EvaluateMacroString(const char *theMacroString);
    float       EvaluateMacroMath(float value, float newval, char oper);
    const char *GetExprToken(const char *ptr, char *token);
    const char *GrabNextToken(qboolean crossline);
    qboolean    isMacro(void);

    qboolean EndOfFile();
    qboolean isValid(void);

    Container<macro *> *GetMacroList() { return &macrolist; }

    void AddMacro(const char *name, const char *value);
};

#if defined(ARCHIVE_SUPPORTED)

inline void Script::Archive(Archiver& arc)
{
    int pos;

    arc.ArchiveBoolean(&tokenready);

    arc.ArchiveString(&filename);
    if (arc.Loading()) {
        LoadFile(filename.c_str());
    }

    if (!arc.Loading()) {
        pos = script_p - buffer;
    }
    arc.ArchiveInteger(&pos);
    if (arc.Loading()) {
        script_p = buffer + pos;
    }


    arc.ArchiveInteger(&line);
    arc.ArchiveRaw(&token, sizeof(token));

}
#endif

#endif

#endif 

#ifdef __cplusplus
extern "C" {
#endif

    void        *MissionObjective_NewObjective(const char *name);
    void         MissionObjective_ClearObjectiveList(void);
    const char  *MissionObjective_GetDescription(const char *name);
    void         MissionObjective_SetDescription(const char *name, const char *desc);
    qboolean     MissionObjective_GetShowObjective(const char *name);
    void         MissionObjective_SetShowObjective(const char *name, qboolean show);
    qboolean     MissionObjective_GetObjectiveComplete(const char *name);
    void         MissionObjective_SetObjectiveComplete(const char *name, qboolean complete);
    qboolean     MissionObjective_GetObjectiveFailed(const char *name);
    void         MissionObjective_SetObjectiveFailed(const char *name, qboolean failed);
    const char  *MissionObjective_GetNameFromIndex(unsigned int index);
    int          MissionObjective_GetIndexFromName(const char *name);
    qboolean     MissionObjectives_ParseObjectiveFile(const char *objectivesName);

    void        *MissionInformation_NewInformation(const char *name);
    void         MissionInformation_ClearInformationList(void);
    const char  *MissionInformation_GetShader(const char *name);
    void         MissionInformation_SetShader(const char *name, const char *shader);
    const char  *MissionInformation_GetInformationData(const char *name);
    void         MissionInformation_SetInformationData(const char *name, const char *data);
    const char  *MissionInformation_GetNameFromIndex(unsigned int index);
    int          MissionInformation_GetIndexFromName(const char *name);
    qboolean     MissionInformation_GetShowInformation(const char *name);
    void         MissionInformation_SetShowInformation(const char *name, qboolean show);

#ifdef __cplusplus
}
#endif
