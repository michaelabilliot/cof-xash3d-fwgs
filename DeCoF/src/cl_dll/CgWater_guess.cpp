// Cry of Fear Cg water (no build records the name of this file)

#include <string>
#include <list>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "cl_entity.h"
#include "studio_util.h"
#include "r_studioint.h"
#include "glmanager.h"
#include "gl_texloader.h"
#include "CgWater_guess.h"

extern engine_studio_api_t IEngineStudio;
extern int current_ext_texture_id;
extern GLint g_iWaterOldTexture_guess;

int iUnknown1054506C_guess;	// V_CalcNormalRefdef stores pparams->waterlevel here, 3 is under water
CCgWater_guess g_CgWater_guess;

CCgWater_guess::CCgWater_guess()
{
	m_bEnabled_guess = false;
}

void CCgWater_guess::DrawBlackQuad_guess( void )
{
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho( 0, 1, 1, 0, 0.1f, 100 );

	glDisable( GL_DEPTH_TEST );
	glCullFace( GL_NONE );
	glDisable( GL_CULL_FACE );
	glDisable( GL_TEXTURE_2D );
	glColor4f( 0, 0, 0, 1 );

	glBegin( GL_QUADS );
	glVertex3f( 0, 1, -1 );
	glVertex3f( 1, 1, -1 );
	glVertex3f( 1, 0, -1 );
	glVertex3f( 0, 0, -1 );
	glEnd();

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
}

void CCgWater_guess::EndShaders_guess( void )
{
	g_CgContext_guess.DisableFragmentProfile_guess();
	g_CgContext_guess.DisableVertexProfile_guess();
}

void CCgWater_guess::Init_guess( void )
{
	if ( IEngineStudio.IsHardware() != 1 )
		return;

	glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress( "glActiveTextureARB" );
	glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)wglGetProcAddress( "glMultiTexCoord2fARB" );

	m_bAllowTextureShader_guess = true;
	m_bEnabled_guess = true;
	m_bDrawn_guess = false;

	if ( !g_TextureManager_guess.IsHardware_guess() )
	{
		m_bEnabled_guess = false;
		return;
	}

	if ( !(int)gEngfuncs.pfnGetCvarFloat( "cg_water" ) )
	{
		gEngfuncs.Con_DPrintf( "Disabling CG Effects\n" );
		m_bEnabled_guess = false;
	}

	bool bTextureShader = true;
	if ( !isExtensionSupported_guess( "GL_NV_texture_shader", strdup( (const char *)glGetString( GL_EXTENSIONS ) ) ) )
		bTextureShader = false;

	if ( !m_bEnabled_guess )
		return;

	m_flTexWidth_guess = 2;
	while ( m_flTexWidth_guess < ScreenWidth )
		m_flTexWidth_guess *= 2;

	m_flTexHeight_guess = 2;
	while ( m_flTexHeight_guess < ScreenHeight )
		m_flTexHeight_guess *= 2;

	m_iSceneTexture_guess = 256;
	g_TextureManager_guess.CreateTexture_guess( m_flTexWidth_guess, m_flTexHeight_guess, &m_iSceneTexture_guess, GL_TEXTURE_2D, GL_RGB );
	g_TextureManager_guess.CreateTexture_guess( m_flTexWidth_guess, m_flTexHeight_guess, &m_iMaskTexture_guess, GL_TEXTURE_2D, GL_RGB );
	g_TextureManager_guess.CreateTexture_guess( 256, 256, &m_iOffsetTexture_guess, GL_TEXTURE_2D, GL_RGB );
	g_TextureManager_guess.CreateTexture_guess( m_flTexWidth_guess, m_flTexHeight_guess, &m_iDistortTexture_guess, GL_TEXTURE_2D, GL_RGB );

	unsigned char offsets[256][256][2];

	for ( int t = 0; t < 256; t++ )
	{
		for ( int s = 0; s < 256; s++ )
		{
			offsets[t][s][0] = s;
			offsets[t][s][1] = t;
		}
	}

	glBindTexture( GL_TEXTURE_2D, m_iOffsetTexture_guess );

	if ( bTextureShader )
		glTexImage2D( GL_TEXTURE_2D, 0, GL_DSDT8_NV, 256, 256, 0, GL_DSDT_NV, GL_UNSIGNED_BYTE, offsets );
	else
		glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 256, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, offsets );

	if ( !m_iNoiseTexture_guess )
		m_iNoiseTexture_guess = current_ext_texture_id++;
	CreateTexture( "textures/noise.tga", 1, m_iNoiseTexture_guess );

	m_pWaterVP_guess = g_CgContext_guess.AddProgram_guess( "water_vp.cg", CGTYPE_VERTEX_guess );

	if ( m_pWaterVP_guess )
	{
		m_pModelViewProj_guess = cgGetNamedParameter( m_pWaterVP_guess->program_guess, "ModelViewProj" );
		m_pScreen_guess = cgGetNamedParameter( m_pWaterVP_guess->program_guess, "flScreen" );
	}
	else
		gEngfuncs.Con_Printf( "Error on vertex\n" );

	const char *water, *black;

	if ( g_CgContext_guess.GetFragmentProfile_guess() == CG_PROFILE_FP20 )
	{
		black = "fp20_black_fp.cg";
		water = "fp20_water_fp.cg";
	}
	else
	{
		black = "black_fp.cg";
		water = "water_fp.cg";
	}

	m_pWaterFP_guess = g_CgContext_guess.AddProgram_guess( water, CGTYPE_FRAGMENT_guess );
	m_pBlackFP_guess = g_CgContext_guess.AddProgram_guess( black, CGTYPE_FRAGMENT_guess );
	m_pBlackVP_guess = g_CgContext_guess.AddProgram_guess( "black_vp.cg", CGTYPE_VERTEX_guess );
	m_pBlackModelViewProj_guess = cgGetNamedParameter( m_pBlackVP_guess->program_guess, "ModelViewProj" );

	if ( !m_pBlackFP_guess )
		gEngfuncs.Con_Printf( "Error Loading Black Shader\n" );

	if ( m_pWaterFP_guess )
		m_pTexOffset_guess = cgGetNamedParameter( m_pWaterFP_guess->program_guess, "TexOffset" );
	else
		gEngfuncs.Con_Printf( "Error on fragment\n" );

	glActiveTextureARB( GL_TEXTURE0_ARB );
}

void CCgWater_guess::PostDraw_guess( void )
{
	if ( !g_TextureManager_guess.IsHardware_guess() )
		return;

	glActiveTextureARB( GL_TEXTURE1_ARB );
	glDisable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, m_iSavedTexture1_guess );

	glActiveTextureARB( GL_TEXTURE2_ARB );
	glDisable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, m_iSavedTexture2_guess );

	glActiveTextureARB( GL_TEXTURE3_ARB );
	glDisable( GL_TEXTURE_2D );

	glActiveTextureARB( GL_TEXTURE0_ARB );
}

void CCgWater_guess::PreDraw_guess( void )
{
	if ( !g_TextureManager_guess.IsHardware_guess() )
		return;

	if ( !m_iSavedTexture1_guess || !m_iSavedTexture2_guess )
		return;

	glActiveTextureARB( GL_TEXTURE1_ARB );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &m_iSavedTexture1_guess );

	glActiveTextureARB( GL_TEXTURE2_ARB );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &m_iSavedTexture2_guess );

	glActiveTextureARB( GL_TEXTURE0_ARB );
}

void CCgWater_guess::Render_guess( void )
{
	if ( !m_bEnabled_guess )
		Init_guess();

	m_bDrawn_guess = true;

	if ( !m_bEnabled_guess )
		return;

	glGetIntegerv( GL_TEXTURE_BINDING_2D, &g_iWaterOldTexture_guess );

	glDisable( GL_BLEND );
	glEnable( GL_TEXTURE_2D );

	// the scene
	glBindTexture( GL_TEXTURE_2D, m_iSceneTexture_guess );
	glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, ScreenWidth, ScreenHeight );
	DrawBlackQuad_guess();

	// the water surfaces with the scrolling noise texture
	glDisable( GL_CULL_FACE );
	glDepthMask( GL_FALSE );
	glBindTexture( GL_TEXTURE_2D, m_iNoiseTexture_guess );
	glColor4f( 1, 1, 1, 1 );

	int i;
	cl_entity_t *ent;

	for ( i = 0; i < 1024; i++ )
	{
		ent = gEngfuncs.GetEntityByIndex( i );

		if ( !ent || ent->curstate.skin != CONTENTS_WATER )
			continue;

		float maxs1 = ent->curstate.maxs[1];
		float mins1 = ent->curstate.mins[1];
		float maxs0 = ent->curstate.maxs[0];
		float mins0 = ent->curstate.mins[0];

		float s = gEngfuncs.pfnGetCvarFloat( "cl_waterspeed_x" );
		s = gEngfuncs.GetClientTime() / s;
		float t = gEngfuncs.pfnGetCvarFloat( "cl_waterspeed_y" );
		t = cos( gEngfuncs.GetClientTime() ) / t;

		vec3_t mins = ent->curstate.mins;
		vec3_t maxs = ent->curstate.maxs;
		float z = maxs[2] + 1.0f;

		vec3_t v1, v2, v3, v4;
		v1[0] = mins[0];
		v1[1] = mins[1];
		v1[2] = z;
		v2[0] = mins[0];
		v2[1] = maxs[1];
		v2[2] = z;
		v3[0] = maxs[0];
		v3[1] = maxs[1];
		v3[2] = z;
		v4[0] = maxs[0];
		v4[1] = mins[1];
		v4[2] = z;

		glBegin( GL_QUADS );

		vec3_t tc1, tc2, tc3, tc4;
		float s0 = s + 0;
		float t0 = t + 0;
		float r0 = t0 + 0;
		tc1[0] = s0;
		tc1[1] = t0;
		tc1[2] = r0;
		glTexCoord2fv( tc1 );
		glVertex3fv( v1 );

		float t1 = ( maxs1 - mins1 ) * ( 1.0f / 128 ) + t;
		tc2[0] = s0;
		tc2[1] = t1;
		tc2[2] = r0;
		glTexCoord2fv( tc2 );
		glVertex3fv( v2 );

		float s1 = ( maxs0 - mins0 ) * ( 1.0f / 128 ) + s;
		tc3[0] = s1;
		tc3[1] = t1;
		tc3[2] = r0;
		glTexCoord2fv( tc3 );
		glVertex3fv( v3 );

		tc4[0] = s1;
		tc4[1] = t0;
		tc4[2] = r0;
		glTexCoord2fv( tc4 );
		glVertex3fv( v4 );

		glEnd();
	}

	// distorted copy
	glEnable( GL_CULL_FACE );
	glDepthMask( GL_TRUE );
	glBindTexture( GL_TEXTURE_2D, m_iDistortTexture_guess );
	glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, ScreenWidth, ScreenHeight );
	DrawBlackQuad_guess();

	// the water mask
	glDisable( GL_TEXTURE_2D );
	glColor4f( 1, 1, 1, 1 );
	glDisable( GL_CULL_FACE );

	for ( i = 0; i < 1024; i++ )
	{
		ent = gEngfuncs.GetEntityByIndex( i );

		if ( !ent || ent->curstate.skin != CONTENTS_WATER )
			continue;

		vec3_t mins = ent->curstate.mins;
		vec3_t maxs = ent->curstate.maxs;
		vec3_t v1, v2, v3, v4;
		v1[0] = mins[0];
		v1[1] = mins[1];
		v1[2] = maxs[2];
		v2[0] = mins[0];
		v2[1] = maxs[1];
		v2[2] = maxs[2];
		v3[0] = maxs[0];
		v3[1] = maxs[1];
		v3[2] = maxs[2];
		v4[0] = maxs[0];
		v4[1] = mins[1];
		v4[2] = maxs[2];

		glBegin( GL_QUADS );
		glVertex3fv( v1 );
		glVertex3fv( v2 );
		glVertex3fv( v3 );
		glVertex3fv( v4 );
		glEnd();
	}

	glEnable( GL_CULL_FACE );
	glDepthMask( GL_FALSE );
	glBindTexture( GL_TEXTURE_2D, m_iMaskTexture_guess );
	glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, ScreenWidth, ScreenHeight );
	DrawBlackQuad_guess();

	// put the scene back
	glBindTexture( GL_TEXTURE_2D, m_iSceneTexture_guess );
	ScreenQuad_guess();

	g_CgContext_guess.EnableVertexProfile_guess();
	g_CgContext_guess.EnableFragmentProfile_guess();

	if ( m_pWaterVP_guess )
		g_CgContext_guess.BindProgram_guess( m_pWaterVP_guess );
	else
		gEngfuncs.Con_Printf( "Error on Vertex\n" );

	if ( m_pWaterFP_guess )
		g_CgContext_guess.BindProgram_guess( m_pWaterFP_guess );
	else
		gEngfuncs.Con_Printf( "Error on fragment\n" );

	glActiveTextureARB( GL_TEXTURE0_ARB );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, m_iDistortTexture_guess );

	glActiveTextureARB( GL_TEXTURE1_ARB );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, m_iOffsetTexture_guess );

	glActiveTextureARB( GL_TEXTURE2_ARB );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, m_iSceneTexture_guess );

	glActiveTextureARB( GL_TEXTURE3_ARB );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, m_iMaskTexture_guess );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho( 0, 1, 1, 0, 0.1f, 4096 );

	cgGLSetStateMatrixParameter( m_pModelViewProj_guess, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY );

	float offset[4] = { 0, 0.05f, 0, 0.05f };
	cgGLSetParameter4fv( m_pTexOffset_guess, offset );
	cgGLSetParameter1f( m_pScreen_guess, 0.965f );

	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glColor4f( 1, 1, 1, 1 );

	glBegin( GL_QUADS );
	glTexCoord2f( 0, 0 );
	glVertex3f( 0, 1, -1 );
	glTexCoord2f( ScreenWidth / m_flTexWidth_guess, 0 );
	glVertex3f( 1, 1, -1 );
	glTexCoord2f( ScreenWidth / m_flTexWidth_guess, ScreenHeight / m_flTexHeight_guess );
	glVertex3f( 1, 0, -1 );
	glTexCoord2f( 0, ScreenHeight / m_flTexHeight_guess );
	glVertex3f( 0, 0, -1 );
	glEnd();

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glActiveTextureARB( GL_TEXTURE1_ARB );
	glDisable( GL_TEXTURE_2D );
	glActiveTextureARB( GL_TEXTURE2_ARB );
	glDisable( GL_TEXTURE_2D );
	glActiveTextureARB( GL_TEXTURE3_ARB );
	glDisable( GL_TEXTURE_2D );
	glActiveTextureARB( GL_TEXTURE0_ARB );

	g_CgContext_guess.DisableFragmentProfile_guess();
	g_CgContext_guess.DisableVertexProfile_guess();

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glBindTexture( GL_TEXTURE_2D, g_iWaterOldTexture_guess );
}

void CCgWater_guess::ScreenQuad_guess( void )
{
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho( 0, 1, 1, 0, 0.1f, 100 );

	glDisable( GL_DEPTH_TEST );
	glCullFace( GL_NONE );
	glDisable( GL_CULL_FACE );
	glColor4f( 1, 1, 1, 1 );

	glBegin( GL_QUADS );
	glTexCoord2f( 0, 0 );
	glVertex3f( 0, 1, -1 );
	glTexCoord2f( ScreenWidth / m_flTexWidth_guess, 0 );
	glVertex3f( 1, 1, -1 );
	glTexCoord2f( ScreenWidth / m_flTexWidth_guess, ScreenHeight / m_flTexHeight_guess );
	glVertex3f( 1, 0, -1 );
	glTexCoord2f( 0, ScreenHeight / m_flTexHeight_guess );
	glVertex3f( 0, 0, -1 );
	glEnd();

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
}

void CCgWater_guess::SetBlackShader_guess( void )
{
	g_CgContext_guess.EnableFragmentProfile_guess();
	g_CgContext_guess.EnableVertexProfile_guess();

	if ( m_pBlackFP_guess )
		g_CgContext_guess.BindProgram_guess( m_pBlackFP_guess );

	if ( m_pBlackVP_guess )
		g_CgContext_guess.BindProgram_guess( m_pBlackVP_guess );

	cgGLSetStateMatrixParameter( m_pBlackModelViewProj_guess, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY );
}

void CCgWater_guess::UpdateEntities_guess( void )
{
	if ( !(int)gEngfuncs.pfnGetCvarFloat( "cg_water" ) )
	{
		m_bEnabled_guess = false;
		return;
	}

	if ( !m_bEnabled_guess )
		return;

	for ( int i = 0; i < 1024; i++ )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( i );

		if ( !ent || ent->curstate.skin != CONTENTS_WATER )
			continue;

		ent->baseline.effects = EF_NODRAW;
		ent->baseline.renderamt = 0;
		ent->baseline.rendermode = kRenderTransAdd;
		ent->prevstate.effects = EF_NODRAW;
		ent->prevstate.renderamt = 0;
		ent->prevstate.rendermode = kRenderTransAdd;
		ent->curstate.effects = EF_NODRAW;
		ent->curstate.renderamt = 0;
		ent->curstate.rendermode = kRenderTransAdd;
	}
}
