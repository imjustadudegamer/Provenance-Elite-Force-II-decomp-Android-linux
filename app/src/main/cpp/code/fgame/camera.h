

#pragma once

#include "g_local.h"
#include "entity.h"
#include "bspline.h"
#include "../corepp/container.h"
#include "level.h"
#include "CameraPath.h"  

#define CAMERA_SWITCHTIME 0.5f

#define ORBIT             (1 << 0)
#define START_ON          (1 << 1)
#define AUTOMATIC         (1 << 2)
#define NO_TRACE          (1 << 3)
#define NO_WATCH          (1 << 4)
#define LEVEL_EXIT        (1 << 5)

extern Event EV_Camera_CameraThink;
extern Event EV_Camera_StartMoving;
extern Event EV_Camera_Pause;
extern Event EV_Camera_Continue;
extern Event EV_Camera_StopMoving;
extern Event EV_Camera_SetSpeed;
extern Event EV_Camera_SetFollowDistance;
extern Event EV_Camera_SetOrbitHeight;
extern Event EV_Camera_SetYaw;
extern Event EV_Camera_AbsoluteYaw;
extern Event EV_Camera_RelativeYaw;
extern Event EV_Camera_SetFOV;
extern Event EV_Camera_SetInterpolateFOV;
extern Event EV_Camera_SetThread;
extern Event EV_Camera_SetThread2;
extern Event EV_Camera_Orbit;
extern Event EV_Camera_Follow;
extern Event EV_Camera_Watch;
extern Event EV_Camera_WatchPath;
extern Event EV_Camera_LookAt;
extern Event EV_Camera_TurnTo;
extern Event EV_Camera_MoveToEntity;
extern Event EV_Camera_MoveToPos;
extern Event EV_Camera_NoWatch;
extern Event EV_Camera_FadeTime;
extern Event EV_Camera_Cut;
extern Event EV_Camera_LoadKFC;
extern Event EV_Camera_PlayKFCPath;
extern Event EV_Camera_DestroyKFCPath;

class Camera;

class CameraPath; 

class CameraMoveState : public Class
{
public:
    Vector pos;
    Vector movedir; 
    Vector angles;  

    BSpline       cameraPath;
    SplinePathPtr splinePath;
    SplinePathPtr currentNode;
    SplinePathPtr loopNode;

    float cameraTime;
    int   lastTime;
    int   newTime;

    qboolean  followingpath;
    EntityPtr followEnt;
    EntityPtr orbitEnt;

    void operator=(CameraMoveState& newstate);
    void Evaluate(Camera *camera);
    void Initialize(Camera *camera);
    void DoNodeEvents(Camera *camera);
    void Archive(Archiver& arc) override;
};

inline void CameraMoveState::operator=(CameraMoveState& newstate)
{
    movedir = newstate.movedir;
    pos     = newstate.pos;
    angles  = newstate.angles;

    cameraPath  = newstate.cameraPath;
    splinePath  = newstate.splinePath;
    currentNode = newstate.currentNode;
    loopNode    = newstate.loopNode;

    cameraTime = newstate.cameraTime;
    lastTime   = newstate.lastTime;
    newTime    = newstate.newTime;

    followEnt = newstate.followEnt;
    orbitEnt  = newstate.orbitEnt;

    followingpath = newstate.followingpath;
}

//     long, which CameraMoveState::Evaluate (SDK camera.cpp:271-273) can never produce.
inline void CameraMoveState::Archive(Archiver& arc)
{
    arc.ArchiveVector(&pos);
    arc.ArchiveVector(&movedir);
    arc.ArchiveVector(&angles);

    cameraPath.Archive(arc);

    arc.ArchiveSafePointer(&splinePath);
    arc.ArchiveSafePointer(&currentNode);
    arc.ArchiveSafePointer(&loopNode);

    arc.ArchiveFloat(&cameraTime);
    arc.ArchiveInteger(&lastTime);
    arc.ArchiveInteger(&newTime);

    arc.ArchiveBoolean(&followingpath);

    arc.ArchiveSafePointer(&followEnt);
    arc.ArchiveSafePointer(&orbitEnt);
}

class CameraWatchState : public Class
{
public:
    Vector watchAngles;

    EntityPtr watchEnt;
    qboolean  watchNodes;
    qboolean  watchPath;

    void Evaluate(const Camera *camera, const CameraMoveState *move);
    void Initialize(Camera *camera);

    void Archive(Archiver& arc) override;
};

inline void CameraWatchState::Archive(Archiver& arc)
{
    arc.ArchiveVector(&watchAngles);
    arc.ArchiveSafePointer(&watchEnt);
    arc.ArchiveBoolean(&watchNodes);
    arc.ArchiveBoolean(&watchPath);
}

class CameraState : public Class
{
public:
    CameraMoveState  move;
    CameraWatchState watch;
    float            fov;

    void Archive(Archiver& arc) override;






public:
    void Initialize( Camera * camera ); 
    void Evaluate( Camera * camera ); 
};

inline void CameraState::Archive(Archiver& arc)
{
    move.Archive(arc);
    watch.Archive(arc);
    arc.ArchiveFloat(&fov);
}

class Camera : public Entity
{
private:
    friend class CameraState;
    friend class CameraWatchState;
    friend class CameraMoveState;
    float    follow_yaw;
    qboolean follow_yaw_fixed;
    float    follow_dist;
    int      follow_mask;

    float camera_speed;
    float camera_fov;
    float orbit_height;
    qboolean orbit_dotrace;
    float auto_fov;

    float          automatic_startTime;
    float          automatic_stopTime;
    float          automatic_radius;
    float          automatic_maxFOV;
    qboolean       automatic_active;
    Container<str> automatic_states;

    CameraPath   *newCameraPath;          // SDK camera.h:256 -- NULL if not currently in use (fall back to old system)
    float               newCameraPathSeconds;     

protected:
    CameraState currentstate;
    CameraState newstate;

    float watchTime;  
    float followTime; 
    float fovTime;    

    float fadeTime;       
    float fovFadeTime;    
    float followFadeTime; 
    float watchFadeTime;  


    str nextCamera;
    str thread;

    qboolean showcamera;

    void         SetupCamera(Event *ev);
    void         CameraThink(Event *ev);
    void         EvaluateCameraKeyFramePath(void);  
    void         CreateOrbit(const Vector& pos, float radius, const Vector& forward, const Vector& left);  
    void         CreatePath(SplinePath *path, splinetype_t type);
    void         UpdateStates(void);
    Vector       CalculatePosition(void);
    Vector       CalculateOrientation(void);
    float        CalculateFov(void);
    virtual void bind(Entity *master, qboolean use_my_angles = false) override;
    void         unbind(void) override;

public:
    CLASS_PROTOTYPE(Camera);

    Camera();
    void Stop(void);
    void FollowPath(SplinePath *path, qboolean loop, Entity *watch);
    void Orbit(Entity *ent, float dist, Entity *watch, float yaw_offset = 0, qboolean dotrace = qtrue);
    void FollowEntity(Entity *ent, float dist, int mask, Entity *watch = NULL);
    void Watch(const str& watch, float time);  
    void Watch(Entity *ent, float time);       
    void SetFOV(Event *ev);                    
    void SetFOV(float fov, float time);

    void     StartMoving(Event *ev);
    void     StopMoving(Event *ev);
    void     Pause(Event *ev);
    void     Continue(Event *ev);
    void     SetAnglesEvent(Event *ev);
    void     SetSpeed(Event *ev);
    void     SetFollowDistance(Event *ev);
    void     SetOrbitHeight(float height);  
    void     SetOrbitHeight(Event *ev);     
    void     SetFollowYaw(Event *ev);
    void     AbsoluteYaw(Event *ev);
    void     RelativeYaw(Event *ev);
    void     SetInterpolateFOV(Event *ev);
    void     OrbitEvent(Event *ev);
    void     FollowEvent(Event *ev);
    void     WatchEvent(Event *ev);
    void     WatchPathEvent(Event *ev);
    void     WatchNodesEvent(Event *ev);
    void     NoWatchEvent(Event *ev);
    void     LookAt(Event *ev);
    void     MoveToEntity(Event *ev);
    void     MoveToPos(Event *ev);
    void     Cut(Event *ev);

    void     LoadKFC(Event *ev);
    void     SetPlaybackOffsets(const float yawOffsetDegrees, const Vector& originOffset);
    float    GetPathLengthInSeconds(void);

    void     FadeTime(Event *ev);
    void     TurnTo(Event *ev);
    void     SetNextCamera(Event *ev);
    void     SetThread(Event *ev);   
    float    CalculateScore(Entity *player, const str& state);  
    float    AutomaticStart(Entity *player);
    float    AutomaticStop(Entity *player);
    qboolean IsAutomatic(void);
    qboolean IsLevelExit(void);
    void     SetAutoStateEvent(Event *ev);
    void     SetAutoRadiusEvent(Event *ev);
    void     SetAutoStartTimeEvent(Event *ev);
    void     SetAutoStopTimeEvent(Event *ev);
    void     SetMaximumAutoFOVEvent(Event *ev);
    void     SetAutoActiveEvent(Event *ev);

    str&  NextCamera(void);
    str&  Thread(void);          
    float Fov(void);
    float GetCameraTime(void);   

    void Reset(const Vector& org, const Vector& ang);  

    void Archive(Archiver& arc) override;
};

// records; the byte layout must match retail exactly.
inline void Camera::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveFloat(&follow_yaw);
    arc.ArchiveBoolean(&follow_yaw_fixed);
    arc.ArchiveFloat(&follow_dist);
    arc.ArchiveInteger(&follow_mask);

    arc.ArchiveFloat(&camera_speed);
    arc.ArchiveFloat(&camera_fov);
    arc.ArchiveFloat(&orbit_height);
    arc.ArchiveBoolean(&orbit_dotrace);
    arc.ArchiveFloat(&auto_fov);

    arc.ArchiveFloat(&automatic_startTime);
    arc.ArchiveFloat(&automatic_stopTime);
    arc.ArchiveFloat(&automatic_radius);
    arc.ArchiveFloat(&automatic_maxFOV);
    arc.ArchiveBoolean(&automatic_active);

    automatic_states.Archive(arc);

    if (arc.Saving()) {
        bool cameraValid;

        if (newCameraPath) {
            cameraValid = true;
        } else {
            cameraValid = false;
        }

        arc.ArchiveBool(&cameraValid);

        if (cameraValid) {
            arc.ArchiveObject(newCameraPath);
        }
    } else {
        bool cameraValid;

        arc.ArchiveBool(&cameraValid);

        if (cameraValid) {
            newCameraPath = new CameraPath();

            arc.ArchiveObject(newCameraPath);
        } else {
            newCameraPath = NULL;
        }
    }

    arc.ArchiveFloat(&newCameraPathSeconds);

    currentstate.Archive(arc);
    newstate.Archive(arc);

    arc.ArchiveFloat(&watchTime);
    arc.ArchiveFloat(&followTime);
    arc.ArchiveFloat(&fovTime);
    arc.ArchiveFloat(&fadeTime);
    arc.ArchiveFloat(&fovFadeTime);
    arc.ArchiveFloat(&followFadeTime);
    arc.ArchiveFloat(&watchFadeTime);

    arc.ArchiveString(&nextCamera);
    arc.ArchiveString(&thread);

    arc.ArchiveBoolean(&showcamera);

    if (arc.Loading()) {
        if (spawnflags & AUTOMATIC) {
            level.AddAutomaticCamera(this);
        }
    }
}




void    SetCamera(Entity *ent, float switchTime);
Entity *GetWatchEntity(const str& watch);  

typedef SafePtr<Camera> CameraPtr;

class CameraManager : public Listener
{
protected:
    Container<str> pathList;
    BSpline        cameraPath;
    SplinePathPtr  path;
    SplinePathPtr  current;
    float          speed;
    int            watch;
    str            pathName;
    CameraPtr      cam;
    qboolean       cameraPath_dirty;
    float          playbackStartTime;
    qboolean       isPreviewPlaybackRunning;

    void NewPath(Event *ev);
    void SetPath(Event *ev);
    void SetTargetName(Event *ev);
    void SetTarget(Event *ev);
    void SetThread(Event *ev);  
    void AddPoint(Event *ev);
    void ReplacePoint(Event *ev);
    void DeletePoint(Event *ev);
    void MovePlayer(Event *ev);
    void NextPoint(Event *ev);
    void PreviousPoint(Event *ev);
    void ShowingPath(Event *ev);
    void ShowPath(Event *ev);
    void HidePath(Event *ev);
    void StopPlayback(Event *ev);
    void PlayPath(Event *ev);
    void LoopPath(Event *ev);
    void Watch(Event *ev);    
    void NoWatch(Event *ev);
    void Fov(Event *ev);      
    void FadeTime(Event *ev);  
    void Speed(Event *ev);
    void Save(Event *ev);
    void Load(Event *ev);
    void SaveMap(Event *ev);
    void PlayKFCPath(Event *ev);     
    void DestroyKFCPath(Event *ev);  
    void UpdateEvent(Event *ev);
    void NextPath(Event *ev);
    void PreviousPath(Event *ev);
    void RenamePath(Event *ev);

    void UpdateUI(void);
    void SetPathName(str name);
    void SavePath(str pathName);
    void SetPath(str newPathName);  
    void ShowPath(void);            

public:
    CLASS_PROTOTYPE(CameraManager);

    CameraManager();
    void Archive(Archiver& arc) override;
};

inline void CameraManager::Archive(Archiver& arc)
{

    pathList.Archive(arc);

    arc.ArchiveSafePointer(&path);
    arc.ArchiveSafePointer(&current);
    arc.ArchiveFloat(&speed);
    arc.ArchiveInteger(&watch);
    arc.ArchiveString(&pathName);
    arc.ArchiveSafePointer(&cam);

    cameraPath_dirty = qtrue;
}

extern CameraManager CameraMan;
