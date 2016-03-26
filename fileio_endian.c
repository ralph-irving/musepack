/*
 *  File IO endian handling functions
 *
 *  (C) Frank Klemm 2002. All rights reserved.
 *
 *  Principles:
 *    Do switch the endianess of 8, 16, 24, 32 and 64 items in-place,
 *    i.e. source and destination is the same.
 *
 *  History:
 *    2002-11-10    created
 *
 *  Global functions:
 *    - Calc_EndianSwapper
 *
 *  TODO:
 *    - May be rewrite the stuff using MMX / Altivec
 */

#include "fileio.h"
#include "fileio_endian.h"

static void
ChangeEndianess8 ( void* array, size_t elems )
{
    (void) array;
    (void) elems;
}


static void
ChangeEndianess16 ( void* array, size_t elems )
{
    unsigned char*  p;
    unsigned char   ch0;

    for ( p = (unsigned char*) array; elems--; p += 2 ) {
        ch0  = p[0];
        p[0] = p[1];
        p[1] = ch0;
    }
}


static void
ChangeEndianess24 ( void* array, size_t elems )
{
    unsigned char*  p;
    unsigned char   ch0;

    for ( p = (unsigned char*) array; elems--; p += 3 ) {
        ch0  = p[0];
        p[0] = p[2];
        p[2] = ch0;
    }
}


static void
ChangeEndianess32 ( void* array, size_t elems )
{
    unsigned char*  p;
    unsigned char   ch0;
    unsigned char   ch1;

    for ( p = (unsigned char*) array; elems--; p += 4 ) {
        ch0  = p[0];
        ch1  = p[1];
        p[0] = p[3];
        p[1] = p[2];
        p[3] = ch0;
        p[2] = ch1;
    }
}


static void
ChangeEndianess64 ( void* array, size_t elems )
{
    unsigned char*  p;
    unsigned char   ch0;
    unsigned char   ch1;
    unsigned char   ch2;
    unsigned char   ch3;

    for ( p = (unsigned char*) array; elems--; p += 8 ) {
        ch0  = p[0];
        ch1  = p[1];
        ch2  = p[2];
        ch3  = p[3];
        p[0] = p[7];
        p[1] = p[6];
        p[2] = p[5];
        p[3] = p[4];
        p[7] = ch0;
        p[6] = ch1;
        p[5] = ch2;
        p[4] = ch3;
    }
}


void (* Calc_EndianSwapper (endian_t endianess, unsigned int bits) ) (void *, size_t)  // Don't bite into the carpenter !!!
//^^^^                                                             ^^^^^^^^^^^^^^^^^^  // This is the return type, K&R must be crazy when designing the C syntax
{
    static uint8_t  test [4] = { 0x12, 0x34, 0x56, 0x78 };
    unsigned int    Bytes    = ( bits + 7 ) / 8;

    if ( ( *(uint32_t*)test == 0x12345678  &&  endianess == Endian_big )  ||  ( *(uint32_t*)test == 0x78563412  &&  endianess == Endian_little ) )
        return NULL;

    if ( ( *(uint32_t*)test == 0x78563412  &&  endianess == Endian_big )  ||  ( *(uint32_t*)test == 0x12345678  &&  endianess == Endian_little ) )
        switch ( Bytes ) {
        case  0:
        case  1: return NULL;
        case  2: return & ChangeEndianess16;
        case  3: return & ChangeEndianess24;
        case  4: return & ChangeEndianess32;
        case  8: return & ChangeEndianess64;
        }

    return (void*)-1;
}



/*
 *  Write a 80 bit IEEE854 big endian number as 10 octets. Destination is passed as pointer,
 *  End of destination (p+10) is returned.
 */
uint8_t*
Write_to_80bit_BE_IEEE854_Float ( uint8_t* p, long double val )
{
#ifndef HAVE_IEEE854_LONGDOUBLE
    uint32_t  word32 = 0x401E;

    if ( val > 0.L )
        while ( val < (long double)0x80000000 )                // scales value in the range 2^31...2^32
            word32--, val *= 2.L;                              // so you have the exponent

    *p++   = (uint8_t)(word32 >>  8);
    *p++   = (uint8_t)(word32 >>  0);                          // write exponent, sign is assumed as '+'
    word32 = (uint32_t) val;
    *p++   = (uint8_t)(word32 >> 24);
    *p++   = (uint8_t)(word32 >> 16);
    *p++   = (uint8_t)(word32 >>  8);
    *p++   = (uint8_t)(word32 >>  0);                          // write the upper 32 bit of the mantissa
    word32 = (uint32_t) ( (val - word32) * 4294967296.L );
    *p++   = (uint8_t)(word32 >> 24);
    *p++   = (uint8_t)(word32 >> 16);
    *p++   = (uint8_t)(word32 >>  8);
    *p++   = (uint8_t)(word32 >>  0);                          // write the lower 32 bit of the mantissa
#elif ENDIAN == HAVE_LITTLE_ENDIAN
    const uint8_t*  q = (uint8_t*) &val;

    *p++ = q[9];                                               // only change the endianess
    *p++ = q[8];
    *p++ = q[7];
    *p++ = q[6];
    *p++ = q[5];
    *p++ = q[4];
    *p++ = q[3];
    *p++ = q[2];
    *p++ = q[1];
    *p++ = q[0];
#elif defined MUST_ALIGNED
    const uint8_t*  q = (uint8_t*) &val;

    *p++ = q[0];                                               // only copy
    *p++ = q[1];
    *p++ = q[2];
    *p++ = q[3];
    *p++ = q[4];
    *p++ = q[5];
    *p++ = q[6];
    *p++ = q[7];
    *p++ = q[8];
    *p++ = q[9];
#else
    *(Ldouble*)p = val;                                        // copy directly
    p += 10;
#endif /* HAVE_IEEE854_LONGDOUBLE */

    return p;
}


const uint8_t*
Read_from_80bit_BE_IEEE854_Float ( const uint8_t* p, long double* val )
{
    /*
#ifndef HAVE_IEEE854_LONGDOUBLE
    uint16_t  expo;
    uint32_t  mant1;
    uint32_t  mant2;

    expo  = 256 * p[0] + p[1];
    mant1 = 16777216LU * p[2] + 65536LU * p[3] + 256LU * p[4] + p[5];
    mant2 = 16777216LU * p[6] + 65536LU * p[7] + 256LU * p[8] + p[9];

    *val = mant1 * 4294967296.L + mant2;

    while ( expo > 0x40.. )
        *val *= 2.0L, expo--;
    while ( expo < 0x40.. )
        *val *= 0.5L, expo++;
#elif ENDIAN == HAVE_LITTLE_ENDIAN
    const uint8_t*  q = (uint8_t*) val;

    q[9] = *p++;                                               // only change the endianess
    q[8] = *p++;
    q[7] = *p++;
    q[6] = *p++;
    q[5] = *p++;
    q[4] = *p++;
    q[3] = *p++;
    q[2] = *p++;
    q[1] = *p++;
    q[0] = *p++;
#elif defined MUST_ALIGNED
    const uint8_t*  q = (uint8_t*) val;

    q[0] = *p++;                                               // only copy
    q[1] = *p++;
    q[2] = *p++;
    q[3] = *p++;
    q[4] = *p++;
    q[5] = *p++;
    q[6] = *p++;
    q[7] = *p++;
    q[8] = *p++;
    q[9] = *p++;
#else
    *val = *(Ldouble*)p;                                        // copy directly
    p += 10;
#endif */ /* HAVE_IEEE854_LONGDOUBLE */

    return p;
}

/* end of fileio-endian.c */
