// hla3d_guess.cpp -- the engine's glue to the Aureal A3D renderer: hardware voices for the
// sound channels, the listener blip and the A3D 2.0 geometry (wavetracing) of the BSP world
//
// No string names the file or its functions: the hw names that other files call come from
// the sound code (snd_win.c, SND_DMA.C, SND_MIX.C), the A3dWorld and A3dLeaf classes from
// their RTTI, everything else is a _guess.

#include <math.h>

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "host.h"
#include "net.h"
#include "delta.h"
#include "server.h"
#include "client.h"
#include "gl_model.h"
#include "GLQUAKE.H"
#include "sound.h"
#include "winquake.h"
}

#include <ia3dapi.h>

extern "C"
{
// A3D renderer (linked from its library)
int		hwRendererInit_guess( HWND hwnd, int first, int one, int speed, int bits, int one2 );
int		hwRendererShutdown_guess( void );
int		hwGetA3d_guess( IA3d3 **ppA3d );
int		hwSetRendererFlags_guess( int flags );
int		hwSetDistanceScale_guess( float scale );
int		hwSetGlobalScale_guess( float scale );
float	hwGetUnitsPerMeter_guess( void );
int		hwIsInitialized_guess( void );
int		hwSetCooperativeLevel_guess( HWND hwnd, int level );
int		hwSetOutputMode_guess( int mode );
int		hwAllocRenderer_guess( void );
int		hwFreeRenderer_guess( void );
int		hwSetListenerScale_guess( float scale );
void	*hwCreateVoice_guess( void *pData, int size, int one, short channels, int speed, int bits, int one2 );
int		hwDestroyVoice_guess( void *voice );
int		hwPlayVoice_guess( void *voice, int mode );
int		hwStopVoice_guess( void *voice );
int		hwResetVoice_guess( void *voice );
int		hwSetVoicePosition_guess( void *voice, vec_t *origin );
int		A3DSource_SetGain_guess( void *voice, float gain );
int		hwSetVoiceDoppler_guess( void *voice, float doppler );
int		hwSetVoiceDistance_guess( void *voice, float dist );
int		hwSetVoiceRolloff_guess( void *voice, float rolloff );
int		hwSetVoiceReflections_guess( void *voice, int reflect );
int		hwSetVoiceOcclusions_guess( void *voice, int occlude );
int		hwSetVoiceMinMaxDistance_guess( void *voice, float mindist, float maxdist );
int		hwIsVoicePlaying_guess( void *voice );
int		hwOpenStream_guess( void );
int		hwCloseStream_guess( void );
int		hwStartStream_guess( void );
int		hwStopStream_guess( void );
int		hwSet2DVolume_guess( float vol );
int		hwLockVoice_guess( void *voice, int offset, int size, void **ppData1, DWORD *pSize1, void **ppData2, DWORD *pSize2 );
int		hwUnlockVoice_guess( void *voice, void *pData1, DWORD size1, void *pData2, DWORD size2 );
int		A3DSource_SetStreamPos_guess( void *voice, int pos );

float	hwVolumeToGain_guess( int left, int right );
void	*hwGetVoice_guess( int slot );
void	hwSetSlotChannel_guess( int slot, int ch_idx );
int		hwFindChannelSlot_guess( int ch_idx );
void	*hwGetStaticVoice_guess( int idx );
int		hwGetChannelPos_guess( int ch_idx );
void	hwSetChannelPos_guess( int ch_idx, int pos );
void	hwStopSlot_guess( int slot );
void	hwKillChannel_guess( int ch_idx );
void	hwSetMaxBufferSize_guess( int size );
int		hwGetMaxBufferSize_guess( void );
int		hwGetMaxBufferSamples_guess( void );
int		hwGetBufferSamples_guess( void );
int		hwGetNumStaticChannels_guess( void );
int		hwGetMaxChannels_guess( void );
int		hwInit_guess( HWND hwnd, int numvoices, int speed, int bits );
int		hwShutdown_guess( void );
int		hwAllocVoices_guess( HWND hwnd );
int		hwFreeVoices_guess( void );
int		hwFreeChannels_guess( void );
void	hwBlockSound_guess( void );
void	hwStartSound_guess( void );
int		hwGetBufferSize_guess( void );
void	hwSetBufferSize_guess( int size );
void	hwSetAutoDistance_guess( float mindist, float maxdist );
void	hwSetMinMaxDistance_guess( float mindist, float maxdist );
void	hwSetDistanceModel_guess( float doppler, int distance, float rolloff );
void	hwSetBlipDir_guess( int dir );
void	hwUpdateBlip_guess( void );
void	hwRestart_guess( void );
void	hwSuspendChannel_guess( int ch_idx );
void	hwInitGeometry_guess( IA3d3 *pA3d );
void	hwShutdownGeometry_guess( void );
void	hwRenderGeometry_guess( void );
int		hwCompareLeafDist_guess( const void *a, const void *b );
void	hwRenderWorldGeometry_guess( model_t *world );
class A3dLeaf *hwGetViewA3dLeaf_guess( model_t *world );
void	hwRenderLeafPolys_guess( mleaf_t *leaf, model_t *world );
int		hwPolyTooSmall_guess( float *v0, float *v1, float *v2 );
int		hwPolyColinear_guess( float *v0, float *v1, float *v2 );
void	hwCallLeafLists_guess( model_t *world );
void	hwSetLeafNum_guess( int leafnum_guess );
void	hwSetNumPolys_guess( int numpolys );
void	hwSetPolySize_guess( float polysize );
void	hwSetPolyKeep_guess( float polykeep );
void	hwSetReflectionDelay_guess( int delay );
void	hwSetReflectionGain_guess( float gain );
void	hwSetShowTossed_guess( int showtossed );
void	hwSetUsePVS_guess( int usepvs );
void	hwSetBloat_guess( float bloat );
void	hwSetOcclusions_guess( int occlude );
void	hwSetReflections_guess( int reflect );
void	hwSetOcclusionFactor_guess( float factor );
void	hwSetOcclusionUpdate_guess( int update );
void	hwSetOcclusionEpsilon_guess( float epsilon );
}

class A3dWorld;
class A3dLeaf;

extern "C"
{
qboolean	a3d_forceupdate_guess = 1;
int			g_nMaxBufferSize_guess = 0x8000;
int			g_bFirstInit_guess = 1;
int			g_nBufferSize_guess = 0x8000;
float		g_flRolloff_guess = 1.0;
int			g_nDistanceModel_guess = 18;
float		g_flMinDistance_guess = 8.0;
float		g_flMaxDistance_guess = 1000.0;
float		g_flAutoMinDistance_guess = 2.0;
float		g_flAutoMaxDistance_guess = 30.0;
int			g_bUsePVS_guess = 1;
float		g_flBloat_guess = 1.0001;
float		g_flUnused_guess = 40000.0;
int			g_bOcclusions_guess = 1;
int			g_bReflections_guess = 1;
int			g_bOcclusionUpdate_guess = 1;
float		g_flPolySize_guess = 10000000.0;
float		g_flPolyKeep_guess = 1000000000.0;
int			g_nNumPolys_guess = 200;
float		g_flReflectionDelay_guess = 1.0;
float		g_flReflectionGain_guess = 1.0;
float		g_flPolyScale_guess = 0.1;
float		g_flPolyAspect_guess = 0.00765;

int			*g_pChannelStates_guess;
int			*g_pSlotChannels_guess;
int			*g_pChannelPos_guess;
void			**g_pVoices_guess;
int			g_nVoices_guess;
int			g_nMaxVoices_guess;
float		g_flDoppler_guess;
int			g_nBlipDir_guess;
int			g_bShowTossed_guess;
model_t		*g_pWorldModel_guess;
mleaf_t		*g_pLastViewLeaf_guess;
A3dWorld	*g_pA3dWorld_guess;
IA3dList		*g_pViewList_guess;
vec3_t		g_vecListener_guess;
IA3dMaterial	*g_pMaterial_guess;
IA3d3		*g_pA3d_guess;
int			g_bPolyKeep_guess;
IA3dGeom		*g_pGeom_guess;
A3dWorld	*g_pCurrentWorld_guess;
A3dLeaf		*g_pCurrentLeaf_guess;
int			g_bPrecomputed_guess;
unsigned int	g_nPolysRendered_guess;
}

// list of pointers or leaf numbers
class A3dListNode_guess
{
public:
	A3dListNode_guess( A3dListNode_guess *prev, void *data )
	{
		m_pPrev = prev;
		m_pNext = NULL;
		m_pData = data;
	}

	~A3dListNode_guess()
	{
	}

	A3dListNode_guess *Append( void *data )
	{
		if ( !m_pNext )
		{
			m_pNext = new A3dListNode_guess( this, data );
			return m_pNext;
		}
		A3dListNode_guess *last = Last();
		return last->Append( data );
	}

	A3dListNode_guess *Last( void )
	{
		A3dListNode_guess *node = this;
		while ( node->m_pNext )
			node = node->m_pNext;
		return node;
	}

	A3dListNode_guess	*m_pPrev;
	A3dListNode_guess	*m_pNext;
	void				*m_pData;
};

class A3dList_guess
{
public:
	A3dList_guess()
	{
		m_pHead = NULL;
		m_pTail = NULL;
		m_pCurrent = NULL;
		m_nCount = 0;
	}

	~A3dList_guess()
	{
		Clear();
	}

	void Add( void *data )
	{
		if ( !m_nCount )
		{
			m_pCurrent = m_pHead = new A3dListNode_guess( NULL, data );
		}
		else
		{
			m_pTail = m_pHead->Last();
			m_pTail = m_pTail->Append( data );
			m_pCurrent = m_pTail;
		}
		m_nCount++;
	}

	int Count( void )
	{
		return m_nCount;
	}

	void Clear( void )
	{
		A3dListNode_guess *next_guess;
		A3dListNode_guess *node_guess = m_pHead;
		for ( int c_guess = 0; c_guess < m_nCount; c_guess++ )
		{
			next_guess = node_guess->m_pNext;
			delete node_guess;
			node_guess = next_guess;
		}
		m_nCount = 0;
		m_pCurrent = NULL;
		m_pHead = NULL;
		m_pTail = NULL;
	}

	void *Next( void )
	{
		if ( m_pCurrent )
		{
			void *data = m_pCurrent->m_pData;
			m_pCurrent = m_pCurrent->m_pNext;
			return data;
		}
		return NULL;
	}

	void Reset( void )
	{
		m_pCurrent = m_pHead;
	}

	A3dListNode_guess	*m_pHead;
	A3dListNode_guess	*m_pTail;
	A3dListNode_guess	*m_pCurrent;
	int					m_nCount;
};

// a triangle kept for the second list of a leaf
struct A3dPoly_guess
{
	A3dPoly_guess( int tag, float *v0, float *v1, float *v2 )
	{
		m_nTag = tag;
		m_pVert[0] = v0;
		m_pVert[1] = v1;
		m_pVert[2] = v2;
	}

	int		m_nTag;
	float	*m_pVert[3];
};

class A3dLeaf
{
public:
	A3dLeaf( mleaf_t *leaf )
	{
		g_pCurrentLeaf_guess = this;
		m_Origin[0] = ( leaf->minmaxs[0] + leaf->minmaxs[3] ) / 2;
		m_Origin[1] = ( leaf->minmaxs[1] + leaf->minmaxs[4] ) / 2;
		m_Origin[2] = ( leaf->minmaxs[2] + leaf->minmaxs[5] ) / 2;
		m_nVisLeafs = 0;
		m_pNearLeafs = new A3dList_guess;
		m_pFarLeafs = new A3dList_guess;
		m_pPolys = new A3dList_guess;
		m_pPolyList = NULL;
		m_pKeptList = NULL;
	}

	virtual ~A3dLeaf()
	{
		if ( m_nVisLeafs > 0 )
			delete m_pVisLeafs;
		if ( m_pNearLeafs )
			delete m_pNearLeafs;
		if ( m_pFarLeafs )
			delete m_pFarLeafs;
		if ( m_pPolys )
			delete m_pPolys;
		if ( m_pPolyList )
			m_pPolyList->Release();
		if ( m_pKeptList )
			m_pKeptList->Release();
	}

	void BuildVisLeafs( void )
	{
		int nNear = m_pNearLeafs->Count();
		m_nVisLeafs = nNear + m_pFarLeafs->Count();
		m_pVisLeafs = new unsigned short[m_nVisLeafs];

		int i = m_pNearLeafs->Count();
		m_pNearLeafs->Reset();
		while ( i-- > 0 )
		{
			unsigned short nearleaf = (unsigned short)m_pNearLeafs->Next();
			m_pVisLeafs[i] = nearleaf;
		}

		int j = m_pFarLeafs->Count();
		m_pFarLeafs->Reset();
		while ( j-- > 0 )
		{
			unsigned short farleaf = (unsigned short)m_pFarLeafs->Next();
			m_pVisLeafs[j + nNear] = farleaf;
		}

		g_pCurrentLeaf_guess = this;
		qsort( m_pVisLeafs, m_nVisLeafs, sizeof( unsigned short ), hwCompareLeafDist_guess );
		m_pNearLeafs->Clear();
		m_pFarLeafs->Clear();
	}

	unsigned short	m_nVisLeafs;
	unsigned short	*m_pVisLeafs;
	short			m_Origin[3];
	IA3dList		*m_pPolyList;
	IA3dList		*m_pKeptList;
	A3dList_guess	*m_pNearLeafs;
	A3dList_guess	*m_pFarLeafs;
	A3dList_guess	*m_pPolys;
};

class A3dWorld
{
public:
	A3dWorld( IA3dGeom *pGeom, model_t *world, int usepvs, int build, float polysize, float aspect, float polykeep, float scale )
	{
		IA3d3 *pA3d_guess;
		int i_guess;
		float flScale_guess;
		A3dLeaf *a3dleafs_guess;
		mleaf_t *thisLeaf_guess;
		HRESULT res_guess;
		int counts_guess;
		IA3dList *myList_guess;

		g_pCurrentWorld_guess = this;
		m_bUsePVS = usepvs;
		m_bBuild = build;
		m_flAspect = aspect;
		m_flPolySize = polysize;
		m_flMinSize = polysize * scale;
		m_flPolyKeep = polykeep;
		m_nLeafs = world->numleafs;
		m_pLeafs = (A3dLeaf **)calloc( 4, m_nLeafs );

		pGeom->QueryInterface( IID_IA3d3, (void **)&pA3d_guess );
		if ( build )
		{
			pGeom->LoadIdentity();
			pGeom->Rotate3f( 90.0, 1.0, 0, 0 );
			flScale_guess = hwGetUnitsPerMeter_guess();
			flScale_guess = 1 / flScale_guess;
			pGeom->Scale3f( flScale_guess, flScale_guess, flScale_guess );
		}

		for ( i_guess = 0; i_guess < m_nLeafs; i_guess++ )
		{
			thisLeaf_guess = &world->leafs[i_guess];
			a3dleafs_guess = m_pLeafs[i_guess] = new A3dLeaf( thisLeaf_guess );
			if ( build )
			{
				counts_guess = 0;
				res_guess = pGeom->NewList( &myList_guess );
				res_guess = myList_guess->Begin();
				counts_guess = BuildLeafPolys( pGeom, pA3d_guess, thisLeaf_guess, world, a3dleafs_guess );
				res_guess = myList_guess->End();
				if ( counts_guess > 0 )
					a3dleafs_guess->m_pPolyList = myList_guess;
				else
					res_guess = myList_guess->Release();

				res_guess = pGeom->NewList( &myList_guess );
				res_guess = myList_guess->Begin();
				counts_guess = RenderPolys( pGeom, a3dleafs_guess );
				res_guess = myList_guess->End();
				if ( counts_guess > 0 )
					a3dleafs_guess->m_pKeptList = myList_guess;
				else
					res_guess = myList_guess->Release();
			}
		}

		for ( i_guess = 0; i_guess < m_nLeafs; i_guess++ )
			ComputeVis( i_guess, world );

		pA3d_guess->Release();
	}

	virtual ~A3dWorld()
	{
		for ( int i = 0; i < m_nLeafs; i++ )
		{
			delete m_pLeafs[i];
		}
		if ( m_pLeafs )
			Mem_Free( m_pLeafs );
	}

	void ComputeVis( int leafnum, model_t *world )
	{
		float maxdist;
		short deltaZ;
		A3dLeaf *a3d;
		int visible;
		A3dLeaf *test;
		int k_guess;
		short orgX;
		byte *bits;
		mleaf_t *leaf_guess;
		short orgY;
		short orgZ;
		short deltaX;
		short deltaY;
		float hx;
		float dd_guess;
		float ez_guess;
		float hy;

		a3d = m_pLeafs[leafnum];
		leaf_guess = &world->leafs[leafnum];
		orgX = a3d->m_Origin[0];
		orgY = a3d->m_Origin[1];
		orgZ = a3d->m_Origin[2];
		if ( m_bUsePVS )
			bits = Mod_LeafPVS( leaf_guess, world );
		visible = 1;
		hx = leaf_guess->minmaxs[0] - leaf_guess->minmaxs[3];
		hy = leaf_guess->minmaxs[1] - leaf_guess->minmaxs[4];
		ez_guess = leaf_guess->minmaxs[2] - leaf_guess->minmaxs[5];
		maxdist += (float)( sqrt( maxdist = hx * hx + hy * hy + ez_guess * ez_guess ) * 400.0 );
		for ( k_guess = 0; k_guess < m_nLeafs; k_guess++ )
		{
			if ( k_guess == leafnum )
				continue;
			if ( m_bUsePVS )
				visible = ( bits[k_guess >> 3] & ( 1 << ( k_guess & 7 ) ) ) != 0;
			if ( visible )
			{
				test = m_pLeafs[k_guess];
				deltaX = test->m_Origin[0] - orgX;
				deltaY = test->m_Origin[1] - orgY;
				deltaZ = test->m_Origin[2] - orgZ;
				dd_guess = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
				if ( dd_guess < 50000 )
					a3d->m_pNearLeafs->Add( (void *)k_guess );
				else if ( dd_guess < 100000 + maxdist )
					a3d->m_pFarLeafs->Add( (void *)k_guess );
			}
		}
		a3d->BuildVisLeafs();
	}


	int RenderPolys( IA3dGeom *pGeom, A3dLeaf *a3dleaf )
	{
		int counts_guess = 0;
		pGeom->Begin( A3D_TRIANGLES );
		int i = a3dleaf->m_pPolys->Count();
		a3dleaf->m_pPolys->Reset();
		while ( i-- > 0 )
		{
			A3dPoly_guess *poly = (A3dPoly_guess *)a3dleaf->m_pPolys->Next();
			pGeom->Tag( poly->m_nTag );
			pGeom->Vertex3fv( poly->m_pVert[0] );
			pGeom->Vertex3fv( poly->m_pVert[1] );
			pGeom->Vertex3fv( poly->m_pVert[2] );
			delete poly;
			counts_guess++;
		}
		pGeom->End();
		a3dleaf->m_pPolys->Clear();
		return counts_guess;
	}

	int BuildLeafPolys( IA3dGeom *pGeom, IA3d3 *pA3d, mleaf_t *leaf, model_t *world, A3dLeaf *a3dleaf )
	{
		int count = 0;
		for ( int s_guess = 0; s_guess < leaf->nummarksurfaces; s_guess++ )
		{
			msurface_t *fs;
			float *pPos;
			int nCorners;
			int tri;
			int v2;
			int v1;
			int edgenum;
			int v3;
			int v0;

			fs = leaf->firstmarksurface[s_guess];
			edgenum = world->surfedges[fs->firstedge];
			if ( edgenum > 0 )
			{
				v0 = world->edges[edgenum].v[0];
				v1 = world->edges[edgenum].v[1];
			}
			else
			{
				v0 = world->edges[edgenum * -1].v[1];
				v1 = world->edges[edgenum * -1].v[0];
			}
			pGeom->Begin( A3D_TRIANGLES );
			nCorners = fs->numedges - 1;
			for ( tri = 1; tri < nCorners; tri++ )
			{
				edgenum = world->surfedges[fs->firstedge + tri];
				if ( edgenum > 0 )
					v2 = world->edges[edgenum].v[1];
				else
					v2 = world->edges[edgenum * -1].v[0];

				if ( tri + 1 < nCorners )
				{
					edgenum = world->surfedges[fs->firstedge + tri + 1];
					if ( edgenum > 0 )
						v3 = world->edges[edgenum].v[1];
					else
						v3 = world->edges[edgenum * -1].v[0];
					if ( IsColinear( world->vertexes[v1].position, world->vertexes[v2].position, world->vertexes[v3].position ) )
					{
						v1 = v3;
						tri++;
						continue;
					}
				}

				int result = Classify( world->vertexes[v0].position, world->vertexes[v1].position, world->vertexes[v2].position );
				int keep_guess;
				if ( result > 0 )
				{
					if ( result == 1 )
					{
						A3dPoly_guess *poly = new A3dPoly_guess( (int)fs->plane, world->vertexes[v0].position, world->vertexes[v1].position, world->vertexes[v2].position );
						a3dleaf->m_pPolys->Add( poly );
					}
					v1 = v2;
					continue;
				}

				{
					pGeom->Tag( (DWORD)fs->plane );
					keep_guess = ( result == -1 );
					pA3d->Compat( 0x3f1, keep_guess );
					pPos = world->vertexes[v0].position;
					pGeom->Vertex3f( pPos[0], pPos[1], pPos[2] );
					pPos = world->vertexes[v1].position;
					pGeom->Vertex3f( pPos[0], pPos[1], pPos[2] );
					pPos = world->vertexes[v2].position;
					pGeom->Vertex3f( pPos[0], pPos[1], pPos[2] );
					pA3d->Compat( 0x3f1, 0 );
					v1 = v2;
					count++;
				}
			}
			pGeom->End();
		}
		return count;
	}

	int Classify( float *v0, float *v1, float *v2 )
	{
		float edge1_guess[3], edge2_guess[3], normal_guess[3];
		float dot, area;
		float edge3_guess[3];

		VectorSubtract( v1, v0, edge1_guess );
		VectorSubtract( v2, v0, edge2_guess );
		normal_guess[0] = edge1_guess[1] * edge2_guess[2] - edge1_guess[2] * edge2_guess[1];
		normal_guess[1] = edge1_guess[2] * edge2_guess[0] - edge1_guess[0] * edge2_guess[2];
		normal_guess[2] = edge1_guess[0] * edge2_guess[1] - edge1_guess[1] * edge2_guess[0];
		area = DotProduct( normal_guess, normal_guess );
		if ( area < m_flPolySize )
		{
			if ( area < m_flMinSize )
				return 2;
			return 1;
		}
		if ( area > m_flPolyKeep )
			return 0;

		dot *= ( dot = DotProduct( edge1_guess, edge2_guess ) );
		if ( dot != 0 && area / dot < m_flAspect )
			return 1;

		VectorSubtract( v1, v2, edge3_guess );
		normal_guess[0] = edge3_guess[1] * edge1_guess[2] - edge3_guess[2] * edge1_guess[1];
		normal_guess[1] = edge3_guess[2] * edge1_guess[0] - edge3_guess[0] * edge1_guess[2];
		normal_guess[2] = edge3_guess[0] * edge1_guess[1] - edge3_guess[1] * edge1_guess[0];
		area = DotProduct( normal_guess, normal_guess );
		dot *= ( dot = DotProduct( edge3_guess, edge1_guess ) );
		if ( dot != 0 && area / dot < m_flAspect )
			return 1;

		normal_guess[0] = edge3_guess[1] * edge2_guess[2] - edge3_guess[2] * edge2_guess[1];
		normal_guess[1] = edge3_guess[2] * edge2_guess[0] - edge3_guess[0] * edge2_guess[2];
		normal_guess[2] = edge3_guess[0] * edge2_guess[1] - edge3_guess[1] * edge2_guess[0];
		area = DotProduct( normal_guess, normal_guess );
		dot *= ( dot = DotProduct( edge3_guess, edge2_guess ) );
		if ( dot != 0 && area / dot < m_flAspect )
			return 1;

		return 0;
	}

	int IsColinear( float *v0, float *v1, float *v2 )
	{
		float edge1_guess[3], edge2_guess[3], normal_guess[3];

		VectorSubtract( v1, v0, edge1_guess );
		VectorSubtract( v2, v0, edge2_guess );
		normal_guess[0] = edge1_guess[1] * edge2_guess[2] - edge1_guess[2] * edge2_guess[1];
		normal_guess[1] = edge1_guess[2] * edge2_guess[0] - edge1_guess[0] * edge2_guess[2];
		normal_guess[2] = edge1_guess[0] * edge2_guess[1] - edge1_guess[1] * edge2_guess[0];
		if ( DotProduct( normal_guess, normal_guess ) < 0.000001f )
			return 1;
		return 0;
	}

	int			m_nLeafs;
	A3dLeaf		**m_pLeafs;
	int			m_bUsePVS;
	int			m_bBuild;
	float		m_flAspect;
	float		m_flPolySize;
	float		m_flMinSize;
	float		m_flPolyKeep;
};

float hwVolumeToGain_guess( int left, int right )
{
	float gain = ( left + right ) / 510.0f;
	return gain;
}

void *hwGetVoice_guess( int slot )
{
	if ( slot < 0 || slot >= g_nVoices_guess )
		return NULL;
	if ( g_nBlipDir_guess && slot == g_nVoices_guess - 1 )
		return NULL;
	return g_pVoices_guess[slot];
}

void hwSetSlotChannel_guess( int slot, int ch_idx )
{
	if ( slot < 0 || slot >= g_nVoices_guess )
		return;
	if ( !g_pSlotChannels_guess || g_nVoices_guess < 1 )
		return;
	g_pSlotChannels_guess[slot] = ch_idx;
}

int hwFindChannelSlot_guess( int ch_idx )
{
	int i;

	if ( !g_pSlotChannels_guess || g_nVoices_guess < 1 )
		return -1;

	for ( i = 0; i < g_nVoices_guess; i++ )
	{
		if ( g_pSlotChannels_guess[i] == ch_idx )
			return i;
	}
	return -1;
}

void *hwAllocChannel_guess( int ch_idx, int alloc )
{
	int i;
	int freeslots_guess = -1;

	if ( !hwIsInitialized_guess() )
		return NULL;

	if ( ch_idx < 4 || ch_idx >= g_nVoices_guess + 4 )
		return NULL;

	if ( g_pSlotChannels_guess )
		g_pSlotChannels_guess[ch_idx - 4] = ch_idx;
	return g_pVoices_guess[ch_idx - 4];

	for ( i = 0; i < g_nVoices_guess; i++ )
	{
		if ( g_pSlotChannels_guess[i] == ch_idx )
			return g_pVoices_guess[i];
		if ( freeslots_guess == -1 && g_pVoices_guess[i] && g_pSlotChannels_guess[i] == -1 )
			freeslots_guess = i;
	}

	if ( freeslots_guess != -1 )
	{
		g_pSlotChannels_guess[freeslots_guess] = ch_idx;
		return g_pVoices_guess[freeslots_guess];
	}
	return NULL;
}

void *hwGetStaticVoice_guess( int idx )
{
	if ( !hwIsInitialized_guess() )
		return NULL;
	if ( idx < g_nVoices_guess || idx >= g_nMaxVoices_guess )
		return NULL;
	return g_pVoices_guess[idx];
}

int hwGetChannelPos_guess( int ch_idx )
{
	ch_idx = hwFindChannelSlot_guess( ch_idx );
	if ( ch_idx >= 0 )
		return g_pChannelPos_guess[ch_idx];
	return 0;
}

void hwSetChannelPos_guess( int ch_idx, int pos )
{
	ch_idx = hwFindChannelSlot_guess( ch_idx );
	if ( ch_idx >= 0 )
		g_pChannelPos_guess[ch_idx] = pos;
}

int hwGetChannelState_guess( int ch_idx )
{
	ch_idx = hwFindChannelSlot_guess( ch_idx );
	if ( !g_pVoices_guess[ch_idx] )
		return 0;
	return g_pChannelStates_guess[ch_idx];
}

int hwSetChannelState_guess( int ch_idx, int state )
{
	int slot = hwFindChannelSlot_guess( ch_idx );
	if ( slot < 0 )
		return -1;
	if ( !g_pVoices_guess[slot] )
		return -1;
	g_pChannelStates_guess[slot] = state;
	return 0;
}

void hwStopSlot_guess( int slot )
{
	void *voice = hwGetVoice_guess( slot );
	if ( !voice )
		return;
	hwStopVoice_guess( voice );
}

void hwSetChannelDistance_guess( int ch_idx, float dist )
{
	void *voice = hwAllocChannel_guess( ch_idx, 0 );
	if ( !voice )
		return;

	if ( dist == 0 )
	{
		hwSetVoiceReflections_guess( voice, 0 );
		hwSetVoiceOcclusions_guess( voice, 0 );
		hwSetVoiceMinMaxDistance_guess( voice, 1000000000.0, 2000000000.0 );
	}
	else
	{
		hwSetVoiceMinMaxDistance_guess( voice, g_flMinDistance_guess, g_flMaxDistance_guess );
	}
	hwSetVoiceDistance_guess( voice, dist );
}

void hwKillChannel_guess( int ch_idx )
{
	void *voice = NULL;
	int tmpUnused_guess = 0;

	voice = hwAllocChannel_guess( ch_idx, 0 );
	if ( !voice )
		return;
	hwStopVoice_guess( voice );
}

void hwSetMaxBufferSize_guess( int size )
{
	if ( size <= 0 || size == g_nMaxBufferSize_guess )
		return;

	g_nMaxBufferSize_guess = size;
	if ( hwIsInitialized_guess() )
	{
		hwFreeVoices_guess();
		hwAllocVoices_guess( NULL );
	}
}

int hwGetMaxBufferSize_guess( void )
{
	return g_nMaxBufferSize_guess;
}

int hwGetMaxBufferSamples_guess( void )
{
	return g_nMaxBufferSize_guess / ( shm->samplebits / 8 );
}

int hwGetBufferSamples_guess( void )
{
	return g_nBufferSize_guess / ( shm->samplebits / 8 );
}

int hwGetNumChannels_guess( void )
{
	return g_nVoices_guess;
}

int hwGetNumStaticChannels_guess( void )
{
	return g_nMaxVoices_guess - g_nVoices_guess;
}

int hwGetMaxChannels_guess( void )
{
	return g_nMaxVoices_guess;
}

int hwInit_guess( HWND hwnd, int numvoices, int speed, int bits )
{
	int hr;
	IA3d3 *pCurA3d_guess;
	IA3d3 *pOld_guess;

	if ( hwIsInitialized_guess() )
		return 0;

	hr = hwRendererInit_guess( hwnd, g_bFirstInit_guess, 1, speed, bits, 1 );
	hwSetOutputMode_guess( 0 );
	if ( hr < 0 )
		return hr;

	g_bFirstInit_guess = 0;
	hwSetRendererFlags_guess( 0x10000 );
	hwSetDistanceScale_guess( 18.0 );
	hwSetGlobalScale_guess( 1.0 );
	hwSet2DVolume_guess( 0.5 );

	g_nMaxVoices_guess = numvoices;
	g_pVoices_guess = (void **)Mem_Malloc( numvoices * sizeof( void * ) );
	g_pChannelPos_guess = (int *)Mem_Malloc( numvoices * sizeof( int ) );
	g_pChannelStates_guess = (int *)Mem_Malloc( numvoices * sizeof( int ) );
	memset( g_pVoices_guess, 0, numvoices * sizeof( void * ) );
	memset( g_pChannelPos_guess, 0, numvoices * sizeof( int ) );
	memset( g_pChannelStates_guess, 0, numvoices * sizeof( int ) );
	g_pSlotChannels_guess = NULL;

	hr = hwAllocVoices_guess( hwnd );
	if ( hr < 0 )
	{
		Mem_Free( g_pVoices_guess );
		g_pVoices_guess = NULL;
		return hr;
	}

	hwGetA3d_guess( &pCurA3d_guess );
	pOld_guess = pCurA3d_guess;
	hwInitGeometry_guess( pCurA3d_guess );
	hwSetDistanceModel_guess( 0, 18, 1.0 );
	hwSetMinMaxDistance_guess( 8.0, 1000.0 );
	a3d_forceupdate_guess = true;
	return 0;
}

int hwShutdown_guess( void )
{
	hwShutdownGeometry_guess();
	if ( g_pVoices_guess )
		hwFreeChannels_guess();

	g_flDoppler_guess = 0;
	g_flRolloff_guess = 1.0;
	g_nDistanceModel_guess = 18;
	g_nMaxVoices_guess = 0;
	g_nMaxBufferSize_guess = 0x8000;
	g_nBufferSize_guess = 0x8000;
	g_nBlipDir_guess = 0;
	hwRendererShutdown_guess();
	return 0;
}

int hwAllocVoices_guess( HWND hwnd )
{
	int len2_guess = g_nMaxBufferSize_guess;
	int flags_guess;
	void *mem_guess;
	int bOk_guess;
	int rc_guess;
	int j_guess;
	int c_guess;
	void *hw_guess;

	if ( !hwIsInitialized_guess() )
		return -2;

	mem_guess = Mem_Malloc( len2_guess );
	memset( mem_guess, 0, len2_guess );
	bOk_guess = 1;

	if ( hwnd )
	{
		flags_guess = 3;
		rc_guess = hwSetCooperativeLevel_guess( hwnd, flags_guess );
	}

	rc_guess = hwAllocRenderer_guess();
	if ( rc_guess < 0 )
	{
		Mem_Free( mem_guess );
		return rc_guess;
	}

	hwSetListenerScale_guess( 20.0 );
	hwOpenStream_guess();

	for ( c_guess = 0; c_guess < g_nMaxVoices_guess; c_guess++ )
	{
		if ( g_pVoices_guess[c_guess] )
			continue;

		if ( bOk_guess )
		{
			hw_guess = hwCreateVoice_guess( mem_guess, len2_guess, 1, 1, shm->dmaspeed, shm->samplebits, 1 );
			if ( hwPlayVoice_guess( hw_guess, 0 ) < 0 )
			{
				hwDestroyVoice_guess( hw_guess );
				hw_guess = NULL;
				bOk_guess = 0;
			}
			else
			{
				g_pVoices_guess[c_guess] = hw_guess;
				g_nVoices_guess++;
			}
		}
	}

	Mem_Free( mem_guess );
	mem_guess = NULL;

	if ( !g_pSlotChannels_guess )
	{
		g_pSlotChannels_guess = (int *)Mem_Malloc( g_nVoices_guess * sizeof( int ) );
		if ( !g_pSlotChannels_guess )
			return -1;
		for ( j_guess = 0; j_guess < g_nVoices_guess; j_guess++ )
			g_pSlotChannels_guess[j_guess] = -1;
	}
	return 1;
}

int hwFreeVoices_guess( void )
{
	int c_guess;
	void *hw_guess;

	if ( !hwIsInitialized_guess() )
		return -2;

	for ( c_guess = 0; c_guess < g_nMaxVoices_guess; c_guess++ )
	{
		hw_guess = g_pVoices_guess[c_guess];
		if ( hw_guess )
		{
			hwStopVoice_guess( hw_guess );
			hwDestroyVoice_guess( hw_guess );
			g_pVoices_guess[c_guess] = NULL;
		}
	}

	hwFreeRenderer_guess();
	hwCloseStream_guess();
	g_nVoices_guess = 0;
	if ( g_pSlotChannels_guess )
	{
		Mem_Free( g_pSlotChannels_guess );
		g_pSlotChannels_guess = NULL;
	}
	return 0;
}

int hwFreeChannels_guess( void )
{
	hwFreeVoices_guess();
	Mem_Free( g_pVoices_guess );
	Mem_Free( g_pChannelPos_guess );
	Mem_Free( g_pChannelStates_guess );
	g_pChannelPos_guess = NULL;
	g_pChannelStates_guess = NULL;
	g_pVoices_guess = NULL;
	g_nMaxVoices_guess = -1;
	return 0;
}

void hwStopAllSounds_guess( void )
{
	int i;

	for ( i = 0; i < g_nMaxVoices_guess; i++ )
		hwStopSlot_guess( i );
}

void hwBlockSound_guess( void )
{
	hwStopStream_guess();
}

void hwStartSound_guess( void )
{
	hwStartStream_guess();
}

int hwGetBufferSize_guess( void )
{
	return g_nBufferSize_guess;
}

void hwSetBufferSize_guess( int size )
{
	if ( size < 0 )
		return;

	if ( size > g_nMaxBufferSize_guess )
		g_nBufferSize_guess = g_nMaxBufferSize_guess;
	else
		g_nBufferSize_guess = size;
}

void hwSetAutoDistance_guess( float mindist, float maxdist )
{
	g_flAutoMinDistance_guess = mindist;
	g_flAutoMaxDistance_guess = maxdist;
}

void hwSetMinMaxDistance_guess( float mindist, float maxdist )
{
	int i_guess;
	channel_t *c_guess = channels;
	void *hw_guess;

	g_flMinDistance_guess = mindist;
	g_flMaxDistance_guess = maxdist;

	for ( i_guess = 0; i_guess < hwGetNumChannels_guess(); i_guess++ )
	{
		hw_guess = hwGetVoice_guess( i_guess );
		if ( hw_guess )
			hwSetVoiceMinMaxDistance_guess( hw_guess, g_flMinDistance_guess, g_flMaxDistance_guess );
	}
}

void hwSetDistanceModel_guess( float doppler, int distance, float rolloff )
{
	int c_guess;
	void *hw_guess;

	for ( c_guess = 0; c_guess < hwGetNumChannels_guess(); c_guess++ )
	{
		hw_guess = hwGetVoice_guess( c_guess );
		if ( !hw_guess )
			continue;
		if ( g_flDoppler_guess != doppler )
			hwSetVoiceDoppler_guess( hw_guess, doppler );
		if ( g_flRolloff_guess != rolloff )
			hwSetVoiceRolloff_guess( hw_guess, rolloff );
	}

	if ( g_nDistanceModel_guess != distance )
		hwSetDistanceScale_guess( distance );

	g_flDoppler_guess = doppler;
	g_nDistanceModel_guess = distance;
	g_flRolloff_guess = rolloff;
}

void hwSetBlipDir_guess( int dir )
{
	void *curHvoice_guess;
	void *pCurData1_guess;
	DWORD len1_guess;
	void *ptr22_guess;
	DWORD size2_guess;
	unsigned short *wave_guess;
	int c_guess;

	if ( g_nBlipDir_guess == dir )
		return;

	curHvoice_guess = g_pVoices_guess[g_nVoices_guess - 1];
	if ( !g_nBlipDir_guess )
	{
		hwResetVoice_guess( curHvoice_guess );
		if ( hwLockVoice_guess( curHvoice_guess, 0, g_nMaxBufferSize_guess, &pCurData1_guess, &len1_guess, &ptr22_guess, &size2_guess ) >= 0 )
		{
			c_guess = 0;
			wave_guess = (unsigned short *)pCurData1_guess;
			Q_memset( pCurData1_guess, 0, len1_guess );
			while ( c_guess < hwGetMaxBufferSamples_guess() )
			{
				wave_guess[c_guess] = 0xffff;
				c_guess += 11025;
			}

			if ( hwUnlockVoice_guess( curHvoice_guess, pCurData1_guess, len1_guess, ptr22_guess, size2_guess ) < 0 )
			{
				if ( hwUnlockVoice_guess( curHvoice_guess, pCurData1_guess, len1_guess, ptr22_guess, size2_guess ) < 0 )
					return;
			}
		}
		else
			return;
	}
	else if ( !dir )
	{
		hwResetVoice_guess( curHvoice_guess );
	}

	g_nBlipDir_guess = dir;
}

void hwUpdateBlip_guess( void )
{
	void *voices_guess;
	vec3_t dirs_guess;
	vec3_t spot_guess;

	switch ( g_nBlipDir_guess )
	{
	case 1:
		VectorCopy( listener_forward, dirs_guess );
		break;
	case 2:
		VectorCopy( listener_forward, dirs_guess );
		VectorInverse( dirs_guess );
		break;
	case 3:
		VectorCopy( listener_right, dirs_guess );
		break;
	case 4:
		VectorCopy( listener_right, dirs_guess );
		VectorInverse( dirs_guess );
		break;
	case 5:
		VectorCopy( listener_up, dirs_guess );
		break;
	case 6:
		VectorCopy( listener_up, dirs_guess );
		VectorInverse( dirs_guess );
		break;
	default:
		return;
	}

	VectorScale( dirs_guess, 1.1f, dirs_guess );
	VectorAdd( listener_origin, dirs_guess, spot_guess );

	voices_guess = g_pVoices_guess[g_nVoices_guess - 1];
	hwSetVoicePosition_guess( voices_guess, spot_guess );
	if ( !hwIsVoicePlaying_guess( voices_guess ) )
		hwPlayVoice_guess( voices_guess, -1 );
}

void hwRestart_guess( void )
{
	int i_guess;
	channel_t *c_guess;
	int bLoop2_guess;
	sfxcache_t *nSc_guess;
	void *hw_guess;

	c_guess = channels;
	for ( i_guess = 0; i_guess < total_channels; i_guess++, c_guess++ )
	{
		if ( c_guess->sfx )
		{
			hw_guess = hwAllocChannel_guess( i_guess, 1 );
			if ( !hw_guess )
				continue;

			hwResetVoice_guess( hw_guess );
			nSc_guess = S_LoadSound( c_guess->sfx, c_guess );
			if ( !nSc_guess )
				continue;

			bLoop2_guess = nSc_guess->loopstart >= 0;
			hwSetChannelPos_guess( i_guess, c_guess->pos );
			A3DSource_SetStreamPos_guess( hw_guess, c_guess->pos );
			hwSetChannelDistance_guess( i_guess, c_guess->dist_mult / sound_nominal_clip_dist );
			A3DSource_SetGain_guess( hw_guess, c_guess->master_vol / 255.0f );
			hwSetVoiceRelative_guess( hw_guess, cl_entities[c_guess->entnum].curstate.movetype == MOVETYPE_NONE );
			if ( bLoop2_guess )
				hwSetChannelState_guess( i_guess, 2 );
			else
				hwSetChannelState_guess( i_guess, 1 );
		}
	}
}

void hwSuspendChannel_guess( int ch_idx )
{
	int ns_guess;
	int st_guess;
	void *hw_guess;

	hw_guess = hwAllocChannel_guess( ch_idx, 1 );
	if ( !hw_guess )
		return;

	st_guess = hwGetChannelState_guess( ch_idx );
	switch ( st_guess )
	{
	case 0:
	case 1:
		ns_guess = 5;
		break;
	case 2:
	case 6:
		ns_guess = 6;
		break;
	case 3:
		ns_guess = 7;
		break;
	default:
		ns_guess = 5;
		break;
	}

	hwSetChannelState_guess( ch_idx, ns_guess );
	hwSetVoiceRelative_guess( hw_guess, cl_entities[channels[ch_idx].entnum].curstate.movetype == MOVETYPE_NONE );
}

void hwInitGeometry_guess( IA3d3 *pA3d )
{
	HRESULT hr;
	DWORD dwBloat;

	if ( !g_pGeom_guess )
	{
		g_pA3d_guess = pA3d;
		hr = g_pA3d_guess->QueryInterface( IID_IA3dGeom, (void **)&g_pGeom_guess );
		if ( g_bOcclusions_guess )
			hr = g_pGeom_guess->Enable( A3D_OCCLUSIONS );
		if ( g_bReflections_guess )
			hr = g_pGeom_guess->Enable( A3D_1ST_REFLECTIONS );
		hr = g_pGeom_guess->SetReflectionDelayScale( g_flReflectionDelay_guess );
		hr = g_pGeom_guess->SetReflectionGainScale( g_flReflectionGain_guess );
		hr = g_pGeom_guess->NewMaterial( &g_pMaterial_guess );
		hr = g_pMaterial_guess->SetTransmittance( 0.3f, 0.9f );
		g_pGeom_guess->BindMaterial( g_pMaterial_guess );
		g_pGeom_guess->LoadIdentity();
		g_pGeom_guess->Rotate3f( 90.0, 1.0, 0, 0 );
		g_pGeom_guess->BindListener();

		g_pA3d_guess->Compat( 0x3e9, 1 );
		memcpy( &dwBloat, &g_flBloat_guess, 4 );
		g_pA3d_guess->Compat( 0x3ea, dwBloat );
		g_pA3d_guess->Compat( 0x3eb, g_nNumPolys_guess );
		g_pA3d_guess->Compat( 0x3ec, 4 );
		g_pA3d_guess->Compat( 0x3ed, 2 );
		g_pA3d_guess->Compat( 0x3ef, 1 );
	}
}

void hwShutdownGeometry_guess( void )
{
	if ( g_pGeom_guess )
	{
		g_pGeom_guess->Release();
		g_pGeom_guess = NULL;
	}
	if ( g_pA3d_guess )
	{
		g_pA3d_guess->Release();
		g_pA3d_guess = NULL;
	}
	if ( g_pViewList_guess )
	{
		g_pViewList_guess->Release();
		g_pViewList_guess = NULL;
	}
	if ( g_pMaterial_guess )
	{
		g_pMaterial_guess->Release();
		g_pMaterial_guess = NULL;
	}
	if ( g_pA3dWorld_guess )
	{
		delete g_pA3dWorld_guess;
		g_pA3dWorld_guess = NULL;
	}
	g_pLastViewLeaf_guess = NULL;
	g_pWorldModel_guess = NULL;
}

void hwUpdateListener_guess( vec_t *origin )
{
	g_vecListener_guess[0] = origin[0];
	g_vecListener_guess[1] = origin[1];
	g_vecListener_guess[2] = origin[2];
}

void hwRenderGeometry_guess( void )
{
	model_t *world = cl.worldmodel;

	if ( world )
	{
		if ( g_pWorldModel_guess != world )
		{
			g_pWorldModel_guess = world;
			if ( g_pA3dWorld_guess )
				delete g_pA3dWorld_guess;
			g_pA3dWorld_guess = new A3dWorld( g_pGeom_guess, world, g_bUsePVS_guess, g_bPrecomputed_guess, g_flPolySize_guess, g_flPolyAspect_guess, g_flPolyKeep_guess, g_flPolyScale_guess );
		}

		if ( r_viewleaf )
		{
			if ( g_bPrecomputed_guess )
				hwCallLeafLists_guess( world );
			else
				hwRenderWorldGeometry_guess( world );
		}
	}
}

int hwCompareLeafDist_guess( const void *a, const void *b )
{
	float x;
	A3dLeaf *lb;
	float y;
	unsigned short *pKeyA_guess;
	unsigned short *pb_guess;
	A3dLeaf *la_guess;
	float lenA_guess;
	float offz_guess;
	float distb_guess;

	pKeyA_guess = (unsigned short *)a;
	pb_guess = (unsigned short *)b;
	la_guess = g_pCurrentWorld_guess->m_pLeafs[*pKeyA_guess];
	lb = g_pCurrentWorld_guess->m_pLeafs[*pb_guess];

	x = la_guess->m_Origin[0] - g_pCurrentLeaf_guess->m_Origin[0];
	y = la_guess->m_Origin[1] - g_pCurrentLeaf_guess->m_Origin[1];
	offz_guess = la_guess->m_Origin[2] - g_pCurrentLeaf_guess->m_Origin[2];
	lenA_guess = x * x + y * y + offz_guess * offz_guess;
	x = lb->m_Origin[0] - g_pCurrentLeaf_guess->m_Origin[0];
	y = lb->m_Origin[1] - g_pCurrentLeaf_guess->m_Origin[1];
	offz_guess = lb->m_Origin[2] - g_pCurrentLeaf_guess->m_Origin[2];
	distb_guess = x * x + y * y + offz_guess * offz_guess;

	if ( lenA_guess > distb_guess )
		return 1;
	if ( lenA_guess < distb_guess )
		return -1;
	return 0;
}


void hwRenderWorldGeometry_guess( model_t *world )
{
	A3dLeaf *a3dleaf_guess;
	int nLeafnum_guess;
	HRESULT rc_guess;
	float flInvUnits_guess;
	int c_guess;
	mleaf_t *leafs_guess;

	a3dleaf_guess = hwGetViewA3dLeaf_guess( world );
	if ( a3dleaf_guess )
	{
		if ( g_pViewList_guess )
			g_pViewList_guess->Release();
		rc_guess = g_pGeom_guess->NewList( &g_pViewList_guess );
		rc_guess = g_pGeom_guess->LoadIdentity();
		g_pGeom_guess->Rotate3f( 90.0, 1.0, 0, 0 );
		flInvUnits_guess = hwGetUnitsPerMeter_guess();
		flInvUnits_guess = 1 / flInvUnits_guess;
		g_pGeom_guess->Scale3f( flInvUnits_guess, flInvUnits_guess, flInvUnits_guess );
		g_pViewList_guess->Begin();
		g_pGeom_guess->BindMaterial( g_pMaterial_guess );
		g_nPolysRendered_guess = 0;

		g_flPolySize_guess = g_flPolySize_guess * g_flPolyScale_guess;
		hwRenderLeafPolys_guess( r_viewleaf, world );
		g_flPolySize_guess = ( 1 / g_flPolyScale_guess ) * g_flPolySize_guess;

		if ( a3dleaf_guess )
		{
			for ( c_guess = 0; c_guess < a3dleaf_guess->m_nVisLeafs; c_guess++ )
			{
				nLeafnum_guess = a3dleaf_guess->m_pVisLeafs[c_guess];
				leafs_guess = &world->leafs[nLeafnum_guess];
				if ( g_nPolysRendered_guess >= g_nNumPolys_guess )
					break;
				hwRenderLeafPolys_guess( leafs_guess, world );
			}
		}
		g_pViewList_guess->End();
	}
	else
	{
		if ( g_pViewList_guess )
			g_pViewList_guess->Call();
	}
}

A3dLeaf *hwGetViewA3dLeaf_guess( model_t *world )
{
	A3dLeaf *a3dleaf_guess;
	int leafnum_guess;
	mleaf_t *curleaf_guess;
	mleaf_t *leafss_guess;
	int ofs_guess;

	if ( g_pLastViewLeaf_guess == r_viewleaf )
		return NULL;

	g_pLastViewLeaf_guess = r_viewleaf;
	curleaf_guess = r_viewleaf;
	leafss_guess = world->leafs;
	ofs_guess = (int)curleaf_guess - (int)leafss_guess;
	leafnum_guess = ofs_guess / sizeof( mleaf_t );
	if ( leafnum_guess < 0 || leafnum_guess >= g_pA3dWorld_guess->m_nLeafs )
		return NULL;

	a3dleaf_guess = g_pA3dWorld_guess->m_pLeafs[leafnum_guess];
	return a3dleaf_guess;
}

void hwRenderLeafPolys_guess( mleaf_t *leaf, model_t *world )
{
	msurface_t *fs;
	float *pPos;
	int nCorners;
	int tri;
	int v2;
	int v1;
	int edgenum;
	int s_guess;
	int v3;
	int v0;

	for ( s_guess = 0; s_guess < leaf->nummarksurfaces; s_guess++ )
	{
		fs = leaf->firstmarksurface[s_guess];
		edgenum = world->surfedges[fs->firstedge];
		if ( edgenum > 0 )
		{
			v0 = world->edges[edgenum].v[0];
			v1 = world->edges[edgenum].v[1];
		}
		else
		{
			v0 = world->edges[edgenum * -1].v[1];
			v1 = world->edges[edgenum * -1].v[0];
		}

		g_pGeom_guess->Begin( A3D_TRIANGLES );
		nCorners = fs->numedges - 1;
		for ( tri = 1; tri < nCorners; tri++ )
		{
			edgenum = world->surfedges[fs->firstedge + tri];
			if ( edgenum > 0 )
				v2 = world->edges[edgenum].v[1];
			else
				v2 = world->edges[edgenum * -1].v[0];

			if ( tri + 1 < nCorners )
			{
				edgenum = world->surfedges[fs->firstedge + tri + 1];
				if ( edgenum > 0 )
					v3 = world->edges[edgenum].v[1];
				else
					v3 = world->edges[edgenum * -1].v[0];
				if ( hwPolyColinear_guess( world->vertexes[v1].position, world->vertexes[v2].position, world->vertexes[v3].position ) )
				{
					v1 = v3;
					tri++;
					continue;
				}
			}

			if ( g_bShowTossed_guess )
			{
				if ( !hwPolyTooSmall_guess( world->vertexes[v0].position, world->vertexes[v1].position, world->vertexes[v2].position ) )
				{
					v1 = v2;
					continue;
				}
			}
			else if ( hwPolyTooSmall_guess( world->vertexes[v0].position, world->vertexes[v1].position, world->vertexes[v2].position ) )
			{
				v1 = v2;
				continue;
			}

			if ( g_nPolysRendered_guess++ >= g_nNumPolys_guess )
			{
				g_pGeom_guess->End();
				return;
			}

			g_pGeom_guess->Tag( (DWORD)fs->plane );
			g_pA3d_guess->Compat( 0x3f1, g_bPolyKeep_guess );
			pPos = world->vertexes[v0].position;
			g_pGeom_guess->Vertex3f( pPos[0], pPos[1], pPos[2] );
			pPos = world->vertexes[v1].position;
			g_pGeom_guess->Vertex3f( pPos[0], pPos[1], pPos[2] );
			pPos = world->vertexes[v2].position;
			g_pGeom_guess->Vertex3f( pPos[0], pPos[1], pPos[2] );
			g_pA3d_guess->Compat( 0x3f1, 0 );
			v1 = v2;
		}
		g_pGeom_guess->End();
	}
}

int hwPolyTooSmall_guess( float *v0, float *v1, float *v2 )
{
	float area;
	float edge1_guess[3], edge2_guess[3], normal_guess[3];

	VectorSubtract( v1, v0, edge1_guess );
	VectorSubtract( v2, v0, edge2_guess );
	normal_guess[0] = edge1_guess[1] * edge2_guess[2] - edge1_guess[2] * edge2_guess[1];
	normal_guess[1] = edge1_guess[2] * edge2_guess[0] - edge1_guess[0] * edge2_guess[2];
	normal_guess[2] = edge1_guess[0] * edge2_guess[1] - edge1_guess[1] * edge2_guess[0];
	area = DotProduct( normal_guess, normal_guess );

	if ( area < g_flPolySize_guess )
		return 1;

	if ( area > g_flPolyKeep_guess )
	{
		g_bPolyKeep_guess = 1;
		return 0;
	}

	g_bPolyKeep_guess = 0;
	return 0;
}

int hwPolyColinear_guess( float *v0, float *v1, float *v2 )
{
	float edge1_guess[3], edge2_guess[3], normal_guess[3];

	VectorSubtract( v1, v0, edge1_guess );
	VectorSubtract( v2, v0, edge2_guess );
	normal_guess[0] = edge1_guess[1] * edge2_guess[2] - edge1_guess[2] * edge2_guess[1];
	normal_guess[1] = edge1_guess[2] * edge2_guess[0] - edge1_guess[0] * edge2_guess[2];
	normal_guess[2] = edge1_guess[0] * edge2_guess[1] - edge1_guess[1] * edge2_guess[0];
	if ( DotProduct( normal_guess, normal_guess ) < 0.000001f )
		return 1;
	return 0;
}

void hwCallLeafLists_guess( model_t *world )
{
	int count_guess = 0;
	A3dLeaf *a3dleaf_guess;
	int nLeafnum_guess;
	int idx_guess;
	IA3dList *pCurList_guess;
	A3dLeaf *curOther_guess;
	mleaf_t *pViewLeaf_guess;
	mleaf_t *nLeafs_guess;
	int nOfs_guess;

	pViewLeaf_guess = r_viewleaf;
	nLeafs_guess = world->leafs;
	nOfs_guess = (int)pViewLeaf_guess - (int)nLeafs_guess;
	nLeafnum_guess = nOfs_guess / sizeof( mleaf_t );
	if ( nLeafnum_guess < 0 || nLeafnum_guess >= g_pA3dWorld_guess->m_nLeafs )
	{
		hwRenderLeafPolys_guess( r_viewleaf, world );
		return;
	}

	g_pGeom_guess->BindMaterial( g_pMaterial_guess );
	a3dleaf_guess = g_pA3dWorld_guess->m_pLeafs[nLeafnum_guess];
	pCurList_guess = a3dleaf_guess->m_pPolyList;
	if ( pCurList_guess )
		count_guess += pCurList_guess->Call();
	pCurList_guess = a3dleaf_guess->m_pKeptList;
	if ( pCurList_guess )
		count_guess += pCurList_guess->Call();

	for ( idx_guess = 0; idx_guess < a3dleaf_guess->m_nVisLeafs; idx_guess++ )
	{
		nLeafnum_guess = a3dleaf_guess->m_pVisLeafs[idx_guess];
		curOther_guess = g_pA3dWorld_guess->m_pLeafs[nLeafnum_guess];
		pCurList_guess = curOther_guess->m_pPolyList;
		if ( pCurList_guess )
		{
			count_guess += pCurList_guess->Call();
			if ( count_guess > g_nNumPolys_guess )
				return;
		}
	}
}

void hwSetLeafNum_guess( int leafnum )
{
}

void hwSetNumPolys_guess( int numpolys )
{
	g_nNumPolys_guess = numpolys;
	g_pA3d_guess->Compat( 0x3eb, g_nNumPolys_guess );
}

void hwSetPolySize_guess( float polysize )
{
	g_flPolySize_guess = polysize;
}

void hwSetPolyKeep_guess( float polykeep )
{
	g_flPolyKeep_guess = polykeep;
}

void hwSetReflectionDelay_guess( int delay )
{
	g_flReflectionDelay_guess = delay;
	g_pGeom_guess->SetReflectionDelayScale( g_flReflectionDelay_guess );
}

void hwSetReflectionGain_guess( float gain )
{
	g_flReflectionGain_guess = gain;
	g_pGeom_guess->SetReflectionGainScale( g_flReflectionGain_guess );
}

void hwSetShowTossed_guess( int showtossed )
{
	g_bShowTossed_guess = showtossed;
}

void hwSetUsePVS_guess( int usepvs )
{
	g_bUsePVS_guess = usepvs;
}

void hwSetBloat_guess( float bloat )
{
	DWORD dwBloat;

	g_flBloat_guess = bloat;
	memcpy( &dwBloat, &g_flBloat_guess, 4 );
	g_pA3d_guess->Compat( 0x3ea, dwBloat );
}

void hwSetOcclusions_guess( int occlude )
{
	g_bOcclusions_guess = occlude;
	if ( g_bOcclusions_guess )
		g_pGeom_guess->Enable( A3D_OCCLUSIONS );
	else
		g_pGeom_guess->Disable( A3D_OCCLUSIONS );
}

void hwSetReflections_guess( int reflect )
{
	g_bReflections_guess = reflect;
	if ( g_bReflections_guess )
		g_pGeom_guess->Enable( A3D_1ST_REFLECTIONS );
	else
		g_pGeom_guess->Disable( A3D_1ST_REFLECTIONS );
}

void hwSetOcclusionFactor_guess( float factor )
{
	HRESULT hr;

	if ( !g_pMaterial_guess || !g_pGeom_guess )
		return;

	hr = g_pMaterial_guess->SetTransmittance( factor, 0.75f );
	g_pGeom_guess->BindMaterial( g_pMaterial_guess );
}

void hwSetOcclusionUpdate_guess( int update )
{
	g_bOcclusionUpdate_guess = update;
	if ( !g_bOcclusionUpdate_guess )
		g_pGeom_guess->BindMaterial( g_pMaterial_guess );
}

void hwSetOcclusionEpsilon_guess( float epsilon )
{
	DWORD dwEpsilon;

	memcpy( &dwEpsilon, &epsilon, 4 );
	g_pA3d_guess->Compat( 0x3f2, dwEpsilon );
}
