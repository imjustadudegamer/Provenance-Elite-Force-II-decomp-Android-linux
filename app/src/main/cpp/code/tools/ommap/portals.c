
#include "qbsp.h"


int		c_active_portals;
int		c_peak_portals;
int		c_boundary;
int		c_boundary_sides;

portal_t *AllocPortal (void)
{
	portal_t	*p;
	
	if (numthreads == 1)
		c_active_portals++;
	if (c_active_portals > c_peak_portals)
		c_peak_portals = c_active_portals;
	
	p = malloc (sizeof(portal_t));
	memset (p, 0, sizeof(portal_t));
	
	return p;
}

void FreePortal (portal_t *p)
{
	if (p->winding)
		FreeWinding (p->winding);
	if (numthreads == 1)
		c_active_portals--;
	free (p);
}


qboolean Portal_Passable(portal_t *p) {
	if (!p->onnode) {
		return qfalse;	
	}

	if (p->nodes[0]->planenum != PLANENUM_LEAF
		|| p->nodes[1]->planenum != PLANENUM_LEAF) {
		Error ("Portal_EntityFlood: not a leaf");
	}

	if ( !p->nodes[0]->opaque && !p->nodes[1]->opaque ) {
		return qtrue;
	}

	return qfalse;
}



int		c_tinyportals;

void AddPortalToNodes (portal_t *p, node_t *front, node_t *back)
{
	if (p->nodes[0] || p->nodes[1])
		Error ("AddPortalToNode: allready included");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;
	
	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}


void RemovePortalFromNode (portal_t *portal, node_t *l)
{
	portal_t	**pp, *t;
	
	pp = &l->portals;
	while (1)
	{
		t = *pp;
		if (!t)
			Error ("RemovePortalFromNode: portal not in leaf");	

		if ( t == portal )
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Error ("RemovePortalFromNode: portal not bounding leaf");
	}
	
	if (portal->nodes[0] == l)
	{
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if (portal->nodes[1] == l)
	{
		*pp = portal->next[1];	
		portal->nodes[1] = NULL;
	}
}


void PrintPortal (portal_t *p)
{
	int			i;
	winding_t	*w;
	
	w = p->winding;
	for (i=0 ; i<w->numpoints ; i++)
		_printf ("(%5.0f,%5.0f,%5.0f)\n",w->p[i][0]
		, w->p[i][1], w->p[i][2]);
}

#define	SIDESPACE	8
void MakeHeadnodePortals (tree_t *tree)
{
	vec3_t		bounds[2];
	int			i, j, n;
	portal_t	*p, *portals[6];
	plane_t		bplanes[6], *pl;
	node_t *node;

	node = tree->headnode;

	for (i=0 ; i<3 ; i++)
	{
		bounds[0][i] = tree->mins[i] - SIDESPACE;
		bounds[1][i] = tree->maxs[i] + SIDESPACE;
		if ( bounds[0][i] >= bounds[1][i] ) {
			Error( "Backwards tree volume" );
		}
	}
	
	tree->outside_node.planenum = PLANENUM_LEAF;
	tree->outside_node.brushlist = NULL;
	tree->outside_node.portals = NULL;
	tree->outside_node.opaque = qfalse;

	for (i=0 ; i<3 ; i++)
		for (j=0 ; j<2 ; j++)
		{
			n = j*3 + i;

			p = AllocPortal ();
			portals[n] = p;
			
			pl = &bplanes[n];
			memset (pl, 0, sizeof(*pl));
			if (j)
			{
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			}
			else
			{
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->plane = *pl;
			p->winding = BaseWindingForPlane (pl->normal, pl->dist);
			AddPortalToNodes (p, node, &tree->outside_node);
		}
		
	for (i=0 ; i<6 ; i++)
	{
		for (j=0 ; j<6 ; j++)
		{
			if (j == i)
				continue;
			ChopWindingInPlace (&portals[i]->winding, bplanes[j].normal, bplanes[j].dist, ON_EPSILON);
		}
	}
}



#define	BASE_WINDING_EPSILON	0.001
#define	SPLIT_WINDING_EPSILON	0.001

winding_t	*BaseWindingForNode (node_t *node)
{
	winding_t	*w;
	node_t		*n;
	plane_t		*plane;
	vec3_t		normal;
	vec_t		dist;

	w = BaseWindingForPlane (mapplanes[node->planenum].normal
		, mapplanes[node->planenum].dist);

	for (n=node->parent ; n && w ; )
	{
		plane = &mapplanes[n->planenum];

		if (n->children[0] == node)
		{	
			ChopWindingInPlace (&w, plane->normal, plane->dist, BASE_WINDING_EPSILON);
		}
		else
		{	
			VectorSubtract (vec3_origin, plane->normal, normal);
			dist = -plane->dist;
			ChopWindingInPlace (&w, normal, dist, BASE_WINDING_EPSILON);
		}
		node = n;
		n = n->parent;
	}

	return w;
}


void MakeNodePortal (node_t *node)
{
	portal_t	*new_portal, *p;
	winding_t	*w;
	vec3_t		normal;
	float		dist;
	int			side;

	w = BaseWindingForNode (node);

	for (p = node->portals ; p && w; p = p->next[side])	
	{
		if (p->nodes[0] == node)
		{
			side = 0;
			VectorCopy (p->plane.normal, normal);
			dist = p->plane.dist;
		}
		else if (p->nodes[1] == node)
		{
			side = 1;
			VectorSubtract (vec3_origin, p->plane.normal, normal);
			dist = -p->plane.dist;
		}
		else
			Error ("CutNodePortals_r: mislinked portal");

		ChopWindingInPlace (&w, normal, dist, CLIP_EPSILON);
	}

	if (!w)
	{
		return;
	}

	if (WindingIsTiny (w))
	{
		c_tinyportals++;
		FreeWinding (w);
		return;
	}

	new_portal = AllocPortal ();
	new_portal->plane = mapplanes[node->planenum];
	new_portal->onnode = node;
	new_portal->winding = w;
	new_portal->hint = node->hint;
	AddPortalToNodes (new_portal, node->children[0], node->children[1]);
}


void SplitNodePortals (node_t *node)
{
	portal_t	*p, *next_portal, *new_portal;
	node_t		*f, *b, *other_node;
	int			side;
	plane_t		*plane;
	winding_t	*frontwinding, *backwinding;

	plane = &mapplanes[node->planenum];
	f = node->children[0];
	b = node->children[1];

	for (p = node->portals ; p ; p = next_portal)	
	{
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Error ("SplitNodePortals: mislinked portal");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);

		ClipWindingEpsilon (p->winding, plane->normal, plane->dist,
			SPLIT_WINDING_EPSILON, &frontwinding, &backwinding);

		if (frontwinding && WindingIsTiny(frontwinding))
		{
			if (!f->tinyportals)
				VectorCopy(frontwinding->p[0], f->referencepoint);
			f->tinyportals++;
			if (!other_node->tinyportals)
				VectorCopy(frontwinding->p[0], other_node->referencepoint);
			other_node->tinyportals++;

			FreeWinding (frontwinding);
			frontwinding = NULL;
			c_tinyportals++;
		}

		if (backwinding && WindingIsTiny(backwinding))
		{
			if (!b->tinyportals)
				VectorCopy(backwinding->p[0], b->referencepoint);
			b->tinyportals++;
			if (!other_node->tinyportals)
				VectorCopy(backwinding->p[0], other_node->referencepoint);
			other_node->tinyportals++;

			FreeWinding (backwinding);
			backwinding = NULL;
			c_tinyportals++;
		}

		if (!frontwinding && !backwinding)
		{	
			continue;
		}

		if (!frontwinding)
		{
			FreeWinding (backwinding);
			if (side == 0)
				AddPortalToNodes (p, b, other_node);
			else
				AddPortalToNodes (p, other_node, b);
			continue;
		}
		if (!backwinding)
		{
			FreeWinding (frontwinding);
			if (side == 0)
				AddPortalToNodes (p, f, other_node);
			else
				AddPortalToNodes (p, other_node, f);
			continue;
		}
		
		new_portal = AllocPortal ();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding (p->winding);
		p->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes (p, f, other_node);
			AddPortalToNodes (new_portal, b, other_node);
		}
		else
		{
			AddPortalToNodes (p, other_node, f);
			AddPortalToNodes (new_portal, other_node, b);
		}
	}

	node->portals = NULL;
}


void CalcNodeBounds (node_t *node)
{
	portal_t	*p;
	int			s;
	int			i;

	ClearBounds (node->mins, node->maxs);
	for (p = node->portals ; p ; p = p->next[s])
	{
		s = (p->nodes[1] == node);
		for (i=0 ; i<p->winding->numpoints ; i++)
			AddPointToBounds (p->winding->p[i], node->mins, node->maxs);
	}
}

void MakeTreePortals_r (node_t *node)
{
	int		i;

	CalcNodeBounds (node);
	if (node->mins[0] >= node->maxs[0])
	{
		_printf ("WARNING: node without a volume\n");
		_printf("node has %d tiny portals\n", node->tinyportals);
		_printf("node reference point %1.2f %1.2f %1.2f\n", node->referencepoint[0],
															node->referencepoint[1],
															node->referencepoint[2]);
	}

	for (i=0 ; i<3 ; i++)
	{
		if (node->mins[i] < MIN_WORLD_COORD || node->maxs[i] > MAX_WORLD_COORD)
		{
			_printf ("WARNING: node with unbounded volume\n");
			break;
		}
	}
	if (node->planenum == PLANENUM_LEAF)
		return;

	MakeNodePortal (node);
	SplitNodePortals (node);

	MakeTreePortals_r (node->children[0]);
	MakeTreePortals_r (node->children[1]);
}

void MakeTreePortals (tree_t *tree)
{
	qprintf( "----- MakeTreePortals -----\n");
	MakeHeadnodePortals (tree);
	MakeTreePortals_r (tree->headnode);
	qprintf("%6d tiny portals\n", c_tinyportals);
}


int		c_floodedleafs;

void FloodPortals_r (node_t *node, int dist) {
	portal_t	*p;
	int			s;

	if ( node->occupied ) {
		return;
	}

	if ( node->opaque ) {
		return;
	}

	c_floodedleafs++;
	node->occupied = dist;

	for (p=node->portals ; p ; p = p->next[s]) {
		s = (p->nodes[1] == node);
		FloodPortals_r (p->nodes[!s], dist+1);
	}
}

qboolean PlaceOccupant (node_t *headnode, vec3_t origin, entity_t *occupant)
{
	node_t	*node;
	vec_t	d;
	plane_t	*plane;

	node = headnode;
	while (node->planenum != PLANENUM_LEAF)
	{
		plane = &mapplanes[node->planenum];
		d = DotProduct (origin, plane->normal) - plane->dist;
		if (d >= 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	if ( node->opaque )
		return qfalse;
	node->occupant = occupant;

	FloodPortals_r (node, 1);

	return qtrue;
}

qboolean FloodEntities( tree_t *tree ) {
	int		i;
	vec3_t	origin;
	const char	*cl;
	qboolean	inside;
	node_t *headnode;

	headnode = tree->headnode;
	qprintf ("--- FloodEntities ---\n");
	inside = qfalse;
	tree->outside_node.occupied = 0;

	c_floodedleafs = 0;
	for (i=1 ; i<num_entities ; i++)
	{
		GetVectorForKey (&entities[i], "origin", origin);
		if (VectorCompare(origin, vec3_origin))
			continue;

		cl = ValueForKey (&entities[i], "classname");

		origin[2] += 1;	

		if (PlaceOccupant (headnode, origin, &entities[i]))
			inside = qtrue;
	}

	qprintf("%5i flooded leafs\n", c_floodedleafs );

	if (!inside)
	{
		qprintf ("no entities in open -- no filling\n");
	}
	else if (tree->outside_node.occupied)
	{
		qprintf ("entity reached from outside -- no filling\n");
	}

	return (qboolean)(inside && !tree->outside_node.occupied);
}


int		c_areas;

void FloodAreas_r (node_t *node)
{
	portal_t	*p;
	int			s;
	bspbrush_t	*b;

	if ( node->areaportal ) {
		if ( node->area == -1 ) {
			node->area = c_areas;
		}
		b = node->brushlist->original;

		if (b->portalareas[0] == c_areas || b->portalareas[1] == c_areas)
			return;

		if (b->portalareas[1] != -1)
		{
			_printf ("WARNING: areaportal brush %i touches > 2 areas\n", b->brushnum );
			return;
		}
		if (b->portalareas[0] != -1) {
			b->portalareas[1] = c_areas;
		} else {
			b->portalareas[0] = c_areas;
		}

		return;
	}

	if (node->area != -1) {
		return;		
	}
	if ( node->cluster == -1 ) {
		return;
	}

	node->area = c_areas;

	for (p=node->portals ; p ; p = p->next[s])
	{
		s = (p->nodes[1] == node);

		if ( !Portal_Passable(p) )
			continue;

		FloodAreas_r (p->nodes[!s]);
	}
}


void FindAreas_r (node_t *node)
{
	if (node->planenum != PLANENUM_LEAF)
	{
		FindAreas_r (node->children[0]);
		FindAreas_r (node->children[1]);
		return;
	}

	if (node->opaque)
		return;

	if (node->areaportal)
		return;

	if (node->area != -1)
		return;		

	FloodAreas_r (node);
	c_areas++;
}

void CheckAreas_r (node_t *node)
{
	bspbrush_t	*b;

	if (node->planenum != PLANENUM_LEAF)
	{
		CheckAreas_r (node->children[0]);
		CheckAreas_r (node->children[1]);
		return;
	}

	if (node->opaque)
		return;

	if (node->cluster != -1)
		if (node->area == -1)
			_printf("WARNING: cluster %d has area set to -1\n", node->cluster);
	if (node->areaportal)
	{
		b = node->brushlist->original;

		if (b->portalareas[0] == -1 || b->portalareas[1] == -1)
			_printf ("WARNING: areaportal brush %i doesn't touch two areas\n", b->brushnum);
	}
}

void FloodAreas (tree_t *tree)
{
	qprintf ("--- FloodAreas ---\n");
	FindAreas_r( tree->headnode );

	CheckAreas_r( tree->headnode );

	qprintf ("%5i areas\n", c_areas);
}


int		c_outside;
int		c_inside;
int		c_solid;

void FillOutside_r (node_t *node)
{
	if (node->planenum != PLANENUM_LEAF)
	{
		FillOutside_r (node->children[0]);
		FillOutside_r (node->children[1]);
		return;
	}

	if (!node->occupied) {
		if ( !node->opaque ) {
			c_outside++;
			node->opaque = qtrue;
		} else {
			c_solid++;
		}
	} else {
		c_inside++;
	}

}

void FillOutside (node_t *headnode)
{
	c_outside = 0;
	c_inside = 0;
	c_solid = 0;
	qprintf ("--- FillOutside ---\n");
	FillOutside_r (headnode);
	qprintf ("%5i solid leafs\n", c_solid);
	qprintf ("%5i leafs filled\n", c_outside);
	qprintf ("%5i inside leafs\n", c_inside);
}



