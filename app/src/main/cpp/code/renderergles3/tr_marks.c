
#include "tr_local.h"

#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

static int s_markVisited;       
static int s_markRejFlags;      
static int s_markRejPlane;      
static int s_markRejAABB;       
static int s_markRejType;       
static int s_markListFull;      
static int s_markAdded;         
static int s_markGridTri, s_markGridAcc;
static int s_markTerrTri, s_markTerrAcc;
static int s_markFaceTri, s_markFaceAcc, s_markFaceRejNormal;
static int s_markAddCalls, s_markAddChopOverflow, s_markAddChopEmpty, s_markAddNoSpace, s_markAddOk;
static int s_markBailCount;

#define UIVK_MARK_RESET() do { \
    s_markVisited = s_markRejFlags = s_markRejPlane = s_markRejAABB = s_markRejType = 0; \
    s_markListFull = s_markAdded = 0; \
    s_markGridTri = s_markGridAcc = s_markTerrTri = s_markTerrAcc = 0; \
    s_markFaceTri = s_markFaceAcc = s_markFaceRejNormal = 0; \
    s_markAddCalls = s_markAddChopOverflow = s_markAddChopEmpty = s_markAddNoSpace = s_markAddOk = 0; \
} while (0)

#define MAX_VERTS_ON_POLY		64

#define MARKER_OFFSET			0.2f

#define MAX_MARK_SURFACES		1024

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
static void R_ChopPolyBehindPlane( int numInPoints, const vec3_t inPoints[MAX_VERTS_ON_POLY],
								   int *numOutPoints, vec3_t outPoints[MAX_VERTS_ON_POLY], 
							const vec3_t normal, vec_t dist, vec_t epsilon) {
	float		dists[MAX_VERTS_ON_POLY+4];
	int			sides[MAX_VERTS_ON_POLY+4];
	int			counts[3];
	float		dot;
	int			i, j;
	const float* p1, * p2;
	float		*clip;
	float		d;

	if ( numInPoints >= MAX_VERTS_ON_POLY - 2 ) {
		s_markAddChopOverflow++;
		*numOutPoints = 0;
		return;
	}

	counts[0] = counts[1] = counts[2] = 0;

	for ( i = 0 ; i < numInPoints ; i++ ) {
		dot = DotProduct( inPoints[i], normal );
		dot -= dist;
		dists[i] = dot;
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*numOutPoints = 0;

	if ( !counts[0] ) {
		return;
	}
	if ( !counts[1] ) {
		*numOutPoints = numInPoints;
		Com_Memcpy( outPoints, inPoints, numInPoints * sizeof(vec3_t) );
		return;
	}

	for ( i = 0 ; i < numInPoints ; i++ ) {
		p1 = inPoints[i];
		clip = outPoints[ *numOutPoints ];
		
		if ( sides[i] == SIDE_ON ) {
			VectorCopy( p1, clip );
			(*numOutPoints)++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, clip );
			(*numOutPoints)++;
			clip = outPoints[ *numOutPoints ];
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		p2 = inPoints[ (i+1) % numInPoints ];

		d = dists[i] - dists[i+1];
		if ( d == 0 ) {
			dot = 0;
		} else {
			dot = dists[i] / d;
		}


		for (j=0 ; j<3 ; j++) {
			clip[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		(*numOutPoints)++;
	}
}

void R_BoxSurfaces_r(mnode_t *node, vec3_t mins, vec3_t maxs, surfaceType_t **list, int listsize, int *listlength, vec3_t dir) {

	int			s, c;
	msurface_t	*surf, **mark;

	(void)dir;

	while ( node->contents == -1 ) {
		s = BoxOnPlaneSide( mins, maxs, node->plane );
		if (s == 1) {
			node = node->children[0];
		} else if (s == 2) {
			node = node->children[1];
		} else {
			R_BoxSurfaces_r(node->children[0], mins, maxs, list, listsize, listlength, dir);
			node = node->children[1];
		}
	}

	// [UIVK:DO] retail R_BoxSurfaces_r @0x0001F640 is FOUR gates, not five:
	// angles with the projection direction' normal test is NOT here in retail -- it
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;
	while (c--) {
		if (*listlength >= listsize) { s_markListFull++; break; }
		surf = *mark;
		s_markVisited++;
		if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) )
			|| ( surf->shader->contentFlags & CONTENTS_FOG ) ) {
			s_markRejFlags++;
			surf->viewCount = tr.viewCount;
		}
		else if (*(surf->data) == SF_FACE) {
			s = BoxOnPlaneSide( mins, maxs, &(( srfSurfaceFace_t * ) surf->data)->plane );
			if (s == 1 || s == 2) {
				s_markRejPlane++;
				surf->viewCount = tr.viewCount;
			}
		}
		else if (*(surfaceType_t *) (surf->data) == SF_TERRAIN
			|| *(surfaceType_t *) (surf->data) == SF_GRID) {
			const vec3_t *bounds = ( const vec3_t * )( ( const byte * )surf->data + 12 );
			if ( mins[0] > bounds[1][0] || mins[1] > bounds[1][1] || mins[2] > bounds[1][2]
				|| maxs[0] < bounds[0][0] || maxs[1] < bounds[0][1] || maxs[2] < bounds[0][2] ) {
				s_markRejAABB++;
				surf->viewCount = tr.viewCount;
			}
		}
		else {
			s_markRejType++;
			surf->viewCount = tr.viewCount;
		}
		if (surf->viewCount != tr.viewCount) {
			surf->viewCount = tr.viewCount;
			list[*listlength] = (surfaceType_t *) surf->data;
			(*listlength)++;
			s_markAdded++;
		}
		mark++;
	}
}

void R_AddMarkFragments(int numClipPoints, vec3_t clipPoints[2][MAX_VERTS_ON_POLY],
				   int numPlanes, const vec3_t *normals, const float *dists,
				   int maxPoints, vec3_t pointBuffer,
				   markFragment_t *fragmentBuffer,
				   int *returnedPoints, int *returnedFragments) {
	int pingPong, i;
	markFragment_t	*mf;

	pingPong = 0;
	s_markAddCalls++;

	for ( i = 0 ; i < numPlanes ; i++ ) {

		R_ChopPolyBehindPlane( numClipPoints, clipPoints[pingPong],
						   &numClipPoints, clipPoints[!pingPong],
							normals[i], dists[i], 0.5 );
		pingPong ^= 1;
		if ( numClipPoints == 0 ) {
			break;
		}
	}
	if ( numClipPoints == 0 ) {
		s_markAddChopEmpty++;
		return;
	}

	if ( numClipPoints + (*returnedPoints) > maxPoints ) {
		s_markAddNoSpace++;
		return;	
	}

	mf = fragmentBuffer + (*returnedFragments);
	mf->firstPoint = (*returnedPoints);
	mf->numPoints = numClipPoints;
	mf->iIndex = 0;
	Com_Memcpy( pointBuffer + (*returnedPoints) * 3, clipPoints[pingPong], numClipPoints * sizeof(vec3_t) );

	(*returnedPoints) += numClipPoints;
	(*returnedFragments)++;
	s_markAddOk++;
}

void R_TessellateMarkFragments(
	int* pReturnedPoints,
	int* pReturnedFragments,
	int numsurfaces,
	surfaceType_t** surfaces,
	const vec3_t projectionDir,
	int numPlanes,
    const vec3_t* normals,
    float* dists,
	int maxPoints,
	vec3_t pointBuffer,
	int maxFragments,
	markFragment_t* fragmentBuffer
)
{
	int i, j, k, m, n;
	int* indexes;
	float* v;
	vec3_t clipPoints[2][MAX_VERTS_ON_POLY];
	vec3_t v1, v2;
	vec3_t normal;
	drawVert_t* dv;
	srfGridMesh_t* cv;
	srfSurfaceFace_t* surf;
	int numClipPoints;

	for (i = 0; i < numsurfaces; i++) {
		if (*surfaces[i] == SF_GRID) {
			cv = (srfGridMesh_t*)surfaces[i];

			for (m = 0; m < cv->height - 1; m++) {
				for (n = 0; n < cv->width - 1; n++) {

                    numClipPoints = 3;

                    dv = cv->verts + m * cv->width + n;

                    VectorCopy(dv[0].xyz, clipPoints[0][0]);
                    VectorMA(clipPoints[0][0], MARKER_OFFSET, dv[0].normal, clipPoints[0][0]);
                    VectorCopy(dv[cv->width].xyz, clipPoints[0][1]);
                    VectorMA(clipPoints[0][1], MARKER_OFFSET, dv[cv->width].normal, clipPoints[0][1]);
                    VectorCopy(dv[1].xyz, clipPoints[0][2]);
                    VectorMA(clipPoints[0][2], MARKER_OFFSET, dv[1].normal, clipPoints[0][2]);
					VectorSubtract(clipPoints[0][0], clipPoints[0][1], v1);
					VectorSubtract(clipPoints[0][2], clipPoints[0][1], v2);
					CrossProduct(v1, v2, normal);
					VectorNormalize(normal);
                    s_markGridTri++;
                    if (DotProduct(normal, projectionDir) < -0.1) {
                        s_markGridAcc++;
                        R_AddMarkFragments(numClipPoints, clipPoints,
                            numPlanes, normals, dists,
                            maxPoints, pointBuffer,
                            fragmentBuffer,
                            pReturnedPoints, pReturnedFragments);

                        if (*pReturnedFragments == maxFragments) {
                            return;	
                        }
                    }

					VectorCopy(dv[1].xyz, clipPoints[0][0]);
					VectorMA(clipPoints[0][0], MARKER_OFFSET, dv[1].normal, clipPoints[0][0]);
					VectorCopy(dv[cv->width].xyz, clipPoints[0][1]);
					VectorMA(clipPoints[0][1], MARKER_OFFSET, dv[cv->width].normal, clipPoints[0][1]);
					VectorCopy(dv[cv->width+1].xyz, clipPoints[0][2]);
					VectorMA(clipPoints[0][2], MARKER_OFFSET, dv[cv->width+1].normal, clipPoints[0][2]);
                    VectorSubtract(clipPoints[0][0], clipPoints[0][1], v1);
                    VectorSubtract(clipPoints[0][2], clipPoints[0][1], v2);
                    CrossProduct(v1, v2, normal);
                    VectorNormalize(normal);
                    s_markGridTri++;
                    if (DotProduct(normal, projectionDir) < -0.05) {
                        s_markGridAcc++;
                        R_AddMarkFragments(numClipPoints, clipPoints,
                            numPlanes, normals, dists,
                            maxPoints, pointBuffer,
                            fragmentBuffer,
                            pReturnedPoints, pReturnedFragments);

                        if (*pReturnedFragments == maxFragments) {
                            return;	
                        }
                    }
				}
			}
		} else if (*surfaces[i] == SF_TERRAIN) {
            // normal (retail never reads the terrain vertex normal here), and the single
            {
                srfTriangles_t *trs = (srfTriangles_t *)surfaces[i];
                drawVert_t     *dv0, *dv1, *dv2;

                indexes = trs->indexes;
                for (k = 0; k < trs->numIndexes; k += 3) {
                    dv0 = &trs->verts[indexes[k + 0]];
                    dv1 = &trs->verts[indexes[k + 1]];
                    dv2 = &trs->verts[indexes[k + 2]];

                    VectorSubtract(dv0->xyz, dv1->xyz, v1);
                    VectorSubtract(dv0->xyz, dv2->xyz, v2);
                    CrossProduct(v2, v1, normal);
                    VectorNormalize(normal);

                    s_markTerrTri++;
                    if (DotProduct(normal, projectionDir) <= -0.5) {
                        s_markTerrAcc++;
                        numClipPoints = 3;
                        VectorMA(dv0->xyz, MARKER_OFFSET, normal, clipPoints[0][0]);
                        VectorMA(dv1->xyz, MARKER_OFFSET, normal, clipPoints[0][1]);
                        VectorMA(dv2->xyz, MARKER_OFFSET, normal, clipPoints[0][2]);

                        R_AddMarkFragments(numClipPoints, clipPoints,
                            numPlanes, normals, dists,
                            maxPoints, pointBuffer,
                            fragmentBuffer,
                            pReturnedPoints, pReturnedFragments);
                        if (*pReturnedFragments == maxFragments) {
                            return;	
                        }
                    }
                }
            }
		} else if (*surfaces[i] == SF_FACE) {
            surf = (srfSurfaceFace_t*)surfaces[i];
            if (DotProduct(surf->plane.normal, projectionDir) > -0.5) {
                s_markFaceRejNormal++;
                continue;
            }

            indexes = (int*)((byte*)surf + surf->ofsIndices);

            for (k = 0; k < surf->numIndices; k += 3) {
                s_markFaceTri++;
                s_markFaceAcc++;
                for (j = 0; j < 3; j++) {
                    v = surf->points[indexes[k + j]];
                    VectorMA(v, MARKER_OFFSET, surf->plane.normal, clipPoints[0][j]);
                }

                R_AddMarkFragments(3, clipPoints,
                    numPlanes, normals, dists,
                    maxPoints, pointBuffer,
                    fragmentBuffer,
                    pReturnedPoints, pReturnedFragments);
                if (*pReturnedFragments == maxFragments) {
                    return;	
                }
            }
		}
	}
}

int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, float fPushDist ) {
    int numsurfaces;
	int i;
    surfaceType_t* surfaces[MAX_MARK_SURFACES];
    vec3_t mins, maxs;
	int returnedFragments;
	int returnedPoints;
	vec3_t normals[MAX_VERTS_ON_POLY + 2];
	float dists[MAX_VERTS_ON_POLY + 2];
	vec3_t projectionDir;
	vec3_t v1, v2;

	tr.viewCount++;
	UIVK_MARK_RESET();

	VectorNormalize2( projection, projectionDir );
	ClearBounds( mins, maxs );
	for ( i = 0 ; i < numPoints ; i++ ) {
		vec3_t	temp;

		AddPointToBounds( points[i], mins, maxs );
		VectorAdd( points[i], projection, temp );
		AddPointToBounds( temp, mins, maxs );
		VectorMA( points[i], -20, projectionDir, temp );
		AddPointToBounds( temp, mins, maxs );
	}

	if (numPoints > MAX_VERTS_ON_POLY) numPoints = MAX_VERTS_ON_POLY;
	for ( i = 0 ; i < numPoints ; i++ ) {
		VectorSubtract(points[(i+1)%numPoints], points[i], v1);
		VectorAdd(points[i], projection, v2);
		VectorSubtract(points[i], v2, v2);
		CrossProduct(v1, v2, normals[i]);
		VectorNormalize(normals[i]);
		dists[i] = DotProduct(normals[i], points[i]);
	}
	VectorCopy(projectionDir, normals[numPoints]);
	dists[numPoints] = DotProduct(normals[numPoints], points[0]) - fPushDist;
	VectorCopy(projectionDir, normals[numPoints+1]);
	VectorInverse(normals[numPoints+1]);
	dists[numPoints+1] = DotProduct(normals[numPoints+1], points[0]) - fPushDist;

	returnedPoints = 0;
	returnedFragments = 0;
	numsurfaces = 0;

	R_BoxSurfaces_r(tr.world->nodes, mins, maxs, surfaces, MAX_MARK_SURFACES, &numsurfaces, projectionDir);

	if (!numsurfaces) {
		if (PROBE_On()) {
			UIVK_PROBE(
				"MARK:bail",
				"%d EMPTY-LIST np=%d proj=(%.2f,%.2f,%.2f) pdir=(%.3f,%.3f,%.3f)"
				" push=%.2f mins=(%.1f,%.1f,%.1f) maxs=(%.1f,%.1f,%.1f)"
				" vis=%d add=%d listfull=%d rejflag=%d rejplane=%d rejaabb=%d rejtype=%d\n",
				s_markBailCount++,
				numPoints,
				projection[0], projection[1], projection[2],
				projectionDir[0], projectionDir[1], projectionDir[2],
				fPushDist,
				mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
				s_markVisited, s_markAdded, s_markListFull,
				s_markRejFlags, s_markRejPlane, s_markRejAABB, s_markRejType
			);
		}
		return 0;
	}

    R_TessellateMarkFragments(
        &returnedPoints,
        &returnedFragments,
        numsurfaces,
        surfaces,
        projectionDir,
        numPoints + 2,
        normals,
        dists,
        maxPoints,
        pointBuffer,
        maxFragments,
        fragmentBuffer
	);

	if (PROBE_On()) {
		UIVK_PROBE(
			"MARK:bail",
			"%d np=%d proj=(%.2f,%.2f,%.2f) pdir=(%.3f,%.3f,%.3f) push=%.2f"
			" vis=%d add=%d listfull=%d rejflag=%d rejplane=%d rejaabb=%d rejtype=%d"
			" grid=%d/%d terr=%d/%d face=%d/%d facenorm=%d"
			" addcalls=%d clipempty=%d clipover=%d nospace=%d ok=%d"
			" nsurf=%d nfrag=%d npts=%d maxp=%d maxf=%d\n",
			s_markBailCount++,
			numPoints,
			projection[0], projection[1], projection[2],
			projectionDir[0], projectionDir[1], projectionDir[2],
			fPushDist,
			s_markVisited, s_markAdded, s_markListFull,
			s_markRejFlags, s_markRejPlane, s_markRejAABB, s_markRejType,
			s_markGridAcc, s_markGridTri, s_markTerrAcc, s_markTerrTri,
			s_markFaceAcc, s_markFaceTri, s_markFaceRejNormal,
			s_markAddCalls, s_markAddChopEmpty, s_markAddChopOverflow,
			s_markAddNoSpace, s_markAddOk,
			numsurfaces, returnedFragments, returnedPoints, maxPoints, maxFragments
		);
	}

    return returnedFragments;
}

void R_BoxSurfacesForBModel_r(bmodel_t* pBmodel, const vec3_t mins, const vec3_t maxs, surfaceType_t** list, int listsize, int* listlength, const vec3_t dir)
{
	int s;
	int c;
	msurface_t* surf;
	int i;

	surf = pBmodel->firstSurface;

	for (i = 0; i < listsize && i < pBmodel->numSurfaces; i++, surf++) {
		s = surf->shader->surfaceFlags;
		c = surf->shader->contentFlags;

		if (!(s & SURF_NOIMPACT)
			&& !(s & SURF_NOMARKS)
			&& !(c & CONTENTS_FOG))
		{
			switch (*surf->data)
			{
			case SF_FACE:
            {
                int plane;
                srfSurfaceFace_t* face;

				face = (srfSurfaceFace_t*)surf->data;
				plane = BoxOnPlaneSide(mins, maxs, &face->plane);
				if (plane == PLANE_Y || plane == PLANE_Z) {
					surf->viewCount = tr.viewCount;
				}
				else if (DotProduct(dir, face->plane.normal) > 0.5f) {
					surf->viewCount = tr.viewCount;
				}
				break;
			}
            case SF_GRID:
                surf->viewCount = tr.viewCount;
				break;
			default:
				break;
			}
		}
		else
		{
			surf->viewCount = tr.viewCount;
		}

		if (surf->viewCount != tr.viewCount)
		{
			surf->viewCount = tr.viewCount;
			list[(*listlength)++] = surf->data;
		}
	}
}

int R_MarkFragmentsForInlineModel(clipHandle_t bmodel, const vec3_t vAngles, const vec3_t vOrigin, int numPoints,
    const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer,
    int maxFragments, markFragment_t* fragmentBuffer)
{
	int i;
	int numsurfaces;
	vec3_t vTmp;
	vec3_t v1, v2;
	vec3_t vMins, vMaxs;
	vec3_t vTransPoints[64];
	vec3_t vTransProj;
	vec3_t vTransProjDir;
	bmodel_t* pBmodel;
	surfaceType_t* surfaces[64];
	int returnedFragments;
	int returnedPoints;
	vec3_t normals[MAX_VERTS_ON_POLY+2];
	float dists[MAX_VERTS_ON_POLY+2];

    tr.viewCount++;

    pBmodel = &tr.world->bmodels[bmodel];

	if (numPoints > 64) {
		numPoints = 64;
	}

	if (vAngles[0] || vAngles[1] || vAngles[2])
	{
		vec3_t axis[3];

		AngleVectorsLeft(vAngles, axis[0], axis[1], axis[2]);
	
		for (i = 0; i < numPoints; i++) {
			VectorSubtract(points[i], vOrigin, vTmp);
			MatrixTransformVectorRight(axis, vTmp, vTransPoints[i]);
		}

		MatrixTransformVectorRight(axis, projection, vTransProj);
	}
	else
	{
        for (i = 0; i < numPoints; i++) {
            VectorSubtract(points[i], vOrigin, vTransPoints[i]);
		}

		VectorCopy(projection, vTransProj);
	}

    VectorNormalize2(vTransProj, vTransProjDir);
    ClearBounds(vMins, vMaxs);

	for (i = 0; i < numPoints; i++) {
		AddPointToBounds(vTransPoints[i], vMins, vMaxs);
		VectorAdd(vTransPoints[i], vTransProj, vTmp);
		AddPointToBounds(vTmp, vMins, vMaxs);
		VectorMA(vTransPoints[i], -20.f, vTransProjDir, vTmp);
		AddPointToBounds(vTmp, vMins, vMaxs);
	}

	for (i = 0; i < numPoints; i++) {
		VectorSubtract(vTransPoints[(i + 1) % numPoints], vTransPoints[i], v1);
		VectorAdd(vTransPoints[i], vTransProj, v2);
		VectorSubtract(vTransPoints[i], v2, v2);
		CrossProduct(v1, v2, normals[i]);
		VectorNormalize(normals[i]);
		dists[i] = DotProduct(normals[i], vTransPoints[i]);
	}

	VectorCopy(vTransProjDir, normals[numPoints]);
	dists[numPoints] = DotProduct(normals[numPoints], vTransPoints[0]) - 32.f;

	VectorCopy(vTransProjDir, normals[numPoints + 1]);
	VectorInverse(normals[numPoints + 1]);
	dists[numPoints + 1] = DotProduct(normals[numPoints + 1], vTransPoints[0]) - 20.f;

	returnedPoints = 0;
	returnedFragments = 0;
	numsurfaces = 0;

	R_BoxSurfacesForBModel_r(pBmodel, vMins, vMaxs, surfaces, ARRAY_LEN(surfaces), &numsurfaces, vTransProjDir);
	if (!numsurfaces) {
		return 0;
	}

    R_TessellateMarkFragments(
        &returnedPoints,
        &returnedFragments,
        numsurfaces,
        surfaces,
        vTransProjDir,
        numPoints + 2,
        normals,
        dists,
        maxPoints,
        pointBuffer,
        maxFragments,
        fragmentBuffer
	);

    return returnedFragments;
}