/*
===========================================================================
Copyright (C) 2026 the ProvenanceEF2 team

This file is part of ProvenanceEF2 source code.

ProvenanceEF2 source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

ProvenanceEF2 source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ProvenanceEF2 source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "g_local.h"
#include "archive.h"
#include "interpreter.h"
#include "program.h"
#include "compiler.h"
#include "scriptmaster.h"
#include "scriptthread.h"
#include "scriptslave.h"
#include "camera.h"
#include "worldspawn.h"
#include "entity.h"
#include "game.h"

const char *param_types[] = {"void", "string", "float", "vector", "entity", "function" };


extern Event EV_ScriptSlave_NewOrders;  


CLASS_DECLARATION(Listener, Interpreter, NULL) {
    {NULL, NULL}
};




void Interpreter::CleanupStack( int localstack_used_shadow1, int oldstacktop )
{
	int i;
	
	for( i = localstack_used_shadow1; i < oldstacktop; i++ ) 
	{
		if ( localstack_type[ i ] == ev_string )
			program->FreeString( localstack[ i ] );
		localstack_type[ i ] = ev_void;
	}
}


void Interpreter::DoMove( void )
{
}


void Interpreter::Execute( Event *e )
{
	eval_t	      *a;
	eval_t	      *b;
	eval_t	      *c;
	dstatement_t	*st;
	dfunction_t	   *newf;
	int		      runaway;
	int		      exitdepth;
	Listener       *obj;
	Entity         *ent;
	Event          *ev;
	char           text[ 128 ];
	int            i;
	int            n;
	TargetList     *list;
	int            oldstacktop;
	Interpreter    *newThread;
	int				stridx;
	
	if ( threadDying )
	{
		return;
	}
	
	runaway = 100000;
	
	updateList.ClearObjectList();
	
	exitdepth = 0;
	
	doneProcessing = false;
	while( !doneProcessing && !threadDying )
	{
		instruction_pointer++;
		
		st = &program->statements[ instruction_pointer ];
		if ( st->a < 0 )
		{
			a = ( eval_t * )&localstack[ localstack_base - st->a ];
		}
		else
		{
			a = ( eval_t * )&program->pr_globals[ st->a ];
		}
		if ( st->b < 0 )
		{
			b = ( eval_t * )&localstack[ localstack_base - st->b ];
		}
		else
		{
			b = ( eval_t * )&program->pr_globals[ st->b ];
		}
		if ( st->c < 0 )
		{
			c = ( eval_t * )&localstack[ localstack_base - st->c ];
		}
		else
		{
			c = ( eval_t * )&program->pr_globals[ st->c ];
		}
		
		if ( !--runaway )
		{
			RunError( "runaway loop error" );
		}
		
		pr_xfunction->profile++;
		pr_xstatement = instruction_pointer;
		
		if ( pr_trace )
		{
			PrintStatement( st );
		}
		
		switch( st->op )
		{
		case OP_ADD_F:
			c->_float = a->_float + b->_float;
			break;
			
		case OP_ADD_V:
            VectorAdd( a->vector, b->vector, c->vector );
			break;
			
		case OP_ADD_S:
            program->strings[ c->string ].s = program->strings[ a->string ].s + program->strings[ b->string ].s;
			break;
			
		case OP_ADD_FS:
            sprintf( text, "%g", a->_float );
            program->strings[ c->string ].s = text + program->strings[ b->string ].s;
			break;
			
		case OP_ADD_SF:
            sprintf( text, "%g", b->_float );
            program->strings[ c->string ].s = program->strings[ a->string ].s + text;
			break;
			
		case OP_ADD_VS:
			sprintf( text, "(%g %g %g)", a->vector[0], a->vector[1], a->vector[2]);
			program->strings[ c->string ].s = text + program->strings[ b->string ].s;
			break;
			
		case OP_ADD_SV:
			sprintf( text, "(%g %g %g)", b->vector[0], b->vector[1], b->vector[2]);
			program->strings[ c->string ].s = program->strings[ a->string ].s + text ;
			break ;
			
		case OP_SUB_F:
			c->_float = a->_float - b->_float;
			break;
			
		case OP_SUB_V:
            VectorSubtract( a->vector, b->vector, c->vector );
			break;
			
		case OP_MUL_F:
			c->_float = a->_float * b->_float;
			break;
			
		case OP_MUL_V:
            c->_float = DotProduct( a->vector, b->vector );
			break;
			
		case OP_MUL_FV:
            VectorScale( b->vector, a->_float, c->vector );
			break;
			
		case OP_MUL_VF:
            VectorScale( a->vector, b->_float, c->vector );
			break;
			
		case OP_DIV_F:
            if ( b->_float != 0.0f )
			{
				c->_float = a->_float / b->_float;
			}
            else
			{
				assert( 0 );
				c->_float = 0;
			}
			break;
			
		case OP_BITAND:
			c->_float = ( int )a->_float & ( int )b->_float;
			break;
			
		case OP_BITOR:
			c->_float = ( int )a->_float | ( int )b->_float;
			break;
			
		case OP_GE:
			c->_float = a->_float >= b->_float;
			break;
			
		case OP_LE:
			c->_float = a->_float <= b->_float;
			break;
			
		case OP_GT:
			c->_float = a->_float > b->_float;
			break;
			
		case OP_LT:
			c->_float = a->_float < b->_float;
			break;
			
		case OP_AND:
			c->_float = a->_float && b->_float;
			break;
			
		case OP_OR:
			c->_float = a->_float || b->_float;
			break;
			
		case OP_NOT_F:
			c->_float = !a->_float;
			break;
			
		case OP_NOT_V:
			c->_float = !a->vector[ 0 ] && !a->vector[ 1 ] && !a->vector[ 2 ];
			break;
			
		case OP_NOT_S:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
            c->_float = !&program->strings[ a->string ].s || ( program->strings[ a->string ].s.length() == 0 );
#pragma GCC diagnostic pop
			break;
			
		case OP_NOT_FNC:
			c->_float = !a->function;
			break;
			
		case OP_NOT_ENT:
			c->_float = !a->entity;
			break;
			
		case OP_EQ_F:
			c->_float = ( a->_float == b->_float );
			break;
			
		case OP_EQ_V:
			c->_float = ( a->vector[ 0 ] == b->vector[ 0 ] ) &&
						( a->vector[ 1 ] == b->vector[ 1 ] ) &&
						( a->vector[ 2 ] == b->vector[ 2 ] );
			break;
			
		case OP_EQ_S:
			c->_float = !program->strings[ a->string ].s.cmp( program->strings[ b->string ].s );
			break;
			
		case OP_EQ_E:
			c->_float = ( a->_int == b->_int );
			break;
			
		case OP_EQ_FNC:
			c->_float = ( a->function == b->function );
			break;
			
		case OP_NE_F:
			c->_float = ( a->_float != b->_float );
			break;
			
		case OP_NE_V:
			c->_float = ( a->vector[ 0 ] != b->vector[ 0 ] ) ||
						( a->vector[ 1 ] != b->vector[ 1 ] ) ||
						( a->vector[ 2 ] != b->vector[ 2 ] );
			break;
			
		case OP_NE_S:
			c->_float = program->strings[ a->string ].s.cmp( program->strings[ b->string ].s );
			break;
			
		case OP_NE_E:
			c->_float = ( a->_int != b->_int );
			break;
			
		case OP_NE_FNC:
			c->_float = ( a->function != b->function );
			break;
			
		case OP_UADD_F :
            b->_float += a->_float;
            break;
			
		case OP_USUB_F :
            b->_float -= a->_float;
            break;
			
		case OP_UMUL_F :
            b->_float *= a->_float;
            break;
			
		case OP_UDIV_F :
            if ( a->_float != 0.0f )
			{
				b->_float /= a->_float;
			}
            else
			{
				assert( 0 );
				b->_float = 0;
			}
            break;
			
		case OP_UOR_F :
			b->_float = ( int )b->_float | ( int )a->_float;
            break;
			
		case OP_UAND_F : 
            b->_float = ( int )b->_float & ( int )a->_float;
            break;
			
		case OP_UINC_F :
            a->_float++;
            break;
			
		case OP_UDEC_F :
            a->_float--;
            break;
			
		case OP_STORE_F:
		case OP_STORE_ENT:
		case OP_STORE_FNC:		
			b->_int = a->_int;
			break;
			
		case OP_STORE_S:
            if ( st->b == OFS_RETURN )
			{
				b->string = 0;
			}
            program->strings[ b->string ] = program->strings[ a->string ];
			break;
			
			
		case OP_STORE_V:
            VectorCopy( a->vector, b->vector );
			break;
			
		case OP_STORE_FTOS:
            if ( st->b == OFS_RETURN )
			{
				b->string = 0;
			}
            if ( a->_float == ( float )( int )a->_float )
			{
				sprintf( text, "%d", ( int )a->_float );
			}
            else
			{
				sprintf( text, "%f", a->_float );
			}
            program->strings[ b->string ].s = text;
			break;
			
		case OP_IFNOT:
			if ( !a->_int )
			{
				instruction_pointer += st->b - 1;
			}
			break;
			
		case OP_IF:
			if ( a->_int )
			{
				instruction_pointer += st->b - 1;
			}
			break;
			
		case OP_GOTO:
			instruction_pointer += st->a - 1;
			break;
			
		case OP_THREAD:
			
			if ( !a->function )
			{
				RunError( "NULL function" );
			}
			
			newf = &program->functions[ a->function ];
			
            newThread = Director.CreateThread( newf->s_name );
            newThread->ThreadCall( this, newf, st->b );
			
			program->pr_globals[ OFS_RETURN ]      = newThread->ThreadNum();
			program->pr_globals[ OFS_RETURN + 1 ]  = 0;
			program->pr_globals[ OFS_RETURN + 2 ]  = 0;
			
            oldstacktop = localstack_used;
            localstack_used -= st->b;
			if ( localstack_used < 0 )
			{
				RunError( "Execute: locals stack underflow\n" );
			}
			
			CleanupStack( localstack_used, oldstacktop );
			break;
			
		case OP_CALL:
			
			if ( !a->function )
			{
				RunError( "NULL function" );
			}
			
			newf = &program->functions[ a->function ];
			if ( newf->first_statement < 0 )
			{
				if ( ValidEvent( newf->eventnum ) )
				{
					ev = EventForFunction( newf, st->b );
					ProcessEvent( ev );
					if ( scr_printeventcalls->integer )
						gi.Printf( "event call - %s\n", ev->getName() );
				}
				
				oldstacktop = localstack_used;
				localstack_used -= st->b;
				if ( localstack_used < 0 )
				{
					RunError( "Execute: locals stack underflow\n" );
				}
				
				CleanupStack( localstack_used, oldstacktop );
				break;
			}
			
			instruction_pointer = EnterFunction( newf );
			break;
			
		case OP_OCALL:
			
			if ( !a->function )
			{
				RunError( "NULL function" );
			}
			
			newf = &program->functions[ a->function ];
			assert( newf->first_statement < 0 );
			
            if ( ( st->b >= RESERVED_OFS ) && ( st->b < OFS_END ) )
			{
				switch( st->b )
				{
				case OFS_CAM :
					obj = &CameraMan;
					break;
					
				default :
					obj = NULL;  
					RunError( "Execute: Invalid object call\n" );
					break;
				}
				
				if ( obj->ValidEvent( newf->eventnum ) )
				{
					ev = EventForFunction( newf, st->c );
					obj->ProcessEvent( ev );
					
					if ( scr_printeventcalls->integer )
						gi.Printf( "event call - %s\n", ev->getName() );
				}
			}
            else if ( b->entity > 0 )
			{
				ent = G_GetEntity( b->entity - 1 );
				if ( ent && ent->ValidEvent( newf->eventnum ) )
				{
					if ( !updateList.ObjectInList( ent->entnum ) )
					{
						updateList.AddObject( ent->entnum );
						
						ent->ProcessEvent( EV_ScriptSlave_NewOrders );
					}
					
					ev = EventForFunction( newf, st->c );
					ent->ProcessEvent( ev );
					
					if ( scr_printeventcalls->integer )							
						gi.Printf( "event call - %s.%s\n", ent->targetname.c_str(), ev->getName() );
				}
			}
            else if ( b->entity < 0 )
			{
				list = world->GetTargetList( -b->entity );
				
				if ( !list )
				{
					NullEntityError( st );
				}
				
				if ( list )
				{
					n = list->list.NumObjects();
					
					if ( !n )
					{
						NullEntityError( st );
					}
					
					if ( n )
					{
						ev = EventForFunction( newf, st->c );
						for( i = 1; i < n; i++ ) 
                        {
							ent = list->list.ObjectAt( i );
							if ( ent->ValidEvent( newf->eventnum ) )
							{
								if ( !updateList.ObjectInList( ent->entnum ) )
								{
									updateList.AddObject( ent->entnum );
									
									ent->ProcessEvent( EV_ScriptSlave_NewOrders );
								}
								
								ent->ProcessEvent( new Event( ev ) );
								
								if ( scr_printeventcalls->integer )										
									gi.Printf( "event call - %s.%s\n", ent->targetname.c_str(), ev->getName() );
							}
							else
							{
								ev->Error( "Entity '%s'(%d) cannot process event %s\n", 
									ent->targetname.c_str(), ent->entnum, ev->getName() );
							}
                        }
						
						ent = list->list.ObjectAt( i );
						if ( ent->ValidEvent( newf->eventnum ) )
                        {
							if ( !updateList.ObjectInList( ent->entnum ) )
							{
								updateList.AddObject( ent->entnum );
								
								ent->ProcessEvent( EV_ScriptSlave_NewOrders );
							}
							
							ent->ProcessEvent( ev );
							
							if ( scr_printeventcalls->integer )
								gi.Printf( "event call - %s.%s\n", ent->targetname.c_str(), ev->getName() );
                        }
						else
                        {
							ev->Error( "Entity '%s'(%d) cannot process event %s\n", 
								ent->targetname.c_str(), ent->entnum, ev->getName() );
							delete ev;
                        }
					}
				}
			}
			else
			{
				NullEntityError( st );
			}
			
            oldstacktop = localstack_used;
            localstack_used -= st->c;
			if ( localstack_used < 0 )
			{
				RunError( "Execute: locals stack underflow\n" );
			}
			
			CleanupStack( localstack_used, oldstacktop );
            break;
			
		case OP_PUSH_F :
			if ( localstack_used >= LOCALSTACK_SIZE )
			{
				RunError( "Execute: locals stack overflow\n" );
			}

			localstack_type[ localstack_used ] = ev_float;
			localstack[ localstack_used++ ] = a->_int;
			break;
			 
		case OP_PUSH_FTOS :
			if ( localstack_used >= LOCALSTACK_SIZE )
			{
				RunError( "Execute: locals stack overflow\n" );
			}

			if ( a->_float == ( float )( int )a->_float )
			{
				sprintf( text, "%d", ( int )a->_float );
			}
			else
			{
				sprintf( text, "%f", a->_float );
			}
			localstack_type[ localstack_used ] = ev_string;
			stridx = program->AllocString();
			program->strings[ stridx ].s = text;
			localstack[ localstack_used++ ] = ( int )stridx;
			break;
			 
		case OP_PUSH_ENT :
			if ( localstack_used >= LOCALSTACK_SIZE )
			{
				RunError( "Execute: locals stack overflow\n" );
			}

			localstack_type[ localstack_used ] = ev_entity;
			localstack[ localstack_used++ ] = a->_int;
			break;
			 
		case OP_PUSH_FNC :
			if ( localstack_used >= LOCALSTACK_SIZE )
			{
				RunError( "Execute: locals stack overflow\n" );
			}

			localstack_type[ localstack_used ] = ev_function;
			localstack[ localstack_used++ ] = a->_int;
			break;
			 
		case OP_PUSH_S :
			if ( localstack_used >= LOCALSTACK_SIZE )
			{
				RunError( "Execute: locals stack overflow\n" );
			}

			localstack_type[ localstack_used ] = ev_string;
			stridx = program->AllocString();
			program->strings[ stridx ].s = program->strings[ a->string ].s;
			localstack[ localstack_used++ ] = ( int )stridx;
			break;
			 
		case OP_PUSH_V :
			if ( localstack_used + 3 > LOCALSTACK_SIZE )
			{
				RunError( "Execute: locals stack overflow\n" );
			}

			localstack_type[ localstack_used ] = ev_float;
			localstack[ localstack_used++ ] = *( int * )&a->vector[ 0 ];

			localstack_type[ localstack_used ] = ev_float;
			localstack[ localstack_used++ ] = *( int * )&a->vector[ 1 ];

			localstack_type[ localstack_used ] = ev_float;
			localstack[ localstack_used++ ] = *( int * )&a->vector[ 2 ];
			break;
			 
		case OP_DONE:
		case OP_RETURN:
			if ( st->a < 0 )
			{
				if ( localstack_type[ localstack_base - st->a ] == ev_string )
				{
					program->pr_globals[ OFS_RETURN ] = 0;
				}
				else
				{
					program->pr_globals[ OFS_RETURN ]      = *(float *)&localstack[ localstack_base - st->a ];
					program->pr_globals[ OFS_RETURN + 1 ]  = *(float *)&localstack[ localstack_base - st->a + 1 ];
					program->pr_globals[ OFS_RETURN + 2 ]  = *(float *)&localstack[ localstack_base - st->a + 2 ];
				}
			}
			else if ( st->a != 0 )
			{
				program->pr_globals[ OFS_RETURN ]      = program->pr_globals[ st->a ];
				program->pr_globals[ OFS_RETURN + 1 ]  = program->pr_globals[ st->a + 1 ];
				program->pr_globals[ OFS_RETURN + 2 ]  = program->pr_globals[ st->a + 2 ];
			}
			 
			instruction_pointer = LeaveFunction();
			if ( pr_depth == exitdepth )
			{
				DoMove();
				Director.NotifyOtherThreads( threadNum );
				PostEvent( EV_Remove, 0.0f );
				doneProcessing = true;
				threadDying = true;
			}
			break;
			 
		default:
			RunError( "Bad opcode %i", st->op );
		}
	}
}


void Interpreter::PrintStatement( const dstatement_t *s )
{
	int i;
	
	if ( ( unsigned )s->op < ( sizeof( pr_opcodes ) / sizeof( pr_opcodes[ 0 ] ) ) )
	{
		Com_Printf( "%s ", pr_opcodes[ s->op ].name );
		
		i = strlen( pr_opcodes[ s->op ].name );
		for( ; i < 10; i++ )
		{
			Com_Printf( " " );
		}
	}
	
	if ( ( s->op == OP_IF ) || ( s->op == OP_IFNOT ) )
	{
		Com_Printf( "%sbranch %i", GlobalString( s->a ), s->b );
	}
	else if ( s->op == OP_GOTO )
	{
		Com_Printf( "branch %i", s->a );
	}
	else if ( ( unsigned )( s->op - OP_STORE_F ) < 6 )
	{
		Com_Printf( "%s", GlobalString( s->a ) );
		Com_Printf( "%s", GlobalStringNoContents( s->b ) );
	}
	else
	{
		if ( s->a )
		{
			Com_Printf( "%s", GlobalString( s->a ) );
		}
		if ( s->b )
		{
			Com_Printf( "%s", GlobalString( s->b ) );
		}
		if ( s->c )
		{
			Com_Printf( "%s", GlobalStringNoContents( s->c ) );
		}
	}
	
	Com_Printf( "\n" );
}


void Interpreter::Profile( void )
{
	dfunction_t	*f;
	dfunction_t *best;
	int			max;
	int			num;
	int			i;
	
	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for( i = 0; i < program->numfunctions; i++ )
		{
			f = &program->functions[ i ];
			if ( f->profile > max )
			{
				max = f->profile;
				best = f;
			}
		}
		   
		if ( best )
		{
			if ( num < 10 )
			{
				Com_Printf( "%7i %s\n", best->profile, best->s_name.c_str() );
			}
			   
			num++;
			best->profile = 0;
		}
	}
	while( best );
}


void Interpreter::Reset( void )
{
	CleanupStack( 0, localstack_used );
	
	pr_depth = 0;
	localstack_used = 0;
	localstack_base = 0;
	pr_trace = false;
	
	pr_xfunction = NULL;
	pr_xstatement = -1;
	
	threadDying			= false;
	doneProcessing		= true;
	
	memset( localstack, 0, sizeof( localstack ) );
	memset( pr_stack, 0, sizeof( pr_stack ) );
}


void Interpreter::RunError( const char *error, ... )
{
	va_list	argptr;
	char		string[ 1024 ];
	
	va_start( argptr, error );
	vsprintf( string, error, argptr );
	va_end( argptr );
	
	StackTrace();
	Com_Printf( "%s\n", string );
	
	pr_depth = 0;
	
	gi.Error( ERR_DROP, "Program error" );
}


void Interpreter::StackTrace( void )
{
	dfunction_t	*f;
	int			i;
	
	if ( pr_depth <= 0 )
	{
		Com_Printf( "<NO STACK>\n" );
		return;
	}
	
	pr_stack[ pr_depth ].f = pr_xfunction;
	for( i = pr_depth; i >= 0; i-- )
	{
		f = pr_stack[ i ].f;
		
		if ( !f )
		{
			Com_Printf( "<NO FUNCTION>\n" );
		}
		else
		{
			Com_Printf( "%12s : %s\n", f->s_file.c_str(), f->s_name.c_str() );
		}
	}
}

inline void Interpreter::NullEntityError(const dstatement_t *statement)
{
    str filename(program->filenames[statement->file - 1]);
    Com_Printf("===============================================\n\n");
    gi.WDPrintf("Null entity referenced in %s on line %d\n", filename.c_str(), statement->linenumber);
    Com_Printf("Stack Trace follows:\n");
    StackTrace();
    Com_Printf("===============================================\n\n");
}



void Interpreter::ThreadCall( const Interpreter *source, dfunction_t *newf, int args )
{
	int start;
	int i;
	
	if ( scr_printfunccalls->integer )
		gi.Printf( "thread call - %s (ignore previous and next func call)\n", newf->s_name.c_str() );		
	
	Reset();
	start = source->localstack_used - args;
	for( i = 0; i < args; i++ )
	{
		localstack_type[ i ] = source->localstack_type[ start + i ];
		switch( localstack_type[ i ] )
		{
		case ev_string :
			localstack[ i ] = program->AllocString();
			program->strings[ localstack[ i ] ].s = program->strings[ source->localstack[ start + i ] ].s;
            break;
			
		case ev_vector :
            localstack[ i + 0 ] = source->localstack[ start + i + 0 ];
            localstack[ i + 1 ] = source->localstack[ start + i + 1 ];
            localstack[ i + 2 ] = source->localstack[ start + i + 2 ];
			
            localstack_type[ i + 1 ] = ev_float;
            localstack_type[ i + 2 ] = ev_float;
			
            i += 2;
            break;
			
		default :
            localstack[ i ] = source->localstack[ start + i ];
            break;
		}
	}
	
	localstack_used = args;
	localstack_base = 0;
	
	instruction_pointer = EnterFunction( newf );
	
	CancelEventsOfType( EV_ScriptThread_Execute );
	PostEvent( EV_ScriptThread_Execute, 0.0f );
}




int Interpreter::EnterFunction( dfunction_t *f )
{
	int i;
	int c;
	int off;
	int stackpos;
	etype_t type;
	
	if ( pr_depth >= MAX_PRSTACK_DEPTH )
	{
		RunError( "stack overflow" );
	}
	
	if ( scr_printfunccalls->integer )
		gi.Printf( "func call - %s\n", f->s_name.c_str() );		
	
	pr_stack[ pr_depth ].s = pr_xstatement;
	pr_stack[ pr_depth ].f = pr_xfunction;
	pr_stack[ pr_depth ].stackbase = localstack_base;
	pr_depth++;
	
	c = f->locals - f->parm_total;
	assert( c >= 0 );
	
	if ( localstack_used + c > LOCALSTACK_SIZE )
	{
		RunError( "EnterFuncton: locals stack overflow\n" );
	}
	
	off = f->parm_start + f->parm_total;
	for( i = 0; i < c; i++ )
	{
		stackpos = localstack_used + i;
		
		type = program->pr_global_defs[ off + i ]->type->type;
		
		localstack_type[ stackpos ] = type;
		switch( localstack_type[ stackpos ] )
		{
		case ev_string :
			localstack[ stackpos ] = program->AllocString();
            break;
			
		case ev_vector :
            localstack[ stackpos + 0 ] = program->getInteger( off + i + 0 );
            localstack[ stackpos + 1 ] = program->getInteger( off + i + 1 );
            localstack[ stackpos + 2 ] = program->getInteger( off + i + 2 );
			
            localstack_type[ stackpos + 1 ] = ev_float;
            localstack_type[ stackpos + 2 ] = ev_float;
			
            i += 2;
            break;
			
		default :
            localstack[ stackpos ] = program->getInteger( off + i );
            break;
		}
	}
	
	localstack_used += c;
	localstack_base = localstack_used - f->locals - 1;
	
	pr_xfunction = f;
	
	return f->first_statement - 1;
}


qboolean Interpreter::Goto( const char *label )
{
	func_t func;
	
	func = program->findFunction( label );
	if ( ( func < 0 ) || ( func >= program->numfunctions ) )
	{
		gi.WDPrintf( "Can't find function '%s' in program\n", label );
		return false;
	}
	
	instruction_pointer = EnterFunction( &program->functions[ func ] );
	
	CancelEventsOfType( EV_ScriptThread_Execute );
	PostEvent( EV_ScriptThread_Execute, 0.0f );
	
	return true;
}


int Interpreter::LeaveFunction( void )
{
	int c;
	int oldstacktop;
	
	if ( pr_depth <= 0 )
	{
		gi.Error( ERR_DROP, "prog stack underflow" );
	}
	
	c = pr_xfunction->locals;
	oldstacktop = localstack_used;
	localstack_used -= c;
	if ( localstack_used < 0 )
	{
		RunError( "LeaveFunction: locals stack underflow\n" );
	}
	
	assert( localstack_used == ( localstack_base + 1 ) );
	
	CleanupStack( localstack_used, oldstacktop );
	
	pr_depth--;
	pr_xfunction = pr_stack[ pr_depth ].f;
	localstack_base = pr_stack[ pr_depth ].stackbase;
	
	return pr_stack[ pr_depth ].s;
}


qboolean Interpreter::Setup( int num, const char *label )
{
	Reset();
	
	threadNum = num;
	threadName = label;
	
	return Goto( label );
}


qboolean Interpreter::labelExists( const char *label )
{
	func_t func;
	
	func = program->findFunction( label );
	if ( func < 0 || ( func >= program->numfunctions ) )
	{
		return false;
	}
	
	return true;
}






Interpreter::Interpreter()
{
	localstack_used = 0;	
	localstack_base = 0;
	threadNum = 0;
	program = &::program;
	Reset();
}




Interpreter::~Interpreter()
{
	Director.NotifyOtherThreads( threadNum );
	Director.RemoveThread( threadNum );
	
	CleanupStack( 0, localstack_used );
}

Event *Interpreter::EventForFunction( const dfunction_t *func, int args )
{
	Event *ev;
	int i;
	eval_t *st;
	int pos;
	int start;
	
	ev = new Event( func->eventnum );
	ev->SetSource( EV_FROM_SCRIPT );
	ev->SetThread( ( CThread * )this );
	ev->SetLineNumber( program->statements[ instruction_pointer ].linenumber );
	
	start = localstack_used - args;
	for( i = 0, pos = 0; pos < args; i++ )
	{
		st = ( eval_t * )&localstack[ start + pos ];
		
		if (
			(func->parm_type[i] != localstack_type[start + pos]) &&
			(func->parm_type[i] != ev_vector) && 
			(localstack_type[start + pos] != ev_float)
			)
		{
			RunError	 ( "===============================\n"
				"Type mismatch on line %d. Func %s expected %s, got %s\n"
				"===============================\n", 
				program->statements[ instruction_pointer ].linenumber,
				func->s_name.c_str(),
				param_types[func->parm_type[i]],
				param_types[localstack_type[start + pos]] );
		}
		
		switch( func->parm_type[ i ] )
		{
		case ev_string :
            ev->AddString( program->strings[ st->string ].s );
            break;
			
		case ev_float :
            ev->AddFloat( st->_float );
            break;
			
		case ev_vector :
            ev->AddVector( Vector( st->vector ) );
            break;
			
		case ev_entity :
			if ( st->entity > 0 )
			{
				if ( st->entity >= game.maxentities )
				{
					gi.WDPrintf( "Bad entity number %d from line %d in the script\n", st->entity - 1, program->statements[ instruction_pointer ].linenumber );
				}
				else
				{
					ev->AddEntity( G_GetEntity( st->entity - 1 ) );
				}
			}
            else if ( st->entity < 0 )
			{
				TargetList *list;
				
				list = world->GetTargetList( -st->entity );
				if ( list )
				{
					ev->AddEntity( list->GetNextEntity( NULL ) );
				}
				else
				{
					ev->AddEntity( NULL );
				}
			}
            else
			{
				ev->AddEntity( NULL );
			}
            break;
            
		default:
            RunError( "Bad type on builtin call" );
		}
		
		pos += func->parm_size[ i ];
	}
	
	return ev;
}


const char *Interpreter::GlobalString( int ofs )
{

	return "";

}


const char *Interpreter::GlobalStringNoContents( int ofs )
{

	return "";

}
