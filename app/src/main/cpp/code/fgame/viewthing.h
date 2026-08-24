/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

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


#pragma once

#include "animate.h"

class Viewthing;

class ViewMaster : public Listener
{
public:
    CLASS_PROTOTYPE(ViewMaster);

    EntityPtr current_viewthing;
    Container<str> _modelNamesArray;    
    Container<str> _setNamesArray;      
    Vector         _lastBaseOrigin;     
    bool           _spawnAtLastItemsOrigin;  
    int            _currentSetIdx;      
    int            _numberOfModelsInSet;  
    int            _numberOfSets;       

    ViewMaster();
    ~ViewMaster();
    void Init(void);
    void Next(Event *ev);
    void ResetOrigin(Event *ev);
    void Prev(Event *ev);
    void DeleteAll(Event *ev);
    void Spawn(Event *ev);
    void SetModelEvent(Event *ev);
    void PassEvent(Event *ev);

    void Archive(Archiver& arc) override;
    void SetFrame(Event *ev);
    void SetSelected(Event *ev);






public:
    void Copy ( Event *ev ); 
    void Delete ( Event *ev ); 
    void DeleteCurrentViewthing(); 
    void DisplayCurrentModelInSet(bool spawnAtLastOrigin=true); 
    void JumpToModel ( Event *ev ); 
    void LoadModelsInSet ( const str& setFilename); 
    void LoadSet ( Event *ev ); 
    void NextModelInSet ( Event *ev ); 
    void Offset ( Event *ev ); 
    void PrevModelInSet ( Event *ev ); 
    void PullToCamera ( Event *ev ); 
    void Save ( Event *ev ); 
    void SetXTranslation( Event *ev ); 
    void SetYTranslation( Event *ev ); 
    void SetZTranslation( Event *ev ); 
    void Shoot ( Event *ev ); 
    void SpawnFromTS( Event *ev ); 
    void ToggleDrop( Event *ev ); 
    void XTranslate ( Event *ev ); 
    void YTranslate ( Event *ev ); 
    void ZTranslate ( Event *ev ); 
    void _randomizeViewthing(Viewthing* viewthingPtr); 
    void _resetOrigin(); 
    void _selectNext(); 
    void _selectPrevious(); 
    void _setToPosition( const Vector& pos ); 
    void _updateViewthingCounts(int countAdjustment=0); 
    void spawnAtPosition(const str& modelName, const Vector& pos); 
};

inline void ViewMaster::Archive(Archiver& arc)
{

    arc.ArchiveSafePointer(&current_viewthing);
}

extern ViewMaster Viewmodel;

class Viewthing : public Entity
{
public:
    CLASS_PROTOTYPE(Viewthing);

    int    animstate;
    int    frame;
    int    lastframe;
    Vector baseorigin;
    byte   origSurfaces[MAX_MODEL_SURFACES];
    int    current_morph;    
    qboolean _static;        
    qboolean _selected;      
    int    _pulseCount;      

    Viewthing();
    void UpdateCvars(qboolean quiet = false);
    void PrintTime(Event *ev);
    void ThinkEvent(Event *ev);
    void LastFrameEvent(Event *ev);
    void ToggleAnimateEvent(Event *ev);
    void NextFrameEvent(Event *ev);
    void PrevFrameEvent(Event *ev);
    void NextAnimEvent(Event *ev);
    void PrevAnimEvent(Event *ev);
    void ScaleUpEvent(Event *ev);
    void ScaleDownEvent(Event *ev);
    void SetScaleEvent(Event *ev);
    void SetYawEvent(Event *ev);
    void SetPitchEvent(Event *ev);
    void SetRollEvent(Event *ev);
    void SetAnglesEvent(Event *ev);
    void AttachModel(Event *ev);
    void Delete(Event *ev);
    void DetachAll(Event *ev);
    void ChangeOrigin(Event *ev);
    void SaveSurfaces(Event *ev);
    void SetAnim(int num);  
    void SetAnim(Event *ev);

    void Archive(Archiver& arc) override;






public:
    void SetModelEvent( Event *ev ); 
    void ChangePitchEvent ( Event *ev ); 
    void ChangeRollEvent ( Event *ev ); 
    void ChangeYawEvent ( Event *ev ); 
    void Flash ( Event *ev ); 
    void Morph ( Event *ev ); 
    void NextMorph ( Event *ev ); 
    void PrevMorph ( Event *ev ); 
    void SetFrame ( Event *ev ); 
    void SetSelected( qboolean state = false ); 
    void SetStatic ( Event *ev ); 
    void Unmorph ( Event *ev ); 
};

inline void Viewthing::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveInteger(&animstate);
    arc.ArchiveInteger(&frame);
    arc.ArchiveInteger(&lastframe);
    arc.ArchiveVector(&baseorigin);
    arc.ArchiveRaw(origSurfaces, sizeof(origSurfaces));
    arc.ArchiveInteger(&current_morph);
}
