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

#include "cl_ui.h"


class FilePickerItem : public UIListCtrlItem
{
    str strings[3];

public:
    FilePickerItem(const str& fileName, const str& date, const str& size);
    int            getListItemValue(int which) const override;
    griditemtype_t getListItemType(int which) const override;
    str            getListItemString(int which) const override;
    void           DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected) override;
};

CLASS_DECLARATION(USignal, FilePickerClass, NULL) {
    {&EV_UIListBase_ItemSelected,      &FilePickerClass::FileSelected},
    {&EV_UIListBase_ItemDoubleClicked, &FilePickerClass::FileChosen  },
    {NULL,                             NULL                          }
};

FilePickerClass::FilePickerClass()
{
    window = new UIFloatingWindow();
    window->Create(NULL, UIRect2D(20, 40, 600, 400), "File Picker", UColor(0.1f, 0.1f, 0.1f, 1.0f), UWhite);
    window->setName("FilePicker");
    listbox = new UIListCtrl();
    listbox->InitFrame(window->getChildSpace(), window->getChildSpace()->getClientFrame(), 0, "tahoma-10");
    listbox->setBackgroundColor(UBlack, WIDGET_STATE_NORMAL, true);
    listbox->setForegroundColor(UGreen);
    listbox->setFont("serpentinemedium-10");
    listbox->AddColumn("File Name", 0, 300, false, false);
    listbox->AddColumn("Date", 1, 170, false, false);
    listbox->AddColumn("Size", 2, 130, true, false);
    listbox->Connect(this, EV_UIListBase_ItemDoubleClicked, EV_UIListBase_ItemDoubleClicked);
    listbox->Connect(this, EV_UIListBase_ItemSelected, EV_UIListBase_ItemSelected);
    listbox->AllowActivate(true);
}

FilePickerClass::~FilePickerClass()
{
    if (listbox) {
        delete listbox;
        listbox = NULL;
    }

    if (window) {
        delete window;
        window = NULL;
    }
}

void FilePickerClass::Setup(const char *root_directory, const char *current_directory, const char *ext, int show_directories)
{
    showDirectories = show_directories;
    Initialize(root_directory, current_directory, ext);
}

void FilePickerClass::Initialize(const char *root_directory, const char *current_directory, const char *ext)
{
    rootDirectory = root_directory;
    if (rootDirectory.length() > 1 && rootDirectory[rootDirectory.length() - 1] != '/') {
        rootDirectory += "/";
    }

    currentDirectory = current_directory;
    if (currentDirectory.length() > 1 && currentDirectory[currentDirectory.length() - 1] != '/') {
        currentDirectory += "/";
    }

    extension = ext;

    SetupFiles();
}

void FilePickerClass::GotoParentDirectory(void)
{
    int i;

    if (currentDirectory == rootDirectory) {
        return;
    }

    for (i = currentDirectory.length() - 2; i > 0; i--) {
        if (currentDirectory[i] == '/') {
            break;
        }
    }

    if (currentDirectory[i] == '/') {
        i++;
    }

    currentDirectory = str(currentDirectory, 0, i);

    SetupFiles();
}

void FilePickerClass::SetupFiles(void)
{
    str    work;
    str    name;
    str    fullpath;
    char **filenames;
    int    numfiles;
    int    i;
    char   date[128];
    char   size[128];

    listbox->DeleteAllItems();

    work = currentDirectory;

    if (work.length() != rootDirectory.length() || strcmp(currentDirectory, rootDirectory) != 0) {
        listbox->AddItem(new FilePickerItem("..", "", ""));
    }

    if (showDirectories) {
        filenames = FS_ListFiles(currentDirectory, "/", qfalse, &numfiles);

        for (i = 0; i < numfiles; i++) {
            const char *filename = filenames[i];

            if (*filename != '.') {
                name = "[";
                name += filename;
                if (name[name.length() - 1] == '/') {
                    name.CapLength(name.length() - 1);
                }
                name += "]";

                fullpath = currentDirectory + filename;
                FS_FileTime(fullpath, date, size);

                listbox->AddItem(new FilePickerItem(name, date, ""));
            }
        }

        FS_FreeFileList(filenames);
    }

    filenames = FS_ListFiles(currentDirectory, extension, qfalse, &numfiles);

    for (i = 0; i < numfiles; i++) {
        const char *filename = filenames[i];

        name     = filename;
        fullpath = currentDirectory + name;

        FS_FileTime(fullpath, date, size);

        listbox->AddItem(new FilePickerItem(name, date, size));
    }

    FS_FreeFileList(filenames);

    window->setTitle(currentDirectory);
}

void FilePickerClass::FileSelected(const str& inCurrentDirectory, const str& partialName, const str& fullname) {}

void FilePickerClass::FileSelected(Event *ev)
{
    str name = listbox->GetItem(listbox->getCurrentItem())->getListItemString(0);

    if (*name != '[' && *name != '.') {
        FileSelected(currentDirectory, name, currentDirectory + name);
    }
}

void FilePickerClass::FileChosen(const str& inCurrentDirectory, const str& partialName, const str& fullname) {}

void FilePickerClass::FileChosen(Event *ev)
{
    str filename = listbox->GetItem(listbox->getCurrentItem())->getListItemString(0);

    if (*filename == '[') {
        currentDirectory += str(filename, 1, filename.length() - 1);
        currentDirectory += "/";
        SetupFiles();
    } else if (*filename == '.') {
        GotoParentDirectory();
    } else {
        FileChosen(currentDirectory, filename, currentDirectory + filename);
    }
}

void FilePickerClass::CloseWindow(void)
{
    PostEvent(EV_Remove, 0);
}

FilePickerItem::FilePickerItem(const str& fileName, const str& date, const str& size)
{
    strings[0] = fileName;
    strings[1] = date;
    strings[2] = size;
}

int FilePickerItem::getListItemValue(int which) const
{
    return atol(strings[which]);
}

griditemtype_t FilePickerItem::getListItemType(int which) const
{
    return griditemtype_t::TYPE_STRING;
}

str FilePickerItem::getListItemString(int which) const
{
    return strings[which];
}

void FilePickerItem::DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected) {}
