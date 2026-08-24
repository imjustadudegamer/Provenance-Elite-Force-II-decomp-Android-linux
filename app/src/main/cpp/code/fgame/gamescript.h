//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/gamescript.h                                  $
// $Revision:: 4                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Subclass of script that preprocesses labels
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#pragma once

#include "../corepp/class.h"
#include "../corepp/script.h"
#include "../corepp/const_str.h"
#include "archive.h"

class Listener;
class CThread;
class ScriptVariable;


typedef struct {
    scriptmarker_t pos;
    str            labelname;
} script_label_t;

class GameScript;

class GameScriptMarker : public Class
{
public:
    CLASS_PROTOTYPE(GameScriptMarker);

    str             filename;
    scriptmarker_t  scriptmarker;
    virtual void    Archive(Archiver &arc) override;
};

inline void GameScriptMarker::Archive(Archiver &arc)
{
    arc.ArchiveString(&filename);
    arc.ArchiveBoolean(&scriptmarker.tokenready);
    arc.ArchiveInteger(&scriptmarker.offset);
    arc.ArchiveInteger(&scriptmarker.line);
    arc.ArchiveRaw(scriptmarker.token, sizeof(scriptmarker.token));
}

class GameScript : public Script
{
protected:
    Container<script_label_t *> *labelList;
    GameScript                  *sourcescript;
    unsigned                     crc;

public:
    CLASS_PROTOTYPE(GameScript);

    GameScript();
    GameScript(GameScript *scr);
    ~GameScript();
    void     Close(void);
    void     SetSourceScript(GameScript *scr);
    void     LoadFile(const char *filename);

    void     Mark(GameScriptMarker *mark);
    void     Restore(GameScriptMarker *mark);

    void     FreeLabels(void);
    void     FindLabels(void);
    qboolean labelExists(const char *name);
    qboolean Goto(const char *name);
    virtual void Archive(Archiver &arc) override;
};

class ScriptLibrarian : public Class
{
protected:
    Container<GameScript *> scripts;
    str                     dialog_script;
    str                     game_script;

public:
    CLASS_PROTOTYPE(ScriptLibrarian);

    ~ScriptLibrarian();

    void            CloseScripts(void);
    void            SetDialogScript(const str &scriptname);
    void            SetGameScript(const str &scriptname);
    const char     *GetGameScript(void);
    GameScript     *FindScript(const char *name);
    GameScript     *GetScript(const char *name);
    // Goto / labelExists (SDK gamescript.h:103-104) are NOT carried: retail has no symbol
    virtual void    Archive(Archiver &arc) override;
};

inline void ScriptLibrarian::Archive(Archiver &arc)
{
    GameScript *scr;
    int i, num;

    Class::Archive(arc);

    if (arc.Loading()) {
        scripts.FreeObjectList();
    } else {
        num = scripts.NumObjects();
    }
    arc.ArchiveInteger(&num);

    for (i = 1; i <= num; i++) {
        if (arc.Saving()) {
            scr = scripts.ObjectAt(i);
        } else {
            scr = new GameScript;
        }
        arc.ArchiveObject(scr);
        if (arc.Loading()) {
            scripts.AddObject(scr);
        }
    }
    arc.ArchiveString(&dialog_script);
    arc.ArchiveString(&game_script);
}

extern ScriptLibrarian ScriptLib;

