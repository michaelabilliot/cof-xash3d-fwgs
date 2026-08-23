
#include "dx_local.h"
#include "dx_profiler.h"

static const char *g_dxProfilePhaseNames[DX_PROFILE_COUNT] =
{
	"BeginFrame total",
	"lightmap updates",
	"Clear",
	"BeginScene",
	"scene setup/view matrices",
	"R_DrawEnts total",
	"skybox",
	"static world",
	"entity classify/sort",
	"opaque entities",
	"sprite batch flushes",
	"water depth prepass",
	"water color pass",
	"translucent entities",
	"beams",
	"viewmodel",
	"2D/HUD batching",
	"EndScene",
	"Present",
	"EndFrame misc",
	"brush model internals",
	"studio model internals",
	"sprite model internals",
	"decal internals",
	"water model internals",
	"studio static light probe",
	"studio sky probe",
	"studio dynamic light accumulation",
	"studio local elight selection"
};

static const char *g_dxProfileEventNames[DX_PROFILE_EVENT_COUNT] =
{
	"world batches drawn",
	"world batches PVS-culled",
	"world batches frustum-culled",
	"static visibility cache hits",
	"static visibility cache misses",
	"water batches drawn",
	"sprites queued",
	"sprite batches drawn",
	"decal batches drawn",
	"studio pose cache hits",
	"studio pose cache misses/updates",
	"lightmap allocations updated",
	"ripple texture uploads",
	"glow queries issued",
	"studio VB locks",
	"studio upload KB",
	"studio batches drawn",
	"studio update budget skips",
	"lightmap atlas uploads",
	"lightmap threaded jobs",
	"studio light probe cache hits",
	"studio light probe rebuilds",
	"studio sky traces",
	"studio dynamic light candidates",
	"studio local light candidates",
	"studio sky probes used",
	"2D quads queued",
	"2D batches drawn"
};

struct dx_profile_stats_t
{
	double sumSeconds = 0.0;
	double maxSeconds = 0.0;
	unsigned int calls = 0;
};

struct dx_profile_counters_t
{
	unsigned int drawCalls = 0;
	unsigned int drawIndexedCalls = 0;
	unsigned int primitiveCount = 0;

	unsigned int contextVbLocks = 0;
	unsigned int contextIbLocks = 0;
	unsigned int contextVbDiscards = 0;
	unsigned int contextIbDiscards = 0;
	unsigned int contextVbNoOverwrite = 0;
	unsigned int contextIbNoOverwrite = 0;
	unsigned int contextVbBytes = 0;
	unsigned int contextIbBytes = 0;

	unsigned int modelVbLocks = 0;
	unsigned int modelIbLocks = 0;
	unsigned int modelVbDiscards = 0;
	unsigned int modelIbDiscards = 0;
	unsigned int modelVbNoOverwrite = 0;
	unsigned int modelIbNoOverwrite = 0;
	unsigned int modelVbBytes = 0;
	unsigned int modelIbBytes = 0;

	unsigned int renderStateCalls = 0;
	unsigned int renderStateChanges = 0;
	unsigned int renderStateSkips = 0;
	unsigned int stageStateCalls = 0;
	unsigned int stageStateChanges = 0;
	unsigned int stageStateSkips = 0;
	unsigned int samplerStateCalls = 0;
	unsigned int samplerStateChanges = 0;
	unsigned int samplerStateSkips = 0;

	unsigned int textureBindCalls = 0;
	unsigned int textureBindChanges = 0;
	unsigned int textureBindSkips = 0;
	unsigned int rawTextureBindCalls = 0;
	unsigned int rawTextureBindChanges = 0;
	unsigned int rawTextureBindSkips = 0;

	unsigned int events[DX_PROFILE_EVENT_COUNT] = {};
};

struct frame_debug_capture_t
{
	bool active = false;
	double startTime = 0.0;
	double lastSampleTime = 0.0;
	double duration = 0.0;
	double sumFrameSeconds = 0.0;
	double sumFrameSecondsSq = 0.0;
	double minFrameSeconds = DBL_MAX;
	double maxFrameSeconds = 0.0;
	double worstFrameSeconds = 0.0;
	unsigned int frameCount = 0;
	unsigned int stutterFrames = 0;
	double previousFrameSeconds = 0.0;
	bool hasPreviousFrameSeconds = false;
	double sumAbsFrameDeltaSeconds = 0.0;
	double maxAbsFrameDeltaSeconds = 0.0;
	std::vector<double> frameTimes;
	dx_profile_stats_t phases[DX_PROFILE_COUNT];
	dx_profile_counters_t counterSums;
	dx_profile_counters_t counterMax;
	dx_profile_counters_t worstCounters;
	double worstFramePhases[DX_PROFILE_COUNT] = {};
};

static frame_debug_capture_t g_frameDebugCapture;
static bool g_debugCmdsRegistered = false;
static double g_dxProfileFramePhases[DX_PROFILE_COUNT] = {};

static unsigned int g_dxProfileFramePhaseCalls[DX_PROFILE_COUNT] = {};
static dx_profile_counters_t g_dxProfileFrameCounters;

static dx_speeds_frame_t g_dxSpeedsFrame;
static bool g_dxSpeedsCollecting = false;

double DX_Now( void )
{

	static LARGE_INTEGER frequency = {};
	static BOOL hasFrequency = QueryPerformanceFrequency( &frequency );
	if( hasFrequency && frequency.QuadPart > 0 )
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter( &counter );
		return static_cast<double>( counter.QuadPart ) /
			static_cast<double>( frequency.QuadPart );
	}

	if( gEngfuncs.pfnTime )
		return gEngfuncs.pfnTime( );
	if( gp_host )
		return gp_host->realtime;
	if( gp_cl )
		return gp_cl->time;
	return 0.0;
}

bool DX_ProfileActive( void )
{
	return g_frameDebugCapture.active;
}

void DX_ProfileTime( int phase, double seconds )
{

	if( !g_frameDebugCapture.active || phase < 0 || phase >= DX_PROFILE_COUNT ||
		!std::isfinite( seconds ) || seconds <= 0.0 || seconds >= 10.0 )
	{
		return;
	}
	g_dxProfileFramePhases[phase] += seconds;
	++g_dxProfileFramePhaseCalls[phase];
}

void DX_ProfileDraw( bool indexed, unsigned int primitiveCount )
{

	if( g_dxSpeedsCollecting )
	{
		++g_dxSpeedsFrame.drawCalls;
		if( indexed )
			++g_dxSpeedsFrame.indexedDrawCalls;
		g_dxSpeedsFrame.primitiveCount += primitiveCount;
	}

	if( !g_frameDebugCapture.active )
		return;

	++g_dxProfileFrameCounters.drawCalls;
	if( indexed )
		++g_dxProfileFrameCounters.drawIndexedCalls;
	g_dxProfileFrameCounters.primitiveCount += primitiveCount;
}

void DX_ProfileLock( int owner, int bufferKind, unsigned int lockFlags, unsigned int bytes )
{
	if( !g_frameDebugCapture.active )
		return;

	unsigned int *locks = nullptr;
	unsigned int *discards = nullptr;
	unsigned int *noOverwrites = nullptr;
	unsigned int *byteCounter = nullptr;
	if( owner == DX_PROFILE_SCRATCH_CONTEXT )
	{
		if( bufferKind == DX_PROFILE_SCRATCH_VB )
		{
			locks = &g_dxProfileFrameCounters.contextVbLocks;
			discards = &g_dxProfileFrameCounters.contextVbDiscards;
			noOverwrites = &g_dxProfileFrameCounters.contextVbNoOverwrite;
			byteCounter = &g_dxProfileFrameCounters.contextVbBytes;
		}
		else
		{
			locks = &g_dxProfileFrameCounters.contextIbLocks;
			discards = &g_dxProfileFrameCounters.contextIbDiscards;
			noOverwrites = &g_dxProfileFrameCounters.contextIbNoOverwrite;
			byteCounter = &g_dxProfileFrameCounters.contextIbBytes;
		}
	}
	else
	{
		if( bufferKind == DX_PROFILE_SCRATCH_VB )
		{
			locks = &g_dxProfileFrameCounters.modelVbLocks;
			discards = &g_dxProfileFrameCounters.modelVbDiscards;
			noOverwrites = &g_dxProfileFrameCounters.modelVbNoOverwrite;
			byteCounter = &g_dxProfileFrameCounters.modelVbBytes;
		}
		else
		{
			locks = &g_dxProfileFrameCounters.modelIbLocks;
			discards = &g_dxProfileFrameCounters.modelIbDiscards;
			noOverwrites = &g_dxProfileFrameCounters.modelIbNoOverwrite;
			byteCounter = &g_dxProfileFrameCounters.modelIbBytes;
		}
	}

	++( *locks );
	if( lockFlags & D3DLOCK_DISCARD ) ++( *discards );
	if( lockFlags & D3DLOCK_NOOVERWRITE ) ++( *noOverwrites );
	*byteCounter += bytes;
}

void DX_ProfileState( int kind, bool changed )
{
	if( !g_frameDebugCapture.active )
		return;

	unsigned int *calls = nullptr;
	unsigned int *changes = nullptr;
	unsigned int *skips = nullptr;
	switch( kind )
	{
	case DX_PROFILE_STATE_RENDER:
		calls = &g_dxProfileFrameCounters.renderStateCalls;
		changes = &g_dxProfileFrameCounters.renderStateChanges;
		skips = &g_dxProfileFrameCounters.renderStateSkips;
		break;
	case DX_PROFILE_STATE_STAGE:
		calls = &g_dxProfileFrameCounters.stageStateCalls;
		changes = &g_dxProfileFrameCounters.stageStateChanges;
		skips = &g_dxProfileFrameCounters.stageStateSkips;
		break;
	case DX_PROFILE_STATE_SAMPLER:
	default:
		calls = &g_dxProfileFrameCounters.samplerStateCalls;
		changes = &g_dxProfileFrameCounters.samplerStateChanges;
		skips = &g_dxProfileFrameCounters.samplerStateSkips;
		break;
	}

	++( *calls );
	if( changed ) ++( *changes );
	else ++( *skips );
}

void DX_ProfileTexture( int kind, bool changed )
{
	if( !g_frameDebugCapture.active )
		return;

	if( kind == DX_PROFILE_TEXTURE_RAW )
	{
		++g_dxProfileFrameCounters.rawTextureBindCalls;
		if( changed ) ++g_dxProfileFrameCounters.rawTextureBindChanges;
		else ++g_dxProfileFrameCounters.rawTextureBindSkips;
	}
	else
	{
		++g_dxProfileFrameCounters.textureBindCalls;
		if( changed ) ++g_dxProfileFrameCounters.textureBindChanges;
		else ++g_dxProfileFrameCounters.textureBindSkips;
	}
}

void DX_ProfileEvent( int eventId, unsigned int amount )
{
	if( !g_frameDebugCapture.active || eventId < 0 || eventId >= DX_PROFILE_EVENT_COUNT )
		return;
	g_dxProfileFrameCounters.events[eventId] += amount;
}

static void DX_ProfileReset( void )
{
	memset( g_dxProfileFramePhases, 0, sizeof( g_dxProfileFramePhases ) );
	memset( g_dxProfileFramePhaseCalls, 0, sizeof( g_dxProfileFramePhaseCalls ) );
	g_dxProfileFrameCounters = dx_profile_counters_t{};
}

void DX_ProfilerBeginFrame( bool collectSpeeds )
{
	g_dxSpeedsCollecting = collectSpeeds;
	g_dxSpeedsFrame = dx_speeds_frame_t{};
	DX_ProfileReset( );
}

const dx_speeds_frame_t &DX_ProfileSpeeds( void )
{
	return g_dxSpeedsFrame;
}

static void DX_ProfileAccumulate( double frameSeconds )
{
	for( int i = 0; i < DX_PROFILE_COUNT; ++i )
	{
		const double seconds = g_dxProfileFramePhases[i];
		if( seconds <= 0.0 )
			continue;
		dx_profile_stats_t &phase = g_frameDebugCapture.phases[i];
		phase.sumSeconds += seconds;
		phase.maxSeconds = std::max( phase.maxSeconds, seconds );
		phase.calls += g_dxProfileFramePhaseCalls[i];
	}

#define DX_PROFILE_ACCUM_COUNTER( field ) \
		g_frameDebugCapture.counterSums.field += g_dxProfileFrameCounters.field; \
		g_frameDebugCapture.counterMax.field = std::max( g_frameDebugCapture.counterMax.field, g_dxProfileFrameCounters.field )

	DX_PROFILE_ACCUM_COUNTER( drawCalls );
	DX_PROFILE_ACCUM_COUNTER( drawIndexedCalls );
	DX_PROFILE_ACCUM_COUNTER( primitiveCount );
	DX_PROFILE_ACCUM_COUNTER( contextVbLocks );
	DX_PROFILE_ACCUM_COUNTER( contextIbLocks );
	DX_PROFILE_ACCUM_COUNTER( contextVbDiscards );
	DX_PROFILE_ACCUM_COUNTER( contextIbDiscards );
	DX_PROFILE_ACCUM_COUNTER( contextVbNoOverwrite );
	DX_PROFILE_ACCUM_COUNTER( contextIbNoOverwrite );
	DX_PROFILE_ACCUM_COUNTER( contextVbBytes );
	DX_PROFILE_ACCUM_COUNTER( contextIbBytes );
	DX_PROFILE_ACCUM_COUNTER( modelVbLocks );
	DX_PROFILE_ACCUM_COUNTER( modelIbLocks );
	DX_PROFILE_ACCUM_COUNTER( modelVbDiscards );
	DX_PROFILE_ACCUM_COUNTER( modelIbDiscards );
	DX_PROFILE_ACCUM_COUNTER( modelVbNoOverwrite );
	DX_PROFILE_ACCUM_COUNTER( modelIbNoOverwrite );
	DX_PROFILE_ACCUM_COUNTER( modelVbBytes );
	DX_PROFILE_ACCUM_COUNTER( modelIbBytes );
	DX_PROFILE_ACCUM_COUNTER( renderStateCalls );
	DX_PROFILE_ACCUM_COUNTER( renderStateChanges );
	DX_PROFILE_ACCUM_COUNTER( renderStateSkips );
	DX_PROFILE_ACCUM_COUNTER( stageStateCalls );
	DX_PROFILE_ACCUM_COUNTER( stageStateChanges );
	DX_PROFILE_ACCUM_COUNTER( stageStateSkips );
	DX_PROFILE_ACCUM_COUNTER( samplerStateCalls );
	DX_PROFILE_ACCUM_COUNTER( samplerStateChanges );
	DX_PROFILE_ACCUM_COUNTER( samplerStateSkips );
	DX_PROFILE_ACCUM_COUNTER( textureBindCalls );
	DX_PROFILE_ACCUM_COUNTER( textureBindChanges );
	DX_PROFILE_ACCUM_COUNTER( textureBindSkips );
	DX_PROFILE_ACCUM_COUNTER( rawTextureBindCalls );
	DX_PROFILE_ACCUM_COUNTER( rawTextureBindChanges );
	DX_PROFILE_ACCUM_COUNTER( rawTextureBindSkips );

	for( int i = 0; i < DX_PROFILE_EVENT_COUNT; ++i )
	{
		g_frameDebugCapture.counterSums.events[i] += g_dxProfileFrameCounters.events[i];
		g_frameDebugCapture.counterMax.events[i] = std::max(
			g_frameDebugCapture.counterMax.events[i], g_dxProfileFrameCounters.events[i] );
	}

#undef DX_PROFILE_ACCUM_COUNTER

	if( frameSeconds >= g_frameDebugCapture.worstFrameSeconds )
	{
		g_frameDebugCapture.worstFrameSeconds = frameSeconds;
		memcpy( g_frameDebugCapture.worstFramePhases,
			g_dxProfileFramePhases, sizeof( g_dxProfileFramePhases ) );
		g_frameDebugCapture.worstCounters = g_dxProfileFrameCounters;
	}
}

static double R_DebugClampDuration( double seconds )
{
	if( seconds < 0.1 )
		return 0.1;
	if( seconds > 600.0 )
		return 600.0;
	return seconds;
}

static void R_DebugPrintUsage( void )
{
	gEngfuncs.Con_Printf( "usage: dx_fps_debug <seconds>\n" );
	gEngfuncs.Con_Printf( "example: dx_fps_debug 10\n" );
}

static void R_DebugCommand( void )
{
	if( gEngfuncs.Cmd_Argc( ) < 2 )
	{
		R_DebugPrintUsage( );
		return;
	}

	const char *arg = gEngfuncs.Cmd_Argv( 1 );
	char *end = nullptr;
	const double requestedDuration = arg ? strtod( arg, &end ) : 0.0;
	if( !arg || end == arg || requestedDuration <= 0.0 )
	{
		R_DebugPrintUsage( );
		return;
	}

	const double now = DX_Now( );
	g_frameDebugCapture = frame_debug_capture_t{};
	g_frameDebugCapture.active = true;
	g_frameDebugCapture.startTime = now;
	g_frameDebugCapture.lastSampleTime = now;
	g_frameDebugCapture.duration = R_DebugClampDuration( requestedDuration );
	g_frameDebugCapture.frameTimes.reserve( static_cast<size_t>(
		std::min( 240000.0, std::max( 1024.0, g_frameDebugCapture.duration * 4000.0 ) ) ) );
	DX_ProfileReset( );

	gEngfuncs.Con_Printf( "D3D9 frame profiler: sampling %.2f seconds...\n",
		g_frameDebugCapture.duration );
}

void DX_ProfilerRegisterCommands( void )
{
	if( g_debugCmdsRegistered )
		return;

	gEngfuncs.Cmd_AddCommand( "dx_fps_debug", R_DebugCommand,
		"Measure renderer frame pacing and D3D pass statistics for N seconds" );
	gEngfuncs.Cmd_AddCommand( "dx_frame_debug", R_DebugCommand,
		"Alias for dx_fps_debug" );
	g_debugCmdsRegistered = true;
}

void DX_ProfilerRemoveCommands( void )
{
	if( !g_debugCmdsRegistered )
		return;

	gEngfuncs.Cmd_RemoveCommand( "dx_fps_debug" );
	gEngfuncs.Cmd_RemoveCommand( "dx_frame_debug" );
	g_debugCmdsRegistered = false;
}

static void R_DebugPrintPhases( unsigned int frames )
{
	gEngfuncs.Con_Printf( "\n  time by renderer phase (avg/max ms; timed regions may be nested):\n" );
	bool printedPhase[DX_PROFILE_COUNT] = {};
	for( int printed = 0; printed < 10; ++printed )
	{
		int best = -1;
		double bestAvg = 0.0;
		for( int i = 0; i < DX_PROFILE_COUNT; ++i )
		{
			const dx_profile_stats_t &phase = g_frameDebugCapture.phases[i];
			if( printedPhase[i] || phase.calls == 0 )
				continue;
			const double avg = phase.sumSeconds / static_cast<double>( frames );
			if( avg > bestAvg )
			{
				bestAvg = avg;
				best = i;
			}
		}
		if( best < 0 || bestAvg <= 0.0 )
			break;
		printedPhase[best] = true;
		const dx_profile_stats_t &phase = g_frameDebugCapture.phases[best];
		gEngfuncs.Con_Printf( "    %-28s avg %.4f | max %.4f | calls/frame %.2f\n",
			g_dxProfilePhaseNames[best],
			1000.0 * phase.sumSeconds / static_cast<double>( frames ),
			1000.0 * phase.maxSeconds,
			static_cast<double>( phase.calls ) / static_cast<double>( frames ) );
	}

	gEngfuncs.Con_Printf( "\n  worst frame breakdown %.3f ms:\n",
		g_frameDebugCapture.worstFrameSeconds * 1000.0 );
	bool printedWorst[DX_PROFILE_COUNT] = {};
	for( int printed = 0; printed < 8; ++printed )
	{
		int best = -1;
		double bestTime = 0.0;
		for( int i = 0; i < DX_PROFILE_COUNT; ++i )
		{
			if( printedWorst[i] ) continue;
			if( g_frameDebugCapture.worstFramePhases[i] > bestTime )
			{
				bestTime = g_frameDebugCapture.worstFramePhases[i];
				best = i;
			}
		}
		if( best < 0 || bestTime <= 0.0 )
			break;
		printedWorst[best] = true;
		gEngfuncs.Con_Printf( "    %-28s %.4f ms\n",
			g_dxProfilePhaseNames[best], bestTime * 1000.0 );
	}
}

static void R_DebugPrintDiagnosis( unsigned int frames, double avgFrameMs )
{
	if( frames == 0 )
		return;

	const double invFrames = 1.0 / static_cast<double>( frames );
	const dx_profile_counters_t &sum = g_frameDebugCapture.counterSums;
	const dx_profile_counters_t &maxc = g_frameDebugCapture.counterMax;
	const dx_profile_counters_t &worst = g_frameDebugCapture.worstCounters;

	gEngfuncs.Con_Printf( "\n  quick diagnosis:\n" );

	int bestPhase = -1;
	double bestAvgSeconds = 0.0;
	for( int i = 0; i < DX_PROFILE_COUNT; ++i )
	{
		const dx_profile_stats_t &phase = g_frameDebugCapture.phases[i];
		if( phase.calls == 0 ) continue;
		const double avg = phase.sumSeconds * invFrames;
		if( avg > bestAvgSeconds )
		{
			bestAvgSeconds = avg;
			bestPhase = i;
		}
	}

	if( bestPhase >= 0 )
	{
		gEngfuncs.Con_Printf( "    highest avg timed phase: %s %.4f ms/frame\n",
			g_dxProfilePhaseNames[bestPhase], bestAvgSeconds * 1000.0 );
	}
	else
	{
		gEngfuncs.Con_Printf( "    no timed phase data was captured; only counters are valid.\n" );
	}

	if( worst.drawCalls > sum.drawCalls * invFrames * 1.10 )
	{
		gEngfuncs.Con_Printf( "    worst frame had more draw calls than average: %u vs %.1f.\n",
			worst.drawCalls, sum.drawCalls * invFrames );
	}
	if( worst.modelVbLocks > sum.modelVbLocks * invFrames * 1.10 )
	{
		gEngfuncs.Con_Printf( "    worst frame had more model VB locks than average: %u vs %.1f.\n",
			worst.modelVbLocks, sum.modelVbLocks * invFrames );
	}
	if( maxc.events[DX_PROFILE_EVENT_LIGHTMAP_ALLOC_UPDATED] >
		std::max( 16.0, sum.events[DX_PROFILE_EVENT_LIGHTMAP_ALLOC_UPDATED] * invFrames * 4.0 ) )
	{
		gEngfuncs.Con_Printf( "    lightmap updates are bursty: avg %.2f allocations/frame, max %u. v11 should process these as one fast threaded slice.\n",
			sum.events[DX_PROFILE_EVENT_LIGHTMAP_ALLOC_UPDATED] * invFrames,
			maxc.events[DX_PROFILE_EVENT_LIGHTMAP_ALLOC_UPDATED] );
	}
	if( sum.modelVbLocks * invFrames > 64.0 )
	{
		gEngfuncs.Con_Printf( "    model dynamic VB locks are very high: %.1f/frame. Check studio model internals and studio VB lock events.\n",
			sum.modelVbLocks * invFrames );
	}
	if( sum.renderStateCalls * invFrames > 1000.0 || sum.stageStateCalls * invFrames > 1000.0 ||
		sum.samplerStateCalls * invFrames > 1000.0 )
	{
		gEngfuncs.Con_Printf( "    state wrapper traffic is high even when skipped: render %.0f, stage %.0f, sampler %.0f calls/frame.\n",
			sum.renderStateCalls * invFrames, sum.stageStateCalls * invFrames,
			sum.samplerStateCalls * invFrames );
	}
	if( avgFrameMs > 0.0 && g_frameDebugCapture.maxFrameSeconds * 1000.0 > avgFrameMs * 2.0 )
	{
		gEngfuncs.Con_Printf( "    max frame is over 2x average; look at the worst-frame timed breakdown first.\n" );
	}
}

static void R_DebugPrintCounters( unsigned int frames )
{
	const dx_profile_counters_t &sum = g_frameDebugCapture.counterSums;
	const dx_profile_counters_t &maxc = g_frameDebugCapture.counterMax;
	const dx_profile_counters_t &worst = g_frameDebugCapture.worstCounters;
	const double invFrames = frames ? 1.0 / static_cast<double>( frames ) : 0.0;

	gEngfuncs.Con_Printf( "\n  D3D submission counters (avg/frame | max/frame | worst-frame):\n" );
	gEngfuncs.Con_Printf( "    draws:                 %.1f | %u | %u   indexed avg %.1f\n",
		sum.drawCalls * invFrames, maxc.drawCalls, worst.drawCalls,
		sum.drawIndexedCalls * invFrames );
	gEngfuncs.Con_Printf( "    primitives:            %.1f | %u | %u\n",
		sum.primitiveCount * invFrames, maxc.primitiveCount, worst.primitiveCount );
	gEngfuncs.Con_Printf( "    context scratch locks: VB %.2f/%.1f KB | IB %.2f/%.1f KB | discards VB/IB %.2f/%.2f\n",
		sum.contextVbLocks * invFrames, ( sum.contextVbBytes * invFrames ) / 1024.0,
		sum.contextIbLocks * invFrames, ( sum.contextIbBytes * invFrames ) / 1024.0,
		sum.contextVbDiscards * invFrames, sum.contextIbDiscards * invFrames );
	gEngfuncs.Con_Printf( "    model scratch locks:   VB %.2f/%.1f KB | IB %.2f/%.1f KB | discards VB/IB %.2f/%.2f\n",
		sum.modelVbLocks * invFrames, ( sum.modelVbBytes * invFrames ) / 1024.0,
		sum.modelIbLocks * invFrames, ( sum.modelIbBytes * invFrames ) / 1024.0,
		sum.modelVbDiscards * invFrames, sum.modelIbDiscards * invFrames );
	gEngfuncs.Con_Printf( "    render states:         calls %.1f | real %.1f | skipped %.1f\n",
		sum.renderStateCalls * invFrames, sum.renderStateChanges * invFrames,
		sum.renderStateSkips * invFrames );
	gEngfuncs.Con_Printf( "    texture stage states:  calls %.1f | real %.1f | skipped %.1f\n",
		sum.stageStateCalls * invFrames, sum.stageStateChanges * invFrames,
		sum.stageStateSkips * invFrames );
	gEngfuncs.Con_Printf( "    sampler states:        calls %.1f | real %.1f | skipped %.1f\n",
		sum.samplerStateCalls * invFrames, sum.samplerStateChanges * invFrames,
		sum.samplerStateSkips * invFrames );
	gEngfuncs.Con_Printf( "    DX_Bind textures:      calls %.1f | real %.1f | skipped %.1f\n",
		sum.textureBindCalls * invFrames, sum.textureBindChanges * invFrames,
		sum.textureBindSkips * invFrames );
	gEngfuncs.Con_Printf( "    raw texture binds:     calls %.1f | real %.1f | skipped %.1f\n",
		sum.rawTextureBindCalls * invFrames, sum.rawTextureBindChanges * invFrames,
		sum.rawTextureBindSkips * invFrames );

	gEngfuncs.Con_Printf( "\n  renderer events (avg/frame | max/frame | worst-frame):\n" );
	for( int i = 0; i < DX_PROFILE_EVENT_COUNT; ++i )
	{
		if( sum.events[i] == 0 && maxc.events[i] == 0 && worst.events[i] == 0 )
			continue;
		gEngfuncs.Con_Printf( "    %-30s %.2f | %u | %u\n",
			g_dxProfileEventNames[i], sum.events[i] * invFrames,
			maxc.events[i], worst.events[i] );
	}
}

static void R_DebugWriteReport(
	double measuredSeconds, unsigned int frames,
	double minMs, double maxMs, double avgMs, double jitterMs,
	double minFps, double maxFps, double avgFps,
	double p50Ms, double p95Ms, double p99Ms, double p999Ms,
	double onePercentLowFps, double pointOnePercentLowFps,
	double avgAbsDeltaMs, double maxAbsDeltaMs,
	unsigned int slow125, unsigned int slow150, unsigned int slow200,
	double smoothness, double avgCoarseRendererMs,
	double avgUnaccountedMs, double avgPresentMs,
	double worstPresentMs, double worstRendererMs, double worstOutsideMs )
{
	SYSTEMTIME localTime;
	GetLocalTime( &localTime );

	char filename[128];
	snprintf( filename, sizeof( filename ),
		"dx_fps_debug_%04u%02u%02u_%02u%02u%02u.txt",
		static_cast<unsigned int>( localTime.wYear ),
		static_cast<unsigned int>( localTime.wMonth ),
		static_cast<unsigned int>( localTime.wDay ),
		static_cast<unsigned int>( localTime.wHour ),
		static_cast<unsigned int>( localTime.wMinute ),
		static_cast<unsigned int>( localTime.wSecond ) );

	FILE *file = fopen( filename, "wb" );
	if( !file )
	{
		gEngfuncs.Con_Printf(
			"D3D9 frame profiler: could not write report file '%s'.\n",
			filename );
		return;
	}

	const double invFrames = frames ?
		1.0 / static_cast<double>( frames ) : 0.0;
	const dx_profile_counters_t &sum = g_frameDebugCapture.counterSums;
	const dx_profile_counters_t &maxc = g_frameDebugCapture.counterMax;
	const dx_profile_counters_t &worst = g_frameDebugCapture.worstCounters;

	fprintf( file, "D3D9 frame profiler report\n" );
	fprintf( file, "==========================\n\n" );
	fprintf( file, "timestamp_local=%04u-%02u-%02u %02u:%02u:%02u.%03u\n",
		static_cast<unsigned int>( localTime.wYear ),
		static_cast<unsigned int>( localTime.wMonth ),
		static_cast<unsigned int>( localTime.wDay ),
		static_cast<unsigned int>( localTime.wHour ),
		static_cast<unsigned int>( localTime.wMinute ),
		static_cast<unsigned int>( localTime.wSecond ),
		static_cast<unsigned int>( localTime.wMilliseconds ) );
	fprintf( file, "measured_seconds=%.9f\n", measuredSeconds );
	fprintf( file, "frames_sampled=%u\n\n", frames );

	fprintf( file, "[frame_summary]\n" );
	fprintf( file, "frame_time_min_ms=%.9f\n", minMs );
	fprintf( file, "frame_time_max_ms=%.9f\n", maxMs );
	fprintf( file, "frame_time_avg_ms=%.9f\n", avgMs );
	fprintf( file, "frame_time_jitter_stddev_ms=%.9f\n", jitterMs );
	fprintf( file, "fps_min=%.6f\n", minFps );
	fprintf( file, "fps_max=%.6f\n", maxFps );
	fprintf( file, "fps_avg=%.6f\n", avgFps );
	fprintf( file, "frame_p50_ms=%.9f\n", p50Ms );
	fprintf( file, "frame_p95_ms=%.9f\n", p95Ms );
	fprintf( file, "frame_p99_ms=%.9f\n", p99Ms );
	fprintf( file, "frame_p99_9_ms=%.9f\n", p999Ms );
	fprintf( file, "fps_1_percent_low=%.6f\n", onePercentLowFps );
	fprintf( file, "fps_0_1_percent_low=%.6f\n", pointOnePercentLowFps );
	fprintf( file, "frame_delta_avg_abs_ms=%.9f\n", avgAbsDeltaMs );
	fprintf( file, "frame_delta_max_abs_ms=%.9f\n", maxAbsDeltaMs );
	fprintf( file, "slow_frames_gt_1_25x_avg=%u\n", slow125 );
	fprintf( file, "slow_frames_gt_1_5x_avg=%u\n", slow150 );
	fprintf( file, "slow_frames_gt_2x_avg=%u\n", slow200 );
	fprintf( file, "smoothness_percent=%.6f\n", smoothness );
	fprintf( file, "stutter_frames=%u\n", g_frameDebugCapture.stutterFrames );
	fprintf( file, "coarse_renderer_avg_ms=%.9f\n", avgCoarseRendererMs );
	fprintf( file, "unaccounted_outside_renderer_avg_ms=%.9f\n",
		avgUnaccountedMs );
	fprintf( file, "present_avg_ms=%.9f\n", avgPresentMs );
	fprintf( file, "present_worst_frame_ms=%.9f\n", worstPresentMs );
	fprintf( file, "renderer_worst_frame_ms=%.9f\n", worstRendererMs );
	fprintf( file, "outside_renderer_worst_frame_ms=%.9f\n", worstOutsideMs );
	fprintf( file, "worst_frame_total_ms=%.9f\n",
		g_frameDebugCapture.worstFrameSeconds * 1000.0 );
	fprintf( file, "\n" );

	fprintf( file,
		"[renderer_phases]\n"
		"id\tname\tavg_ms_per_frame\tmax_ms\tcalls_per_frame\t"
		"total_ms\tcalls\tworst_frame_ms\n" );
	for( int i = 0; i < DX_PROFILE_COUNT; ++i )
	{
		const dx_profile_stats_t &phase =
			g_frameDebugCapture.phases[i];
		fprintf( file, "%d\t%s\t%.9f\t%.9f\t%.9f\t%.9f\t%u\t%.9f\n",
			i, g_dxProfilePhaseNames[i],
			1000.0 * phase.sumSeconds * invFrames,
			1000.0 * phase.maxSeconds,
			static_cast<double>( phase.calls ) * invFrames,
			1000.0 * phase.sumSeconds,
			phase.calls,
			1000.0 * g_frameDebugCapture.worstFramePhases[i] );
	}
	fprintf( file, "\n" );

	fprintf( file,
		"[submission_counters]\n"
		"name\tavg_per_frame\tmax_frame\tworst_frame\ttotal\n" );

#define DX_FPS_FILE_COUNTER( field ) \
	fprintf( file, #field "\t%.9f\t%u\t%u\t%u\n", \
		static_cast<double>( sum.field ) * invFrames, \
		maxc.field, worst.field, sum.field )

	DX_FPS_FILE_COUNTER( drawCalls );
	DX_FPS_FILE_COUNTER( drawIndexedCalls );
	DX_FPS_FILE_COUNTER( primitiveCount );
	DX_FPS_FILE_COUNTER( contextVbLocks );
	DX_FPS_FILE_COUNTER( contextIbLocks );
	DX_FPS_FILE_COUNTER( contextVbDiscards );
	DX_FPS_FILE_COUNTER( contextIbDiscards );
	DX_FPS_FILE_COUNTER( contextVbNoOverwrite );
	DX_FPS_FILE_COUNTER( contextIbNoOverwrite );
	DX_FPS_FILE_COUNTER( contextVbBytes );
	DX_FPS_FILE_COUNTER( contextIbBytes );
	DX_FPS_FILE_COUNTER( modelVbLocks );
	DX_FPS_FILE_COUNTER( modelIbLocks );
	DX_FPS_FILE_COUNTER( modelVbDiscards );
	DX_FPS_FILE_COUNTER( modelIbDiscards );
	DX_FPS_FILE_COUNTER( modelVbNoOverwrite );
	DX_FPS_FILE_COUNTER( modelIbNoOverwrite );
	DX_FPS_FILE_COUNTER( modelVbBytes );
	DX_FPS_FILE_COUNTER( modelIbBytes );
	DX_FPS_FILE_COUNTER( renderStateCalls );
	DX_FPS_FILE_COUNTER( renderStateChanges );
	DX_FPS_FILE_COUNTER( renderStateSkips );
	DX_FPS_FILE_COUNTER( stageStateCalls );
	DX_FPS_FILE_COUNTER( stageStateChanges );
	DX_FPS_FILE_COUNTER( stageStateSkips );
	DX_FPS_FILE_COUNTER( samplerStateCalls );
	DX_FPS_FILE_COUNTER( samplerStateChanges );
	DX_FPS_FILE_COUNTER( samplerStateSkips );
	DX_FPS_FILE_COUNTER( textureBindCalls );
	DX_FPS_FILE_COUNTER( textureBindChanges );
	DX_FPS_FILE_COUNTER( textureBindSkips );
	DX_FPS_FILE_COUNTER( rawTextureBindCalls );
	DX_FPS_FILE_COUNTER( rawTextureBindChanges );
	DX_FPS_FILE_COUNTER( rawTextureBindSkips );

#undef DX_FPS_FILE_COUNTER

	fprintf( file, "\n" );
	fprintf( file,
		"[renderer_events]\n"
		"id\tname\tavg_per_frame\tmax_frame\tworst_frame\ttotal\n" );
	for( int i = 0; i < DX_PROFILE_EVENT_COUNT; ++i )
	{
		fprintf( file, "%d\t%s\t%.9f\t%u\t%u\t%u\n",
			i, g_dxProfileEventNames[i],
			static_cast<double>( sum.events[i] ) * invFrames,
			maxc.events[i], worst.events[i], sum.events[i] );
	}
	fprintf( file, "\n" );

	fprintf( file, "[raw_frame_times]\n" );
	fprintf( file, "frame\tframe_ms\tinstant_fps\tabs_delta_ms\n" );
	double previous = 0.0;
	bool havePrevious = false;
	for( size_t i = 0; i < g_frameDebugCapture.frameTimes.size( ); ++i )
	{
		const double seconds = g_frameDebugCapture.frameTimes[i];
		const double milliseconds = seconds * 1000.0;
		const double fps = seconds > 0.0 ? 1.0 / seconds : 0.0;
		const double absDelta = havePrevious ?
			fabs( seconds - previous ) * 1000.0 : 0.0;
		fprintf( file, "%u\t%.9f\t%.6f\t%.9f\n",
			static_cast<unsigned int>( i ),
			milliseconds, fps, absDelta );
		previous = seconds;
		havePrevious = true;
	}

	fflush( file );
	fclose( file );

	gEngfuncs.Con_Printf(
		"D3D9 frame profiler: full report written to %s\n", filename );
}

static void R_DebugFinish( double now )
{
	const double measuredSeconds = std::max( g_frameDebugCapture.sumFrameSeconds,
		now - g_frameDebugCapture.startTime );
	const unsigned int frames = g_frameDebugCapture.frameCount;

	gEngfuncs.Con_Printf( "\nD3D9 frame profiler over %.3f seconds:\n",
		measuredSeconds );

	if( frames == 0 || g_frameDebugCapture.sumFrameSeconds <= 0.0 )
	{
		gEngfuncs.Con_Printf( "  no rendered frames were sampled.\n" );
		g_frameDebugCapture.active = false;
		return;
	}

	const double avgFrameSeconds = g_frameDebugCapture.sumFrameSeconds /
		static_cast<double>( frames );
	const double variance = std::max( 0.0,
		( g_frameDebugCapture.sumFrameSecondsSq / static_cast<double>( frames ) ) -
		avgFrameSeconds * avgFrameSeconds );
	const double stddevSeconds = sqrt( variance );
	const double smoothness = avgFrameSeconds > 0.0 ?
		std::max( 0.0, std::min( 100.0,
			100.0 * ( 1.0 - stddevSeconds / avgFrameSeconds ) ) ) : 0.0;

	const double minMs = g_frameDebugCapture.minFrameSeconds * 1000.0;
	const double maxMs = g_frameDebugCapture.maxFrameSeconds * 1000.0;
	const double avgMs = avgFrameSeconds * 1000.0;
	const double jitterMs = stddevSeconds * 1000.0;
	const double avgFps = avgFrameSeconds > 0.0 ? 1.0 / avgFrameSeconds : 0.0;
	const double minFps = g_frameDebugCapture.maxFrameSeconds > 0.0 ?
		1.0 / g_frameDebugCapture.maxFrameSeconds : 0.0;
	const double maxFps = g_frameDebugCapture.minFrameSeconds > 0.0 ?
		1.0 / g_frameDebugCapture.minFrameSeconds : 0.0;

	double p50Seconds = avgFrameSeconds;
	double p95Seconds = avgFrameSeconds;
	double p99Seconds = avgFrameSeconds;
	double p999Seconds = avgFrameSeconds;
	double onePercentLowFps = avgFps;
	double pointOnePercentLowFps = avgFps;
	unsigned int slow125 = 0;
	unsigned int slow150 = 0;
	unsigned int slow200 = 0;
	if( !g_frameDebugCapture.frameTimes.empty( ) )
	{
		std::vector<double> sortedFrames = g_frameDebugCapture.frameTimes;
		std::sort( sortedFrames.begin( ), sortedFrames.end( ) );
		const auto percentile = [&]( double pct ) -> double
			{
				if( sortedFrames.empty( ) ) return avgFrameSeconds;
				const double scaled = pct * static_cast<double>( sortedFrames.size( ) - 1 );
				const size_t index = static_cast<size_t>( std::max( 0.0,
					std::min( scaled, static_cast<double>( sortedFrames.size( ) - 1 ) ) ) );
				return sortedFrames[index];
			};
		p50Seconds = percentile( 0.50 );
		p95Seconds = percentile( 0.95 );
		p99Seconds = percentile( 0.99 );
		p999Seconds = percentile( 0.999 );

		const auto lowFpsAverage = [&]( double worstFraction ) -> double
			{
				if( sortedFrames.empty( ) ) return avgFps;
				size_t count = static_cast<size_t>(
					ceil( worstFraction * static_cast<double>( sortedFrames.size( ) ) ) );
				count = std::max<size_t>( 1, std::min( count, sortedFrames.size( ) ) );
				double sum = 0.0;
				for( size_t i = sortedFrames.size( ) - count; i < sortedFrames.size( ); ++i )
					sum += sortedFrames[i];
				const double avgWorstFrame = sum / static_cast<double>( count );
				return avgWorstFrame > 0.0 ? 1.0 / avgWorstFrame : 0.0;
			};
		onePercentLowFps = lowFpsAverage( 0.01 );
		pointOnePercentLowFps = lowFpsAverage( 0.001 );

		for( double frame : sortedFrames )
		{
			if( frame > avgFrameSeconds * 1.25 ) ++slow125;
			if( frame > avgFrameSeconds * 1.50 ) ++slow150;
			if( frame > avgFrameSeconds * 2.00 ) ++slow200;
		}
	}

	const double avgAbsDeltaMs = frames > 1 ?
		1000.0 * g_frameDebugCapture.sumAbsFrameDeltaSeconds /
		static_cast<double>( frames - 1 ) : 0.0;
	const double maxAbsDeltaMs = 1000.0 * g_frameDebugCapture.maxAbsFrameDeltaSeconds;

	gEngfuncs.Con_Printf( "  frames sampled: %u\n", frames );
	gEngfuncs.Con_Printf( "  frame time: min %.3f ms | max %.3f ms | avg %.3f ms | jitter %.3f ms\n",
		minMs, maxMs, avgMs, jitterMs );
	gEngfuncs.Con_Printf( "  framerate:  min %.1f fps | max %.1f fps | avg %.1f fps\n",
		minFps, maxFps, avgFps );
	gEngfuncs.Con_Printf( "  percentiles: p50 %.3f ms | p95 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n",
		p50Seconds * 1000.0, p95Seconds * 1000.0,
		p99Seconds * 1000.0, p999Seconds * 1000.0 );
	gEngfuncs.Con_Printf( "  lows:       1%% low %.1f fps | 0.1%% low %.1f fps\n",
		onePercentLowFps, pointOnePercentLowFps );
	gEngfuncs.Con_Printf( "  frame delta: avg |dT| %.3f ms | max |dT| %.3f ms\n",
		avgAbsDeltaMs, maxAbsDeltaMs );
	gEngfuncs.Con_Printf( "  slow frames: >1.25x avg %u | >1.5x avg %u | >2x avg %u\n",
		slow125, slow150, slow200 );
	gEngfuncs.Con_Printf( "  smoothness: %.1f%%  (100%% = perfectly even frame times)\n",
		smoothness );
	gEngfuncs.Con_Printf( "  stutter frames: %u  (frames slower than avg + 2*jitter)\n",
		g_frameDebugCapture.stutterFrames );

	const double coarseRendererSeconds =
		g_frameDebugCapture.phases[DX_PROFILE_BEGINFRAME].sumSeconds +
		g_frameDebugCapture.phases[DX_PROFILE_SCENE_SETUP].sumSeconds +
		g_frameDebugCapture.phases[DX_PROFILE_DRAWENTS].sumSeconds +
		g_frameDebugCapture.phases[DX_PROFILE_ENDSCENE].sumSeconds +
		g_frameDebugCapture.phases[DX_PROFILE_PRESENT].sumSeconds +
		g_frameDebugCapture.phases[DX_PROFILE_ENDFRAME_MISC].sumSeconds;
	const double avgCoarseRendererMs = 1000.0 * coarseRendererSeconds /
		static_cast<double>( frames );
	const double avgUnaccountedMs = std::max( 0.0, avgMs - avgCoarseRendererMs );
	gEngfuncs.Con_Printf( "  coarse renderer time: %.3f ms avg | unaccounted/outside renderer: %.3f ms avg\n",
		avgCoarseRendererMs, avgUnaccountedMs );

	const double avgPresentMs = 1000.0 *
		g_frameDebugCapture.phases[DX_PROFILE_PRESENT].sumSeconds /
		static_cast<double>( frames );
	const double worstPresentMs = 1000.0 *
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_PRESENT];
	const double worstRendererMs = 1000.0 * (
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_BEGINFRAME] +
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_SCENE_SETUP] +
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_DRAWENTS] +
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_ENDSCENE] +
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_PRESENT] +
		g_frameDebugCapture.worstFramePhases[DX_PROFILE_ENDFRAME_MISC] );
	const double worstOutsideMs = std::max( 0.0, maxMs - worstRendererMs );
	gEngfuncs.Con_Printf( "  responsibility estimate: Present %.3f ms avg / %.3f ms worst | outside %.3f ms avg / %.3f ms worst\n",
		avgPresentMs, worstPresentMs, avgUnaccountedMs, worstOutsideMs );
	if( avgUnaccountedMs > avgMs * 0.25 || worstOutsideMs > maxMs * 0.25 )
	{
		gEngfuncs.Con_Printf( "    note: a large outside-renderer slice points at engine/client simulation, audio/input, OS scheduling, overlays, or another hook.\n" );
	}
	if( avgPresentMs > avgMs * 0.20 || worstPresentMs > maxMs * 0.40 )
	{
		gEngfuncs.Con_Printf( "    note: a large Present slice points at driver/GPU/RTX Remix synchronization rather than CPU scene submission.\n" );
	}

	R_DebugPrintDiagnosis( frames, avgMs );
	R_DebugPrintPhases( frames );
	R_DebugPrintCounters( frames );
	gEngfuncs.Con_Printf( "\n" );

	R_DebugWriteReport(
		measuredSeconds, frames,
		minMs, maxMs, avgMs, jitterMs,
		minFps, maxFps, avgFps,
		p50Seconds * 1000.0, p95Seconds * 1000.0,
		p99Seconds * 1000.0, p999Seconds * 1000.0,
		onePercentLowFps, pointOnePercentLowFps,
		avgAbsDeltaMs, maxAbsDeltaMs,
		slow125, slow150, slow200,
		smoothness, avgCoarseRendererMs,
		avgUnaccountedMs, avgPresentMs,
		worstPresentMs, worstRendererMs, worstOutsideMs );

	g_frameDebugCapture.active = false;
}

void DX_ProfilerSampleFrame( void )
{
	if( !g_frameDebugCapture.active )
		return;

	const double now = DX_Now( );
	double frameSeconds = now - g_frameDebugCapture.lastSampleTime;
	g_frameDebugCapture.lastSampleTime = now;

	if( frameSeconds > 0.0 && frameSeconds < 10.0 )
	{
		g_frameDebugCapture.minFrameSeconds = std::min(
			g_frameDebugCapture.minFrameSeconds, frameSeconds );
		g_frameDebugCapture.maxFrameSeconds = std::max(
			g_frameDebugCapture.maxFrameSeconds, frameSeconds );
		g_frameDebugCapture.sumFrameSeconds += frameSeconds;
		g_frameDebugCapture.sumFrameSecondsSq += frameSeconds * frameSeconds;
		if( g_frameDebugCapture.hasPreviousFrameSeconds )
		{
			const double delta = fabs( frameSeconds - g_frameDebugCapture.previousFrameSeconds );
			g_frameDebugCapture.sumAbsFrameDeltaSeconds += delta;
			g_frameDebugCapture.maxAbsFrameDeltaSeconds = std::max(
				g_frameDebugCapture.maxAbsFrameDeltaSeconds, delta );
		}
		g_frameDebugCapture.previousFrameSeconds = frameSeconds;
		g_frameDebugCapture.hasPreviousFrameSeconds = true;
		g_frameDebugCapture.frameTimes.push_back( frameSeconds );
		++g_frameDebugCapture.frameCount;

		if( g_frameDebugCapture.frameCount > 8 )
		{
			const double avg = g_frameDebugCapture.sumFrameSeconds /
				static_cast<double>( g_frameDebugCapture.frameCount );
			const double var = std::max( 0.0,
				( g_frameDebugCapture.sumFrameSecondsSq /
					static_cast<double>( g_frameDebugCapture.frameCount ) ) - avg * avg );
			const double dev = sqrt( var );
			if( frameSeconds > avg + 2.0 * dev )
				++g_frameDebugCapture.stutterFrames;
		}

		DX_ProfileAccumulate( frameSeconds );
	}

	if( now - g_frameDebugCapture.startTime >= g_frameDebugCapture.duration )
		R_DebugFinish( now );
}
