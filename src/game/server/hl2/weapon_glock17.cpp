//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Glock17 - hand gun with togglable rapid secondary fire mode
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	PISTOL_FASTEST_REFIRE_TIME		0.1f
#define	PISTOL_FASTEST_DRY_REFIRE_TIME	0.2f

#define	PISTOL_ACCURACY_SHOT_PENALTY_TIME		0.2f	// Applied amount of time each shot adds to the time we must recover from
#define	PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME	1.5f	// Maximum penalty to deal out

ConVar glock17_use_new_accuracy( "glock17_use_new_accuracy", "1" );

// Fire modes definitions
enum Glock17FireMode_e
{
	FIREMODE_STANDARD = 0,
	FIREMODE_RAPID,
};

//-----------------------------------------------------------------------------
// CWeaponGlock17
//-----------------------------------------------------------------------------

class CWeaponGlock17 : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponGlock17, CBaseHLCombatWeapon );

	CWeaponGlock17(void);

	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	ItemPreFrame( void );
	void	ItemBusyFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void ); // Added for fire mode toggle
	void	AddViewKick( void );
	void	DryFire( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void	UpdatePenaltyTime( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity( void );

	virtual bool Reload( void );

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;
			
		static Vector cone;

		// If in rapid mode, the gun is inherently much wider in spread
		if ( m_iFireMode == FIREMODE_RAPID )
		{
			float ramp = RemapValClamped( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME, 0.0f, 1.0f ); 

			VectorLerp( VECTOR_CONE_8DEGREES, VECTOR_CONE_8DEGREES, ramp, cone );
			return cone;
		}

		if ( glock17_use_new_accuracy.GetBool() )
		{
			float ramp = RemapValClamped( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME, 0.0f, 1.0f ); 

			// Standard Mode lerp
			VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );
		}
		else
		{
			cone = VECTOR_CONE_4DEGREES;
		}

		return cone;
	}
	
	virtual int	GetMinBurst() 
	{ 
		return 1; 
	}

	virtual int	GetMaxBurst() 
	{ 
		return 3; 
	}

	virtual float GetFireRate( void ) 
	{
		// Doubled fire rate when in rapid mode (0.25s vs 0.50s delay between shots for NPCs)
		if ( m_iFireMode == FIREMODE_RAPID )
			return 0.20f;
		return 0.40f; 
	}

	// Dynamic clip size handling based on weapon script definitions
	virtual int GetMaxClip1( void )
	{
		if ( m_iFireMode == FIREMODE_RAPID )
			return BaseClass::GetMaxClip1() * 2; // Double clip size

		return BaseClass::GetMaxClip1();
	}

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;
	int		m_iFireMode; // Tracks current fire mode
};


IMPLEMENT_SERVERCLASS_ST(CWeaponGlock17, DT_WeaponGlock17)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_glock17, CWeaponGlock17 );
PRECACHE_WEAPON_REGISTER( weapon_glock17 );

BEGIN_DATADESC( CWeaponGlock17 )

	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flLastAttackTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flAccuracyPenalty,		FIELD_FLOAT ), 
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),
	DEFINE_FIELD( m_iFireMode,				FIELD_INTEGER ), // Save/Restore fire mode state

END_DATADESC()

acttable_t	CWeaponGlock17::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};


IMPLEMENT_ACTTABLE( CWeaponGlock17 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponGlock17::CWeaponGlock17( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = 0.0f;
	m_iFireMode = FIREMODE_STANDARD; // Default to Standard

	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;

	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGlock17::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGlock17::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

			WeaponSound( SINGLE_NPC );
			pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
			pOperator->DoMuzzleFlash();
			m_iClip1 = m_iClip1 - 1;
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGlock17::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flSoonestPrimaryAttack	= gpGlobals->curtime + PISTOL_FASTEST_DRY_REFIRE_TIME;
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGlock17::PrimaryAttack( void )
{
	if ( ( gpGlobals->curtime - m_flLastAttackTime ) > 0.5f )
	{
		m_nNumShotsFired = 0;
	}
	else
	{
		m_nNumShotsFired++;
	}

	m_flLastAttackTime = gpGlobals->curtime;

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, GetOwner() );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if( pOwner )
	{
		pOwner->ViewPunchReset();
	}

	BaseClass::PrimaryAttack();

	// In rapid mode, build up penalty time faster
	if ( m_iFireMode == FIREMODE_RAPID )
	{
		m_flAccuracyPenalty += ( PISTOL_ACCURACY_SHOT_PENALTY_TIME * 1.5f );
	}
	else
	{
		m_flAccuracyPenalty += PISTOL_ACCURACY_SHOT_PENALTY_TIME;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pOwner, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: Toggles fire modes on Secondary Click (+attack2)
//-----------------------------------------------------------------------------
void CWeaponGlock17::SecondaryAttack( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	// Toggle fire mode state
	if ( m_iFireMode == FIREMODE_STANDARD )
	{
		m_iFireMode = FIREMODE_RAPID;
		WeaponSound( SPECIAL1 ); // Optional: Plays a weapon click/toggle sound if configured
	}
	else
	{
		m_iFireMode = FIREMODE_STANDARD;
		WeaponSound( SPECIAL2 ); // Optional: Plays a fallback weapon sound
	}

	// Prevent instant switching loop spam by setting a small cooldown
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGlock17::UpdatePenaltyTime( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	// Check our penalty time decay
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGlock17::ItemPreFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGlock17::ItemBusyFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponGlock17::ItemPostFrame( void )
{
	// Handle weapon fire mode toggling manually from buttons if needed, 
	// but BaseClass::ItemPostFrame invokes SecondaryAttack() automatically.
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	float flRefireDelay = PISTOL_FASTEST_REFIRE_TIME;
	if ( m_iFireMode == FIREMODE_RAPID )
	{
		flRefireDelay *= 0.5f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponGlock17::GetPrimaryAttackActivity( void )
{
	if ( m_nNumShotsFired < 1 )
		return ACT_VM_PRIMARYATTACK;

	if ( m_nNumShotsFired < 2 )
		return ACT_VM_RECOIL1;

	if ( m_nNumShotsFired < 3 )
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponGlock17::Reload( void )
{
	bool fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
		m_flAccuracyPenalty = 0.0f;
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGlock17::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle	viewPunch;

	// Kick more aggressively in rapid fire
	if ( m_iFireMode == FIREMODE_RAPID )
	{
		viewPunch.x = random->RandomFloat( 0.5f, 1.0f );
		viewPunch.y = random->RandomFloat( -1.2f, 1.2f );
	}
	else
	{
		viewPunch.x = random->RandomFloat( 0.25f, 0.5f );
		viewPunch.y = random->RandomFloat( -.6f, .6f );
	}
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch( viewPunch );
}
