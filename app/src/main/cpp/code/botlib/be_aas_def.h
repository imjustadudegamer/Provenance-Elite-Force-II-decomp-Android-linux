

#include "../qcommon/q_shared.h"

#define AAS_DEBUG

#define DF_AASENTNUMBER(x)		(x - aasworld.entities)
#define DF_NUMBERAASENT(x)		(&aasworld.entities[x])
#define DF_AASENTCLIENT(x)		(x - aasworld.entities - 1)
#define DF_CLIENTAASENT(x)		(&aasworld.entities[x + 1])

typedef struct aas_link_s
{
	int entnum;
	int areanum;
	struct aas_link_s *next_ent, *prev_ent;
	struct aas_link_s *next_area, *prev_area;
} aas_link_t;

typedef struct bsp_link_s
{
	int entnum;
	int leafnum;
	struct bsp_link_s *next_ent, *prev_ent;
	struct bsp_link_s *next_leaf, *prev_leaf;
} bsp_link_t;

typedef struct bsp_entdata_s
{
	vec3_t origin;
	vec3_t angles;
	vec3_t absmins;
	vec3_t absmaxs;
	int solid;
	int modelnum;
} bsp_entdata_t;

typedef struct aas_entity_s
{
	aas_entityinfo_t i;
	aas_link_t *areas;
	bsp_link_t *leaves;
} aas_entity_t;

typedef struct aas_settings_s
{
	vec3_t phys_gravitydirection;
	float phys_friction;
	float phys_stopspeed;
	float phys_gravity;
	float phys_waterfriction;
	float phys_watergravity;
	float phys_maxvelocity;
	float phys_maxwalkvelocity;
	float phys_maxcrouchvelocity;
	float phys_maxswimvelocity;
	float phys_walkaccelerate;
	float phys_airaccelerate;
	float phys_swimaccelerate;
	float phys_maxstep;
	float phys_maxsteepness;
	float phys_maxwaterjump;
	float phys_maxbarrier;
	float phys_jumpvel;
	float phys_falldelta5;
	float phys_falldelta10;
	float rs_waterjump;
	float rs_teleport;
	float rs_barrierjump;
	float rs_startcrouch;
	float rs_startgrapple;
	float rs_startwalkoffledge;
	float rs_startjump;
	float rs_rocketjump;
	float rs_bfgjump;
	float rs_jumppad;
	float rs_aircontrolledjumppad;
	float rs_funcbob;
	float rs_startelevator;
	float rs_falldamage5;
	float rs_falldamage10;
	float rs_maxfallheight;
	float rs_maxjumpfallheight;
} aas_settings_t;

#define CACHETYPE_PORTAL		0
#define CACHETYPE_AREA			1

typedef struct aas_routingcache_s
{
	byte type;									
	float time;									
	int size;									
	int cluster;								
	int areanum;								
	vec3_t origin;								
	float starttraveltime;						
	int travelflags;							
	struct aas_routingcache_s *prev, *next;
	struct aas_routingcache_s *time_prev, *time_next;
	unsigned char *reachabilities;				
	unsigned short int traveltimes[1];			
} aas_routingcache_t;

typedef struct aas_routingupdate_s
{
	int cluster;
	int areanum;								
	vec3_t start;								
	unsigned short int tmptraveltime;			
	unsigned short int *areatraveltimes;		
	qboolean inlist;							
	struct aas_routingupdate_s *next;
	struct aas_routingupdate_s *prev;
} aas_routingupdate_t;

typedef struct aas_reversedlink_s
{
	int linknum;								
	int areanum;								
	struct aas_reversedlink_s *next;			
} aas_reversedlink_t;

typedef struct aas_reversedreachability_s
{
	int numlinks;
	aas_reversedlink_t *first;
} aas_reversedreachability_t;

typedef struct aas_reachabilityareas_s
{
	int firstarea, numareas;
} aas_reachabilityareas_t;

typedef struct aas_s
{
	int loaded;									
	int initialized;							
	int savefile;								
	int bspchecksum;
	float time;
	int numframes;
	char filename[MAX_QPATH];
	char mapname[MAX_QPATH];
	int numbboxes;
	aas_bbox_t *bboxes;
	int numvertexes;
	aas_vertex_t *vertexes;
	int numplanes;
	aas_plane_t *planes;
	int numedges;
	aas_edge_t *edges;
	int edgeindexsize;
	aas_edgeindex_t *edgeindex;
	int numfaces;
	aas_face_t *faces;
	int faceindexsize;
	aas_faceindex_t *faceindex;
	int numareas;
	aas_area_t *areas;
	int numareasettings;
	aas_areasettings_t *areasettings;
	int reachabilitysize;
	aas_reachability_t *reachability;
	int numnodes;
	aas_node_t *nodes;
	int numportals;
	aas_portal_t *portals;
	int portalindexsize;
	aas_portalindex_t *portalindex;
	int numclusters;
	aas_cluster_t *clusters;
	int numreachabilityareas;
	float reachabilitytime;
	aas_link_t *linkheap;						
	int linkheapsize;							
	aas_link_t *freelinks;						
	aas_link_t **arealinkedentities;			
	int maxentities;
	int maxclients;
	aas_entity_t *entities;
	int travelflagfortype[MAX_TRAVELTYPES];
	int *areacontentstravelflags;
	aas_routingupdate_t *areaupdate;
	aas_routingupdate_t *portalupdate;
	int frameroutingupdates;
	aas_reversedreachability_t *reversedreachability;
	unsigned short ***areatraveltimes;
	aas_routingcache_t ***clusterareacache;
	aas_routingcache_t **portalcache;
	aas_routingcache_t *oldestcache;		
	aas_routingcache_t *newestcache;		
	int *portalmaxtraveltimes;
	int *reachabilityareaindex;
	aas_reachabilityareas_t *reachabilityareas;
} aas_t;

#define AASINTERN

#ifndef BSPCINCLUDE

#include "be_aas_main.h"
#include "be_aas_entity.h"
#include "be_aas_sample.h"
#include "be_aas_cluster.h"
#include "be_aas_reach.h"
#include "be_aas_route.h"
#include "be_aas_routealt.h"
#include "be_aas_debug.h"
#include "be_aas_file.h"
#include "be_aas_optimize.h"
#include "be_aas_bsp.h"
#include "be_aas_move.h"

#endif 
