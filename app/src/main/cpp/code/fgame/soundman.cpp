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


#include "g_local.h"
#include "debuglines.h"
#include "entity.h"
#include "trigger.h"
#include "player.h"
#include "soundman.h"
#include "scriptthread.h"

Event EV_SoundManager_AddSpeaker (	"addspeaker",
	EV_CONSOLE,
	NULL,
	NULL,
	"Add a new sound where the player is standing."
);


SoundManager SoundMan;


Event EV_SoundManager_AddRandomSpeaker (    "addrandomspeaker", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Add a new sound where the player is standing.");
Event EV_SoundManager_AddMusicTrigger (    "addmusictrigger", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Add a new music trigger where the player is standing.");
Event EV_SoundManager_AddReverbTrigger (    "addreverbtrigger", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Add a new reverb trigger where the player is standing.");
Event EV_SoundManager_Replace (    "replace", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Replace the current sound position with the player's.");
Event EV_SoundManager_Delete (    "delete", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Delete the current sound.");
Event EV_SoundManager_MovePlayer (    "moveplayer", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Move the player to the current sound position.");
extern Event EV_ScriptSlave_Next;

Event EV_SoundManager_Next (    "next",
    EV_CONSOLE,
    NULL,
    NULL,
    "Go to the next sound.");
Event EV_SoundManager_Previous (    "prev", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Go to the previous sound.");

extern Event EV_Show;


Event EV_SoundManager_Show
(
	"show",
	EV_DEFAULT,
	"E",
	"path",
	"Show all the sounds."
);

Event EV_SoundManager_Hide
(
	"hide",
	EV_DEFAULT,
	NULL,
	NULL,
	"Hides the sounds."
);

Event EV_SoundManager_ShowingSounds (    "_showing_sounds", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Internal event for showing the sounds.");
extern Event EV_Hide;


Event EV_SoundManager_Save (    "save", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Saves the sounds.");
Event EV_SoundManager_UpdateInput (    "updateinput", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Updates the current sound with user interface values.");

Event EV_SoundManager_GlobalTranslate (    "globaltranslate", 
    EV_CONSOLE,
    "v",
    "translate_amount",
    "Translates all sounds and triggers by specified amount.");

Event EV_SoundManager_SwitchFacet (    "switchfacet", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Switch the current facet that we are editing.");

Event EV_SoundManager_PreviewReverb (    "previewreverb", 
    EV_CONSOLE,
    NULL,
    NULL,
    "Test out the current reverb settings.");

Event EV_SoundManager_ResetReverb (    "resetreverb", 
    EV_CONSOLE,
    NULL,
    NULL,
    "reset the reverb settings to a normal.");

Event EV_SoundManager_Reset (    "reset",
    EV_CONSOLE,
    NULL,
    NULL,
    "Resets the state of all sounds and triggers.");

CLASS_DECLARATION(Listener, SoundManager, NULL) {
    {&EV_SoundManager_AddSpeaker,       &SoundManager::AddSpeaker          },
    {&EV_SoundManager_AddRandomSpeaker, &SoundManager::AddRandomSpeaker    },
    {&EV_SoundManager_AddMusicTrigger,  &SoundManager::AddMusicTrigger     },
    {&EV_SoundManager_AddReverbTrigger, &SoundManager::AddReverbTrigger    },
    {&EV_SoundManager_Replace,          &SoundManager::Replace             },
    {&EV_SoundManager_Delete,           &SoundManager::Delete              },
    {&EV_SoundManager_MovePlayer,       &SoundManager::MovePlayer          },
    {&EV_SoundManager_Next,             &SoundManager::Next                },
    {&EV_SoundManager_Previous,         &SoundManager::Previous            },
    {&EV_SoundManager_Show,          &SoundManager::Show                },
    {&EV_SoundManager_ShowingSounds,    &SoundManager::ShowingSounds       },
    {&EV_SoundManager_Hide,          &SoundManager::Hide                },
    {&EV_SoundManager_Save,             &SoundManager::Save                },
    {&EV_SoundManager_UpdateInput,      &SoundManager::UpdateEvent         },
    {&EV_SoundManager_Reset,            &SoundManager::ResetEvent          },
    {&EV_SoundManager_GlobalTranslate,  &SoundManager::GlobalTranslateEvent},
    {&EV_SoundManager_SwitchFacet,      &SoundManager::SwitchFacetEvent    },
    {&EV_SoundManager_PreviewReverb,    &SoundManager::PreviewReverbEvent  },
    {&EV_SoundManager_ResetReverb,      &SoundManager::ResetReverbEvent    },
    {NULL,                              NULL                               }
};

Player *SoundManager_GetPlayer(void)
{
    assert(g_entities[0].entity && g_entities[0].entity->isSubclassOf(Player));
    if (!g_entities[0].entity || !(g_entities[0].entity->isSubclassOf(Player))) {
        gi.WPrintf("No player found.\n");
        return NULL;
    }

    return (Player *)g_entities[0].entity;
}

SoundManager::SoundManager()
{
    currentFacet = 0;
    Reset();
}

void SoundManager::Reset(void)
{
    current = NULL;
    soundList.ClearObjectList();
}

void SoundManager::UpdateUI(void)
{
    if (current) {
        gi.cvar_set("snd_multifaceted", "");
        gi.cvar_set("snd_currentfacet", "");
        gi.cvar_set("snd_onetime", "0");
        gi.cvar_set("snd_useangles", "0");
        gi.cvar_set("snd_yaw", "0");

        gi.cvar_set("snd_origin", va("%.2f %.2f %.2f", current->origin[0], current->origin[1], current->origin[2]));
        gi.cvar_set("snd_targetname", current->targetname.c_str());
        if (current->isSubclassOf(TriggerSpeaker)) {
            TriggerSpeaker *speaker;

            speaker = (TriggerSpeaker *)current;
            if (speaker->volume != DEFAULT_VOL) {
                gi.cvar_set("snd_volume", va("%.1f", speaker->volume));
            } else {
                gi.cvar_set("snd_volume", "Default");
            }
            if (speaker->min_dist != DEFAULT_MIN_DIST) {
                gi.cvar_set("snd_mindist", va("%.1f", speaker->min_dist));
            } else {
                gi.cvar_set("snd_mindist", "Default");
            }

            gi.cvar_set("ui_pickedsound", speaker->Noise().c_str());

            if (current->isSubclassOf(RandomSpeaker)) {
                RandomSpeaker *random;

                random = (RandomSpeaker *)current;
                gi.cvar_set("snd_mindelay", va("%.1f", random->mindelay));
                gi.cvar_set("snd_maxdelay", va("%.1f", random->maxdelay));
                gi.cvar_set("snd_channel", va("%d", random->channel));
                gi.cvar_set("snd_chance", va("%.1f", random->chance));
                gi.cvar_set("snd_type", "RandomSpeaker");
            } else {
                gi.cvar_set("snd_type", "Speaker");
            }
        } else if (current->isSubclassOf(TriggerMusic) || current->isSubclassOf(TriggerReverb)) {
            int      multiFaceted;
            Trigger *trigger;

            trigger = (Trigger *)current;
            gi.cvar_set("snd_width", va("%.0f", trigger->maxs[0]));
            gi.cvar_set("snd_length", va("%.0f", trigger->maxs[1]));
            gi.cvar_set("snd_height", va("%.0f", trigger->maxs[2]));
            if (trigger->UsingTriggerDir()) {
                gi.cvar_set("snd_useangles", "1");
            } else {
                gi.cvar_set("snd_useangles", "0");
            }
            gi.cvar_set("snd_yaw", va("%.0f", trigger->angles[1]));
            multiFaceted = trigger->GetMultiFaceted();
            if (multiFaceted) {
                if (multiFaceted == 1) {
                    gi.cvar_set("snd_multifaceted", "North/South");
                    if (currentFacet) {
                        gi.cvar_set("snd_currentfacet", "South");
                    } else {
                        gi.cvar_set("snd_currentfacet", "North");
                    }
                } else {
                    gi.cvar_set("snd_multifaceted", "East/West");
                    if (currentFacet) {
                        gi.cvar_set("snd_currentfacet", "West");
                    } else {
                        gi.cvar_set("snd_currentfacet", "East");
                    }
                }
            } else {
                gi.cvar_set("snd_multifaceted", "Not");
            }

            if (current->isSubclassOf(TriggerMusic)) {
                TriggerMusic *music;

                music = (TriggerMusic *)trigger;

                gi.cvar_set("snd_type", "MusicTrigger");
                if (music->oneshot) {
                    gi.cvar_set("snd_onetime", "1");
                } else {
                    gi.cvar_set("snd_onetime", "0");
                }

                if (!currentFacet) {
                    gi.cvar_set("snd_currentmood", music->current.c_str());
                    gi.cvar_set("snd_fallbackmood", music->fallback.c_str());
                } else {
                    gi.cvar_set("snd_currentmood", music->altcurrent.c_str());
                    gi.cvar_set("snd_fallbackmood", music->altfallback.c_str());
                }
            } else if (current->isSubclassOf(TriggerReverb)) {
                TriggerReverb *reverb;

                reverb = (TriggerReverb *)trigger;

                gi.cvar_set("snd_type", "ReverbTrigger");
                if (reverb->oneshot) {
                    gi.cvar_set("snd_onetime", "1");
                } else {
                    gi.cvar_set("snd_onetime", "0");
                }

                if (!currentFacet) {
                    gi.cvar_set("snd_reverbtypedisplay", EAXMode_NumToName(reverb->reverbtype));
                    gi.cvar_set("snd_reverbtype", va("%d", reverb->reverbtype));
                    gi.cvar_set("snd_reverblevel", va("%.2f", reverb->reverblevel));
                } else {
                    gi.cvar_set("snd_reverbtypedisplay", EAXMode_NumToName(reverb->altreverbtype));
                    gi.cvar_set("snd_reverbtype", va("%d", reverb->altreverbtype));
                    gi.cvar_set("snd_reverblevel", va("%.2f", reverb->altreverblevel));
                }
            }
        }
        if (EventPending(EV_SoundManager_ShowingSounds)) {
            gi.cvar_set("snd_hiddenstate", "visible");
        } else {
            gi.cvar_set("snd_hiddenstate", "hidden");
        }
        gi.cvar_set("snd_speakernum", va("%d", soundList.IndexOfObject(current) - 1));
    }
}

void SoundManager::UpdateRandomSpeaker(RandomSpeaker *speaker)
{
    cvar_t *cvar;
    Vector  tempvec;

    if (!speaker) {
        return;
    }

    cvar = gi.cvar("snd_origin", "", 0);
    sscanf(cvar->string, "%f %f %f", &tempvec[0], &tempvec[1], &tempvec[2]);
    speaker->setOrigin(tempvec);

    cvar = gi.cvar("snd_targetname", "", 0);
    speaker->SetTargetName(cvar->string);

    cvar = gi.cvar("snd_volume", "", 0);
    if (str(cvar->string) == "Default") {
        speaker->SetVolume(DEFAULT_VOL);
    } else {
        speaker->SetVolume(cvar->value);
    }

    cvar = gi.cvar("snd_mindist", "", 0);
    if (str(cvar->string) == "Default") {
        speaker->SetMinDist(DEFAULT_MIN_DIST);
    } else {
        speaker->SetMinDist(cvar->value);
    }

    cvar = gi.cvar("snd_mindelay", "", 0);
    speaker->SetMinDelay(cvar->value);

    cvar = gi.cvar("snd_maxdelay", "", 0);
    speaker->SetMaxDelay(cvar->value);

    cvar = gi.cvar("snd_chance", "", 0);
    speaker->SetChance(cvar->value);

    cvar = gi.cvar("ui_pickedsound", "", 0);
    if (cvar->string[0]) {
        speaker->SetNoise(cvar->string);
        speaker->ScheduleSound();
    }
}

void SoundManager::UpdateSpeaker(TriggerSpeaker *speaker)
{
    cvar_t *cvar;
    Vector  tempvec;

    if (!speaker) {
        return;
    }

    speaker->ambient = true;

    cvar = gi.cvar("snd_origin", "", 0);
    sscanf(cvar->string, "%f %f %f", &tempvec[0], &tempvec[1], &tempvec[2]);
    speaker->setOrigin(tempvec);

    cvar = gi.cvar("snd_targetname", "", 0);
    speaker->SetTargetName(cvar->string);

    cvar = gi.cvar("snd_volume", "", 0);
    if (str(cvar->string) == "Default") {
        speaker->SetVolume(DEFAULT_VOL);
    } else {
        speaker->SetVolume(cvar->value);
    }

    cvar = gi.cvar("snd_mindist", "", 0);
    if (str(cvar->string) == "Default") {
        speaker->SetMinDist(DEFAULT_MIN_DIST);
    } else {
        speaker->SetMinDist(cvar->value);
    }

    cvar = gi.cvar("ui_pickedsound", "", 0);
    if (cvar->string[0]) {
        speaker->SetNoise(cvar->string);
        speaker->StartSound();
    }
}

void SoundManager::UpdateTriggerMusic(TriggerMusic *music)
{
    cvar_t *cvar;
    str     currentMood;
    str     fallback;
    str     faceted;
    Vector  tempvec;

    if (!music) {
        return;
    }


    music->SetOneShot(false);
    music->useTriggerDir = false;
    music->triggerable = false;

    cvar = gi.cvar("snd_origin", "", 0);
    sscanf(cvar->string, "%f %f %f", &tempvec[0], &tempvec[1], &tempvec[2]);
    music->setOrigin(tempvec);

    cvar = gi.cvar("snd_targetname", "", 0);
    music->SetTargetName(cvar->string);

    cvar           = gi.cvar("snd_width", "", 0);
    music->mins[0] = -cvar->value;
    music->maxs[0] = cvar->value;

    cvar           = gi.cvar("snd_length", "", 0);
    music->mins[1] = -cvar->value;
    music->maxs[1] = cvar->value;

    cvar           = gi.cvar("snd_height", "", 0);
    music->mins[2] = 0;
    music->maxs[2] = cvar->value;

    music->setSize(music->mins, music->maxs);

    cvar    = gi.cvar("snd_multifaceted", "", 0);
    faceted = cvar->string;

    if (faceted == "North/South") {
        music->SetMultiFaceted(1);
    } else if (faceted == "East/West") {
        music->SetMultiFaceted(2);
    } else {
        music->SetMultiFaceted(0);
    }

    cvar    = gi.cvar("snd_currentmood", "", 0);
    currentMood = cvar->string;

    cvar     = gi.cvar("snd_fallbackmood", "", 0);
    fallback = cvar->string;

    if (music->multiFaceted && currentFacet) {
        music->SetAltMood(currentMood, fallback);
    } else {
        music->SetMood(currentMood, fallback);
    }

    cvar = gi.cvar("snd_onetime", "", 0);
    if (cvar->integer) {
        music->SetOneShot(true);
    }

    cvar               = gi.cvar("snd_yaw", "", 0);
    music->angles[YAW] = cvar->value;
    music->setAngles();

    cvar = gi.cvar("snd_useangles", "", 0);
    if (cvar->integer) {
        music->SetTriggerDir(music->angles[YAW]);
    }
    UpdateUI();
}

void SoundManager::UpdateTriggerReverb(TriggerReverb *reverb)
{
    cvar_t *cvar;
    int     reverbtype;
    float   reverblevel;
    str     faceted;
    Vector  tempvec;

    if (!reverb) {
        return;
    }


    reverb->SetOneShot(false);
    reverb->useTriggerDir = false;
    reverb->triggerable = false;

    cvar = gi.cvar("snd_origin", "", 0);
    sscanf(cvar->string, "%f %f %f", &tempvec[0], &tempvec[1], &tempvec[2]);
    reverb->setOrigin(tempvec);

    cvar = gi.cvar("snd_targetname", "", 0);
    reverb->SetTargetName(cvar->string);

    cvar            = gi.cvar("snd_width", "", 0);
    reverb->mins[0] = -cvar->value;
    reverb->maxs[0] = cvar->value;

    cvar            = gi.cvar("snd_length", "", 0);
    reverb->mins[1] = -cvar->value;
    reverb->maxs[1] = cvar->value;

    cvar            = gi.cvar("snd_height", "", 0);
    reverb->mins[2] = 0;
    reverb->maxs[2] = cvar->value;

    reverb->setSize(reverb->mins, reverb->maxs);

    cvar    = gi.cvar("snd_multifaceted", "", 0);
    faceted = cvar->string;

    if (faceted == "North/South") {
        reverb->SetMultiFaceted(1);
    } else if (faceted == "East/West") {
        reverb->SetMultiFaceted(2);
    } else {
        reverb->SetMultiFaceted(0);
    }

    cvar       = gi.cvar("snd_reverbtype", "", 0);
    reverbtype = cvar->integer;

    cvar        = gi.cvar("snd_reverblevel", "", 0);
    reverblevel = cvar->value;

    if (reverb->multiFaceted && currentFacet) {
        reverb->SetAltReverb(reverbtype, reverblevel);
    } else {
        reverb->SetReverb(reverbtype, reverblevel);
    }

    cvar = gi.cvar("snd_onetime", "", 0);
    if (cvar->integer) {
        reverb->SetOneShot(true);
    }

    cvar                = gi.cvar("snd_yaw", "", 0);
    reverb->angles[YAW] = cvar->value;
    reverb->setAngles();

    cvar = gi.cvar("snd_useangles", "", 0);
    if (cvar->integer) {
        reverb->SetTriggerDir(reverb->angles[YAW]);
    }
    UpdateUI();
}

void SoundManager::UpdateEvent(Event *ev)
{
    if (!current) {
        return;
    }
    if (current->isSubclassOf(RandomSpeaker)) {
        UpdateRandomSpeaker((RandomSpeaker *)current);
    } else if (current->isSubclassOf(TriggerSpeaker)) {
        UpdateSpeaker((TriggerSpeaker *)current);
    } else if (current->isSubclassOf(TriggerMusic)) {
        UpdateTriggerMusic((TriggerMusic *)current);
    } else if (current->isSubclassOf(TriggerReverb)) {
        UpdateTriggerReverb((TriggerReverb *)current);
    }
}

void SoundManager::AddSpeaker(Event *ev)
{
    Player *player;
    Vector  ang;
    Vector  pos;

    player = SoundManager_GetPlayer();
    if (player) {
        player->GetPlayerView(&pos, &ang);

        current = new TriggerSpeaker;
        current->setOrigin(pos);
        current->setAngles(ang);

        soundList.AddUniqueObject(current);

        Show();
    }
    UpdateUI();
}

void SoundManager::AddRandomSpeaker(Event *ev)
{
    Player *player;
    Vector  ang;
    Vector  pos;

    player = SoundManager_GetPlayer();
    if (player) {
        player->GetPlayerView(&pos, &ang);

        current = new RandomSpeaker;
        current->setOrigin(pos);
        current->setAngles(ang);

        soundList.AddUniqueObject(current);

        Show();
    }
    UpdateUI();
}

void SoundManager::AddMusicTrigger(Event *ev)
{
    Player *player;
    Vector  ang;

    player = SoundManager_GetPlayer();
    if (player) {
        player->GetPlayerView(NULL, &ang);

        current = new TriggerMusic;
        current->setOrigin(player->origin);
        current->setAngles(ang);
        current->setSize(Vector("-16 -16 0"), Vector("16 16 64"));
        ((TriggerMusic *)current)->triggerable = false;

        soundList.AddUniqueObject(current);

        Show();
    }
    UpdateUI();
}

void SoundManager::AddReverbTrigger(Event *ev)
{
    Player *player;
    Vector  ang;

    player = SoundManager_GetPlayer();
    if (player) {
        player->GetPlayerView(NULL, &ang);

        current = new TriggerReverb;
        current->setOrigin(player->origin);
        current->setAngles(ang);
        current->setSize(Vector("-16 -16 0"), Vector("16 16 64"));
        ((TriggerReverb *)current)->triggerable = false;

        soundList.AddUniqueObject(current);

        Show();
    }
    UpdateUI();
}

void SoundManager::Replace(Event *ev)
{
    Player *player;
    Vector  ang;
    Vector  pos;

    player = SoundManager_GetPlayer();
    if (current && player) {
        player->GetPlayerView(&pos, &ang);

        if (current->isSubclassOf(TriggerMusic) || current->isSubclassOf(TriggerReverb)) {
            current->setOrigin(player->origin);
        } else {
            current->setOrigin(pos);
        }
        current->setAngles(ang);
    }
    UpdateUI();
}

void SoundManager::Delete(Event *ev)
{
    int index = 0;

    if (!current) {
        return;
    }

    if (soundList.ObjectInList(current)) {
        index = soundList.IndexOfObject(current);
        soundList.RemoveObject(current);
    }
    current->PostEvent(EV_Remove, 0);

    if ((index > 0) && (index < soundList.NumObjects())) {
        current = soundList.ObjectAt(index);
        CurrentGainsFocus();
    } else {
        current = NULL;
    }

    UpdateUI();
}

void SoundManager::MovePlayer(Event *ev)
{
    Player *player;
    Vector  pos;

    player = SoundManager_GetPlayer();
    if (current && player) {
        player->GetPlayerView(&pos, NULL);

        if (current->isSubclassOf(TriggerMusic) || current->isSubclassOf(TriggerReverb)) {
            player->setOrigin(current->origin);
        } else {
            player->setOrigin(current->origin - pos + player->origin);
        }
        player->SetViewAngles(current->angles);
    }
}

void SoundManager::CurrentLostFocus(void)
{
    if (current) {
        if (current->isSubclassOf(TriggerMusic) || current->isSubclassOf(TriggerReverb)) {
            current->PostEvent(EV_Trigger_SetTriggerable, 0.1f);
        }
    }
}

void SoundManager::CurrentGainsFocus(void)
{
    if (current) {
        if (current->isSubclassOf(TriggerMusic) || current->isSubclassOf(TriggerReverb)) {
            current->ProcessEvent(EV_Trigger_SetNotTriggerable);
        }
    }
}

void SoundManager::Next(Event *ev)
{
    int index;

    currentFacet = 0;

    if (current) {
        index = soundList.IndexOfObject(current);
        if (index < soundList.NumObjects()) {
            index++;
        } else {
            index = 1;
        }
        CurrentLostFocus();
    } else {
        index = 1;
    }

    if (index <= soundList.NumObjects()) {
        current = soundList.ObjectAt(index);
        CurrentGainsFocus();
        UpdateUI();
    }
}

void SoundManager::Previous(Event *ev)
{
    int index;

    currentFacet = 0;

    if (current) {
        index = soundList.IndexOfObject(current);
        if (index > 1) {
            index--;
        } else {
            index = soundList.NumObjects();
        }
        CurrentLostFocus();
    } else {
        index = 1;
    }

    if (index <= soundList.NumObjects()) {
        current = soundList.ObjectAt(index);
        CurrentGainsFocus();
        UpdateUI();
    }
}

void SoundManager::ResetEvent(Event *ev)
{
    int     i;
    Entity *ent;

    for (i = 1; i <= soundList.NumObjects(); i++) {
        ent = soundList.ObjectAt(i);
        if (ent->isSubclassOf(TriggerSpeaker)) {
        } else if (ent->isSubclassOf(TriggerMusic)) {
            TriggerMusic *music;

            music = (TriggerMusic *)ent;
            music->SetOneShot(music->oneshot);
        } else if (ent->isSubclassOf(TriggerReverb)) {
            TriggerReverb *reverb;

            reverb = (TriggerReverb *)ent;
            reverb->SetOneShot(reverb->oneshot);
        }
    }
}

void SoundManager::GlobalTranslateEvent(Event *ev)
{
    int     i;
    Entity *ent;
    Vector  amount;

    amount = ev->GetVector(1);

    for (i = 1; i <= soundList.NumObjects(); i++) {
        ent = soundList.ObjectAt(i);
        ent->addOrigin(amount);
    }
}

void SoundManager::SwitchFacetEvent(Event *ev)
{
    if (current && current->isSubclassOf(Trigger)) {
        Trigger *trigger;

        trigger = (Trigger *)current;
        if (trigger->GetMultiFaceted() && !currentFacet) {
            currentFacet = 1;
        } else {
            currentFacet = 0;
        }
    }
    UpdateUI();
}

void SoundManager::PreviewReverbEvent(Event *ev)
{
    cvar_t *cvar;
    int     reverbtype;
    float   reverblevel;

    cvar       = gi.cvar("snd_reverbtype", "", 0);
    reverbtype = cvar->integer;

    cvar        = gi.cvar("snd_reverblevel", "", 0);
    reverblevel = cvar->value;

    if (g_entities[0].inuse && g_entities[0].client) {
        Player *client;

        client = (Player *)g_entities[0].entity;
        client->SetReverb(reverbtype, reverblevel);
    }
}

void SoundManager::ResetReverbEvent(Event *ev)
{
    if (g_entities[0].inuse && g_entities[0].client) {
        Player *client;

        client = (Player *)g_entities[0].entity;
        client->SetReverb("Generic", 0);
    }
}

void SoundManager::ShowingSounds(Event *ev)
{
    int     i;
    Entity *ent;

    for (i = 1; i <= soundList.NumObjects(); i++) {
        ent = soundList.ObjectAt(i);
        if (ent->isSubclassOf(TriggerSpeaker)) {
            TriggerSpeaker *speaker;

            speaker = (TriggerSpeaker *)ent;
            if (current == ent) {
                if (speaker->volume != DEFAULT_VOL) {
                    G_DrawDebugNumber(speaker->origin + Vector(0, 0, 10), speaker->volume, 0.5, 0, 1, 0, 2);
                }
                if (speaker->min_dist != DEFAULT_MIN_DIST) {
                    G_DrawDebugNumber(speaker->origin + Vector(0, 0, 20), speaker->min_dist, 0.5, 0, 0, 1, 0);
                }

                if (speaker->min_dist != DEFAULT_VOL) {
                    G_DebugCircle(speaker->origin, speaker->min_dist, 0, 1, 1, 1, true);
                    G_DebugCircle(speaker->origin, 2 * speaker->min_dist, 0, 0.5, 0.5, 0.5, true);
                    G_DebugCircle(speaker->origin, 4 * speaker->min_dist, 0, 0.25, 0.25, 0.25, true);
                }

                if (speaker->isSubclassOf(RandomSpeaker)) {
                    RandomSpeaker *random;

                    G_DebugPyramid(speaker->origin, 24, 1, 1, 0.5, 1);
                    random = (RandomSpeaker *)ent;

                    G_DrawDebugNumber(random->origin + Vector(0, 0, 30), random->mindelay, 0.5, 0.25, 0, 1, 1);
                    G_DrawDebugNumber(random->origin + Vector(0, 0, 38), random->maxdelay, 0.5, 0.25, 0, 1, 1);
                    G_DrawDebugNumber(random->origin + Vector(0, 0, 46), random->chance, 0.5, 0.25, 0, 1, 2);
                } else {
                    G_DebugPyramid(speaker->origin, 24, 1, 1, 0, 1);
                }
            } else {
                if (speaker->isSubclassOf(RandomSpeaker)) {
                    G_DebugPyramid(speaker->origin, 24, 1, 0, 1, 1);
                } else {
                    G_DebugPyramid(speaker->origin, 24, 1, 0, 0, 1);
                }
            }
        } else if (ent->isSubclassOf(TriggerMusic)) {
            int           facet;
            TriggerMusic *music;

            music = (TriggerMusic *)ent;

            if (current == ent) {
                if (music->oneshot) {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 0, 1, 1, 1);
                } else {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 1, 1, 0, 1);
                }
            } else {
                if (music->oneshot) {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 0, 0.5, 0.5, 1);
                } else {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 1, 0, 0, 1);
                }
            }

            if (music->useTriggerDir) {
                Vector org;

                org = ent->origin;
                org[2] += 0.5f * ent->maxs[2];

                G_DebugArrow(org, music->GetTriggerDir(), 48, 0.5, 1, 1, 1);
            }
            facet = music->GetMultiFaceted();
            if (facet) {
                if (current == ent) {
                    G_DebugHighlightFacet(
                        ent->origin, ent->mins, ent->maxs, (facet_t)(((facet - 1) << 1) + currentFacet), 1, 1, 1, 1
                    );
                } else {
                    G_DebugHighlightFacet(
                        ent->origin, ent->mins, ent->maxs, (facet_t)(((facet - 1) << 1) + 0), 0.6f, 0.3f, 0.2f, 0.1f
                    );
                    G_DebugHighlightFacet(
                        ent->origin, ent->mins, ent->maxs, (facet_t)(((facet - 1) << 1) + 1), 0.6f, 0.3f, 0.2f, 0.1f
                    );
                }
            }
        } else if (ent->isSubclassOf(TriggerReverb)) {
            int            facet;
            TriggerReverb *reverb;

            reverb = (TriggerReverb *)ent;

            if (current == ent) {
                if (reverb->oneshot) {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 0.5, 0.5, 1, 1);
                } else {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 0, 0.5, 1, 1);
                }
            } else {
                if (reverb->oneshot) {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 0.5, 0, 1, 1);
                } else {
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 0, 0, 1, 1);
                }
            }

            if (reverb->useTriggerDir) {
                Vector org;

                org = ent->origin;
                org[2] += 0.5f * ent->maxs[2];

                G_DebugArrow(org, reverb->GetTriggerDir(), 48, 0.5, 1, 1, 1);
            }
            facet = reverb->GetMultiFaceted();
            if (facet) {
                if (current == ent) {
                    G_DebugHighlightFacet(
                        ent->origin, ent->mins, ent->maxs, (facet_t)(((facet - 1) << 1) + currentFacet), 1, 1, 1, 1
                    );
                } else {
                    G_DebugHighlightFacet(
                        ent->origin, ent->mins, ent->maxs, (facet_t)(((facet - 1) << 1) + 0), 0.3f, 0.3f, 0.6f, 0.1f
                    );
                    G_DebugHighlightFacet(
                        ent->origin, ent->mins, ent->maxs, (facet_t)(((facet - 1) << 1) + 1), 0.3f, 0.3f, 0.6f, 0.1f
                    );
                }
            }
        }
    }
    PostEvent(EV_SoundManager_ShowingSounds, FRAMETIME);
}

void SoundManager::Show(void)
{
    CurrentGainsFocus();
    CancelEventsOfType(EV_SoundManager_ShowingSounds);
    PostEvent(EV_SoundManager_ShowingSounds, FRAMETIME);
    UpdateUI();
}

void SoundManager::Show(Event *ev)
{
    Show();
}

void SoundManager::Hide(Event *ev)
{
    CurrentLostFocus();
    CancelEventsOfType(EV_SoundManager_ShowingSounds);
    UpdateUI();
}

void SoundManager::Save(void)
{
    Entity *ent;
    str     buf;
    str     filename;
    // MUST be signed (SDK soundman.cpp:1300 declares `int i`).  As a size_t the
    int i;

    filename = "maps/";
    filename += level.mapname;
    for (i = (int)filename.length() - 1; i >= 0; i--) {
        if (filename.c_str()[i] == '.') {
            filename.CapLength( i );   
            break;
        }
    }

    filename += ".snd";

    gi.Printf("Saving soundmanager file to '%s'...\n", filename.c_str());

    buf = "";
    buf += va("//\n");
    buf += va("// Sound Manager File \"%s\", %d Sound Entities.\n", filename.c_str(), soundList.NumObjects());
    buf += va("//\n");

    buf += va("// TriggerSpeakers\n");
    for (i = 1; i <= soundList.NumObjects(); i++) {
        TriggerSpeaker *speaker;

        ent = soundList.ObjectAt(i);
        if (!ent->isSubclassOf(TriggerSpeaker) || ent->isSubclassOf(RandomSpeaker)) {
            continue;
        }

        speaker = (TriggerSpeaker *)ent;
        buf += "spawn TriggerSpeaker";
        if (ent->targetname != "") {
            buf += va(" targetname %s", ent->targetname.c_str());
        }
        buf += va(" origin \"%.2f %.2f %.2f\"", ent->origin.x, ent->origin.y, ent->origin.z);
        buf += " spawnflags 1";
        if (speaker->volume != DEFAULT_VOL) {
            buf += va(" volume %.2f", speaker->volume);
        }
        if (speaker->min_dist != DEFAULT_MIN_DIST) {
            buf += va(" min_dist %.1f", speaker->min_dist);
        }
        buf += va(" sound \"%s\"", speaker->Noise().c_str());
        buf += " _addtosoundmanager 0";
        buf += va("\n");
    }
    buf += va("// RandomSpeakers\n");
    for (i = 1; i <= soundList.NumObjects(); i++) {
        RandomSpeaker *speaker;

        ent = soundList.ObjectAt(i);
        if (!ent->isSubclassOf(TriggerSpeaker) || !ent->isSubclassOf(RandomSpeaker)) {
            continue;
        }

        speaker = (RandomSpeaker *)ent;
        buf += "spawn RandomSpeaker";
        if (ent->targetname != "") {
            buf += va(" targetname %s", ent->targetname.c_str());
        }
        buf += va(" origin \"%.2f %.2f %.2f\"", ent->origin.x, ent->origin.y, ent->origin.z);
        if (speaker->volume != DEFAULT_VOL) {
            buf += va(" volume %.2f", speaker->volume);
        }
        if (speaker->min_dist != DEFAULT_MIN_DIST) {
            buf += va(" min_dist %.1f", speaker->min_dist);
        }
        buf += va(" channel %d", speaker->channel);
        buf += va(" mindelay %.2f", speaker->mindelay);
        buf += va(" maxdelay %.2f", speaker->maxdelay);
        buf += va(" chance %.2f", speaker->chance);
        buf += va(" sound \"%s\"", speaker->Noise().c_str());
        buf += " _addtosoundmanager 0";
        buf += va("\n");
    }
    buf += va("// TriggerMusics\n");
    for (i = 1; i <= soundList.NumObjects(); i++) {
        TriggerMusic *music;

        ent = soundList.ObjectAt(i);
        if (!ent->isSubclassOf(TriggerMusic)) {
            continue;
        }

        music = (TriggerMusic *)ent;
        buf += "spawn TriggerMusic";
        if (ent->targetname != "") {
            buf += va(" targetname %s", ent->targetname.c_str());
        }
        buf += va(" origin \"%.2f %.2f %.2f\"", ent->origin.x, ent->origin.y, ent->origin.z);
        if (music->useTriggerDir) {
            buf += va(" angle %.1f", AngleMod(ent->angles.y));
        }
        if (music->oneshot) {
            buf += " oneshot 0";
        }
        buf += va(" current %s", music->current.c_str());

        buf += va(" fallback %s", music->fallback.c_str());

        if (music->GetMultiFaceted()) {
            buf += va(" multifaceted %d", music->GetMultiFaceted());

            buf += va(" altcurrent %s", music->altcurrent.c_str());

            buf += va(" altfallback %s", music->altfallback.c_str());
        }

        buf += va(" _setmins \"%.2f %.2f %.2f\"", ent->mins.x, ent->mins.y, ent->mins.z);

        buf += va(" _setmaxs \"%.2f %.2f %.2f\"", ent->maxs.x, ent->maxs.y, ent->maxs.z);
        buf += " _addtosoundmanager 0";
        buf += va("\n");
    }

    buf += va("// TriggerReverbs\n");
    for (i = 1; i <= soundList.NumObjects(); i++) {
        TriggerReverb *reverb;

        ent = soundList.ObjectAt(i);
        if (!ent->isSubclassOf(TriggerReverb)) {
            continue;
        }

        reverb = (TriggerReverb *)ent;
        buf += "spawn TriggerReverb";
        if (ent->targetname != "") {
            buf += va(" targetname %s", ent->targetname.c_str());
        }
        buf += va(" origin \"%.2f %.2f %.2f\"", ent->origin.x, ent->origin.y, ent->origin.z);
        if (reverb->useTriggerDir) {
            buf += va(" angle %.1f", AngleMod(ent->angles.y));
        }
        if (reverb->oneshot) {
            buf += " oneshot 0";
        }
        buf += va(" reverbtype %d", reverb->reverbtype);

        buf += va(" reverblevel %.2f", reverb->reverblevel);

        if (reverb->GetMultiFaceted()) {
            buf += va(" multifaceted %d", reverb->GetMultiFaceted());

            buf += va(" altreverbtype %d", reverb->altreverbtype);

            buf += va(" altreverblevel %.2f", reverb->altreverblevel);
        }

        buf += va(" _setmins \"%.2f %.2f %.2f\"", ent->mins.x, ent->mins.y, ent->mins.z);

        buf += va(" _setmaxs \"%.2f %.2f %.2f\"", ent->maxs.x, ent->maxs.y, ent->maxs.z);
        buf += " _addtosoundmanager 0";
        buf += va("\n");
    }

    buf += "end\n";

    gi.FS_WriteFile(filename.c_str(), (void *)buf.c_str(), buf.length() + 1);
    gi.Printf("done.\n");
}

void SoundManager::Save(Event *ev)
{
    Save();
}

void SoundManager::Load(void)
{
    str      filename;
    intptr_t i;

    filename = "maps/";
    filename += level.mapname;
    for (i = filename.length() - 1; i >= 0; i--) {
        if (filename.c_str()[i] == '.') {
            filename.CapLength( i );   
            break;
        }
    }

    filename += ".snd";

    if (gi.FS_ReadFile(filename.c_str(), NULL, true) != -1) {
        Reset();

        if (level.consoleThread) {
            level.consoleThread->Parse(filename.c_str());
        }
    }
}

void SoundManager::AddEntity(Entity *ent)
{
    if (ent) {
        soundList.AddUniqueObject(ent);
    }
}



void SoundManager::Init( void )
{
   currentFacet = 0;
   Reset();
}

