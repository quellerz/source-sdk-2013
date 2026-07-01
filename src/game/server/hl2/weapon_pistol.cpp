//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Pistol - hand gun with two fire modes. 
//          Primary fire: Standard HL2 pistol
//          Secondary fire: Akimbo
//
// Quell:   FIXME #1: There is a bug when player has 0 ammo in right pistol's clip
//                    it can't shoot more than one bullet from left pistol.
//
//          FIXME #2: There is a bug that player reloads only left pistol
//                    when in secondary fire mode and if right pistol has ammo
//
//          FIXME #3: There is a bug that player can't reload at all in
//                    secondary fire mode when right pistol's clip is empty.
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

ConVar	pistol_use_new_accuracy( "pistol_use_new_accuracy", "1" );

//-----------------------------------------------------------------------------
// CWeaponPistol
//-----------------------------------------------------------------------------

class CWeaponPistol : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponPistol, CBaseHLCombatWeapon );

	CWeaponPistol(void);

	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	ItemPreFrame( void );
	void	ItemBusyFrame( void );
	void	PrimaryAttack( void );
    void    SecondaryAttack( void );
    void    LeftGunAttack( void );
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
		if ( m_bDualMode )
		{
			float ramp = RemapValClamped( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME, 0.0f, 1.0f ); 

			VectorLerp( VECTOR_CONE_8DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );
			return cone;
		}

		if ( pistol_use_new_accuracy.GetBool() )
		{
			float ramp = RemapValClamped( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME, 0.0f, 1.0f ); 

			// Standard Mode lerp
			VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_4DEGREES, ramp, cone );
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
		if ( m_bDualMode )
			return 0.20f;
		return 0.40f; 
	}

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;

    bool    m_bDualMode;
    bool    m_bFlip;

    int     m_iLeftClip;
};


IMPLEMENT_SERVERCLASS_ST(CWeaponPistol, DT_WeaponPistol)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_pistol, CWeaponPistol );
PRECACHE_WEAPON_REGISTER( weapon_pistol );

BEGIN_DATADESC( CWeaponPistol )

	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flLastAttackTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flAccuracyPenalty,		FIELD_FLOAT ), //NOTENOTE: This is NOT tracking game time
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),
    DEFINE_FIELD( m_iLeftClip,              FIELD_INTEGER ),

END_DATADESC()

acttable_t	CWeaponPistol::m_acttable[] = 
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


IMPLEMENT_ACTTABLE( CWeaponPistol );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponPistol::CWeaponPistol( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = 0.0f;

	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;

	m_bFiresUnderwater	= true;

    m_bDualMode         = false;
    m_bFlip             = false;

    m_iLeftClip         = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponPistol::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
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
void CWeaponPistol::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flSoonestPrimaryAttack	= gpGlobals->curtime + PISTOL_FASTEST_DRY_REFIRE_TIME;
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponPistol::PrimaryAttack( void )
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
	m_flSoonestPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, GetOwner() );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if( pOwner )
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pOwner->ViewPunchReset();
	}

    if (!m_bDualMode)
    {
        BaseClass::PrimaryAttack();
    }
    else
    {
        bool rightHasAmmo = (m_iClip1 > 0);
        bool leftHasAmmo  = (m_iLeftClip > 0);

        if (!rightHasAmmo && !leftHasAmmo)
        {
            Reload();
            return;
        }

        if (!m_bFlip)
        {
            // Right pistol's turn
            if (rightHasAmmo)
            {
                BaseClass::PrimaryAttack();
                m_bFlip = true;
            }
            else
                LeftGunAttack();
        }
        else
        {
            // Left pistol's turn
            if (leftHasAmmo)
            {
                LeftGunAttack();
                m_bFlip = false;
            }
            else
                BaseClass::PrimaryAttack();
        }
    }

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_flAccuracyPenalty += PISTOL_ACCURACY_SHOT_PENALTY_TIME;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pOwner, true, GetClassname() );
}

void CWeaponPistol::SecondaryAttack()
{
    bool bWasDual = m_bDualMode;
    m_bDualMode = !m_bDualMode;

    if (!bWasDual && m_bDualMode)
    {
        if (m_iLeftClip == 0)
        {
            int ammo = ToBasePlayer(GetOwner())->GetAmmoCount(m_iPrimaryAmmoType);
            int amount = MIN(GetMaxClip1(), ammo);
            m_iLeftClip = amount;
            ToBasePlayer(GetOwner())->RemoveAmmo(amount, m_iPrimaryAmmoType);
        }
    }

    WeaponSound( SPECIAL1 );

    SendWeaponAnim( ACT_VM_IDLE );

    m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
}

void CWeaponPistol::LeftGunAttack()
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 )
	{
		return;
	}

	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( GetSecondaryAttackActivity() );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FireBulletsInfo_t info;
	info.m_vecSrc	 = pPlayer->Weapon_ShootPosition( );

	info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems,
	// especially if the weapon we're firing has a really fast rate of fire.
	info.m_iShots = 0;
	float fireRate = GetFireRate();

	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		info.m_iShots++;
		if ( !fireRate )
			break;
	}

	// Make sure we don't fire more than the amount in the clip
	if ( UsesClipsForAmmo1() )
	{
        info.m_iShots = min(info.m_iShots, m_iLeftClip);
        m_iLeftClip -= info.m_iShots;
	}
	else
	{
		info.m_iShots = min( info.m_iShots, pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) );
		pPlayer->RemoveAmmo( info.m_iShots, m_iPrimaryAmmoType );
	}

	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 2;

#if !defined( CLIENT_DLL )
	// Fire the bullets
	info.m_vecSpread = pPlayer->GetAttackSpread( this );
#else
	//!!!HACKHACK - what does the client want this function for?
	info.m_vecSpread = GetActiveWeapon()->GetBulletSpread();
#endif // CLIENT_DLL

	pPlayer->FireBullets( info );

    if (!m_iLeftClip && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	//Add our view kick in
	AddViewKick();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::UpdatePenaltyTime( void )
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
void CWeaponPistol::ItemPreFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::ItemBusyFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponPistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;
    
    if ( pOwner->m_nButtons & IN_ATTACK2 )
    {
        if ( m_flNextSecondaryAttack <= gpGlobals->curtime )
        {
            SecondaryAttack();
            return;
        }
    }

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
    else if ( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack < gpGlobals->curtime ) )
    {
        if ( m_bDualMode )
        {
            if ( m_iClip1 <= 0 && m_iLeftClip <= 0 )
            {
                DryFire();
            }
        }
        else
        {
            if ( m_iClip1 <= 0 )
            {
                DryFire();
            }
        }
    }	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponPistol::GetPrimaryAttackActivity( void )
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
bool CWeaponPistol::Reload( void )
{
    CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

    if ( !pPlayer )
        return false;

    // In dual mode, don't automatically reload if the other pistol has ammo.
    if ( m_bDualMode && ( m_iClip1 == 0 ) && ( m_iLeftClip > 0 ) )
    {
        return false;
    }

    bool fRet = false;

    // Reload the right pistol normally
    if (!m_bDualMode || (m_iClip1 == 0 && m_iLeftClip == 0))
    {
        if (m_iClip1 < GetMaxClip1())
        {
            fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
        }
    } 

    // Reload left  pistol only in akimbo mode
    if ( m_bDualMode )
    {
        int reserve = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );

        int need = GetMaxClip1() - m_iLeftClip;

        int give = MIN( need, reserve );

        if ( give > 0 )
        {
            m_iLeftClip += give;
            pPlayer->RemoveAmmo( give, m_iPrimaryAmmoType );

            fRet = true;
        }
    }

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
void CWeaponPistol::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle	viewPunch;

	viewPunch.x = random->RandomFloat( 0.25f, 0.5f );
	viewPunch.y = random->RandomFloat( -.6f, .6f );
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch( viewPunch );
}
