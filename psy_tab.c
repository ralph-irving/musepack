/*
 *  Clear Vocal Detection functions
 *
 *  (C) Andree Buschmann 2000, Frank Klemm 2001,02. All rights reserved.
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


/*
 * ath_gain und ath_gain20  und nur einstellige ATHs
 */

#include "mppenc.h"

// Antialiasing für Berechnung der Subbandleistungen
const float  Butfly    [7] = { 0.5f, 0.2776f, 0.1176f, 0.0361f, 0.0075f, 0.000948f, 0.0000598f };

// Antialiasing für Berechnung der Maskierungsschwellen
const float  InvButfly [7] = { 2.f, 3.6023f, 8.5034f, 27.701f, 133.33f, 1054.852f, 16722.408f };


/*
// w_low für long               0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31   32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47   48   49   50   51   52   53   54   55   56
const int   wl [PART_LONG] = {  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  13,  15,  17,  19,  21,  23,  25,  27,  29,  31,  33,  35,  38,  41,  44,  47,  50,  54,  58,  62,  67,  72,  78,  84,  91,  98, 106, 115, 124, 134, 145, 157, 170, 184, 199, 216, 234, 254, 276, 301, 329, 360, 396, 437, 485 };
const int   wh [PART_LONG] = {  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  12,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,  37,  40,  43,  46,  49,  53,  57,  61,  66,  71,  77,  83,  90,  97, 105, 114, 123, 133, 144, 156, 169, 183, 198, 215, 233, 253, 275, 300, 328, 359, 395, 436, 484, 511 };
// Weite:                       1    1    1    1    1    1    1    1    1    1    1    2    2    2    2    2    2    2    2    2    2    2    2    3    3    3    3    3    4    4    4    5    5    6    6    7    7    8    9    9   10   11   12   13   14   15   17   18   20   22   25   28   31   36   41   48   27

// inverse partition-width for long
const float iw [PART_LONG] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/3, 1.f/3, 1.f/3, 1.f/3, 1.f/3, 1.f/4, 1.f/4, 1.f/4, 1.f/5, 1.f/5, 1.f/6, 1.f/6, 1.f/7, 1.f/7, 1.f/8, 1.f/9, 1.f/9, 1.f/10, 1.f/11, 1.f/12, 1.f/13, 1.f/14, 1.f/15, 1.f/17, 1.f/18, 1.f/20, 1.f/22, 1.f/25, 1.f/28, 1.f/31, 1.f/36, 1.f/41, 1.f/48, 1.f/27 };


// w_low für short                    0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17   18
const int   wl_short [PART_SHORT] = { 0,  1,  2,  3,  4,  5,  6,  8, 10, 12, 15, 18, 23, 29, 36, 46, 59, 75,  99 };
const int   wh_short [PART_SHORT] = { 0,  1,  2,  3,  5,  6,  7,  9, 12, 14, 18, 23, 29, 36, 46, 58, 75, 99, 127 };

// inverse partition-width for short
const float iw_short [PART_SHORT] = { 1.f, 1.f, 1.f, 1.f, 1.f/2, 1.f/2, 1.f/2, 1.f/2, 1.f/3, 1.f/3, 1.f/4, 1.f/6, 1.f/7, 1.f/8, 1.f/11, 1.f/13, 1.f/17, 1.f/25, 1.f/29 };
*/


const int   wl [PART_LONG] = {     0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   13,   15,   17,   19,   21,   23,   25,   27,   29,   31,   33,   35,   37,   39,   41,   44,   47,   50,   53,   56,   60,   64,   68,   73,   78,   83,   89,   96,  103,  111,  120,  129,  139,  150,  162,  175,  189,  204,  220,  238,  257,  278,  301,  326,  354,  385,  420,  460 };
const int   wh [PART_LONG] = {     0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   12,   14,   16,   18,   20,   22,   24,   26,   28,   30,   32,   34,   36,   38,   40,   43,   46,   49,   52,   55,   59,   63,   67,   72,   77,   82,   88,   95,  102,  110,  119,  128,  138,  149,  161,  174,  188,  203,  219,  237,  256,  277,  300,  325,  353,  384,  419,  459,  511 };
const float iw [PART_LONG] = { 1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 2,1./ 3,1./ 3,1./ 3,1./ 3,1./ 3,1./ 4,1./ 4,1./ 4,1./ 5,1./ 5,1./ 5,1./ 6,1./ 7,1./ 7,1./ 8,1./ 9,1./ 9,1./10,1./11,1./12,1./13,1./14,1./15,1./16,1./18,1./19,1./21,1./23,1./25,1./28,1./31,1./35,1./40,1./52 };

#if 0
const int   wl_short [PART_SHORT] = {    0,    0,    1,    2,    3,    4,    6,    7,    9,   11,   13,   16,   19,   24,   30,   37,   47,   59,   75,   96 };
const int   wh_short [PART_SHORT] = {    0,    1,    2,    3,    4,    6,    7,    9,   10,   13,   15,   19,   23,   29,   37,   47,   59,   75,   96,  127 };
const float iw_short [PART_SHORT] = {1./ 1,1./ 2,1./ 2,1./ 2,1./ 2,1./ 3,1./ 2,1./ 3,1./ 2,1./ 3,1./ 3,1./ 4,1./ 5,1./ 6,1./ 8,1./11,1./13,1./17,1./22,1./32 };
#else
//                                     0-2   3-5   6-8  9-12 13-18 19-24 25-30 31-36 37-43 44-52 53-63 64-77 78-95 96-119
const int   wl_short [PART_SHORT] = {    0,    0,    1,    2,    3,    4,    6,    7,    9,   11,   13,   16,   19,   24,   30,   37,   47,   59,   75,   96 };
const int   wh_short [PART_SHORT] = {    1,    2,    3,    4,    5,    7,    8,    9,   11,   13,   15,   19,   23,   29,   37,   47,   59,   75,   96,  127 };
const float iw_short [PART_SHORT] = {1./ 2,1./ 3,1./ 3,1./ 3,1./ 3,1./ 4,1./ 3,1./ 3,1./ 3,1./ 3,1./ 3,1./ 4,1./ 5,1./ 6,1./ 8,1./11,1./13,1./17,1./22,1./32 };
#endif

#if 0
const int   wl_ultrashort [PART_ULTRASHORT] = {    0,    0,    1,    2,    3,    4,    6,    9,   12,   18,   27,   40 };
const int   wh_ultrashort [PART_ULTRASHORT] = {    0,    0,    2,    3,    4,    6,    8,   12,   18,   27,   40,   63 };
const float iw_ultrashort [PART_ULTRASHORT] = {1./ 1,1./ 1,1./ 2,1./ 2,1./ 2,1./ 3,1./ 3,1./ 4,1./ 7,1./10,1./14,1./24 };
#elif 0
const int   wl_ultrashort [PART_ULTRASHORT] = {    0,    0,    0,    1,    2,    3,    4,    7,   11,   18 };
const int   wh_ultrashort [PART_ULTRASHORT] = {    0,    0,    0,    1,    2,    4,    6,   11,   18,   31 };
const float iw_ultrashort [PART_ULTRASHORT] = {1./ 1,1./ 1,1./ 1,1./ 1,1./ 1,1./ 2,1./ 3,1./ 5,1./ 8,1./14 };
#else
//                                               0-5  6-12 13-24 25-36 37-52 53-77 78-119
const int   wl_ultrashort [PART_ULTRASHORT] = {    0,    0,    0,    1,    2,    3,    5,    7,   11,   18 };
const int   wh_ultrashort [PART_ULTRASHORT] = {    0,    1,    2,    3,    4,    5,    7,   11,   18,   31 };
const float iw_ultrashort [PART_ULTRASHORT] = {1./ 1,1./ 2,1./ 3,1./ 3,1./ 3,1./ 3,1./ 3,1./ 5,1./ 8,1./14 };
#endif

/*
 *  Klemm 1994 and 1997. Experimental data. Sorry, data looks a little bit
 *  dodderly. Data below 30 Hz is extrapolated from other material, above 18
 *  kHz the ATH is limited due to the original purpose (too much noise at
 *  ATH is not good even if it's theoretically inaudible).
 */

static float
ATHformula_Frank ( float freq )
{
    /*
     * one value per 100 cent = 1
     * semitone = 1/4
     * third = 1/12
     * octave = 1/40 decade
     * rest is linear interpolated, values are currently in millibel rel. 20 µPa
     */
    static short tab [] = {
        /*    10.0 */  9669, 9669, 9626, 9512,
        /*    12.6 */  9353, 9113, 8882, 8676,
        /*    15.8 */  8469, 8243, 7997, 7748,
        /*    20.0 */  7492, 7239, 7000, 6762,
        /*    25.1 */  6529, 6302, 6084, 5900,
        /*    31.6 */  5717, 5534, 5351, 5167,
        /*    39.8 */  5004, 4812, 4638, 4466,
        /*    50.1 */  4310, 4173, 4050, 3922,
        /*    63.1 */  3723, 3577, 3451, 3281,
        /*    79.4 */  3132, 3036, 2902, 2760,
        /*   100.0 */  2658, 2591, 2441, 2301,
        /*   125.9 */  2212, 2125, 2018, 1900,
        /*   158.5 */  1770, 1682, 1594, 1512,
        /*   199.5 */  1430, 1341, 1260, 1198,
        /*   251.2 */  1136, 1057,  998,  943,
        /*   316.2 */   887,  846,  744,  712,
        /*   398.1 */   693,  668,  637,  606,
        /*   501.2 */   580,  555,  529,  502,
        /*   631.0 */   475,  448,  422,  398,
        /*   794.3 */   375,  351,  327,  322,
        /*  1000.0 */   312,  301,  291,  268,
        /*  1258.9 */   246,  215,  182,  146,
        /*  1584.9 */   107,   61,   13,  -35,
        /*  1995.3 */   -96, -156, -179, -235,
        /*  2511.9 */  -295, -350, -401, -421,
        /*  3162.3 */  -446, -499, -532, -535,
        /*  3981.1 */  -513, -476, -431, -313,
        /*  5011.9 */  -179,    8,  203,  403,
        /*  6309.6 */   580,  736,  881, 1022,
        /*  7943.3 */  1154, 1251, 1348, 1421,
        /* 10000.0 */  1479, 1399, 1285, 1193,
        /* 12589.3 */  1287, 1519, 1914, 2369,
#if 0
        /* 15848.9 */  3352, 4865, 5942, 6177,
        /* 19952.6 */  6385, 6604, 6833, 7009,
        /* 25118.9 */  7066, 7127, 7191, 7260,
#else
        /* 15848.9 */  3352, 4352, 5352, 6352,
        /* 19952.6 */  7352, 8352, 9352, 9999,
        /* 25118.9 */  9999, 9999, 9999, 9999,
#endif
    };
    double    freq_log;
    unsigned  index;

    if ( freq <    10. ) freq =    10.;
    if ( freq > 29853. ) freq = 29853.;

    freq_log = 40. * log10 (0.1 * freq);   /* 4 steps per third, starting at 10 Hz */
    index    = (unsigned) freq_log;
    return 0.01 * (tab [index] * (1 + index - freq_log) + tab [index+1] * (freq_log - index));
}


// Berechnung der Ruhehörschwelle in FFT-Auflösung
static void
ATHSetup ( float         fs,
           unsigned int  EarModelFlag,
           int           Ltq_offset,
           int           Ltq_max,
           float         Ltq_20kHz )
{
    int     n;
    int     k;
    float   f;
    float   erg;
    double  tmp;

    for ( n = 0; n < 512; n++ ) {
        f = (float) ( (n+1) * (fs / 2000.f) / 512 );   // Frequenz in kHz

        switch ( EarModelFlag ) {
        case 0:         // ISO-Ruhehörschwelle
            tmp  = 3.64*pow (f,-0.8) -  6.5*exp (-0.6*(f-3.3)*(f-3.3)) + 0.001*pow (f, 4.0);
            break;
        default:
        case 1:         // gemessene Ruhehörschwelle (Nick Berglmeir, Andree Buschmann, Kopfhörer)
            tmp  = 3.00*pow (f,-0.8) -  5.0*exp (-0.1*(f-3.0)*(f-3.0)) + 0.0000015022693846297*pow (f, 6.0) + 10.*exp (-(f-0.1)*(f-0.1));
            break;
        case 2:         // gemessene Ruhehörschwelle (Filburt, Kopfhörer)
            tmp  = 9.00*pow (f,-0.5) - 15.0*exp (-0.1*(f-4.0)*(f-4.0)) + 0.0341796875*pow (f, 2.5)          + 15.*exp (-(f-0.1)*(f-0.1)) - 18;
            tmp  = mind ( tmp, Ltq_max - 18 );
            break;
        case  3:
            tmp = ATHformula_Frank ( 1.e3 * f );
            break;
        case  4:
            tmp  = ATHformula_Frank ( 1.e3 * f );
            if ( f > 4.8 ) {
                tmp += 3.00*pow (f,-0.8) -  5.0*exp (-0.1*(f-3.0)*(f-3.0)) + 0.0000015022693846297*pow (f, 6.0) + 10.*exp (-(f-0.1)*(f-0.1));
                tmp *= 0.5 ;
            }
            break;
        case  5:
            tmp  = ATHformula_Frank ( 1.e3 * f );
            if ( f > 4.8 ) {
                tmp = 3.00*pow (f,-0.8) -  5.0*exp (-0.1*(f-3.0)*(f-3.0)) + 0.0000015022693846297*pow (f, 6.0) + 10.*exp (-(f-0.1)*(f-0.1));
            }
            break;
        }

        tmp      += f * f / 400. * Ltq_20kHz;
        tmp       = mind ( tmp, Ltq_max );              // Limit ATH
        tmp      += Ltq_offset - 23;                    // Add choosen Offset
        fftLtq[n] = POW10 ( 0.1 * tmp);                 // Umrechnung in Leistung
    }

    // Ruhehörschwelle in Partitionen (long)
    for ( n = 0; n < PART_LONG; n++ ) {
        erg = 1.e20f;
        for ( k = wl[n]; k <= wh[n]; k++ )
            erg = minf ( erg, fftLtq[k] );

        partLtq[n] = erg;               // Ruhehörschwelle
        invLtq [n] = 1.f / partLtq[n];  // Inverse
    }
}

#ifdef _WIN32
static double
asinh ( double x )
{
    return x >= 0  ?  log (sqrt (x*x+1) + x)  :  -log (sqrt (x*x+1) - x);
}
#endif


// 13*atan(0.76*freq) + 3.5*(freq/7.5)²

static double
Freq2Bark ( double Hz )                 // Klemm 2002
{
    return 9.97074*asinh (1.1268e-3 * Hz) - 6.25817*asinh (0.197193e-3 * Hz) ;
}

static double
Bark2Freq ( double Bark )               // Klemm 2002
{
    return 956.86 * sinh (0.101561*Bark) + 11.7296 * sinh (0.304992*Bark) + 6.33622e-3*sinh (0.538621*Bark);
}

static double
LongPart2Bark ( int Part, float fs )
{
    return Freq2Bark ((wl [Part] + wh [Part]) * fs / 2048.);
}

// Berechnung der Tabelle zur Lautheitsberechnung basierend auf absLtq = ank
static void
Loudness_Table ( float fs )
{
    int    n;
    float  midfreq;
    float  tmp;

    // ca. dB(A)
    for ( n = 0; n < PART_LONG; n++ ){
        // Was soll die falsche Berechnung der Mittenfrequenz?
        midfreq      = (wh[n] + wl[n] + 3) * (0.25 * fs / 512);     // Mittenfrequenz in kHz, Warum +3 ???
        tmp          = LOG10 (midfreq) - 3.5f;                      // dB(A)
        tmp          = -10 * tmp * tmp + 3 - midfreq/3000;
        Loudness [n] = POW10 ( 0.1 * tmp );                         // Umrechnung in Leistung
    }
}


// Berechnung der Spreadingfunktion
static void
SetupSpread ( float fs )
{
    int    i;
    int    j;
    float  diff;
    float  tmp;
    float  x;

    // Berechnung der Spreading-Funktion für alle auftretenden Werte
    for ( i = 0; i < PART_LONG; i++ ) {                                                 // i ist maskierende Partition, Quelle
        for ( j = 0; j < PART_LONG; j++ ) {                                             // j ist maskierte Partition, Ziel
            diff = LongPart2Bark (j, fs) - LongPart2Bark (i, fs);                       // Differenz der Partitionen in Bark

            if  ( diff <= 0. ) {                                                        // Maskierung nach unten
                tmp  = -32.f * diff;
            }
            else {                                                                      // Maskierung nach unten
                x    = (i  ?  wl[i]+wh[i]  :  1) * (fs / 1000. / 2048);                 // Mittenfrequenz in kHz
                tmp  = (+22.f + 0.23f / x) * diff;
                tmp += 8. * minf ( (diff-0.5f) * (diff-0.5f) - 2 * (diff-0.5f), 0.f );  // Einsattlung (bis 6 dB)
            }

            SPRD [i] [j] = POW10 ( -0.1 * tmp );                                        // [Quelle] [Ziel]
        }
    }

    // Normierung
    for ( i = 0; i < PART_LONG; i++ ) {                 // i ist maskierte   Partition
        float  norm = 0.f;
        for ( j = 0; j < PART_LONG; j++ )               // j ist maskierende Partition
            norm += SPRD [j] [i];
        for ( j = 0; j < PART_LONG; j++ )               // j ist maskierende Partition
            SPRD [j] [i] /= norm;
    }
}


// Aufruf aller Initialisierungsprozeduren
int                                                     // number of maximum usable subbands
InitPsychoacousticTables ( float  fs,
                           float  bandwidth,
                           float  tmn,
                           float  nmt,
                           int    minvalmodel,
                           int    ltq_model,
                           float  ltq_offset,
                           float  ltq_max,
                           float  ltq_20kHz,
                           float  TD1,
                           float  TD2,
                           float  TD3,
                           float  TD4 )
{
    TonalitySetup    ( fs, LONG_ADVANCE, tmn, nmt, minvalmodel );
    TransDetectSetup ( fs,               TD1, TD2, TD3, TD4 );
    ATHSetup         ( fs, ltq_model, ltq_offset, ltq_max, ltq_20kHz );
    Loudness_Table   ( fs );
    SetupSpread      ( fs );

    return mini ( maxi ( (int) ( bandwidth * 64. / fs ), 1), 31);
}

/* end of psy_tab.c */
