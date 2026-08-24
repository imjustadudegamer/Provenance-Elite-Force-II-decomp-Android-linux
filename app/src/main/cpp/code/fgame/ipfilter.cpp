

#include "ipfilter.h"
#include "g_local.h"

typedef struct
   {
	unsigned	mask;
	unsigned	compare;
   } ipfilter_t;

#define	MAX_IPFILTERS	1024

ipfilter_t	ipfilters[ MAX_IPFILTERS ];
int			numipfilters;

static qboolean StringToFilter
   (
   const char *s,
   ipfilter_t *f
   )

   {
	char	num[ 128 ];
	int	i;
   int   j;
	byte	b[ 4 ];
	byte	m[ 4 ];

	for( i = 0; i < 4; i++ )
	   {
		b[ i ] = 0;
		m[ i ] = 0;
	   }

	for( i = 0; i < 4; i++ )
	   {
		if ( *s < '0' || *s > '9' )
		   {
         gi.SendServerCommand( 0, "print \"Bad filter address: %s\n\"", s );
			return false;
		   }

		j = 0;
		while( *s >= '0' && *s <= '9' )
		   {
			num[ j++ ] = *s++;
		   }

		num[ j ] = 0;
		b[ i ] = atoi( num );
		if ( b[ i ] != 0 )
         {
			m[ i ] = 255;
         }

		if ( !*s )
         {
			break;
         }

		s++;
	   }

	f->mask = *( unsigned * )m;
	f->compare = *( unsigned * )b;

	return true;
   }

qboolean SV_FilterPacket
   (
   const char *from
   )

   {
	int i;
	unsigned	in;
	byte m[ 4 ];
	const char *p;

	if ( !from )
		return false;


	i = 0;
	p = from;
	while( *p && i < 4 )
      {
		m[ i ] = 0;
		while( *p >= '0' && *p <= '9' )
         {
			m[ i ] = m[ i ] * 10 + ( *p - '0' );
			p++;
		   }

		if ( !*p || *p == ':' )
         {
			break;
         }

		i++;
      p++;
	   }

	in = *( unsigned * )m;
	for( i = 0; i < numipfilters; i++ )
      {
		if ( ( in & ipfilters[ i ].mask ) == ipfilters[ i ].compare )
         {
         return ( int )filterban->integer;
         }
      }

   return !( int )filterban->integer;
   }


void SVCmd_AddIP_f
   (
   void
   )

   {
	int i;

	if ( gi.argc() < 3 )
      {
      gi.SendServerCommand( 0, "print \"Usage: addip <ip-mask>\n\"" );
		return;
      }

	for( i = 0; i < numipfilters; i++ )
      {
		if ( ipfilters[ i ].compare == 0xffffffff )
         {
			break;
         }
      }

	if ( i == numipfilters )
	   {
		if ( numipfilters == MAX_IPFILTERS )
         {
         gi.SendServerCommand( 0, "print \"IP filter list is full\n\"" );
			return;
		   }
		numipfilters++;
	   }

	if ( !StringToFilter( gi.argv( 2 ), &ipfilters[ i ] ) )
      {
		ipfilters[ i ].compare = 0xffffffff;
      }
   }

void SVCmd_RemoveIP_f
   (
   void
   )

   {
	ipfilter_t	f;
	int			i;
   int         j;

	if ( gi.argc() < 3 )
      {
      gi.SendServerCommand( 0, "print \"Usage: sv removeip <ip-mask>\n\"" );
		return;
	   }

	if ( !StringToFilter( gi.argv( 2 ), &f ) )
      {
		return;
      }

	for( i = 0; i < numipfilters; i++ )
      {
		if ( ( ipfilters[ i ].mask == f.mask ) && ( ipfilters[ i ].compare == f.compare ) )
		   {
			for ( j = i + 1; j < numipfilters; j++ )
            {
				ipfilters[ j - 1 ] = ipfilters[ j ];
            }

			numipfilters--;
         gi.SendServerCommand( 0, "print \"Removed.\n\"" );
			return;
		   }
      }

   gi.SendServerCommand( 0, "print \"Didn't find %s.\n\"", gi.argv( 2 ) );
   }

void SVCmd_ListIP_f
   (
   void
   )

   {
	int   i;
	byte	b[ 4 ];

   gi.SendServerCommand( 0, "print \"Filter list:\n\"" );
	for( i = 0; i < numipfilters; i++ )
   	{
		*( unsigned * )b = ipfilters[ i ].compare;
      gi.SendServerCommand( 0, "print \"%3i.%3i.%3i.%3i\n\"", b[ 0 ], b[ 1 ], b[ 2 ], b[ 3 ] );
	   }
   }

void SVCmd_WriteIP_f
   (
   void
   )

   {
	FILE	 *f;
	char	 name[ MAX_OSPATH ];
	byte	 b[ 4 ];
	int	 i;

	Com_sprintf( name, sizeof( name ), "%s/listip.cfg", GAMEVERSION );
   gi.SendServerCommand( 0, "print \"Writing %s.\n\"", name );

	f = fopen( name, "wb" );
	if ( !f )
	   {
      gi.SendServerCommand( 0, "print \"Couldn't open %s.\n\"", name );
      return;
	   }

   fprintf( f, "set filterban %d\n", ( int )filterban->integer );

	for( i = 0; i < numipfilters; i++ )
	   {
		*( unsigned * )b = ipfilters[ i ].compare;
		fprintf( f, "sv addip %i.%i.%i.%i\n", b[ 0 ], b[ 1 ], b[ 2 ], b[ 3 ] );
	   }

	fclose( f );
   }

void G_ServerCommand
   (
   void
   )

   {
	const char *cmd;

	cmd = gi.argv(1);
	if ( Q_stricmp( cmd, "addip" ) == 0 )
      {
		SVCmd_AddIP_f();
      }
	else if ( Q_stricmp( cmd, "removeip" ) == 0 )
      {
		SVCmd_RemoveIP_f();
      }
	else if ( Q_stricmp( cmd, "listip" ) == 0 )
      {
		SVCmd_ListIP_f();
      }
	else if ( Q_stricmp( cmd, "writeip" ) == 0 )
      {
		SVCmd_WriteIP_f();
      }
	else
      {
      gi.SendServerCommand( 0, "print \"Unknown server command %s.\n\"", cmd );
      }
   }
