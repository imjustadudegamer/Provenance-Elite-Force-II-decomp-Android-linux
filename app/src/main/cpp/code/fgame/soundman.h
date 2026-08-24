

#pragma once

#include "g_local.h"
#include "entity.h"
#include "trigger.h"

class SoundManager : public Listener
{
protected:
    int                 currentFacet;
    Entity             *current;
    Container<Entity *> soundList;

    void AddSpeaker(Event *ev);
    void AddRandomSpeaker(Event *ev);
    void AddMusicTrigger(Event *ev);
    void AddReverbTrigger(Event *ev);
    void Replace(Event *ev);
    void Delete(Event *ev);
    void MovePlayer(Event *ev);
    void Next(Event *ev);
    void Previous(Event *ev);
    void ShowingSounds(Event *ev);
    void Show(Event *ev);
    void Hide(Event *ev);
    void Save(Event *ev);
    void UpdateEvent(Event *ev);
    void ResetEvent(Event *ev);
    void GlobalTranslateEvent(Event *ev);
    void SwitchFacetEvent(Event *ev);
    void PreviewReverbEvent(Event *ev);
    void ResetReverbEvent(Event *ev);

    void UpdateUI(void);
    void CurrentLostFocus(void);
    void CurrentGainsFocus(void);
    void UpdateSpeaker(TriggerSpeaker *speaker);
    void UpdateRandomSpeaker(RandomSpeaker *speaker);
    void UpdateTriggerMusic(TriggerMusic *music);
    void UpdateTriggerReverb(TriggerReverb *reverb);

public:
    CLASS_PROTOTYPE(SoundManager);

    SoundManager();
    void Reset(void);
    void Load(void);
    void AddEntity(Entity *ent);
    void Archive(Archiver& arc) override;






public:
    void Show( void ); 
    void Save( void ); 
    void Init( void ); 
};

inline void SoundManager::Archive(Archiver& arc)
{
    int i;
    int num;
    // DELIBERATE, and identical to retail: SDK soundman.h:82 declares a LOCAL
    int shadowedCurrentFacet;


    arc.ArchiveInteger(&shadowedCurrentFacet);
    arc.ArchiveObjectPointer((Class **)&current);

    if (arc.Saving()) {
        num = soundList.NumObjects();
        arc.ArchiveInteger(&num);
    } else {
        soundList.ClearObjectList();
        arc.ArchiveInteger(&num);
        soundList.Resize(num);
    }
    for (i = 1; i <= num; i++) {
        arc.ArchiveObjectPointer((Class **)soundList.AddressOfObjectAt(i));
    }
}

extern SoundManager SoundMan;
