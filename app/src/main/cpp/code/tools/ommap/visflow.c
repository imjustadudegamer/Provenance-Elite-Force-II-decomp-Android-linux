/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "vis.h"


int CountBits (byte *bits, int numbits)
{
	int		i;
	int		c;

	c = 0;
	for (i=0 ; i<numbits ; i++)
		if (bits[i>>3] & (1<<(i&7)) )
			c++;

	return c;
}

int		c_fullskip;
int		c_portalskip, c_leafskip;
int		c_vistest, c_mighttest;

int		c_chop, c_nochop;

int		active;

void CheckStack (leaf_t *leaf, threaddata_t *thread)
{
	pstack_t	*p, *p2;

	for (p=thread->pstack_head.next ; p ; p=p->next)
	{
		if (p->leaf == leaf)
			Error ("CheckStack: leaf recursion");
		for (p2=thread->pstack_head.next ; p2 != p ; p2=p2->next)
			if (p2->leaf == p->leaf)
				Error ("CheckStack: late leaf recursion");
	}
}


winding_t *AllocStackWinding (pstack_t *stack)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (stack->freewindings[i])
		{
			stack->freewindings[i] = 0;
			return &stack->windings[i];
		}
	}

	Error ("AllocStackWinding: failed");

	return NULL;
}

void FreeStackWinding (winding_t *w, pstack_t *stack)
{
	int		i;

	i = w - stack->windings;

	if (i<0 || i>2)
		return;		

	if (stack->freewindings[i])
		Error ("FreeStackWinding: allready free");
	stack->freewindings[i] = 1;
}

winding_t	*VisChopWinding (winding_t *in, pstack_t *stack, plane_t *split)
{
	vec_t	dists[128];
	int		sides[128];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*neww;

	counts[0] = counts[1] = counts[2] = 0;

	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}

	if (!counts[1])
		return in;		
	
	if (!counts[0])
	{
		FreeStackWinding (in, stack);
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	
	neww = AllocStackWinding (stack);

	neww->numpoints = 0;

	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->points[i];

		if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
		{
			FreeStackWinding (neww, stack);
			return in;		
		}

		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}
		
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
		if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
		{
			FreeStackWinding (neww, stack);
			return in;		
		}

		p2 = in->points[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}
	
	FreeStackWinding (in, stack);
	
	return neww;
}

winding_t	*ClipToSeperators (winding_t *source, winding_t *pass, winding_t *target, qboolean flipclip, pstack_t *stack)
{
	int			i, j, k, l;
	plane_t		plane;
	vec3_t		v1, v2;
	float		d;
	vec_t		length;
	int			counts[3];
	qboolean		fliptest;

	for (i=0 ; i<source->numpoints ; i++)
	{
		l = (i+1)%source->numpoints;
		VectorSubtract (source->points[l] , source->points[i], v1);

		for (j=0 ; j<pass->numpoints ; j++)
		{
			VectorSubtract (pass->points[j], source->points[i], v2);

			plane.normal[0] = v1[1]*v2[2] - v1[2]*v2[1];
			plane.normal[1] = v1[2]*v2[0] - v1[0]*v2[2];
			plane.normal[2] = v1[0]*v2[1] - v1[1]*v2[0];
			

			length = plane.normal[0] * plane.normal[0]
			+ plane.normal[1] * plane.normal[1]
			+ plane.normal[2] * plane.normal[2];
			
			if (length < ON_EPSILON)
				continue;

			length = 1/sqrt(length);
			
			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

			plane.dist = DotProduct (pass->points[j], plane.normal);

#if 1
			fliptest = qfalse;
			for (k=0 ; k<source->numpoints ; k++)
			{
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
				{	
					fliptest = qfalse;
					break;
				}
				else if (d > ON_EPSILON)
				{	
					fliptest = qtrue;
					break;
				}
			}
			if (k == source->numpoints)
				continue;		
#else
			fliptest = flipclip;
#endif
			if (fliptest)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
#if 1
			counts[0] = counts[1] = counts[2] = 0;
			for (k=0 ; k<pass->numpoints ; k++)
			{
				if (k==j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	
				
			if (!counts[0])
				continue;	
#else
			k = (j+1)%pass->numpoints;
			d = DotProduct (pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_EPSILON)
				continue;
			k = (j+pass->numpoints-1)%pass->numpoints;
			d = DotProduct (pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_EPSILON)
				continue;			
#endif
			if (flipclip)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}

#ifdef SEPERATORCACHE
			stack->seperators[flipclip][stack->numseperators[flipclip]] = plane;
			if (++stack->numseperators[flipclip] >= MAX_SEPERATORS)
				Error("MAX_SEPERATORS");
#endif
			d = DotProduct (stack->portal->origin, plane.normal) - plane.dist;
			if (d < -stack->portal->radius)
				return NULL;
			if (d > stack->portal->radius)
				break;

			target = VisChopWinding (target, stack, &plane);
			if (!target)
				return NULL;		

			break;		
		}
	}
	
	return target;
}

void RecursiveLeafFlow (int leafnum, threaddata_t *thread, pstack_t *prevstack)
{
	pstack_t	stack;
	vportal_t	*p;
	plane_t		backplane;
	leaf_t 		*leaf;
	int			i, j, n;
	long		*test, *might, *prevmight, *vis, more;
	int			pnum;

	thread->c_chains++;

	leaf = &leafs[leafnum];

	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;
	stack.depth = prevstack->depth + 1;

#ifdef SEPERATORCACHE
	stack.numseperators[0] = 0;
	stack.numseperators[1] = 0;
#endif

	might = (long *)stack.mightsee;
	vis = (long *)thread->base->portalvis;
	
	for (i = 0; i < leaf->numportals; i++)
	{
		p = leaf->portals[i];
		if (p->removed)
			continue;
		pnum = p - portals;


		if ( ! (prevstack->mightsee[pnum >> 3] & (1<<(pnum&7)) ) )
		{
			continue;	
		}

		if (p->status == stat_done)
		{
			test = (long *)p->portalvis;
		}
		else
		{
			test = (long *)p->portalflood;
		}

		more = 0;
		prevmight = (long *)prevstack->mightsee;
		for (j=0 ; j<portallongs ; j++)
		{
			might[j] = prevmight[j] & test[j];
			more |= (might[j] & ~vis[j]);
		}
		
		if (!more && 
			(thread->base->portalvis[pnum>>3] & (1<<(pnum&7))) )
		{	
			continue;
		}

		stack.portalplane = p->plane;
		VectorSubtract (vec3_origin, p->plane.normal, backplane.normal);
		backplane.dist = -p->plane.dist;
		
		
		stack.portal = p;
		stack.next = NULL;
		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;
		
#if 1
		{
			float d;

			d = DotProduct (p->origin, thread->pstack_head.portalplane.normal);
			d -= thread->pstack_head.portalplane.dist;
			if (d < -p->radius)
			{
				continue;
			}
			else if (d > p->radius)
			{
				stack.pass = p->winding;
			}
			else	
			{
				stack.pass = VisChopWinding (p->winding, &stack, &thread->pstack_head.portalplane);
				if (!stack.pass)
					continue;
			}
		}
#else
		stack.pass = VisChopWinding (p->winding, &stack, &thread->pstack_head.portalplane);
		if (!stack.pass)
			continue;
#endif

	
#if 1
		{
			float d;

			d = DotProduct (thread->base->origin, p->plane.normal);
			d -= p->plane.dist;
			if (d > thread->base->radius)
			{
				continue;
			}
			else if (d < -thread->base->radius)
			{
				stack.source = prevstack->source;
			}
			else	
			{
				stack.source = VisChopWinding (prevstack->source, &stack, &backplane);
				if (!stack.source)
					continue;
			}
		}
#else
		stack.source = VisChopWinding (prevstack->source, &stack, &backplane);
		if (!stack.source)
			continue;
#endif

		if (!prevstack->pass)
		{	

			thread->base->portalvis[pnum>>3] |= (1<<(pnum&7));

			RecursiveLeafFlow (p->leaf, thread, &stack);
			continue;
		}

#ifdef SEPERATORCACHE
		if (stack.numseperators[0])
		{
			for (n = 0; n < stack.numseperators[0]; n++)
			{
				stack.pass = VisChopWinding (stack.pass, &stack, &stack.seperators[0][n]);
				if (!stack.pass)
					break;		
			}
			if (n < stack.numseperators[0])
				continue;
		}
		else
		{
			stack.pass = ClipToSeperators (prevstack->source, prevstack->pass, stack.pass, qfalse, &stack);
		}
#else
		stack.pass = ClipToSeperators (stack.source, prevstack->pass, stack.pass, qfalse, &stack);
#endif
		if (!stack.pass)
			continue;

#ifdef SEPERATORCACHE
		if (stack.numseperators[1])
		{
			for (n = 0; n < stack.numseperators[1]; n++)
			{
				stack.pass = VisChopWinding (stack.pass, &stack, &stack.seperators[1][n]);
				if (!stack.pass)
					break;		
			}
		}
		else
		{
			stack.pass = ClipToSeperators (prevstack->pass, prevstack->source, stack.pass, qtrue, &stack);
		}
#else
		stack.pass = ClipToSeperators (prevstack->pass, stack.source, stack.pass, qtrue, &stack);
#endif
		if (!stack.pass)
			continue;

		thread->base->portalvis[pnum>>3] |= (1<<(pnum&7));

		RecursiveLeafFlow (p->leaf, thread, &stack);
		stack.next = NULL;
	}	
}

void PortalFlow (int portalnum)
{
	threaddata_t	data;
	int				i;
	vportal_t		*p;
	int				c_might, c_can;

#ifdef MREDEBUG
	_printf("\r%6d", portalnum);
#endif

	p = sorted_portals[portalnum];

	if (p->removed)
	{
		p->status = stat_done;
		return;
	}

	p->status = stat_working;

	c_might = CountBits (p->portalflood, numportals*2);

	memset (&data, 0, sizeof(data));
	data.base = p;
	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.depth = 0;
	for (i=0 ; i<portallongs ; i++)
		((long *)data.pstack_head.mightsee)[i] = ((long *)p->portalflood)[i];

	RecursiveLeafFlow (p->leaf, &data, &data.pstack_head);

	p->status = stat_done;

	c_can = CountBits (p->portalvis, numportals*2);

	qprintf ("portal:%4i  mightsee:%4i  cansee:%4i (%i chains)\n", 
		(int)(p - portals),	c_might, c_can, data.c_chains);
}

void RecursivePassageFlow (vportal_t *portal, threaddata_t *thread, pstack_t *prevstack)
{
	pstack_t	stack;
	vportal_t	*p;
	leaf_t 		*leaf;
	passage_t	*passage, *nextpassage;
	int			i, j;
	long		*might, *vis, *prevmight, *cansee, *portalvis, more;
	int			pnum;

	leaf = &leafs[portal->leaf];

	prevstack->next = &stack;

	stack.next = NULL;
	stack.depth = prevstack->depth + 1;

	vis = (long *)thread->base->portalvis;

	passage = portal->passages;
	nextpassage = passage;
	for (i = 0; i < leaf->numportals; i++, passage = nextpassage)
	{
		p = leaf->portals[i];
		if ( p->removed ) {
			continue;
		}
		nextpassage = passage->next;
		pnum = p - portals;

		if ( ! (prevstack->mightsee[pnum >> 3] & (1<<(pnum&7)) ) ) {
			continue;	
		}

		thread->base->portalvis[pnum>>3] |= (1<<(pnum&7));

		prevmight = (long *)prevstack->mightsee;
		cansee = (long *)passage->cansee;
		might = (long *)stack.mightsee;
		memcpy(might, prevmight, portalbytes);
		if (p->status == stat_done)
			portalvis = (long *) p->portalvis;
		else
			portalvis = (long *) p->portalflood;
		more = 0;
		for (j = 0; j < portallongs; j++)
		{
			if (*might)
			{
				*might &= *cansee++ & *portalvis++;
				more |= (*might & ~vis[j]);
			}
			else
			{
				cansee++;
				portalvis++;
			}
			might++;
		}

		if ( !more ) {
			continue;
		}

		RecursivePassageFlow(p, thread, &stack);

		stack.next = NULL;
	}
}

void PassageFlow (int portalnum)
{
	threaddata_t	data;
	int				i;
	vportal_t		*p;

#ifdef MREDEBUG
	_printf("\r%6d", portalnum);
#endif

	p = sorted_portals[portalnum];

	if (p->removed)
	{
		p->status = stat_done;
		return;
	}

	p->status = stat_working;


	memset (&data, 0, sizeof(data));
	data.base = p;
	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.depth = 0;
	for (i=0 ; i<portallongs ; i++)
		((long *)data.pstack_head.mightsee)[i] = ((long *)p->portalflood)[i];

	RecursivePassageFlow (p, &data, &data.pstack_head);

	p->status = stat_done;

}

void RecursivePassagePortalFlow (vportal_t *portal, threaddata_t *thread, pstack_t *prevstack)
{
	pstack_t	stack;
	vportal_t	*p;
	leaf_t 		*leaf;
	plane_t		backplane;
	passage_t	*passage, *nextpassage;
	int			i, j, n;
	long		*might, *vis, *prevmight, *cansee, *portalvis, more;
	int			pnum;


	leaf = &leafs[portal->leaf];

	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;
	stack.depth = prevstack->depth + 1;

#ifdef SEPERATORCACHE
	stack.numseperators[0] = 0;
	stack.numseperators[1] = 0;
#endif

	vis = (long *)thread->base->portalvis;

	passage = portal->passages;
	nextpassage = passage;
	for (i = 0; i < leaf->numportals; i++, passage = nextpassage)
	{
		p = leaf->portals[i];
		if (p->removed)
			continue;
		nextpassage = passage->next;
		pnum = p - portals;

		if ( ! (prevstack->mightsee[pnum >> 3] & (1<<(pnum&7)) ) )
			continue;	

		prevmight = (long *)prevstack->mightsee;
		cansee = (long *)passage->cansee;
		might = (long *)stack.mightsee;
		memcpy(might, prevmight, portalbytes);
		if (p->status == stat_done)
			portalvis = (long *) p->portalvis;
		else
			portalvis = (long *) p->portalflood;
		more = 0;
		for (j = 0; j < portallongs; j++)
		{
			if (*might)
			{
				*might &= *cansee++ & *portalvis++;
				more |= (*might & ~vis[j]);
			}
			else
			{
				cansee++;
				portalvis++;
			}
			might++;
		}

		if (!more && (thread->base->portalvis[pnum>>3] & (1<<(pnum&7))) )
		{	
			continue;
		}

		stack.portalplane = p->plane;
		VectorSubtract (vec3_origin, p->plane.normal, backplane.normal);
		backplane.dist = -p->plane.dist;
		
		
		stack.portal = p;
		stack.next = NULL;
		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;

#if 1
		{
			float d;

			d = DotProduct (p->origin, thread->pstack_head.portalplane.normal);
			d -= thread->pstack_head.portalplane.dist;
			if (d < -p->radius)
			{
				continue;
			}
			else if (d > p->radius)
			{
				stack.pass = p->winding;
			}
			else	
			{
				stack.pass = VisChopWinding (p->winding, &stack, &thread->pstack_head.portalplane);
				if (!stack.pass)
					continue;
			}
		}
#else
		stack.pass = VisChopWinding (p->winding, &stack, &thread->pstack_head.portalplane);
		if (!stack.pass)
			continue;
#endif

	
#if 1
		{
			float d;

			d = DotProduct (thread->base->origin, p->plane.normal);
			d -= p->plane.dist;
			if (d > thread->base->radius)
			{
				continue;
			}
			else if (d < -thread->base->radius)
			{
				stack.source = prevstack->source;
			}
			else	
			{
				stack.source = VisChopWinding (prevstack->source, &stack, &backplane);
				if (!stack.source)
					continue;
			}
		}
#else
		stack.source = VisChopWinding (prevstack->source, &stack, &backplane);
		if (!stack.source)
			continue;
#endif

		if (!prevstack->pass)
		{	

			thread->base->portalvis[pnum>>3] |= (1<<(pnum&7));

			RecursivePassagePortalFlow (p, thread, &stack);
			continue;
		}

#ifdef SEPERATORCACHE
		if (stack.numseperators[0])
		{
			for (n = 0; n < stack.numseperators[0]; n++)
			{
				stack.pass = VisChopWinding (stack.pass, &stack, &stack.seperators[0][n]);
				if (!stack.pass)
					break;		
			}
			if (n < stack.numseperators[0])
				continue;
		}
		else
		{
			stack.pass = ClipToSeperators (prevstack->source, prevstack->pass, stack.pass, qfalse, &stack);
		}
#else
		stack.pass = ClipToSeperators (stack.source, prevstack->pass, stack.pass, qfalse, &stack);
#endif
		if (!stack.pass)
			continue;

#ifdef SEPERATORCACHE
		if (stack.numseperators[1])
		{
			for (n = 0; n < stack.numseperators[1]; n++)
			{
				stack.pass = VisChopWinding (stack.pass, &stack, &stack.seperators[1][n]);
				if (!stack.pass)
					break;		
			}
		}
		else
		{
			stack.pass = ClipToSeperators (prevstack->pass, prevstack->source, stack.pass, qtrue, &stack);
		}
#else
		stack.pass = ClipToSeperators (prevstack->pass, stack.source, stack.pass, qtrue, &stack);
#endif
		if (!stack.pass)
			continue;

		thread->base->portalvis[pnum>>3] |= (1<<(pnum&7));

		RecursivePassagePortalFlow(p, thread, &stack);
		stack.next = NULL;
	}
}

void PassagePortalFlow (int portalnum)
{
	threaddata_t	data;
	int				i;
	vportal_t		*p;

#ifdef MREDEBUG
	_printf("\r%6d", portalnum);
#endif

	p = sorted_portals[portalnum];

	if (p->removed)
	{
		p->status = stat_done;
		return;
	}

	p->status = stat_working;


	memset (&data, 0, sizeof(data));
	data.base = p;
	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.depth = 0;
	for (i=0 ; i<portallongs ; i++)
		((long *)data.pstack_head.mightsee)[i] = ((long *)p->portalflood)[i];

	RecursivePassagePortalFlow (p, &data, &data.pstack_head);

	p->status = stat_done;

}

winding_t *PassageChopWinding (winding_t *in, winding_t *out, plane_t *split)
{
	vec_t	dists[128];
	int		sides[128];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*neww;

	counts[0] = counts[1] = counts[2] = 0;

	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}

	if (!counts[1])
		return in;		
	
	if (!counts[0])
	{
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	
	neww = out;

	neww->numpoints = 0;

	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->points[i];

		if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
		{
			return in;		
		}

		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}
		
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
		if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
		{
			return in;		
		}

		p2 = in->points[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}
	
	return neww;
}

int AddSeperators (winding_t *source, winding_t *pass, qboolean flipclip, plane_t *seperators, int maxseperators)
{
	int			i, j, k, l;
	plane_t		plane;
	vec3_t		v1, v2;
	float		d;
	vec_t		length;
	int			counts[3], numseperators;
	qboolean	fliptest;

	numseperators = 0;
	for (i=0 ; i<source->numpoints ; i++)
	{
		l = (i+1)%source->numpoints;
		VectorSubtract (source->points[l] , source->points[i], v1);

		for (j=0 ; j<pass->numpoints ; j++)
		{
			VectorSubtract (pass->points[j], source->points[i], v2);

			plane.normal[0] = v1[1]*v2[2] - v1[2]*v2[1];
			plane.normal[1] = v1[2]*v2[0] - v1[0]*v2[2];
			plane.normal[2] = v1[0]*v2[1] - v1[1]*v2[0];
			

			length = plane.normal[0] * plane.normal[0]
			+ plane.normal[1] * plane.normal[1]
			+ plane.normal[2] * plane.normal[2];
			
			if (length < ON_EPSILON)
				continue;

			length = 1/sqrt(length);
			
			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

			plane.dist = DotProduct (pass->points[j], plane.normal);

#if 1
			fliptest = qfalse;
			for (k=0 ; k<source->numpoints ; k++)
			{
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
				{	
					fliptest = qfalse;
					break;
				}
				else if (d > ON_EPSILON)
				{	
					fliptest = qtrue;
					break;
				}
			}
			if (k == source->numpoints)
				continue;		
#else
			fliptest = flipclip;
#endif
			if (fliptest)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
#if 1
			counts[0] = counts[1] = counts[2] = 0;
			for (k=0 ; k<pass->numpoints ; k++)
			{
				if (k==j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	
				
			if (!counts[0])
				continue;	
#else
			k = (j+1)%pass->numpoints;
			d = DotProduct (pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_EPSILON)
				continue;
			k = (j+pass->numpoints-1)%pass->numpoints;
			d = DotProduct (pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_EPSILON)
				continue;			
#endif
			if (flipclip)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}

			if (numseperators >= maxseperators)
				Error("max seperators");
			seperators[numseperators] = plane;
			numseperators++;
			break;
		}
	}
	return numseperators;
}

void CreatePassages(int portalnum)
{
	int i, j, k, n, numseperators, numsee;
	float d;
	vportal_t *portal, *p, *target;
	leaf_t *leaf;
	passage_t	*passage, *lastpassage;
	plane_t seperators[MAX_SEPERATORS*2];
	winding_t *w;
	winding_t in, out, *res;

#ifdef MREDEBUG
	_printf("\r%6d", portalnum);
#endif

	portal = sorted_portals[portalnum];

	if (portal->removed)
	{
		portal->status = stat_done;
		return;
	}

	lastpassage = NULL;
	leaf = &leafs[portal->leaf];
	for (i = 0; i < leaf->numportals; i++)
	{
		target = leaf->portals[i];
		if (target->removed)
			continue;

		passage = (passage_t *) malloc(sizeof(passage_t) + portalbytes);
		memset(passage, 0, sizeof(passage_t) + portalbytes);
		numseperators = AddSeperators(portal->winding, target->winding, qfalse, seperators, MAX_SEPERATORS*2);
		numseperators += AddSeperators(target->winding, portal->winding, qtrue, &seperators[numseperators], MAX_SEPERATORS*2-numseperators);

		passage->next = NULL;
		if (lastpassage)
			lastpassage->next = passage;
		else
			portal->passages = passage;
		lastpassage = passage;

		numsee = 0;
		for (j = 0; j < numportals * 2; j++)
		{
			p = &portals[j];
			if (p->removed)
				continue;
			if ( ! (target->portalflood[j >> 3] & (1<<(j&7)) ) )
				continue;
			if ( ! (portal->portalflood[j >> 3] & (1<<(j&7)) ) )
				continue;
			for (k = 0; k < numseperators; k++)
			{
				d = DotProduct (p->origin, seperators[k].normal) - seperators[k].dist;
				if (d < -p->radius + ON_EPSILON)
					break;
				w = p->winding;
				for (n = 0; n < w->numpoints; n++)
				{
					d = DotProduct (w->points[n], seperators[k].normal) - seperators[k].dist;
					if (d > ON_EPSILON)
						break;
				}
				if (n >= w->numpoints)
					break;
			}
			if (k < numseperators)
				continue;
			memcpy(&in, p->winding, sizeof(winding_t));
			for (k = 0; k < numseperators; k++)
			{
				res = PassageChopWinding(&in, &out, &seperators[k]);
				if (res == &out)
					memcpy(&in, &out, sizeof(winding_t));
				if (res == NULL)
					break;
			}
			if (k < numseperators)
				continue;
			passage->cansee[j >> 3] |= (1<<(j&7));
			numsee++;
		}
	}
}

void PassageMemory(void)
{
	int i, j, totalmem, totalportals;
	vportal_t *portal, *target;
	leaf_t *leaf;

	totalmem = 0;
	totalportals = 0;
	for (i = 0; i < numportals; i++)
	{
		portal = sorted_portals[i];
		if (portal->removed)
			continue;
		leaf = &leafs[portal->leaf];
		for (j = 0; j < leaf->numportals; j++)
		{
			target = leaf->portals[j];
			if (target->removed)
				continue;
			totalmem += sizeof(passage_t) + portalbytes;
			totalportals++;
		}
	}
	_printf("%7i average number of passages per leaf\n", totalportals / numportals);
	_printf("%7i MB required passage memory\n", totalmem >> 10 >> 10);
}


int		c_flood, c_vis;


void SimpleFlood (vportal_t *srcportal, int leafnum)
{
	int		i;
	leaf_t	*leaf;
	vportal_t	*p;
	int		pnum;

	leaf = &leafs[leafnum];
	
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];
		if (p->removed)
			continue;
		pnum = p - portals;
		if ( ! (srcportal->portalfront[pnum>>3] & (1<<(pnum&7)) ) )
			continue;

		if (srcportal->portalflood[pnum>>3] & (1<<(pnum&7)) )
			continue;

		srcportal->portalflood[pnum>>3] |= (1<<(pnum&7));
		
		SimpleFlood (srcportal, p->leaf);
	}
}

void BasePortalVis (int portalnum)
{
	int			j, k;
	vportal_t	*tp, *p;
	float		d;
	winding_t	*w;

	p = portals+portalnum;

	if (p->removed)
		return;

	p->portalfront = malloc (portalbytes);
	memset (p->portalfront, 0, portalbytes);

	p->portalflood = malloc (portalbytes);
	memset (p->portalflood, 0, portalbytes);
	
	p->portalvis = malloc (portalbytes);
	memset (p->portalvis, 0, portalbytes);
	
	for (j=0, tp = portals ; j<numportals*2 ; j++, tp++)
	{
		if (j == portalnum)
			continue;
		if (tp->removed)
			continue;
		w = tp->winding;
		for (k=0 ; k<w->numpoints ; k++)
		{
			d = DotProduct (w->points[k], p->plane.normal)
				- p->plane.dist;
			if (d > ON_EPSILON)
				break;
		}
		if (k == w->numpoints)
			continue;	

		w = p->winding;
		for (k=0 ; k<w->numpoints ; k++)
		{
			d = DotProduct (w->points[k], tp->plane.normal)
				- tp->plane.dist;
			if (d < -ON_EPSILON)
				break;
		}
		if (k == w->numpoints)
			continue;	

		p->portalfront[j>>3] |= (1<<(j&7));
	}
	
	SimpleFlood (p, p->leaf);

	p->nummightsee = CountBits (p->portalflood, numportals*2);
	c_flood += p->nummightsee;
}






void RecursiveLeafBitFlow (int leafnum, byte *mightsee, byte *cansee)
{
	vportal_t	*p;
	leaf_t 		*leaf;
	int			i, j;
	long		more;
	int			pnum;
	byte		newmight[MAX_PORTALS/8];

	leaf = &leafs[leafnum];
	
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];
		if (p->removed)
			continue;
		pnum = p - portals;

		if (! (mightsee[pnum>>3] & (1<<(pnum&7)) ) )
			continue;

		more = 0;
		for (j=0 ; j<portallongs ; j++)
		{
			((long *)newmight)[j] = ((long *)mightsee)[j] 
				& ((long *)p->portalflood)[j];
			more |= ((long *)newmight)[j] & ~((long *)cansee)[j];
		}

		if (!more)
			continue;	

		cansee[pnum>>3] |= (1<<(pnum&7));

		RecursiveLeafBitFlow (p->leaf, newmight, cansee);
	}	
}

void BetterPortalVis (int portalnum)
{
	vportal_t	*p;

	p = portals+portalnum;

	if (p->removed)
		return;

	RecursiveLeafBitFlow (p->leaf, p->portalflood, p->portalvis);

	p->nummightsee = CountBits (p->portalvis, numportals*2);
	c_vis += p->nummightsee;
}


