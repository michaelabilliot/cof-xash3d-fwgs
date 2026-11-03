#pragma once

#include <cstddef>

// Partition before visibility testing. Merging only adjacent ranges on the
// exact same plane keeps each draw's index sequence invariant as the eye moves.
template<class Ranges, class Emit>
void DX_ForEachStableGlassSpan( const Ranges &ranges, size_t total,
	bool batchCoplanar, Emit emit )
{
	for( size_t i = 0; i < ranges.size( ); )
	{
		const auto &plane = ranges[i++];
		const size_t first = plane.firstIndex;
		size_t count = plane.indexCount;
		if( !count || count % 3 || first > total || count > total - first )
			continue;
		while( batchCoplanar && i < ranges.size( ) )
		{
			const auto &next = ranges[i];
			if( next.firstIndex != first + count || !next.indexCount ||
				next.indexCount % 3 || next.indexCount > total - first - count ||
				next.planeDist != plane.planeDist ||
				next.normal[0] != plane.normal[0] ||
				next.normal[1] != plane.normal[1] ||
				next.normal[2] != plane.normal[2] )
				break;
			count += next.indexCount;
			++i;
		}
		emit( plane, first, count );
	}
}
