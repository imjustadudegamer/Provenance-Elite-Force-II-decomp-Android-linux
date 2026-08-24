
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    const char  *TIKI_FileExtension(const char *in);
    void         TIKI_ParseFrameCommands(
        dloaddef_t       *ld,
        dloadframecmd_t **cmdlist,
        int               maxcmds,
        int              *numcmds,
        const char       *filename,
        const char       *animname
        );
    void         TIKI_ParseAnimationCommands(dloaddef_t *ld, dloadanim_t *anim, const char *filename);
    void         TIKI_ParseAnimationFlags(dloaddef_t *ld, dloadanim_t *anim);
    void         TIKI_ParseAnimationsFail(dloaddef_t *ld);
    void         TIKI_ParseAnimations(dloaddef_t *ld);
    void         TIKI_ParseExpressions(dloaddef_t *ld, const char *filename);
    void         TIKI_ParseExpressionTargets(dloaddef_t *ld, dloadexpression_t *expression, const char *filename);
    int          TIKI_ParseSurfaceFlag(const char *token);
    void         TIKI_InitSetup(dloaddef_t *ld);
    dloadanim_t *TIKI_AllocAnim(dloaddef_t *ld);

#ifdef __cplusplus
    qboolean TIKI_LoadSetupCase(
        dtiki_t        *tiki,
        const char     *filename,
        dloadsurface_t *loadsurfaces,
        int            *numSurfacesSetUp,
        msg_t          *msg
    );
    qboolean TIKI_LoadSetup(
        dtiki_t        *tiki,
        const char     *filename,
        dloadsurface_t *loadsurfaces,
        int            *numSurfacesSetUp,
        byte           *modelData,
        size_t          modelDataSize
    );
#endif

    qboolean TIKI_ParseSetup(dloaddef_t *ld);
    void     TIKI_ParseInitCommands(dloaddef_t *ld, dloadinitcmd_t **cmdlist, int maxcmds, int *numcmds);
    void     TIKI_ParseInit(dloaddef_t *ld);

#ifdef __cplusplus
}
#endif

void WriteSkelmodel(dloaddef_t* ld, const char* name);
