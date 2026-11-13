/***
*
*	Cry of Fear: the doctor boss (CDoctorBoss), declared in cof_doctorboss.h
*	(1.55 PDB; members and methods in PDB order).
*
****/

#ifndef COF_DOCTORBOSS_H
#define COF_DOCTORBOSS_H

class CDoctorBoss : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SetYawSpeed( void );
	virtual int Classify( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void MonsterThink( void );
	CBasePlayer *pPlayer;
	void HurtSound( void );
	int IsAimingOrShooting( void );
	virtual void SendInitMessage( CBasePlayer *player );
	bool m_bKilled;
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	Activity GetActivity( void ) { return m_Activity; }
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int m_iCurrentBox;
	int m_iDesiredBox;
	float m_flSequenceTime;
	float m_flNextAIThink;
	float m_flNextShoot;
	bool m_bWaitingToReload;
	float m_flReloadTime;
	void SetDoctorAnimation( char *szSequence );
	Activity GetAct( char *szSequence );
	int m_iBrassShell;
	EHANDLE m_pDamageHelper;
	int m_iClip;
	int m_iGunType;
	int m_iActivity;
	bool m_bJustTakenDamage;
	void EXPORT DecideWhatToDo( void );
};

#endif // COF_DOCTORBOSS_H
