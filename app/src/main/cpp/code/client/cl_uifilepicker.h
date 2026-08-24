/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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

class UIListCtrl;

class FilePickerClass : public USignal
{
    UIFloatingWindow *window;
    UIListCtrl       *listbox;
    int               showDirectories;
    str               currentDirectory;
    str               rootDirectory;
    str               extension;

public:
    CLASS_PROTOTYPE(FilePickerClass);

private:
    void SetupFiles(void);
    void GotoParentDirectory(void);
    void Initialize(const char *root_directory, const char *current_directory, const char *ext);

protected:
    void         CloseWindow(void);
    void         FileSelected(Event *ev);
    virtual void FileSelected(const str& currentDirectory, const str& partialName, const str& fullname);
    void         FileChosen(Event *ev);
    virtual void FileChosen(const str& currentDirectory, const str& partialName, const str& fullname);

public:
    FilePickerClass();
    ~FilePickerClass();

    void Setup(const char *root_directory, const char *current_directory, const char *ext, int show_directories);
};
