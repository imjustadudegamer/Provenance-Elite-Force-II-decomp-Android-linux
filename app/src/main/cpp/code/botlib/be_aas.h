

#ifndef MAX_STRINGFIELD
#define MAX_STRINGFIELD				80
#endif

#define TFL_INVALID				0x00000001	
#define TFL_WALK				0x00000002	
#define TFL_CROUCH				0x00000004	
#define TFL_BARRIERJUMP			0x00000008	
#define TFL_JUMP				0x00000010	
#define TFL_LADDER				0x00000020	
#define TFL_WALKOFFLEDGE		0x00000080	
#define TFL_SWIM				0x00000100	
#define TFL_WATERJUMP			0x00000200	
#define TFL_TELEPORT			0x00000400	
#define TFL_ELEVATOR			0x00000800	
#define TFL_ROCKETJUMP			0x00001000	
#define TFL_BFGJUMP				0x00002000	
#define TFL_GRAPPLEHOOK			0x00004000	
#define TFL_DOUBLEJUMP			0x00008000	
#define TFL_RAMPJUMP			0x00010000	
#define TFL_STRAFEJUMP			0x00020000	
#define TFL_JUMPPAD				0x00040000	
#define TFL_AIR					0x00080000	
#define TFL_WATER				0x00100000	
#define TFL_SLIME				0x00200000	
#define TFL_LAVA				0x00400000	
#define TFL_DONOTENTER			0x00800000	
#define TFL_FUNCBOB				0x01000000	
#define TFL_FLIGHT				0x02000000	
#define TFL_BRIDGE				0x04000000	
#define TFL_NOTTEAM1			0x08000000	
#define TFL_NOTTEAM2			0x10000000	

#define TFL_DEFAULT	TFL_WALK|TFL_CROUCH|TFL_BARRIERJUMP|\
	TFL_JUMP|TFL_LADDER|\
	TFL_WALKOFFLEDGE|TFL_SWIM|TFL_WATERJUMP|\
	TFL_TELEPORT|TFL_ELEVATOR|\
	TFL_AIR|TFL_WATER|TFL_JUMPPAD|TFL_FUNCBOB


typedef struct aas_trace_s
{
	qboolean	startsolid;	
	float		fraction;	
	vec3_t		endpos;		
	int			ent;		
	int			lastarea;	
	int			area;		
	int			planenum;	
} aas_trace_t;


typedef struct aas_entityinfo_s
{
	int		valid;			
	int		type;			
	int		flags;			
	float	ltime;			
	float	update_time;	
	int		number;			
	vec3_t	origin;			
	vec3_t	angles;			
	vec3_t	old_origin;		
	vec3_t	lastvisorigin;	
	vec3_t	mins;			
	vec3_t	maxs;			
	int		groundent;		
	int		solid;			
	int		modelindex;		
	int		modelindex2;	
	int		frame;			
	int		event;			
	int		eventParm;		
	int		powerups;		
	int		weapon;			
	int		legsAnim;		
	int		torsoAnim;		
} aas_entityinfo_t;

typedef struct aas_areainfo_s
{
	int contents;
	int flags;
	int presencetype;
	int cluster;
	vec3_t mins;
	vec3_t maxs;
	vec3_t center;
} aas_areainfo_t;

#define SE_NONE					0
#define SE_HITGROUND			1		
#define SE_LEAVEGROUND			2		
#define SE_ENTERWATER			4		
#define SE_ENTERSLIME			8		
#define SE_ENTERLAVA			16		
#define SE_HITGROUNDDAMAGE		32		
#define SE_GAP					64		
#define SE_TOUCHJUMPPAD			128		
#define SE_TOUCHTELEPORTER		256		
#define SE_ENTERAREA			512		
#define SE_HITGROUNDAREA		1024	
#define SE_HITBOUNDINGBOX		2048	
#define SE_TOUCHCLUSTERPORTAL	4096	

typedef struct aas_clientmove_s
{
	vec3_t endpos;			
	int endarea;			
	vec3_t velocity;		
	aas_trace_t trace;		
	int presencetype;		
	int stopevent;			
	int endcontents;		
	float time;				
	int frames;				
} aas_clientmove_t;

#define ALTROUTEGOAL_ALL				1
#define ALTROUTEGOAL_CLUSTERPORTALS		2
#define ALTROUTEGOAL_VIEWPORTALS		4

typedef struct aas_altroutegoal_s
{
	vec3_t origin;
	int areanum;
	unsigned short starttraveltime;
	unsigned short goaltraveltime;
	unsigned short extratraveltime;
} aas_altroutegoal_t;

#define RSE_NONE				0
#define RSE_NOROUTE				1	
#define RSE_USETRAVELTYPE		2	
#define RSE_ENTERCONTENTS		4	
#define RSE_ENTERAREA			8	

typedef struct aas_predictroute_s
{
	vec3_t endpos;			
	int endarea;			
	int stopevent;			
	int endcontents;		
	int endtravelflags;		
	int numareas;			
	int time;				
} aas_predictroute_t;
