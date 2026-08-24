

class FindCoverMovement;
class FindFleeMovement;
class FindEnemyMovement;

#ifndef __ACTORUTIL_H__
#define __ACTORUTIL_H__

#include "actor.h"
#include "actorincludes.h"
#include "weapon.h"

qboolean EntityIsValidTarget( const Entity *ent );

class FindMovement : public StandardMovement
	{
	public:
		Actor			*self;

	qboolean validpath (	PathNode *node, int i );
	qboolean done ( PathNode *node, const PathNode *end );
	};



class FindCoverMovement : public StandardMovement
	{
	public:
		Actor			*self;

	



public:
    qboolean validpath ( PathNode *node, int i ); 
    qboolean done ( PathNode *node, const PathNode *end ); 
};



class FindFleeMovement : public StandardMovement
	{
	public:
		Actor			*self;

	qboolean validpath ( PathNode *node, int i );  
	qboolean done ( PathNode *node, const PathNode *end );  

	};

class FindEnemyMovement : public StandardMovement
	{
	public:
		Actor			*self;

	qboolean done ( PathNode *node, const PathNode *end ); 

	};


#endif 
