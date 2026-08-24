


#define AASID						(('S'<<24)+('A'<<16)+('A'<<8)+'E')
#define AASVERSION_OLD				4
#define AASVERSION					5

#define PRESENCE_NONE				1
#define PRESENCE_NORMAL				2
#define PRESENCE_CROUCH				4

#define MAX_TRAVELTYPES				32
#define TRAVEL_INVALID				1		
#define TRAVEL_WALK					2		
#define TRAVEL_CROUCH				3		
#define TRAVEL_BARRIERJUMP			4		
#define TRAVEL_JUMP					5		
#define TRAVEL_LADDER				6		
#define TRAVEL_WALKOFFLEDGE			7		
#define TRAVEL_SWIM					8		
#define TRAVEL_WATERJUMP			9		
#define TRAVEL_TELEPORT				10		
#define TRAVEL_ELEVATOR				11		
#define TRAVEL_ROCKETJUMP			12		
#define TRAVEL_BFGJUMP				13		
#define TRAVEL_GRAPPLEHOOK			14		
#define TRAVEL_DOUBLEJUMP			15		
#define TRAVEL_RAMPJUMP				16		
#define TRAVEL_STRAFEJUMP			17		
#define TRAVEL_JUMPPAD				18		
#define TRAVEL_FUNCBOB				19		

#define TRAVELTYPE_MASK				0xFFFFFF
#define TRAVELFLAG_NOTTEAM1			(1 << 24)
#define TRAVELFLAG_NOTTEAM2			(2 << 24)

#define FACE_SOLID					1		
#define FACE_LADDER					2		
#define FACE_GROUND					4		
#define FACE_GAP					8		
#define FACE_LIQUID					16		
#define FACE_LIQUIDSURFACE			32		
#define FACE_BRIDGE					64		

#define AREACONTENTS_WATER				1
#define AREACONTENTS_LAVA				2
#define AREACONTENTS_SLIME				4
#define AREACONTENTS_CLUSTERPORTAL		8
#define AREACONTENTS_TELEPORTAL			16
#define AREACONTENTS_ROUTEPORTAL		32
#define AREACONTENTS_TELEPORTER			64
#define AREACONTENTS_JUMPPAD			128
#define AREACONTENTS_DONOTENTER			256
#define	AREACONTENTS_VIEWPORTAL			512
#define AREACONTENTS_MOVER				1024
#define AREACONTENTS_NOTTEAM1			2048
#define AREACONTENTS_NOTTEAM2			4096
#define AREACONTENTS_MODELNUMSHIFT		24
#define AREACONTENTS_MAXMODELNUM		0xFF
#define AREACONTENTS_MODELNUM			(AREACONTENTS_MAXMODELNUM << AREACONTENTS_MODELNUMSHIFT)

#define AREA_GROUNDED				1		
#define AREA_LADDER					2		
#define AREA_LIQUID					4		
#define AREA_DISABLED				8		
#define AREA_BRIDGE					16		

#define AAS_LUMPS					14
#define AASLUMP_BBOXES				0
#define AASLUMP_VERTEXES			1
#define AASLUMP_PLANES				2
#define AASLUMP_EDGES				3
#define AASLUMP_EDGEINDEX			4
#define AASLUMP_FACES				5
#define AASLUMP_FACEINDEX			6
#define AASLUMP_AREAS				7
#define AASLUMP_AREASETTINGS		8
#define AASLUMP_REACHABILITY		9
#define AASLUMP_NODES				10
#define AASLUMP_PORTALS				11
#define AASLUMP_PORTALINDEX			12
#define AASLUMP_CLUSTERS			13


typedef struct aas_bbox_s
{
	int presencetype;
	int flags;
	vec3_t mins, maxs;
} aas_bbox_t;


typedef struct aas_reachability_s
{
	int areanum;						
	int facenum;						
	int edgenum;						
	vec3_t start;						
	vec3_t end;							
	int traveltype;					
	unsigned short int traveltime;
} aas_reachability_t;

typedef struct aas_areasettings_s
{
	int contents;						
	int areaflags;						
	int presencetype;					
	int cluster;						
	int clusterareanum;				
	int numreachableareas;			
	int firstreachablearea;			
} aas_areasettings_t;

typedef struct aas_portal_s
{
	int areanum;						
	int frontcluster;					
	int backcluster;					
	int clusterareanum[2];			
} aas_portal_t;

typedef int aas_portalindex_t;

typedef struct aas_cluster_s
{
	int numareas;						
	int numreachabilityareas;			
	int numportals;						
	int firstportal;					
} aas_cluster_t;


typedef vec3_t aas_vertex_t;

typedef struct aas_plane_s
{
	vec3_t normal;						
	float dist;							
	int type;
} aas_plane_t;

typedef struct aas_edge_s
{
	int v[2];							
} aas_edge_t;

typedef int aas_edgeindex_t;

typedef struct aas_face_s
{
	int planenum;						
	int faceflags;						
	int numedges;						
	int firstedge;						
	int frontarea;						
	int backarea;						
} aas_face_t;

typedef int aas_faceindex_t;

typedef struct aas_area_s
{
	int areanum;						
	int numfaces;						
	int firstface;						
	vec3_t mins;						
	vec3_t maxs;						
	vec3_t center;						
} aas_area_t;

typedef struct aas_node_s
{
	int planenum;
	int children[2];					
} aas_node_t;


typedef struct
{
	int fileofs;
	int filelen;
} aas_lump_t;

typedef struct aas_header_s
{
	int ident;
	int version;
	int bspchecksum;
	aas_lump_t lumps[AAS_LUMPS];
} aas_header_t;


