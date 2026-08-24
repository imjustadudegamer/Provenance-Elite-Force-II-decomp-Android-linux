

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#ifndef BSPC
#include "l_libvar.h"
#endif
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_interface.h"
#include "be_aas_funcs.h"
#include "be_aas_def.h"



#define BBOX_NORMAL_EPSILON		0.001

#define ON_EPSILON					0 

#define TRACEPLANE_EPSILON			0.125

typedef struct aas_tracestack_s
{
	vec3_t start;		
	vec3_t end;			
	int planenum;		
	int nodenum;		
} aas_tracestack_t;

int numaaslinks;

void AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs)
{
	int index;
	vec3_t boxmins[3] = {{0, 0, 0}, {-15, -15, -24}, {-15, -15, -24}};
	vec3_t boxmaxs[3] = {{0, 0, 0}, { 15,  15,  32}, { 15,  15,   8}};

	if (presencetype == PRESENCE_NORMAL) index = 1;
	else if (presencetype == PRESENCE_CROUCH) index = 2;
	else
	{
		botimport.Print(PRT_FATAL, "AAS_PresenceTypeBoundingBox: unknown presence type\n");
		index = 2;
	} 
	VectorCopy(boxmins[index], mins);
	VectorCopy(boxmaxs[index], maxs);
} 
void AAS_InitAASLinkHeap(void)
{
	int i, max_aaslinks;

	max_aaslinks = aasworld.linkheapsize;
	if (!aasworld.linkheap)
	{
#ifdef BSPC
		max_aaslinks = 6144;
#else
		max_aaslinks = (int) LibVarValue("max_aaslinks", "6144");
#endif
		if (max_aaslinks < 0) max_aaslinks = 0;
		aasworld.linkheapsize = max_aaslinks;
		aasworld.linkheap = (aas_link_t *) GetHunkMemory(max_aaslinks * sizeof(aas_link_t));
	} 
	aasworld.linkheap[0].prev_ent = NULL;
	aasworld.linkheap[0].next_ent = &aasworld.linkheap[1];
	for (i = 1; i < max_aaslinks-1; i++)
	{
		aasworld.linkheap[i].prev_ent = &aasworld.linkheap[i - 1];
		aasworld.linkheap[i].next_ent = &aasworld.linkheap[i + 1];
	} 
	aasworld.linkheap[max_aaslinks-1].prev_ent = &aasworld.linkheap[max_aaslinks-2];
	aasworld.linkheap[max_aaslinks-1].next_ent = NULL;
	aasworld.freelinks = &aasworld.linkheap[0];
	numaaslinks = max_aaslinks;
} 
void AAS_FreeAASLinkHeap(void)
{
	if (aasworld.linkheap) FreeMemory(aasworld.linkheap);
	aasworld.linkheap = NULL;
	aasworld.linkheapsize = 0;
} 
aas_link_t *AAS_AllocAASLink(void)
{
	aas_link_t *link;

	link = aasworld.freelinks;
	if (!link)
	{
#ifndef BSPC
		if (botDeveloper)
#endif
		{
			botimport.Print(PRT_FATAL, "empty aas link heap\n");
		} 
		return NULL;
	} 
	if (aasworld.freelinks) aasworld.freelinks = aasworld.freelinks->next_ent;
	if (aasworld.freelinks) aasworld.freelinks->prev_ent = NULL;
	numaaslinks--;
	return link;
} 
void AAS_DeAllocAASLink(aas_link_t *link)
{
	if (aasworld.freelinks) aasworld.freelinks->prev_ent = link;
	link->prev_ent = NULL;
	link->next_ent = aasworld.freelinks;
	link->prev_area = NULL;
	link->next_area = NULL;
	aasworld.freelinks = link;
	numaaslinks++;
} 
void AAS_InitAASLinkedEntities(void)
{
	if (!aasworld.loaded) return;
	if (aasworld.arealinkedentities) FreeMemory(aasworld.arealinkedentities);
	aasworld.arealinkedentities = (aas_link_t **) GetClearedHunkMemory(
						aasworld.numareas * sizeof(aas_link_t *));
} 
void AAS_FreeAASLinkedEntities(void)
{
	if (aasworld.arealinkedentities) FreeMemory(aasworld.arealinkedentities);
	aasworld.arealinkedentities = NULL;
} 
int AAS_PointAreaNum(vec3_t point)
{
	int nodenum;
	vec_t	dist;
	aas_node_t *node;
	aas_plane_t *plane;

	if (!aasworld.loaded)
	{
		botimport.Print(PRT_ERROR, "AAS_PointAreaNum: aas not loaded\n");
		return 0;
	} 

	nodenum = 1;
	while (nodenum > 0)
	{
#ifdef AAS_SAMPLE_DEBUG
		if (nodenum >= aasworld.numnodes)
		{
			botimport.Print(PRT_ERROR, "nodenum = %d >= aasworld.numnodes = %d\n", nodenum, aasworld.numnodes);
			return 0;
		} 
#endif 
		node = &aasworld.nodes[nodenum];
#ifdef AAS_SAMPLE_DEBUG
		if (node->planenum < 0 || node->planenum >= aasworld.numplanes)
		{
			botimport.Print(PRT_ERROR, "node->planenum = %d >= aasworld.numplanes = %d\n", node->planenum, aasworld.numplanes);
			return 0;
		} 
#endif 
		plane = &aasworld.planes[node->planenum];
		dist = DotProduct(point, plane->normal) - plane->dist;
		if (dist > 0) nodenum = node->children[0];
		else nodenum = node->children[1];
	} 
	if (!nodenum)
	{
#ifdef AAS_SAMPLE_DEBUG
		botimport.Print(PRT_MESSAGE, "in solid\n");
#endif 
		return 0;
	} 
	return -nodenum;
} 
int AAS_PointReachabilityAreaIndex( vec3_t origin )
{
	int areanum, cluster, i, index;

	if (!aasworld.initialized)
		return 0;

	if ( !origin )
	{
		index = 0;
		for (i = 0; i < aasworld.numclusters; i++)
		{
			index += aasworld.clusters[i].numreachabilityareas;
		} 
		return index;
	} 

	areanum = AAS_PointAreaNum( origin );
	if ( !areanum || !AAS_AreaReachability(areanum) )
		return 0;
	cluster = aasworld.areasettings[areanum].cluster;
	areanum = aasworld.areasettings[areanum].clusterareanum;
	if (cluster < 0)
	{
		cluster = aasworld.portals[-cluster].frontcluster;
		areanum = aasworld.portals[-cluster].clusterareanum[0];
	} 

	index = 0;
	for (i = 0; i < cluster; i++)
	{
		index += aasworld.clusters[i].numreachabilityareas;
	} 
	index += areanum;
	return index;
} 
int AAS_AreaCluster(int areanum)
{
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		botimport.Print(PRT_ERROR, "AAS_AreaCluster: invalid area number\n");
		return 0;
	} 
	return aasworld.areasettings[areanum].cluster;
} 
int AAS_AreaPresenceType(int areanum)
{
	if (!aasworld.loaded) return 0;
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		botimport.Print(PRT_ERROR, "AAS_AreaPresenceType: invalid area number\n");
		return 0;
	} 
	return aasworld.areasettings[areanum].presencetype;
} 
int AAS_PointPresenceType(vec3_t point)
{
	int areanum;

	if (!aasworld.loaded) return 0;

	areanum = AAS_PointAreaNum(point);
	if (!areanum) return PRESENCE_NONE;
	return aasworld.areasettings[areanum].presencetype;
} 
vec_t AAS_BoxOriginDistanceFromPlane(vec3_t normal, vec3_t mins, vec3_t maxs, int side)
{
	vec3_t v1, v2;
	int i;

	if (side)
	{
		for (i = 0; i < 3; i++)
		{
			if (normal[i] > BBOX_NORMAL_EPSILON) v1[i] = maxs[i];
			else if (normal[i] < -BBOX_NORMAL_EPSILON) v1[i] = mins[i];
			else v1[i] = 0;
		} 
	} 
	else
	{
		for (i = 0; i < 3; i++)
		{
			if (normal[i] > BBOX_NORMAL_EPSILON) v1[i] = mins[i];
			else if (normal[i] < -BBOX_NORMAL_EPSILON) v1[i] = maxs[i];
			else v1[i] = 0;
		} 
	} 
	VectorCopy(normal, v2);
	VectorInverse(v2);
	return DotProduct(v1, v2);
} 
qboolean AAS_AreaEntityCollision(int areanum, vec3_t start, vec3_t end,
										int presencetype, int passent, aas_trace_t *trace)
{
	int collision;
	vec3_t boxmins, boxmaxs;
	aas_link_t *link;
	bsp_trace_t bsptrace;

	AAS_PresenceTypeBoundingBox(presencetype, boxmins, boxmaxs);

	Com_Memset(&bsptrace, 0, sizeof(bsp_trace_t)); 
	bsptrace.fraction = 1;
	collision = qfalse;
	for (link = aasworld.arealinkedentities[areanum]; link; link = link->next_ent)
	{
		if (link->entnum == passent) continue;
		if (AAS_EntityCollision(link->entnum, start, boxmins, boxmaxs, end,
												CONTENTS_SOLID|CONTENTS_PLAYERCLIP, &bsptrace))
		{
			collision = qtrue;
		} 
	} 
	if (collision)
	{
		trace->startsolid = bsptrace.startsolid;
		trace->ent = bsptrace.ent;
		VectorCopy(bsptrace.endpos, trace->endpos);
		trace->area = 0;
		trace->planenum = 0;
		return qtrue;
	} 
	return qfalse;
} 
aas_trace_t AAS_TraceClientBBox(vec3_t start, vec3_t end, int presencetype,
																				int passent)
{
	int side, nodenum, tmpplanenum;
	float front, back, frac;
	vec3_t cur_start, cur_end, cur_mid, v1, v2;
	aas_tracestack_t tracestack[127];
	aas_tracestack_t *tstack_p;
	aas_node_t *aasnode;
	aas_plane_t *plane;
	aas_trace_t trace;

	Com_Memset(&trace, 0, sizeof(aas_trace_t));

	if (!aasworld.loaded) return trace;
	
	tstack_p = tracestack;
	VectorCopy(start, tstack_p->start);
	VectorCopy(end, tstack_p->end);
	tstack_p->planenum = 0;
	tstack_p->nodenum = 1;		
	tstack_p++;
	
	while (1)
	{
		tstack_p--;
		if (tstack_p < tracestack)
		{
			tstack_p++;
			trace.startsolid = qfalse;
			trace.fraction = 1.0;
			VectorCopy(end, trace.endpos);
			trace.ent = 0;
			trace.area = 0;
			trace.planenum = 0;
			return trace;
		} 
		nodenum = tstack_p->nodenum;
		if (nodenum < 0)
		{
#ifdef AAS_SAMPLE_DEBUG
			if (-nodenum > aasworld.numareasettings)
			{
				botimport.Print(PRT_ERROR, "AAS_TraceBoundingBox: -nodenum out of range\n");
				return trace;
			} 
#endif 
			if (!(aasworld.areasettings[-nodenum].presencetype & presencetype))
			{
				if (tstack_p->start[0] == start[0] &&
						tstack_p->start[1] == start[1] &&
						tstack_p->start[2] == start[2])
				{
					trace.startsolid = qtrue;
					trace.fraction = 0.0;
					VectorClear(v1);
				} 
				else
				{
					trace.startsolid = qfalse;
					VectorSubtract(end, start, v1);
					VectorSubtract(tstack_p->start, start, v2);
					trace.fraction = VectorLength(v2) / VectorNormalize(v1);
					VectorMA(tstack_p->start, -0.125, v1, tstack_p->start);
				} 
				VectorCopy(tstack_p->start, trace.endpos);
				trace.ent = 0;
				trace.area = -nodenum;
				trace.planenum = tstack_p->planenum;
				plane = &aasworld.planes[trace.planenum];
				if (DotProduct(v1, plane->normal) > 0) trace.planenum ^= 1;
				return trace;
			} 
			else
			{
				if (passent >= 0)
				{
					if (AAS_AreaEntityCollision(-nodenum, tstack_p->start,
													tstack_p->end, presencetype, passent,
													&trace))
					{
						if (!trace.startsolid)
						{
							VectorSubtract(end, start, v1);
							VectorSubtract(trace.endpos, start, v2);
							trace.fraction = VectorLength(v2) / VectorLength(v1);
						} 
						return trace;
					} 
				} 
			} 
			trace.lastarea = -nodenum;
			continue;
		} 
		if (!nodenum)
		{
			if (tstack_p->start[0] == start[0] &&
					tstack_p->start[1] == start[1] &&
					tstack_p->start[2] == start[2])
			{
				trace.startsolid = qtrue;
				trace.fraction = 0.0;
				VectorClear(v1);
			} 
			else
			{
				trace.startsolid = qfalse;
				VectorSubtract(end, start, v1);
				VectorSubtract(tstack_p->start, start, v2);
				trace.fraction = VectorLength(v2) / VectorNormalize(v1);
				VectorMA(tstack_p->start, -0.125, v1, tstack_p->start);
			} 
			VectorCopy(tstack_p->start, trace.endpos);
			trace.ent = 0;
			trace.area = 0;	
			trace.planenum = tstack_p->planenum;
			plane = &aasworld.planes[trace.planenum];
			if (DotProduct(v1, plane->normal) > 0) trace.planenum ^= 1;
			return trace;
		} 
#ifdef AAS_SAMPLE_DEBUG
		if (nodenum > aasworld.numnodes)
		{
			botimport.Print(PRT_ERROR, "AAS_TraceBoundingBox: nodenum out of range\n");
			return trace;
		} 
#endif 
		aasnode = &aasworld.nodes[nodenum];
		VectorCopy(tstack_p->start, cur_start);
		VectorCopy(tstack_p->end, cur_end);
		plane = &aasworld.planes[aasnode->planenum];

		switch(plane->type)
		{
			default: 
			{
				front = DotProduct(cur_start, plane->normal) - plane->dist;
				back = DotProduct(cur_end, plane->normal) - plane->dist;
				break;
			} 
		} 
		if ((front >= -ON_EPSILON && back >= -ON_EPSILON))
		{
			tstack_p->nodenum = aasnode->children[0];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceBoundingBox: stack overflow\n");
				return trace;
			} 
		} 
		else if ((front < ON_EPSILON && back < ON_EPSILON))
		{
			tstack_p->nodenum = aasnode->children[1];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceBoundingBox: stack overflow\n");
				return trace;
			} 
		} 
		else
		{
			tmpplanenum = tstack_p->planenum;
			if ( front == back ) front -= 0.001f; 
			if (front < 0) frac = (front + TRACEPLANE_EPSILON)/(front-back);
			else frac = (front - TRACEPLANE_EPSILON)/(front-back); 
			if (frac < 0)
				frac = 0.001f; 
			else if (frac > 1)
				frac = 0.999f; 
			cur_mid[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac;
			cur_mid[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac;
			cur_mid[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac;

			side = front < 0;
			VectorCopy(cur_mid, tstack_p->start);
			tstack_p->planenum = aasnode->planenum;
			tstack_p->nodenum = aasnode->children[!side];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceBoundingBox: stack overflow\n");
				return trace;
			} 
			VectorCopy(cur_start, tstack_p->start);
			VectorCopy(cur_mid, tstack_p->end);
			tstack_p->planenum = tmpplanenum;
			tstack_p->nodenum = aasnode->children[side];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceBoundingBox: stack overflow\n");
				return trace;
			} 
		} 
	} 
} 
int AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas)
{
	int side, nodenum, tmpplanenum;
	int numareas;
	float front, back, frac;
	vec3_t cur_start, cur_end, cur_mid;
	aas_tracestack_t tracestack[127];
	aas_tracestack_t *tstack_p;
	aas_node_t *aasnode;
	aas_plane_t *plane;

	numareas = 0;
	areas[0] = 0;
	if (!aasworld.loaded) return numareas;

	tstack_p = tracestack;
	VectorCopy(start, tstack_p->start);
	VectorCopy(end, tstack_p->end);
	tstack_p->planenum = 0;
	tstack_p->nodenum = 1;		
	tstack_p++;

	while (1)
	{
		tstack_p--;
		if (tstack_p < tracestack)
		{
			return numareas;
		} 
		nodenum = tstack_p->nodenum;
		if (nodenum < 0)
		{
#ifdef AAS_SAMPLE_DEBUG
			if (-nodenum > aasworld.numareasettings)
			{
				botimport.Print(PRT_ERROR, "AAS_TraceAreas: -nodenum = %d out of range\n", -nodenum);
				return numareas;
			} 
#endif 
			areas[numareas] = -nodenum;
			if (points) VectorCopy(tstack_p->start, points[numareas]);
			numareas++;
			if (numareas >= maxareas) return numareas;
			continue;
		} 
		if (!nodenum)
		{
			continue;
		} 
#ifdef AAS_SAMPLE_DEBUG
		if (nodenum > aasworld.numnodes)
		{
			botimport.Print(PRT_ERROR, "AAS_TraceAreas: nodenum out of range\n");
			return numareas;
		} 
#endif 
		aasnode = &aasworld.nodes[nodenum];
		VectorCopy(tstack_p->start, cur_start);
		VectorCopy(tstack_p->end, cur_end);
		plane = &aasworld.planes[aasnode->planenum];

		switch(plane->type)
		{
			default: 
			{
				front = DotProduct(cur_start, plane->normal) - plane->dist;
				back = DotProduct(cur_end, plane->normal) - plane->dist;
				break;
			} 
		} 

		if (front > 0 && back > 0)
		{
			tstack_p->nodenum = aasnode->children[0];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceAreas: stack overflow\n");
				return numareas;
			} 
		} 
		else if (front <= 0 && back <= 0)
		{
			tstack_p->nodenum = aasnode->children[1];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceAreas: stack overflow\n");
				return numareas;
			} 
		} 
		else
		{
			tmpplanenum = tstack_p->planenum;
			if (front < 0) frac = (front)/(front-back);
			else frac = (front)/(front-back);
			if (frac < 0) frac = 0;
			else if (frac > 1) frac = 1;
			cur_mid[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac;
			cur_mid[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac;
			cur_mid[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac;

			side = front < 0;
			VectorCopy(cur_mid, tstack_p->start);
			tstack_p->planenum = aasnode->planenum;
			tstack_p->nodenum = aasnode->children[!side];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceAreas: stack overflow\n");
				return numareas;
			} 
			VectorCopy(cur_start, tstack_p->start);
			VectorCopy(cur_mid, tstack_p->end);
			tstack_p->planenum = tmpplanenum;
			tstack_p->nodenum = aasnode->children[side];
			tstack_p++;
			if (tstack_p >= &tracestack[127])
			{
				botimport.Print(PRT_ERROR, "AAS_TraceAreas: stack overflow\n");
				return numareas;
			} 
		} 
	} 
} 
#define AAS_OrthogonalToVectors(v1, v2, res) \
	(res)[0] = ((v1)[1] * (v2)[2]) - ((v1)[2] * (v2)[1]);\
	(res)[1] = ((v1)[2] * (v2)[0]) - ((v1)[0] * (v2)[2]);\
	(res)[2] = ((v1)[0] * (v2)[1]) - ((v1)[1] * (v2)[0]);
qboolean AAS_InsideFace(aas_face_t *face, vec3_t pnormal, vec3_t point, float epsilon)
{
	int i, firstvertex, edgenum;
	vec3_t v0;
	vec3_t edgevec, pointvec, sepnormal;
	aas_edge_t *edge;
#ifdef AAS_SAMPLE_DEBUG
	int lastvertex = 0;
#endif 

	if (!aasworld.loaded) return qfalse;

	for (i = 0; i < face->numedges; i++)
	{
		edgenum = aasworld.edgeindex[face->firstedge + i];
		edge = &aasworld.edges[abs(edgenum)];
		firstvertex = edgenum < 0;
		VectorCopy(aasworld.vertexes[edge->v[firstvertex]], v0);
		VectorSubtract(aasworld.vertexes[edge->v[!firstvertex]], v0, edgevec);
#ifdef AAS_SAMPLE_DEBUG
		if (lastvertex && lastvertex != edge->v[firstvertex])
		{
			botimport.Print(PRT_MESSAGE, "winding not counter clockwise\n");
		} 
		lastvertex = edge->v[!firstvertex];
#endif 
		VectorSubtract(point, v0, pointvec);
		AAS_OrthogonalToVectors(edgevec, pnormal, sepnormal);
		if (DotProduct(pointvec, sepnormal) < -epsilon) return qfalse;
	} 
	return qtrue;
} 
qboolean AAS_PointInsideFace(int facenum, vec3_t point, float epsilon)
{
	int i, firstvertex, edgenum;
	vec_t *v1, *v2;
	vec3_t edgevec, pointvec, sepnormal;
	aas_edge_t *edge;
	aas_plane_t *plane;
	aas_face_t *face;

	if (!aasworld.loaded) return qfalse;

	face = &aasworld.faces[facenum];
	plane = &aasworld.planes[face->planenum];
	for (i = 0; i < face->numedges; i++)
	{
		edgenum = aasworld.edgeindex[face->firstedge + i];
		edge = &aasworld.edges[abs(edgenum)];
		firstvertex = edgenum < 0;
		v1 = aasworld.vertexes[edge->v[firstvertex]];
		v2 = aasworld.vertexes[edge->v[!firstvertex]];
		VectorSubtract(v2, v1, edgevec);
		VectorSubtract(point, v1, pointvec);
		CrossProduct(edgevec, plane->normal, sepnormal);
		if (DotProduct(pointvec, sepnormal) < -epsilon) return qfalse;
	} 
	return qtrue;
} 
aas_face_t *AAS_AreaGroundFace(int areanum, vec3_t point)
{
	int i, facenum;
	vec3_t up = {0, 0, 1};
	vec3_t normal;
	aas_area_t *area;
	aas_face_t *face;

	if (!aasworld.loaded) return NULL;

	area = &aasworld.areas[areanum];
	for (i = 0; i < area->numfaces; i++)
	{
		facenum = aasworld.faceindex[area->firstface + i];
		face = &aasworld.faces[abs(facenum)];
		if (face->faceflags & FACE_GROUND)
		{
			if (aasworld.planes[face->planenum].normal[2] < 0) VectorNegate(up, normal);
			else VectorCopy(up, normal);
			if (AAS_InsideFace(face, normal, point, 0.01f)) return face;
		} 
	} 
	return NULL;
} 
void AAS_FacePlane(int facenum, vec3_t normal, float *dist)
{
	aas_plane_t *plane;

	plane = &aasworld.planes[aasworld.faces[facenum].planenum];
	VectorCopy(plane->normal, normal);
	*dist = plane->dist;
} 
aas_face_t *AAS_TraceEndFace(aas_trace_t *trace)
{
	int i, facenum;
	aas_area_t *area;
	aas_face_t *face, *firstface = NULL;

	if (!aasworld.loaded) return NULL;

	if (trace->startsolid) return NULL;
	area = &aasworld.areas[trace->lastarea];
	for (i = 0; i < area->numfaces; i++)
	{
		facenum = aasworld.faceindex[area->firstface + i];
		face = &aasworld.faces[abs(facenum)];
		if ((face->planenum & ~1) == (trace->planenum & ~1))
		{
			if (AAS_InsideFace(face,
						aasworld.planes[face->planenum].normal, trace->endpos, 0.01f)) return face;
		} 
	} 
	return firstface;
} 
int AAS_BoxOnPlaneSide2(vec3_t absmins, vec3_t absmaxs, aas_plane_t *p)
{
	int i, sides;
	float dist1, dist2;
	vec3_t corners[2];

	for (i = 0; i < 3; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = absmins[i];
			corners[1][i] = absmaxs[i];
		} 
		else
		{
			corners[1][i] = absmins[i];
			corners[0][i] = absmaxs[i];
		} 
	} 
	dist1 = DotProduct(p->normal, corners[0]) - p->dist;
	dist2 = DotProduct(p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0) sides = 1;
	if (dist2 < 0) sides |= 2;

	return sides;
} 
#define AAS_BoxOnPlaneSide(absmins, absmaxs, p) (\
	( (p)->type < 3) ?\
	(\
		( (p)->dist <= (absmins)[(p)->type]) ?\
		(\
			1\
		)\
		:\
		(\
			( (p)->dist >= (absmaxs)[(p)->type]) ?\
			(\
				2\
			)\
			:\
			(\
				3\
			)\
		)\
	)\
	:\
	(\
		AAS_BoxOnPlaneSide2((absmins), (absmaxs), (p))\
	)\
) 
void AAS_UnlinkFromAreas(aas_link_t *areas)
{
	aas_link_t *link, *nextlink;

	for (link = areas; link; link = nextlink)
	{
		nextlink = link->next_area;
		if (link->prev_ent) link->prev_ent->next_ent = link->next_ent;
		else aasworld.arealinkedentities[link->areanum] = link->next_ent;
		if (link->next_ent) link->next_ent->prev_ent = link->prev_ent;
		AAS_DeAllocAASLink(link);
	} 
} 

typedef struct
{
	int nodenum;		
} aas_linkstack_t;

aas_link_t *AAS_AASLinkEntity(vec3_t absmins, vec3_t absmaxs, int entnum)
{
	int side, nodenum;
	aas_linkstack_t linkstack[128];
	aas_linkstack_t *lstack_p;
	aas_node_t *aasnode;
	aas_plane_t *plane;
	aas_link_t *link, *areas;

	if (!aasworld.loaded)
	{
		botimport.Print(PRT_ERROR, "AAS_LinkEntity: aas not loaded\n");
		return NULL;
	} 

	areas = NULL;
	lstack_p = linkstack;
	lstack_p->nodenum = 1;		
	lstack_p++;
	
	while (1)
	{
		lstack_p--;
		if (lstack_p < linkstack) break;
		nodenum = lstack_p->nodenum;
		if (nodenum < 0)
		{
			for (link = aasworld.arealinkedentities[-nodenum]; link; link = link->next_ent)
			{
				if (link->entnum == entnum) break;
			} 
			if (link) continue;
			link = AAS_AllocAASLink();
			if (!link) return areas;
			link->entnum = entnum;
			link->areanum = -nodenum;
			link->prev_area = NULL;
			link->next_area = areas;
			if (areas) areas->prev_area = link;
			areas = link;
			link->prev_ent = NULL;
			link->next_ent = aasworld.arealinkedentities[-nodenum];
			if (aasworld.arealinkedentities[-nodenum])
					aasworld.arealinkedentities[-nodenum]->prev_ent = link;
			aasworld.arealinkedentities[-nodenum] = link;
			continue;
		} 
		if (!nodenum) continue;
		aasnode = &aasworld.nodes[nodenum];
		plane = &aasworld.planes[aasnode->planenum];
		side = AAS_BoxOnPlaneSide2(absmins, absmaxs, plane);
		if (side & 1)
		{
			lstack_p->nodenum = aasnode->children[0];
			lstack_p++;
		} 
		if (lstack_p >= &linkstack[127])
		{
			botimport.Print(PRT_ERROR, "AAS_LinkEntity: stack overflow\n");
			break;
		} 
		if (side & 2)
		{
			lstack_p->nodenum = aasnode->children[1];
			lstack_p++;
		} 
		if (lstack_p >= &linkstack[127])
		{
			botimport.Print(PRT_ERROR, "AAS_LinkEntity: stack overflow\n");
			break;
		} 
	} 
	return areas;
} 
aas_link_t *AAS_LinkEntityClientBBox(vec3_t absmins, vec3_t absmaxs, int entnum, int presencetype)
{
	vec3_t mins, maxs;
	vec3_t newabsmins, newabsmaxs;

	AAS_PresenceTypeBoundingBox(presencetype, mins, maxs);
	VectorSubtract(absmins, maxs, newabsmins);
	VectorSubtract(absmaxs, mins, newabsmaxs);
	return AAS_AASLinkEntity(newabsmins, newabsmaxs, entnum);
} 
int AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas)
{
	aas_link_t *linkedareas, *link;
	int num;

	linkedareas = AAS_AASLinkEntity(absmins, absmaxs, -1);
	num = 0;
	for (link = linkedareas; link; link = link->next_area)
	{
		areas[num] = link->areanum;
		num++;
		if (num >= maxareas)
			break;
	} 
	AAS_UnlinkFromAreas(linkedareas);
	return num;
} 
int AAS_AreaInfo( int areanum, aas_areainfo_t *info )
{
	aas_areasettings_t *settings;
	if (!info)
		return 0;
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		botimport.Print(PRT_ERROR, "AAS_AreaInfo: areanum %d out of range\n", areanum);
		return 0;
	} 
	settings = &aasworld.areasettings[areanum];
	info->cluster = settings->cluster;
	info->contents = settings->contents;
	info->flags = settings->areaflags;
	info->presencetype = settings->presencetype;
	VectorCopy(aasworld.areas[areanum].mins, info->mins);
	VectorCopy(aasworld.areas[areanum].maxs, info->maxs);
	VectorCopy(aasworld.areas[areanum].center, info->center);
	return sizeof(aas_areainfo_t);
} 
aas_plane_t *AAS_PlaneFromNum(int planenum)
{
	if (!aasworld.loaded) return NULL;

	return &aasworld.planes[planenum];
} 
