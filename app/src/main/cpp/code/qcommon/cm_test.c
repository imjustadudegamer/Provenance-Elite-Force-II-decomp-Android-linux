#include "cm_local.h"


int CM_PointLeafnum_r( const vec3_t p, int num ) {
	float		d;
	cNode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = cm.nodes + num;
		plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;		

	return -1 - num;
}

int CM_PointLeafnum( const vec3_t p ) {
	if ( !cm.numNodes ) {	
		return 0;
	}
	return CM_PointLeafnum_r (p, 0);
}




void CM_StoreLeafs( leafList_t *ll, int nodenum ) {
	int		leafNum;

	leafNum = -1 - nodenum;

	if ( cm.leafs[ leafNum ].cluster != -1 ) {
		ll->lastLeaf = leafNum;
	}

	if ( ll->count >= ll->maxcount) {
		ll->overflowed = qtrue;
		return;
	}
	ll->list[ ll->count++ ] = leafNum;
}

void CM_StoreBrushes( leafList_t *ll, int nodenum ) {
	int			i, k;
	int			leafnum;
	int			brushnum;
	cLeaf_t		*leaf;
	cbrush_t	*b;

	leafnum = -1 - nodenum;

	leaf = &cm.leafs[leafnum];

	for ( k = 0 ; k < leaf->numLeafBrushes ; k++ ) {
		brushnum = cm.leafbrushes[leaf->firstLeafBrush+k];
		b = &cm.brushes[brushnum];
		if ( b->checkcount == cm.checkcount ) {
			continue;	
		}
		b->checkcount = cm.checkcount;
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( b->bounds[0][i] >= ll->bounds[1][i] || b->bounds[1][i] <= ll->bounds[0][i] ) {
				break;
			}
		}
		if ( i != 3 ) {
			continue;
		}
		if ( ll->count >= ll->maxcount) {
			ll->overflowed = qtrue;
			return;
		}
		((cbrush_t **)ll->list)[ ll->count++ ] = b;
	}
#if 0
	for ( k = 0 ; k < leaf->numLeafSurfaces ; k++ ) {
		patch = cm.surfaces[ cm.leafsurfaces[ leaf->firstleafsurface + k ] ];
		if ( !patch ) {
			continue;
		}
	}
#endif
}

void CM_BoxLeafnums_r( leafList_t *ll, int nodenum ) {
	cplane_t	*plane;
	cNode_t		*node;
	int			s;

	while (1) {
		if (nodenum < 0) {
			ll->storeLeafs( ll, nodenum );
			return;
		}
	
		node = &cm.nodes[nodenum];
		plane = node->plane;
		s = BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], plane );
		if (s == 1) {
			nodenum = node->children[0];
		} else if (s == 2) {
			nodenum = node->children[1];
		} else {
			CM_BoxLeafnums_r( ll, node->children[0] );
			nodenum = node->children[1];
		}
	}
}

int	CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastLeaf) {
	leafList_t	ll;

	cm.checkcount++;

	VectorCopy( mins, ll.bounds[0] );
	VectorCopy( maxs, ll.bounds[1] );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.storeLeafs = CM_StoreLeafs;
	ll.lastLeaf = 0;
	ll.overflowed = qfalse;

	CM_BoxLeafnums_r( &ll, 0 );

	*lastLeaf = ll.lastLeaf;
	return ll.count;
}

int CM_BoxBrushes( const vec3_t mins, const vec3_t maxs, cbrush_t **list, int listsize ) {
	leafList_t	ll;

	cm.checkcount++;

	VectorCopy( mins, ll.bounds[0] );
	VectorCopy( maxs, ll.bounds[1] );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = (void *)list;
	ll.storeLeafs = CM_StoreBrushes;
	ll.lastLeaf = 0;
	ll.overflowed = qfalse;
	
	CM_BoxLeafnums_r( &ll, 0 );

	return ll.count;
}

dshader_t *CM_ShaderPointer( int iShaderNum )
{
	return ( dshader_t * )&cm.shaders[ iShaderNum ];
}




int CM_PointContents( const vec3_t p, clipHandle_t model ) {
	int			leafnum;
	int			i, k;
	int			brushnum;
	cLeaf_t		*leaf;
	cbrush_t	*b;
	int			contents;
	float		d;
	cmodel_t	*clipm;

	if (!cm.numNodes) {	
		return 0;
	}

	if ( model ) {
		clipm = CM_ClipHandleToModel( model );
		leaf = &clipm->leaf;
	} else {
		leafnum = CM_PointLeafnum_r (p, 0);
		leaf = &cm.leafs[leafnum];
	}

	contents = 0;
	for (k=0 ; k<leaf->numLeafBrushes ; k++) {
		brushnum = cm.leafbrushes[leaf->firstLeafBrush+k];
		b = &cm.brushes[brushnum];

		if ( !CM_BoundsIntersectPoint( b->bounds[0], b->bounds[1], p ) ) {
			continue;
		}

		for ( i = 0 ; i < b->numsides ; i++ ) {
			d = DotProduct( p, b->sides[i].plane->normal );
			if ( d > b->sides[i].plane->dist ) {
				break;
			}
		}

		if ( i == b->numsides ) {
			contents |= b->contents;
		}
	}

	return contents;
}

int CM_PointBrushNum( const vec3_t p, clipHandle_t model ) {
	int i, k;
	int brushnum;
	cLeaf_t *leaf;
	cbrush_t *b;
	cmodel_t *clipm;

	if( !cm.numNodes ) {
		return 0;
	}

	if( model ) {
		clipm = CM_ClipHandleToModel( model );
		leaf = &clipm->leaf;
	} else {
		leaf = &cm.leafs[ CM_PointLeafnum_r( p, 0 ) ];
	}

	for( k = 0; k<leaf->numLeafBrushes; k++ ) {
		brushnum = cm.leafbrushes[ leaf->firstLeafBrush + k ];
		b = &cm.brushes[ brushnum ];

		for( i = 0; i < b->numsides; i++ ) {
			if( DotProduct( p, b->sides[ i ].plane->normal ) > b->sides[ i ].plane->dist ) {
				return brushnum;
			}
		}

		if( i == b->numsides ) {
			return brushnum;
		}
	}

	return -1;
}

int	CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles) {
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		forward, right, up;

	VectorSubtract (p, origin, p_l);

	if ( model != BOX_MODEL_HANDLE && 
	(angles[0] || angles[1] || angles[2]) )
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	return CM_PointContents( p_l, model );
}




byte	*CM_ClusterPVS (int cluster) {
	if (cluster < 0 || cluster >= cm.numClusters || !cm.vised ) {
		return cm.visibility;
	}

	return cm.visibility + cluster * cm.clusterBytes;
}




void CM_FloodArea_r( int areaNum, int floodnum) {
	int		i;
	cArea_t *area;
	int		*con;

	area = &cm.areas[ areaNum ];

	if ( area->floodvalid == cm.floodvalid ) {
		if (area->floodnum == floodnum)
			return;
		Com_Error (ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = cm.floodvalid;
	con = cm.areaPortals + areaNum * cm.numAreas;
	for ( i=0 ; i < cm.numAreas  ; i++ ) {
		if ( con[i] > 0 ) {
			CM_FloodArea_r( i, floodnum );
		}
	}
}

void	CM_FloodAreaConnections( void ) {
	int		i;
	cArea_t	*area;
	int		floodnum;

	cm.floodvalid++;
	floodnum = 0;

	for (i = 0 ; i < cm.numAreas ; i++) {
		area = &cm.areas[i];
		if (area->floodvalid == cm.floodvalid) {
			continue;		
		}
		floodnum++;
		CM_FloodArea_r (i, floodnum);
	}

}

void	CM_AdjustAreaPortalState( int area1, int area2, qboolean open ) {
	if ( area1 < 0 || area2 < 0 ) {
		return;
	}

	if ( area1 >= cm.numAreas || area2 >= cm.numAreas ) {
		Com_Error (ERR_DROP, "CM_ChangeAreaPortalState: bad area number");
	}

	if ( open ) {
		cm.areaPortals[ area1 * cm.numAreas + area2 ]++;
		cm.areaPortals[ area2 * cm.numAreas + area1 ]++;
	} else {
		cm.areaPortals[ area1 * cm.numAreas + area2 ]--;
		cm.areaPortals[ area2 * cm.numAreas + area1 ]--;
		if ( cm.areaPortals[ area2 * cm.numAreas + area1 ] < 0 ) {
			Com_DPrintf("CM_AdjustAreaPortalState: negative reference count\n");
		}
	}

	CM_FloodAreaConnections ();
}

void	CM_ResetAreaPortals( void )
{
	memset( cm.areaPortals, 0, cm.numAreas * cm.numAreas * sizeof( *cm.areaPortals ) );
	CM_FloodAreaConnections();
}

void	CM_WritePortalState( fileHandle_t f )
{
	FS_Write( &cm.numAreas, sizeof( int ), f );
	FS_Write( cm.areaPortals, cm.numAreas * cm.numAreas * sizeof( *cm.areaPortals ), f );
}

void	CM_ReadPortalState( fileHandle_t f )
{
	int numareaportals;
	FS_Read(&numareaportals, sizeof(int), f);
	if (numareaportals != cm.numAreas) {
		Com_Error(ERR_DROP, "numareaportals differs from save game");
	}
	FS_Read(cm.areaPortals, cm.numAreas * cm.numAreas * sizeof(*cm.areaPortals), f);
	CM_FloodAreaConnections();
}

int		CM_AreaForPoint( vec3_t vPos ) {
	int leafnum = CM_PointLeafnum( vPos );
	return CM_LeafArea( leafnum );
}

qboolean	CM_AreasConnected( int area1, int area2 ) {
#ifndef BSPC
	if ( cm_noAreas->integer ) {
		return qtrue;
	}
#endif

	if ( area1 < 0 || area2 < 0 ) {
		return qfalse;
	}

	if (area1 >= cm.numAreas || area2 >= cm.numAreas) {
		Com_Error (ERR_DROP, "area >= cm.numAreas");
	}

	if (cm.areas[area1].floodnum == cm.areas[area2].floodnum) {
		return qtrue;
	}
	return qfalse;
}


int CM_WriteAreaBits (byte *buffer, int area)
{
	int		i;
	int		floodnum;
	int		bytes;

	bytes = (cm.numAreas+7)>>3;

#ifndef BSPC
	if (cm_noAreas->integer || area == -1)
#else
	if ( area == -1)
#endif
	{	
		Com_Memset (buffer, 255, bytes);
	}
	else
	{
		floodnum = cm.areas[area].floodnum;
		for (i=0 ; i<cm.numAreas ; i++)
		{
			if (cm.areas[i].floodnum == floodnum || area == -1)
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}

qboolean CM_inPVS( vec3_t p1, vec3_t p2 ) {
	int		leaf1;
	int		leaf2;

	leaf1 = CM_PointLeafnum( p1 );
	leaf2 = CM_PointLeafnum( p2 );

	return CM_LeafInPVS( leaf1, leaf2 );
}

qboolean CM_LeafInPVS( int leaf1, int leaf2 ) {
	int		cluster;
	byte	*mask;

	mask = CM_ClusterPVS( cm.leafs[ leaf1 ].cluster );
	cluster = cm.leafs[ leaf2 ].cluster;

	if( !mask || mask[ cluster >> 3 ] & ( 1 << ( cluster & 7 ) ) )
	{
		if( CM_AreasConnected( cm.leafs[ leaf1 ].area, cm.leafs[ leaf2 ].area ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 )
{
	if (maxs[0] < mins2[0] - SURFACE_CLIP_EPSILON ||
		maxs[1] < mins2[1] - SURFACE_CLIP_EPSILON ||
		maxs[2] < mins2[2] - SURFACE_CLIP_EPSILON ||
		mins[0] > maxs2[0] + SURFACE_CLIP_EPSILON ||
		mins[1] > maxs2[1] + SURFACE_CLIP_EPSILON ||
		mins[2] > maxs2[2] + SURFACE_CLIP_EPSILON)
	{
		return qfalse;
	}

	return qtrue;
}

qboolean CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point )
{
	if (maxs[0] < point[0] - SURFACE_CLIP_EPSILON ||
		maxs[1] < point[1] - SURFACE_CLIP_EPSILON ||
		maxs[2] < point[2] - SURFACE_CLIP_EPSILON ||
		mins[0] > point[0] + SURFACE_CLIP_EPSILON ||
		mins[1] > point[1] + SURFACE_CLIP_EPSILON ||
		mins[2] > point[2] + SURFACE_CLIP_EPSILON)
	{
		return qfalse;
	}

	return qtrue;
}
