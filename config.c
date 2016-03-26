#include <stdio.h>
#include <limits.h>
#define  NOT_INCLUDE_CONFIG_H
#include "mppdec.h"


static void
version ( FILE* fp )
{
    FILE*  fpi = fopen ("version", "r");
    char   buff [256];
    char   version [16];

    fprintf ( fp, "/* parsed values from file \"version\" */\n\n" );
    while ( fgets ( buff, sizeof buff, fpi ) ) {
        if ( 1 == sscanf ( buff, "MPPDEC_VERSION=%15[0-9.a-z]", version ) ) {
            fprintf ( fp, "#ifndef MPPDEC_VERSION\n# define MPPDEC_VERSION   \"%s\"\n#endif\n\n", version );
            switch ( version[3]-'0' ) {
            case 1: case 3: case 5: case 7: case 9:
                fprintf ( fp, "#define MPPDEC_BUILD  \"--Alpha--\"\n\n" );
                break;
            case 2: case 4: case 6: case 8:
                fprintf ( fp, "#define MPPDEC_BUILD  \"-Beta-\"\n\n" );
                break;
            case 0:
                fprintf ( fp, "#define MPPDEC_BUILD  \"Release\"\n\n" );
                break;
            }
        }
        else
        if ( 1 == sscanf ( buff, "MPPENC_VERSION=%15[0-9.a-z]", version ) ) {
            fprintf ( fp, "#ifndef MPPENC_VERSION\n# define MPPENC_VERSION   \"%s\"\n#endif\n\n", version );
            switch ( version[3]-'0' ) {
            case 1: case 3: case 5: case 7: case 9:
                fprintf ( fp, "#define MPPENC_BUILD  \"--Alpha--\"\n\n" );
                break;
            case 2: case 4: case 6: case 8:
                fprintf ( fp, "#define MPPENC_BUILD  \"-Beta-\"\n\n" );
                break;
            case 0:
                fprintf ( fp, "#define MPPENC_BUILD  \"Release\"\n\n" );
                break;
            }
        }
    }

    fclose (fpi);

}



unsigned long   v_lng = (unsigned long ) 0x8877665544332211L;
unsigned short  v_sht = (unsigned short) 0x8877665544332211L;
unsigned int    v_int = (unsigned int  ) 0x8877665544332211L;

unsigned char*  lo = (unsigned char*) &v_lng;
unsigned char*  sh = (unsigned char*) &v_sht;
unsigned char*  in = (unsigned char*) &v_int;

#define ROUND32_1(x)   ( floattmp = (x) + (Int32_t)0x00FF8000L, *(Int32_t*)(&floattmp) - (Int32_t)0x4B7F8000L )
#define ROUND32_2(x)   ( (Int32_t) floor ((x) + 0.5) )

#define ROUND64_1(x)   ( doubletmp = (x) + (Int64_t)0x001FFFFF80000000L, *(Int64_t*)(&doubletmp) - (Int64_t)0x433FFFFF80000000L )
#define ROUND64_2(x)   ( (Int64_t) floor ((x) + 0.5) )


int
test_round_16 ( float x )
{
    float    floattmp;
    Int32_t  tmp1 = ROUND32_1 (x);
    Int32_t  tmp2 = ROUND32_2 (x);

    if ( tmp1 != tmp2 )
        return 1;

    return 0;
}


int
test_round_32 ( double x )
{
#ifdef NO_INT64_T
    return 1;
#else
    double   doubletmp;
    Int64_t  tmp1 = ROUND64_1 (x);
    Int64_t  tmp2 = ROUND64_2 (x);

    if ( tmp1 != tmp2 )
        return 1;

    return 0;
#endif
}


static void
Convert_to_80bit_BE_IEEE854_Float ( unsigned char* p, long double val )
{
    unsigned long  word32 = 0x401E;

    if ( val > 0.L )
        while ( val < (long double)0x80000000 )
            word32--, val *= 2.L;

    *p++   = (Uint8_t)(word32 >>  8);
    *p++   = (Uint8_t)(word32 >>  0);
    word32 = (Uint32_t) val;
    *p++   = (Uint8_t)(word32 >> 24);
    *p++   = (Uint8_t)(word32 >> 16);
    *p++   = (Uint8_t)(word32 >>  8);
    *p++   = (Uint8_t)(word32 >>  0);
    word32 = (Uint32_t) ( (val - word32) * 4294967296.L );
    *p++   = (Uint8_t)(word32 >> 24);
    *p++   = (Uint8_t)(word32 >> 16);
    *p++   = (Uint8_t)(word32 >>  8);
    *p++   = (Uint8_t)(word32 >>  0);
}


int
test_long_double ( int endian )
{
    long double           val;
    unsigned char         p [10];
    const unsigned char*  q = (const unsigned char*) & val;
    int                   i;

    if ( sizeof (val) < 10  ||  sizeof(val) > 16 )
        return 0;
    if ( endian == 0 )
        return 0;

    for ( val = 4.29e9; val >= 1.e-35; val *= 0.999 ) {
        Convert_to_80bit_BE_IEEE854_Float ( p, val );
        for ( i = 0; i < 10; i++ )
            if ( p[i] != q[endian==2 ? i : 9-i] ) {
                printf ("%40.30Lf ", val );
                for ( i = 0; i<10; i++ ) printf ("%02X %02X  ", p[i], q[endian==2 ? i : 9-i] );
                printf ("\n");
                return 0;
            }
    }

    return 1;
}


int
main ( int argc, char** argv )
{
    int           flag = 0;  // Bit 0: little, Bit 1: big, Bit 2: unknown
    unsigned int  k;
    long          m;
    FILE*         fp   = fopen ( "config.h", "w" );
    int           endian;

    if ( fp == NULL ) {
        fprintf ( stderr, "config: Can't write 'config.h'\n");
        return 1;
    }

    if ( argc > 1 )
        fprintf ( stderr, "\n*** Compile sources with ***\n\n%s\n\n", argv[1] );

    if ( argc > 2 )
        fprintf ( stderr, "\n*** Execute binary with ***\n\n%s\n\n", argv[2] );

    fprintf ( fp, "\n" );
    fprintf ( fp, "/* Determine Endianess of the machine */\n" );
    fprintf ( fp, "\n" );
    fprintf ( fp, "#define HAVE_LITTLE_ENDIAN  1234\n" );
    fprintf ( fp, "#define HAVE_BIG_ENDIAN     4321\n" );
    fprintf ( fp, "\n" );

        flag = 0;
    for ( k = 0; k < sizeof(v_int); k++ )
        if      ( in[k] == 0x11 * (k+1)             ) flag |= 1;
        else if ( in[k] == 0x11 * (sizeof(v_int)-k) ) flag |= 2;
        else                                          flag |= 4;

    for ( k = 0; k < sizeof(v_lng); k++ )
        if      ( lo[k] == 0x11 * (k+1)             ) flag |= 1;
        else if ( lo[k] == 0x11 * (sizeof(v_lng)-k) ) flag |= 2;
        else                                          flag |= 4;

    for ( k = 0; k < sizeof(v_sht); k++ )
        if      ( sh[k] == 0x11 * (k+1)             ) flag |= 1;
        else if ( sh[k] == 0x11 * (sizeof(v_sht)-k) ) flag |= 2;
        else                                          flag |= 4;

    switch (flag) {
    case 1:
        endian = 1;
        fprintf ( fp, "#define ENDIAN              HAVE_LITTLE_ENDIAN\n" );
        break;
    case 2:
        endian = 2;
        fprintf ( fp, "#define ENDIAN              HAVE_BIG_ENDIAN\n" );
        break;
    default:
        endian = 0;
        fprintf ( fp, "/* unknown endianess */\n" );
        break;

    }
    fprintf ( fp, "\n" );


    fprintf ( fp, "\n" );
    fprintf ( fp, "/* Test the fast float-to-int rounding trick works */\n" );
    fprintf ( fp, "\n" );
    flag = 0;
    for ( m = 0; m <= 32767; m++ ) {
        flag |= test_round_16 ( (float)(+m - 0.499) );
        flag |= test_round_16 ( (float)(+m        ) );
        flag |= test_round_16 ( (float)(+m + 0.499) );
        flag |= test_round_16 ( (float)(-m - 0.499) );
        flag |= test_round_16 ( (float)(-m        ) );
        flag |= test_round_16 ( (float)(-m + 0.499) );
    }
    if ( flag == 0 )
        fprintf ( fp, "#define HAVE_IEEE754_FLOAT\n" );
    else
        fprintf ( fp, "/* #define HAVE_IEEE754_FLOAT */\n" );


    flag = 0;
    for ( m = 0; (unsigned long)m <= 0x7FFFFFFF; m += 1 + (m>>16) ) {
        flag |= test_round_32 ( +m - 0.499 );
        flag |= test_round_32 ( +m         );
        flag |= test_round_32 ( +m + 0.499 );
        flag |= test_round_32 ( -m - 0.499 );
        flag |= test_round_32 ( -m         );
        flag |= test_round_32 ( -m + 0.499 );
    }
    if ( flag == 0 )
        fprintf ( fp, "#define HAVE_IEEE754_DOUBLE\n" );
    else
        fprintf ( fp, "/* #define HAVE_IEEE754_DOUBLE */\n" );
    fprintf ( fp, "\n" );


    fprintf ( fp, "\n" );
    fprintf ( fp, "/* Test the presence of a 80 bit floating point type for writing AIFF headers */\n" );
    fprintf ( fp, "\n" );
    if ( test_long_double(endian) )
        fprintf ( fp, "#define HAVE_IEEE854_LONGDOUBLE\n" );
    else
        fprintf ( fp, "/* #define HAVE_IEEE854_LONGDOUBLE */\n" );


    fprintf ( fp, "\n\n" );
    version ( fp );
    fprintf ( fp, "/* end of config.h */\n" );
    fclose (fp);
    return 0;
}

/* end of config.h */
