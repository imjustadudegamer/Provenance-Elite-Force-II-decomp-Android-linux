
#ifndef __MP_MODECTF_HPP__
#define __MP_MODECTF_HPP__

#include "mp_modeTeamBase.hpp"

class MultiplayerPlayerCtfData
{
public:
	int					_hasFlag;
	EntityPtr			_carriedFlag;
	MultiplayerItem *	_flag;

	MultiplayerPlayerCtfData() { _hasFlag = false; }
	void				init( void ) { _hasFlag = false; }
};

class CtfFlag
{
public:
	MultiplayerItem	*		_tempFlag;
	float					_tempFlagTime;
	MultiplayerItem	*		_realFlag;
	str						_teamName;
};

class ModeCaptureTheFlag : public ModeTeamBase
	{
	private:
		static const float					_maxGuardingDist;
		static const float					_ctfCapturePoints;
		static const float					_maxDroppedFlagTime;

		static const int					_pointsForFlagReturn;
		static const int					_pointsForKillingFlagCarrier;
		static const int					_pointsForAssist;
		static const int					_pointsForDefense;
		static const int					_pointsForTakingTheFlag;
		static const int					_pointsForCapturingTheFlag;

		Team*                               _redTeam;
		Team*                               _blueTeam;

		Container<CtfFlag>					_flags;

		MultiplayerPlayerCtfData			*_playerCtfData;

		int									_flagCarrierIconIndex;

		int									_oneFlagTakenIconIndex;
		int									_oneFlagMissingIconIndex;
		int									_oneFlagInBaseIconIndex;

		int									_redFlagTakenIconIndex;
		int									_redFlagMissingIconIndex;
		int									_redFlagInBaseIconIndex;

		int									_blueFlagTakenIconIndex;
		int									_blueFlagMissingIconIndex;
		int									_blueFlagInBaseIconIndex;

	protected:

		void								grabTheFlag( Player *player, MultiplayerItem *item );
		void								score( Player *player );
		void								putFlagBack( Player *player );

		CtfFlag *							findFlag( MultiplayerItem *item );
		float								findNearestTeamFlagDist( const str &teamName, const Vector &position );
		float								findNearestTeamFlagCarrierDist( const str &teamName, const Vector &position );

		void								returnFlag( MultiplayerItem *item, Player *player );
		void								dropFlag( Player *player );
		void								attachFlag( Player *player, Entity *obj );

	public:
		CLASS_PROTOTYPE( ModeCaptureTheFlag );

											ModeCaptureTheFlag();
											~ModeCaptureTheFlag();

		 void					init( int maxPlayers ) override;

		 bool					shouldKeepItem( MultiplayerItem *item ) override;
		 void					itemKept( MultiplayerItem *item ) override;

		 void					RemovePlayer( Player *player ) override;

		 void					addPlayerToTeam( Player *player, Team *team ) override;

		void								playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath ) override;
		 void					itemTouched( Player *player, MultiplayerItem *item ) override;

		bool								isEndOfMatch( void ) override;

		virtual int							getStat( Player *player, int statNum, int value ) override;
		 int					getIcon( Player *player, int statNum, int value ) override;
		 int					getScoreIcon( Player *player, int index, int value ) override;

		 void					setupMultiplayerUI( Player *player ) override;

		 void					score( const Player *player ) override;

		 void					update( float frameTime ) override;

		 bool					checkGameType( const char *rule ) override;
		 bool					doesPlayerHaveItem( Player *player, const char *itemName ) override;

		 void					playerEventNotification( const char *eventName, const char *eventItemName, Player *eventPlayer ) override;

		 void					playerChangedModel( Player *player ) override;
	};

#endif 
