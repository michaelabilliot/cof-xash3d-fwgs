
#include "dx_local.h"
#include "dx_studio_shader.h"

constexpr float DX_STUDIO_SHADE_LAMBERT = 1.4953241f;

#ifndef MAXSTUDIOBONES
#define MAXSTUDIOBONES 128
#endif

#ifndef MAXSTUDIOCONTROLLERS
#define MAXSTUDIOCONTROLLERS 8
#endif

#ifndef DEG2RAD
#define DEG2RAD( x ) ( (x) * ( (float)M_PI / 180.0f ) )
#endif

static void QuaternionMultiply( const float a[4], const float b[4], float out[4] )
{
	float r[4];
	r[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
	r[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	r[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
	r[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
	out[0] = r[0]; out[1] = r[1]; out[2] = r[2]; out[3] = r[3];
}

static void AngleQuaternion( const float angles[3], float q[4] )
{
	float qx[4], qy[4], qz[4], tmp[4];

	qx[0] = sinf( angles[0] * 0.5f ); qx[1] = 0.0f; qx[2] = 0.0f; qx[3] = cosf( angles[0] * 0.5f );
	qy[0] = 0.0f; qy[1] = sinf( angles[1] * 0.5f ); qy[2] = 0.0f; qy[3] = cosf( angles[1] * 0.5f );
	qz[0] = 0.0f; qz[1] = 0.0f; qz[2] = sinf( angles[2] * 0.5f ); qz[3] = cosf( angles[2] * 0.5f );

	QuaternionMultiply( qz, qy, tmp );
	QuaternionMultiply( tmp, qx, q );
}

static void QuaternionSlerp( const float p[4], const float q[4], float t, float qt[4] )
{
	float qq[4] = { q[0], q[1], q[2], q[3] };
	float cosOmega = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if( cosOmega < 0.0f )
	{
		cosOmega = -cosOmega;
		qq[0] = -qq[0]; qq[1] = -qq[1]; qq[2] = -qq[2]; qq[3] = -qq[3];
	}

	if( cosOmega > 0.9995f )
	{
		qt[0] = p[0] + t * ( qq[0] - p[0] );
		qt[1] = p[1] + t * ( qq[1] - p[1] );
		qt[2] = p[2] + t * ( qq[2] - p[2] );
		qt[3] = p[3] + t * ( qq[3] - p[3] );

		const float lengthSq = qt[0] * qt[0] + qt[1] * qt[1] +
			qt[2] * qt[2] + qt[3] * qt[3];
		if( lengthSq > 1e-12f )
		{
			const float inverseLength = 1.0f / sqrtf( lengthSq );
			qt[0] *= inverseLength;
			qt[1] *= inverseLength;
			qt[2] *= inverseLength;
			qt[3] *= inverseLength;
		}
		return;
	}

	cosOmega = std::max( -1.0f, std::min( 1.0f, cosOmega ) );
	const float omega = acosf( cosOmega );
	const float sinOmega = sinf( omega );
	const float sclp = sinf( ( 1.0f - t ) * omega ) / sinOmega;
	const float sclq = sinf( t * omega ) / sinOmega;

	qt[0] = sclp * p[0] + sclq * qq[0];
	qt[1] = sclp * p[1] + sclq * qq[1];
	qt[2] = sclp * p[2] + sclq * qq[2];
	qt[3] = sclp * p[3] + sclq * qq[3];
}

static void QuaternionMatrix( const float q[4], const float pos[3], BoneMat34 mat )
{
	mat[0][0] = 1.0f - 2.0f * q[1] * q[1] - 2.0f * q[2] * q[2];
	mat[1][0] = 2.0f * q[0] * q[1] + 2.0f * q[3] * q[2];
	mat[2][0] = 2.0f * q[0] * q[2] - 2.0f * q[3] * q[1];

	mat[0][1] = 2.0f * q[0] * q[1] - 2.0f * q[3] * q[2];
	mat[1][1] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[2] * q[2];
	mat[2][1] = 2.0f * q[1] * q[2] + 2.0f * q[3] * q[0];

	mat[0][2] = 2.0f * q[0] * q[2] + 2.0f * q[3] * q[1];
	mat[1][2] = 2.0f * q[1] * q[2] - 2.0f * q[3] * q[0];
	mat[2][2] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[1] * q[1];

	mat[0][3] = pos[0];
	mat[1][3] = pos[1];
	mat[2][3] = pos[2];
}

static void ConcatTransforms( const BoneMat34 a, const BoneMat34 b, BoneMat34 out )
{
	for( int i = 0; i < 3; ++i )
	{
		out[i][0] = a[i][0] * b[0][0] + a[i][1] * b[1][0] + a[i][2] * b[2][0];
		out[i][1] = a[i][0] * b[0][1] + a[i][1] * b[1][1] + a[i][2] * b[2][1];
		out[i][2] = a[i][0] * b[0][2] + a[i][1] * b[1][2] + a[i][2] * b[2][2];
		out[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3];
	}
}

static void VectorTransform( const float in[3], const BoneMat34 mat, float out[3] )
{
	out[0] = in[0] * mat[0][0] + in[1] * mat[0][1] + in[2] * mat[0][2] + mat[0][3];
	out[1] = in[0] * mat[1][0] + in[1] * mat[1][1] + in[2] * mat[1][2] + mat[1][3];
	out[2] = in[0] * mat[2][0] + in[1] * mat[2][1] + in[2] * mat[2][2] + mat[2][3];
}

static void StudioGetAnimSpan( mstudioanimvalue_t *panimvalue, int frame, float scale, float baseValue,
								float *outA, float *outB )
{
	int k = frame;

	while( panimvalue->num.total <= k )
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;
	}

	short rawA, rawB;

	if( panimvalue->num.valid > k )
	{
		rawA = panimvalue[k + 1].value;

		if( panimvalue->num.valid > k + 1 )
			rawB = panimvalue[k + 2].value;
		else
			rawB = ( panimvalue->num.total > k + 1 ) ? rawA : panimvalue[panimvalue->num.valid + 2].value;
	}
	else
	{
		rawA = panimvalue[panimvalue->num.valid].value;
		rawB = ( panimvalue->num.total > k + 1 ) ? rawA : panimvalue[panimvalue->num.valid + 2].value;
	}

	*outA = baseValue + rawA * scale;
	*outB = baseValue + rawB * scale;
}

static float StudioControllerInterpolant( const cl_entity_s *ent, float renderTime )
{

	if( R_NoInterpolationEnabled( ) )
		return 1.0f;

	float dadt = 1.0f;

	if( ent && ent->curstate.animtime >= ent->latched.prevanimtime + 0.01f )
	{
		dadt = ( renderTime - ent->curstate.animtime ) / 0.1f;
		if( dadt > 2.0f )
			dadt = 2.0f;
	}

	return dadt;
}

static void StudioCalcBoneAdj( studiohdr_t *hdr, const cl_entity_s *ent,
	float renderTime, float adj[MAXSTUDIOCONTROLLERS] )
{
	byte zeroController[4] = { 0, 0, 0, 0 };
	const byte *currentController = ent ? ent->curstate.controller : zeroController;
	const byte *previousController = ent ? ent->latched.prevcontroller : zeroController;
	const byte mouthOpen = ent ? ent->mouth.mouthopen : 0;
	const float dadt = StudioControllerInterpolant( ent, renderTime );
	mstudiobonecontroller_t *controllers;

	for( int i = 0; i < MAXSTUDIOCONTROLLERS; ++i )
		adj[i] = 0.0f;

	if( !hdr || hdr->numbonecontrollers <= 0 )
		return;

	controllers = (mstudiobonecontroller_t *)( (byte *)hdr + hdr->bonecontrollerindex );

	for( int j = 0; j < hdr->numbonecontrollers && j < MAXSTUDIOCONTROLLERS; ++j )
	{
		const int controller = controllers[j].index;
		float value = 0.0f;

		if( controller == STUDIO_MOUTH )
		{

			float t = (float)mouthOpen / 64.0f;
			if( t < 0.0f ) t = 0.0f;
			if( t > 1.0f ) t = 1.0f;
			value = ( 1.0f - t ) * controllers[j].start + t * controllers[j].end;
		}
		else if( controller >= 0 && controller < 4 )
		{
			if( controllers[j].type & STUDIO_RLOOP )
			{
				const int current = currentController[controller];
				const int previous = previousController[controller];

				if( abs( current - previous ) > 128 )
				{
					const int a = ( current + 128 ) & 255;
					const int b = ( previous + 128 ) & 255;
					value = ( ( a * dadt ) + ( b * ( 1.0f - dadt ) ) - 128 ) *
						( 360.0f / 256.0f ) + controllers[j].start;
				}
				else
				{
					value = ( current * dadt + previous * ( 1.0f - dadt ) ) *
						( 360.0f / 256.0f ) + controllers[j].start;
				}
			}
			else
			{
				float t = ( currentController[controller] * dadt +
					previousController[controller] * ( 1.0f - dadt ) ) / 255.0f;
				if( t < 0.0f ) t = 0.0f;
				if( t > 1.0f ) t = 1.0f;
				value = ( 1.0f - t ) * controllers[j].start + t * controllers[j].end;
			}
		}
		else
		{
			continue;
		}

		switch( controllers[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = DEG2RAD( value );
			break;

		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

static void StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone,
	mstudioanim_t *panim, const float adj[MAXSTUDIOCONTROLLERS], float pos[3] )
{
	for( int j = 0; j < 3; j++ )
	{
		if( panim->offset[j] == 0 )
		{
			pos[j] = pbone->value[j];
		}
		else
		{
			mstudioanimvalue_t *pv = (mstudioanimvalue_t *)( (byte *)panim + panim->offset[j] );
			float a, b;
			StudioGetAnimSpan( pv, frame, pbone->scale[j], pbone->value[j], &a, &b );
			pos[j] = a * ( 1.0f - s ) + b * s;
		}

		const int controller = pbone->bonecontroller[j];
		if( controller >= 0 && controller < MAXSTUDIOCONTROLLERS )
			pos[j] += adj[controller];
	}
}

static void StudioCalcBoneQuaternion( int frame, float s, mstudiobone_t *pbone,
	mstudioanim_t *panim, const float adj[MAXSTUDIOCONTROLLERS], float q[4] )
{
	float angle1[3], angle2[3];

	for( int j = 0; j < 3; j++ )
	{
		if( panim->offset[j + 3] == 0 )
		{
			angle1[j] = angle2[j] = pbone->value[j + 3];
		}
		else
		{
			mstudioanimvalue_t *pv = (mstudioanimvalue_t *)( (byte *)panim + panim->offset[j + 3] );
			StudioGetAnimSpan( pv, frame, pbone->scale[j + 3], pbone->value[j + 3], &angle1[j], &angle2[j] );
		}

		const int controller = pbone->bonecontroller[j + 3];
		if( controller >= 0 && controller < MAXSTUDIOCONTROLLERS )
		{
			angle1[j] += adj[controller];
			angle2[j] += adj[controller];
		}
	}

	if( angle1[0] == angle2[0] && angle1[1] == angle2[1] && angle1[2] == angle2[2] )
	{
		AngleQuaternion( angle1, q );
	}
	else
	{
		float q1[4], q2[4];
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
}

static mstudioanim_t *StudioGetSequenceAnim( studiohdr_t *hdr, model_t *mod,
	mstudioseqdesc_t *pseqdesc )
{
	if( !hdr || !pseqdesc )
		return nullptr;

	if( mod && gEngfuncs.R_StudioGetAnim )
	{
		mstudioanim_t *panim = static_cast<mstudioanim_t *>( gEngfuncs.R_StudioGetAnim( hdr, mod, pseqdesc ) );
		if( panim )
			return panim;
	}

	if( pseqdesc->seqgroup != 0 )
		return nullptr;

	return (mstudioanim_t *)( (byte *)hdr + pseqdesc->animindex );
}

static void StudioSlerpPose( int numbones,
	float pos[][3], float rotq[][4],
	const float pos2[][3], const float rotq2[][4], float s )
{
	if( s < 0.0f ) s = 0.0f;
	if( s > 1.0f ) s = 1.0f;

	for( int i = 0; i < numbones; ++i )
	{
		float q[4];
		QuaternionSlerp( rotq[i], rotq2[i], s, q );
		rotq[i][0] = q[0]; rotq[i][1] = q[1];
		rotq[i][2] = q[2]; rotq[i][3] = q[3];

		pos[i][0] += ( pos2[i][0] - pos[i][0] ) * s;
		pos[i][1] += ( pos2[i][1] - pos[i][1] ) * s;
		pos[i][2] += ( pos2[i][2] - pos[i][2] ) * s;
	}
}

static float StudioGetEntityBlend( const cl_entity_s *ent, float renderTime, int channel )
{
	if( !ent || channel < 0 || channel >= 2 )
		return 0.0f;

	const float dadt = StudioControllerInterpolant( ent, renderTime );
	float s = ( ent->curstate.blending[channel] * dadt +
		ent->latched.prevblending[channel] * ( 1.0f - dadt ) ) / 255.0f;

	if( s < 0.0f ) s = 0.0f;
	if( s > 1.0f ) s = 1.0f;
	return s;
}

static void StudioCalcAnimationStream( studiohdr_t *hdr, model_t *mod,
	mstudioseqdesc_t *pseqdesc, float frame,
	const float adj[MAXSTUDIOCONTROLLERS], int stream,
	float pos[][3], float rotq[][4] )
{
	mstudiobone_t *pbones = (mstudiobone_t *)( (byte *)hdr + hdr->boneindex );
	mstudioanim_t *panim = StudioGetSequenceAnim( hdr, mod, pseqdesc );

	if( !panim )
	{

		for( int i = 0; i < hdr->numbones; ++i )
		{
			pos[i][0] = pbones[i].value[0];
			pos[i][1] = pbones[i].value[1];
			pos[i][2] = pbones[i].value[2];
			AngleQuaternion( &pbones[i].value[3], rotq[i] );
		}
		return;
	}

	if( stream < 0 )
		stream = 0;
	panim += stream * hdr->numbones;

	float f = ( frame * ( pseqdesc->numframes - 1 ) ) / 256.0f;
	if( f < 0.0f )
		f = 0.0f;

	int iframe = static_cast<int>( f );
	float s = R_NoInterpolationEnabled( ) ? 0.0f : f - iframe;

	if( pseqdesc->numframes > 1 && iframe >= pseqdesc->numframes - 1 )
	{
		iframe = pseqdesc->numframes - 2;
		s = 1.0f;
	}
	else if( pseqdesc->numframes <= 1 )
	{
		iframe = 0;
		s = 0.0f;
	}

	const bool threadedBones = DX_ThreadStudioBones( hdr->numbones );
	if( threadedBones )
	{
		DX_ParallelFor( hdr->numbones, 16, [&]( int begin, int end )
		{
			for( int i = begin; i < end; ++i )
			{
				StudioCalcBonePosition( iframe, s, &pbones[i], &panim[i], adj, pos[i] );
				StudioCalcBoneQuaternion( iframe, s, &pbones[i], &panim[i], adj, rotq[i] );
			}
		} );
	}
	else
	{
		for( int i = 0; i < hdr->numbones; ++i )
		{
			StudioCalcBonePosition( iframe, s, &pbones[i], &panim[i], adj, pos[i] );
			StudioCalcBoneQuaternion( iframe, s, &pbones[i], &panim[i], adj, rotq[i] );
		}
	}
}

static void StudioCalcSequencePose( studiohdr_t *hdr, model_t *mod,
	int seqIndex, float frame, const float adj[MAXSTUDIOCONTROLLERS],
	float blend0, float blend1, float pos[][3], float rotq[][4] )
{
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)( (byte *)hdr + hdr->seqindex ) + seqIndex;
	const int numblends = pseqdesc->numblends > 0 ? pseqdesc->numblends : 1;

	StudioCalcAnimationStream( hdr, mod, pseqdesc, frame, adj, 0, pos, rotq );

	if( numblends > 1 )
	{
		float pos2[MAXSTUDIOBONES][3];
		float rotq2[MAXSTUDIOBONES][4];

		StudioCalcAnimationStream( hdr, mod, pseqdesc, frame, adj, 1, pos2, rotq2 );
		StudioSlerpPose( hdr->numbones, pos, rotq, pos2, rotq2, blend0 );

		if( numblends == 4 )
		{
			float pos3[MAXSTUDIOBONES][3];
			float rotq3[MAXSTUDIOBONES][4];
			float pos4[MAXSTUDIOBONES][3];
			float rotq4[MAXSTUDIOBONES][4];

			StudioCalcAnimationStream( hdr, mod, pseqdesc, frame, adj, 2, pos3, rotq3 );
			StudioCalcAnimationStream( hdr, mod, pseqdesc, frame, adj, 3, pos4, rotq4 );
			StudioSlerpPose( hdr->numbones, pos3, rotq3, pos4, rotq4, blend0 );
			StudioSlerpPose( hdr->numbones, pos, rotq, pos3, rotq3, blend1 );
		}
	}

	if( pseqdesc->motionbone >= 0 && pseqdesc->motionbone < hdr->numbones )
	{
		if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
		if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
		if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;
	}
}

#ifndef STUDIO_LOOPING
#define STUDIO_LOOPING 0x0001
#endif

float StudioFrameAdvance( studiohdr_t *hdr, cl_entity_s *ent, float curtime )
{
	if( !hdr || !ent )
		return ent ? ent->curstate.frame : 0.0f;

	int seq = ent->curstate.sequence;
	if( seq < 0 || seq >= hdr->numseq )
		return ent->curstate.frame;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)( (byte *)hdr + hdr->seqindex ) + seq;
	if( pseqdesc->numframes <= 1 )
		return 0.0f;

	if( R_NoInterpolationEnabled( ) )
		return ent->curstate.frame;

	float sequenceFrame = ent->curstate.frame * ( pseqdesc->numframes - 1 ) / 256.0f;

	if( curtime > ent->curstate.animtime )
	{
		sequenceFrame += ( curtime - ent->curstate.animtime ) *
			ent->curstate.framerate * pseqdesc->fps;
	}

	const float lastFrame = (float)( pseqdesc->numframes - 1 );

	if( pseqdesc->flags & STUDIO_LOOPING )
	{
		sequenceFrame = fmodf( sequenceFrame, lastFrame );
		if( sequenceFrame < 0.0f )
			sequenceFrame += lastFrame;
	}
	else
	{

		if( sequenceFrame >= lastFrame - 0.001f )
			sequenceFrame = lastFrame - 0.001f;
		if( sequenceFrame < 0.0f )
			sequenceFrame = 0.0f;
	}

	return sequenceFrame * ( 256.0f / lastFrame );
}

static void StudioCalcBoneTransformsInternal( model_t *mod, studiohdr_t *hdr,
	int          seqIndex,
	float        frame,
	BoneMat34 *boneMats,
	int          gaitSeqIndex,
	float        gaitFrame,
	float        gaitYaw,
	const cl_entity_s *ent,
	float        renderTime )
{
	if( !boneMats || !R_StudioHeaderValid( hdr ) )
		return;

	if( seqIndex < 0 || seqIndex >= hdr->numseq )
		seqIndex = 0;

	mstudiobone_t *pbones = (mstudiobone_t *)( (byte *)hdr + hdr->boneindex );
	float adj[MAXSTUDIOCONTROLLERS];
	StudioCalcBoneAdj( hdr, ent, renderTime, adj );

	float pos[MAXSTUDIOBONES][3];
	float rotq[MAXSTUDIOBONES][4];

	const float blend0 = StudioGetEntityBlend( ent, renderTime, 0 );
	const float blend1 = StudioGetEntityBlend( ent, renderTime, 1 );

	StudioCalcSequencePose( hdr, mod, seqIndex, frame, adj, blend0, blend1, pos, rotq );

	if( gaitSeqIndex > 0 && gaitSeqIndex < hdr->numseq )
	{
		float gpos[MAXSTUDIOBONES][3];
		float grotq[MAXSTUDIOBONES][4];

		StudioCalcSequencePose( hdr, mod, gaitSeqIndex, gaitFrame, adj,
			blend0, blend1, gpos, grotq );

		bool copyBones = true;
		for( int i = 0; i < hdr->numbones; ++i )
		{
			if( R_FixedStringEquals( pbones[i].name, sizeof( pbones[i].name ), "Bip01 Spine" ) )
			{
				copyBones = false;
			}
			else
			{
				const int parent = pbones[i].parent;
				if( parent >= 0 && parent < hdr->numbones &&
					R_FixedStringEquals( pbones[parent].name, sizeof( pbones[parent].name ), "Bip01 Pelvis" ) )
				{
					copyBones = true;
				}
			}

			if( !copyBones )
				continue;

			pos[i][0] = gpos[i][0];
			pos[i][1] = gpos[i][1];
			pos[i][2] = gpos[i][2];
			rotq[i][0] = grotq[i][0];
			rotq[i][1] = grotq[i][1];
			rotq[i][2] = grotq[i][2];
			rotq[i][3] = grotq[i][3];
		}
	}

	(void)gaitYaw;

	BoneMat34 localMats[MAXSTUDIOBONES];
	if( DX_ThreadStudioBones( hdr->numbones ) )
	{
		DX_ParallelFor( hdr->numbones, 16, [&]( int begin, int end )
		{
			for( int i = begin; i < end; ++i )
				QuaternionMatrix( rotq[i], pos[i], localMats[i] );
		} );
	}
	else
	{
		for( int i = 0; i < hdr->numbones; i++ )
			QuaternionMatrix( rotq[i], pos[i], localMats[i] );
	}

	for( int i = 0; i < hdr->numbones; ++i )
	{
		const int parent = pbones[i].parent;
		if( parent == -1 )
			memcpy( boneMats[i], localMats[i], sizeof( BoneMat34 ) );
		else if( parent >= 0 && parent < i && parent < hdr->numbones )
			ConcatTransforms( boneMats[parent], localMats[i], boneMats[i] );
		else
			memcpy( boneMats[i], localMats[i], sizeof( BoneMat34 ) );
	}
}

static int R_PlayerGaitSlot( const cl_entity_s *ent )
{
	if( !ent || !ent->player )
		return -1;

	int playerIndex = ent->curstate.number - 1;
	if( playerIndex < 0 || playerIndex >= MAX_CLIENTS )
		playerIndex = ent->index - 1;

	return ( playerIndex >= 0 && playerIndex < MAX_CLIENTS ) ? playerIndex : -1;
}

static float R_NormalizeYaw180( float yaw )
{
	yaw = yaw - static_cast<int>( yaw / 360.0f ) * 360.0f;
	if( yaw > 180.0f ) yaw -= 360.0f;
	if( yaw < -180.0f ) yaw += 360.0f;
	return yaw;
}

static float R_NormalizeYaw360( float yaw )
{
	yaw = fmodf( yaw, 360.0f );
	if( yaw < 0.0f )
		yaw += 360.0f;
	return yaw;
}

static void R_SetGaitControllers( cl_entity_s *ent, float yawDelta )
{
	if( !ent )
		return;

	const float controllerValue =
		( ( yawDelta * 0.25f ) + 30.0f ) * ( 255.0f / 60.0f );
	const byte controller = static_cast<byte>( std::max( 0.0f,
		std::min( 255.0f, controllerValue ) ) );

	for( int i = 0; i < 4; ++i )
	{
		ent->curstate.controller[i] = controller;
		ent->latched.prevcontroller[i] = controller;
	}
}

static void R_ResetGaitControllers( cl_entity_s *ent )
{
	if( !ent )
		return;

	for( int i = 0; i < 4; ++i )
	{
		ent->curstate.controller[i] = 127;
		ent->latched.prevcontroller[i] = 127;
	}
}

static void R_SyncPlayerInfoGait( int playerIndex,
	const player_gait_state_t &state, int gaitSequence )
{
	if( playerIndex < 0 || playerIndex >= MAX_CLIENTS ||
		!gEngfuncs.pfnPlayerInfo )
	{
		return;
	}

	player_info_t *info = gEngfuncs.pfnPlayerInfo( playerIndex );
	if( !info )
		return;

	info->gaitsequence = gaitSequence;
	info->gaitframe = state.gaitFrame;
	info->gaityaw = state.gaitYaw;
	VectorCopy( state.previousOrigin, info->prevgaitorigin );
}

static player_gait_pose_t R_PreparePlayerGait(
	cl_entity_s *ent, studiohdr_t *hdr, float poseTime )
{
	player_gait_pose_t result{};
	if( !ent || !ent->player || !hdr || hdr->numseq <= 0 )
		return result;

	const int playerIndex = R_PlayerGaitSlot( ent );
	if( playerIndex < 0 )
		return result;

	player_gait_state_t &state = g_playerGaitStates[playerIndex];

	int gaitSequence = ent->curstate.gaitsequence;
	if( gaitSequence <= 0 || gaitSequence >= hdr->numseq )
	{
		R_ResetGaitControllers( ent );

		if( !state.initialized )
		{
			state.initialized = true;
			state.gaitYaw = R_NormalizeYaw360( ent->angles[YAW] );
		}
		VectorCopy( ent->origin, state.previousOrigin );
		state.lastPoseTime = poseTime;
		R_SyncPlayerInfoGait( playerIndex, state, 0 );
		return result;
	}

	const float aimYaw = R_NormalizeYaw360( ent->angles[YAW] );

	if( !state.initialized )
	{
		state.initialized = true;
		state.gaitFrame = 0.0f;
		state.gaitYaw = aimYaw;
		VectorCopy( ent->origin, state.previousOrigin );
		state.lastPoseTime = poseTime;
	}

	float dt = poseTime - state.lastPoseTime;
	if( dt < 0.0f ) dt = 0.0f;
	if( dt > 1.0f ) dt = 1.0f;

	float estimatedVelocity[3] =
	{
		ent->origin[0] - state.previousOrigin[0],
		ent->origin[1] - state.previousOrigin[1],
		ent->origin[2] - state.previousOrigin[2]
	};

	if( dt > 0.0f )
	{
		VectorCopy( ent->origin, state.previousOrigin );
		state.lastPoseTime = poseTime;
	}
	else
	{
		estimatedVelocity[0] = 0.0f;
		estimatedVelocity[1] = 0.0f;
		estimatedVelocity[2] = 0.0f;
	}

	float gaitMovement = sqrtf(
		estimatedVelocity[0] * estimatedVelocity[0] +
		estimatedVelocity[1] * estimatedVelocity[1] +
		estimatedVelocity[2] * estimatedVelocity[2] );

	if( dt <= 0.0f || gaitMovement / dt < 5.0f )
	{
		gaitMovement = 0.0f;
		estimatedVelocity[0] = 0.0f;
		estimatedVelocity[1] = 0.0f;
	}

	if( estimatedVelocity[0] == 0.0f && estimatedVelocity[1] == 0.0f )
	{
		float yawDiff = R_NormalizeYaw180( aimYaw - state.gaitYaw );

		if( dt < 0.25f )
			yawDiff *= dt * 4.0f;
		else
			yawDiff *= dt;

		state.gaitYaw = R_NormalizeYaw360( state.gaitYaw + yawDiff );
		gaitMovement = 0.0f;
	}
	else
	{
		state.gaitYaw = atan2f( estimatedVelocity[1], estimatedVelocity[0] ) *
			57.29577951308232f;
		if( state.gaitYaw > 180.0f ) state.gaitYaw = 180.0f;
		if( state.gaitYaw < -180.0f ) state.gaitYaw = -180.0f;
	}

	float yawDelta = R_NormalizeYaw180( aimYaw - state.gaitYaw );
	if( yawDelta > 120.0f )
	{
		state.gaitYaw -= 180.0f;
		gaitMovement = -gaitMovement;
		yawDelta -= 180.0f;
	}
	else if( yawDelta < -120.0f )
	{
		state.gaitYaw += 180.0f;
		gaitMovement = -gaitMovement;
		yawDelta += 180.0f;
	}

	state.gaitYaw = R_NormalizeYaw360( state.gaitYaw );
	R_SetGaitControllers( ent, yawDelta );

	mstudioseqdesc_t *gaitDesc = reinterpret_cast<mstudioseqdesc_t *>(
		reinterpret_cast<byte *>( hdr ) + hdr->seqindex ) + gaitSequence;

	if( gaitDesc->numframes > 0 )
	{
		if( gaitDesc->linearmovement[0] > 0.0f )
		{
			state.gaitFrame +=
				( gaitMovement / gaitDesc->linearmovement[0] ) *
				static_cast<float>( gaitDesc->numframes );
		}
		else
		{
			state.gaitFrame += gaitDesc->fps * dt;
		}

		const float numFrames = static_cast<float>( gaitDesc->numframes );
		state.gaitFrame =
			state.gaitFrame - static_cast<int>( state.gaitFrame / numFrames ) * numFrames;
		if( state.gaitFrame < 0.0f )
			state.gaitFrame += numFrames;
	}

	R_SyncPlayerInfoGait( playerIndex, state, gaitSequence );

	result.active = true;
	result.sequence = gaitSequence;
	result.worldYaw = state.gaitYaw;

	if( gaitDesc->numframes > 1 )
	{

		const float lastFrame = static_cast<float>( gaitDesc->numframes - 1 );
		const float sampleFrame = state.gaitFrame > lastFrame ? 0.0f : state.gaitFrame;
		result.frame256 = sampleFrame * ( 256.0f / lastFrame );
	}
	else
	{
		result.frame256 = 0.0f;
	}

	return result;
}

static int StudioSelectBodypartModel( const mstudiobodyparts_t *bodypart, int entityBody )
{
	if( !bodypart || bodypart->nummodels <= 1 || bodypart->base <= 0 )
		return 0;

	int modelIndex = ( entityBody / bodypart->base ) % bodypart->nummodels;
	if( modelIndex < 0 )
		modelIndex += bodypart->nummodels;
	return modelIndex;
}

static mstudiomodel_t *StudioGetSelectedBodypartModel( studiohdr_t *hdr,
	const mstudiobodyparts_t *bodypart, int entityBody )
{
	if( !hdr || !bodypart || bodypart->nummodels <= 0 || bodypart->nummodels > 4096 ||
		bodypart->modelindex < 0 ||
		!R_StudioRangeValid( hdr, bodypart->modelindex,
			static_cast<size_t>( bodypart->nummodels ) * sizeof( mstudiomodel_t ) ) )
		return nullptr;

	const int selected = StudioSelectBodypartModel( bodypart, entityBody );
	if( selected < 0 || selected >= bodypart->nummodels )
		return nullptr;
	mstudiomodel_t *models = reinterpret_cast<mstudiomodel_t *>(
		reinterpret_cast<byte *>( hdr ) + bodypart->modelindex );
	return &models[selected];
}

static mstudiotexture_t *R_StudioGetTextureForSkin( studiohdr_t *hdr,
	mstudiomesh_t *mesh, int entitySkin )
{
	if( !hdr || !mesh || hdr->numtextures <= 0 || hdr->numtextures > 4096 ||
		!R_StudioRangeValid( hdr, hdr->textureindex,
			static_cast<size_t>( hdr->numtextures ) * sizeof( mstudiotexture_t ) ) )
		return nullptr;

	int textureIndex = mesh->skinref;
	if( hdr->numskinref > 0 || hdr->numskinfamilies > 0 )
	{
		if( hdr->numskinref <= 0 || hdr->numskinref > 4096 ||
			hdr->numskinfamilies <= 0 || hdr->numskinfamilies > 4096 ||
			mesh->skinref < 0 || mesh->skinref >= hdr->numskinref )
			return nullptr;

		const size_t familyCount = static_cast<size_t>( hdr->numskinfamilies );
		const size_t refsPerFamily = static_cast<size_t>( hdr->numskinref );
		if( familyCount > std::numeric_limits<size_t>::max( ) / refsPerFamily )
			return nullptr;
		const size_t totalRefs = familyCount * refsPerFamily;
		if( !R_StudioRangeValid( hdr, hdr->skinindex, totalRefs * sizeof( short ) ) )
			return nullptr;

		int skinFamily = entitySkin;
		if( skinFamily < 0 || skinFamily >= hdr->numskinfamilies )
			skinFamily = 0;
		const size_t refIndex = static_cast<size_t>( skinFamily ) * refsPerFamily +
			static_cast<size_t>( mesh->skinref );
		short *skinrefs = reinterpret_cast<short *>( reinterpret_cast<byte *>( hdr ) + hdr->skinindex );
		textureIndex = skinrefs[refIndex];
	}

	if( textureIndex < 0 || textureIndex >= hdr->numtextures )
		return nullptr;
	mstudiotexture_t *textures = reinterpret_cast<mstudiotexture_t *>(
		reinterpret_cast<byte *>( hdr ) + hdr->textureindex );
	return &textures[textureIndex];
}

static unsigned int R_StudioTextureFlags( const mstudiotexture_t *texture )
{
	return texture ? static_cast<unsigned int>( texture->flags ) : 0u;
}

static float R_StudioHalfToFloat( short value )
{
	const uint16_t h = static_cast<uint16_t>( value );
	const uint32_t sign = static_cast<uint32_t>( h & 0x8000u ) << 16;
	const uint32_t exponent = ( h >> 10 ) & 0x1Fu;
	uint32_t mantissa = h & 0x03FFu;
	uint32_t bits = 0;

	if( exponent == 0 )
	{
		if( mantissa == 0 )
		{
			bits = sign;
		}
		else
		{

			int shift = 0;
			while( ( mantissa & 0x0400u ) == 0 )
			{
				mantissa <<= 1;
				++shift;
			}
			mantissa &= 0x03FFu;
			const uint32_t e = static_cast<uint32_t>( 113 - shift );
			bits = sign | ( e << 23 ) | ( mantissa << 13 );
		}
	}
	else if( exponent == 0x1Fu )
	{
		bits = sign | 0x7F800000u | ( mantissa << 13 );
	}
	else
	{
		bits = sign | ( ( exponent + 112u ) << 23 ) | ( mantissa << 13 );
	}

	float result = 0.0f;
	memcpy( &result, &bits, sizeof( result ) );
	return result;
}

static D3DCOLOR R_StudioFullbrightDiffuse( D3DCOLOR diffuse )
{
	return D3DCOLOR_ARGB( ( diffuse >> 24 ) & 0xFF, 255, 255, 255 );
}

static int R_StudioCpuBatchKey( int textureId, unsigned int materialFlags )
{
	const unsigned int stateFlags = materialFlags &
		( STUDIO_NF_MASKED | STUDIO_NF_ADDITIVE );
	return static_cast<int>( ( static_cast<unsigned int>( textureId ) & 0xFFFFu ) |
		( ( stateFlags & 0xFFFFu ) << 16 ) );
}

static int R_StudioCpuBatchTextureId( int key )
{
	return static_cast<int>( static_cast<unsigned int>( key ) & 0xFFFFu );
}

static unsigned int R_StudioCpuBatchStateFlags( int key )
{
	return ( static_cast<unsigned int>( key ) >> 16 ) & 0xFFFFu;
}

static void R_StudioTransformPointByWorld( const D3DMATRIX &world,
	const float in[3], float out[3] )
{
	out[0] = in[0] * world._11 + in[1] * world._21 + in[2] * world._31 + world._41;
	out[1] = in[0] * world._12 + in[1] * world._22 + in[2] * world._32 + world._42;
	out[2] = in[0] * world._13 + in[1] * world._23 + in[2] * world._33 + world._43;
}

static void R_StudioTransformVector( const D3DMATRIX &world,
	const float in[3], float out[3] )
{
	out[0] = in[0] * world._11 + in[1] * world._21 + in[2] * world._31;
	out[1] = in[0] * world._12 + in[1] * world._22 + in[2] * world._32;
	out[2] = in[0] * world._13 + in[1] * world._23 + in[2] * world._33;
}

static void R_StudioRotateVectorByBone( const float in[3], const BoneMat34 bone,
	float out[3] )
{
	out[0] = in[0] * bone[0][0] + in[1] * bone[0][1] + in[2] * bone[0][2];
	out[1] = in[0] * bone[1][0] + in[1] * bone[1][1] + in[2] * bone[1][2];
	out[2] = in[0] * bone[2][0] + in[1] * bone[2][1] + in[2] * bone[2][2];
}

static bool R_StudioNormalizeVector( float vector[3] )
{
	const float lengthSq = vector[0] * vector[0] +
		vector[1] * vector[1] + vector[2] * vector[2];
	if( lengthSq <= 1e-12f )
		return false;

	const float inverseLength = 1.0f / sqrtf( lengthSq );
	vector[0] *= inverseLength;
	vector[1] *= inverseLength;
	vector[2] *= inverseLength;
	return true;
}

static void R_StudioCrossProduct( const float a[3], const float b[3], float out[3] )
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static void R_StudioBuildChromeUV( const D3DMATRIX &world,
	const BoneMat34 *boneMatrices, int normalBoneIndex,
	const float skinnedNormal[3], float textureWidth, float textureHeight,
	float &outU, float &outV, const float *customChromeOrigin = nullptr )
{
	const float safeWidth = std::max( 1.0f, textureWidth );
	const float safeHeight = std::max( 1.0f, textureHeight );

	if( !boneMatrices || normalBoneIndex < 0 || normalBoneIndex >= MAXSTUDIOBONES )
	{
		outU = 32.0f / safeWidth;
		outV = 32.0f / safeHeight;
		return;
	}

	const float localBoneOrigin[3] =
	{
		boneMatrices[normalBoneIndex][0][3],
		boneMatrices[normalBoneIndex][1][3],
		boneMatrices[normalBoneIndex][2][3]
	};

	float worldBoneOrigin[3];
	R_StudioTransformPointByWorld( world, localBoneOrigin, worldBoneOrigin );

	const float *chromeOrigin = customChromeOrigin ? customChromeOrigin : g_vOrigin;
	float toBone[3] =
	{
		worldBoneOrigin[0] - chromeOrigin[0],
		worldBoneOrigin[1] - chromeOrigin[1],
		worldBoneOrigin[2] - chromeOrigin[2]
	};

	if( !R_StudioNormalizeVector( toBone ) )
	{

		toBone[0] = g_vForward[0];
		toBone[1] = g_vForward[1];
		toBone[2] = g_vForward[2];
		if( !R_StudioNormalizeVector( toBone ) )
		{
			outU = 32.0f / safeWidth;
			outV = 32.0f / safeHeight;
			return;
		}
	}

	float chromeUp[3];
	R_StudioCrossProduct( toBone, g_vRight, chromeUp );
	if( !R_StudioNormalizeVector( chromeUp ) )
	{
		outU = 32.0f / safeWidth;
		outV = 32.0f / safeHeight;
		return;
	}

	float chromeRight[3];
	R_StudioCrossProduct( chromeUp, toBone, chromeRight );
	if( !R_StudioNormalizeVector( chromeRight ) )
	{
		outU = 32.0f / safeWidth;
		outV = 32.0f / safeHeight;
		return;
	}

	float worldNormal[3];
	R_StudioTransformVector( world, skinnedNormal, worldNormal );
	if( !R_StudioNormalizeVector( worldNormal ) )
	{
		outU = 32.0f / safeWidth;
		outV = 32.0f / safeHeight;
		return;
	}

	outU = ( DotProduct( worldNormal, chromeRight ) + 1.0f ) *
		( 32.0f / safeWidth );
	outV = ( DotProduct( worldNormal, chromeUp ) + 1.0f ) *
		( 32.0f / safeHeight );
}

void DX_MatrixIdentity( D3DMATRIX &matrix )
{
	matrix._11 = 1.0f; matrix._12 = 0.0f; matrix._13 = 0.0f; matrix._14 = 0.0f;
	matrix._21 = 0.0f; matrix._22 = 1.0f; matrix._23 = 0.0f; matrix._24 = 0.0f;
	matrix._31 = 0.0f; matrix._32 = 0.0f; matrix._33 = 1.0f; matrix._34 = 0.0f;
	matrix._41 = 0.0f; matrix._42 = 0.0f; matrix._43 = 0.0f; matrix._44 = 1.0f;
}

static void DX_ToD3DXMatrix( const D3DMATRIX &source, D3DXMATRIX &destination )
{
	destination._11 = source._11; destination._12 = source._12; destination._13 = source._13; destination._14 = source._14;
	destination._21 = source._21; destination._22 = source._22; destination._23 = source._23; destination._24 = source._24;
	destination._31 = source._31; destination._32 = source._32; destination._33 = source._33; destination._34 = source._34;
	destination._41 = source._41; destination._42 = source._42; destination._43 = source._43; destination._44 = source._44;
}

static void R_StudioBuildWorldFromMatrix( const D3DMATRIX *currentWorldMatrix,
	D3DMATRIX &outWorld )
{
	if( currentWorldMatrix )
	{
		outWorld = *currentWorldMatrix;
		return;
	}

	DX_MatrixIdentity( outWorld );
}

static void R_StudioAttachmentPoint( studiohdr_t *hdr,
	const BoneMat34 *boneMatrices, const D3DMATRIX &world,
	int attachmentIndex, vec3_t outPoint )
{
	const float worldOrigin[3] = { world._41, world._42, world._43 };
	VectorCopy( worldOrigin, outPoint );

	if( !hdr || !boneMatrices || hdr->numattachments <= 0 ||
		hdr->attachmentindex <= 0 || attachmentIndex < 0 ||
		attachmentIndex >= hdr->numattachments )
	{
		return;
	}

	mstudioattachment_t *attachments =
		(mstudioattachment_t *)( (byte *)hdr + hdr->attachmentindex );
	const int bone = attachments[attachmentIndex].bone;
	if( bone < 0 || bone >= hdr->numbones )
		return;

	float localPoint[3];
	VectorTransform( attachments[attachmentIndex].org,
		boneMatrices[bone], localPoint );
	R_StudioTransformPointByWorld( world, localPoint, outPoint );
}

bool R_GetAttachmentPoint( const cl_entity_s *ent, int attachmentIndex, vec3_t outPoint )
{
	if( !ent || !outPoint || attachmentIndex < 0 ||
		attachmentIndex >= MAXSTUDIOATTACHMENTS )
	{
		return false;
	}

	entity_cache_t *entityCache = R_FindEntityCache( ent );
	if( !entityCache || !entityCache->attachments.valid[attachmentIndex] )
		return false;
	attachment_cache_t &cache = entityCache->attachments;

	if( gp_cl && fabsf( static_cast<float>( gp_cl->time ) - cache.updateTime ) > 0.50f )
		return false;

	VectorCopy( cache.points[attachmentIndex], outPoint );
	return true;
}

static void R_StudioStoreAttachments( const cl_entity_s *ent,
	studiohdr_t *hdr, const BoneMat34 *boneMatrices, const D3DMATRIX &world,
	float renderTime )
{
	if( !ent )
		return;

	attachment_cache_t &cache = R_GetEntityCache( ent ).attachments;
	for( int i = 0; i < MAXSTUDIOATTACHMENTS; ++i )
		cache.valid[i] = false;
	cache.updateTime = gp_cl ? static_cast<float>( gp_cl->time ) : renderTime;

	if( !hdr || !boneMatrices || hdr->numattachments <= 0 || hdr->attachmentindex <= 0 )
		return;

	mstudioattachment_t *attachments =
		(mstudioattachment_t *)( (byte *)hdr + hdr->attachmentindex );
	const int count = std::min( MAXSTUDIOATTACHMENTS, hdr->numattachments );
	for( int i = 0; i < count; ++i )
	{
		const int bone = attachments[i].bone;
		if( bone < 0 || bone >= hdr->numbones )
			continue;
		float localPoint[3];
		VectorTransform( attachments[i].org, boneMatrices[bone], localPoint );
		R_StudioTransformPointByWorld( world, localPoint, cache.points[i] );
		cache.valid[i] = true;
	}
	if( count > 0 && cache.valid[0] )
	{
		for( int i = count; i < MAXSTUDIOATTACHMENTS; ++i )
		{
			VectorCopy( cache.points[0], cache.points[i] );
			cache.valid[i] = true;
		}
	}
}

static void R_StudioUpdateAttachments( studiohdr_t *hdr,
	const cl_entity_s *ent, const BoneMat34 *boneMatrices, const D3DMATRIX &world,
	bool viewModel, float renderTime )
{
	R_StudioStoreAttachments( ent, hdr, boneMatrices, world, renderTime );

	if( viewModel && gp_cl && gEngfuncs.R_BeamGetEntity )
	{
		cl_entity_s *player = gEngfuncs.R_BeamGetEntity( gp_cl->playernum + 1 );
		if( player && player != ent )
			R_StudioStoreAttachments( player, hdr, boneMatrices, world, renderTime );
	}
}

static bool R_StringContainsInsensitive( const char *text, const char *needle )
{
	if( !text || !needle || !needle[0] )
		return false;

	const size_t needleLen = strlen( needle );
	for( const char *p = text; *p; ++p )
	{
		size_t i = 0;
		while( i < needleLen && p[i] )
		{
			char a = p[i];
			char b = needle[i];
			if( a >= 'A' && a <= 'Z' ) a = static_cast<char>( a - 'A' + 'a' );
			if( b >= 'A' && b <= 'Z' ) b = static_cast<char>( b - 'A' + 'a' );
			if( a != b ) break;
			++i;
		}
		if( i == needleLen )
			return true;
	}
	return false;
}

int R_FindMuzzleFlashTexture( int spriteType )
{

	int spriteIndex = spriteType + 1;
	if( spriteIndex < 1 ) spriteIndex = 1;
	if( spriteIndex > 16 ) spriteIndex = 16;

	char name[64];
	const char *formats[] =
	{
		"muzzleflash%d",
		"muzzleflash%d.spr",
		"sprites/muzzleflash%d",
		"sprites/muzzleflash%d.spr"
	};

	for( const char *format : formats )
	{
		snprintf( name, sizeof( name ), format, spriteIndex );
		const int texture = DX_FindTexture( name );
		if( texture > 0 )
			return texture;
	}

	snprintf( name, sizeof( name ), "muzzleflash%d", spriteIndex );
	for( int i = 1; i <= g_texcount; ++i )
	{
		if( R_StringContainsInsensitive( g_Textures[i].name, name ) )
			return i;
	}

	if( spriteIndex != 1 )
		return R_FindMuzzleFlashTexture( 0 );
	return 0;
}

static bool R_StudioMuzzleAttachment( int eventId, int &attachment )
{
	switch( eventId )
	{
	case CL_EVENT_MUZZLEFLASH0: attachment = 0; return true;
	case CL_EVENT_MUZZLEFLASH1: attachment = 1; return true;
	case CL_EVENT_MUZZLEFLASH2: attachment = 2; return true;
	case CL_EVENT_MUZZLEFLASH3: attachment = 3; return true;
	case CL_EVENT_NPC_MUZZLEFLASH0: attachment = 0; return true;
	default: break;
	}
	return false;
}

float R_CurrentRenderTime( float fallback )
{
	return gp_cl ? static_cast<float>( gp_cl->time ) : fallback;
}

static int R_StudioLocalLights( const cl_entity_s *ent,
	const D3DMATRIX *currentWorldMatrix, studio_local_light_t out[DX_STUDIO_MAX_LOCAL_LIGHTS] )
{
	if( !out )
		return 0;

	if( dx_studio_extended_lighting.value == 0.0f || dx_studio_normals.value == 0.0f ||
		dx_dlight_studio.value == 0.0f || !ent ||
		g_dxStudioLocalLightCount <= 0 )
	{
		return 0;
	}

	DX_MODEL_PROFILE_SCOPE( studioLocalLightsProfileScope, DX_PROFILE_PHASE_STUDIO_LOCAL_LIGHTS );
	vec3_t entityOrigin;
	if( currentWorldMatrix )
	{
		entityOrigin[0] = currentWorldMatrix->_41;
		entityOrigin[1] = currentWorldMatrix->_42;
		entityOrigin[2] = currentWorldMatrix->_43;
	}
	else
	{
		VectorCopy( ent->origin, entityOrigin );
	}

	D3DXMATRIX inverseWorld;
	bool useInverseWorld = false;
	if( currentWorldMatrix )
	{
		D3DXMATRIX source;
		memcpy( &source, currentWorldMatrix, sizeof( source ) );
		if( !D3DXMatrixInverse( &inverseWorld, nullptr, &source ) )
			return 0;
		useInverseWorld = true;
	}

	float strengths[DX_STUDIO_MAX_LOCAL_LIGHTS];
	int count = 0;
	const float intensityScale = R_DynamicLightIntensityScale( );

	DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_LOCAL_LIGHT_CANDIDATE,
		static_cast<unsigned int>( g_dxStudioLocalLightCount ) );
	for( int lightIndex = 0; lightIndex < g_dxStudioLocalLightCount; ++lightIndex )
	{
		const dx_dlight_t &light =
			g_dxDLights[g_dxStudioLocalLightIndices[lightIndex]];
		if( light.radius <= 0.0f )
			continue;

		vec3_t effectiveOrigin;
		VectorCopy( light.origin, effectiveOrigin );
		if( ( light.key & 0xFFF ) == ent->index )
		{
			const int attachment = ( light.key >> 12 ) & 0xF;
			if( attachment > 0 && attachment < MAXSTUDIOATTACHMENTS )
				VectorCopy( ent->attachment[attachment], effectiveOrigin );
			else
				VectorCopy( entityOrigin, effectiveOrigin );
		}

		vec3_t mid;
		VectorSubtract( entityOrigin, effectiveOrigin, mid );
		const float distSq = DotProduct( mid, mid );
		const float radiusSq = light.radius * light.radius;
		const float strength = distSq > radiusSq && distSq > 0.0001f
			? radiusSq / distSq : 1.0f;
		if( strength <= 0.05f )
			continue;

		int slot = -1;
		if( count < DX_STUDIO_MAX_LOCAL_LIGHTS )
		{
			slot = count++;
		}
		else
		{
			float weakest = strength;
			for( int i = 0; i < count; ++i )
			{
				if( strengths[i] < weakest )
				{
					weakest = strengths[i];
					slot = i;
				}
			}
		}
		if( slot < 0 )
			continue;

		vec3_t localOrigin;
		if( useInverseWorld )
		{
			D3DXVECTOR3 input( effectiveOrigin[0], effectiveOrigin[1], effectiveOrigin[2] );
			D3DXVECTOR3 local;
			D3DXVec3TransformCoord( &local, &input, &inverseWorld );
			localOrigin[0] = local.x;
			localOrigin[1] = local.y;
			localOrigin[2] = local.z;
		}
		else
		{
			VectorCopy( effectiveOrigin, localOrigin );
		}

		VectorCopy( localOrigin, out[slot].origin );
		out[slot].radiusSq = radiusSq;
		out[slot].r = light.r;
		out[slot].g = light.g;
		out[slot].b = light.b;
		out[slot].intensity = intensityScale;
		strengths[slot] = strength;
	}
	return count;
}

static bool R_StudioLocalLightsEqual( const studio_local_light_t *a, int aCount,
	const studio_local_light_t *b, int bCount )
{
	if( aCount != bCount )
		return false;
	for( int i = 0; i < aCount; ++i )
	{
		if( fabsf( a[i].origin[0] - b[i].origin[0] ) > 0.001f ||
			fabsf( a[i].origin[1] - b[i].origin[1] ) > 0.001f ||
			fabsf( a[i].origin[2] - b[i].origin[2] ) > 0.001f ||
			fabsf( a[i].radiusSq - b[i].radiusSq ) > 0.01f ||
			fabsf( a[i].r - b[i].r ) > 0.0001f ||
			fabsf( a[i].g - b[i].g ) > 0.0001f ||
			fabsf( a[i].b - b[i].b ) > 0.0001f ||
			fabsf( a[i].intensity - b[i].intensity ) > 0.0001f )
		{
			return false;
		}
	}
	return true;
}

static D3DCOLOR R_StudioApplyLocalLights( D3DCOLOR baseDiffuse,
	const float position[3], const float normal[3],
	const studio_local_light_t *lights, int lightCount )
{
	if( !position || !normal || !lights || lightCount <= 0 )
		return baseDiffuse;

	float outR = static_cast<float>( ( baseDiffuse >> 16 ) & 0xFF ) / 255.0f;
	float outG = static_cast<float>( ( baseDiffuse >> 8 ) & 0xFF ) / 255.0f;
	float outB = static_cast<float>( baseDiffuse & 0xFF ) / 255.0f;
	for( int i = 0; i < lightCount; ++i )
	{
		vec3_t delta;
		VectorSubtract( position, lights[i].origin, delta );
		const float distSq = std::max( 0.000001f, DotProduct( delta, delta ) );
		const float facing = std::max( 0.0f, -DotProduct( normal, delta ) );
		if( facing <= 0.0f )
			continue;
		const float invDist = 1.0f / sqrtf( distSq );
		const float attenuation = facing * lights[i].radiusSq *
			invDist * invDist * invDist * lights[i].intensity;
		outR += lights[i].r * attenuation;
		outG += lights[i].g * attenuation;
		outB += lights[i].b * attenuation;
	}

	return D3DCOLOR_ARGB( static_cast<byte>( ( baseDiffuse >> 24 ) & 0xFF ),
		static_cast<int>( std::max( 0.0f, std::min( 1.0f, outR ) ) * 255.0f + 0.5f ),
		static_cast<int>( std::max( 0.0f, std::min( 1.0f, outG ) ) * 255.0f + 0.5f ),
		static_cast<int>( std::max( 0.0f, std::min( 1.0f, outB ) ) * 255.0f + 0.5f ) );
}

static bool R_StudioEventFrameCrossed( const mstudioseqdesc_t *seqdesc,
	float lastFrame, float currentFrame, float eventFrame,
	bool sequenceChanged, bool sameSequenceRestart )
{
	constexpr float kEventEpsilon = 0.001f;

	if( sequenceChanged || lastFrame < -0.5f )
		return eventFrame <= currentFrame + kEventEpsilon;

	if( currentFrame + kEventEpsilon < lastFrame )
	{
		if( seqdesc && ( seqdesc->flags & STUDIO_LOOPING ) )
		{

			const float lastNativeFrame =
				std::max( 1.0f, static_cast<float>( seqdesc->numframes - 1 ) );
			const bool realWrap =
				lastFrame >= lastNativeFrame * 0.75f &&
				currentFrame <= lastNativeFrame * 0.25f;
			return realWrap &&
				( eventFrame > lastFrame ||
				  eventFrame <= currentFrame + kEventEpsilon );
		}

		return sameSequenceRestart &&
			eventFrame <= currentFrame + kEventEpsilon;
	}

	return eventFrame > lastFrame + kEventEpsilon &&
		eventFrame <= currentFrame + kEventEpsilon;
}

static bool R_FindMuzzleFlash(
	const mstudioseqdesc_t *seqdesc, studiohdr_t *hdr, float nativeFrame,
	int &outAttachment, int &outOption )
{
	outAttachment = 0;
	outOption = 20;

	if( !seqdesc || !hdr || seqdesc->numevents <= 0 || seqdesc->eventindex <= 0 )
		return false;

	mstudioevent_t *events =
		(mstudioevent_t *)( (byte *)hdr + seqdesc->eventindex );

	int bestIndex = -1;
	float bestDistance = 1.0e30f;

	for( int i = 0; i < seqdesc->numevents; ++i )
	{
		int attachment = 0;
		if( !R_StudioMuzzleAttachment( events[i].event, attachment ) )
			continue;

		const float distance =
			fabsf( static_cast<float>( events[i].frame ) - nativeFrame );
		if( distance < bestDistance )
		{
			bestDistance = distance;
			bestIndex = i;
		}
	}

	if( bestIndex < 0 )
		return false;

	int attachment = 0;
	R_StudioMuzzleAttachment( events[bestIndex].event, attachment );
	if( attachment < 0 || attachment >= MAXSTUDIOATTACHMENTS )
		attachment = 0;

	outAttachment = attachment;
	outOption = atoi( events[bestIndex].options );
	return true;
}

static bool R_StudioProcessMuzzleEvents( studiohdr_t *hdr,
	cl_entity_s *ent, int sequence, float renderFrame,
	const BoneMat34 *boneMatrices, const D3DMATRIX &world,
	bool viewModel, float renderTime )
{
	if( !hdr || !ent || !boneMatrices || sequence < 0 || sequence >= hdr->numseq )
		return false;

	R_StudioUpdateAttachments( hdr, ent, boneMatrices, world,
		viewModel, renderTime );

	mstudioseqdesc_t *seqdesc =
		(mstudioseqdesc_t *)( (byte *)hdr + hdr->seqindex ) + sequence;
	if( !seqdesc )
		return false;

	muzzleflash_state_t &state =
		R_GetEntityCache( ent ).muzzleflash;
	const bool sequenceChanged = state.lastSequence != sequence;

	const float nativeLastFrame =
		std::max( 1.0f, static_cast<float>( seqdesc->numframes - 1 ) );
	const float nativeRenderFrame =
		std::max( 0.0f, std::min( nativeLastFrame,
			renderFrame * ( nativeLastFrame / 256.0f ) ) );

	const bool hasEfMuzzle =
		( ent->curstate.effects & EF_MUZZLEFLASH ) != 0;

	if( ent->player && !viewModel )
	{
		bool spawned = false;

		if( hasEfMuzzle )
		{
			if( dx_muzzleflash_sprite.value != 0.0f )
			{
				int attachment = 0;
				int option = 20;
				R_FindMuzzleFlash(
					seqdesc, hdr, nativeRenderFrame, attachment, option );

				vec3_t muzzleOrigin;
				R_StudioAttachmentPoint( hdr, boneMatrices, world,
					attachment, muzzleOrigin );
				R_SpawnRendererMuzzleFlash( ent, muzzleOrigin,
					option, false, renderTime );

				spawned = true;
			}

			ent->curstate.effects &= ~EF_MUZZLEFLASH;
		}

		state.hadEfMuzzleFlash = false;
		state.lastSequence = sequence;
		state.lastAnimTime = ent->curstate.animtime;
		state.lastFrame = nativeRenderFrame;
		return spawned;
	}

	if( dx_muzzleflash_sprite.value == 0.0f )
	{
		state.hadEfMuzzleFlash = hasEfMuzzle;
		state.lastSequence = sequence;
		state.lastAnimTime = ent->curstate.animtime;
		state.lastFrame = nativeRenderFrame;
		return false;
	}

	const float lastFrame = sequenceChanged ? -1.0f : state.lastFrame;
	const bool animTimeChanged =
		state.lastAnimTime < -9000.0f ||
		fabsf( ent->curstate.animtime - state.lastAnimTime ) > 0.0001f;
	const bool sameSequenceRestart =
		!sequenceChanged && animTimeChanged &&
		nativeRenderFrame + 0.001f < lastFrame;
	bool spawned = false;

	if( seqdesc->numevents > 0 && seqdesc->eventindex > 0 )
	{
		mstudioevent_t *events =
			(mstudioevent_t *)( (byte *)hdr + seqdesc->eventindex );

		for( int i = 0; i < seqdesc->numevents; ++i )
		{
			int attachment = 0;
			if( !R_StudioMuzzleAttachment(
				events[i].event, attachment ) )
			{
				continue;
			}

			if( attachment < 0 || attachment >= MAXSTUDIOATTACHMENTS )
				attachment = 0;

			const float eventFrame =
				static_cast<float>( events[i].frame );
			if( !R_StudioEventFrameCrossed(
				seqdesc, lastFrame, nativeRenderFrame, eventFrame,
				sequenceChanged, sameSequenceRestart ) )
			{
				continue;
			}

			vec3_t muzzleOrigin;
			R_StudioAttachmentPoint( hdr, boneMatrices, world,
				attachment, muzzleOrigin );
			R_SpawnRendererMuzzleFlash( ent, muzzleOrigin,
				atoi( events[i].options ), viewModel, renderTime );
			spawned = true;
		}
	}

	if( hasEfMuzzle && !state.hadEfMuzzleFlash && !spawned )
	{
		vec3_t muzzleOrigin;
		R_StudioAttachmentPoint(
			hdr, boneMatrices, world, 0, muzzleOrigin );
		R_SpawnRendererMuzzleFlash(
			ent, muzzleOrigin, 20, viewModel, renderTime );
		spawned = true;
	}

	state.hadEfMuzzleFlash = hasEfMuzzle;
	state.lastSequence = sequence;
	state.lastAnimTime = ent->curstate.animtime;

	const bool loopingWrap = !sequenceChanged &&
		( seqdesc->flags & STUDIO_LOOPING ) &&
		lastFrame >= nativeLastFrame * 0.75f &&
		nativeRenderFrame <= nativeLastFrame * 0.25f;

	if( sequenceChanged || sameSequenceRestart ||
		loopingWrap || lastFrame < -0.5f )
	{
		state.lastFrame = nativeRenderFrame;
	}
	else
	{
		state.lastFrame = std::max( lastFrame, nativeRenderFrame );
	}

	return spawned;
}

static float R_MuzzleFlashFade( const cl_entity_s *ent,
	float renderTime )
{
	if( !ent || g_rendererMuzzleFlashes.empty( ) )
		return 0.0f;

	const float now = R_CurrentRenderTime( renderTime );
	float bestFade = 0.0f;
	for( const muzzleflash_t &flash : g_rendererMuzzleFlashes )
	{
		if( flash.entity != ent || now > flash.dieTime )
			continue;

		const float life = std::max( 0.001f, flash.dieTime - flash.startTime );
		const float fade = std::max( 0.0f, std::min( 1.0f,
			( flash.dieTime - now ) / life ) );
		bestFade = std::max( bestFade, fade );
	}
	return bestFade;
}

static D3DCOLOR R_ApplyViewmodelMuzzleLight( D3DCOLOR baseLight,
	const cl_entity_s *ent, float renderTime )
{
	if( dx_muzzleflash_viewmodel_light.value == 0.0f || !ent )
		return baseLight;

	const float fade = R_MuzzleFlashFade( ent, renderTime );
	if( fade <= 0.0001f )
		return baseLight;

	const float scale = std::max( 0.0f, std::min( 4.0f,
		dx_muzzleflash_viewmodel_light_scale.value ) );
	const float amount = fade * scale;

	const byte a = static_cast<byte>( ( baseLight >> 24 ) & 0xFF );
	const int r = static_cast<int>( ( baseLight >> 16 ) & 0xFF );
	const int g = static_cast<int>( ( baseLight >> 8 ) & 0xFF );
	const int b = static_cast<int>( baseLight & 0xFF );

	const int addR = static_cast<int>( 180.0f * amount + 0.5f );
	const int addG = static_cast<int>( 110.0f * amount + 0.5f );
	const int addB = static_cast<int>( 35.0f * amount + 0.5f );
	return D3DCOLOR_ARGB( a,
		std::min( 255, r + addR ),
		std::min( 255, g + addG ),
		std::min( 255, b + addB ) );
}

static bool R_MuzzleBeforeViewmodel( )
{
	return dx_muzzleflash_before_viewmodel.value != 0.0f;
}

static void R_DrawMuzzleFlashes( const cl_entity_s *ent )
{
	if( !device || !ent || g_rendererMuzzleFlashes.empty( ) )
		return;

	const float now = R_CurrentRenderTime( 0.0f );
	D3DMATRIX savedWorld{};
	device->GetTransform( D3DTS_WORLD, &savedWorld );
	D3DMATRIX identity;
	DX_MatrixIdentity( identity );
	device->SetTransform( D3DTS_WORLD, &identity );

	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	DX_SetRenderStateCached( D3DRS_ALPHAREF, 0 );
	R_SetLightmapStageEnabled( false );
	R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
		D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_DIFFUSE );
	device->SetFVF( BRUSH_FVF );

	for( size_t i = 0; i < g_rendererMuzzleFlashes.size( ); )
	{
		muzzleflash_t &flash = g_rendererMuzzleFlashes[i];
		if( now > flash.dieTime )
		{
			g_rendererMuzzleFlashes[i] = g_rendererMuzzleFlashes.back( );
			g_rendererMuzzleFlashes.pop_back( );
			continue;
		}

		if( flash.entity != ent )
		{
			++i;
			continue;
		}

		DX_SetRenderStateCached( D3DRS_ZENABLE,
			( !flash.viewModel || dx_muzzleflash_depth_test.value != 0.0f ) ? TRUE : FALSE );

		const float life = std::max( 0.001f, flash.dieTime - flash.startTime );
		const float fade = std::max( 0.0f, std::min( 1.0f,
			( flash.dieTime - now ) / life ) );
		const byte alpha = static_cast<byte>( 255.0f * fade + 0.5f );
		const D3DCOLOR color = D3DCOLOR_ARGB( alpha, 255, 255, 255 );

		float width = 32.0f;
		float height = 32.0f;
		if( flash.textureId > 0 && flash.textureId <= g_texcount )
		{
			width = static_cast<float>( std::max( 1, g_Textures[flash.textureId].width ) );
			height = static_cast<float>( std::max( 1, g_Textures[flash.textureId].height ) );
			DX_Bind( 0, flash.textureId );
		}
		else
		{
			DX_BindRawTexture( 0, nullptr );
			R_SetTextureStage( D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE,
				D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE );
		}

		const float halfWidth = width * flash.scale * 0.5f;
		const float halfHeight = height * flash.scale * 0.5f;
		const float sr = sinf( flash.rotation );
		const float cr = cosf( flash.rotation );
		vec3_t right, up;
		for( int axis = 0; axis < 3; ++axis )
		{
			right[axis] = g_vRight[axis] * cr + g_vUp[axis] * sr;
			up[axis] = g_vRight[axis] * -sr + g_vUp[axis] * cr;
		}

		brush_vertex_t verts[4];
		vec3_t p;
		VectorMA( flash.origin, -halfHeight, up, p );
		VectorMA( p, -halfWidth, right, p );
		verts[0].x = p[0]; verts[0].y = p[1]; verts[0].z = p[2];
		verts[0].diffuse = color; verts[0].u = 0.0f; verts[0].v = 1.0f;

		VectorMA( flash.origin, halfHeight, up, p );
		VectorMA( p, -halfWidth, right, p );
		verts[1].x = p[0]; verts[1].y = p[1]; verts[1].z = p[2];
		verts[1].diffuse = color; verts[1].u = 0.0f; verts[1].v = 0.0f;

		VectorMA( flash.origin, halfHeight, up, p );
		VectorMA( p, halfWidth, right, p );
		verts[2].x = p[0]; verts[2].y = p[1]; verts[2].z = p[2];
		verts[2].diffuse = color; verts[2].u = 1.0f; verts[2].v = 0.0f;

		VectorMA( flash.origin, -halfHeight, up, p );
		VectorMA( p, halfWidth, right, p );
		verts[3].x = p[0]; verts[3].y = p[1]; verts[3].z = p[2];
		verts[3].diffuse = color; verts[3].u = 1.0f; verts[3].v = 1.0f;

		R_DrawScratchPrimitive( D3DPT_TRIANGLEFAN, 2, 4,
			verts, sizeof( brush_vertex_t ) );
		++i;
	}

	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
	device->SetTransform( D3DTS_WORLD, &savedWorld );
	R_SetOpaqueEntityBaseline( );
}

static bool R_StudioNormalsEnabled( )
{
	return dx_studio_normals.value != 0.0f;
}

static bool R_StudioNormalLighting( )
{
	return R_StudioNormalsEnabled( ) &&
		dx_studio_normal_lighting.value != 0.0f;
}

static bool R_StudioUploadNormals( )
{
	return R_StudioNormalsEnabled( ) &&
		dx_studio_upload_normals.value != 0.0f;
}

static float R_StudioDirectLightScale( )
{
	float direct = dx_studio_direct.value;
	if( direct < 0.0f && gEngfuncs.pfnGetCvarPointer )
	{

		static cvar_t *engineDirect = nullptr;
		static bool triedEngineDirect = false;
		if( !triedEngineDirect )
		{
			engineDirect = gEngfuncs.pfnGetCvarPointer( "v_direct" );
			triedEngineDirect = true;
		}
		if( engineDirect )
			direct = engineDirect->value;
	}
	if( direct < 0.0f )
		direct = 0.9f;

	return std::max( 0.75f, std::min( 1.0f, direct ) );
}

static byte R_StudioScaleColorChannel( byte channel, float factor )
{
	return static_cast<byte>( std::max( 0, std::min( 255,
		static_cast<int>( channel * factor + 0.5f ) ) ) );
}

static float R_StudioClassicNormalFactor( float lightCos, float direct = -1.0f )
{
	lightCos = std::max( -1.0f, std::min( 1.0f, lightCos ) );
	if( direct < 0.0f )
		direct = R_StudioDirectLightScale( );
	direct = std::max( 0.0f, std::min( 1.0f, direct ) );

	float modifiedCos = ( lightCos + ( DX_STUDIO_SHADE_LAMBERT - 1.0f ) ) /
		DX_STUDIO_SHADE_LAMBERT;
	modifiedCos = std::max( 0.0f, modifiedCos );
	return std::max( 0.0f, std::min( 1.0f, 1.0f - direct * modifiedCos ) );
}

static float R_StudioFlatshadeFactor( float direct = -1.0f )
{

	if( direct < 0.0f )
		direct = R_StudioDirectLightScale( );
	direct = std::max( 0.0f, std::min( 1.0f, direct ) );
	return std::max( 0.0f, std::min( 1.0f, 1.0f - direct * 0.2f ) );
}

static D3DCOLOR R_StudioApplyDiffuseFactor( D3DCOLOR baseDiffuse, float factor )
{
	const byte a = static_cast<byte>( ( baseDiffuse >> 24 ) & 0xFF );
	const byte r = static_cast<byte>( ( baseDiffuse >> 16 ) & 0xFF );
	const byte g = static_cast<byte>( ( baseDiffuse >> 8 ) & 0xFF );
	const byte b = static_cast<byte>( baseDiffuse & 0xFF );
	return D3DCOLOR_ARGB( a, R_StudioScaleColorChannel( r, factor ),
		R_StudioScaleColorChannel( g, factor ),
		R_StudioScaleColorChannel( b, factor ) );
}

static D3DCOLOR R_StudioApplyLightFactor( D3DCOLOR baseDiffuse,
	float lightCos, float direct = -1.0f )
{
	if( !R_StudioNormalLighting( ) )
		return baseDiffuse;

	return R_StudioApplyDiffuseFactor( baseDiffuse,
		R_StudioClassicNormalFactor( lightCos, direct ) );
}

static void R_StudioLightToEntity( const float worldLightDir[3],
	const D3DMATRIX *world, float out[3] )
{
	if( !world )
	{
		VectorCopy( worldLightDir, out );
		return;
	}

	out[0] = worldLightDir[0] * world->_11 + worldLightDir[1] * world->_12 + worldLightDir[2] * world->_13;
	out[1] = worldLightDir[0] * world->_21 + worldLightDir[1] * world->_22 + worldLightDir[2] * world->_23;
	out[2] = worldLightDir[0] * world->_31 + worldLightDir[1] * world->_32 + worldLightDir[2] * world->_33;
	if( !R_StudioNormalizeVector( out ) )
		VectorCopy( worldLightDir, out );
}

static void R_StudioLightToBone( const float lightDir[3],
	const BoneMat34 bone, float out[3] )
{

	out[0] = lightDir[0] * bone[0][0] + lightDir[1] * bone[1][0] + lightDir[2] * bone[2][0];
	out[1] = lightDir[0] * bone[0][1] + lightDir[1] * bone[1][1] + lightDir[2] * bone[2][1];
	out[2] = lightDir[0] * bone[0][2] + lightDir[1] * bone[1][2] + lightDir[2] * bone[2][2];
}

bool R_StudioRangeValid( const studiohdr_t *hdr, int offset, size_t bytes )
{
	if( !hdr || hdr->length < static_cast<int>( sizeof( studiohdr_t ) ) || offset < 0 )
		return false;
	const size_t length = static_cast<size_t>( hdr->length );
	const size_t begin = static_cast<size_t>( offset );
	return begin <= length && bytes <= length - begin;
}

bool R_StudioHeaderValid( const studiohdr_t *hdr )
{
	if( !hdr || hdr->length < static_cast<int>( sizeof( studiohdr_t ) ) ||
		hdr->numbones <= 0 || hdr->numbones > MAXSTUDIOBONES ||
		hdr->numbodyparts < 0 || hdr->numbodyparts > 1024 ||
		hdr->numseq <= 0 || hdr->numseq > 4096 ||
		hdr->numtextures < 0 || hdr->numtextures > 4096 ||
		hdr->numskinref < 0 || hdr->numskinref > 4096 ||
		hdr->numskinfamilies < 0 || hdr->numskinfamilies > 4096 )
		return false;

	if( !R_StudioRangeValid( hdr, hdr->boneindex,
		static_cast<size_t>( hdr->numbones ) * sizeof( mstudiobone_t ) ) ||
		!R_StudioRangeValid( hdr, hdr->seqindex,
			static_cast<size_t>( hdr->numseq ) * sizeof( mstudioseqdesc_t ) ) )
		return false;

	return hdr->numbodyparts == 0 || R_StudioRangeValid( hdr, hdr->bodypartindex,
		static_cast<size_t>( hdr->numbodyparts ) * sizeof( mstudiobodyparts_t ) );
}

static bool R_StudioModelValid( const studiohdr_t *hdr, const mstudiomodel_t *model )
{
	if( !hdr || !model || model->numverts < 0 || model->numverts > 65535 ||
		model->numnorms < 0 || model->numnorms > 65535 ||
		model->nummesh < 0 || model->nummesh > 4096 )
		return false;

	if( model->numverts > 0 &&
		( !R_StudioRangeValid( hdr, model->vertindex,
			static_cast<size_t>( model->numverts ) * sizeof( vec3_t ) ) ||
			!R_StudioRangeValid( hdr, model->vertinfoindex,
				static_cast<size_t>( model->numverts ) * sizeof( byte ) ) ) )
		return false;

	if( model->numnorms > 0 &&
		( !R_StudioRangeValid( hdr, model->normindex,
			static_cast<size_t>( model->numnorms ) * sizeof( vec3_t ) ) ||
			!R_StudioRangeValid( hdr, model->norminfoindex,
				static_cast<size_t>( model->numnorms ) * sizeof( byte ) ) ) )
		return false;

	return model->nummesh == 0 || R_StudioRangeValid( hdr, model->meshindex,
		static_cast<size_t>( model->nummesh ) * sizeof( mstudiomesh_t ) );
}

static bool R_StudioCommandSpan( const studiohdr_t *hdr, int triindex,
	short *&command, const short *&end )
{
	if( !R_StudioRangeValid( hdr, triindex, sizeof( short ) ) )
		return false;
	byte *base = reinterpret_cast<byte *>( const_cast<studiohdr_t *>( hdr ) );
	command = reinterpret_cast<short *>( base + triindex );
	end = reinterpret_cast<const short *>( base + hdr->length );
	return command < end;
}

void ExtractStudioModelBatched( studiohdr_t *hdr, model_drawdata_t &out )
{
	out.renderdata.clear( );
	out.texIds.clear( );
	if( !R_StudioHeaderValid( hdr ) )
		return;

	BoneMat34 boneMats[MAXSTUDIOBONES];
	StudioCalcBoneTransformsInternal( nullptr, hdr, 0, 0, boneMats, -1, 0.0f, 0.0f, nullptr, 0.0f );

	auto body = (mstudiobodyparts_t *)( (byte *)hdr + hdr->bodypartindex );

	struct Key { int v; short s, t; bool operator==( const Key &o ) const { return v == o.v && s == o.s && t == o.t; } };
	struct Hash
	{
		size_t operator()( const Key &k ) const noexcept
		{
			const std::uint64_t packed =
				( static_cast<std::uint64_t>( static_cast<std::uint32_t>( k.v ) ) << 32 ) |
				( static_cast<std::uint64_t>( static_cast<std::uint16_t>( k.s ) ) << 16 ) |
				static_cast<std::uint16_t>( k.t );
			return dxfast::MumxHash64( packed );
		}
	};

	auto getBatch = [&]( int texId ) -> renderdata_t &
		{
			for( auto &b : out.renderdata )
				if( b.texnum == texId ) return b;
			out.renderdata.emplace_back( );
			out.renderdata.back( ).texnum = texId;
			out.texIds.push_back( texId );
			return out.renderdata.back( );
		};

	for( int b = 0; b < hdr->numbodyparts; ++b )
	{
		auto model = StudioGetSelectedBodypartModel( hdr, &body[b], 0 );
		if( !model || !R_StudioModelValid( hdr, model ) )
			continue;
		{
			vec3_t *pos = (vec3_t *)( (byte *)hdr + model->vertindex );
			byte *vertbone = (byte *)( (byte *)hdr + model->vertinfoindex );
			auto mesh = (mstudiomesh_t *)( (byte *)hdr + model->meshindex );

			for( int j = 0; j < model->nummesh; ++j )
			{
				auto &ms = mesh[j];
				mstudiotexture_t *studioTexture =
					R_StudioGetTextureForSkin( hdr, &ms, 0 );
				int texId = studioTexture ? studioTexture->index : -1;
				if( texId <= 0 || texId > g_texcount )
					continue;
				const bool floatUv = ( R_StudioTextureFlags( studioTexture ) &
					STUDIO_NF_UV_COORDS ) != 0;
				auto &batch = getBatch( texId );
				dxfast::HashMap<Key, int, Hash> remap;

				float tw = (float)g_Textures[texId].width;
				float th = (float)g_Textures[texId].height;

				short *cmd = nullptr;
				const short *cmdEnd = nullptr;
				if( !R_StudioCommandSpan( hdr, ms.triindex, cmd, cmdEnd ) )
					continue;

				while( cmd < cmdEnd && *cmd )
				{
					int count = *cmd++;
					const bool fan = count < 0;
					if( fan )
					{
						if( count == std::numeric_limits<short>::min( ) ) break;
						count = -count;
					}
					if( count < 3 || count > 256 ||
						static_cast<size_t>( cmdEnd - cmd ) < static_cast<size_t>( count ) * 4u )
						break;

					int tmp[256];

					for( int i = 0; i < count; ++i )
					{
						const int vertIdx = cmd[0];
						const short s = cmd[2];
						const short t = cmd[3];
						cmd += 4;
						if( vertIdx < 0 || vertIdx >= model->numverts )
						{
							count = 0; break;
						}

						Key k{ vertIdx, s, t };
						auto found = remap.find( k );
						if( found != remap.end( ) )
						{
							tmp[i] = found->second;
							continue;
						}

						const int boneIdx = vertbone[vertIdx];
						if( boneIdx < 0 || boneIdx >= hdr->numbones )
						{
							count = 0; break;
						}
						float inPos[3] = { pos[vertIdx][0], pos[vertIdx][1], pos[vertIdx][2] };
						float skinnedPos[3];
						VectorTransform( inPos, boneMats[boneIdx], skinnedPos );

						brush_vertex_t v;
						v.x = skinnedPos[0]; v.y = skinnedPos[1]; v.z = skinnedPos[2];
						v.diffuse = 0xFFFFFFFF;
						if( floatUv )
						{
							v.u = R_StudioHalfToFloat( s );
							v.v = R_StudioHalfToFloat( t );
						}
						else
						{
							v.u = static_cast<float>( s ) / tw;
							v.v = static_cast<float>( t ) / th;
						}

						int idx = (int)batch.verts.size( );
						batch.verts.push_back( v );
						remap.emplace( k, idx );
						tmp[i] = idx;
					}
					if( count < 3 )
						break;

					if( fan )
					{
						for( int i = 1; i < count - 1; ++i )
						{
							batch.indices.push_back( tmp[0] );
							batch.indices.push_back( tmp[i] );
							batch.indices.push_back( tmp[i + 1] );
						}
					}
					else
					{
						for( int i = 0; i < count - 2; ++i )
						{
							if( i & 1 )
							{
								batch.indices.push_back( tmp[i + 1] );
								batch.indices.push_back( tmp[i] );
								batch.indices.push_back( tmp[i + 2] );
							}
							else
							{
								batch.indices.push_back( tmp[i] );
								batch.indices.push_back( tmp[i + 1] );
								batch.indices.push_back( tmp[i + 2] );
							}
						}
					}
				}
			}
		}
	}

	for( auto &batch : out.renderdata )
	{
		batch.vertCount = (int)batch.verts.size( );
		batch.indexCount = (int)batch.indices.size( );
		batch.primitiveCount = batch.indexCount / 3;
	}
}

static void StudioSkinModelInternal( model_t *mod, studiohdr_t *hdr, int seq, float frame,
	model_drawdata_t &out, int gaitSeq, float gaitFrame, float gaitYaw,
	int entityBody, int entitySkin, const cl_entity_s *ent, float renderTime,
	D3DCOLOR vertexDiffuse, const float studioWorldLightDir[3], float directLightScale,
	const D3DMATRIX *currentWorldMatrix, const BoneMat34 *precomputedBoneMatrices = nullptr )
{
	out.renderdata.clear( );
	out.texIds.clear( );
	if( !R_StudioHeaderValid( hdr ) )
		return;

	BoneMat34 localBoneMats[MAXSTUDIOBONES];
	const BoneMat34 *boneMats = precomputedBoneMatrices;
	if( !boneMats )
	{
		StudioCalcBoneTransformsInternal( mod, hdr, seq, frame, localBoneMats,
			gaitSeq, gaitFrame, gaitYaw, ent, renderTime );
		boneMats = localBoneMats;
	}

	vec3_t cpuLocalLightDir;
	if( studioWorldLightDir )
		R_StudioLightToEntity( studioWorldLightDir, currentWorldMatrix, cpuLocalLightDir );
	else
		VectorSet( cpuLocalLightDir, 0.0f, 0.0f, -1.0f );
	directLightScale = std::max( 0.0f, std::min( 1.0f, directLightScale ) );
	studio_local_light_t localLights[DX_STUDIO_MAX_LOCAL_LIGHTS];
	const int localLightCount = R_StudioLocalLights( ent, currentWorldMatrix, localLights );

	D3DMATRIX chromeWorld;
	bool haveChromeWorld = false;

	auto body = (mstudiobodyparts_t *)( (byte *)hdr + hdr->bodypartindex );

	struct Key { int v, n; short s, t; bool operator==( const Key &o ) const { return v == o.v && n == o.n && s == o.s && t == o.t; } };
	struct Hash
	{
		size_t operator()( const Key &k ) const noexcept
		{
			const std::uint64_t first =
				( static_cast<std::uint64_t>( static_cast<std::uint32_t>( k.v ) ) << 32 ) |
				static_cast<std::uint32_t>( k.n );
			const std::uint64_t second =
				( static_cast<std::uint64_t>( static_cast<std::uint16_t>( k.s ) ) << 16 ) |
				static_cast<std::uint16_t>( k.t );
			return dxfast::MumxHashPair64( first, second );
		}
	};

	auto getBatch = [&]( int texId, unsigned int materialFlags ) -> renderdata_t &
		{
			const int batchKey = R_StudioCpuBatchKey( texId, materialFlags );
			for( auto &b : out.renderdata )
				if( b.texnum == batchKey ) return b;
			out.renderdata.emplace_back( );
			out.renderdata.back( ).texnum = batchKey;
			out.texIds.push_back( batchKey );
			return out.renderdata.back( );
		};

	for( int b = 0; b < hdr->numbodyparts; ++b )
	{
		auto model = StudioGetSelectedBodypartModel( hdr, &body[b], entityBody );
		if( !model || !R_StudioModelValid( hdr, model ) )
			continue;
		{
			vec3_t *pos = (vec3_t *)( (byte *)hdr + model->vertindex );
			byte *vertbone = (byte *)( (byte *)hdr + model->vertinfoindex );
			vec3_t *normals = (vec3_t *)( (byte *)hdr + model->normindex );
			byte *normalbone = (byte *)( (byte *)hdr + model->norminfoindex );
			auto mesh = (mstudiomesh_t *)( (byte *)hdr + model->meshindex );

			for( int j = 0; j < model->nummesh; ++j )
			{
				auto &ms = mesh[j];
				mstudiotexture_t *studioTexture =
					R_StudioGetTextureForSkin( hdr, &ms, entitySkin );
				int texId = studioTexture ? studioTexture->index : -1;
				if( texId <= 0 || texId > g_texcount )
					continue;
				const unsigned int materialFlags = R_StudioTextureFlags( studioTexture );
				const bool chrome = ( materialFlags & STUDIO_NF_CHROME ) != 0;
				const bool fullbright = ( materialFlags & STUDIO_NF_FULLBRIGHT ) != 0;
				const bool flatshade = ( materialFlags & STUDIO_NF_FLATSHADE ) != 0;
				const bool floatUv = ( materialFlags & STUDIO_NF_UV_COORDS ) != 0;
				const bool materialNormalLighting = R_StudioNormalLighting( ) &&
					!fullbright && !flatshade;
				D3DCOLOR materialDiffuse = fullbright
					? R_StudioFullbrightDiffuse( vertexDiffuse ) : vertexDiffuse;
				if( R_StudioNormalLighting( ) && flatshade && !fullbright )
					materialDiffuse = R_StudioApplyDiffuseFactor( materialDiffuse,
						R_StudioFlatshadeFactor( directLightScale ) );
				if( chrome && !haveChromeWorld )
				{
					device->GetTransform( D3DTS_WORLD, &chromeWorld );
					haveChromeWorld = true;
				}
				auto &batch = getBatch( texId, materialFlags );
				dxfast::HashMap<Key, int, Hash> remap;

				float tw = (float)g_Textures[texId].width;
				float th = (float)g_Textures[texId].height;

				short *cmd = nullptr;
				const short *cmdEnd = nullptr;
				if( !R_StudioCommandSpan( hdr, ms.triindex, cmd, cmdEnd ) )
					continue;

				while( cmd < cmdEnd && *cmd )
				{
					int count = *cmd++;
					const bool fan = count < 0;
					if( fan )
					{
						if( count == std::numeric_limits<short>::min( ) ) break;
						count = -count;
					}
					if( count < 3 || count > 256 ||
						static_cast<size_t>( cmdEnd - cmd ) < static_cast<size_t>( count ) * 4u )
						break;

					int tmp[256];

					for( int i = 0; i < count; ++i )
					{
						const int vertIdx = cmd[0];
						const int normalIdx = cmd[1];
						const short s = cmd[2];
						const short t = cmd[3];
						cmd += 4;
						if( vertIdx < 0 || vertIdx >= model->numverts )
						{
							count = 0; break;
						}

						Key k{ vertIdx, normalIdx, s, t };
						auto found = remap.find( k );
						if( found != remap.end( ) )
						{
							tmp[i] = found->second;
							continue;
						}

						const int boneIdx = vertbone[vertIdx];
						if( boneIdx < 0 || boneIdx >= hdr->numbones )
						{
							count = 0; break;
						}
						float inPos[3] = { pos[vertIdx][0], pos[vertIdx][1], pos[vertIdx][2] };
						float skinnedPos[3];
						VectorTransform( inPos, boneMats[boneIdx], skinnedPos );

						float skinnedNormal[3] = { 0.0f, 0.0f, 1.0f };
						if( ( materialNormalLighting || chrome ) && normalIdx >= 0 && normalIdx < model->numnorms &&
							model->normindex > 0 && model->norminfoindex > 0 )
						{
							const int normalBoneIndex = normalbone[normalIdx] < hdr->numbones
								? normalbone[normalIdx] : boneIdx;
							const float localNormal[3] = { normals[normalIdx][0],
								normals[normalIdx][1], normals[normalIdx][2] };
							R_StudioRotateVectorByBone( localNormal, boneMats[normalBoneIndex],
								skinnedNormal );
							R_StudioNormalizeVector( skinnedNormal );
						}

						brush_vertex_t v;
						v.x = skinnedPos[0]; v.y = skinnedPos[1]; v.z = skinnedPos[2];
						D3DCOLOR litDiffuse = materialNormalLighting
							? R_StudioApplyLightFactor( materialDiffuse,
								DotProduct( skinnedNormal, cpuLocalLightDir ), directLightScale )
							: materialDiffuse;
						if( localLightCount > 0 )
							litDiffuse = R_StudioApplyLocalLights( litDiffuse, skinnedPos,
								skinnedNormal, localLights, localLightCount );
						v.diffuse = litDiffuse;
						if( chrome && normalIdx >= 0 && normalIdx < model->numnorms && model->norminfoindex > 0 )
						{
							const int normalBoneIndex = normalbone[normalIdx] < hdr->numbones
								? normalbone[normalIdx] : boneIdx;
							R_StudioBuildChromeUV( chromeWorld, boneMats,
								normalBoneIndex, skinnedNormal, tw, th, v.u, v.v );
						}
						else if( floatUv )
						{
							v.u = R_StudioHalfToFloat( s );
							v.v = R_StudioHalfToFloat( t );
						}
						else
						{
							v.u = static_cast<float>( s ) / tw;
							v.v = static_cast<float>( t ) / th;
						}

						int idx = (int)batch.verts.size( );
						batch.verts.push_back( v );
						remap.emplace( k, idx );
						tmp[i] = idx;
					}
					if( count < 3 )
						break;

					if( fan )
					{
						for( int i = 1; i < count - 1; ++i )
						{
							batch.indices.push_back( tmp[0] );
							batch.indices.push_back( tmp[i] );
							batch.indices.push_back( tmp[i + 1] );
						}
					}
					else
					{
						for( int i = 0; i < count - 2; ++i )
						{
							if( i & 1 )
							{
								batch.indices.push_back( tmp[i + 1] );
								batch.indices.push_back( tmp[i] );
								batch.indices.push_back( tmp[i + 2] );
							}
							else
							{
								batch.indices.push_back( tmp[i] );
								batch.indices.push_back( tmp[i + 1] );
								batch.indices.push_back( tmp[i + 2] );
							}
						}
					}
				}
			}
		}
	}

	for( auto &batch : out.renderdata )
	{
		batch.vertCount = (int)batch.verts.size( );
		batch.indexCount = (int)batch.indices.size( );
		batch.primitiveCount = batch.indexCount / 3;
	}
}

void R_StudioStoreLocalLights( studio_cache_t &cache,
	const studio_local_light_t *lights, int count )
{
	cache.localLightCount = std::max( 0, std::min( DX_STUDIO_MAX_LOCAL_LIGHTS, count ) );
	for( int i = 0; i < cache.localLightCount; ++i )
		cache.localLights[i] = lights ? lights[i] : studio_local_light_t{};
}

static studio_cache_t &R_GetStudioEntityGpuCache( const cl_entity_s *ent )
{
	entity_cache_t &entityCache = R_GetEntityCache( ent );
	if( entityCache.studioGpu )
		return *entityCache.studioGpu;

	g_studioEntityGpuCacheStorage.emplace_back( );
	entityCache.studioGpu = &g_studioEntityGpuCacheStorage.back( );
	return *entityCache.studioGpu;
}

static bool R_StudioCanSmoothGpuPose( studiohdr_t *hdr )
{
	if( dx_studio_gpu_smooth.value == 0.0f ||
		dx_studio_gpu_skinning.value == 0.0f || !hdr )
	{
		return false;
	}
	if( !DX_EnsureStudioSkinningShaders( ) )
		return false;
	const int boneCount = std::max( 0, std::min( MAXSTUDIOBONES, hdr->numbones ) );
	return boneCount > 0 && boneCount <= DX_StudioGpuSkinningBoneCap( );
}

static bool R_StudioGpuSkinningForCache( const studio_cache_t &cache )
{
	if( dx_studio_gpu_skinning.value == 0.0f || cache.gpuSkinningBoneCount <= 0 )
	{
		return false;
	}
	if( cache.containsChrome && dx_studio_gpu_chrome.value == 0.0f )
		return false;
	if( !DX_EnsureStudioSkinningShaders( ) )
		return false;
	return cache.gpuSkinningBoneCount <= DX_StudioGpuSkinningBoneCap( );
}

static int R_StudioPoseBudget( )
{
	return std::max( 0, std::min( 1024,
		static_cast<int>( dx_studio_update_budget.value ) ) );
}

static bool R_StudioPoseBudgetOK( bool isViewModel,
	const studio_cache_t &cache )
{

	if( cache.gpuSkinningBufferMode && dx_studio_gpu_smooth.value != 0.0f )
		return true;
	if( isViewModel && dx_studio_viewmodel_throttle.value == 0.0f )
		return true;
	if( !cache.poseUploaded )
		return true;
	const int budget = R_StudioPoseBudget( );
	if( budget <= 0 )
		return true;
	if( g_studioPoseUpdatesThisFrame >= budget )
		return false;
	++g_studioPoseUpdatesThisFrame;
	return true;
}

static void R_FreeStudioBuffers( studio_cache_t &cache )
{
	if( cache.combinedVertexBuffer )
	{
		cache.combinedVertexBuffer->Release( );
		cache.combinedVertexBuffer = nullptr;
	}
	if( cache.gpuSkinSourceVertexBuffer )
	{
		cache.gpuSkinSourceVertexBuffer->Release( );
		cache.gpuSkinSourceVertexBuffer = nullptr;
	}
	cache.combinedVertexCount = 0;
	cache.combinedVertexBufferMode = false;
	cache.gpuSkinSourceVertexCount = 0;
	cache.gpuSkinningBufferMode = false;
	cache.gpuSkinningPoseValid = false;

	for( auto &batch : cache.batches )
	{
		if( batch.vertexBuffer )
		{
			batch.vertexBuffer->Release( );
			batch.vertexBuffer = nullptr;
		}
		if( batch.indexBuffer )
		{
			batch.indexBuffer->Release( );
			batch.indexBuffer = nullptr;
		}
	}
	cache.poseUploaded = false;
	cache.buffersUploadNormals = false;
}

static void R_DestroyStudioGpuCache( studio_cache_t &cache )
{
	R_FreeStudioBuffers( cache );
	cache.batches.clear( );
	cache.model = nullptr;
	cache.body = 0;
	cache.skin = 0;
	cache.topologyValid = false;
	cache.poseUploaded = false;
	cache.containsChrome = false;
}

void R_ClearStudioGpuCaches( )
{
	for( auto &cache : g_studioEntityGpuCacheStorage )
		R_DestroyStudioGpuCache( cache );
	g_studioEntityGpuCacheStorage.clear( );
	R_ForEachEntityCache( []( entity_cache_t &entityCache )
	{
		entityCache.studioGpu = nullptr;
		entityCache.lightProbe = studio_light_probe_cache_t{};
	} );
	g_playerGaitStates.fill( player_gait_state_t{} );
	g_viewModelLightProbeCache.valid = false;
}

void R_ModelsLostDevice( )
{
	DX_ShutdownWorkers( );
	R_FreeModelScratchBuffers( );
	DX_FreeStudioSkinningShaders( );
	R_ReleaseGlowOcclusionQueries( );
	R_InvalidateWorldVis( );
	DX_InvalidateTextureBindCache( );
	for( auto &cache : g_studioEntityGpuCacheStorage )
		R_FreeStudioBuffers( cache );
	for( auto &cache : g_worldLightmapStorage )
	{
		for( auto &batch : cache.waterBatches )
			R_FreeWaterBatches( batch, false );
	}
	R_FreeRippleCaches( false );

}

static bool R_CreateStudioBuffers( studio_cache_t &cache )
{
	const bool studioNormals = R_StudioNormalsEnabled( );
	const bool uploadNormals = R_StudioUploadNormals( );
	const bool gpuSkinning = R_StudioGpuSkinningForCache( cache );
	const bool combinedVB = !gpuSkinning;
	if( cache.buffersUploadNormals != uploadNormals ||
		cache.combinedVertexBufferMode != combinedVB ||
		cache.gpuSkinningBufferMode != gpuSkinning )
	{
		for( auto &batch : cache.batches )
		{
			if( batch.vertexBuffer )
			{
				batch.vertexBuffer->Release( );
				batch.vertexBuffer = nullptr;
			}
		}
		if( cache.combinedVertexBuffer )
		{
			cache.combinedVertexBuffer->Release( );
			cache.combinedVertexBuffer = nullptr;
		}
		if( cache.gpuSkinSourceVertexBuffer )
		{
			cache.gpuSkinSourceVertexBuffer->Release( );
			cache.gpuSkinSourceVertexBuffer = nullptr;
		}
		cache.combinedVertexCount = 0;
		cache.gpuSkinSourceVertexCount = 0;
		cache.poseUploaded = false;
		cache.gpuSkinningPoseValid = false;
		cache.buffersUploadNormals = uploadNormals;
		cache.combinedVertexBufferMode = combinedVB;
		cache.gpuSkinningBufferMode = gpuSkinning;
	}

	const UINT vertexStride = uploadNormals
		? sizeof( studio_normal_lit_vertex_t )
		: ( studioNormals ? sizeof( studio_fast_lit_vertex_t ) : sizeof( brush_vertex_t ) );
	const DWORD vertexFVF = uploadNormals
		? STUDIO_NORMAL_LIT_FVF
		: ( studioNormals ? STUDIO_FAST_LIT_FVF : BRUSH_FVF );

	UINT combinedVertexCount = 0;
	for( auto &batch : cache.batches )
	{
		batch.vertexCount = static_cast<UINT>( batch.sources.size( ) );
		batch.primitiveCount = static_cast<UINT>( batch.indices.size( ) / 3 );
		batch.vertexStart = combinedVertexCount;
		if( batch.vertexCount != 0 && batch.primitiveCount != 0 )
			combinedVertexCount += batch.vertexCount;
	}

	if( gpuSkinning )
	{
		for( auto &batch : cache.batches )
		{
			if( batch.vertexBuffer )
			{
				batch.vertexBuffer->Release( );
				batch.vertexBuffer = nullptr;
			}
		}
		if( cache.combinedVertexBuffer )
		{
			cache.combinedVertexBuffer->Release( );
			cache.combinedVertexBuffer = nullptr;
			cache.combinedVertexCount = 0;
		}

		if( combinedVertexCount == 0 )
			return false;

		if( !cache.gpuSkinSourceVertexBuffer ||
			cache.gpuSkinSourceVertexCount != combinedVertexCount )
		{
			if( cache.gpuSkinSourceVertexBuffer )
			{
				cache.gpuSkinSourceVertexBuffer->Release( );
				cache.gpuSkinSourceVertexBuffer = nullptr;
			}

			UINT sourceVertexBytes = 0;
			if( !R_UIntMul( combinedVertexCount, static_cast<UINT>( sizeof( studio_skin_vertex_t ) ), sourceVertexBytes ) ||
				sourceVertexBytes > DX_SCRATCH_MAX_BYTES )
				return false;
			HRESULT result = device->CreateVertexBuffer(
				sourceVertexBytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
				&cache.gpuSkinSourceVertexBuffer, nullptr );
			if( FAILED( result ) || !cache.gpuSkinSourceVertexBuffer )
			{
				R_FreeStudioBuffers( cache );
				return false;
			}

			void *sourceMemory = nullptr;
			result = cache.gpuSkinSourceVertexBuffer->Lock( 0, 0, &sourceMemory, 0 );
			if( FAILED( result ) || !sourceMemory )
			{
				R_FreeStudioBuffers( cache );
				return false;
			}

			auto *destination = static_cast<studio_skin_vertex_t *>( sourceMemory );
			for( const auto &batch : cache.batches )
			{
				for( size_t sourceIndex = 0; sourceIndex < batch.sources.size( ); ++sourceIndex )
				{
					const studio_source_vertex_t &source = batch.sources[sourceIndex];
					studio_skin_vertex_t &vertex =
						destination[batch.vertexStart + static_cast<UINT>( sourceIndex )];
					const int bone = source.bone < MAXSTUDIOBONES ? source.bone : 0;
					const int normalBone = source.normalBone < MAXSTUDIOBONES
						? source.normalBone : bone;
					vertex.x = source.x; vertex.y = source.y; vertex.z = source.z;
					vertex.nx = source.nx; vertex.ny = source.ny; vertex.nz = source.nz;
					vertex.u = source.u; vertex.v = source.v;
					vertex.boneConstBase = static_cast<float>(
						DX_STUDIO_GPU_SKIN_CONST_BONES + bone * 3 );
					vertex.normalBoneConstBase = static_cast<float>(
						DX_STUDIO_GPU_SKIN_CONST_BONES + normalBone * 3 );
					vertex.chromeFlag = batch.chrome ? 1.0f : 0.0f;
				}
			}

			cache.gpuSkinSourceVertexBuffer->Unlock( );
			cache.gpuSkinSourceVertexCount = combinedVertexCount;
			cache.poseUploaded = false;
			cache.gpuSkinningPoseValid = false;
		}
	}

	if( combinedVB )
	{
		for( auto &batch : cache.batches )
		{
			if( batch.vertexBuffer )
			{
				batch.vertexBuffer->Release( );
				batch.vertexBuffer = nullptr;
			}
		}

		if( combinedVertexCount == 0 )
			return false;

		if( !cache.combinedVertexBuffer || cache.combinedVertexCount != combinedVertexCount )
		{
			if( cache.combinedVertexBuffer )
			{
				cache.combinedVertexBuffer->Release( );
				cache.combinedVertexBuffer = nullptr;
			}

			UINT combinedVertexBytes = 0;
			if( !R_UIntMul( combinedVertexCount, vertexStride, combinedVertexBytes ) ||
				combinedVertexBytes > DX_SCRATCH_MAX_BYTES )
				return false;
			HRESULT result = device->CreateVertexBuffer(
				combinedVertexBytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
				vertexFVF, D3DPOOL_DEFAULT, &cache.combinedVertexBuffer, nullptr );
			if( FAILED( result ) || !cache.combinedVertexBuffer )
			{
				R_FreeStudioBuffers( cache );
				return false;
			}
			cache.combinedVertexCount = combinedVertexCount;
			cache.poseUploaded = false;
		}
	}

	for( auto &batch : cache.batches )
	{
		if( batch.vertexCount == 0 || batch.primitiveCount == 0 )
			continue;

		if( !combinedVB && !gpuSkinning && !batch.vertexBuffer )
		{
			UINT batchVertexBytes = 0;
			if( !R_UIntMul( batch.vertexCount, vertexStride, batchVertexBytes ) ||
				batchVertexBytes > DX_SCRATCH_MAX_BYTES )
				return false;
			HRESULT result = device->CreateVertexBuffer(
				batchVertexBytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
				vertexFVF, D3DPOOL_DEFAULT, &batch.vertexBuffer, nullptr );
			if( FAILED( result ) || !batch.vertexBuffer )
			{
				R_FreeStudioBuffers( cache );
				return false;
			}
		}

		if( batch.indexBuffer )
			continue;

		HRESULT result = device->CreateIndexBuffer(
			static_cast<UINT>( batch.indices.size( ) * sizeof( DWORD ) ),
			D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED,
			&batch.indexBuffer, nullptr );
		if( FAILED( result ) || !batch.indexBuffer )
		{
			R_FreeStudioBuffers( cache );
			return false;
		}

		void *indexMemory = nullptr;
		result = batch.indexBuffer->Lock( 0, 0, &indexMemory, 0 );
		if( FAILED( result ) || !indexMemory )
		{
			R_FreeStudioBuffers( cache );
			return false;
		}
		memcpy( indexMemory, batch.indices.data( ), batch.indices.size( ) * sizeof( DWORD ) );
		batch.indexBuffer->Unlock( );
	}

	cache.buffersUploadNormals = uploadNormals;
	cache.combinedVertexBufferMode = combinedVB;
	cache.gpuSkinningBufferMode = gpuSkinning;
	return true;
}

static studio_batch_t &R_FindStudioGpuBatch(
	studio_cache_t &cache, int textureId, unsigned int materialFlags )
{
	for( auto &batch : cache.batches )
	{
		if( batch.textureId == textureId && batch.materialFlags == materialFlags )
			return batch;
	}

	studio_batch_t batch;
	batch.textureId = textureId;
	batch.materialFlags = materialFlags;
	batch.chrome = ( materialFlags & STUDIO_NF_CHROME ) != 0;
	cache.batches.push_back( std::move( batch ) );
	return cache.batches.back( );
}

static bool R_BuildStudioTopology( studio_cache_t &cache,
	model_t *mod, studiohdr_t *hdr, int entityBody, int entitySkin )
{
	R_DestroyStudioGpuCache( cache );
	if( !mod || !R_StudioHeaderValid( hdr ) )
		return false;

	cache.model = mod;
	cache.body = entityBody;
	cache.skin = entitySkin;
	cache.gpuSkinningBoneCount = std::max( 0, std::min( MAXSTUDIOBONES, hdr->numbones ) );

	auto *bodyparts = reinterpret_cast<mstudiobodyparts_t *>(
		reinterpret_cast<byte *>( hdr ) + hdr->bodypartindex );

	struct mesh_vertex_key_t
	{
		int vertexIndex;
		int normalIndex;
		short s;
		short t;

		bool operator==( const mesh_vertex_key_t &other ) const
		{
			return vertexIndex == other.vertexIndex && normalIndex == other.normalIndex &&
				s == other.s && t == other.t;
		}
	};
	struct mesh_vertex_key_hash_t
	{
		size_t operator()( const mesh_vertex_key_t &key ) const noexcept
		{
			const std::uint64_t first =
				( static_cast<std::uint64_t>( static_cast<std::uint32_t>( key.vertexIndex ) ) << 32 ) |
				static_cast<std::uint32_t>( key.normalIndex );
			const std::uint64_t second =
				( static_cast<std::uint64_t>( static_cast<std::uint16_t>( key.s ) ) << 16 ) |
				static_cast<std::uint16_t>( key.t );
			return dxfast::MumxHashPair64( first, second );
		}
	};

	for( int bodypartIndex = 0; bodypartIndex < hdr->numbodyparts; ++bodypartIndex )
	{
		mstudiomodel_t *model = StudioGetSelectedBodypartModel(
			hdr, &bodyparts[bodypartIndex], entityBody );
		if( !model || !R_StudioModelValid( hdr, model ) )
			continue;

		auto *positions = reinterpret_cast<vec3_t *>(
			reinterpret_cast<byte *>( hdr ) + model->vertindex );
		auto *vertexBones = reinterpret_cast<byte *>(
			reinterpret_cast<byte *>( hdr ) + model->vertinfoindex );
		auto *normals = reinterpret_cast<vec3_t *>(
			reinterpret_cast<byte *>( hdr ) + model->normindex );
		auto *normalBones = reinterpret_cast<byte *>(
			reinterpret_cast<byte *>( hdr ) + model->norminfoindex );
		auto *meshes = reinterpret_cast<mstudiomesh_t *>(
			reinterpret_cast<byte *>( hdr ) + model->meshindex );

		for( int meshIndex = 0; meshIndex < model->nummesh; ++meshIndex )
		{
			mstudiomesh_t &mesh = meshes[meshIndex];
			mstudiotexture_t *studioTexture =
				R_StudioGetTextureForSkin( hdr, &mesh, entitySkin );
			const int textureId = studioTexture ? studioTexture->index : -1;
			if( textureId <= 0 || textureId > g_texcount )
				continue;

			const unsigned int materialFlags = R_StudioTextureFlags( studioTexture );
			const bool chrome = ( materialFlags & STUDIO_NF_CHROME ) != 0;
			const bool floatUv = ( materialFlags & STUDIO_NF_UV_COORDS ) != 0;
			cache.containsChrome |= chrome;
			studio_batch_t &batch = R_FindStudioGpuBatch( cache, textureId, materialFlags );
			const float textureWidth = static_cast<float>(
				std::max( 1, g_Textures[textureId].width ) );
			const float textureHeight = static_cast<float>(
				std::max( 1, g_Textures[textureId].height ) );

			dxfast::HashMap<mesh_vertex_key_t, DWORD, mesh_vertex_key_hash_t> remap;
			short *command = nullptr;
			const short *commandEnd = nullptr;
			if( !R_StudioCommandSpan( hdr, mesh.triindex, command, commandEnd ) )
				continue;

			while( command < commandEnd && *command )
			{
				int count = *command++;
				const bool fan = count < 0;
				if( fan )
				{
					if( count == std::numeric_limits<short>::min( ) ) break;
					count = -count;
				}
				if( count < 3 || count > 256 ||
					static_cast<size_t>( commandEnd - command ) < static_cast<size_t>( count ) * 4u )
					break;

				DWORD polygon[256];
				for( int vertexInPolygon = 0; vertexInPolygon < count; ++vertexInPolygon )
				{
					const int vertexIndex = command[0];
					const int normalIndex = command[1];
					const short s = command[2];
					const short t = command[3];
					command += 4;
					if( vertexIndex < 0 || vertexIndex >= model->numverts )
					{
						count = 0; break;
					}

					const mesh_vertex_key_t key{ vertexIndex, normalIndex, s, t };
					auto found = remap.find( key );
					if( found != remap.end( ) )
					{
						polygon[vertexInPolygon] = found->second;
						continue;
					}

					const DWORD newIndex = static_cast<DWORD>( batch.sources.size( ) );
					studio_source_vertex_t source;
					source.x = positions[vertexIndex][0];
					source.y = positions[vertexIndex][1];
					source.z = positions[vertexIndex][2];
					if( floatUv )
					{
						source.u = R_StudioHalfToFloat( s );
						source.v = R_StudioHalfToFloat( t );
					}
					else
					{
						source.u = static_cast<float>( s ) / textureWidth;
						source.v = static_cast<float>( t ) / textureHeight;
					}
					source.bone = vertexBones[vertexIndex];
					if( source.bone >= hdr->numbones )
					{
						count = 0; break;
					}
					source.normalBone = source.bone;
					if( normalIndex >= 0 && normalIndex < model->numnorms &&
						model->normindex > 0 && model->norminfoindex > 0 )
					{
						source.nx = normals[normalIndex][0];
						source.ny = normals[normalIndex][1];
						source.nz = normals[normalIndex][2];
						const int normalBone = normalBones[normalIndex];
						source.normalBone = normalBone < hdr->numbones ? normalBone : source.bone;
					}
					else
					{
						source.nx = 0.0f;
						source.ny = 0.0f;
						source.nz = 1.0f;
					}
					batch.sources.push_back( source );
					remap.emplace( key, newIndex );
					polygon[vertexInPolygon] = newIndex;
				}
				if( count < 3 )
					break;

				if( fan )
				{
					for( int triangle = 1; triangle < count - 1; ++triangle )
					{
						batch.indices.push_back( polygon[0] );
						batch.indices.push_back( polygon[triangle] );
						batch.indices.push_back( polygon[triangle + 1] );
					}
				}
				else
				{
					for( int triangle = 0; triangle < count - 2; ++triangle )
					{
						if( triangle & 1 )
						{
							batch.indices.push_back( polygon[triangle + 1] );
							batch.indices.push_back( polygon[triangle] );
							batch.indices.push_back( polygon[triangle + 2] );
						}
						else
						{
							batch.indices.push_back( polygon[triangle] );
							batch.indices.push_back( polygon[triangle + 1] );
							batch.indices.push_back( polygon[triangle + 2] );
						}
					}
				}
			}
		}
	}

	cache.topologyValid = !cache.batches.empty( );
	if( !cache.topologyValid )
		return false;

	return R_CreateStudioBuffers( cache );
}

static bool R_StudioPoseCached( const studio_cache_t &cache,
	const cl_entity_s *ent, int sequence, float frame, int gaitSequence,
	float gaitFrame, float gaitYaw, D3DCOLOR diffuse, float directLightScale,
	const float cpuLocalLightDir[3] )
{

	const bool cpuChromeNeedsRefresh = cache.containsChrome && !cache.gpuSkinningBufferMode;
	if( cpuChromeNeedsRefresh || !cache.poseUploaded ||
		cache.lastSequence != sequence || fabsf( cache.lastFrame - frame ) > 0.0001f ||
		cache.lastGaitSequence != gaitSequence ||
		fabsf( cache.lastGaitFrame - gaitFrame ) > 0.0001f ||
		fabsf( cache.lastGaitYaw - gaitYaw ) > 0.0001f || cache.lastDiffuse != diffuse ||
		cache.lastNormalLightingEnabled != R_StudioNormalLighting( ) ||
		cache.lastUploadNormalsEnabled != R_StudioUploadNormals( ) ||
		( R_StudioNormalLighting( ) &&
			fabsf( cache.lastNormalLightScale - directLightScale ) > 0.0001f ) ||
		( R_StudioNormalLighting( ) && !cache.gpuSkinningBufferMode &&
			( !cache.lastCpuLocalLightDirValid || !cpuLocalLightDir ||
				fabsf( cache.lastCpuLocalLightDir[0] - cpuLocalLightDir[0] ) > 0.0001f ||
				fabsf( cache.lastCpuLocalLightDir[1] - cpuLocalLightDir[1] ) > 0.0001f ||
				fabsf( cache.lastCpuLocalLightDir[2] - cpuLocalLightDir[2] ) > 0.0001f ) ) )
	{
		return false;
	}

	if( !ent )
		return true;

	return memcmp( cache.lastController, ent->curstate.controller,
		sizeof( cache.lastController ) ) == 0 &&
		memcmp( cache.lastBlending, ent->curstate.blending,
			sizeof( cache.lastBlending ) ) == 0 &&
		cache.lastMouth == ent->mouth.mouthopen;
}

static UINT R_StudioVertexStride( bool studioNormals, bool uploadNormals )
{
	return uploadNormals
		? sizeof( studio_normal_lit_vertex_t )
		: ( studioNormals ? sizeof( studio_fast_lit_vertex_t ) : sizeof( brush_vertex_t ) );
}

static void R_FillStudioBatchCPU( const studio_batch_t &batch,
	void *vertexMemory, const BoneMat34 *boneMatrices, const D3DMATRIX &chromeWorld,
	bool studioNormals, bool normalLighting, bool uploadNormals, D3DCOLOR diffuse,
	float directLightScale, const float lightDirByBone[MAXSTUDIOBONES][3],
	const studio_local_light_t *localLights, int localLightCount )
{
	if( !vertexMemory || !boneMatrices )
		return;

	const float chromeTextureWidth =
		( batch.textureId > 0 && batch.textureId <= g_texcount )
		? static_cast<float>( g_Textures[batch.textureId].width ) : 64.0f;
	const float chromeTextureHeight =
		( batch.textureId > 0 && batch.textureId <= g_texcount )
		? static_cast<float>( g_Textures[batch.textureId].height ) : 64.0f;

	const bool materialFullbright =
		( batch.materialFlags & STUDIO_NF_FULLBRIGHT ) != 0;
	const bool materialFlatshade =
		( batch.materialFlags & STUDIO_NF_FLATSHADE ) != 0;
	const bool materialNormalLighting = normalLighting &&
		!materialFullbright && !materialFlatshade;
	D3DCOLOR materialDiffuse = materialFullbright
		? R_StudioFullbrightDiffuse( diffuse ) : diffuse;
	if( normalLighting && materialFlatshade && !materialFullbright )
	{
		materialDiffuse = R_StudioApplyDiffuseFactor( materialDiffuse,
			R_StudioFlatshadeFactor( directLightScale ) );
	}

	if( !studioNormals )
	{

		auto *destination = static_cast<brush_vertex_t *>( vertexMemory );
		for( size_t vertexIndex = 0; vertexIndex < batch.sources.size( ); ++vertexIndex )
		{
			const studio_source_vertex_t &source = batch.sources[vertexIndex];
			const int boneIndex = source.bone < MAXSTUDIOBONES ? source.bone : 0;
			const float localPosition[3] = { source.x, source.y, source.z };
			float skinnedPosition[3];
			VectorTransform( localPosition, boneMatrices[boneIndex], skinnedPosition );

			brush_vertex_t &vertex = destination[vertexIndex];
			vertex.x = skinnedPosition[0];
			vertex.y = skinnedPosition[1];
			vertex.z = skinnedPosition[2];
			vertex.diffuse = materialDiffuse;

			if( batch.chrome )
			{
				const int normalBoneIndex = source.normalBone < MAXSTUDIOBONES
					? source.normalBone : boneIndex;
				const float localNormal[3] = { source.nx, source.ny, source.nz };
				float skinnedNormal[3];
				R_StudioRotateVectorByBone( localNormal, boneMatrices[normalBoneIndex],
					skinnedNormal );
				R_StudioBuildChromeUV( chromeWorld, boneMatrices, normalBoneIndex,
					skinnedNormal, chromeTextureWidth, chromeTextureHeight,
					vertex.u, vertex.v );
			}
			else
			{
				vertex.u = source.u;
				vertex.v = source.v;
			}
		}
		return;
	}

	if( uploadNormals )
	{
		auto *destination = static_cast<studio_normal_lit_vertex_t *>( vertexMemory );
		for( size_t vertexIndex = 0; vertexIndex < batch.sources.size( ); ++vertexIndex )
		{
			const studio_source_vertex_t &source = batch.sources[vertexIndex];
			const int boneIndex = source.bone < MAXSTUDIOBONES ? source.bone : 0;
			const float localPosition[3] = { source.x, source.y, source.z };
			float skinnedPosition[3];
			VectorTransform( localPosition, boneMatrices[boneIndex], skinnedPosition );

			const int normalBoneIndex = source.normalBone < MAXSTUDIOBONES
				? source.normalBone : boneIndex;
			const float localNormal[3] = { source.nx, source.ny, source.nz };
			float skinnedNormal[3];
			R_StudioRotateVectorByBone( localNormal, boneMatrices[normalBoneIndex],
				skinnedNormal );

			const float lambert = materialNormalLighting ?
				( localNormal[0] * lightDirByBone[normalBoneIndex][0] +
				  localNormal[1] * lightDirByBone[normalBoneIndex][1] +
				  localNormal[2] * lightDirByBone[normalBoneIndex][2] ) : 0.0f;

			studio_normal_lit_vertex_t &vertex = destination[vertexIndex];
			vertex.x = skinnedPosition[0];
			vertex.y = skinnedPosition[1];
			vertex.z = skinnedPosition[2];
			vertex.nx = skinnedNormal[0];
			vertex.ny = skinnedNormal[1];
			vertex.nz = skinnedNormal[2];
			D3DCOLOR vertexLight = materialNormalLighting
				? R_StudioApplyLightFactor( materialDiffuse, lambert, directLightScale )
				: materialDiffuse;
			if( localLightCount > 0 )
			{
				float localLitNormal[3] = { skinnedNormal[0], skinnedNormal[1], skinnedNormal[2] };
				R_StudioNormalizeVector( localLitNormal );
				vertexLight = R_StudioApplyLocalLights( vertexLight, skinnedPosition,
					localLitNormal, localLights, localLightCount );
			}
			vertex.diffuse = vertexLight;

			if( batch.chrome )
			{
				R_StudioBuildChromeUV( chromeWorld, boneMatrices, normalBoneIndex,
					skinnedNormal, chromeTextureWidth, chromeTextureHeight,
					vertex.u, vertex.v );
			}
			else
			{
				vertex.u = source.u;
				vertex.v = source.v;
			}
		}
		return;
	}

	auto *destination = static_cast<studio_fast_lit_vertex_t *>( vertexMemory );
	for( size_t vertexIndex = 0; vertexIndex < batch.sources.size( ); ++vertexIndex )
	{
		const studio_source_vertex_t &source = batch.sources[vertexIndex];
		const int boneIndex = source.bone < MAXSTUDIOBONES ? source.bone : 0;
		const float localPosition[3] = { source.x, source.y, source.z };
		float skinnedPosition[3];
		VectorTransform( localPosition, boneMatrices[boneIndex], skinnedPosition );

		studio_fast_lit_vertex_t &vertex = destination[vertexIndex];
		vertex.x = skinnedPosition[0];
		vertex.y = skinnedPosition[1];
		vertex.z = skinnedPosition[2];

		if( materialNormalLighting || batch.chrome || localLightCount > 0 )
		{
			const int normalBoneIndex = source.normalBone < MAXSTUDIOBONES
				? source.normalBone : boneIndex;
			const float localNormal[3] = { source.nx, source.ny, source.nz };
			const float lambert = materialNormalLighting ?
				( localNormal[0] * lightDirByBone[normalBoneIndex][0] +
				  localNormal[1] * lightDirByBone[normalBoneIndex][1] +
				  localNormal[2] * lightDirByBone[normalBoneIndex][2] ) : 0.0f;
			D3DCOLOR vertexLight = materialNormalLighting
				? R_StudioApplyLightFactor( materialDiffuse, lambert, directLightScale )
				: materialDiffuse;
			float skinnedNormal[3];
			if( batch.chrome || localLightCount > 0 )
			{
				R_StudioRotateVectorByBone( localNormal, boneMatrices[normalBoneIndex],
					skinnedNormal );
				R_StudioNormalizeVector( skinnedNormal );
			}
			if( localLightCount > 0 )
				vertexLight = R_StudioApplyLocalLights( vertexLight, skinnedPosition,
					skinnedNormal, localLights, localLightCount );
			vertex.diffuse = vertexLight;

			if( batch.chrome )
			{
				R_StudioBuildChromeUV( chromeWorld, boneMatrices, normalBoneIndex,
					skinnedNormal, chromeTextureWidth, chromeTextureHeight,
					vertex.u, vertex.v );
			}
			else
			{
				vertex.u = source.u;
				vertex.v = source.v;
			}
		}
		else
		{
			vertex.diffuse = materialDiffuse;
			vertex.u = source.u;
			vertex.v = source.v;
		}
	}
}

static void R_StudioCommitPose( studio_cache_t &cache,
	const cl_entity_s *ent, int sequence, float frame, int gaitSequence,
	float gaitFrame, float gaitYaw, D3DCOLOR diffuse, float directScale,
	const float localLightDir[3], const studio_local_light_t *localLights,
	int localLightCount )
{
	R_StudioStoreLocalLights( cache, localLights, localLightCount );
	cache.poseUploaded = true;
	cache.lastSequence = sequence;
	cache.lastFrame = frame;
	cache.lastGaitSequence = gaitSequence;
	cache.lastGaitFrame = gaitFrame;
	cache.lastGaitYaw = gaitYaw;
	cache.lastDiffuse = diffuse;
	cache.lastNormalLightingEnabled = R_StudioNormalLighting( );
	cache.lastUploadNormalsEnabled = R_StudioUploadNormals( );
	cache.lastNormalLightScale = directScale;
	VectorCopy( localLightDir, cache.lastCpuLocalLightDir );
	cache.lastCpuLocalLightDirValid = true;

	if( ent )
	{
		memcpy( cache.lastController, ent->curstate.controller,
			sizeof( cache.lastController ) );
		memcpy( cache.lastBlending, ent->curstate.blending,
			sizeof( cache.lastBlending ) );
		cache.lastMouth = ent->mouth.mouthopen;
	}
	else
	{
		memset( cache.lastController, 0, sizeof( cache.lastController ) );
		memset( cache.lastBlending, 0, sizeof( cache.lastBlending ) );
		cache.lastMouth = 0;
	}
}

static void R_StudioBoneLightDirs( studiohdr_t *hdr, const float localLightDir[3],
	const BoneMat34 *boneMatrices, float outDirs[MAXSTUDIOBONES][3] )
{
	if( DX_ThreadStudioBones( hdr->numbones ) )
	{
		DX_ParallelFor( hdr->numbones, 16, [&]( int begin, int end )
		{
			for( int bone = begin; bone < end; ++bone )
				R_StudioLightToBone( localLightDir, boneMatrices[bone], outDirs[bone] );
		} );
		return;
	}

	for( int bone = 0; bone < hdr->numbones; ++bone )
		R_StudioLightToBone( localLightDir, boneMatrices[bone], outDirs[bone] );
}

static bool R_UploadStudioVertices( studio_cache_t &cache,
	const BoneMat34 *boneMatrices, const D3DMATRIX &chromeWorld,
	bool studioNormals, bool normalLighting, bool uploadNormals,
	D3DCOLOR diffuse, float directScale,
	float lightDirByBone[MAXSTUDIOBONES][3],
	const studio_local_light_t *localLights, int localLightCount,
	UINT vertexStride, bool threadedSkinning )
{
	const bool combinedVB = cache.combinedVertexBufferMode && cache.combinedVertexBuffer;

	if( combinedVB )
	{
		const unsigned int uploadBytes = static_cast<unsigned int>(
			cache.combinedVertexCount * vertexStride );
		void *vertexMemory = nullptr;

		if( threadedSkinning )
		{
			std::vector<byte> uploadBuffer( uploadBytes );
			DX_ParallelFor( static_cast<int>( cache.batches.size( ) ), 1,
				[&]( int begin, int end )
			{
				for( int batchIndex = begin; batchIndex < end; ++batchIndex )
				{
					const studio_batch_t &batch = cache.batches[batchIndex];
					if( batch.sources.empty( ) || batch.vertexCount == 0 )
						continue;
					R_FillStudioBatchCPU( batch,
						uploadBuffer.data( ) + static_cast<size_t>( batch.vertexStart ) * vertexStride,
						boneMatrices, chromeWorld, studioNormals, normalLighting,
						uploadNormals, diffuse, directScale, lightDirByBone,
						localLights, localLightCount );
				}
			} );

			DX_ProfileLock( DX_PROFILE_SCRATCH_MODEL, DX_PROFILE_SCRATCH_VB, D3DLOCK_DISCARD, uploadBytes );
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_VB_LOCK, 1 );
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_UPLOAD_KB,
				( uploadBytes + 1023u ) / 1024u );
			HRESULT result = cache.combinedVertexBuffer->Lock( 0, 0,
				&vertexMemory, D3DLOCK_DISCARD );
			if( FAILED( result ) || !vertexMemory )
			{
				R_FreeStudioBuffers( cache );
				return false;
			}
			memcpy( vertexMemory, uploadBuffer.data( ), uploadBytes );
			cache.combinedVertexBuffer->Unlock( );
			return true;
		}

		DX_ProfileLock( DX_PROFILE_SCRATCH_MODEL, DX_PROFILE_SCRATCH_VB, D3DLOCK_DISCARD, uploadBytes );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_VB_LOCK, 1 );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_UPLOAD_KB,
			( uploadBytes + 1023u ) / 1024u );
		HRESULT result = cache.combinedVertexBuffer->Lock( 0, 0,
			&vertexMemory, D3DLOCK_DISCARD );
		if( FAILED( result ) || !vertexMemory )
		{
			R_FreeStudioBuffers( cache );
			return false;
		}

		byte *base = static_cast<byte *>( vertexMemory );
		for( auto &batch : cache.batches )
		{
			if( batch.sources.empty( ) || batch.vertexCount == 0 )
				continue;
			R_FillStudioBatchCPU( batch,
				base + static_cast<size_t>( batch.vertexStart ) * vertexStride,
				boneMatrices, chromeWorld, studioNormals, normalLighting,
				uploadNormals, diffuse, directScale, lightDirByBone,
				localLights, localLightCount );
		}
		cache.combinedVertexBuffer->Unlock( );
		return true;
	}

	if( threadedSkinning )
	{
		std::vector<std::vector<byte>> uploadBuffers( cache.batches.size( ) );
		for( size_t i = 0; i < cache.batches.size( ); ++i )
		{
			const studio_batch_t &batch = cache.batches[i];
			if( batch.vertexBuffer && !batch.sources.empty( ) )
				uploadBuffers[i].resize( static_cast<size_t>( batch.vertexCount ) * vertexStride );
		}

		DX_ParallelFor( static_cast<int>( cache.batches.size( ) ), 1,
			[&]( int begin, int end )
		{
			for( int i = begin; i < end; ++i )
			{
				const studio_batch_t &batch = cache.batches[i];
				if( uploadBuffers[i].empty( ) )
					continue;
				R_FillStudioBatchCPU( batch, uploadBuffers[i].data( ),
					boneMatrices, chromeWorld, studioNormals, normalLighting,
					uploadNormals, diffuse, directScale, lightDirByBone,
					localLights, localLightCount );
			}
		} );

		for( size_t i = 0; i < cache.batches.size( ); ++i )
		{
			studio_batch_t &batch = cache.batches[i];
			if( !batch.vertexBuffer || uploadBuffers[i].empty( ) )
				continue;

			const unsigned int uploadBytes = static_cast<unsigned int>( uploadBuffers[i].size( ) );
			DX_ProfileLock( DX_PROFILE_SCRATCH_MODEL, DX_PROFILE_SCRATCH_VB, D3DLOCK_DISCARD, uploadBytes );
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_VB_LOCK, 1 );
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_UPLOAD_KB,
				( uploadBytes + 1023u ) / 1024u );

			void *vertexMemory = nullptr;
			HRESULT result = batch.vertexBuffer->Lock( 0, 0, &vertexMemory, D3DLOCK_DISCARD );
			if( FAILED( result ) || !vertexMemory )
			{
				R_FreeStudioBuffers( cache );
				return false;
			}
			memcpy( vertexMemory, uploadBuffers[i].data( ), uploadBytes );
			batch.vertexBuffer->Unlock( );
		}
		return true;
	}

	for( auto &batch : cache.batches )
	{
		if( !batch.vertexBuffer || batch.sources.empty( ) )
			return false;

		const unsigned int uploadBytes = static_cast<unsigned int>( batch.vertexCount * vertexStride );
		DX_ProfileLock( DX_PROFILE_SCRATCH_MODEL, DX_PROFILE_SCRATCH_VB, D3DLOCK_DISCARD, uploadBytes );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_VB_LOCK, 1 );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_UPLOAD_KB,
			( uploadBytes + 1023u ) / 1024u );

		void *vertexMemory = nullptr;
		HRESULT result = batch.vertexBuffer->Lock( 0, 0, &vertexMemory, D3DLOCK_DISCARD );
		if( FAILED( result ) || !vertexMemory )
		{
			R_FreeStudioBuffers( cache );
			return false;
		}
		R_FillStudioBatchCPU( batch, vertexMemory, boneMatrices, chromeWorld,
			studioNormals, normalLighting, uploadNormals, diffuse, directScale,
			lightDirByBone, localLights, localLightCount );
		batch.vertexBuffer->Unlock( );
	}
	return true;
}

static bool R_UpdateStudioVertices( studio_cache_t &cache,
	model_t *mod, studiohdr_t *hdr, const cl_entity_s *ent, int sequence,
	float frame, int gaitSequence, float gaitFrame, float gaitYaw,
	float renderTime, D3DCOLOR diffuse, const float studioWorldLightDir[3],
	float directScale, const D3DMATRIX *worldMatrix, bool isViewModel,
	const BoneMat34 *precomputedBoneMatrices = nullptr )
{
	float worldLightDir[3];
	if( studioWorldLightDir )
	{
		VectorCopy( studioWorldLightDir, worldLightDir );
	}
	else
	{
		VectorSet( worldLightDir, 0.0f, 0.0f,
			( ent && ( ent->curstate.effects & EF_INVLIGHT ) ) ? 1.0f : -1.0f );
	}
	R_StudioNormalizeVector( worldLightDir );
	VectorCopy( worldLightDir, cache.currentWorldLightDir );
	directScale = std::max( 0.0f, std::min( 1.0f, directScale ) );

	float localLightDir[3];
	R_StudioLightToEntity( worldLightDir, worldMatrix, localLightDir );

	studio_local_light_t localLights[DX_STUDIO_MAX_LOCAL_LIGHTS];
	const int localLightCount = R_StudioLocalLights( ent, worldMatrix, localLights );
	const bool localLightsChanged = !R_StudioLocalLightsEqual(
		cache.localLights, cache.localLightCount, localLights, localLightCount );

	const bool poseCached = R_StudioPoseCached( cache, ent,
		sequence, frame, gaitSequence, gaitFrame, gaitYaw, diffuse,
		directScale, localLightDir ) &&
		( cache.gpuSkinningBufferMode || !localLightsChanged );

	if( cache.gpuSkinningBufferMode )
		R_StudioStoreLocalLights( cache, localLights, localLightCount );

	if( poseCached )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_CACHE_HIT, 1 );
		return true;
	}

	BoneMat34 localBoneMatrices[MAXSTUDIOBONES];
	const BoneMat34 *boneMatrices = precomputedBoneMatrices;
	if( !boneMatrices )
	{
		StudioCalcBoneTransformsInternal( mod, hdr, sequence, frame,
			localBoneMatrices, gaitSequence, gaitFrame, gaitYaw, ent, renderTime );
		boneMatrices = localBoneMatrices;
	}

	DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_CACHE_MISS, 1 );
	if( !R_StudioPoseBudgetOK( isViewModel, cache ) )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_BUDGET_SKIP, 1 );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_CACHE_HIT, 1 );
		return true;
	}
	if( !R_CreateStudioBuffers( cache ) )
		return false;

	const int cachedBoneCount = std::max( 0, std::min( MAXSTUDIOBONES, hdr->numbones ) );
	memcpy( cache.gpuSkinningBoneMatrices, boneMatrices,
		static_cast<size_t>( cachedBoneCount ) * sizeof( BoneMat34 ) );
	cache.gpuSkinningBoneCount = cachedBoneCount;
	cache.gpuSkinningPoseValid = cachedBoneCount > 0;

	D3DMATRIX chromeWorld;
	if( cache.containsChrome )
	{
		if( worldMatrix )
			chromeWorld = *worldMatrix;
		else
			DX_MatrixIdentity( chromeWorld );
	}

	const bool studioNormals = R_StudioNormalsEnabled( );
	const bool normalLighting = R_StudioNormalLighting( );
	const bool uploadNormals = R_StudioUploadNormals( );
	const UINT vertexStride = R_StudioVertexStride( studioNormals, uploadNormals );

	if( cache.gpuSkinningBufferMode && cache.gpuSkinSourceVertexBuffer )
	{
		R_StudioCommitPose( cache, ent, sequence, frame, gaitSequence,
			gaitFrame, gaitYaw, diffuse, directScale, localLightDir,
			localLights, localLightCount );
		return true;
	}

	float lightDirByBone[MAXSTUDIOBONES][3];
	if( normalLighting )
		R_StudioBoneLightDirs( hdr, localLightDir, boneMatrices, lightDirByBone );

	int totalVertices = 0;
	for( const auto &batch : cache.batches )
		totalVertices += static_cast<int>( batch.sources.size( ) );

	const bool threadedSkinning = !isViewModel &&
		DX_ThreadStudioSkinning( totalVertices ) && cache.batches.size( ) > 1;

	if( !R_UploadStudioVertices( cache, boneMatrices, chromeWorld,
		studioNormals, normalLighting, uploadNormals, diffuse, directScale,
		lightDirByBone, localLights, localLightCount, vertexStride, threadedSkinning ) )
	{
		return false;
	}

	R_StudioCommitPose( cache, ent, sequence, frame, gaitSequence,
		gaitFrame, gaitYaw, diffuse, directScale, localLightDir,
		localLights, localLightCount );
	return true;
}

static bool R_StudioGlowShellEnabled( const cl_entity_s *ent )
{
	return ent && dx_studio_glowshell.value != 0.0f &&
		ent->curstate.renderfx == kRenderFxGlowShell;
}

static float R_StudioGlowShellScale( const cl_entity_s *ent )
{
	const float unit = 1.0f / 128.0f;
	return ent ? std::max( unit,
		static_cast<float>( ent->curstate.renderamt ) * unit ) : unit;
}

static D3DCOLOR R_StudioGlowShellColor( const cl_entity_s *ent )
{
	int r = 255, g = 255, b = 255;
	if( ent && ( ent->curstate.rendercolor.r || ent->curstate.rendercolor.g ||
		ent->curstate.rendercolor.b ) )
	{
		r = ent->curstate.rendercolor.r;
		g = ent->curstate.rendercolor.g;
		b = ent->curstate.rendercolor.b;
	}
	return D3DCOLOR_ARGB( 255, r, g, b );
}

static void R_StudioGlowShellChromeOrigin( float time, float outOrigin[3] )
{
	const float frequency = std::max( 0.0f, dx_glowshell_freq.value );
	outOrigin[0] = cosf( frequency * time ) * 4000.0f;
	outOrigin[1] = sinf( frequency * time ) * 4000.0f;
	outOrigin[2] = cosf( frequency * time * 0.33f ) * 4000.0f;
}

static int R_StudioGlowShellTextureId( )
{
	if( !gEngfuncs.GetDefaultSprite || !gEngfuncs.R_GetSpriteFrame )
		return 0;

	model_t *chromeSprite = gEngfuncs.GetDefaultSprite( REF_CHROME_SPRITE );
	if( !chromeSprite )
		return 0;

	mspriteframe_t *frame = static_cast<mspriteframe_t *>(
		gEngfuncs.R_GetSpriteFrame( chromeSprite, 0.0f, 0.0f ) );
	return frame ? frame->gl_texturenum : 0;
}

static void R_BeginStudioGlowShellPass( D3DCOLOR shellColor )
{
	DX_AllowFog( false );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );

	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_ONE );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR, shellColor );
	R_SetLightmapStageEnabled( false );
	R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
		D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_DIFFUSE );
}

static bool R_DrawStudioSkinBatches( const studio_cache_t &cache,
	const cl_entity_s *ent, bool glowShellPass = false )
{
	if( !cache.gpuSkinningBufferMode || !cache.gpuSkinSourceVertexBuffer ||
		!cache.gpuSkinningPoseValid || cache.gpuSkinningBoneCount <= 0 ||
		!DX_EnsureStudioSkinningShaders( ) )
	{
		return false;
	}
	if( cache.gpuSkinningBoneCount > DX_StudioGpuSkinningBoneCap( ) )
		return false;

	const int glowShellTexture = glowShellPass ? R_StudioGlowShellTextureId( ) : 0;
	if( glowShellPass && glowShellTexture <= 0 )
		return false;

	D3DMATRIX d3dWorld{}, d3dView{}, d3dProjection{};
	D3DXMATRIX world, view, projection, worldViewProjection, transposed;
	device->GetTransform( D3DTS_WORLD, &d3dWorld );
	device->GetTransform( D3DTS_VIEW, &d3dView );
	device->GetTransform( D3DTS_PROJECTION, &d3dProjection );
	DX_ToD3DXMatrix( d3dWorld, world );
	DX_ToD3DXMatrix( d3dView, view );
	DX_ToD3DXMatrix( d3dProjection, projection );
	worldViewProjection = world * view * projection;
	D3DXMatrixTranspose( &transposed, &worldViewProjection );

	const D3DCOLOR drawDiffuse = glowShellPass
		? R_StudioGlowShellColor( ent ) : cache.lastDiffuse;
	const byte a = static_cast<byte>( ( drawDiffuse >> 24 ) & 0xFF );
	const byte r = static_cast<byte>( ( drawDiffuse >> 16 ) & 0xFF );
	const byte g = static_cast<byte>( ( drawDiffuse >> 8 ) & 0xFF );
	const byte b = static_cast<byte>( drawDiffuse & 0xFF );
	float constants0[4] = { 1.0f, 0.0f, 0.5f, 0.0f };
	float diffuse[4] = { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
	float entityLightDir3[3];
	R_StudioLightToEntity( cache.currentWorldLightDir, &d3dWorld,
		entityLightDir3 );
	float lightDir[4] =
	{
		entityLightDir3[0], entityLightDir3[1], entityLightDir3[2], 0.0f
	};
	const float directScale = cache.lastNormalLightingEnabled
		? cache.lastNormalLightScale : 0.0f;
	float lightScale[4] =
	{
		glowShellPass ? 0.0f : directScale,
		DX_STUDIO_SHADE_LAMBERT - 1.0f,
		1.0f / DX_STUDIO_SHADE_LAMBERT,
		0.000001f
	};
	float worldRows[3][4] =
	{
		{ world._11, world._21, world._31, world._41 },
		{ world._12, world._22, world._32, world._42 },
		{ world._13, world._23, world._33, world._43 }
	};
	float chromeOrigin3[3] = { g_vOrigin[0], g_vOrigin[1], g_vOrigin[2] };
	if( glowShellPass )
		R_StudioGlowShellChromeOrigin( gp_cl ? gp_cl->time : 0.0f, chromeOrigin3 );
	float viewOrigin[4] = { chromeOrigin3[0], chromeOrigin3[1], chromeOrigin3[2], 0.0f };
	float viewRight[4] = { g_vRight[0], g_vRight[1], g_vRight[2], 0.0f };
	float viewUp[4] = { g_vUp[0], g_vUp[1], g_vUp[2], 0.0f };
	float shellParams[4] =
	{
		glowShellPass ? R_StudioGlowShellScale( ent ) : 0.0f,
		glowShellPass ? 1.0f : 0.0f,
		0.0f, 0.0f
	};

	const bool useLocalLightShader = !glowShellPass && cache.localLightCount > 0;
	DX_BindStudioGpuSkinningShader( useLocalLightShader );
	device->SetStreamSource( 0, cache.gpuSkinSourceVertexBuffer, 0,
		sizeof( studio_skin_vertex_t ) );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_ZERO, constants0, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_DIFFUSE, diffuse, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_LIGHTDIR, lightDir, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_LIGHTSCALE, lightScale, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_WVP,
		reinterpret_cast<const float *>( &transposed ), 4 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_WORLD,
		&worldRows[0][0], 3 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_VIEWORIGIN, viewOrigin, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_VIEWRIGHT, viewRight, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_VIEWUP, viewUp, 1 );
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_SHELL, shellParams, 1 );

	if( useLocalLightShader )
	{
		float localLightConstants[DX_STUDIO_MAX_LOCAL_LIGHTS * 2 * 4] = {};
		for( int i = 0; i < cache.localLightCount && i < DX_STUDIO_MAX_LOCAL_LIGHTS; ++i )
		{
			float *position = &localLightConstants[static_cast<size_t>( i * 2 ) * 4u];
			float *color = position + 4;
			position[0] = cache.localLights[i].origin[0];
			position[1] = cache.localLights[i].origin[1];
			position[2] = cache.localLights[i].origin[2];
			position[3] = cache.localLights[i].radiusSq;
			color[0] = cache.localLights[i].r;
			color[1] = cache.localLights[i].g;
			color[2] = cache.localLights[i].b;
			color[3] = cache.localLights[i].intensity;
		}
		device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_LOCAL_LIGHT0,
			localLightConstants, DX_STUDIO_MAX_LOCAL_LIGHTS * 2 );
	}

	float boneConstants[MAXSTUDIOBONES * 3 * 4];
	for( int bone = 0; bone < cache.gpuSkinningBoneCount; ++bone )
	{
		for( int row = 0; row < 3; ++row )
		{
			float *dst = &boneConstants[static_cast<size_t>( bone * 3 + row ) * 4u];
			dst[0] = cache.gpuSkinningBoneMatrices[bone][row][0];
			dst[1] = cache.gpuSkinningBoneMatrices[bone][row][1];
			dst[2] = cache.gpuSkinningBoneMatrices[bone][row][2];
			dst[3] = cache.gpuSkinningBoneMatrices[bone][row][3];
		}
	}
	device->SetVertexShaderConstantF( DX_STUDIO_GPU_SKIN_CONST_BONES,
		boneConstants, cache.gpuSkinningBoneCount * 3 );

	if( glowShellPass )
	{
		R_BeginStudioGlowShellPass( drawDiffuse );
		DX_Bind( 0, glowShellTexture );
		const float width = ( glowShellTexture > 0 && glowShellTexture <= g_texcount )
			? static_cast<float>( std::max( 1, g_Textures[glowShellTexture].width ) )
			: 64.0f;
		const float height = ( glowShellTexture > 0 && glowShellTexture <= g_texcount )
			? static_cast<float>( std::max( 1, g_Textures[glowShellTexture].height ) )
			: 64.0f;
		const float chromeScale[4] =
		{
			32.0f / width, 32.0f / height, 0.0f, 0.0f
		};
		device->SetVertexShaderConstantF(
			DX_STUDIO_GPU_SKIN_CONST_CHROMESCALE, chromeScale, 1 );
	}

	int boundTexture = glowShellPass ? glowShellTexture : -1;
	unsigned int lastLightingClass = ~0u;
	studio_material_state_t materialState = DX_STUDIO_MATERIAL_BASE;
	for( const auto &batch : cache.batches )
	{
		if( !batch.indexBuffer || batch.primitiveCount == 0 )
			continue;

		if( !glowShellPass )
		{
			if( batch.textureId != boundTexture )
			{
				DX_Bind( 0, batch.textureId );
				boundTexture = batch.textureId;
			}
			R_ApplyStudioMaterialState( batch.materialFlags, ent, materialState );
		}

		const unsigned int lightingClass = glowShellPass ? 0u :
			( batch.materialFlags & ( STUDIO_NF_FULLBRIGHT | STUDIO_NF_FLATSHADE ) );
		if( !glowShellPass && lightingClass != lastLightingClass )
		{
			float batchDiffuse[4] = { diffuse[0], diffuse[1], diffuse[2], diffuse[3] };
			float batchLightScale[4] = { lightScale[0], lightScale[1], lightScale[2], lightScale[3] };
			if( lightingClass & STUDIO_NF_FULLBRIGHT )
			{
				batchDiffuse[0] = batchDiffuse[1] = batchDiffuse[2] = 1.0f;
				batchLightScale[0] = 0.0f;
			}
			else if( lightingClass & STUDIO_NF_FLATSHADE )
			{

				const float flatFactor = R_StudioFlatshadeFactor( directScale );
				batchDiffuse[0] *= flatFactor;
				batchDiffuse[1] *= flatFactor;
				batchDiffuse[2] *= flatFactor;
				batchLightScale[0] = 0.0f;
			}
			device->SetVertexShaderConstantF(
				DX_STUDIO_GPU_SKIN_CONST_DIFFUSE, batchDiffuse, 1 );
			device->SetVertexShaderConstantF(
				DX_STUDIO_GPU_SKIN_CONST_LIGHTSCALE, batchLightScale, 1 );
			lastLightingClass = lightingClass;
		}

		if( !glowShellPass && batch.chrome )
		{
			const float width = ( batch.textureId > 0 && batch.textureId <= g_texcount )
				? static_cast<float>( std::max( 1, g_Textures[batch.textureId].width ) ) : 64.0f;
			const float height = ( batch.textureId > 0 && batch.textureId <= g_texcount )
				? static_cast<float>( std::max( 1, g_Textures[batch.textureId].height ) ) : 64.0f;
			const float chromeScale[4] =
			{
				32.0f / width,
				32.0f / height,
				0.0f,
				0.0f
			};
			device->SetVertexShaderConstantF(
				DX_STUDIO_GPU_SKIN_CONST_CHROMESCALE, chromeScale, 1 );
		}

		device->SetIndices( batch.indexBuffer );
		DX_ProfileDraw( true, batch.primitiveCount );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_BATCH_DRAWN, 1 );
		device->DrawIndexedPrimitive( D3DPT_TRIANGLELIST,
			static_cast<INT>( batch.vertexStart ), 0, batch.vertexCount,
			0, batch.primitiveCount );
	}

	device->SetVertexShader( nullptr );
	device->SetVertexDeclaration( nullptr );
	device->SetFVF( BRUSH_FVF );
	return true;
}

static void R_DrawStudioGlowShellCPU( const studio_cache_t &cache,
	const cl_entity_s *ent )
{
	if( !R_StudioGlowShellEnabled( ent ) ||
		!cache.gpuSkinningPoseValid || cache.gpuSkinningBoneCount <= 0 )
	{
		return;
	}

	const int chromeTexture = R_StudioGlowShellTextureId( );
	if( chromeTexture <= 0 )
		return;

	D3DMATRIX world{};
	device->GetTransform( D3DTS_WORLD, &world );

	const float chromeWidth = chromeTexture <= g_texcount
		? static_cast<float>( std::max( 1, g_Textures[chromeTexture].width ) ) : 64.0f;
	const float chromeHeight = chromeTexture <= g_texcount
		? static_cast<float>( std::max( 1, g_Textures[chromeTexture].height ) ) : 64.0f;
	const float shellScale = R_StudioGlowShellScale( ent );
	const D3DCOLOR shellColor = R_StudioGlowShellColor( ent );
	float chromeOrigin[3];
	R_StudioGlowShellChromeOrigin( gp_cl ? gp_cl->time : 0.0f, chromeOrigin );

	R_BeginStudioGlowShellPass( shellColor );
	device->SetFVF( BRUSH_FVF );
	DX_Bind( 0, chromeTexture );

	static std::vector<brush_vertex_t> shellVertices;
	for( const studio_batch_t &batch : cache.batches )
	{
		if( batch.sources.empty( ) || batch.indices.empty( ) ||
			batch.primitiveCount == 0 )
		{
			continue;
		}

		shellVertices.resize( batch.sources.size( ) );
		for( size_t vertexIndex = 0; vertexIndex < batch.sources.size( ); ++vertexIndex )
		{
			const studio_source_vertex_t &source = batch.sources[vertexIndex];
			const int boneIndex = source.bone < cache.gpuSkinningBoneCount
				? source.bone : 0;
			const int normalBoneIndex = source.normalBone < cache.gpuSkinningBoneCount
				? source.normalBone : boneIndex;

			const float localPosition[3] = { source.x, source.y, source.z };
			float position[3];
			VectorTransform( localPosition,
				cache.gpuSkinningBoneMatrices[boneIndex], position );

			const float localNormal[3] = { source.nx, source.ny, source.nz };
			float normal[3];
			R_StudioRotateVectorByBone( localNormal,
				cache.gpuSkinningBoneMatrices[normalBoneIndex], normal );
			if( !R_StudioNormalizeVector( normal ) )
			{
				normal[0] = normal[1] = 0.0f;
				normal[2] = 1.0f;
			}

			brush_vertex_t &vertex = shellVertices[vertexIndex];
			vertex.x = position[0] + normal[0] * shellScale;
			vertex.y = position[1] + normal[1] * shellScale;
			vertex.z = position[2] + normal[2] * shellScale;
			vertex.diffuse = shellColor;
			R_StudioBuildChromeUV( world, cache.gpuSkinningBoneMatrices,
				normalBoneIndex, normal, chromeWidth, chromeHeight,
				vertex.u, vertex.v, chromeOrigin );
		}

		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_BATCH_DRAWN, 1 );
		R_DrawScratchIndexedPrimitive( D3DPT_TRIANGLELIST,
			static_cast<UINT>( shellVertices.size( ) ),
			batch.primitiveCount, batch.indices.data( ), D3DFMT_INDEX32,
			shellVertices.data( ), sizeof( brush_vertex_t ) );
	}
}

static void R_DrawStudioGpuBatches( const studio_cache_t &cache, const cl_entity_s *ent )
{
	if( R_DrawStudioSkinBatches( cache, ent, false ) )
	{
		if( R_StudioGlowShellEnabled( ent ) )
			R_DrawStudioSkinBatches( cache, ent, true );
		return;
	}

	const bool studioNormals = R_StudioNormalsEnabled( );
	const bool uploadNormals = cache.buffersUploadNormals;
	device->SetFVF( uploadNormals ? STUDIO_NORMAL_LIT_FVF :
		( studioNormals ? STUDIO_FAST_LIT_FVF : BRUSH_FVF ) );
	const UINT vertexStride = uploadNormals
		? sizeof( studio_normal_lit_vertex_t )
		: ( studioNormals ? sizeof( studio_fast_lit_vertex_t ) : sizeof( brush_vertex_t ) );

	const bool combinedVB = cache.combinedVertexBufferMode && cache.combinedVertexBuffer;
	if( combinedVB )
		device->SetStreamSource( 0, cache.combinedVertexBuffer, 0, vertexStride );

	int boundTexture = -1;
	studio_material_state_t materialState = DX_STUDIO_MATERIAL_BASE;
	for( const auto &batch : cache.batches )
	{
		if( !batch.indexBuffer || batch.primitiveCount == 0 )
			continue;
		if( !combinedVB && !batch.vertexBuffer )
			continue;

		if( batch.textureId != boundTexture )
		{
			DX_Bind( 0, batch.textureId );
			boundTexture = batch.textureId;
		}
		R_ApplyStudioMaterialState( batch.materialFlags, ent, materialState );

		if( !combinedVB )
			device->SetStreamSource( 0, batch.vertexBuffer, 0, vertexStride );
		device->SetIndices( batch.indexBuffer );
		DX_ProfileDraw( true, batch.primitiveCount );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_BATCH_DRAWN, 1 );
		device->DrawIndexedPrimitive( D3DPT_TRIANGLELIST,
			combinedVB ? static_cast<INT>( batch.vertexStart ) : 0,
			0, batch.vertexCount, 0, batch.primitiveCount );
	}

	if( R_StudioGlowShellEnabled( ent ) )
		R_DrawStudioGlowShellCPU( cache, ent );
}

static int R_StudioHeaderRenderMode( studiohdr_t *hdr )
{
	if( !hdr || hdr->numtextures <= 0 || hdr->textureindex <= 0 )
		return kRenderNormal;

	mstudiotexture_t *textures = reinterpret_cast<mstudiotexture_t *>(
		reinterpret_cast<byte *>( hdr ) + hdr->textureindex );

	int opaque = 0;
	int additive = 0;
	for( int i = 0; i < hdr->numtextures; ++i )
	{
		const unsigned int flags = R_StudioTextureFlags( &textures[i] );
		if( ( flags & STUDIO_NF_ADDITIVE ) != 0 &&
			( flags & STUDIO_NF_CHROME ) == 0 )
		{
			++additive;
		}
		else
		{
			++opaque;
		}
	}

	return additive > opaque ? kRenderTransAdd : kRenderNormal;
}

static void R_CacheStudioRenderMode( const model_t *mod, int mode )
{
	if( !mod )
		return;

	auto &slot = g_studioRenderModeFrontCache[
		R_PointerFrontCacheIndex( mod, DX_STUDIO_MODEL_FRONT_CACHE_SIZE - 1 )];
	slot.key = mod;
	slot.effectiveRenderMode = mode;
}

int R_StudioRenderMode( const cl_entity_s *ent )
{
	if( !ent || !ent->model || ent->model->type != mod_studio )
		return ent ? ent->curstate.rendermode : kRenderNormal;

	if( ent->curstate.rendermode != kRenderNormal )
		return ent->curstate.rendermode;

	const model_t *mod = ent->model;
	auto &slot = g_studioRenderModeFrontCache[
		R_PointerFrontCacheIndex( mod, DX_STUDIO_MODEL_FRONT_CACHE_SIZE - 1 )];
	if( slot.key == mod )
		return slot.effectiveRenderMode;

	studiohdr_t *hdr = static_cast<studiohdr_t *>(
		gEngfuncs.Mod_Extradata( mod_studio, const_cast<model_t *>( mod ) ) );
	const int mode = R_StudioHeaderRenderMode( hdr );
	R_CacheStudioRenderMode( mod, mode );
	return mode;
}

bool IsStudioModelAnimated( studiohdr_t *hdr )
{
	if( !hdr || hdr->numseq <= 1 ) return false;
	for( int i = 0; i < hdr->numseq; ++i )
	{
		mstudioseqdesc_t *seq = (mstudioseqdesc_t *)( (byte *)hdr + hdr->seqindex ) + i;
		if( seq->numframes > 1 ) return true;
	}
	return false;
}

static void QuaternionNormalize( float q[4] )
{
	const float lenSq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];

	if( lenSq > 1e-12f )
	{
		const float invLen = 1.0f / sqrtf( lenSq );
		q[0] *= invLen;
		q[1] *= invLen;
		q[2] *= invLen;
		q[3] *= invLen;
	}
	else
	{
		q[0] = q[1] = q[2] = 0.0f;
		q[3] = 1.0f;
	}
}

static void GoldSrcEntityAngleMatrix( const float angles[3], const float origin[3], BoneMat34 mat )
{
	const float pitch = DEG2RAD( angles[0] );
	const float yaw = DEG2RAD( angles[1] );
	const float roll = DEG2RAD( angles[2] );

	const float sp = sinf( pitch ), cp = cosf( pitch );
	const float sy = sinf( yaw ), cy = cosf( yaw );
	const float sr = sinf( roll ), cr = cosf( roll );

	mat[0][0] = cp * cy;
	mat[0][1] = sr * sp * cy - cr * sy;
	mat[0][2] = cr * sp * cy + sr * sy;
	mat[0][3] = origin[0];

	mat[1][0] = cp * sy;
	mat[1][1] = sr * sp * sy + cr * cy;
	mat[1][2] = cr * sp * sy - sr * cy;
	mat[1][3] = origin[1];

	mat[2][0] = -sp;
	mat[2][1] = sr * cp;
	mat[2][2] = cr * cp;
	mat[2][3] = origin[2];
}

static void QuaternionFromMatrix( const BoneMat34 mat, float q[4] )
{
	const float trace = mat[0][0] + mat[1][1] + mat[2][2];

	if( trace > 0.0f )
	{
		const float s = sqrtf( trace + 1.0f ) * 2.0f;
		q[3] = 0.25f * s;
		q[0] = ( mat[2][1] - mat[1][2] ) / s;
		q[1] = ( mat[0][2] - mat[2][0] ) / s;
		q[2] = ( mat[1][0] - mat[0][1] ) / s;
	}
	else if( mat[0][0] > mat[1][1] && mat[0][0] > mat[2][2] )
	{
		const float s = sqrtf( 1.0f + mat[0][0] - mat[1][1] - mat[2][2] ) * 2.0f;
		q[3] = ( mat[2][1] - mat[1][2] ) / s;
		q[0] = 0.25f * s;
		q[1] = ( mat[0][1] + mat[1][0] ) / s;
		q[2] = ( mat[0][2] + mat[2][0] ) / s;
	}
	else if( mat[1][1] > mat[2][2] )
	{
		const float s = sqrtf( 1.0f + mat[1][1] - mat[0][0] - mat[2][2] ) * 2.0f;
		q[3] = ( mat[0][2] - mat[2][0] ) / s;
		q[0] = ( mat[0][1] + mat[1][0] ) / s;
		q[1] = 0.25f * s;
		q[2] = ( mat[1][2] + mat[2][1] ) / s;
	}
	else
	{
		const float s = sqrtf( 1.0f + mat[2][2] - mat[0][0] - mat[1][1] ) * 2.0f;
		q[3] = ( mat[1][0] - mat[0][1] ) / s;
		q[0] = ( mat[0][2] + mat[2][0] ) / s;
		q[1] = ( mat[1][2] + mat[2][1] ) / s;
		q[2] = 0.25f * s;
	}

	QuaternionNormalize( q );
}

static float R_NormalizePlayerPitchDegrees( float pitch )
{
	while( pitch > 180.0f ) pitch -= 360.0f;
	while( pitch < -180.0f ) pitch += 360.0f;
	return pitch;
}

static int R_PlayerAimBlendForPitch( const mstudioseqdesc_t *seqdesc,
	float pitch, float &residualPitch )
{
	residualPitch = pitch;
	if( !seqdesc || fabsf( seqdesc->blendend[0] - seqdesc->blendstart[0] ) < 0.1f )
		return 127;

	float blendValue = pitch * 3.0f;
	if( blendValue < seqdesc->blendstart[0] )
	{
		residualPitch = pitch - seqdesc->blendstart[0] / 3.0f;
		return 0;
	}
	if( blendValue > seqdesc->blendend[0] )
	{
		residualPitch = pitch - seqdesc->blendend[0] / 3.0f;
		return 255;
	}

	residualPitch = 0.0f;
	const float t = ( blendValue - seqdesc->blendstart[0] ) /
		( seqdesc->blendend[0] - seqdesc->blendstart[0] );
	return std::max( 0, std::min( 255, static_cast<int>( t * 255.0f + 0.5f ) ) );
}

static void R_ApplyPlayerAimBlend( cl_entity_s *ent, studiohdr_t *hdr )
{
	if( !ent || !ent->player || !hdr || hdr->numseq <= 0 )
		return;

	int sequence = ent->curstate.sequence;
	if( sequence < 0 || sequence >= hdr->numseq )
		sequence = 0;

	mstudioseqdesc_t *seqdesc = reinterpret_cast<mstudioseqdesc_t *>(
		reinterpret_cast<byte *>( hdr ) + hdr->seqindex ) + sequence;
	if( seqdesc->numblends <= 1 )
		return;

	float pitch = R_NormalizePlayerPitchDegrees( ent->angles[PITCH] );

	const float statePitch = R_NormalizePlayerPitchDegrees( ent->curstate.angles[PITCH] );
	if( fabsf( statePitch ) > 0.001f || fabsf( pitch ) < 0.001f )
		pitch = statePitch;

	float residualPitch = pitch;
	const int blend = R_PlayerAimBlendForPitch( seqdesc, pitch, residualPitch );
	ent->curstate.blending[0] = static_cast<byte>( blend );
	ent->latched.prevblending[0] = static_cast<byte>( blend );
	ent->latched.prevseqblending[0] = static_cast<byte>( blend );
	ent->angles[PITCH] = residualPitch;
	ent->curstate.angles[PITCH] = residualPitch;
	ent->latched.prevangles[PITCH] = residualPitch;
}

static inline void R_ApplyStudioPitch(
	const cl_entity_s *ent, float angles[3], bool studioEntity )
{
	if( !studioEntity || !ent || ent->player )
		return;

	if( !gp_host ||
		!FBitSet( gp_host->features, ENGINE_COMPENSATE_QUAKE_BUG ) )
	{
		angles[PITCH] = -angles[PITCH];
	}
}

static void R_InterpolateEntityTransform( const cl_entity_s *ent,
	float renderTime, BoneMat34 out, bool studioEntity )
{
	if( !ent )
	{
		float zeroOrigin[3] = { 0.0f, 0.0f, 0.0f };
		float zeroAngles[3] = { 0.0f, 0.0f, 0.0f };
		GoldSrcEntityAngleMatrix( zeroAngles, zeroOrigin, out );
		return;
	}

	float origin[3] = { ent->origin[0], ent->origin[1], ent->origin[2] };

	if( ent->player )
	{
		const float playerAngles[3] = { 0.0f, ent->angles[YAW], 0.0f };

		GoldSrcEntityAngleMatrix( playerAngles, origin, out );
		return;
	}

	float angles[3] = { ent->angles[0], ent->angles[1], ent->angles[2] };
	R_ApplyStudioPitch( ent, angles, studioEntity );

	if( ent->curstate.movetype == MOVETYPE_STEP &&
		renderTime < ent->curstate.animtime + 1.0f &&
		ent->curstate.animtime != ent->latched.prevanimtime )
	{
		const float frac = ( renderTime - ent->curstate.animtime ) /
			( ent->curstate.animtime - ent->latched.prevanimtime );

		origin[0] = ent->latched.prevorigin[0] +
			( ent->curstate.origin[0] - ent->latched.prevorigin[0] ) * frac;
		origin[1] = ent->latched.prevorigin[1] +
			( ent->curstate.origin[1] - ent->latched.prevorigin[1] ) * frac;
		origin[2] = ent->latched.prevorigin[2] +
			( ent->curstate.origin[2] - ent->latched.prevorigin[2] ) * frac;

		BoneMat34 previous, current;
		float previousQ[4], currentQ[4], interpolatedQ[4];

		float previousAngles[3] =
		{
			ent->latched.prevangles[0],
			ent->latched.prevangles[1],
			ent->latched.prevangles[2]
		};
		float currentAngles[3] =
		{
			ent->curstate.angles[0],
			ent->curstate.angles[1],
			ent->curstate.angles[2]
		};

		R_ApplyStudioPitch(
			ent, previousAngles, studioEntity );
		R_ApplyStudioPitch(
			ent, currentAngles, studioEntity );

		GoldSrcEntityAngleMatrix( previousAngles, origin, previous );
		GoldSrcEntityAngleMatrix( currentAngles, origin, current );
		QuaternionFromMatrix( previous, previousQ );
		QuaternionFromMatrix( current, currentQ );
		QuaternionSlerp( previousQ, currentQ, frac, interpolatedQ );
		QuaternionNormalize( interpolatedQ );
		QuaternionMatrix( interpolatedQ, origin, out );
		return;
	}

	GoldSrcEntityAngleMatrix( angles, origin, out );
}

static void R_BoneMatToWorld( const BoneMat34 in, D3DMATRIX *out )
{
	out->_11 = in[0][0]; out->_12 = in[1][0]; out->_13 = in[2][0]; out->_14 = 0.0f;
	out->_21 = in[0][1]; out->_22 = in[1][1]; out->_23 = in[2][1]; out->_24 = 0.0f;
	out->_31 = in[0][2]; out->_32 = in[1][2]; out->_33 = in[2][2]; out->_34 = 0.0f;
	out->_41 = in[0][3]; out->_42 = in[1][3]; out->_43 = in[2][3]; out->_44 = 1.0f;
}

static float R_EntityModelScale( const cl_entity_s *ent )
{
	if( !ent || !ent->model || ent->model->type == mod_brush ||
		ent->curstate.scale <= 0.0f )
	{
		return 1.0f;
	}
	return ent->curstate.scale;
}

static void R_ApplyEntityScale(
	const cl_entity_s *ent, D3DMATRIX *matrix )
{
	if( !matrix )
		return;

	const float scale = R_EntityModelScale( ent );
	if( scale == 1.0f )
		return;

	matrix->_11 *= scale; matrix->_12 *= scale; matrix->_13 *= scale;
	matrix->_21 *= scale; matrix->_22 *= scale; matrix->_23 *= scale;
	matrix->_31 *= scale; matrix->_32 *= scale; matrix->_33 *= scale;
}

void R_SetEntityWorldTransform( const cl_entity_s *ent,
	float renderTime, D3DMATRIX *out )
{
	BoneMat34 entityMatrix;
	R_InterpolateEntityTransform( ent, renderTime, entityMatrix, false );
	R_BoneMatToWorld( entityMatrix, out );
}

static inline uint32_t R_StudioRenderFxRandomU32( void )
{
	uint32_t x = g_studioRenderFxRandomState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	if( x == 0 )
		x = 0xA341316Cu;
	g_studioRenderFxRandomState = x;
	return x;
}

int R_StudioRenderFxRandomLong( int minimum, int maximum )
{
	if( maximum <= minimum )
		return minimum;

	const uint32_t range =
		static_cast<uint32_t>( maximum - minimum + 1 );
	return minimum + static_cast<int>(
		R_StudioRenderFxRandomU32( ) % range );
}

static inline float R_StudioRenderFxRandomFloat(
	float minimum, float maximum )
{
	const float unit =
		static_cast<float>( R_StudioRenderFxRandomU32( ) & 0x00FFFFFFu ) *
		( 1.0f / 16777215.0f );
	return minimum + ( maximum - minimum ) * unit;
}

static void R_ApplyStudioFx(
	const cl_entity_s *ent, BoneMat34 transform )
{
	if( !ent )
		return;

	if( ent->curstate.renderfx != kRenderFxDistort &&
		ent->curstate.renderfx != kRenderFxHologram )
	{
		return;
	}

	if( R_StudioRenderFxRandomLong( 0, 49 ) == 0 )
	{
		int axis = R_StudioRenderFxRandomLong( 0, 1 );
		if( axis == 1 )
			axis = 2;

		const float scale =
			R_StudioRenderFxRandomFloat( 1.0f, 1.484f );

		transform[axis][0] *= scale;
		transform[axis][1] *= scale;
		transform[axis][2] *= scale;
	}
	else if( R_StudioRenderFxRandomLong( 0, 49 ) == 0 )
	{
		const float offset =
			R_StudioRenderFxRandomFloat( -10.0f, 10.0f );
		const int axis = R_StudioRenderFxRandomLong( 0, 2 );
		transform[axis][3] += offset;
	}
}

static void R_SetStudioWorld( const cl_entity_s *ent,
	float renderTime, D3DMATRIX *out )
{
	BoneMat34 entityMatrix;
	R_InterpolateEntityTransform( ent, renderTime, entityMatrix, true );
	R_ApplyStudioFx( ent, entityMatrix );
	R_BoneMatToWorld( entityMatrix, out );
	R_ApplyEntityScale( ent, out );
}

static void R_SetPlayerGaitWorldTransform( const cl_entity_s *ent,
	float gaitYaw, D3DMATRIX *out )
{
	if( !ent || !out )
		return;

	float origin[3] =
	{
		ent->origin[0],
		ent->origin[1],
		ent->origin[2]
	};
	float angles[3] = { 0.0f, gaitYaw, 0.0f };

	BoneMat34 entityMatrix;
	GoldSrcEntityAngleMatrix( angles, origin, entityMatrix );
	R_BoneMatToWorld( entityMatrix, out );
	R_ApplyEntityScale( ent, out );
}

void R_TransformPoint( const D3DMATRIX &matrix, float x, float y, float z, vec3_t out )
{
	out[0] = x * matrix._11 + y * matrix._21 + z * matrix._31 + matrix._41;
	out[1] = x * matrix._12 + y * matrix._22 + z * matrix._32 + matrix._42;
	out[2] = x * matrix._13 + y * matrix._23 + z * matrix._33 + matrix._43;
}

void R_TransformBounds( const float mins[3], const float maxs[3],
	const D3DMATRIX &matrix, vec3_t outMins, vec3_t outMaxs )
{
	ClearBounds( outMins, outMaxs );
	for( int x = 0; x < 2; ++x )
	{
		for( int y = 0; y < 2; ++y )
		{
			for( int z = 0; z < 2; ++z )
			{
				vec3_t local =
				{
					x ? maxs[0] : mins[0],
					y ? maxs[1] : mins[1],
					z ? maxs[2] : mins[2]
				};
				vec3_t world;
				R_TransformPoint( matrix, local[0], local[1], local[2], world );
				AddPointToBounds( world, outMins, outMaxs );
			}
		}
	}
}

static bool R_RecursiveWorldLightPoint( const model_t *world,
	const mnode_t *node, const vec3_t start, const vec3_t end,
	color24 &outLight, float *outSpot = nullptr, float *outLightVec = nullptr )
{
	if( !world || !node || node->contents < 0 )
		return false;

	const mplane_t *plane = node->plane;
	if( !plane )
		return false;

	const float front = DotProduct( start, plane->normal ) - plane->dist;
	const float back = DotProduct( end, plane->normal ) - plane->dist;
	const int side = front < 0.0f ? 1 : 0;

	if( ( back < 0.0f ? 1 : 0 ) == side )
		return R_RecursiveWorldLightPoint( world, node_child( node, side, world ),
			start, end, outLight, outSpot, outLightVec );

	const float fraction = front / ( front - back );
	vec3_t midpoint;
	midpoint[0] = start[0] + ( end[0] - start[0] ) * fraction;
	midpoint[1] = start[1] + ( end[1] - start[1] ) * fraction;
	midpoint[2] = start[2] + ( end[2] - start[2] ) * fraction;

	if( R_RecursiveWorldLightPoint( world, node_child( node, side, world ),
		start, midpoint, outLight, outSpot, outLightVec ) )
	{
		return true;
	}

	const int firstSurface = node_firstsurface( node, world );
	const int surfaceCount = node_numsurfaces( node, world );
	for( int i = 0; i < surfaceCount; ++i )
	{
		const int surfaceIndex = firstSurface + i;
		if( surfaceIndex < 0 || surfaceIndex >= world->numsurfaces )
			continue;

		const msurface_t *surface = &world->surfaces[surfaceIndex];
		if( !surface->samples || ( surface->flags & SURF_DRAWTILED ) )
			continue;

		float deluxeDir[3];
		if( R_ReadSurfaceStaticLight( surface, midpoint, outLight,
			outLightVec ? deluxeDir : nullptr ) )
		{
			if( outSpot )
				VectorCopy( midpoint, outSpot );
			if( outLightVec )
				VectorCopy( deluxeDir, outLightVec );
			return true;
		}
	}

	return R_RecursiveWorldLightPoint( world, node_child( node, side ^ 1, world ),
		midpoint, end, outLight, outSpot, outLightVec );
}

static void R_StudioInverseTransformPoint( const BoneMat34 matrix,
	const vec3_t point, vec3_t out )
{
	const float x = point[0] - matrix[0][3];
	const float y = point[1] - matrix[1][3];
	const float z = point[2] - matrix[2][3];
	out[0] = x * matrix[0][0] + y * matrix[1][0] + z * matrix[2][0];
	out[1] = x * matrix[0][1] + y * matrix[1][1] + z * matrix[2][1];
	out[2] = x * matrix[0][2] + y * matrix[1][2] + z * matrix[2][2];
}

static void R_StudioTransformDirection( const BoneMat34 matrix,
	const vec3_t direction, vec3_t out )
{
	out[0] = matrix[0][0] * direction[0] + matrix[0][1] * direction[1] + matrix[0][2] * direction[2];
	out[1] = matrix[1][0] * direction[0] + matrix[1][1] * direction[1] + matrix[1][2] * direction[2];
	out[2] = matrix[2][0] * direction[0] + matrix[2][1] * direction[1] + matrix[2][2] * direction[2];
}

static bool R_EngineExtendedLighting( )
{

	static cvar_t *engineExtended = nullptr;
	static bool triedLookup = false;
	if( !triedLookup && gEngfuncs.pfnGetCvarPointer )
	{
		engineExtended = gEngfuncs.pfnGetCvarPointer( "r_lighting_extended" );
		triedLookup = true;
	}
	return engineExtended ? engineExtended->value != 0.0f : false;
}

static bool R_StudioExtendedProbe( )
{
	if( dx_studio_extended_lighting.value == 0.0f )
		return false;

	return R_EngineExtendedLighting( );
}

bool R_StudioTraceLight( const vec3_t start, const vec3_t end,
	color24 &outLight, float *outSpot = nullptr, float *outLightVec = nullptr,
	bool allowReverseExtendedTrace = true,
	bool engineExtendedIndependent = false )
{
	if( !start || !end )
	{
		if( outSpot ) VectorClear( outSpot );
		if( outLightVec ) VectorClear( outLightVec );
		return false;
	}

	bool found = false;
	float bestDistanceSq = FLT_MAX;
	color24 bestLight;
	vec3_t bestSpot;
	vec3_t bestDir;

	const bool extended =
		( engineExtendedIndependent ? R_EngineExtendedLighting( )
			: R_StudioExtendedProbe( ) ) &&
		gEngfuncs.EV_GetPhysent != nullptr;
	const int maxPhysents = extended ? MAX_PHYSENTS : 1;

	for( int i = 0; i < maxPhysents; ++i )
	{
		const physent_t *pe = extended ? gEngfuncs.EV_GetPhysent( i ) : nullptr;
		if( extended && !pe )
			break;
		const model_t *model = nullptr;
		vec3_t localStart, localEnd;
		BoneMat34 objectMatrix;
		bool transformed = false;

		if( pe )
		{
			model = pe->model;
			if( !model || model->type != mod_brush || !model->nodes )
				continue;

			vec3_t offset;
			VectorAdd( model->hulls[0].clip_mins, pe->origin, offset );
			const bool rotated = fabsf( pe->angles[0] ) > 0.0001f ||
				fabsf( pe->angles[1] ) > 0.0001f || fabsf( pe->angles[2] ) > 0.0001f;
			if( rotated )
			{
				GoldSrcEntityAngleMatrix( pe->angles, offset, objectMatrix );
				R_StudioInverseTransformPoint( objectMatrix, start, localStart );
				R_StudioInverseTransformPoint( objectMatrix, end, localEnd );
				transformed = true;
			}
			else
			{
				VectorSubtract( start, offset, localStart );
				VectorSubtract( end, offset, localEnd );
				GoldSrcEntityAngleMatrix( pe->angles, offset, objectMatrix );
			}
		}
		else
		{

			if( i != 0 || !g_worldmodel || !g_worldmodel->nodes )
				continue;
			model = g_worldmodel;
			VectorCopy( start, localStart );
			VectorCopy( end, localEnd );
			float zeroAngles[3] = { 0.0f, 0.0f, 0.0f };
			float zeroOrigin[3] = { 0.0f, 0.0f, 0.0f };
			GoldSrcEntityAngleMatrix( zeroAngles, zeroOrigin, objectMatrix );
		}

		const int firstClipNode = model->hulls[0].firstclipnode;
		if( firstClipNode < 0 )
			continue;
		const mnode_t *root = &model->nodes[firstClipNode];
		color24 light;
		vec3_t localSpot;
		vec3_t localDir;
		if( !R_RecursiveWorldLightPoint( model, root, localStart, localEnd,
			light, localSpot, outLightVec ? localDir : nullptr ) )
		{
			continue;
		}

		vec3_t worldSpot;
		if( pe )
		{
			VectorTransform( localSpot, objectMatrix, worldSpot );
		}
		else
		{
			VectorCopy( localSpot, worldSpot );
		}
		vec3_t delta;
		VectorSubtract( worldSpot, start, delta );
		const float distanceSq = DotProduct( delta, delta );
		if( distanceSq >= bestDistanceSq )
			continue;

		bestDistanceSq = distanceSq;
		bestLight = light;
		VectorCopy( worldSpot, bestSpot );
		if( outLightVec )
		{
			if( pe && transformed )
				R_StudioTransformDirection( objectMatrix, localDir, bestDir );
			else
				VectorCopy( localDir, bestDir );
		}
		found = true;

		if( light.r || light.g || light.b )
			break;
	}

	if( !found )
	{
		if( outSpot ) VectorClear( outSpot );
		if( outLightVec ) VectorClear( outLightVec );
		return false;
	}
	outLight = bestLight;
	if( outSpot ) VectorCopy( bestSpot, outSpot );
	if( outLightVec ) VectorCopy( bestDir, outLightVec );

	if( allowReverseExtendedTrace && extended && outSpot && outLightVec &&
		bestLight.r == 0 && bestLight.g == 0 && bestLight.b == 0 )
	{
		color24 reverseLight;
		vec3_t reverseSpot;
		vec3_t reverseDir;
		if( R_StudioTraceLight( end, start, reverseLight, reverseSpot, reverseDir,
			false, engineExtendedIndependent ) )
		{
			outLight = reverseLight;
			VectorCopy( reverseSpot, outSpot );
			VectorCopy( reverseDir, outLightVec );
		}
	}
	return true;
}

static float R_StudioProbeBrightness( const vec3_t start, int directionSign )
{
	if( !g_worldmodel || !g_worldmodel->nodes || !g_worldmodel->lightdata )
		return 1.0f;

	vec3_t source = { start[0], start[1], start[2] - directionSign * 8.0f };
	vec3_t end = { source[0], source[1], source[2] + directionSign * 2048.0f };
	color24 light;
	if( !R_StudioTraceLight( source, end, light ) )
	{
		return 0.0f;
	}
	return ( static_cast<float>( light.r ) + static_cast<float>( light.g ) +
		static_cast<float>( light.b ) ) * ( 1.0f / 768.0f );
}

static D3DCOLOR R_StudioBuildStaticProbe( const vec3_t start,
	int directionSign, float outDirection[3] )
{
	directionSign = directionSign >= 0 ? 1 : -1;

	if( !start || !g_worldmodel || !g_worldmodel->nodes || !g_worldmodel->lightdata )
	{
		if( outDirection )
		{
			outDirection[0] = 0.0f;
			outDirection[1] = 0.0f;
			outDirection[2] = static_cast<float>( directionSign );
		}
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );
	}

	vec3_t source =
	{
		start[0], start[1], start[2] - directionSign * 8.0f
	};
	vec3_t end =
	{
		source[0], source[1], source[2] + directionSign * 2048.0f
	};

	color24 light;
	vec3_t deluxeDir;
	vec3_t lightSpot;
	const bool wantDeluxe = outDirection && dx_studio_extended_lighting.value != 0.0f;
	const bool hit = wantDeluxe
		? R_StudioTraceLight( source, end, light, lightSpot, deluxeDir )
		: R_StudioTraceLight( source, end, light );

	D3DCOLOR color = D3DCOLOR_ARGB( 255, 128, 128, 128 );
	if( hit )
	{
		color = D3DCOLOR_ARGB( 255,
			R_OverbrightChannel( light.r ),
			R_OverbrightChannel( light.g ),
			R_OverbrightChannel( light.b ) );
	}

	if( !outDirection )
		return color;

	if( hit && wantDeluxe && R_StudioNormalizeVector( deluxeDir ) )
	{
		VectorCopy( deluxeDir, outDirection );
		return color;
	}

	vec3_t p;
	VectorCopy( start, p );
	p[0] -= 16.0f; p[1] -= 16.0f;
	const float grad0 = R_StudioProbeBrightness( p, directionSign );
	p[0] += 32.0f;
	const float grad1 = R_StudioProbeBrightness( p, directionSign );
	p[1] += 32.0f;
	const float grad2 = R_StudioProbeBrightness( p, directionSign );
	p[0] -= 32.0f;
	const float grad3 = R_StudioProbeBrightness( p, directionSign );

	outDirection[0] = grad0 - grad1 - grad2 + grad3;
	outDirection[1] = grad1 + grad0 - grad2 - grad3;
	outDirection[2] = static_cast<float>( directionSign );
	if( !R_StudioNormalizeVector( outDirection ) )
	{
		outDirection[0] = 0.0f;
		outDirection[1] = 0.0f;
		outDirection[2] = static_cast<float>( directionSign );
	}
	return color;
}

D3DCOLOR R_LightColorAlongSegment( const vec3_t start, const vec3_t end,
	float *lightspot, float *lightvec )
{
	if( !start || !end )
	{
		if( lightspot ) VectorClear( lightspot );
		if( lightvec ) VectorClear( lightvec );
		return D3DCOLOR_ARGB( 0, 0, 0, 0 );
	}
	if( !g_worldmodel || !g_worldmodel->nodes || !g_worldmodel->lightdata )
	{
		if( lightspot ) VectorClear( lightspot );
		if( lightvec ) VectorClear( lightvec );
		return D3DCOLOR_ARGB( 0, 255, 255, 255 );
	}

	color24 light;
	if( !R_StudioTraceLight( start, end, light, lightspot, lightvec ) )
	{
		return D3DCOLOR_ARGB( 0, 0, 0, 0 );
	}

	if( lightvec ) VectorNormalize( lightvec );
	return D3DCOLOR_ARGB( 0,
		R_OverbrightChannel( light.r ),
		R_OverbrightChannel( light.g ),
		R_OverbrightChannel( light.b ) );
}

static float R_StudioLightCacheDistSq( )
{
	const float distance = std::max( 0.0f, std::min( 64.0f,
		dx_studio_light_cache_dist.value ) );
	return distance * distance;
}

static float R_StudioPoseUpdateHz( )
{

	return std::max( 0.0f, std::min( 120.0f, dx_studio_update_hz.value ) );
}

static float R_StudioPosePhase01( const cl_entity_s *ent )
{
	if( !ent || dx_studio_stagger_updates.value == 0.0f )
		return 0.0f;

	unsigned int seed = static_cast<unsigned int>( ent->curstate.number );
	if( seed == 0 )
		seed = static_cast<unsigned int>( reinterpret_cast<uintptr_t>( ent ) >> 4 );
	seed = seed * 1664525u + 1013904223u;
	seed ^= seed >> 16;
	return static_cast<float>( seed & 1023u ) * ( 1.0f / 1024.0f );
}

static float R_StudioQuantizePoseTime( const cl_entity_s *ent, float renderTime,
	bool isViewModel )
{
	const float hz = R_StudioPoseUpdateHz( );
	if( hz <= 0.0f || R_NoInterpolationEnabled( ) )
		return renderTime;
	if( isViewModel && dx_studio_viewmodel_throttle.value == 0.0f )
		return renderTime;

	const float phase = R_StudioPosePhase01( ent );
	const float sample = floorf( renderTime * hz + phase ) - phase;
	return sample / hz;
}

static unsigned int R_StudioSkyLightingSignature( )
{

	unsigned int hash = 2166136261u;
	if( !gp_movevars )
		return hash;

	auto mixFloat = [&hash]( float value )
		{
			unsigned int bits = 0;
			static_assert( sizeof( bits ) == sizeof( value ), "float hash size mismatch" );
			memcpy( &bits, &value, sizeof( bits ) );
			hash = ( hash ^ bits ) * 16777619u;
		};
	for( int i = 0; i < 3; ++i )
		mixFloat( gp_movevars->skycolor[i] );
	for( int i = 0; i < 3; ++i )
		mixFloat( gp_movevars->skyvec[i] );
	return hash;
}

static bool R_StudioTrySkyLight( const cl_entity_s *ent, const vec3_t start,
	int directionSign, D3DCOLOR &outColor, float outDir[3] );

static D3DCOLOR R_StudioCachedProbe( studio_light_probe_cache_t &cache,
	const cl_entity_s *ent, const vec3_t start, int directionSign,
	float outLightDir[3] )
{
	directionSign = directionSign >= 0 ? 1 : -1;
	const bool extendedLighting = dx_studio_extended_lighting.value != 0.0f;
	const bool extendedPhysentProbe = extendedLighting && R_StudioExtendedProbe( );
	const bool forceSky = extendedLighting && ent && ent->model &&
		ent->model->type == mod_studio && ( ent->model->flags & STUDIO_FORCE_SKYLIGHT ) != 0;
	const unsigned int skySignature = extendedLighting ? R_StudioSkyLightingSignature( ) : 0u;
	const float maxDistanceSq = R_StudioLightCacheDistSq( );
	const float now = gp_cl ? gp_cl->time : 0.0f;

	const bool timeForward = now >= cache.sampleTime;
	const float sampleAge = timeForward ? now - cache.sampleTime : FLT_MAX;
	const bool styleChanged = cache.lightStyleRevision != g_lightStyleRevision;
	const bool needsTimedRefresh = !cache.usedSky &&
		( extendedPhysentProbe || styleChanged );
	const bool temporalFresh = !needsTimedRefresh || sampleAge < ( 1.0f / 64.0f );

	if( maxDistanceSq > 0.0f && cache.valid && temporalFresh &&
		cache.directionSign == directionSign &&
		cache.extendedLighting == extendedLighting &&
		cache.extendedPhysentProbe == extendedPhysentProbe &&
		cache.forceSky == forceSky && cache.model == ( ent ? ent->model : nullptr ) &&
		cache.skySignature == skySignature )
	{
		const float dx = cache.origin[0] - start[0];
		const float dy = cache.origin[1] - start[1];
		const float dz = cache.origin[2] - start[2];
		if( dx * dx + dy * dy + dz * dz <= maxDistanceSq )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_LIGHT_PROBE_CACHE_HIT, 1 );
			if( cache.usedSky )
				DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_SKY_PROBE_USED, 1 );
			if( outLightDir ) VectorCopy( cache.lightDirection, outLightDir );
			return cache.color;
		}
	}

	DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_LIGHT_PROBE_REBUILD, 1 );
	DX_MODEL_PROFILE_SCOPE( studioLightProbeProfileScope, DX_PROFILE_PHASE_STUDIO_LIGHT_PROBE );
	cache.usedSky = false;
	cache.color = D3DCOLOR_ARGB( 255, 128, 128, 128 );
	cache.lightDirection[0] = 0.0f;
	cache.lightDirection[1] = 0.0f;
	cache.lightDirection[2] = static_cast<float>( directionSign );

	if( extendedLighting && R_StudioTrySkyLight( ent, start, directionSign,
		cache.color, cache.lightDirection ) )
	{
		cache.usedSky = true;
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_SKY_PROBE_USED, 1 );
	}
	else
	{
		cache.color = R_StudioBuildStaticProbe(
			start, directionSign, cache.lightDirection );
	}

	VectorCopy( start, cache.origin );
	cache.lightStyleRevision = g_lightStyleRevision;
	cache.directionSign = directionSign;
	cache.extendedLighting = extendedLighting;
	cache.extendedPhysentProbe = extendedPhysentProbe;
	cache.forceSky = forceSky;
	cache.model = ent ? ent->model : nullptr;
	cache.skySignature = skySignature;
	cache.sampleTime = now;
	cache.valid = true;
	if( outLightDir ) VectorCopy( cache.lightDirection, outLightDir );
	return cache.color;
}

static byte R_StudioClampLightByte( float value )
{
	return static_cast<byte>( std::max( 0.0f, std::min( 255.0f, value ) ) + 0.5f );
}

static bool R_StudioTrySkyLight( const cl_entity_s *ent, const vec3_t start,
	int directionSign, D3DCOLOR &outColor, float outDir[3] )
{
	if( dx_studio_extended_lighting.value == 0.0f || !ent || !ent->model ||
		!start || !gp_movevars )
	{
		return false;
	}

	DX_MODEL_PROFILE_SCOPE( studioSkyProbeProfileScope, DX_PROFILE_PHASE_STUDIO_SKY_PROBE );
	const float skySum = gp_movevars->skycolor[0] + gp_movevars->skycolor[1] +
		gp_movevars->skycolor[2];
	if( fabsf( skySum ) <= 0.0001f )
		return false;

	const bool forceSky = ent->model->type == mod_studio &&
		( ent->model->flags & STUDIO_FORCE_SKYLIGHT ) != 0;
	bool hitSky = forceSky;

	vec3_t source =
	{
		start[0], start[1], start[2] - directionSign * 8.0f
	};
	vec3_t skyReach;
	const float skyDistance = ( gp_host &&
		FBitSet( gp_host->features, ENGINE_WRITE_LARGE_COORD ) ) ? 65536.0f : 8192.0f;
	VectorScale( gp_movevars->skyvec, skyDistance, skyReach );
	vec3_t end;
	VectorSubtract( start, skyReach, end );

	if( !hitSky && gEngfuncs.CL_TraceLine && gEngfuncs.EV_TraceSurface )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_SKY_TRACE, 1 );
		auto trace = gEngfuncs.CL_TraceLine( source, end, PM_WORLD_ONLY );
		msurface_t *surface = gEngfuncs.EV_TraceSurface(
			trace.ent > 0 ? trace.ent : 0, source, end );
		hitSky = surface && ( surface->flags & SURF_DRAWSKY );
	}

	if( !hitSky )
		return false;

	outColor = D3DCOLOR_ARGB( 255,
		R_StudioClampLightByte( gp_movevars->skycolor[0] ),
		R_StudioClampLightByte( gp_movevars->skycolor[1] ),
		R_StudioClampLightByte( gp_movevars->skycolor[2] ) );
	VectorCopy( gp_movevars->skyvec, outDir );
	if( !R_StudioNormalizeVector( outDir ) )
	{
		outDir[0] = 0.0f;
		outDir[1] = 0.0f;
		outDir[2] = static_cast<float>( directionSign );
	}
	return true;
}

static void R_StudioAddPointLightToProbe( const dx_dlight_t &light,
	const vec3_t point, float intensity, float &total, float finalRgb[3],
	float weightedDir[3] )
{

	if( light.radius <= 0.0f )
		return;

	vec3_t dist;
	VectorSubtract( point, light.origin, dist );
	const float distance = VectorLength( dist );
	const float add = ( light.radius - distance ) * intensity;
	if( add <= 0.0f )
		return;

	total += add;
	if( distance > 1.0f )
		VectorScale( dist, add / distance, dist );
	else
		VectorScale( dist, add, dist );
	VectorAdd( weightedDir, dist, weightedDir );

	finalRgb[0] += light.r * add;
	finalRgb[1] += light.g * add;
	finalRgb[2] += light.b * add;
}

static void R_StudioAddLineLightToProbe( const dx_line_light_t &light,
	const vec3_t point, float intensity, float &total, float finalRgb[3],
	float weightedDir[3] )
{
	if( light.radius <= 0.0f )
		return;

	float fraction = 0.0f;
	const float distSq = R_DistancePointToSegmentSq( point, light.start, light.end, fraction );
	const float distance = sqrtf( std::max( 0.0f, distSq ) );
	const float beamScale = std::max( 0.0f, std::min( 8.0f,
		dx_dlight_beam_intensity.value ) );
	const float add = ( light.radius - distance ) * intensity * beamScale;
	if( add <= 0.0f )
		return;

	vec3_t segment, closest, dist;
	VectorSubtract( light.end, light.start, segment );
	VectorMA( light.start, fraction, segment, closest );
	VectorSubtract( point, closest, dist );
	const float safeDistance = VectorLength( dist );
	if( safeDistance > 1.0f )
		VectorScale( dist, add / safeDistance, dist );
	else
		VectorScale( dist, add, dist );

	total += add;
	VectorAdd( weightedDir, dist, weightedDir );
	finalRgb[0] += light.r * add;
	finalRgb[1] += light.g * add;
	finalRgb[2] += light.b * add;
}

static studio_light_result_t R_StudioBuildLight(
	const cl_entity_s *ent, studio_light_probe_cache_t &cache,
	const vec3_t start, int directionSign )
{
	studio_light_result_t result{};
	directionSign = directionSign >= 0 ? 1 : -1;
	result.direction[2] = static_cast<float>( directionSign );

	if( !ent || ent->curstate.rendermode == kRenderGlow ||
		( ent->curstate.effects & EF_FULLBRIGHT ) || !g_worldmodel ||
		!g_worldmodel->nodes || !g_worldmodel->lightdata )
	{
		result.diffuse = D3DCOLOR_ARGB( 255, 255, 255, 255 );
		result.directFraction = 0.0f;
		return result;
	}

	const D3DCOLOR baseLight = R_StudioCachedProbe(
		cache, ent, start, directionSign, result.direction );

	float baseR = static_cast<float>( ( baseLight >> 16 ) & 0xFF );
	float baseG = static_cast<float>( ( baseLight >> 8 ) & 0xFF );
	float baseB = static_cast<float>( baseLight & 0xFF );

	if( dx_studio_extended_lighting.value != 0.0f &&
		ent->curstate.renderfx == kRenderFxLightMultiplier &&
		ent->curstate.iuser4 != 10 )
	{
		const float multiplier = static_cast<float>( ent->curstate.iuser4 ) * 0.1f;
		baseR *= multiplier;
		baseG *= multiplier;
		baseB *= multiplier;
	}

	float total = std::max( baseR, std::max( baseG, baseB ) );
	if( total <= 0.0f )
		total = 1.0f;

	float finalRgb[3] = { baseR, baseG, baseB };
	float weightedDir[3] =
	{
		result.direction[0] * total,
		result.direction[1] * total,
		result.direction[2] * total
	};

	if( dx_dlight_studio.value != 0.0f &&
		dx_studio_extended_lighting.value != 0.0f &&
		( g_dxStudioGlobalLightCount > 0 || !g_dxLineLights.empty( ) ) )
	{
		DX_MODEL_PROFILE_SCOPE( studioDynamicLightsProfileScope, DX_PROFILE_PHASE_STUDIO_DYNAMIC_LIGHTS );
		const float intensity = R_DynamicLightIntensityScale( );
		if( g_dxStudioGlobalLightCount > 0 )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_DYNAMIC_LIGHT_CANDIDATE,
				static_cast<unsigned int>( g_dxStudioGlobalLightCount ) );
			for( int i = 0; i < g_dxStudioGlobalLightCount; ++i )
			{
				const dx_dlight_t &light =
					g_dxDLights[g_dxStudioGlobalLightIndices[i]];
				R_StudioAddPointLightToProbe( light, start, intensity,
					total, finalRgb, weightedDir );
			}
		}
		if( !g_dxLineLights.empty( ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_DYNAMIC_LIGHT_CANDIDATE,
				static_cast<unsigned int>( g_dxLineLights.size( ) ) );
			for( const dx_line_light_t &light : g_dxLineLights )
				R_StudioAddLineLightToProbe( light, start, intensity,
					total, finalRgb, weightedDir );
		}
	}
	else if( dx_dlight_studio.value != 0.0f &&
		dx_studio_extended_lighting.value == 0.0f )
	{

		const D3DCOLOR flat = R_AddStudioDLights( baseLight, start );
		finalRgb[0] = static_cast<float>( ( flat >> 16 ) & 0xFF );
		finalRgb[1] = static_cast<float>( ( flat >> 8 ) & 0xFF );
		finalRgb[2] = static_cast<float>( flat & 0xFF );
	}

	const float shadeScale = ( dx_studio_extended_lighting.value != 0.0f &&
		ent->model->type == mod_studio && ( ent->model->flags & STUDIO_AMBIENT_LIGHT ) )
		? 0.6f : R_StudioDirectLightScale( );
	VectorScale( weightedDir, shadeScale, weightedDir );
	float shade = VectorLength( weightedDir );
	float ambient = total - shade;
	ambient = std::max( 0.0f, std::min( 128.0f, ambient ) );
	shade = std::max( 0.0f, shade );
	if( ambient + shade > 255.0f )
		shade = std::max( 0.0f, 255.0f - ambient );

	if( !R_StudioNormalizeVector( weightedDir ) )
	{
		weightedDir[0] = 0.0f;
		weightedDir[1] = 0.0f;
		weightedDir[2] = static_cast<float>( directionSign );
	}
	VectorCopy( weightedDir, result.direction );

	const float illum = std::max( 1.0f, ambient + shade );
	result.directFraction = std::max( 0.0f, std::min( 1.0f, shade / illum ) );

	const float maxFinal = std::max( finalRgb[0], std::max( finalRgb[1], finalRgb[2] ) );
	if( maxFinal > 0.0001f )
	{
		const float colorScale = illum / maxFinal;
		finalRgb[0] *= colorScale;
		finalRgb[1] *= colorScale;
		finalRgb[2] *= colorScale;
	}
	else
	{
		finalRgb[0] = finalRgb[1] = finalRgb[2] = illum;
	}

	result.diffuse = D3DCOLOR_ARGB( 255,
		R_StudioClampLightByte( finalRgb[0] ),
		R_StudioClampLightByte( finalRgb[1] ),
		R_StudioClampLightByte( finalRgb[2] ) );
	return result;
}

static D3DCOLOR R_StudioEntityLight( const cl_entity_s *ent,
	const vec3_t start, float outLightDir[3], float *outDirectFraction )
{
	const int directionSign = ( ent && ( ent->curstate.effects & EF_INVLIGHT ) )
		? 1 : -1;
	if( !ent )
	{
		if( outLightDir ) VectorSet( outLightDir, 0.0f, 0.0f, static_cast<float>( directionSign ) );
		if( outDirectFraction ) *outDirectFraction = 0.0f;
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );
	}

	studio_light_result_t result = R_StudioBuildLight(
		ent, R_GetEntityCache( ent ).lightProbe, start, directionSign );
	if( outLightDir ) VectorCopy( result.direction, outLightDir );
	if( outDirectFraction ) *outDirectFraction = result.directFraction;
	return result.diffuse;
}

static D3DCOLOR R_StudioViewLightColorAtPoint( const cl_entity_s *ent,
	const vec3_t start, float outLightDir[3], float *outDirectFraction )
{
	if( !ent )
	{
		if( outLightDir ) VectorSet( outLightDir, 0.0f, 0.0f, -1.0f );
		if( outDirectFraction ) *outDirectFraction = 0.0f;
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );
	}
	studio_light_result_t result = R_StudioBuildLight(
		ent, g_viewModelLightProbeCache, start, -1 );
	if( outLightDir ) VectorCopy( result.direction, outLightDir );
	if( outDirectFraction ) *outDirectFraction = result.directFraction;
	return result.diffuse;
}

void R_LoadStudioModel( model_t *mod )
{
	studiohdr_t *phdr = static_cast<studiohdr_t *>( gEngfuncs.Mod_Extradata( mod_studio, mod ) );
	R_CacheStudioRenderMode( mod,
		R_StudioHeaderRenderMode( phdr ) );

	model_drawdata_t *mdd = new model_drawdata_t;
	mdd->mod = mod;
	mdd->loaded = true;

	if( !IsStudioModelAnimated( phdr ) )
	{
		ExtractStudioModelBatched( phdr, *mdd );
	}
	else
	{

		mdd->renderdata.clear( );
		mdd->texIds.clear( );
	}

	R_RegisterStudioModelDrawData( mod, mdd );
}

static void R_DrawStudioBatches( model_drawdata_t &drawData,
	const cl_entity_s *ent )
{
	studio_material_state_t materialState = DX_STUDIO_MATERIAL_BASE;

	for( int i = 0; i < static_cast<int>( drawData.texIds.size( ) ); ++i )
	{
		renderdata_t &rd = drawData.renderdata[i];
		if( rd.primitiveCount <= 0 || rd.indices.empty( ) || rd.verts.empty( ) )
			continue;

		const int batchKey = drawData.texIds[i];
		const int textureId = R_StudioCpuBatchTextureId( batchKey );
		const unsigned int flags =
			R_StudioCpuBatchStateFlags( batchKey );
		DX_Bind( 0, textureId );
		R_ApplyStudioMaterialState( flags, ent, materialState );
		R_DrawScratchIndexedPrimitive( D3DPT_TRIANGLELIST,
			static_cast<UINT>( rd.verts.size( ) ),
			static_cast<UINT>( rd.primitiveCount ), &rd.indices[0], D3DFMT_INDEX32,
			&rd.verts[0], sizeof( brush_vertex_t ) );
	}
}

static void R_DrawStudioEntityModel( model_t *mod, cl_entity_s *ent,
	int sequence, float frame, int gaitSeq, float gaitFrame, float gaitYaw,
	float renderTime, D3DCOLOR studioLight, const float studioLightDir[3],
	float studioDirectLightScale, bool forceOverlayDepth, const D3DMATRIX *currentWorldMatrix,
	const BoneMat34 *forcedPoseBoneMatrices = nullptr,
	bool processRendererMuzzleEvents = true )
{
	DX_MODEL_PROFILE_SCOPE( studioProfileScope, DX_PROFILE_PHASE_STUDIOMODEL );
	model_drawdata_t *mdd = R_FindStudioModelDrawData( mod );
	if( !mdd || !mdd->loaded )
		return;

	studiohdr_t *hdr = static_cast<studiohdr_t *>(
		gEngfuncs.Mod_Extradata( mod_studio, mod ) );
	if( !hdr )
		return;

	studio_cache_t &cache = R_GetStudioEntityGpuCache( ent );

	const bool topologyMatches = cache.topologyValid && cache.model == mod &&
		cache.body == ent->curstate.body && cache.skin == ent->curstate.skin;

	BoneMat34 precomputedMuzzleBones[MAXSTUDIOBONES];
	const BoneMat34 *precomputedPoseBones = nullptr;
	bool muzzleEventsProcessedBeforeDraw = false;
	D3DMATRIX muzzleWorld;
	R_StudioBuildWorldFromMatrix( currentWorldMatrix, muzzleWorld );

	if( forcedPoseBoneMatrices )
		precomputedPoseBones = forcedPoseBoneMatrices;

	if( processRendererMuzzleEvents &&
		forceOverlayDepth && dx_muzzleflash_sprite.value != 0.0f )
	{
		StudioCalcBoneTransformsInternal( mod, hdr, sequence, frame,
			precomputedMuzzleBones, gaitSeq, gaitFrame, gaitYaw, ent, renderTime );
		precomputedPoseBones = precomputedMuzzleBones;
		R_StudioProcessMuzzleEvents( hdr, ent, sequence,
			frame, precomputedMuzzleBones, muzzleWorld, true, renderTime );
		muzzleEventsProcessedBeforeDraw = true;
	}

	const D3DCOLOR effectiveStudioLight = forceOverlayDepth
		? R_ApplyViewmodelMuzzleLight( studioLight, ent, renderTime )
		: studioLight;

	if( ( topologyMatches || R_BuildStudioTopology( cache, mod, hdr,
		ent->curstate.body, ent->curstate.skin ) ) &&
		R_UpdateStudioVertices( cache, mod, hdr, ent, sequence, frame,
			gaitSeq, gaitFrame, gaitYaw, renderTime, effectiveStudioLight,
			studioLightDir, studioDirectLightScale, currentWorldMatrix, forceOverlayDepth, precomputedPoseBones ) )
	{
		if( processRendererMuzzleEvents && !muzzleEventsProcessedBeforeDraw )
		{
			if( cache.gpuSkinningPoseValid )
			{
				R_StudioProcessMuzzleEvents( hdr, ent, sequence,
					frame, cache.gpuSkinningBoneMatrices, muzzleWorld,
					forceOverlayDepth, renderTime );
			}
			else if( dx_muzzleflash_sprite.value != 0.0f )
			{
				BoneMat34 muzzleBones[MAXSTUDIOBONES];
				StudioCalcBoneTransformsInternal( mod, hdr, sequence, frame,
					muzzleBones, gaitSeq, gaitFrame, gaitYaw, ent, renderTime );
				R_StudioProcessMuzzleEvents( hdr, ent, sequence,
					frame, muzzleBones, muzzleWorld, forceOverlayDepth, renderTime );
			}
		}

		const bool normalRenderMode = !ent || ent->curstate.rendermode == kRenderNormal;
		dx_entity_state_t renderState{};
		if( normalRenderMode )
			R_SetOpaqueEntityBaseline( );
		else
			R_BeginEntityRenderMode( ent, &renderState );

		const bool drawMuzzleBeforeModel = processRendererMuzzleEvents &&
			forceOverlayDepth && R_MuzzleBeforeViewmodel( );
		if( drawMuzzleBeforeModel )
			R_DrawMuzzleFlashes( ent );
		R_DrawStudioGpuBatches( cache, ent );

		if( normalRenderMode )
			R_RestoreOpaqueEntityBaseline( );
		else
			R_RestoreEntityRenderMode( &renderState );
		if( processRendererMuzzleEvents && !drawMuzzleBeforeModel )
			R_DrawMuzzleFlashes( ent );
		return;
	}

	model_drawdata_t temporaryDrawData;
	StudioSkinModelInternal( mod, hdr, sequence, frame, temporaryDrawData,
		gaitSeq, gaitFrame, gaitYaw,
		ent->curstate.body, ent->curstate.skin, ent, renderTime,
		effectiveStudioLight, studioLightDir, studioDirectLightScale,
		currentWorldMatrix, forcedPoseBoneMatrices );

	if( processRendererMuzzleEvents && !muzzleEventsProcessedBeforeDraw &&
		dx_muzzleflash_sprite.value != 0.0f )
	{
		BoneMat34 muzzleBones[MAXSTUDIOBONES];
		StudioCalcBoneTransformsInternal( mod, hdr, sequence, frame,
			muzzleBones, gaitSeq, gaitFrame, gaitYaw, ent, renderTime );
		R_StudioProcessMuzzleEvents( hdr, ent, sequence,
			frame, muzzleBones, muzzleWorld, forceOverlayDepth, renderTime );
	}

	const bool normalRenderMode = !ent || ent->curstate.rendermode == kRenderNormal;
	dx_entity_state_t renderState{};
	if( normalRenderMode )
		R_SetOpaqueEntityBaseline( );
	else
		R_BeginEntityRenderMode( ent, &renderState );

	const bool drawMuzzleBeforeModel = processRendererMuzzleEvents &&
		forceOverlayDepth && R_MuzzleBeforeViewmodel( );
	if( drawMuzzleBeforeModel )
		R_DrawMuzzleFlashes( ent );
	R_DrawStudioBatches( temporaryDrawData, ent );

	if( R_StudioGlowShellEnabled( ent ) )
	{
		if( !cache.gpuSkinningPoseValid )
		{
			const int boneCount = std::max( 0,
				std::min( MAXSTUDIOBONES, hdr->numbones ) );
			StudioCalcBoneTransformsInternal( mod, hdr, sequence, frame,
				cache.gpuSkinningBoneMatrices, gaitSeq, gaitFrame, gaitYaw,
				ent, renderTime );
			cache.gpuSkinningBoneCount = boneCount;
			cache.gpuSkinningPoseValid = boneCount > 0;
		}
		R_DrawStudioGlowShellCPU( cache, ent );
	}

	if( normalRenderMode )
		R_RestoreOpaqueEntityBaseline( );
	else
		R_RestoreEntityRenderMode( &renderState );
	if( processRendererMuzzleEvents && !drawMuzzleBeforeModel )
		R_DrawMuzzleFlashes( ent );
}

static model_t *R_ModelFromClientIndex( int modelIndex )
{
	if( !gp_cl || modelIndex <= 0 || modelIndex > gp_cl->nummodels || modelIndex >= MAX_MODELS )
		return nullptr;
	return gp_cl->models[modelIndex];
}

bool R_FixedStringEquals( const char *field, size_t fieldSize, const char *text )
{
	if( !field || !text )
		return false;
	const size_t textLen = strlen( text );
	if( textLen >= fieldSize )
		return false;
	return memcmp( field, text, textLen ) == 0 && field[textLen] == '\0';
}

static int R_StudioFindBoneByName( studiohdr_t *hdr, const char *name )
{
	if( !hdr || !name || !name[0] || hdr->boneindex <= 0 )
		return -1;

	mstudiobone_t *bones = reinterpret_cast<mstudiobone_t *>(
		reinterpret_cast<byte *>( hdr ) + hdr->boneindex );
	const int count = std::min( MAXSTUDIOBONES, hdr->numbones );
	for( int i = 0; i < count; ++i )
	{
		if( R_FixedStringEquals( bones[i].name, sizeof( bones[i].name ), name ) )
			return i;
	}
	return -1;
}

static void R_StudioBuildBones( model_t *mod, studiohdr_t *hdr,
	int sequence, float frame, const cl_entity_s *ent, float renderTime,
	BoneMat34 *localMats )
{
	if( !hdr || !localMats || hdr->numbones <= 0 )
		return;

	if( sequence < 0 || sequence >= hdr->numseq )
		sequence = 0;

	float adj[MAXSTUDIOCONTROLLERS];
	StudioCalcBoneAdj( hdr, ent, renderTime, adj );

	float pos[MAXSTUDIOBONES][3];
	float rotq[MAXSTUDIOBONES][4];
	const float blend0 = StudioGetEntityBlend( ent, renderTime, 0 );
	const float blend1 = StudioGetEntityBlend( ent, renderTime, 1 );
	StudioCalcSequencePose( hdr, mod, sequence, frame, adj, blend0, blend1, pos, rotq );

	const int count = std::min( MAXSTUDIOBONES, hdr->numbones );
	for( int i = 0; i < count; ++i )
		QuaternionMatrix( rotq[i], pos[i], localMats[i] );
}

static void R_StudioMergeWeaponBones( studiohdr_t *playerHdr,
	const BoneMat34 *playerBones, model_t *weaponModel, studiohdr_t *weaponHdr,
	const cl_entity_s *weaponEnt, float renderTime, BoneMat34 *weaponBones )
{
	if( !playerHdr || !playerBones || !weaponModel || !weaponHdr ||
		!weaponBones || playerHdr->numbones <= 0 || weaponHdr->numbones <= 0 )
	{
		return;
	}

	BoneMat34 localMats[MAXSTUDIOBONES];
	R_StudioBuildBones( weaponModel, weaponHdr, 0, 0.0f,
		weaponEnt, renderTime, localMats );

	mstudiobone_t *weaponBone = reinterpret_cast<mstudiobone_t *>(
		reinterpret_cast<byte *>( weaponHdr ) + weaponHdr->boneindex );
	const int weaponCount = std::min( MAXSTUDIOBONES, weaponHdr->numbones );

	for( int i = 0; i < weaponCount; ++i )
	{
		const int parentMatch = R_StudioFindBoneByName( playerHdr, weaponBone[i].name );
		if( parentMatch >= 0 )
		{
			memcpy( weaponBones[i], playerBones[parentMatch], sizeof( BoneMat34 ) );
			continue;
		}

		const int parent = weaponBone[i].parent;
		if( parent >= 0 && parent < i )
			ConcatTransforms( weaponBones[parent], localMats[i], weaponBones[i] );
		else
			memcpy( weaponBones[i], localMats[i], sizeof( BoneMat34 ) );
	}
}

static void R_DrawPlayerWeapon( model_t *playerModel,
	studiohdr_t *playerHdr, cl_entity_s *playerEnt,
	int playerSequence, float playerFrame, int gaitSeq, float gaitFrame,
	float gaitYaw, float renderTime, D3DCOLOR studioLight,
	const float studioLightDir[3], float studioDirectLightScale,
	const D3DMATRIX &entityWorld )
{
	if( dx_player_weapon_models.value == 0.0f || !playerEnt || !playerEnt->player || !gp_cl )
		return;
	if( playerEnt->index <= 0 || playerEnt->index > MAX_CLIENTS )
		return;

	const int weaponModelIndex = playerEnt->curstate.weaponmodel;
	model_t *weaponModel = R_ModelFromClientIndex( weaponModelIndex );
	if( !weaponModel || weaponModel->type != mod_studio || weaponModel == playerModel )
		return;

	studiohdr_t *weaponHdr = static_cast<studiohdr_t *>(
		gEngfuncs.Mod_Extradata( mod_studio, weaponModel ) );
	if( !weaponHdr )
		return;

	if( !R_FindStudioModelDrawData( weaponModel ) )
		R_LoadStudioModel( weaponModel );

	BoneMat34 playerBones[MAXSTUDIOBONES];
	StudioCalcBoneTransformsInternal( playerModel, playerHdr, playerSequence,
		playerFrame, playerBones, gaitSeq, gaitFrame, gaitYaw,
		playerEnt, renderTime );

	BoneMat34 weaponBones[MAXSTUDIOBONES];

	StudioCalcBoneTransformsInternal( weaponModel, weaponHdr, 0, 0.0f,
		weaponBones, -1, 0.0f, 0.0f, playerEnt, renderTime );

	R_StudioMergeWeaponBones( playerHdr, playerBones, weaponModel,
		weaponHdr, playerEnt, renderTime, weaponBones );

	cl_entity_s &weaponEnt = g_playerWeaponDrawEntities[playerEnt->index];
	weaponEnt = *playerEnt;
	weaponEnt.model = weaponModel;
	weaponEnt.player = false;
	weaponEnt.curstate.modelindex = weaponModelIndex;
	weaponEnt.curstate.body = 0;
	weaponEnt.curstate.skin = 0;
	weaponEnt.curstate.sequence = 0;
	weaponEnt.curstate.frame = 0;
	weaponEnt.curstate.gaitsequence = 0;
	weaponEnt.curstate.effects &= ~EF_NODRAW;

	R_StudioStoreAttachments( playerEnt, weaponHdr,
		weaponBones, entityWorld, renderTime );

	R_DrawStudioEntityModel( weaponModel, &weaponEnt, 0, 0.0f,
		-1, 0.0f, 0.0f, renderTime, studioLight, studioLightDir,
		studioDirectLightScale, false, &entityWorld, weaponBones, false );
}

void R_DrawStudioEntity( cl_entity_s *ent )
{
	if( !device || !ent || !ent->model || ent->model->type != mod_studio )
		return;

	model_t *mod = ent->model;

	if( !R_FindStudioModelDrawData( mod ) )
		R_LoadStudioModel( mod );

	studiohdr_t *hdr = static_cast<studiohdr_t *>(
		gEngfuncs.Mod_Extradata( mod_studio, mod ) );
	if( !hdr )
		return;

	R_ApplyPlayerAimBlend( ent, hdr );

	const bool smoothGpuSkinPose = R_StudioCanSmoothGpuPose( hdr );
	const float studioPoseTime = smoothGpuSkinPose
		? gp_cl->time
		: R_StudioQuantizePoseTime( ent, gp_cl->time, false );

	const player_gait_pose_t gait =
		R_PreparePlayerGait( ent, hdr, studioPoseTime );

	D3DMATRIX entityWorld;
	if( gait.active )
		R_SetPlayerGaitWorldTransform( ent, gait.worldYaw, &entityWorld );
	else
		R_SetStudioWorld( ent, gp_cl->time, &entityWorld );
	device->SetTransform( D3DTS_WORLD, &entityWorld );

	const float renderFrame = StudioFrameAdvance( hdr, ent, studioPoseTime );
	const int gaitSeq = gait.active ? gait.sequence : -1;
	const float gaitFrame = gait.active ? gait.frame256 : 0.0f;
	const float gaitBoneYaw = 0.0f;

	vec3_t lightOrigin = { entityWorld._41, entityWorld._42, entityWorld._43 };
	vec3_t studioLightDir;
	float studioDirectLightScale;
	const D3DCOLOR studioLight = R_StudioEntityLight(
		ent, lightOrigin, studioLightDir, &studioDirectLightScale );

	R_DrawStudioEntityModel( mod, ent, ent->curstate.sequence,
		renderFrame, gaitSeq, gaitFrame, gaitBoneYaw, studioPoseTime,
		studioLight, studioLightDir, studioDirectLightScale, false, &entityWorld );
	R_DrawPlayerWeapon( mod, hdr, ent, ent->curstate.sequence,
		renderFrame, gaitSeq, gaitFrame, gaitBoneYaw, studioPoseTime,
		studioLight, studioLightDir, studioDirectLightScale, entityWorld );

}

void R_DrawStudioViewModel( cl_entity_s *ent,
	const vec3_t viewOrigin, const vec3_t viewAngles,
	const vec3_t viewForward, const vec3_t viewRight, const vec3_t viewUp,
	float forwardOffset, float rightOffset, float upOffset )
{
	R_RegisterModelCvars( );
	if( !ent || !ent->model || ent->model->type != mod_studio ||
		!viewOrigin || !viewAngles || !viewForward || !viewRight || !viewUp )
	{
		return;
	}

	D3DMATRIX viewModelWorld;

	vec3_t modelOrigin;
	vec3_t modelAngles;

	const float clientDelta[3] =
	{
		ent->origin[0] - viewOrigin[0],
		ent->origin[1] - viewOrigin[1],
		ent->origin[2] - viewOrigin[2]
	};
	const float clientDeltaSq =
		clientDelta[0] * clientDelta[0] +
		clientDelta[1] * clientDelta[1] +
		clientDelta[2] * clientDelta[2];

	const bool useClientViewModelTransform =
		clientDeltaSq < ( 256.0f * 256.0f ) &&
		fabsf( ent->origin[0] ) < 1048576.0f &&
		fabsf( ent->origin[1] ) < 1048576.0f &&
		fabsf( ent->origin[2] ) < 1048576.0f;

	if( useClientViewModelTransform )
	{
		VectorCopy( ent->origin, modelOrigin );

		modelAngles[PITCH] = -ent->angles[PITCH];
		modelAngles[YAW] = ent->angles[YAW];
		modelAngles[ROLL] = ent->angles[ROLL];
	}
	else
	{
		VectorCopy( viewOrigin, modelOrigin );
		VectorCopy( viewAngles, modelAngles );
	}

	for( int axis = 0; axis < 3; ++axis )
	{
		modelOrigin[axis] +=
			viewForward[axis] * forwardOffset +
			viewRight[axis] * rightOffset +
			viewUp[axis] * upOffset;
	}

	BoneMat34 cameraTransform;
	GoldSrcEntityAngleMatrix( modelAngles, modelOrigin, cameraTransform );
	R_BoneMatToWorld( cameraTransform, &viewModelWorld );
	R_ApplyEntityScale( ent, &viewModelWorld );
	device->SetTransform( D3DTS_WORLD, &viewModelWorld );

	studiohdr_t *hdr = static_cast<studiohdr_t *>(
		gEngfuncs.Mod_Extradata( mod_studio, ent->model ) );
	const float studioPoseTime = R_StudioQuantizePoseTime( ent, gp_cl->time, true );
	const float renderFrame = StudioFrameAdvance( hdr, ent, studioPoseTime );

	vec3_t studioLightDir;
	float studioDirectLightScale;
	const D3DCOLOR studioLight = R_StudioViewLightColorAtPoint(
		ent, viewOrigin, studioLightDir, &studioDirectLightScale );
	R_DrawStudioEntityModel( ent->model, ent, ent->curstate.sequence,
		renderFrame, -1, 0.0f, 0.0f, studioPoseTime,
		studioLight, studioLightDir, studioDirectLightScale, true, &viewModelWorld );

	D3DMATRIX identity;
	DX_MatrixIdentity( identity );
	device->SetTransform( D3DTS_WORLD, &identity );
}
