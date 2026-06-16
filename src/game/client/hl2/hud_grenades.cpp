//=============================================================================//
//
// Purpose: HUD element that shows grenade count
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include <vgui_controls/Panel.h>
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include "c_basehlplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_max_grenade;

//-----------------------------------------------------------------------------
// Purpose: Shows the flashlight icon
//-----------------------------------------------------------------------------
class CHudGrenades : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudGrenades, vgui::Panel );

public:
	CHudGrenades( const char *pElementName );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void OnThink( void );

protected:
	virtual void Paint();
	void UpdateGrenades();

private:
	
	CPanelAnimationVar( vgui::HFont, m_hFont, "Font", "WeaponIconsSmall" );
	CPanelAnimationVarAliasType( float, m_IconX, "icon_xpos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_IconY, "icon_ypos", "4", "proportional_float" );
	
	CPanelAnimationVarAliasType( float, m_flBarInsetX, "BarInsetX", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarInsetY, "BarInsetY", "18", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flBarWidth, "BarWidth", "28", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarHeight, "BarHeight", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkWidth, "BarChunkWidth", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkGap, "BarChunkGap", "2", "proportional_float" );

	int m_iGrenades;
};	

using namespace vgui;

#ifdef HL2_EPISODIC
DECLARE_HUDELEMENT( CHudGrenades );
#endif // HL2_EPISODIC

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudGrenades::CHudGrenades( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudGrenades" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

void CHudGrenades::OnThink( void )
{
	UpdateGrenades();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pScheme - 
//-----------------------------------------------------------------------------
void CHudGrenades::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CHudGrenades::UpdateGrenades()
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

	if (pPlayer)
    {
		if (pPlayer->Weapon_OwnsThisType( "weapon_frag" ))
			m_iGrenades = pPlayer->GetAmmoCount( "grenade" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the flashlight icon
//-----------------------------------------------------------------------------
void CHudGrenades::Paint()
{
#ifdef HL2_EPISODIC
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Only paint if we're using the new flashlight code
	if ( m_iGrenades == 0 || pPlayer->IsInAVehicle() )
	{
		SetPaintBackgroundEnabled( false );
		return;
	}

	SetPaintBackgroundEnabled( true );

	// get bar chunks
	int chunkCount = sk_max_grenade.GetInt();
	int enabledChunks = m_iGrenades;

	Color clrGrenades;
	clrGrenades = gHUD.m_clrNormal;
	clrGrenades[3] = 80;

    wchar_t pState = 'v';

	surface()->DrawSetTextFont( m_hFont );
	surface()->DrawSetTextColor( clrGrenades );
	surface()->DrawSetTextPos( m_IconX, m_IconY );
	surface()->DrawUnicodeChar( pState );

	// draw the suit power bar
	surface()->DrawSetColor( clrGrenades );
	int xpos = m_flBarInsetX, ypos = m_flBarInsetY;
	for (int i = 0; i < enabledChunks; i++)
	{
		surface()->DrawFilledRect( xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight );
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}
	
	// Be even more transparent than we already are
	clrGrenades[3] = clrGrenades[3] / 16;

	// draw the exhausted portion of the bar.
	surface()->DrawSetColor( clrGrenades );
	for (int i = enabledChunks; i < chunkCount; i++)
	{
		surface()->DrawFilledRect( xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight );
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}
#endif // HL2_EPISODIC
}
