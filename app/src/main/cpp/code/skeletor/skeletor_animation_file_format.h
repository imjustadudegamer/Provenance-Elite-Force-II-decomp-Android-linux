

#pragma once

typedef float skelAnimChannel_t[4];

typedef struct {
    SkelVec3 bounds[2];
    float    radius;
    SkelVec3 delta;
    float    angleDelta;
    int      iOfsChannels;
} skelAnimFileFrame_t;

typedef struct {
    int                 ident;
    int                 version;
    int                 flags;
    int                 nBytesUsed;
    float               frameTime;
    SkelVec3            totalDelta;
    float               totalAngleDelta;
    int                 numChannels;
    int                 ofsChannelNames;
    int                 numFrames;
    skelAnimFileFrame_t frame[1];
} skelAnimDataFileHeader_t;

typedef struct {
    SkelVec3 bounds[2];
    float    radius;
    SkelVec3 delta;
    float    angleDelta;
    vec4_t  *pChannels; 
} skelAnimGameFrame_t;

typedef struct {
    short int nFrameNum;
    short int nPrevFrameIndex;
    float     pChannelData[1];
} skanGameFrame;

typedef struct {
    short int      nFramesInChannel;
    skanGameFrame *ary_frames;
} skanChannelHdr;

typedef struct {
    short  nFrameNum;
    vec3_t pos;
    vec3_t rot;
} skanAnimFrame;

typedef struct skelAnimDataGameHeader_s skelAnimDataGameHeader_t;

#ifdef __cplusplus

struct tikiTanTagRecords_s;
typedef struct tikiTanTagRecords_s tikiTanTagRecords_t;

struct tikiTanTagRecords_s {
    int numTags;
    int numFrames;

    static size_t BlockSize(int numTags, int numFrames)
    {
        return sizeof(tikiTanTagRecords_t) + (size_t)numTags * 64
             + (size_t)numTags * (size_t)numFrames * 12 * sizeof(float);
    }

    const char *Name(int tag) const
    {
        return (const char *)(this + 1) + (size_t)tag * 64;
    }

    const float *Pose(int tag, int frame) const
    {
        return (const float *)((const char *)(this + 1) + (size_t)numTags * 64)
             + ((size_t)tag * (size_t)numFrames + (size_t)frame) * 12;
    }

    float *Pose(int tag, int frame)
    {
        return (float *)((const char *)(this + 1) + (size_t)numTags * 64)
             + ((size_t)tag * (size_t)numFrames + (size_t)frame) * 12;
    }
};

typedef struct skelAnimDataGameHeader_s {
    int                  flags;
    size_t               nBytesUsed;
    bool                 bHasDelta;
    bool                 bHasMorph;
    bool                 bHasUpper;
    int                  numFrames;
    SkelVec3             totalDelta;
    float                totalAngleDelta;
    float                frameTime;
    skelChannelList_c    channelList;
    SkelVec3             bounds[2];
    skelAnimGameFrame_t *m_frame;
    short int            nTotalChannels;
    skelAnimHeader_t    *skan;
    struct tikiFrame_s *tanFrames;
    int                  numTanFrames;
    tikiTanTagRecords_t *tanTags;
    int                  numTanTags;
    skanChannelHdr       ary_channels[1];

    skelAnimDataGameHeader_s(const skelAnimDataGameHeader_t&);
    skelAnimDataGameHeader_s();

    static skelAnimDataGameHeader_t *AllocRLEChannelData(size_t numChannels);
    int                              GetFrameNums(
                                     float  timeSeconds,
                                     float  timeTolerance,
                                     int   *beforeFrame,
                                     int   *afterFrame,
                                     float *beforeWeight,
                                     float *afterWeight
                                 );
    SkelVec3                         GetDeltaOverTime(float time1, float time2);
    float                            GetAngularDeltaOverTime(float time1, float time2);
    static skelAnimDataGameHeader_t *AllocAnimData(size_t numFrames, size_t numChannels);
    static void                      DeallocAnimData(skelAnimDataGameHeader_t *data);
} skelAnimDataGameHeader_t;

#endif
