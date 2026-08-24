
#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include "class.h"
#include "../corepp/container.h"
#include "../corepp/vector.h"
#include "../corepp/str.h"

#define	MAXTOKEN	256

typedef struct
	{
	qboolean		tokenready;
   int         offset;
	int			line;
	char			token[ MAXTOKEN ];
	} scriptmarker_t;

typedef struct
	{
	str				macroName;
	str				macroText;
	} macro;

class Script : public Class
	{
	protected:
		qboolean		tokenready;

		str			filename;
		const char	*script_p;
		const char	*end_p;
		Container<macro *> macrolist;

		int			line;
		char			token[ MAXTOKEN ];

		qboolean		releaseBuffer;

		qboolean		AtComment( void );
		void			CheckOverflow( void );

	public:
		const char	*buffer;
		int			length;

      CLASS_PROTOTYPE( Script );

		~Script();
		Script(const char* filename = 0);
		void			Close( void );
		const char *Filename( void );
		int			GetLineNumber( void );
		void			Reset( void );
		void			MarkPosition( scriptmarker_t *mark );
		void			RestorePosition( const scriptmarker_t *mark );
		qboolean		SkipToEOL( void );
		void			SkipWhiteSpace( qboolean crossline );
		void			SkipNonToken( qboolean crossline );
		qboolean		TokenAvailable( qboolean crossline );
		qboolean		CommentAvailable( qboolean crossline );
		void			UnGetToken( void );
		qboolean		AtString( qboolean crossline );
		qboolean		AtOpenParen( qboolean crossline );
		qboolean		AtCloseParen( qboolean crossline );
		qboolean		AtComma( qboolean crossline );
		const char		*GetToken( qboolean crossline );
		const char		*GetLine( qboolean crossline );
		const char		*GetRaw( void );
		const char		*GetString( qboolean crossline );
		qboolean		GetSpecific( const char *string );
		qboolean		GetBoolean( qboolean crossline );
		int				GetInteger( qboolean crossline );
		double			GetDouble( qboolean crossline );
		float			GetFloat( qboolean crossline );
		Vector			GetVector( qboolean crossline );
		int				LinesInFile( void );
		void			Parse( const char *data, int length, const char *name );
		void			LoadFile( const char *name );
		const char		*Token( void );

#ifdef GAME_DLL
		virtual void	Archive( Archiver &arc ) override;
#endif
	};

#ifdef GAME_DLL
inline void Script::Archive
	(
	Archiver &arc
	)
   {
   int pos;

   Class::Archive( arc );

   arc.ArchiveBoolean( &tokenready );

   arc.ArchiveString( &filename );
   if ( arc.Loading() )
      {
      LoadFile( filename.c_str() );
      }

   if ( arc.Saving() )
      {
      pos = script_p - buffer;
      }
   arc.ArchiveInteger( &pos );
   if ( arc.Loading() )
      {
      script_p = buffer + pos;
      }


   arc.ArchiveInteger( &line );
   arc.ArchiveRaw( token, sizeof( token ) );


   }
#endif 

#endif
