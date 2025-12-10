// new and improved

#ifdef BUILDAVX512
#include "crc32_AVX512.h"
#endif

#ifdef _MSC_VER
    #define __restrict__ __restrict
#endif

#include <cstdint>

class crc32_sb16 {
public:
    static uint32_t update( const void* data, const size_t& length ) {
#ifdef BUILDAVX512
        static bool AVX512 = have_vpclmulqdq();
        if ( AVX512 ) {
            return crc32_avx512<true>( 0, static_cast< const char* >(data), length, refl32 );
        }
#endif

        // Static variables initializes once making them perfect for reducing overhead
        static uint32_t table[ 16 ][ 256 ];
        static bool init = [] {
            generate_table( table );
            return true;
        }();

        const uint8_t* __restrict__ p = ( const uint8_t* )data;

        uint32_t crc = 0xFFFFFFFFu;

        const uint8_t* end = p + length;

        // Process 16 bytes at a time
        while ( p + 16 <= end ) {
            crc ^= *( const uint32_t* )p;
            const uint32_t d1 = *( const uint32_t* )(p + 4);
            const uint32_t d2 = *( const uint32_t* )(p + 8);
            const uint32_t d3 = *( const uint32_t* )(p + 12);

            crc =
                // From d0
                (table[ 15 ][ (crc) & 0xFF ] ^
                table[ 14 ][ (crc >> 8) & 0xFF ] ^
                table[ 13 ][ (crc >> 16) & 0xFF ] ^
                table[ 12 ][ (crc >> 24) & 0xFF ]) ^
                // From d1
                (table[ 11 ][ (d1) & 0xFF ] ^
                table[ 10 ][ (d1 >> 8) & 0xFF ] ^
                table[ 9 ][ (d1 >> 16) & 0xFF ] ^
                table[ 8 ][ (d1 >> 24) & 0xFF ]) ^
                // From d2
                (table[ 7 ][ (d2) & 0xFF ] ^
                table[ 6 ][ (d2 >> 8) & 0xFF ] ^
                table[ 5 ][ (d2 >> 16) & 0xFF ] ^
                table[ 4 ][ (d2 >> 24) & 0xFF ]) ^
                // From d3
                (table[ 3 ][ (d3) & 0xFF ] ^
                table[ 2 ][ (d3 >> 8) & 0xFF ] ^
                table[ 1 ][ (d3 >> 16) & 0xFF ] ^
                table[ 0 ][ (d3 >> 24) & 0xFF ]);

            p += 16;
        }

        if ( p + 8 <= end ) {
            crc ^= *( const uint32_t* )p;
            const uint32_t d1 = *( const uint32_t* )(p + 4);

            crc =
                (table[ 7 ][ (crc) & 0xFF ] ^
                table[ 6 ][ (crc >> 8) & 0xFF ] ^
                table[ 5 ][ (crc >> 16) & 0xFF ] ^
                table[ 4 ][ (crc >> 24) & 0xFF ]) ^

                (table[ 3 ][ (d1) & 0xFF ] ^
                table[ 2 ][ (d1 >> 8) & 0xFF ] ^
                table[ 1 ][ (d1 >> 16) & 0xFF ] ^
                table[ 0 ][ (d1 >> 24) & 0xFF ]);

            p += 8;
        }

        if ( p + 4 <= end ) {
            crc ^= *( const uint32_t* )p;
            crc =
                (table[ 3 ][ (crc) & 0xFF ] ^
                table[ 2 ][ (crc >> 8) & 0xFF ] ^
                table[ 1 ][ (crc >> 16) & 0xFF ] ^
                table[ 0 ][ (crc >> 24) & 0xFF ]);

            p += 4;
        }

        // Remaining bytes
        while ( p < end )
            crc = table[ 0 ][ (crc ^ *p++) & 0xFF ] ^ (crc >> 8);

        return crc ^ 0xFFFFFFFFu;
    }
private:
    static constexpr void generate_table( uint32_t( &table )[ 16 ][ 256 ] ) {
        const uint32_t poly = 0xEDB88320;

        // Table[0]: standard CRC-32 table
        for ( uint32_t i = 0; i < 256; ++i ) {
            uint32_t c = i;
            for ( int j = 0; j < 8; ++j )
                c = (c >> 1) ^ (-( int )(c & 1) & poly);
            table[ 0 ][ i ] = c;
        }

        // Table[1..15]
        for ( int t = 1; t < 16; ++t ) {
            for ( int i = 0; i < 256; ++i ) {
                uint32_t c = table[ t - 1 ][ i ];
                table[ t ][ i ] = table[ 0 ][ c & 0xFF ] ^ (c >> 8);
            }
        }
    }
};
