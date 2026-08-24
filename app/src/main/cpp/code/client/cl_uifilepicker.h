
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
