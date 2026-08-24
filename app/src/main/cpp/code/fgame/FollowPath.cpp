//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/FollowPath.cpp                            $
// $Revision:: 47                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
//
// DESCRIPTION:
// Provides base functionality for path following with avoidance
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "actor.h"
#include "actor_locomotion.h"
#include "FollowPath.h"
#include "debuglines.h"
#include <algorithm>


void DrawBox(const Vector &center, const float width, const float r, const float g, const float b, const float alpha )
{
	float squareSize=width/2.0f;
	Vector topLeftPoint		= center + Vector(-squareSize, -squareSize, 0);
	Vector topRightPoint		= center + Vector(+squareSize, -squareSize, 0);
	Vector bottomRightPoint	= center + Vector(+squareSize, +squareSize, 0);
	Vector bottomLeftPoint	= center + Vector(-squareSize, +squareSize, 0);
	G_DebugLine( topLeftPoint,			topRightPoint, r, g, b, alpha );
	G_DebugLine( topRightPoint,		bottomRightPoint, r, g, b, alpha );
	G_DebugLine( bottomRightPoint,	bottomLeftPoint, r, g, b, alpha );
	G_DebugLine( bottomLeftPoint,		topLeftPoint, r, g, b, alpha );
}


CLASS_DECLARATION( Class, FollowNode, NULL )
{
	{ NULL, NULL }
};

FollowNode::FollowNode ( void ):
	_position( 0, 0, 0 ),
	_isTemporary( false ),
	_isJumpNode( false ),
	_jumpAngle( 45.0f )
{
}

FollowNode::FollowNode ( const Vector &position, const bool isTemporary, const bool isJumpNode, const float jumpAngle ):
	_position( position ),
	_isTemporary( isTemporary ),
	_isJumpNode( isJumpNode ),
	_jumpAngle( jumpAngle )

{
}

CLASS_DECLARATION( Class, FollowNodePath, NULL )
{
	{ NULL, NULL }
};

FollowNodePath::FollowNodePath():
	_nodes(),
	_currentNode(NULL)
{
}

void FollowNodePath::RemoveNode(FollowNodePtr node)
{
	if (node == GetCurrentNode())
	{
		FollowNodePtr newCurrentNode=GetNextNode(GetCurrentNode());
		if (newCurrentNode == NULL)
		{
			newCurrentNode=GetPreviousNode(GetCurrentNode());
		}
		SetCurrentNode(newCurrentNode);
	}
	_nodes.RemoveObject(node);
}


void FollowNodePath::InsertNode(FollowNodePtr node, const int index)
{
	int currentNodeIndex=GetNodeIndex(GetCurrentNode());
	_nodes.InsertObjectAt(index, node);
	if (currentNodeIndex >= index)
	{
		SetCurrentNode(GetNodeAt(currentNodeIndex+1));
	}
}

FollowNodePtr FollowNodePath::GetNodeAt(const int index)
{
	return _nodes.ObjectAt(index);
}


void FollowNodePath::AddNode(FollowNodePtr node)
{
	_nodes.AddObject(node);
}

void FollowNodePath::SetCurrentNode(FollowNodePtr node)
{
	_currentNode = node;
}

FollowNodePtr FollowNodePath::GetCurrentNode(void) const
{
	return _currentNode;
}

int const FollowNodePath::GetNodeIndex(const FollowNodePtr node)
{
	return _nodes.IndexOfObject(node);
}

FollowNodePtr FollowNodePath::FirstNode(void)
{
	return GetNodeAt(1);
}

FollowNodePtr FollowNodePath::LastNode(void)
{
	return GetNodeAt(NumNodes());
}

FollowNodePtr FollowNodePath::GetPreviousNode(const FollowNodePtr node )
{
	int nodeIndex=GetNodeIndex(node);
	if (nodeIndex > 1)
	{
		return GetNodeAt(nodeIndex - 1);
	}
	return NULL;
}

FollowNodePtr FollowNodePath::GetNextNode(const FollowNodePtr node )
{
	int nodeIndex=GetNodeIndex(node);
	if (nodeIndex < NumNodes())
	{
		return GetNodeAt(nodeIndex + 1);
	}
	return NULL;
}

const	Steering::ReturnValue FollowNodePath::Evaluate( Actor &self, const Vector &goalPosition )
{
	Steering::ReturnValue returnValue = Steering::EVALUATING;
	if (
			( NumNodes() < 1 ) &&
			( SetPath( self, self.origin, goalPosition ) != PATH_CREATION_SUCCESS )
		)
	{
		return Steering::FAILED_NO_PATH;
	}
	else 
	{
		returnValue = AdvanceCurrentNode( self );

		if (RetreatCurrentNode( self, goalPosition ) == Steering::FAILED_NO_PATH)
		{
			returnValue = Steering::FAILED_NO_PATH;
		}

		if ( g_showactorpath->integer )
		{
			Draw();
		}
	}	
	return returnValue;
}

unsigned int const FollowNodePath::SetPath(Actor &self, const Vector &from, const Vector &to)
{
	PathNode *goalNode = thePathManager.NearestNode( to, &self );
	
	if ( !goalNode )
	{
		return PATH_CREATION_FAILED_END_NODE_NOT_FOUND;
	}
	
	PathNode *node = thePathManager.NearestNode( from, &self );
	if ( !node || ( goalNode == node ) )
	{
		return PATH_CREATION_FAILED_START_NODE_NOT_FOUND;
	}
	
	StandardMovePath find;
	find.heuristic.setSize( self.size );
	find.heuristic.entnum = self.entnum;
	find.heuristic.can_jump = self.CanJump() != 0;
	
	Clear();

	Path *newpath = find.FindPath( node, goalNode );
	if ( newpath != NULL )
	{
		BuildFromPathNodes( newpath, self );
		delete newpath;
		newpath = NULL;
		if ( self.GetActorFlag( ACTOR_FLAG_STRICTLY_FOLLOW_PATHS ) )
		{
			SetCurrentNode( FirstNode() );
		}
		else
		{
			if ( !FindNewCurrentNode( self ) )
			{
				SetCurrentNode( FirstNode() );
			}
		}
	}
	else
	{
		SetCurrentNode( NULL );
		return PATH_CREATION_FAILED_NODES_NOT_CONNECTED;
	}

	assert ( NumNodes() > 0 );
	return PATH_CREATION_SUCCESS;
}

void FollowNodePath::Clear()
{
	for (int i=NumNodes(); i>0; i--)
	{
		FollowNodePtr node=GetNodeAt(i);
		RemoveNode( node );
		delete node;
	}
	_currentNode=NULL;
}

void FollowNodePath::Draw( void )
{
	if (NumNodes() == 0)
	{
		return;
	}
	
	float squareSize=8.0f;
	Vector verticalOffset(0,0,16);
	DrawBox(GetNodeAt(1)->GetPosition() + verticalOffset, squareSize , 0, 1, 0, 1);
	for (int i=NumNodes(); i>1; i--)
	{
		if ( GetNodeAt(i)->GetIsTemporary() )
		{
			DrawBox(GetNodeAt(i)->GetPosition() + verticalOffset, squareSize , 1, 0, 1, 1);
		}
		else
		{
			DrawBox(GetNodeAt(i)->GetPosition() + verticalOffset, squareSize , 0, 1, 0, 1);
		}

		if (i%2)
		{
			G_DebugLine( GetNodeAt(i)->GetPosition() + verticalOffset, GetNodeAt(i-1)->GetPosition() + verticalOffset, 0.0f, 1.0f, 0.0f, 1.0f );
		}
		else
		{
			G_DebugLine( GetNodeAt(i)->GetPosition() + verticalOffset, GetNodeAt(i-1)->GetPosition() + verticalOffset, 0.0f, 0.5f, 0.0f, 1.0f );
		}
	}
	if (_currentNode!=NULL)
	{
		DrawBox(_currentNode->GetPosition() + verticalOffset, squareSize * 2, 1, 1, 1, 1);
	}
	DrawBox(LastNode()->GetPosition() + verticalOffset, squareSize * 4, 1, 0, 0, 1);
}

void FollowNodePath::BuildFromPathNodes(Path *path, const Actor &self)
{
	assert(path->NumNodes() > 0);
	
	int sourceNodeNumber=1;
	int sourceNumNodes=path->NumNodes();
	
	
	AddNode (new FollowNode(path->GetNode(sourceNodeNumber++)->origin));
	PathNodePtr currentPathNode = NULL;
	PathNodePtr nextPathNode = NULL;
	while(sourceNodeNumber < sourceNumNodes)
	{
		bool isJumpNode = false;
		float jumpAngle = 0.0f;

		currentPathNode = path->GetNode(sourceNodeNumber);
		
		if ( sourceNodeNumber < path->NumNodes()  && (currentPathNode->nodeflags & AI_JUMP) && currentPathNode->target.length() )
		{
			nextPathNode = path->GetNode(sourceNodeNumber + 1);
			if (currentPathNode->target == nextPathNode->targetname)
			{
				isJumpNode = true;
				jumpAngle = currentPathNode->jumpAngle;
			}
		}

		AddNode ( new FollowNode( currentPathNode->origin, false, isJumpNode, jumpAngle ) );
		sourceNodeNumber++;
	}
	AddNode (new FollowNode( path->GetNode(sourceNumNodes)->origin));
}

const bool FollowNodePath::FindNewCurrentNode(const Actor &self)
{
	int mask;
	int currentNodeIndex = GetNodeIndex( GetCurrentNode() );
	for ( 
		int currentTestNodeNumber = NumNodes(); 
		currentTestNodeNumber > currentNodeIndex;
		currentTestNodeNumber--
		)
	{
		FollowNodePtr testnode = GetNodeAt(currentTestNodeNumber);
		if (currentTestNodeNumber == 1)
		{
			SetCurrentNode(testnode);
			return true;
		}
		
		mask = self.edict->clipmask & ~CONTENTS_BODY;

		trace_t trace = self.Trace( self.origin, testnode->GetPosition(), mask, "FollowPath" );      
		
		if ( !trace.startsolid && ( trace.fraction == 1.0f ) && self.movementSubsystem->CanWalkTo( testnode->GetPosition() ) )
		{
			SetCurrentNode(testnode);
			return true;
		}
	}
	return false;
}

const Steering::ReturnValue FollowNodePath::AdvanceCurrentNode( Actor const &self )
{
	if ( GetCurrentNode() != NULL )
	{
		FollowNodePtr nextNode = GetNextNode( GetCurrentNode() );

		float radius = std::max( self.total_delta.lengthXY(), 16.0f );
		if ( Vector::DistanceXY( self.origin, GetCurrentNode()->GetPosition() ) <= radius )
		{
			if ( nextNode != NULL )
			{
				SetCurrentNode ( nextNode );
				return Steering::EVALUATING_REACHED_NODE;
			}
			else
			{
				Clear();
				return Steering::SUCCESS;
			}
		}
		return Steering::EVALUATING;
	}
	return Steering::FAILED_NO_PATH;
}

const Steering::ReturnValue FollowNodePath::RetreatCurrentNode(Actor &self, const Vector &goalPosition)
{
	int mask;

	if ( GetCurrentNode() == NULL )
	{
		return Steering::FAILED_NO_PATH;
	}

	if ( GetCurrentNodeIndex() == 1 )
	{
		return Steering::EVALUATING;
	}


	mask = self.edict->clipmask & ~CONTENTS_BODY;

	if ( !GetPreviousNode(GetCurrentNode() )->GetIsJumpNode() )
	{
		trace_t traceToCurrentNode = self.Trace( self.origin, GetCurrentNode()->GetPosition(), mask,  "FollowPath" );
		
		if ( traceToCurrentNode.startsolid || ( traceToCurrentNode.fraction < 1.0f ) )
		{
			gi.WDPrintf( "Couldn't complete path, building new one for %s (%d)\n" , self.targetname.c_str(), self.entnum );

			if ( SetPath( self, self.origin, goalPosition ) != PATH_CREATION_SUCCESS )
			{
				return Steering::FAILED_NO_PATH;
			}
		}
	}
	return Steering::EVALUATING;
}


CLASS_DECLARATION( Steering, FollowPath, NULL )
{
	{ NULL, NULL }
};

FollowPath::FollowPath():
	_avoidanceDistance(96.0f),
	_jumping( false )
{
}

void FollowPath::Begin(Actor &self)
{
	_desiredHeading = self.movementSubsystem->getMoveDir().toAngles();
	_jumping = false;
}

void FollowPath::SetSteeringForceAndDesiredHeading( Actor &self, const Vector &steeringForce )
{
	_desiredHeading = self.movementSubsystem->getMoveDir().toAngles() + steeringForce;
	_desiredHeading.EulerNormalize360();
	SetSteeringForce( steeringForce );
}

const bool FollowPath::DoneTurning( Actor &self ) const
{
	Vector deltaHeading( _desiredHeading - self.movementSubsystem->getMoveDir().toAngles() );
	deltaHeading.EulerNormalize();
	return fabs( deltaHeading[YAW] ) < 1.0f ;
}


const Steering::ReturnValue FollowPath::GotoCurrentNode( Actor &self ) 
{
	float traceLength = std::min(Vector::DistanceXY( self.origin, GetCurrentNode()->GetPosition() ), GetAvoidanceDistance() );
	Vector traceEnd = self.origin + ( self.movementSubsystem->getMoveDir() * traceLength );
	trace_t horizontalTrace;
	trace_t verticalTrace;
	stepmoveresult_t result = self.movementSubsystem->IsMoveValid( horizontalTrace, verticalTrace, self.origin, traceEnd );
	if ( result == STEPMOVE_STUCK )
	{
		return Steering::FAILED;
	}
	if ( result == STEPMOVE_BLOCKED_BY_ENTITY )
	{
		return AvoidObstacle( self, horizontalTrace );
	}

	SteerToCurrentNode( self );
	return Steering::EVALUATING;

}

const Steering::ReturnValue FollowPath::GotoGoal( Actor &self ) 
{
	float traceLength = std::min(Vector::DistanceXY( self.origin, GetGoalPosition() ), GetAvoidanceDistance() );
	Vector traceEnd = self.origin + ( self.movementSubsystem->getMoveDir() * traceLength );
	trace_t horizontalTrace;
	trace_t verticalTrace;
	stepmoveresult_t result = self.movementSubsystem->IsMoveValid( horizontalTrace, verticalTrace, self.origin, traceEnd );
	if ( result == STEPMOVE_STUCK )
	{
		return Steering::FAILED;
	}
	if ( result == STEPMOVE_BLOCKED_BY_ENTITY )
	{
		return AvoidObstacle( self, horizontalTrace, true );
	}

	SetSteeringForceAndDesiredHeading
									( 
										self, 
										self.movementSubsystem->SteerTowardsPoint
										( 
											GetGoalPosition(),			
											vec_zero,											
											self.movementSubsystem->getMoveDir(),		
											self.movementSubsystem->getMoveSpeed()		
										) 
									);
	return Steering::EVALUATING;
}

const Steering::ReturnValue FollowPath::Evaluate( Actor &self )
{
	ResetForces();


	if ( _jumping )
	{
		if ( _jump.Evaluate( self ) == Steering::SUCCESS)
		{
			_jump.End( self );
			_jumping = false;
			self.SetAnim( _oldAnim );
		}
		return Steering::EVALUATING;
	}
	

	if (AtDestination( self ))
	{
		FreePath();	


		return Steering::SUCCESS;
	}


	Steering::ReturnValue returnValue = Steering::EVALUATING;

	trace_t traceToGoal;
	trace_t verticalTrace;
	stepmoveresult_t moveResult = self.movementSubsystem->IsMoveValid( traceToGoal, verticalTrace, self.origin, GetGoalPosition() );

	if  ( 
			(
			!self.GetActorFlag(ACTOR_FLAG_STRICTLY_FOLLOW_PATHS) &&
			(( moveResult == STEPMOVE_OK) || 
			( traceToGoal.entityNum != ENTITYNUM_WORLD && traceToGoal.entityNum != ENTITYNUM_NONE && ClearTraceToGoal( self, traceToGoal, GetGoalRadius() ) ) 
			) && self.movementSubsystem->CanWalkTowardsPoint( GetGoalPosition() ) )
		)
	{
		FreePath();
		SteerToGoal( self );
	}
	else
	{
		if ( DoneTurning( self ) )
		{
			returnValue = GetPath().Evaluate( self, GetGoalPosition() ); 

			int currentNodeIndex = GetPath().GetCurrentNodeIndex();
			if ( currentNodeIndex > 1 )
			{
				FollowNodePtr lastNode = GetPath().GetNodeAt( currentNodeIndex - 1 );
				const Vector currentNodePosition = GetPath().GetCurrentNode()->GetPosition();
				const float distanceFromActorToCurrentNode = Vector::DistanceXY( self.origin, currentNodePosition );
				const float distanceBetweenNodes = Vector::DistanceXY( lastNode->GetPosition(), currentNodePosition );
				if ( lastNode->GetIsJumpNode() && ( distanceFromActorToCurrentNode > 0.5f * distanceBetweenNodes ) )
				{
					_jumping = true;
					_oldAnim = self.animname;
					_jump.SetGoal( GetCurrentNode()->GetPosition() );
					_jump.SetLaunchAngle( lastNode->GetJumpAngle() );
					_jump.Begin( self );
					return Steering::EVALUATING;
				}
			}
			if ( returnValue == Steering::EVALUATING )
			{
				returnValue = GotoCurrentNode( self );
			}

			else if ( returnValue == Steering::FAILED_NO_PATH && moveResult != STEPMOVE_BLOCKED_BY_WORLD )
			{
				returnValue = GotoGoal( self );
			}
			
		}
		else
		{
			Vector newSteeringForce( _desiredHeading - self.movementSubsystem->getMoveDir().toAngles() );
			newSteeringForce.EulerNormalize();
			SetSteeringForce( newSteeringForce );
		}
	}


	if ( returnValue == Steering::EVALUATING )
	{
		self.movementSubsystem->Accelerate( GetSteeringForce() );
		return Steering::EVALUATING;
	}
	else
	{
		return returnValue;
	}
}

void FollowPath::ShowInfo( Actor &self )
{
   Steering::ShowInfo( self );
	
   gi.Printf( "\npath : ( %f, %f, %f ) to ( %f, %f, %f )\n",
      _path.FirstNode()->GetPosition().x, _path.FirstNode()->GetPosition().y, _path.FirstNode()->GetPosition().z,
      _path.LastNode()->GetPosition().x, _path.LastNode()->GetPosition().y, _path.LastNode()->GetPosition().z );
	
   if ( GetCurrentNode() )
	{
      gi.Printf( "currentNode: ( %f, %f, %f )\n",
         GetCurrentNode()->GetPosition().x, GetCurrentNode()->GetPosition().y, GetCurrentNode()->GetPosition().z );
	}
   else
	{
      gi.Printf( "currentNode: NULL\n" );
	}
}

void FollowPath::DeleteTemporaryPathNodes(void)
{
	int numNodes=GetPath().NumNodes();
	int i;
	for (i=numNodes; i>0; i--)
	{
		FollowNodePtr nodeToRemove=GetPath().GetNodeAt(i);
		if (nodeToRemove->GetIsTemporary())
		{
			GetPath().RemoveNode(nodeToRemove);
		}
	}
}

const bool FollowPath::ClearTraceToGoal( Actor &self, const trace_t &traceToGoal, const float radius ) const
{
	bool traceToGoalIsClear = traceToGoal.fraction == 1.0f;																			
	
	float distanceToEdgeOfGoal = Vector::DistanceXY( self.origin, GetGoalPosition() ) - radius;  
	float lengthOfTrace = Vector::DistanceXY( self.origin, Vector( traceToGoal.endpos ) );
	bool traceGotInsideGoalRadius = ( distanceToEdgeOfGoal < lengthOfTrace );														

	return ( !traceToGoal.startsolid && ( traceToGoalIsClear || traceGotInsideGoalRadius) );
}

const bool FollowPath::BuildAvoidancePath(Actor  &self, const bool passOnTheLeft, const Vector &obstaclePosition, const float avoidanceRadius, const bool pursueGoal )
{
	DeleteTemporaryPathNodes();
	Vector myPosition(self.origin);
	float distanceToGoal=Vector::DistanceXY(myPosition, GetGoalPosition());
	Vector parallelDirection(self.movementSubsystem->getMoveDir());
	if ( distanceToGoal < avoidanceRadius * 2.0f || pursueGoal )
	{
		parallelDirection = GetGoalPosition() - myPosition;
	}
	else
	{
		int currentNodeIndex = GetPath().GetNodeIndex(GetPath().GetCurrentNode());
		if (currentNodeIndex < GetPath().NumNodes())
		{
			parallelDirection = GetPath().GetNodeAt(currentNodeIndex + 1)->GetPosition() - GetPath().GetCurrentNode()->GetPosition();
		}
	}

	parallelDirection.normalize();

	Vector perpendicularDirection;	
	perpendicularDirection.CrossProduct(parallelDirection, Vector(0,0,1));
	if (passOnTheLeft)
	{
		perpendicularDirection *= -1.0f;
	}

	Vector firstPathNodeLocation(obstaclePosition + (avoidanceRadius * (perpendicularDirection - parallelDirection)));
	trace_t testTraceForFirstPoint;
	trace_t verticalTrace;
	stepmoveresult_t firstMoveResult = self.movementSubsystem->IsMoveValid( testTraceForFirstPoint, verticalTrace, self.origin, firstPathNodeLocation );

	if ( ( firstMoveResult == STEPMOVE_OK || ClearTraceToGoal( self, testTraceForFirstPoint, avoidanceRadius/2.0f ) ) )
	{
		Vector secondPathNodeLocation(obstaclePosition + (avoidanceRadius * (perpendicularDirection + parallelDirection)));
		trace_t testTraceForSecondPoint;
		stepmoveresult_t secondMoveResult = self.movementSubsystem->IsMoveValid( testTraceForSecondPoint, verticalTrace, firstPathNodeLocation, secondPathNodeLocation );
		
		if ( ( secondMoveResult == STEPMOVE_OK || ClearTraceToGoal( self, testTraceForSecondPoint, avoidanceRadius/2.0f ) ) )
		{
			Vector thirdPathNodeLocation(obstaclePosition + (2.0f * avoidanceRadius * parallelDirection));
			trace_t testTraceForThirdPoint;
			stepmoveresult_t thirdMoveResult = self.movementSubsystem->IsMoveValid( testTraceForThirdPoint, verticalTrace, secondPathNodeLocation, thirdPathNodeLocation );
			
			if ( ( thirdMoveResult == STEPMOVE_OK || ClearTraceToGoal( self, testTraceForThirdPoint, avoidanceRadius/2.0f ) ) )
			{
				int insertionPoint=GetPath().GetNodeIndex(GetPath().GetCurrentNode());
				if (insertionPoint > 0)
				{
					FollowNodePtr nextNode=GetPath().GetNodeAt(insertionPoint);
					while (nextNode != NULL && (Vector::DistanceXY(nextNode->GetPosition(), myPosition) < Vector::DistanceXY(myPosition, thirdPathNodeLocation)))
					{
						insertionPoint++;
						if (insertionPoint <= GetPath().NumNodes())
						{
							nextNode=GetPath().GetNodeAt(insertionPoint);
						}
						else
						{
							nextNode=NULL;
						}
					}
				}
				else
				{
					insertionPoint=1;
				}
				if ( Vector::DistanceXY(myPosition, firstPathNodeLocation) < distanceToGoal)
				{
					if (Vector::DistanceXY(myPosition, secondPathNodeLocation) < distanceToGoal)
					{
						if (Vector::DistanceXY(myPosition, thirdPathNodeLocation) < distanceToGoal)
						{
							InsertPathNode(new FollowNode(thirdPathNodeLocation, true), insertionPoint);
						}
						InsertPathNode(new FollowNode(secondPathNodeLocation, true), insertionPoint);
					}
					InsertPathNode(new FollowNode(firstPathNodeLocation, true), insertionPoint);
					SetSteeringForceAndDesiredHeading( self,
														self.movementSubsystem->SteerTowardsPoint (
														firstPathNodeLocation,							
														vec_zero,											
														self.movementSubsystem->getMoveDir(),		
														self.movementSubsystem->getMoveSpeed())	
														);
					return true;
				}
			}
		}
	}
	return false;
}

const Steering::ReturnValue FollowPath::AvoidObstacle(Actor &self, const trace_t &trace, const bool pursueGoal )	
{
	Entity *hitEntity = G_GetEntity( trace.entityNum );
	assert( hitEntity->entnum != ENTITYNUM_WORLD );
	if ( hitEntity == NULL )
	{
		return Steering::FAILED;
	}

	Vector obstaclePosition( hitEntity->origin );
	float avoidanceRadius( hitEntity->mins.lengthXY() + self.mins.lengthXY() );

	bool passOnLeft = false;
	switch( self._steeringDirectionPreference )
	{
	case STEER_RIGHT_ALWAYS:
		passOnLeft = false;
		break;
	case STEER_LEFT_ALWAYS:
		passOnLeft = true;
		break;
	case STEER_RANDOMLY:
		{
			int random = rand() % 2;
			if ( random )
			{
				passOnLeft = false;
			}
			else
			{
				passOnLeft = true;
			}
		}
		break;
	case STEER_BEST:
		passOnLeft = false;
		break;
	}

	bool foundPath = BuildAvoidancePath(self, passOnLeft, obstaclePosition, avoidanceRadius, pursueGoal );
	
	if (!foundPath)
	{
		foundPath = BuildAvoidancePath(self, !passOnLeft, obstaclePosition, avoidanceRadius, pursueGoal );
	}
	if (foundPath)
	{
		return Steering::EVALUATING;
	}
	else
	{
		return Steering::FAILED;
	}
}

const bool FollowPath::TraceBlockedByEntity(Actor &self, const trace_t & trace ) const
{
	if ( !trace.startsolid && (trace.fraction == 1.0f || trace.entityNum == ENTITYNUM_WORLD)) 
	{
		return false;
	}

	Entity *obstacleEntity = G_GetEntity( trace.entityNum );
	assert( obstacleEntity != NULL );

	if (obstacleEntity->velocity.lengthXY() < Vector::Epsilon())
	{
		return true;
	}

	Vector relativeVelocity = self.velocity - obstacleEntity->velocity;

	float rateOfClosure = self.movementSubsystem->getMoveDir() * relativeVelocity;

	if ( rateOfClosure >= 0.0f ) 
	{
		{
			return true;
		}
	}
	return false;
}

const bool FollowPath::CheckBlocked( Actor &self )
{
	trace_t trace = self.Trace( GetAvoidanceDistance() , "FollowPath" );

	if ( trace.ent )
	{
		Entity *door = trace.ent->entity;
		if ( door && door->isSubclassOf( Door ) )
		{
			Event *ev = new Event( EV_Use );
			ev->AddEntity( &self );
			door->PostEvent( ev, 0 );

			return true;
		}
	}
	return false;
}

const Steering::ReturnValue	FollowPath::ReturnBlockingObject(const trace_t &trace) const
{
	if (trace.entityNum == ENTITYNUM_WORLD)
	{
		return Steering::FAILED_BLOCKED_BY_WORLD;
	}
	else
	{
		Entity *traceEnt = G_GetEntity( trace.entityNum );
		if ( traceEnt && traceEnt->isSubclassOf( Actor) )
		{
			Actor *traceActor = static_cast<Actor *>(traceEnt);
			switch (traceActor->actortype)
			{
			case IS_ENEMY:
				return Steering::FAILED_BLOCKED_BY_ENEMY;
				break;
			case IS_CIVILIAN:
				return Steering::FAILED_BLOCKED_BY_CIVILIAN;
				break;
			case IS_FRIEND:
				return Steering::FAILED_BLOCKED_BY_FRIEND;
				break;
			case IS_TEAMMATE:
				return Steering::FAILED_BLOCKED_BY_TEAMMATE;
				break;
			default:
				break;
			}
		}
	}
	return Steering::ERROR;
}

const bool FollowPath::AtDestination(Actor &self) const
{


	if ((Vector::DistanceXY(GetGoalPosition(), self.origin)) <= GetRadius())
	{
		return true;
	}

	return false;
}

void FollowPath::SteerToCurrentNode(Actor &self )
{
   assert ( GetCurrentNode() != NULL );

	SetSteeringForceAndDesiredHeading
									( 
										self, 
										self.movementSubsystem->SteerTowardsPoint
										( 
											GetCurrentNode()->GetPosition(),			
											vec_zero,											
											self.movementSubsystem->getMoveDir(),		
											self.movementSubsystem->getMoveSpeed()		
										) 
									);
}

void FollowPath::SteerToGoal( Actor &self )
{
    Vector newSteeringForce(self.movementSubsystem->SteerTowardsPoint(
																GetGoalPosition(), 
																vec_zero, 
																self.movementSubsystem->getMoveDir(), 
																self.movementSubsystem->getMoveSpeed() 
																) );

	SetSteeringForceAndDesiredHeading( self, newSteeringForce );
}



void FollowNodePath::Archive( Archiver &arc )
{
	Class::Archive( arc );
	
	int i;
	int num;
	FollowNode *followNode;
	if ( arc.Saving() )
	{
		num = _nodes.NumObjects();
		
		arc.ArchiveInteger( &num );
		
		for( i = 1 ; i <= num ; i++ )
		{
			followNode = _nodes.ObjectAt( i );
			arc.ArchiveObject( followNode );
		}
	}
	else
	{
		arc.ArchiveInteger( &num );
		
		_nodes.ClearObjectList();
		_nodes.Resize( num );
		
		for( i = 1 ; i <= num ; i++ )
		{
			followNode = new FollowNode;
			_nodes.AddObject( followNode );
			arc.ArchiveObject( followNode );
		}
	}
	
	arc.ArchiveSafePointer( &_currentNode );
}


const Vector &FollowPathToEntity::GetGoalPosition( void ) const
{
	assert( _targetEntity != NULL );
	return _targetEntity->origin;
}

const float FollowPathToEntity::GetGoalRadius( void ) const
{
	return Vector( _targetEntity->mins ).lengthXY();
}
