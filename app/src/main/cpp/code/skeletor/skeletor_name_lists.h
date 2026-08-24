

#pragma once

#define MAX_SKELETOR_CHANNELS 2560 
#define MAX_CHANNEL_NAME      32

typedef struct ChannelName_s {
    char  name[MAX_CHANNEL_NAME];
    short channelNum;
} ChannelName_t;

#ifdef __cplusplus

class ChannelNameTable
{
    short int     m_iNumChannels;
    ChannelName_t m_Channels[MAX_SKELETOR_CHANNELS];
    short int     m_lookup[MAX_SKELETOR_CHANNELS];

public:
    ChannelNameTable();

    int         RegisterChannel(const char *name);
    int         FindNameLookup(const char *name);
    void        PrintContents();
    const char *FindName(int index);
    int         NumChannels() const;

private:
    const char *FindNameFromLookup(int index);
    bool        FindIndexFromName(const char *name, int *indexPtr);
    void        SortIntoTable(int index);
    void        CopyChannel(ChannelName_t *dest, const ChannelName_t *source);
    void        SetChannelName(ChannelName_t *channel, const char *newName);
};

typedef enum channelType_e {
    CHANNEL_ROTATION,
    CHANNEL_POSITION,
    CHANNEL_NONE,
    CHANNEL_VALUE
} channelType_t;

channelType_t GetBoneChannelType(const char *name);

#else

typedef struct {
    short int     m_iNumChannels;
    ChannelName_t m_Channels[MAX_SKELETOR_CHANNELS];
    short int     m_lookup[MAX_SKELETOR_CHANNELS];
} ChannelNameTable;

#endif
