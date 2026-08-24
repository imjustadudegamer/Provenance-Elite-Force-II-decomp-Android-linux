

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    qboolean TIKI_Frame_Commands_Server(dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd);
    // does not resolve.  Retail's companion TIKI_InitCommands_Server is @0x000FE580.
    qboolean TIKI_InitCommands_Client(dtiki_t *pmdl, tiki_cmd_t *tiki_cmd);
    qboolean
    TIKI_Frame_Commands_Client(dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd, int *extra_anims);
    qboolean TIKI_Frame_Commands_ClientTime(dtiki_t *pmdl, int animnum, float start, float end, tiki_cmd_t *tiki_cmd);

#ifdef __cplusplus
}
#endif
