
#ifndef __SCRIPTVARIABLE_H__
#define __SCRIPTVARIABLE_H__

#include "g_local.h"
#include "container.h"
#include "listener.h"


class ScriptVariable : public Listener
	{
	private:
		str						name;
		float						value;
		str						string;
		Vector					vec;

		void						setString( const char *newvalue );

	public:
      CLASS_PROTOTYPE( ScriptVariable );

									ScriptVariable();

		qboolean					isVariableCommand( const char *name );

		void						setName( const char *newname );
		const char				*getName( void );

		const char				*stringValue( void );
		void						setStringValue( const char *newvalue );

		int						intValue( void );
		void						setIntValue( int newvalue );

		float						floatValue( void );
		void						setFloatValue( float newvalue );

      void                 setVectorValue( const Vector &newvector );
		Vector               vectorValue( void );

		void 						Var_Append( Event *ev );
		void 						Var_AppendInt( Event *ev );
		void 						Var_AppendFloat( Event *ev );
		void 						Var_String( Event *ev );
      void                 Var_RandomFloat(Event *ev );
      void                 Var_RandomInteger(Event *ev );
		void 						Var_Equals( Event *ev );
		void 						Var_PlusEquals( Event *ev );
		void 						Var_MinusEquals( Event *ev );
		void 						Var_TimesEquals( Event *ev );
		void 						Var_DivideEquals( Event *ev );
      void 						Var_Div( Event *ev );
      void 						Var_Mod( Event *ev );
		void						Var_GetCvar( Event *ev );
      void                 Var_Vector( Event *ev );
      void                 Var_Vector_Add( Event *ev );
      void                 Var_Vector_Subtract( Event *ev );
      void                 Var_Vector_Scale( Event *ev );
      void                 Var_Vector_Normalize( Event *ev );
      void                 Var_Vector_GetX( Event *ev );
      void                 Var_Vector_GetY( Event *ev );
      void                 Var_Vector_GetZ( Event *ev );
      void                 Var_Vector_SetX( Event *ev );
      void                 Var_Vector_SetY( Event *ev );
      void                 Var_Vector_SetZ( Event *ev );
      void                 Var_Vector_DotProduct( Event *ev );
      void                 Var_Vector_CrossProduct( Event *ev );
      void                 Var_Vector_Length( Event *ev );
      void                 Var_Angles_ToForward( Event *ev );
      void                 Var_Angles_ToLeft( Event *ev );
      void                 Var_Angles_ToUp( Event *ev );
      void                 Var_TargetOf( Event * ev );

      virtual void         Archive(	Archiver &arc ) override;
	};

inline void ScriptVariable::Archive
	(
	Archiver &arc
	)

	{
	arc.ArchiveString( &name );
	arc.ArchiveFloat( &value  );
	arc.ArchiveString( &string );
	arc.ArchiveVector( &vec );
	}

class ScriptVariableList : public Class
	{
	private:
		Container<ScriptVariable *> list;

	public:
      CLASS_PROTOTYPE( ScriptVariableList );

							ScriptVariableList();
							~ScriptVariableList();

		void				ClearList( void );
		void				AddVariable( ScriptVariable *var );
		ScriptVariable *CreateVariable( const char *name, float value );
		ScriptVariable *CreateVariable( const char *name, int value	);
		ScriptVariable *CreateVariable( const char *name, const char *text );
		ScriptVariable *CreateVariable( const char *name, const Entity *ent );
		ScriptVariable *CreateVariable( const char *name, const Vector &vec );
		void				RemoveVariable( ScriptVariable *var );
		qboolean			VariableExists( const char *name );
		ScriptVariable *GetVariable( const char *name );
		int				NumVariables( void );
		ScriptVariable *GetVariable( int num );
		ScriptVariable *SetVariable( const char *name, float value );
		ScriptVariable *SetVariable( const char *name, int value	);
		ScriptVariable *SetVariable( const char *name, const char *text );
		ScriptVariable *SetVariable( const char *name, const Entity *ent );
		ScriptVariable *SetVariable( const char *name, const Vector &vec );

      virtual void   Archive( Archiver &arc ) override;
    void   Print( void );  
	





public:
    void RemoveVariable( const char *name ); 
};

inline void ScriptVariableList::Archive
	(
	Archiver &arc
	)

	{
   int i;
	int num;
	ScriptVariable *var;

	Class::Archive( arc );

   if ( arc.Saving() )
      {
	   num = NumVariables();
      }
   else
      {
      ClearList();
      }
	arc.ArchiveInteger( &num );
	for( i = 1; i <= num; i++ )
		{
      if ( arc.Saving() )
         {
		   var = GetVariable( i );
         }
      else
         {
         var = new ScriptVariable;
         AddVariable( var );
         }
      arc.ArchiveObject( var );
		}
	}

typedef SafePtr<ScriptVariable> ScriptVariablePtr;

#endif 
