
#include "snd_local.h"
#include "../corepp/tiki.h"

void load_sfx_info()
{
    TikiScript  tiki;
    const char *token;
    int         current_sound_file;
    char        file_name[MAX_QPATH];

    const float fRangeFloor = 0.0f;
    const float fVolumeCeil = 1.0f;
    float       def_mindist = 160.0f;
    float       def_volume  = 1.0f;

    sfx_infos[0].name[0]            = 0;
    sfx_infos[0].max_factor         = -1.0;
    sfx_infos[0].loop_start         = -1;
    sfx_infos[0].loop_end           = -1;
    sfx_infos[0].max_number_playing = DEFAULT_SFX_NUMBER_PLAYING;
    sfx_infos[0].mindist            = def_mindist;
    sfx_infos[0].volume             = def_volume;
    sfx_infos[0].localsound         = qfalse;
    number_of_sfx_infos             = 1;

    for (current_sound_file = 0; current_sound_file < 10; current_sound_file++) {
        Com_sprintf(file_name, sizeof(file_name), "global/sound%d.txt", current_sound_file);

        if (!tiki.LoadFile(file_name, qtrue)) {
            continue;
        }

        Com_Printf("Loading %s\n", file_name);

        while (tiki.TokenAvailable(qtrue)) {
            token = tiki.GetToken(qtrue);

            if (!Q_stricmp(token, "sound")) {
                if (tiki.TokenAvailable(qtrue)) {
                    token = tiki.GetToken(qtrue);

                    if (number_of_sfx_infos == 1000) {
                        Com_DPrintf("Too many sound infos specified\n");
                        break;
                    }

                    Q_strncpyz(sfx_infos[number_of_sfx_infos].name, token, sizeof(sfx_infos[number_of_sfx_infos].name));
                    sfx_infos[number_of_sfx_infos].max_factor         = -1.f;
                    sfx_infos[number_of_sfx_infos].loop_start         = -1;
                    sfx_infos[number_of_sfx_infos].loop_end           = -1;
                    sfx_infos[number_of_sfx_infos].max_number_playing = DEFAULT_SFX_NUMBER_PLAYING;
                    sfx_infos[number_of_sfx_infos].mindist            = def_mindist;
                    sfx_infos[number_of_sfx_infos].volume             = def_volume;
                    sfx_infos[number_of_sfx_infos].localsound         = qfalse;
                    number_of_sfx_infos++;
                }
            } else if (!Q_stricmp(token, "mindist")) {
                if (tiki.TokenAvailable(qtrue)) {
                    sfx_info_t *pInfo = &sfx_infos[number_of_sfx_infos - 1];
                    float       fValue;

                    token  = tiki.GetToken(qtrue);
                    fValue = (float)atof(token);

                    pInfo->mindist = fValue;
                    if (fValue < fRangeFloor) {
                        pInfo->mindist = def_mindist;
                        Com_WDPrintf("Minimum distance (%f) out of range for sound %s\n", fValue, pInfo->name);
                    }
                }
            } else if (!Q_stricmp(token, "maxdist")) {
                if (tiki.TokenAvailable(qtrue)) {
                    tiki.GetToken(qtrue);
                }
            } else if (!Q_stricmp(token, "volume")) {
                if (tiki.TokenAvailable(qtrue)) {
                    sfx_info_t *pInfo = &sfx_infos[number_of_sfx_infos - 1];
                    float       fValue;

                    token  = tiki.GetToken(qtrue);
                    fValue = (float)atof(token);

                    pInfo->volume = fValue;
                    if (fValue < fRangeFloor || fValue > fVolumeCeil) {
                        pInfo->volume = def_volume;
                        Com_WDPrintf("Volume (%f) out of range for sound %s\n", fValue, pInfo->name);
                    }
                }
            } else if (!Q_stricmp(token, "defaultmindist")) {
                if (tiki.TokenAvailable(qtrue)) {
                    float fValue;

                    token       = tiki.GetToken(qtrue);
                    def_mindist = (float)atof(token);
                    if (def_mindist < fRangeFloor) {
                        fValue      = def_mindist;
                        def_mindist = 160.0f;
                        Com_WDPrintf("Default minimum distance (%f) out of range\n", fValue);
                    }
                }
            } else if (!Q_stricmp(token, "defaultmaxdist")) {
                if (tiki.TokenAvailable(qtrue)) {
                    tiki.GetToken(qtrue);
                }
            } else if (!Q_stricmp(token, "defaultvolume")) {
                if (tiki.TokenAvailable(qtrue)) {
                    float fValue;

                    token      = tiki.GetToken(qtrue);
                    def_volume = (float)atof(token);
                    if (def_volume < fRangeFloor || def_volume > fVolumeCeil) {
                        fValue     = def_volume;
                        def_volume = fVolumeCeil;
                        Com_WDPrintf("Default volume (%f) out of range\n", fValue);
                    }
                }
            } else if (!Q_stricmp(token, "loopstart")) {
                if (tiki.TokenAvailable(qtrue)) {
                    token                                         = tiki.GetToken(qtrue);
                    sfx_infos[number_of_sfx_infos - 1].loop_start = atoi(token);
                }
            } else if (!Q_stricmp(token, "loopend")) {
                if (tiki.TokenAvailable(qtrue)) {
                    token                                       = tiki.GetToken(qtrue);
                    sfx_infos[number_of_sfx_infos - 1].loop_end = atoi(token);
                }
            } else if (!Q_stricmp(token, "maxnumber")) {
                if (tiki.TokenAvailable(qtrue)) {
                    token                                                 = tiki.GetToken(qtrue);
                    sfx_infos[number_of_sfx_infos - 1].max_number_playing = atoi(token);
                }
            } else if (!Q_stricmp(token, "localsound")) {
                sfx_infos[number_of_sfx_infos - 1].localsound = qtrue;
            } else if (!Q_stricmp(token, "maxfactor")) {
                if (tiki.TokenAvailable(qtrue)) {
                    token = tiki.GetToken(qtrue);
                    sfx_infos[number_of_sfx_infos - 1].max_factor = (float)atof(token);
                }
            }
        }
    }

    tiki.Close();

    sfx_infos[0].name[0] = 0;
}
