
#ifndef __GOO_H__
#define __GOO_H__

#include "weapon.h"
#include "weaputils.h"

class GooProjectile : public Projectile
   {
   private:
      str               m_debrismodel;
      int               m_debriscount;

   public:
      CLASS_PROTOTYPE( GooProjectile );

                        GooProjectile();
      void              Explode( Event *ev ) override;
      void              SetDebrisModel( Event *ev );
      void              SetDebrisCount( Event *ev );
      void              Archive( Archiver &arc ) override;
   };

void GooProjectile::Archive
   (
   Archiver &arc
   )

   {
   Projectile::Archive( arc );
   arc.ArchiveString( &m_debrismodel );
   arc.ArchiveInteger( &m_debriscount );
   }


class GooDebris : public Projectile
   {
   private:
      float          nexttouch;

   public:
      CLASS_PROTOTYPE( GooDebris );

                     GooDebris();
      void           Touch( Event *ev ) override;
      void           Prethink( Event *ev );
      void           Archive( Archiver &arc ) override;  
   };

void GooDebris::Archive
   (
   Archiver &arc
   )

   {
   Projectile::Archive( arc );
   arc.ArchiveFloat( &nexttouch );
   }

#endif 
