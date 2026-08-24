
#pragma once

class MapRunnerClass : public FilePickerClass
{
public:
    CLASS_PROTOTYPE(MapRunnerClass);

    void FileChosen(const str& currentDirectory, const str& partialName, const str& fullname) override;
};

class ViewSpawnerClass : public FilePickerClass
{
public:
    CLASS_PROTOTYPE(ViewSpawnerClass);

    void FileChosen(const str& currentDirectory, const str& partialName, const str& fullname) override;
};

class UIPickFileClass : public FilePickerClass
{
public:
    Listener *retobj;
    Event    *retevent;

    CLASS_PROTOTYPE(UIPickFileClass);

    UIPickFileClass();
    ~UIPickFileClass();

    void FileChosen(const str& currentDirectory, const str& partialName, const str& fullname) override;
};

void PickFile(const char *name, Listener *obj, Event& event);
