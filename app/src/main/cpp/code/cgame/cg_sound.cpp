

#include "cg_local.h"

void CG_ProcessSound(server_sound_t *sound)
{
    if (sound->stop_flag) {
        cgi.S_StopSound(sound->entity_number, sound->channel);
    } else {
        cgi.S_StartSound(
            sound->origin,
            sound->entity_number,
            sound->channel,
            cgs.sound_precache[sound->sound_index],
            sound->volume,
            sound->min_dist,
            sound->pitch_modifier
        );
    }
}
