/*
 *  Clear Vocal Detection functions
 *
 *  (C) Frank Klemm 2002. All rights reserved.
 *
 *  Principles:
 *
 *  History:
 *    ca. 1998    created
 *    2002
 *
 *  Global functions:
 *    -
 *
 *  TODO:
 *    -
 */


#include "mppenc.h"


/*
 *  SV1:   DATE 13.12.1998
 *  SV2:   DATE 12.06.1999
 *  SV3:   DATE 19.10.1999
 *  SV4:   DATE 20.10.1999
 *  SV5:   DATE 18.06.2000
 *  SV6:   DATE 10.08.2000
 *  SV7:   DATE 23.08.2000
 *  SV7.F: DATE 20.07.2002
 *  SV7.F: DATE 18.01.2003
 *  SVF.F: DATE 27.01.2003
 */

/*
    ------
     8 bit Versionsnummer
    24 bit +PM
    ------
    16 bit Size of Header (the following bytes = 26 )
    16 bit Subbandwidth * 32
    ------
    32 bit Musepack Frames
    ------
     6 bit maximum used number of subbands
     1 bit MS used ?
     9 bit -
    16 bit Samples Rest
    ------
     8 bit Channel Count
    24 bit -
    ------
     8 bit Profile
     8 bit Version number
    16 bit -
    ------
    16 bit Peak Title
    16 bit Peak Album
    ------
    16 bit Gain Title
    16 bit Gain Album
    ------
*/

void
WriteHeader_SV7F ( const unsigned int  MaxBand,              // writes SV7.15-header
                   const unsigned int  Profile,
                   const unsigned int  MS_on,
                   const Uint32_t      TotalFrames,
                   const unsigned int  SamplesRest,
                   const unsigned int  StreamVersion,
                   int                 SampleFreq )
{
//  0
    WriteBits ( 0x4D502B     , 24 );    // Magic Number "MP+"
    WriteBits ( StreamVersion,  8 );    // StreamVersion
//  4
    WriteBits ( 0            , 16 );    // placeholder for header length
    WriteBits ( MPPENC_VERSION [3] & 1  ?  1  : Profile
                              , 8 );    // 1: Experimental profile, 5...15: below Telephone...above BrainDead
    WriteBits ( (MPPENC_VERSION[0]&15)*100 + (MPPENC_VERSION[2]&15)*10 + (MPPENC_VERSION[3]&15)
                             ,  8 );    // encoder version number
//  8
    WriteBits ( TotalFrames  , 32 );    // number of Musepack frames
// 12
    WriteBits ( 0            , 16 );    // title peak
    WriteBits ( 0            , 16 );    // title gain
// 16
    WriteBits ( 0            , 16 );    // album peak
    WriteBits ( 0            , 16 );    // album gain
// 20
    WriteBits ( MaxBand      ,  6 );    // Bandwidth
    WriteBits ( MS_on        ,  4 );    // MS Coding bits
    WriteBits ( 0            ,  6 );    // -
    WriteBits ( SamplesRest  , 16 );    // true gapless: valid samples in last frame
// 24
    WriteBits ( 2            ,  8 );    // 2 channels
    WriteBits ( 0            , 24 );    // -
// 28
    WriteBits ( SampleFreq   , 16 );    // 32 * Subbband width
    WriteBits ( 0            , 16 );    // -
}


int
UpdateHeader_SV7F ( FILE*     fp,
                    Uint32_t  Frames,
                    Uint      ValidSamples )
{
    Uint8_t  buff [4];
    long     pos;

    if ( ( pos = ftell (fp) ) < 0 )
        return -1;

    if ( fseek ( fp, 8L, SEEK_SET ) < 0 )               // Schreibe Frameanzahl in den Header
        return -1;

    buff [0] = (Uint8_t)(Frames >> 24);
    buff [1] = (Uint8_t)(Frames >> 16);
    buff [2] = (Uint8_t)(Frames >>  8);
    buff [3] = (Uint8_t)(Frames >>  0);

    fwrite ( buff, 1, 4, fp );

    if ( fseek ( fp, 22L, SEEK_SET ) < 0 )              // Schreibe ValidSamples in den Header
        return -1;

    buff [0] = (Uint8_t)(ValidSamples >>  8);
    buff [1] = (Uint8_t)(ValidSamples >>  0);

    fwrite ( buff, 1, 2, fp );

    if ( fseek ( fp, pos, SEEK_SET ) < 0 );
        return -1;
    return 0;
}


static int Cdecl
cmp_fn ( const void* p1, const void* p2 )
{
    return *(const int*)p1 - *(const int*)p2;
}


static int
bestof ( const size_t no, const int* data, int* const best1 , int* const best2 )
{
    int     y [16][2];
    size_t  i;

    for ( i = 0; i < no; i++ ) {
        y [i][0] = data[i];
        y [i][1] = i;
    }

    qsort ( y, no, sizeof(*y), cmp_fn );

    *best1 = y [0][0];
    *best2 = y [1][0];
    return y [0][1];
}


static int
countbits ( int x )
{
    if ( x < 0 )
        x = ~x;
    if ( x <     1 ) return  0;
    if ( x <     2 ) return  1;
    if ( x <     4 ) return  2;
    if ( x <     8 ) return  3;
    if ( x <    16 ) return  4;
    if ( x <    32 ) return  5;
    if ( x <    64 ) return  6;
    if ( x <   128 ) return  7;
    if ( x <   256 ) return  8;
    if ( x <   512 ) return  9;
    if ( x <  1024 ) return 10;
    if ( x <  2048 ) return 11;
    if ( x <  4096 ) return 12;
    if ( x <  8192 ) return 13;
    if ( x < 16384 ) return 14;
    if ( x < 32768 ) return 15;
    if ( x < 65536 ) return 16;
    if ( x <131072 ) return 17;
    fprintf ( stderr, "Subband sample too large: %d\n", x );
    return -1;
}


static int
bastel  ( const HuffEncCode_t* T,
          int                  auxbits,
          int                  S0,
          int                  S1,
          int                  S2 )
{
    int  bits0 = countbits (S0) - auxbits;
    int  bits1 = countbits (S1) - auxbits;
    int  bits2 = countbits (S2) - auxbits;
    int  idx;
    int  ret;

    if ( bits0 >= 0  ||  bits1 >= 0  ||  bits2 >= 0 ) {
        if ( bits0 < 0 ) bits0 = 0;
        if ( bits1 < 0 ) bits1 = 0;
        if ( bits2 < 0 ) bits2 = 0;
        idx = (5-bits0) + 6*(5-bits1) + 36*(5-bits2);

        ret = T [idx].bits + 3 * auxbits + (bits0  ?  bits0-1  :  0) + (bits1  ?  bits1-1  :  0) + (bits2  ?  bits2-1  :  0);
    }
    else {
        if ( bits1 > bits0 ) bits0 = bits1;
        if ( bits2 > bits0 ) bits0 = bits2;
        idx = 215 - bits0;                      // darf nicht größer als 225 werden

        ret = T [idx].bits + 3 * auxbits + 3 * bits0;
    }

    return ret;
}


static void
kodiere ( const HuffEncTable_t* T,
          int                   auxbits,
          int                   S0,
          int                   S1,
          int                   S2,
          int                   book )
{
    static unsigned int  msk[17] = {
        0x0000, 0x0001, 0x0003, 0x0007,
        0x000F, 0x001F, 0x003F, 0x007F,
        0x00FF, 0x01FF, 0x03FF, 0x07FF,
        0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF,
        0xFFFF
    };
    int  bits0 = countbits (S0) - auxbits;
    int  bits1 = countbits (S1) - auxbits;
    int  bits2 = countbits (S2) - auxbits;
    int  idx;
    int  tmp;

    if ( bits0 >= 0  ||  bits1 >= 0  ||  bits2 >= 0 ) {
        if ( bits0 < 0 ) bits0 = 0;
        if ( bits1 < 0 ) bits1 = 0;
        if ( bits2 < 0 ) bits2 = 0;
        idx = (5-bits0) + 6*(5-bits1) + 36*(5-bits2);
        WriteHuffman3 ( T, idx );

        tmp = (bits0 ?  bits0-1  :  0) + auxbits;
        WriteBits ( (S0 < 0  ?  ~S0  :  S0) & msk [tmp], tmp );
        WriteBit  (  S0 < 0 );
        tmp = (bits1 ?  bits1-1  :  0) + auxbits;
        WriteBits ( (S1 < 0  ?  ~S1  :  S1) & msk [tmp], tmp );
        WriteBit  (  S1 < 0 );
        tmp = (bits2 ?  bits2-1  :  0) + auxbits;
        WriteBits ( (S2 < 0  ?  ~S2  :  S2) & msk [tmp], tmp );
        WriteBit  (  S2 < 0 );
    }
    else {
        if ( bits1 > bits0 ) bits0 = bits1;
        if ( bits2 > bits0 ) bits0 = bits2;
        idx = 215 - bits0;                      // darf nicht größer als 225 werden
        WriteHuffman3 ( T, idx );

        tmp = bits0 + auxbits;
        WriteBits ( (S0 < 0  ?  ~S0  :  S0) & msk [tmp], tmp );
        WriteBit  (  S0 < 0 );
        WriteBits ( (S1 < 0  ?  ~S1  :  S1) & msk [tmp], tmp );
        WriteBit  (  S1 < 0 );
        WriteBits ( (S2 < 0  ?  ~S2  :  S2) & msk [tmp], tmp );
        WriteBit  (  S2 < 0 );
    }
}


static void
WriteBitStream_Samples ( const int    MaxBand,
                         const res_t  Res [32],
                         const Uint   q   [32] [36] )
{
    static const HuffEncTablePtr_t  TT [12] [4] = {
        { NULL               , NULL                },     //  0
        { NULL               , NULL                },     //  1
        { NULL               , NULL                },     //  2
        { &Encodertable_Q11  , &Encodertable_Q12  , &Encodertable_Q13, &Encodertable_Q14 },     //  3
        { &Encodertable_Q21  , &Encodertable_Q22  , &Encodertable_Q23, &Encodertable_Q24 },     //  4
        { &Encodertable_Q31  , &Encodertable_Q32  , &Encodertable_Q33, &Encodertable_Q34 },     //  5
        { &Encodertable_Q41  , &Encodertable_Q42   },     //  6
        { &Encodertable_Q51  , &Encodertable_Q52   },     //  7
        { &Encodertable_Q71  , &Encodertable_Q72   },     //  8
        { &Encodertable_Q101 , &Encodertable_Q102  },     //  9
        { &Encodertable_Q151 , &Encodertable_Q152  },     // 10
        { &Encodertable_Q311 , &Encodertable_Q312  },     // 11
    };
    static const HuffEncTablePtr_t  TU  [8] = {
        &Encodertable_BIG1 , &Encodertable_BIG2 , &Encodertable_BIG3 , &Encodertable_BIG4 ,
        &Encodertable_BIGV1, &Encodertable_BIGV2, &Encodertable_BIGV3
    };
    static const Uint32_t  msk [] = {
        0x000, 0x001, 0x003, 0x007, 0x00F, 0x01F, 0x03F, 0x07F,
        0x0FF, 0x1FF, 0x3FF, 0x7FF, 0xFFF
    };
    static const Uint      mul [] = { 0, 0, 0, 3, 5, 7, 9, 11, 15 };
    const HuffEncTable_t*  T;
    const HuffEncCode_t*   T0;
    const HuffEncCode_t*   T1;
    const HuffEncCode_t*   T2;
    const HuffEncCode_t*   T3;
    unsigned int           idx;
    unsigned int           book;
    int                    n;
    int                    k;
    int                    tmp;
    int                    sum0;
    int                    sum1;
    int                    sum [16];

    for ( n = 0; n <= MaxBand; n++ ) {
        switch ( Res[n] ) {
        case -8:
        case -7:
        case -6:
        case -5:
        case -4:
        case -3:
        case -2:
        case -1:
        case  0:
        case  1:
            break;
        case  2:
            break;
        case  3:                                // -1...+1, 4 samples coupled
            T0 = TT [3][0] -> table;
            T1 = TT [3][1] -> table;
            T2 = TT [3][2] -> table;
            T3 = TT [3][3] -> table;
            sum[0] = sum[1] = sum[2] = sum[3] = 0;
            for ( k = 0; k < 36; k += 4 ) {
                idx  = q[n][k+0] + 3*q[n][k+1] + 9*q[n][k+2] + 27*q[n][k+3];
                sum[0] += T0 [idx].bits;
                sum[1] += T1 [idx].bits;
                sum[2] += T2 [idx].bits;
                sum[3] += T3 [idx].bits;
            }
            book = bestof ( 4, sum, &sum0, &sum1 );
            WriteHuffman ( Q1, book );

            T = TT [3] [book];
            Advantage ( T, sum0, sum1, 1 );

            for ( k = 0; k < 36; k += 4 ) {
                idx  = q[n][k+0] + 3*q[n][k+1] + 9*q[n][k+2] + 27*q[n][k+3];
                WriteHuffman3 ( T, idx );
            }
            break;
        case  4:                                // -2...+2, 3 samples coupled
            T0 = TT [4][0] -> table;
            T1 = TT [4][1] -> table;
            T2 = TT [4][2] -> table;
            T3 = TT [4][3] -> table;
            sum[0] = sum[1] = sum[2] = sum[3] = 0;
            for ( k = 0; k < 36; k += 3 ) {
                idx  = q[n][k+0] + 5*q[n][k+1] + 25*q[n][k+2];
                sum[0] += T0 [idx].bits;
                sum[1] += T1 [idx].bits;
                sum[2] += T2 [idx].bits;
                sum[3] += T3 [idx].bits;
            }
            book = bestof ( 4, sum, &sum0, &sum1 );
            WriteHuffman ( Q2, book );

            T = TT [4] [book];
            Advantage ( T, sum0, sum1, 1 );

            for ( k = 0; k < 36; k += 3 ) {
                idx  = q[n][k+0] + 5*q[n][k+1] + 25*q[n][k+2];
                WriteHuffman3 ( T, idx );
            }
            break;
        case  5:                                // -3...+3, 2 samples coupled
            T0 = TT [Res[n]][0] -> table;
            T1 = TT [Res[n]][1] -> table;
            T2 = TT [Res[n]][2] -> table;
            T3 = TT [Res[n]][3] -> table;
            sum[0] = sum[1] = sum[2] = sum[3] = 0;
            for ( k = 0; k < 36; k += 2 ) {
                idx  = q[n][k+0] + mul[Res[n]]*q[n][k+1];
                sum[0] += T0 [idx].bits;
                sum[1] += T1 [idx].bits;
                sum[2] += T2 [idx].bits;
                sum[3] += T3 [idx].bits;
            }
            book = bestof ( 4, sum, &sum0, &sum1 );
            WriteHuffman ( Q3, book );

            T = TT [Res[n]] [book];
            Advantage ( T, sum0, sum1, 1 );

            for ( k = 0; k < 36; k += 2 ) {
                idx  = q[n][k+0] + mul[Res[n]]*q[n][k+1];
                WriteHuffman3 ( T, idx );
            }
            break;
        case  6:                                // -4...+4, 2 samples coupled
        case  7:                                // -5...+5, 2 samples coupled
        case  8:                                // -7...+7, 2 samples coupled
            T0 = TT [Res[n]][0] -> table;
            T1 = TT [Res[n]][1] -> table;
            sum0 = sum1 = 0;
            for ( k = 0; k < 36; k += 2 ) {
                idx  = q[n][k+0] + mul[Res[n]]*q[n][k+1];
                sum0 += T0 [idx].bits;
                sum1 += T1 [idx].bits;
            }
            book = sum0 >= sum1;
            WriteBits ( book, 1 );
            T = TT [Res[n]] [book];

            Advantage ( T, mini(sum0, sum1), maxi(sum0, sum1), 1 );

            for ( k = 0; k < 36; k += 2 ) {
                idx  = q[n][k+0] + mul[Res[n]]*q[n][k+1];
                WriteHuffman3 ( T, idx );
            }
            break;
        case  9:                                // -10...+10
        case 10:                                // -15...+15
        case 11:                                // -31...+31
            T0 = TT [Res[n]] [0] -> table_no_offs;
            T1 = TT [Res[n]] [1] -> table_no_offs;
            sum0 = sum1 = 0;
            for ( k = 0; k < 36; k++ ) {
                idx  = q[n][k];
                sum0 += T0 [idx].bits;
                sum1 += T1 [idx].bits;
            }
            book = sum0 >= sum1;
            WriteBits ( book, 1 );
            T = TT [Res[n]] [book];

            Advantage ( T, mini(sum0, sum1), maxi(sum0, sum1), 1 );

            for ( k = 0; k < 36; k++ ) {
                idx = q[n][k];
                WriteHuffman3 ( T, idx );
            }
            break;
        case 12:                                //    -64...   +63
        case 13:                                //   -128...  +127
        case 14:                                //   -256...  +255
        case 15:                                //   -512...  +511
        case 16:                                //  -1024... +1023
        case 17:                                //  -2048... +2047
        case 18:                                //  -4096... +4095
        case 19:                                //  -8192... +8191
        case 20:                                // -16384...+16383
        case 21:                                // -32768...+32767
            T0 = TU [0] -> table_no_offs;
            T1 = TU [1] -> table_no_offs;
            tmp = Res[n] - 11;
            sum[0] = sum[1] = 36*tmp;
            for ( k = 0; k < 36; k++ ) {
                idx  = q[n][k] >> tmp;
                sum[0] += T0 [idx].bits;
                sum[1] += T1 [idx].bits;
            }

            T0 = TU [2] -> table_no_offs;
            T1 = TU [3] -> table_no_offs;
            tmp = Res[n] - 9;
            sum[2] = sum[3] = 36*tmp;
            for ( k = 0; k < 36; k += 2 ) {
                idx  = (q[n][k] >> tmp)*16 + (q[n][k+1] >> tmp);
                sum[2] += T0 [idx].bits;
                sum[3] += T1 [idx].bits;
            }

            T0 = TU [4] -> table_no_offs;
            T1 = TU [5] -> table_no_offs;
            T2 = TU [6] -> table_no_offs;
            tmp = Res[n] - 11;
            sum[4] = sum[5] = sum[6] = 36;
            for ( k = 0; k < 36; k += 3 ) {
                sum[4] += bastel ( T0, tmp, q[n][k]-(32<<tmp), q[n][k+1]-(32<<tmp), q[n][k+2]-(32<<tmp) );
                sum[5] += bastel ( T1, tmp, q[n][k]-(32<<tmp), q[n][k+1]-(32<<tmp), q[n][k+2]-(32<<tmp) );
                sum[6] += bastel ( T2, tmp, q[n][k]-(32<<tmp), q[n][k+1]-(32<<tmp), q[n][k+2]-(32<<tmp) );
            }

            book = bestof ( 7, sum, &sum0, &sum1 );
            WriteHuffman ( BIGT, book );

            T   = TU [book];
            Advantage ( T, sum0, sum1, 1 );

            if ( book < 2 ) {
                tmp = Res[n] - 11;
                for ( k = 0; k < 36; k++ ) {
                    idx = q[n][k];
                    WriteHuffman3 ( T, idx >> tmp );
                    WriteBits     ( idx & msk[tmp], tmp );
                }
            }
            else if ( book < 4 ) {
                tmp = Res[n] - 9;
                // printf ("%1u: ", book );
                for ( k = 0; k < 36; k += 2 ) {
                    // printf ("%7d%7d", q[n][k]-(1<<(Res[n]-6)), q[n][k+1]-(1<<(Res[n]-6)) );
                    idx = (q[n][k] >> tmp)*16 + (q[n][k+1] >> tmp);
                    WriteHuffman3 ( T, idx );
                    WriteBits     ( q[n][k+0] & msk[tmp], tmp );
                    WriteBits     ( q[n][k+1] & msk[tmp], tmp );
                }
                // printf ("\n");
            }
            else {
                tmp = Res[n] - 11;
                for ( k = 0; k < 36; k += 3 ) {
                    kodiere ( T, tmp, q[n][k]-(32<<tmp), q[n][k+1]-(32<<tmp), q[n][k+2]-(32<<tmp), 10*Res[n]+1+book );
                }
            }
            break;
        }
    }

    return;
}


#define ENCODE_SCF1( new, last, rll )                        \
        d = new - last;                                      \
        if ( new == SCF_RES ) {                              \
            WriteHuffman ( DSCF, 0 );                        \
        }                                                    \
        else if ( -10 <= d  &&  d <= 10  &&  rll < 32 ) {    \
            WriteHuffman ( DSCF, d );                        \
            last = new;                                      \
        }                                                    \
        else {                                               \
            WriteHuffman ( DSCF, 11 );                       \
            WriteBits ( (unsigned int)new, 7 );              \
            rll = 0;                                         \
            last = new;                                      \
        }

#define ENCODE_SCFn( new, last, rll )                        \
        d = new - last;                                      \
        if ( -10 <= d  &&  d <= 10 ) {                       \
            WriteHuffman ( DSCF, d );                        \
            last = new;                                      \
        }                                                    \
        else {                                               \
            WriteHuffman ( DSCF, 11 );                       \
            WriteBits ( (unsigned int)new, 7 );              \
            rll = 0;                                         \
            last = new;                                      \
        }


static void
WriteBitStream_SCF ( const int    MaxBand,
                     const res_t  Res       [32],
                     Uint8_t      dscf_rll  [32],
                     scf_t        scf_last  [32],
                     const scf_t  scf_index [32] [3] )
{
    int n;
    unsigned int      scfi;
    unsigned int      rll;
    int               d;
    scf_t             last;

    for ( n = 0; n <= MaxBand; n++ ) {
        rll = dscf_rll[n];

        if ( Res[n] ) {
            // printf ("%2u: %3d %3d %3d\n", n, scf_index[n][0], scf_index[n][1], scf_index[n][2] );
            scfi = 2 * (scf_index[n][0] == scf_index[n][1] || scf_index[n][1] == SCF_RES ) + (scf_index[n][1] == scf_index[n][2] || scf_index[n][2] == SCF_RES );
            WriteHuffman ( SCFI, scfi );

            last = scf_last [n];
            switch ( scfi ) {
            default:
                ENCODE_SCF1 ( scf_index[n][0], last, rll );
                ENCODE_SCFn ( scf_index[n][1], last, rll );
                ENCODE_SCFn ( scf_index[n][2], last, rll );
                break;
            case 1:
                ENCODE_SCF1 ( scf_index[n][0], last, rll );
                ENCODE_SCFn ( scf_index[n][1], last, rll );
                break;
            case 2:
                ENCODE_SCF1 ( scf_index[n][0], last, rll );
                ENCODE_SCFn ( scf_index[n][2], last, rll );
                break;
            case 3:
                ENCODE_SCF1 ( scf_index[n][0], last, rll );
                break;
            }
            scf_last [n] = last;
        }
        dscf_rll[n] = rll + 1;  // Erhöhe Zähler für nicht neu initialisierte SCF
    }
}


/*
 *  Encoding of the subband resolutions
 *  - Resolution of subband 0 is encoded via HuffTable HDR0.
 *  - Resolution of subband 1 is encoded differentially, residual is encoded via HuffTable DHDR1.
 *  - Remaining subbands are encoded differentially, residual is encoded via HuffTable DHDR.
 *  - There's a special escape code 23 for all following subband resolutions are the same like the resolution of the last subband
 */

static void
WriteBitStream_Res ( const int    MaxBand,
                     const res_t  Res [32] )
{
    int  n;
    int  d;
    int  c = -1;

    d = 0;
    if ( MaxBand > 6  &&
         Res[MaxBand] == Res[MaxBand-1]  &&
         Res[MaxBand] == Res[MaxBand-2]  &&
         Res[MaxBand] == Res[MaxBand-3]  &&
         Res[MaxBand] == Res[MaxBand-4]  &&
         Res[MaxBand] == Res[MaxBand-5]  &&
         Res[MaxBand] == Res[MaxBand-6] ) {
         for ( n = MaxBand; n >= 1; n-- )
             if ( Res[MaxBand] == Res[n-1] )
                 c = n;
	     else
	         break;
		 
         if ( Res[MaxBand] == 0  &&  c == 1 )
             c = 0;
    }

    if ( c == 0 ) {
        WriteHuffman ( HDR0, 23 );                                      // repeat
        return;
    }
    WriteHuffman ( HDR0, Res[0] );                                      // subband 0

    if ( MaxBand < 1 )
        return;

    if ( c == 1 ) {
        WriteHuffman ( DHDR1 , 5 );
        WriteBits ( 23+8, 5 );
        return;
    }

    d = Res[1] - Res[0];                                                // subband 1
    if ( -12 <= d  &&  d <= 4 ) {
        WriteHuffman ( DHDR1 , d );
    }
    else {
        WriteHuffman ( DHDR1 , 5 );
        WriteBits ( (unsigned int)Res[1]+8, 5 );
    }

    for ( n = 2; n <= MaxBand; n++ ) {                                  // subband 2...MaxBand
        if ( c == n ) {
            WriteHuffman ( DHDR , 8 );
            WriteBits ( 23+8, 5 );
            return;
        }
        d = Res[n] - Res[n-1];
        if ( -7 <= d  &&  d <= 7 ) {
            WriteHuffman ( DHDR , d );
        }
        else {
            WriteHuffman ( DHDR , 8 );
            WriteBits ( (unsigned int)Res[n]+8, 5 );
        }
    }
}


// formatting and writing SV7-bitstream for one frame
void
WriteBitstream_SV7F ( int           MaxBand,
                      const res_t   Res       [] [32],
                      const pack_t  MS           [32],
                      const scf_t   SCF_Index [] [32] [ 3],
                      const Uint    S         [] [32] [36] )
{
    static scf_t    SCF_Last [MAXCH] [32];     // Letzter kodierter SCF-Wert
    static Uint8_t  DSCF_RLL [MAXCH] [32];     // Dauer des differentiellen SCF-Coding zur Lauflängenbegrenzung
    int             n;

    ENTER(110);

    /************************************ Bands *********************************/
    while ( MaxBand >= 0  &&  Res[0][MaxBand] == 0  &&  Res[1][MaxBand] == 0 )
        MaxBand--;

    WriteHuffman ( MAXSUB, MaxBand );
    if ( MaxBand < 0 )
        return;

    /************************************ Resolution *********************************/
    WriteBitStream_Res ( MaxBand, Res[0] );
    WriteBitStream_Res ( MaxBand, Res[1] );

    /************************************ MS Channelbits ******************************/
    if ( MS_Channelmode > 0 )
        for ( n = 0; n <= MaxBand; n++ )
            if ( Res[0][n] != 0  ||  Res[1][n] != 0 )
                WriteBits ( MS[n], 1 );

    /************************************ SCF ************************************/
    WriteBitStream_SCF ( MaxBand, Res[0], DSCF_RLL[0], SCF_Last[0], SCF_Index[0] );
    WriteBitStream_SCF ( MaxBand, Res[1], DSCF_RLL[1], SCF_Last[1], SCF_Index[1] );

    /*********************************** Samples *********************************/
    WriteBitStream_Samples ( MaxBand, Res[0], S[0] );
    WriteBitStream_Samples ( MaxBand, Res[1], S[1] );

    LEAVE(110);
    return;
}

/* end of encode_sv7.c */
