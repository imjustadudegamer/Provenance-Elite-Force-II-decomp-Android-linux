

#include "../qcommon/q_shared.h"
#include "l_libvar.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

typedef struct optimized_s
{
	int numvertexes;
	aas_vertex_t *vertexes;
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
	int *vertexoptimizeindex;
	int *edgeoptimizeindex;
	int *faceoptimizeindex;
} optimized_t;

int AAS_KeepEdge(aas_edge_t *edge)
{
	return 1;
} 
int AAS_OptimizeEdge(optimized_t *optimized, int edgenum)
{
	int i, optedgenum;
	aas_edge_t *edge, *optedge;

	edge = &aasworld.edges[abs(edgenum)];
	if (!AAS_KeepEdge(edge)) return 0;

	optedgenum = optimized->edgeoptimizeindex[abs(edgenum)];
	if (optedgenum)
	{
		if (edgenum > 0) return optedgenum;
		else return -optedgenum;
	} 

	optedge = &optimized->edges[optimized->numedges];

	for (i = 0; i < 2; i++)
	{
		if (optimized->vertexoptimizeindex[edge->v[i]])
		{
			optedge->v[i] = optimized->vertexoptimizeindex[edge->v[i]];
		} 
		else
		{
			VectorCopy(aasworld.vertexes[edge->v[i]], optimized->vertexes[optimized->numvertexes]);
			optedge->v[i] = optimized->numvertexes;
			optimized->vertexoptimizeindex[edge->v[i]] = optimized->numvertexes;
			optimized->numvertexes++;
		} 
	} 
	optimized->edgeoptimizeindex[abs(edgenum)] = optimized->numedges;
	optedgenum = optimized->numedges;
	optimized->numedges++;
	if (edgenum > 0) return optedgenum;
	else return -optedgenum;
} 
int AAS_KeepFace(aas_face_t *face)
{
	if (!(face->faceflags & FACE_LADDER)) return 0;
	else return 1;
} 
int AAS_OptimizeFace(optimized_t *optimized, int facenum)
{
	int i, edgenum, optedgenum, optfacenum;
	aas_face_t *face, *optface;

	face = &aasworld.faces[abs(facenum)];
	if (!AAS_KeepFace(face)) return 0;

	optfacenum = optimized->faceoptimizeindex[abs(facenum)];
	if (optfacenum)
	{
		if (facenum > 0) return optfacenum;
		else return -optfacenum;
	} 

	optface = &optimized->faces[optimized->numfaces];
	Com_Memcpy(optface, face, sizeof(aas_face_t));

	optface->numedges = 0;
	optface->firstedge = optimized->edgeindexsize;
	for (i = 0; i < face->numedges; i++)
	{
		edgenum = aasworld.edgeindex[face->firstedge + i];
		optedgenum = AAS_OptimizeEdge(optimized, edgenum);
		if (optedgenum)
		{
			optimized->edgeindex[optface->firstedge + optface->numedges] = optedgenum;
			optface->numedges++;
			optimized->edgeindexsize++;
		} 
	} 
	optimized->faceoptimizeindex[abs(facenum)] = optimized->numfaces;
	optfacenum = optimized->numfaces;
	optimized->numfaces++;
	if (facenum > 0) return optfacenum;
	else return -optfacenum;
} 
void AAS_OptimizeArea(optimized_t *optimized, int areanum)
{
	int i, facenum, optfacenum;
	aas_area_t *area, *optarea;

	area = &aasworld.areas[areanum];
	optarea = &optimized->areas[areanum];
	Com_Memcpy(optarea, area, sizeof(aas_area_t));

	optarea->numfaces = 0;
	optarea->firstface = optimized->faceindexsize;
	for (i = 0; i < area->numfaces; i++)
	{
		facenum = aasworld.faceindex[area->firstface + i];
		optfacenum = AAS_OptimizeFace(optimized, facenum);
		if (optfacenum)
		{
			optimized->faceindex[optarea->firstface + optarea->numfaces] = optfacenum;
			optarea->numfaces++;
			optimized->faceindexsize++;
		} 
	} 
} 
void AAS_OptimizeAlloc(optimized_t *optimized)
{
	optimized->vertexes = (aas_vertex_t *) GetClearedMemory(aasworld.numvertexes * sizeof(aas_vertex_t));
	optimized->numvertexes = 0;
	optimized->edges = (aas_edge_t *) GetClearedMemory(aasworld.numedges * sizeof(aas_edge_t));
	optimized->numedges = 1; 
	optimized->edgeindex = (aas_edgeindex_t *) GetClearedMemory(aasworld.edgeindexsize * sizeof(aas_edgeindex_t));
	optimized->edgeindexsize = 0;
	optimized->faces = (aas_face_t *) GetClearedMemory(aasworld.numfaces * sizeof(aas_face_t));
	optimized->numfaces = 1; 
	optimized->faceindex = (aas_faceindex_t *) GetClearedMemory(aasworld.faceindexsize * sizeof(aas_faceindex_t));
	optimized->faceindexsize = 0;
	optimized->areas = (aas_area_t *) GetClearedMemory(aasworld.numareas * sizeof(aas_area_t));
	optimized->numareas = aasworld.numareas;
	optimized->vertexoptimizeindex = (int *) GetClearedMemory(aasworld.numvertexes * sizeof(int));
	optimized->edgeoptimizeindex = (int *) GetClearedMemory(aasworld.numedges * sizeof(int));
	optimized->faceoptimizeindex = (int *) GetClearedMemory(aasworld.numfaces * sizeof(int));
} 
void AAS_OptimizeStore(optimized_t *optimized)
{
	if (aasworld.vertexes) FreeMemory(aasworld.vertexes);
	aasworld.vertexes = optimized->vertexes;
	aasworld.numvertexes = optimized->numvertexes;
	if (aasworld.edges) FreeMemory(aasworld.edges);
	aasworld.edges = optimized->edges;
	aasworld.numedges = optimized->numedges;
	if (aasworld.edgeindex) FreeMemory(aasworld.edgeindex);
	aasworld.edgeindex = optimized->edgeindex;
	aasworld.edgeindexsize = optimized->edgeindexsize;
	if (aasworld.faces) FreeMemory(aasworld.faces);
	aasworld.faces = optimized->faces;
	aasworld.numfaces = optimized->numfaces;
	if (aasworld.faceindex) FreeMemory(aasworld.faceindex);
	aasworld.faceindex = optimized->faceindex;
	aasworld.faceindexsize = optimized->faceindexsize;
	if (aasworld.areas) FreeMemory(aasworld.areas);
	aasworld.areas = optimized->areas;
	aasworld.numareas = optimized->numareas;
	FreeMemory(optimized->vertexoptimizeindex);
	FreeMemory(optimized->edgeoptimizeindex);
	FreeMemory(optimized->faceoptimizeindex);
} 
void AAS_Optimize(void)
{
	int i, sign;
	optimized_t optimized;

	AAS_OptimizeAlloc(&optimized);
	for (i = 1; i < aasworld.numareas; i++)
	{
		AAS_OptimizeArea(&optimized, i);
	} 
	for (i = 0; i < aasworld.reachabilitysize; i++)
	{
		if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_ELEVATOR) continue;
		if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_JUMPPAD) continue;
		if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_FUNCBOB) continue;
		sign = aasworld.reachability[i].facenum;
		aasworld.reachability[i].facenum = optimized.faceoptimizeindex[abs(aasworld.reachability[i].facenum)];
		if (sign < 0) aasworld.reachability[i].facenum = -aasworld.reachability[i].facenum;
		sign = aasworld.reachability[i].edgenum;
		aasworld.reachability[i].edgenum = optimized.edgeoptimizeindex[abs(aasworld.reachability[i].edgenum)];
		if (sign < 0) aasworld.reachability[i].edgenum = -aasworld.reachability[i].edgenum;
	} 
	AAS_OptimizeStore(&optimized);
	botimport.Print(PRT_MESSAGE, "AAS data optimized.\n");
} 
