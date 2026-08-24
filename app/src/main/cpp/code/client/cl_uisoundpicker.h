
#pragma once

class SoundPickerClass : public FilePickerClass
{
public:
    CLASS_PROTOTYPE(SoundPickerClass);

    SoundPickerClass();

    void FileSelected(const str& currentDirectory, const str& partialName, const str& fullname) override;
    void FileChosen(const str& currentDirectory, const str& partialName, const str& fullname) override;
};
