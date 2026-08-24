

#pragma once

#define GAME_VERSION "EF2-base-1"

static const int MINS_X = -22;
static const int MINS_Y = -22;
static const int MAXS_X = 22;
static const int MAXS_Y = 22;

static const int MINS_Z = 0;
static const int MAXS_Z = 96;

static const int DEAD_MAXS_Z   = 32; 
static const int CROUCH_MAXS_Z = 49;

static const unsigned int DEFAULT_VIEWHEIGHT = 85; 
static const unsigned int CROUCH_VIEWHEIGHT  = 43; 

// SDK bg_public.h:111.  How long the player must have been running for the WAS_RUNNING
static const int MINIMUM_RUNNING_TIME = 800;











// FRAME_EXPLICIT and is NOT the same namespace -- see the comment at animate.cpp:658. Do
#define ANIM_TOGGLEBIT (1 << 9)
#define ANIM_BLEND     (1 << 10)
#define ANIM_NUMBITS   11

#define ANIM_SERVER_EXITCOMMANDS_PROCESSED (1 << 12)

#define ANIM_MASK (~(ANIM_TOGGLEBIT | ANIM_BLEND | ANIM_SERVER_EXITCOMMANDS_PROCESSED))

#define FRAME_NUMBITS  14
#define FRAME_EXPLICIT (1 << 13)
#define FRAME_MASK     (~FRAME_EXPLICIT)

#define CS_SOUNDTRACK			8
#define CS_FOGINFO  			9
#define CS_SKYINFO  			10
#define CS_GAME_VERSION			11
#define CS_LEVEL_START_TIME		12		
#define CS_TERRAININFO  		13
#define CS_ENTITYFADEINFO		14
#define CS_DEBUGINFO			15
#define CS_MODELS				32
#define CS_SOUNDS				(CS_MODELS+MAX_MODELS)
#define CS_IMAGES				(CS_SOUNDS+MAX_SOUNDS)
#define CS_LIGHTSTYLES			(CS_IMAGES+MAX_IMAGES)
#define CS_PLAYERS				(CS_LIGHTSTYLES+MAX_LIGHTSTYLES)
#define CS_ITEMS				(CS_PLAYERS+MAX_CLIENTS) 
#define CS_AMMO					(CS_ITEMS+MAX_ITEMS)     
#define CS_LOCATIONS			(CS_AMMO+MAX_AMMO)
#define CS_NUM_ARENAS			(CS_LOCATIONS+MAX_LOCATIONS)
#define CS_ARENA_INFO			(CS_NUM_ARENAS+1)
#define CS_NUM_TEAMS			(CS_ARENA_INFO+MAX_ARENA_INFO)
#define CS_TEAM_INFO			(CS_NUM_TEAMS+1)
#define CS_ARCHETYPE			(CS_TEAM_INFO + MAX_TEAM_INFO)
#define CS_FAILEDREASON			(CS_ARCHETYPE + MAX_ARCHETYPES)
#define CS_OBJECTIVE_NAME		(CS_FAILEDREASON+1)
#define CS_BOTINFO				(CS_OBJECTIVE_NAME + MAX_OBJECTIVE_NAMES) 
#define CS_GENERAL_STRINGS		(CS_BOTINFO + 2)
#define CS_MAX					(CS_GENERAL_STRINGS + MAX_GENERAL_STRINGS) 

#if (CS_MAX) > MAX_CONFIGSTRINGS
#error overflow: (CS_MAX) > MAX_CONFIGSTRINGS
#endif


#ifdef __cplusplus
extern "C" {
#endif

    static const float CONSTANTLIGHT_RADIUS_SCALE = 8;

    typedef enum {
        GENDER_MALE,
        GENDER_FEMALE,
        GENDER_NEUTER
    } gender_t;


    enum itemtype_e {
        ITEM_NAME_AMMO_LEFT   = 0,
        ITEM_NAME_AMMO_RIGHT  = 1,
        ITEM_NAME_WEAPON_LEFT = 2,
        ITEM_NAME_WEAPON_RIGHT = 3,
        ITEM_NAME_WEAPON_DUAL = 4,
    };


    static const unsigned int MAX_CLIP_PLANES = 5;
    static const float MIN_WALK_NORMAL = 0.65f; 

    static const float STEPSIZE = 33.0f;

    static const float OVERCLIP = 1.001f;

    typedef enum {
        PM_NORMAL,          
        PM_NOCLIP,          
        PM_DEAD,            
        PM_SIDESTRAFE,      
        PM_FORWARDSTRAFE,   
        PM_SPECTATOR,
        PM_SPECTATOR_FOLLOW,
        PM_SECRET_MOVE_MODE,
        PM_3RD_PERSON,
        PM_NONE,

    } pmtype_t;

    // EF2 has SIX fall bands, not four (SDK bg_public.h:130-136).  The two extra ones are
    typedef enum {
        EV_NONE,

        EV_FALL_VERY_SHORT,
        EV_FALL_SHORT,
        EV_FALL_MEDIUM,
        EV_FALL_FAR,
        EV_FALL_VERY_FAR,
        EV_FALL_FATAL,
        EV_TERMINAL_VELOCITY,

        EV_WATER_TOUCH, 
        EV_WATER_LEAVE, 
        EV_WATER_UNDER, 
        EV_WATER_CLEAR, 

        EV_LAST_PREDICTED 

    } entity_event_t;

#define PMF_DUCKED          (1 << 0)  
#define PMF_TIME_JUMP_START (1 << 1)  
#define PMF_TIME_KNOCKBACK  (1 << 2)  
#define PMF_TIME_WATERJUMP  (1 << 3)  
#define PMF_TIME_TELEPORT   (1 << 4)  
#define PMF_NO_PREDICTION   (1 << 5)  
#define PMF_FROZEN          (1 << 6)  
#define PMF_INTERMISSION    (1 << 7)  
#define PMF_CAMERA_VIEW     (1 << 8)  
#define PMF_NO_MOVE         (1 << 9)  
// nothing anywhere sets it, so retail can never put a 1 there -- corroborated by 0/232
#define PMF_SPECTATING      (1 << 10) // PORT-ONLY (retail's dead HAVETARGET bit)
#define PMF_TIME_STUCKJUMP  (1 << 11) 
#define PMF_LEVELEXIT       (1 << 12) 
#define PMF_NO_GRAVITY      (1 << 13) 
#define PMF_JUMP_HELD       (1 << 14) 
#define PMF_FLIGHT          (1 << 15) 
// into `edict->s.eFlags`, not pm_flags (SDK player.cpp:8210 and :8230).  So retail never
#define MP_FLAG_NO_HEALTH           (1 << 0)
#define MP_FLAG_NO_POWERUPS         (1 << 1)
#define MP_FLAG_WEAPONS_STAY        (1 << 2)
#define MP_FLAG_NO_FALLING          (1 << 3)
#define MP_FLAG_INSTANT_ITEMS       (1 << 4)
#define MP_FLAG_SAME_LEVEL          (1 << 5)
#define MP_FLAG_SKINTEAMS           (1 << 6)
#define MP_FLAG_MODELTEAMS          (1 << 7)
#define MP_FLAG_FRIENDLY_FIRE       (1 << 8)
#define MP_FLAG_SPAWN_FARTHEST      (1 << 9)
#define MP_FLAG_FORCE_RESPAWN       (1 << 10)
#define MP_FLAG_NO_ARMOR            (1 << 11)
#define MP_FLAG_FAST_WEAPONS        (1 << 12)
#define MP_FLAG_NOEXIT              (1 << 13)
#define MP_FLAG_INFINITE_AMMO       (1 << 14)
#define MP_FLAG_FIXED_FOV           (1 << 15)
#define MP_FLAG_NO_DROP_WEAPONS     (1 << 16)
#define MP_FLAG_NO_FOOTSTEPS        (1 << 17)
#define MP_FLAG_DONT_ALLOW_VOTE     (1 << 18)
#define MP_FLAG_FULL_COLLISION      (1 << 19)
#define MP_FLAG_NO_AUTO_JOIN_TEAM   (1 << 20)
#define MP_FLAG_AUTO_BALANCE_TEAMS  (1 << 21)
#define MP_FLAG_FORCE_DEFAULT_MODEL (1 << 22)
#define PMF_RADAR_MODE      (1 << 17) 
#define PMF_DISABLE_INVENTORY (1 << 18)
#define PMF_NO_WEAPONBAR      PMF_DISABLE_INVENTORY
#define PMF_NO_HUD          (1 << 19) 
#define PMF_RESPAWNED       (1 << 20) 
#define PMF_SCANNER         (1 << 22)  
// flag retail never consumes would be a divergence, not a port.
#define PMF_ZOOM            (1 << 23)  
// EF2's crouch-jump latches are NOT pm_flags bits.  Retail carries them as two real
#define PMF_TIME_CROUCH_JUMP (1 << 26) 
#define PMF_NIGHTVISION     (1 << 27) 
#define PMF_DAMAGE_FRONT    (1 << 28)
#define PMF_DAMAGE_BACK     (1 << 29)
#define PMF_DAMAGE_LEFT     (1 << 30)
#define PMF_DAMAGE_RIGHT    (1u << 31)

#define PMF_ALL_TIMES                                                                                            \
    (PMF_TIME_WATERJUMP | PMF_TIME_JUMP_START | PMF_TIME_KNOCKBACK | PMF_TIME_TELEPORT | PMF_TIME_STUCKJUMP       \
     | PMF_TIME_CROUCH_JUMP)

    enum moveposflags_e {
        MPF_POSITION_STANDING  = (1 << 0),
        MPF_POSITION_CROUCHING = (1 << 1),
        MPF_POSITION_PRONE     = (1 << 2),
        MPF_POSITION_OFFGROUND = (1 << 3),
        MPF_MOVEMENT_WALKING   = (1 << 4),
        MPF_MOVEMENT_RUNNING   = (1 << 5),
        MPF_MOVEMENT_FALLING   = (1 << 6),
    };

#define MAXTOUCH 32

    enum moveresult_e {
        MOVERESULT_NONE,    
        MOVERESULT_TURNED,  
        MOVERESULT_BLOCKED, 
        MOVERESULT_HITWALL  
    };

typedef struct {
	playerState_t	*ps;

	usercmd_t	cmd;
	int			tracemask;			
	int			debugLevel;			
	qboolean	   noFootsteps;		

	int			numtouch;
	int			touchents[MAXTOUCH];

   int         moveresult;       

	qboolean		landed;
	float			landedVelocity;

	qboolean	   stepped;		      

	int			pmoveEvent;			
	vec3_t		mins, maxs;			
	int			watertype;
	int			waterlevel;

	void        (*trace)( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask, qboolean cylinder );
	int			(*pointcontents)( const vec3_t point, int passEntityNum );
	qboolean		(*trypush)( int entnum, vec3_t move_origin, vec3_t move_end );
} pmove_t;

    void PM_UpdateViewAngles(playerState_t *ps, const usercmd_t *cmd);
    void Pmove_GroundTrace(pmove_t *pmove);
    void Pmove(pmove_t *pmove);


    static const unsigned int MAX_LETTERBOX_SIZE = 0x7fff;

    enum weaponclass_e {
        WEAPON_CLASS_PISTOL  = (1 << 0),
        WEAPON_CLASS_RIFLE   = (1 << 1),
        WEAPON_CLASS_SMG     = (1 << 2),
        WEAPON_CLASS_MG      = (1 << 3),
        WEAPON_CLASS_GRENADE = (1 << 4),
        WEAPON_CLASS_HEAVY   = (1 << 5),
        WEAPON_CLASS_CANNON  = (1 << 6),
        WEAPON_CLASS_ITEM    = (1 << 7),
        WEAPON_CLASS_ITEM1   = (1 << 8),
        WEAPON_CLASS_ITEM2   = (1 << 9),
        WEAPON_CLASS_ITEM3   = (1 << 10),
        WEAPON_CLASS_ITEM4   = (1 << 11),
        WEAPON_CLASS_ANY_ITEM =
            (WEAPON_CLASS_ITEM | WEAPON_CLASS_ITEM1 | WEAPON_CLASS_ITEM2 | WEAPON_CLASS_ITEM3 | WEAPON_CLASS_ITEM4),
        WEAPON_CLASS_ITEM_SLOT_BITS =
            (WEAPON_CLASS_ITEM1 | WEAPON_CLASS_ITEM2 | WEAPON_CLASS_ITEM3 | WEAPON_CLASS_ITEM4),
        WEAPON_CLASS_PRIMARY   = (!(WEAPON_CLASS_PISTOL | WEAPON_CLASS_GRENADE)),
        WEAPON_CLASS_SECONDARY = (WEAPON_CLASS_PISTOL | WEAPON_CLASS_GRENADE),
        WEAPON_CLASS_THROWABLE = (WEAPON_CLASS_GRENADE | WEAPON_CLASS_ITEM),
        WEAPON_CLASS_MISC =
            (WEAPON_CLASS_CANNON | WEAPON_CLASS_ITEM | WEAPON_CLASS_ITEM1 | WEAPON_CLASS_ITEM2 | WEAPON_CLASS_ITEM3
             | WEAPON_CLASS_ITEM4),
        WEAPON_CLASS_ITEMINDEX = (WEAPON_CLASS_ITEM1 | WEAPON_CLASS_ITEM2 | WEAPON_CLASS_ITEM3 | WEAPON_CLASS_ITEM4)
    };

    // NOTE: may not have more than 16
    typedef enum {
        PERS_SCORE, // !!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!
        PERS_TEAM   
    } persEnum_t;

#define EF_EVERYFRAME 0x00000008
#define EF_FRIEND     0x00000010  
#define EF_ENEMY      0x00000020  
#define EF_EFFECT_CUSTOM  0x00000080  
#define EF_EMITTER_CUSTOM 0x00000100  

#define EF_DISPLAY_INFO  0x00000200 
#define EF_DISPLAY_DESC1 0x00000400 
#define EF_DISPLAY_DESC2 0x00000800 
#define EF_DISPLAY_DESC3 0x00001000 

#define EF_EFFECT_ELECTRIC    0x00004000 
#define EF_BEHAVIOR_FAILURE   0x00008000 
#define EF_ENEMY_TARGETED     0x00010000 

#define EF_TELEPORT_BIT          0x00000004 

// but 0x400000 is not any retail eFlags bit, so the flag could never be seen by anything
#define EF_DONT_PROCESS_COMMANDS 0x00000040  



#define TAG_NUMBITS 10 
#define TAG_MASK    ((1 << 10) - 1)

#define CF_CAMERA_ANGLES_ABSOLUTE     (1 << 0)
#define CF_CAMERA_ANGLES_IGNORE_PITCH (1 << 1)
#define CF_CAMERA_ANGLES_IGNORE_YAW   (1 << 2)
#define CF_CAMERA_ANGLES_ALLOWOFFSET  (1 << 3)
#define CF_CAMERA_ANGLES_TURRETMODE   (1 << 4)
#define CF_CAMERA_CUT_BIT             (1 << 7) 

    typedef enum {
        MOD_NONE,
        MOD_DROWN,
        MOD_SUICIDE,
        MOD_CRUSH,
        MOD_CRUSH_EVERY_FRAME,
        MOD_TELEFRAG,
        MOD_LAVA,
        MOD_SLIME,
        MOD_FALLING,
        MOD_LAST_SELF_INFLICTED,
        MOD_EXPLOSION,
        MOD_EXPLODEWALL,
        MOD_ELECTRIC,
        MOD_ELECTRICWATER,
        MOD_THROWNOBJECT,
        MOD_BEAM,
        MOD_ROCKET,
        MOD_IMPACT,
        MOD_GAS,
        MOD_GAS_BLOCKABLE,
        MOD_ACID,
        MOD_SWORD,
        MOD_PLASMA,
        MOD_PLASMABEAM,
        MOD_PLASMASHOTGUN,
        MOD_STING,
        MOD_STING2,
        MOD_SLING,
        MOD_BULLET,
        MOD_FAST_BULLET,
        MOD_VEHICLE,
        MOD_FIRE,
        MOD_FIRE_BLOCKABLE,
        MOD_VORTEX,
        MOD_LIFEDRAIN,
        MOD_FLASHBANG,
        MOD_POO_EXPLOSION,
        MOD_AXE,
        MOD_CHAINSWORD,
        MOD_ON_FIRE,
        MOD_FIRESWORD,
        MOD_ELECTRICSWORD,
        MOD_CIRCLEOFPROTECTION,
        MOD_RADIATION,
        MOD_LIGHTSWORD,
        MOD_GIB,
        MOD_IMPALE,
        MOD_UPPERCUT,
        MOD_POISON,
        MOD_EAT,
        MOD_REDEMPTION,
        MOD_STASIS,
        MOD_PHASER,
        MOD_VAPORIZE,
        MOD_COMP_RIFLE,
        MOD_VAPORIZE_COMP,
        MOD_IMOD_PRIMARY,
        MOD_IMOD_SECONDARY,
        MOD_SMALL_EXPLOSION,
        MOD_TETRYON,
        MOD_DISRUPTOR,
        MOD_VAPORIZE_DISRUPTOR,
        MOD_VAPORIZE_PHOTON,
        MOD_SNIPER,
        MOD_MELEE,
        MOD_ALIEN_MELEE,
        MOD_KLINGON_MELEE,
        MOD_TURRET,
        MOD_DEATH_QUAD,
        MOD_EMPATHY_SHIELD,
        MOD_ARMOR_PIERCING,

        MOD_TOTAL_NUMBER,

    } meansOfDeath_t;

    // (the port's old extra entries) do not exist in retail: skel models ride
    typedef enum {
        ET_MODELANIM,
        ET_PLAYER,
        ET_ITEM,
        ET_GENERAL,
        ET_MISSILE,
        ET_MOVER,
        ET_BEAM,
        ET_MULTIBEAM,
        ET_SPRITE,
        ET_PORTAL,
        ET_EVENT_ONLY,
        ET_RAIN,
        ET_LEAF,
        ET_SPEAKER,
        ET_PUSH_TRIGGER,
        ET_TELEPORT_TRIGGER,
        ET_DECAL,
        ET_EMITTER,
        ET_ROPE,
        ET_EVENTS
    } entityType_t;

    typedef enum
   {
	STAT_HEALTH,
	STAT_DEAD_YAW,				
	STAT_AMMO_LEFT,				
	STAT_CLIPAMMO_LEFT,			
	STAT_NUM_SHOTS_LEFT,		
	STAT_MAX_NUM_SHOTS_LEFT,	
	STAT_AMMO_RIGHT,			
	STAT_CLIPAMMO_RIGHT,		
	STAT_NUM_SHOTS_RIGHT,		
	STAT_ARMOR_LEVEL,			
	STAT_MAXAMMO_LEFT,			
	STAT_MAXAMMO_RIGHT,			
	STAT_MAXCLIPAMMO_LEFT,		
	STAT_MAXCLIPAMMO_RIGHT,		
	STAT_AMMO_TYPE1,			
	STAT_AMMO_TYPE2,			
	STAT_AMMO_TYPE3,			
	STAT_AMMO_TYPE4,			
	STAT_LAST_PAIN,				
	STAT_ACCUMULATED_PAIN,		
	STAT_BOSSHEALTH,			
	STAT_BOSSNAME_CONFIGINDEX,	
	STAT_CINEMATIC,				
	STAT_ADDFADE,				
	STAT_LETTERBOX,				
	STAT_POWERUPTIME,			

	STAT_WEAPON_GENERIC1,
	STAT_WEAPON_GENERIC2,

	STAT_ITEMICON,
	STAT_ITEMTEXT,

	STAT_VOTETEXT,

	STAT_ENEMIES_KILLED,		
	STAT_TEAMMATES_KILLED,		
	STAT_SHOTS_FIRED,			
	STAT_SHOTS_HIT,				
	STAT_ACCURACY,				
	STAT_MISSION_DURATION,		

	STAT_GENERIC,				

	STAT_NUM_OBJECTIVES,		
	STAT_COMPLETE_OBJECTIVES,	
	STAT_FAILED_OBJECTIVES,		
	STAT_INCOMPLETE_OBJECTIVES, 

	STAT_SPECIALMOVETIMER,		
	STAT_POINTS,				

	STAT_SECRETS_TOTAL,			
	STAT_SECRETS_FOUND,			
	STAT_ITEMS_TOTAL,			
	STAT_ITEMS_FOUND,			

	STAT_RED_TEAM_SCORE,
	STAT_BLUE_TEAM_SCORE,
	STAT_TEAM,					
	STAT_SCORE,					
	STAT_KILLS,				
	STAT_DEATHS,				
	STAT_TIMELEFT_SECONDS,		
	STAT_WON_MATCHES,			
	STAT_LOST_MATCHES,			

	STAT_MP_GENERIC1,
	STAT_MP_GENERIC2,
	STAT_MP_GENERIC3,
	STAT_MP_GENERIC4,
	STAT_MP_GENERIC5,
	STAT_MP_GENERIC6,
	STAT_MP_GENERIC7,
	STAT_MP_GENERIC8,

	STAT_MP_SPECTATING_ENTNUM,

	STAT_MP_MODE_ICON,
	STAT_MP_TEAM_ICON,
	STAT_MP_TEAMHUD_ICON,
	STAT_MP_OTHERTEAM_ICON,
	STAT_MP_SPECIALTY_ICON,
	STAT_MP_HOLDABLEITEM_ICON,
	STAT_MP_RUNE_ICON,
	STAT_MP_POWERUP_ICON,

	STAT_MP_AWARD_ICON,
	STAT_MP_AWARD_COUNT,

	STAT_MP_STATE,

	STAT_LAST_STAT				
		   
   } playerstat_t;

    extern const char *means_of_death_strings[];


    typedef enum {
        TEAM_NONE,
        TEAM_SPECTATOR,
        TEAM_FREEFORALL,
        TEAM_ALLIES,
        TEAM_AXIS,
        TEAM_MAX
    } teamtype_t;

    typedef enum {
        SWT_NORMAL,
        SWT_FUSE,
        SWT_FUSE_WET
    } stopWatchType_t;




#define MASK_ALL (-1)

// and the ones it actually used were MOHAA's bit sets, not retail's.
#define MASK_SOLID                          (CONTENTS_SOLID)
#define MASK_USABLE                         (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_USABLE)
#define MASK_PLAYERSOLID                    (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_SETCLIP)
#define MASK_DEADSOLID                      (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_SETCLIP)
#define MASK_MONSTERSOLID                   (CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_BODY | CONTENTS_SETCLIP)
#define MASK_WATER                          (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
#define MASK_OPAQUE                         (CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA | CONTENTS_CAMERACLIP)
#define MASK_SHOT                           (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE | CONTENTS_WEAPONCLIP | CONTENTS_SHOOTABLE_ONLY | CONTENTS_SETCLIP)
#define MASK_PROJECTILE                     (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_WEAPONCLIP | CONTENTS_SHOOTABLE_ONLY)
#define MASK_PROJECTILE_NOTSHOOTABLE        (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_WEAPONCLIP)
#define MASK_MELEE                          (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE | CONTENTS_WEAPONCLIP | CONTENTS_SHOOTABLE_ONLY)
#define MASK_PATHSOLID                      (CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_SETCLIP)
#define MASK_CAMERASOLID                    (CONTENTS_SOLID | CONTENTS_CAMERACLIP | CONTENTS_BODY | CONTENTS_SETCLIP | MASK_WATER)
#define MASK_SETCLIP                        (CONTENTS_BODY)
#define MASK_CURRENT \
    (CONTENTS_CURRENT_0 | CONTENTS_CURRENT_90 | CONTENTS_CURRENT_180 | CONTENTS_CURRENT_270 | CONTENTS_CURRENT_UP | CONTENTS_CURRENT_DOWN)







#ifdef __cplusplus
}
#endif

typedef enum {
    OBJ_FLAG_NONE,
    OBJ_FLAG_HIDDEN    = 1,
    OBJ_FLAG_CURRENT   = 2,
    OBJ_FLAG_COMPLETED = 4,
    OBJ_FLAG_FAILED    = 8 
} objective_flags_t;

typedef enum {
    OBJ_STATUS_NONE = 0,
    OBJ_STATUS_HIDDEN,
    OBJ_STATUS_CURRENT,
    OBJ_STATUS_COMPLETED
} objective_status_t;

typedef enum {
    SOUNDTYPE_NONE,
    SOUNDTYPE_GENERAL,
    SOUNDTYPE_EXPLOSION,
    SOUNDTYPE_WEAPONFIRE,
    SOUNDTYPE_ALERT,
    SOUNDTYPE_FOOTSTEPS_WALK,
    SOUNDTYPE_FOOTSTEPS_RUN,
    SOUNDTYPE_FALL,

    SOUNDTYPE_DIALOG_CONTEXT_SPOTTED_ENEMY,
    SOUNDTYPE_DIALOG_CONTEXT_INJURED,
    SOUNDTYPE_DIALOG_CONTEXT_IN_COMBAT,
    SOUNDTYPE_DIALOG_CONTEXT_WEAPON_USELESS,
    SOUNDTYPE_DIALOG_CONTEXT_INVESTIGATING,

    SOUNDTYPE_TOTAL_NUMBER
} soundType_t;


#define	MAX_CLIP_PLANES	   5
#define	MIN_WALK_NORMAL	   0.65f		   

#define	STEPSIZE		         33.0f

#define  MINS_X               -22
#define  MINS_Y               -22
#define  MAXS_X               22
#define  MAXS_Y               22

#define	MINS_Z				   0
#define  MAXS_Z               96

#define  DEAD_MINS_Z          32
#define  CROUCH_MAXS_Z        49
#define  CROUCH_VIEWHEIGHT	   ( CROUCH_MAXS_Z - 6 )
#define	DEAD_VIEWHEIGHT		( MAXS_Z - 6 )

#define  WATER_TURBO_SPEED    1.35f
#define  WATER_TURBO_TIME     1200
#define  MINIMUM_RUNNING_TIME 800
#define  MINIMUM_WATER_FOR_TURBO  90

#define	OVERCLIP		         1.001f
