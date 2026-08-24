
#pragma once

#include "qal.h"

#undef OPENAL

typedef int          S32;
typedef unsigned int U32;

#define MAX_STREAM_BUFFERS              16
#define MAX_BUFFER_SAMPLES              16384

typedef enum {
    FADE_NONE,
    FADE_IN,
    FADE_OUT
} fade_t;

typedef struct {
    vec3_t vOrigin;
    vec3_t vRelativeOrigin;
    vec3_t vVelocity;

    sfx_t   *pSfx;
    qboolean bPlaying;

    int   iChannel;
    float fVolume;
    float fPitch;

    int      iStartTime;
    qboolean bInUse;
    qboolean bCombine;

    float fBaseVolume;
    float fMinDist;
    float fMaxDist;
    int   iFlags;

    int iEntNum;
    bool bUseListener;
} openal_loop_sound_t;

struct openal_channel {
    sfx_t *pSfx;
    int    iEntNum;
    int    iEntChannel;
    vec3_t vOrigin;

    float fVolume;
    int   iBaseRate;
    float fNewPitchMult;
    float fMinDist;
    float fMaxDist;

    int iStartTime;
    int iTime;
    int iEndTime;
    int iFlags;
    int iPausedOffset;

    qboolean bPendingStart;

    float fObstruction;

    float fDistGain;

    int song_number;

    fade_t fading;
    float  fade_time;
    int    fade_start_time;
    qboolean song_in_use;
    qboolean song_looping;

    ALuint   source;
    ALuint   buffer;
    ALubyte *bufferdata;

public:
    void         play();
    virtual void stop();
    void         pause();

    void  set_no_3d();
    void  set_3d();
    void  set_no_virtualization();
    void  set_virtualization();

    void set_gain(float gain);
    void set_gain_3d(float gain);
    void set_velocity(float v0, float v1, float v2);
    void set_position(float v0, float v1, float v2);

    bool is_free();
    bool is_paused();
    bool is_playing();

    void         force_free();
    virtual bool set_sfx(sfx_t *pSfx);

    void start_sample();
    void stop_sample();
    void resume_sample();
    void end_sample();

    void set_sample_pan(S32 pan);
    void set_sample_playback_rate(S32 pan);
    S32  sample_playback_rate();

    S32          sample_volume();
    virtual U32  sample_offset();
    U32          sample_ms_offset();
    U32          sample_loop_count();
    virtual void set_sample_offset(U32 offset);
    void         set_sample_ms_offset(U32 offset);
    virtual void set_sample_loop_count(S32 count);
    void         set_sample_loop_block(S32 start_offset, S32 end_offset);

    U32 sample_status();

public:
    virtual void update();

    virtual U32 buffer_frequency() const;
};

struct openal_channel_two_d_stream : public openal_channel {
private:
    char         fileName[64];
    void        *streamHandle;
    unsigned int buffers[MAX_STREAM_BUFFERS];
    unsigned int currentBuf;
    unsigned int sampleLoopCount;
    unsigned int sampleLooped;
    unsigned int streamNextOffset;
    unsigned int sampleLengthMs;
    bool         streaming;

public:
    openal_channel_two_d_stream();
    ~openal_channel_two_d_stream();

    void stop() override;
    bool set_sfx(sfx_t *pSfx) override;
    void set_sample_loop_count(S32 count) override;
    void update() override;
    U32  sample_offset() override;
    void set_sample_offset(U32 offset) override;

    bool queue_stream(const char *fileName);

    U32 sample_ms_length() const;

protected:
    U32 buffer_frequency() const override;

private:
    void clear_stream();

    unsigned int getQueueLength() const;
    unsigned int getCurrentStreamPosition() const;
    unsigned int getBitsPerSample() const;
};

struct openal_internal_t {
    openal_channel              chan_3D[MAX_SOUNDSYSTEM_CHANNELS_3D];
    openal_channel              chan_2D[MAX_SOUNDSYSTEM_CHANNELS_2D];
    openal_channel_two_d_stream chan_2D_stream[MAX_SOUNDSYSTEM_CHANNELS_2D_STREAM];
    openal_channel_two_d_stream chan_song[MAX_SOUNDSYSTEM_SONGS];
    openal_channel_two_d_stream chan_mp3;
    openal_channel_two_d_stream chan_trig_music;
    openal_channel_two_d_stream chan_movie;

    openal_channel *channel[MAX_SOUNDSYSTEM_CHANNELS];

    openal_loop_sound_t loop_sounds[MAX_SOUNDSYSTEM_LOOP_SOUNDS];

    sfx_t movieSFX;

    char tm_filename[MAX_RES_NAME];
    int  tm_loopcount;
};

#ifdef __cplusplus
extern "C" {
#endif

    qboolean S_OPENAL_Init();
    void     S_OPENAL_Shutdown();
    void     S_OPENAL_StartSound(
            const vec3_t vOrigin,
            int          iEntNum,
            int          iEntChannel,
            sfxHandle_t  sfxHandle,
            float        fVolume,
            float        fMinDist,
            float        fPitch,
            float        fMaxDist,
            qboolean     bStreamed
        );
    void S_OPENAL_AddLoopingSound(
        const vec3_t vOrigin,
        const vec3_t vVelocity,
        sfxHandle_t  sfxHandle,
        float        fVolume,
        float        fMinDist,
        int          iEntNum,
        int          iUnused7,
        int          iUnused8
    );
    void S_OPENAL_ClearLoopingSounds();
    void S_OPENAL_StopSound(int iEntNum, int iEntChannel);
    void S_OPENAL_StopAllSounds(qboolean bStopMusic);
    void S_OPENAL_Respatialize(int iEntNum, const vec3_t vHeadPos, const vec3_t vAxis[3]);
    void S_OPENAL_SetReverb(int iType, float fLevel);
    void S_OPENAL_Update();

    const char  *S_OPENAL_GetMusicFilename();
    int          S_OPENAL_GetMusicLoopCount();
    unsigned int S_OPENAL_GetMusicOffset();

#ifdef __cplusplus
}
#endif
