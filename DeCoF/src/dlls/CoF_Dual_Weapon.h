// Cry of Fear: the weapon entity of the dual wield mode (gDualWeapon)

#ifndef COF_DUAL_WEAPON_H
#define COF_DUAL_WEAPON_H

// the right hand weapon of the dual wield mode
class CDualWeaponRight : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	int m_iCurrentShot;
	void SetModel( CBasePlayer *pPlayer, int model );
	void SendWeaponAnim( CBasePlayer *pPlayer, int anim );
	bool HasDualWeapon( CBasePlayer *pPlayer );
	void WeaponThink( CBasePlayer *pPlayer );
	void SetDualBodyGroup( CBasePlayer *pPlayer, int group1, int group2 );
	int dualbody;
	int m_iID;
	CBasePlayer *m_pPlayer;
	float m_fEmptyClickTime;
	void EXPORT MeleeHitThinkBranch( void );
	void EXPORT MeleeHitThinkPunch( void );
	void EXPORT MeleeHitThinkSwitchblade( void );
	void EXPORT MeleeHitThinkSwitchblade2( void );
	void EXPORT MeleeHitThinkSwitchblade3( void );
	void EXPORT MeleeHitThinkNightStick( void );
	void EXPORT MeleeHitThinkNightStick2( void );
	void EXPORT MeleeHitThinkNightStick3( void );
	float m_flNextIdleTime;
	float m_flPrimaryAttackDelay;
	float m_flSecondaryAttackDelay;
	float m_flNextPrimaryAttack;
	float m_flNextSecondaryAttack;
	int shoot_loop_sound;
	int shoot_loop_sound_end;
	bool m_bShotLooping;
	int m_iSprintState;
	int m_iWeaponMode;
	int m_iThinkState;
	float m_flThinkDecay;
	float m_fIdleAnimation1Time;
	float m_fIdleAnimation2Time;
	string_t m_sModel;
	int m_iIdleAnimation;
	int m_iIdleAnimation2;
	void SetVariables( float primarydelay, float secondarydelay, int idleanimation );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

extern CDualWeaponRight gDualWeapon;

#endif // COF_DUAL_WEAPON_H
