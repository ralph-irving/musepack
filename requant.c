/*
            neu         alt
-8...-1      IS
 0          nichts      nichts
 1           PNS          -1...+1
 2          -x/+x         -2...+2
 3         -1...+1        -3...+3
 4         -2...+2        -4...+4
 5         -3...+3        -7...+7
 6         -4...+4       -15...+15
 7         -5...+5       -31...+31
 8         -7...+7       -63...+63
 9        -10...+10     -127...+127
10        -15...+15     -255...+255
11        -31...+31     -511...+511
12        -63...+63    -1023...+1023
13       -127...+127   -2047...+2047
14       -255...+255   -4095...+4095
15       -511...+511   -8191...+8191
16      -1023...+1023
17      -2047...+2047
18      -4095...+4095
19      -8191...+8191
20     -16383...+16383
21     -32767...+32767
*/

#include "mppdec.h"

Int8_t      Q_bit [32];         // Anzahl Bits für Speicherung der Auflösung (SV6)
Int8_t      Q_res [32] [16];    // Index -> Auflösung (SV6)
Float       __SCF [8 + 128];    // tabellierte Skalenfaktoren mit Schutzrasen
Float       __Cc7 [1 + 18];     // Requantisierungs-Koeffizienten
const Uint  __Dc7 [1 + 18] = {  // Requantisierungs-Offset
      2,
      0,     1,     2,     3,     4,     7,    15,    31,
     63,   127,   255,   511,  1023,  2047,  4095,  8191,
  16383, 32767
};
Float       Cc8   [22];         // Requantisierungs-Koeffizienten
const Uint  Dc8   [22] = {      // Requantisierungs-Offset
      0,     0,     0,     1,     2,     3,     4,     5,
      7,    10,    15,    31,    63,   127,   255,   511,
   1023,  2047,  4095,  8191, 16383, 32767
};
Uint        Bitrate;
Int         Min_Band;
Int         Max_Band;


// Initialisiert Min_Band, Max_Band; Input sind Funktionsparameter und Bitrate

static void
Set_BandLimits ( Int Max_Band_desired, Bool_t used_IS )
{
    if ( Max_Band_desired > 0 ) {   // Bandbreite vom User ausgewählt
        Max_Band = Max_Band_desired;
    }
    else {                        // Default-Bandbreite
        if      ( Bitrate > 384 ) Max_Band = 31;        // 22.05 kHz
        else if ( Bitrate > 160 ) Max_Band = 29;        // 20.67 kHz
        else if ( Bitrate >  64 ) Max_Band = 26;        // 18.60 kHz
        else if ( Bitrate >   0 ) Max_Band = 20;        // 14.47 kHz
        else                      Max_Band = 31;        // 22.05 kHz
    }

    if ( used_IS ) {
        if      ( Bitrate > 384 ) assert (0);
        else if ( Bitrate > 160 ) Min_Band = 16;        // 11.02 kHz
        else if ( Bitrate > 112 ) Min_Band = 12;        //  8.27 kHz
        else if ( Bitrate > 64  ) Min_Band =  8;        //  5.51 kHz
        else                      Min_Band =  4;        //  2.76 kHz

        if ( Min_Band >= Max_Band )
            Min_Band = Max_Band /* + 1 ????? */;
    }
    else {
        Min_Band = Max_Band + 1;
    }
}


// Initialisiert Q_bit und Q_res

static void
Set_QuantizeMode_SV4_6 ( void )
{
    Int  Band;
    Int  i;

    for ( Band = 0; Band < 11; Band++ ) {
        Q_bit [Band] = 4;
        for ( i = 0; i < (1<<4)-1; i++ )
            Q_res [Band] [i] = (Uint8_t)i;
        Q_res [Band] [(1<<4)-1] = 17;
    }
    for ( Band = 11; Band < 23; Band++ ) {
        Q_bit [Band] = 3;
        for ( i = 0; i < (1<<3)-1; i++ )
            Q_res [Band] [i] = (Uint8_t)i;
        Q_res [Band] [(1<<3)-1] = 17;
    }
    for ( Band = 23; Band < 32; Band++ ) {
        Q_bit [Band] = 2;
        for ( i = 0; i < (1<<2)-1; i++ )
            Q_res [Band] [i] = (Uint8_t)i;
        Q_res [Band] [(1<<2)-1] = 17;
    }
}


// Initialisiert Tabelle SCF

static void
Calc_ScaleFactors ( Ldouble start, Ldouble mult )
{
    size_t  i;

    for ( i = 0; i < sizeof(__SCF)/sizeof(*__SCF); i++ ) {
        __SCF [i] = (Float) start;
        start    *= mult;
    }
}


// Initialisiert Cc, benötigt Tabelle Dc

static void
Calc_RequantTab_SV4_7 ( void )
{
    size_t  i;

    for ( i = 0; i < sizeof(__Cc7)/sizeof(*__Cc7); i++ )
        __Cc7 [i] = (Float) (32768. / (__Dc7 [i] + 0.5));
    __Cc7 [0] = 111.28596247532739441973f;                                       // 16384 / 255 * sqrt(3)
}


static void
Calc_RequantTab_SV8 ( void )
{
    size_t  i;

    for ( i = 0; i < sizeof(Cc8)/sizeof(*Cc8); i++ )
        Cc8 [i] = (Float) (32768. / (Dc8 [i] + 0.5));
    Cc8 [1] =   111.28596247532739441973f;                                       // 16384 / 255 * sqrt(3)
    Cc8 [2] = 12612.40908053710470095719f;                                       //
}


#define C1  1.20050805774840750476L
#define C2  0.83298066476582673961L

void
Init_QuantTab ( Int maximum_Band, Bool_t used_IS, Double amplification, Uint StreamVersion )
{
    switch ( StreamVersion ) {                    // Initialierungen erfolgen voneinander unabhängig, Reihenfolge beliebig
    case 0x04:
    case 0x05:
    case 0x06:
        Set_QuantizeMode_SV4_6 ();
        Set_BandLimits ( maximum_Band, used_IS );
        Calc_RequantTab_SV4_7 ();
        Calc_ScaleFactors ( amplification * C1/(C2*C2*C2*C2*C2*C2*C2*C2), C2 );
        break;

    case 0x07:
    case 0x17:
        Set_BandLimits ( maximum_Band, used_IS );
        Calc_RequantTab_SV4_7 ();
        Calc_ScaleFactors ( amplification * C1/(C2*C2*C2*C2*C2*C2*C2*C2), C2 );
        // -8: 5.17947467923121113475
        //  0: 1.20050805774840750476
        //  1: 1.0
        // 63: 0.0000120050805774840750476
        break;

    case 0xFF:
        Max_Band = maximum_Band;
        Calc_RequantTab_SV8 ();
        Calc_ScaleFactors ( amplification * 2.328306436538696289065e-10L, 1.18920711500272106671L );
        //  -8:  2.328306436538696289065e-10
        //   0:  9.313225746154785156250e-10
        //  80:  0.0009765625
        // 120:  1.0
        // 127:  3.36358566101485817214
        break;

    default:
        fprintf ( stderr, "Unknown StreamVersion %u.%u, abort.\a\n", StreamVersion & 15, StreamVersion >> 4 );
        sleep (5);
        exit (1);
    }
}

/* end of requant.c */
