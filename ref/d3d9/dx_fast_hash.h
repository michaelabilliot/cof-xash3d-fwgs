#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>

#if defined(_MSC_VER) && defined(_M_X64) && !defined(DXFAST_FORCE_PORTABLE_MUL128)
#include <intrin.h>
#endif

/* Enable emhash's pointer-returning try_get() extension.  It avoids building
 * iterators on cold fallback lookups and costs nothing when unused. */
#ifndef EMH_EXT
#define EMH_EXT 1
#endif

#if defined(__has_include)
#  if __has_include("emhash/hash_table7.hpp")
#    include "emhash/hash_table7.hpp"
#  elif __has_include("hash_table7.hpp")
#    include "hash_table7.hpp"
#  else
#    error "emhash7 hash_table7.hpp not found. Add include/emhash/hash_table7.hpp to the project include path or place hash_table7.hpp beside dx_fast_hash.h."
#  endif
#else
#  include "emhash/hash_table7.hpp"
#endif

namespace dxfast
{
	/* mumx's 64-bit integer hash is the xor of the high and low halves of a
	 * 64x64->128 multiply. Keep a portable implementation because this renderer
	 * is also built as Win32, where MSVC does not expose unsigned __int128. */
	static constexpr std::uint64_t MUMX_MULTIPLIER = UINT64_C( 0x2ca7aea0ebd71d49 );

	static inline std::uint64_t Mul128Xor( std::uint64_t a, std::uint64_t b ) noexcept
	{
#if defined(_MSC_VER) && defined(_M_X64) && !defined(DXFAST_FORCE_PORTABLE_MUL128)
		unsigned __int64 high = 0;
		const unsigned __int64 low = _umul128( a, b, &high );
		return static_cast<std::uint64_t>( low ^ high );
#elif defined(__SIZEOF_INT128__) && !defined(DXFAST_FORCE_PORTABLE_MUL128)
		const unsigned __int128 product =
			static_cast<unsigned __int128>( a ) * static_cast<unsigned __int128>( b );
		return static_cast<std::uint64_t>( product ) ^
			static_cast<std::uint64_t>( product >> 64 );
#else
		/* Portable 32-bit-limb product for MSVC Win32 and other compilers without
		 * a native 128-bit integer type. */
		const std::uint64_t a0 = static_cast<std::uint32_t>( a );
		const std::uint64_t a1 = a >> 32;
		const std::uint64_t b0 = static_cast<std::uint32_t>( b );
		const std::uint64_t b1 = b >> 32;

		const std::uint64_t p00 = a0 * b0;
		const std::uint64_t p01 = a0 * b1;
		const std::uint64_t p10 = a1 * b0;
		const std::uint64_t p11 = a1 * b1;

		const std::uint64_t middle = ( p00 >> 32 ) +
			static_cast<std::uint32_t>( p01 ) + static_cast<std::uint32_t>( p10 );
		const std::uint64_t low = ( p00 & UINT64_C( 0xffffffff ) ) |
			( middle << 32 );
		const std::uint64_t high = p11 + ( p01 >> 32 ) + ( p10 >> 32 ) +
			( middle >> 32 );
		return low ^ high;
#endif
	}

	static inline std::uint64_t Mumx64( std::uint64_t value ) noexcept
	{
		return Mul128Xor( value, MUMX_MULTIPLIER );
	}

	static inline std::size_t FoldHash( std::uint64_t value ) noexcept
	{
		if constexpr( sizeof( std::size_t ) < sizeof( std::uint64_t ) )
			value ^= value >> 32;
		return static_cast<std::size_t>( value );
	}

	static inline std::size_t MumxHash64( std::uint64_t value ) noexcept
	{
		return FoldHash( Mumx64( value ) );
	}

	static inline std::size_t MumxHashPair64( std::uint64_t first,
		std::uint64_t second ) noexcept
	{
		const std::uint64_t mixedSecond = Mumx64(
			second + UINT64_C( 0x9e3779b97f4a7c15 ) );
		return FoldHash( Mumx64( first ^ mixedSecond ) );
	}

	/* Generic mumx wrapper: integral/enum and pointer keys get mumx mixing;
	 * non-integral keys (notably std::string) retain std::hash semantics. */
	template<typename T, typename Enable = void>
	struct MumxHash
	{
		std::size_t operator()( const T &value ) const
			noexcept( noexcept( std::hash<T>{}( value ) ) )
		{
			return std::hash<T>{}( value );
		}
	};

	template<typename T>
	struct MumxHash<T, std::enable_if_t<std::is_integral_v<T>>>
	{
		std::size_t operator()( T value ) const noexcept
		{
			std::uint64_t raw = 0;
			if constexpr( std::is_same_v<std::remove_cv_t<T>, bool> )
				raw = value ? 1u : 0u;
			else
				raw = static_cast<std::uint64_t>(
					static_cast<std::make_unsigned_t<T>>( value ) );
			return MumxHash64( raw );
		}
	};

	template<typename T>
	struct MumxHash<T, std::enable_if_t<std::is_enum_v<T>>>
	{
		std::size_t operator()( T value ) const noexcept
		{
			using underlying_t = std::underlying_type_t<T>;
			using unsigned_t = std::make_unsigned_t<underlying_t>;
			return MumxHash64( static_cast<std::uint64_t>(
				static_cast<unsigned_t>( static_cast<underlying_t>( value ) ) ) );
		}
	};

	template<typename T>
	struct MumxHash<T *, void>
	{
		std::size_t operator()( const T *value ) const noexcept
		{
			/* Renderer pointers are naturally aligned.  A full mumx 64x64->128
			 * multiply is unnecessary on this hot path and was measurably more work
			 * than the lookup it replaced on tiny maps.  Remove the guaranteed-zero
			 * alignment bits, fold upper address bits, then use one Fibonacci-style
			 * multiply so emhash's low-bit bucket mask still receives well-distributed
			 * bits.  Integer/enum keys continue to use real mumx above. */
			std::uintptr_t raw = reinterpret_cast<std::uintptr_t>( value );
			raw >>= 4;
			if constexpr( sizeof( std::uintptr_t ) >= 8 )
			{
				std::uint64_t x = static_cast<std::uint64_t>( raw );
				x ^= x >> 32;
				x *= UINT64_C( 0x9e3779b97f4a7c15 );
				x ^= x >> 29;
				return FoldHash( x );
			}
			else
			{
				std::uint32_t x = static_cast<std::uint32_t>( raw );
				x ^= x >> 16;
				x *= UINT32_C( 0x9e3779b1 );
				x ^= x >> 15;
				return static_cast<std::size_t>( x );
			}
		}
	};

	template<typename Key, typename Value,
		typename Hash = MumxHash<Key>,
		typename Equal = std::equal_to<Key>>
		using HashMap = emhash7::HashMap<Key, Value, Hash, Equal>;
}
