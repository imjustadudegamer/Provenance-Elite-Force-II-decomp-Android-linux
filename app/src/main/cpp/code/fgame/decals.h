

#pragma once

#include "g_local.h"
#include "entity.h"

class Decal : public Entity
	{       
   private:
      str            shader;

	public:
      CLASS_PROTOTYPE( Decal );

	                  Decal();
      void           setDirection( Vector dir );
      void           setShader( str shader );
      void           setOrientation( str deg );
      void           setRadius( float rad );
      void Archive( Archiver &arc ) override;
	};

inline void Decal::Archive
	(
	Archiver &arc
	)
   {
   Entity::Archive( arc );

   arc.ArchiveString( &shader );
   if ( arc.Loading() )
      {
      setShader( shader );
      }
   }
