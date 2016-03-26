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

#include "mppenc.h"


// Quantisierungs-Koeffizienten: step/65536 bzw. (2*D[Res]+1)/65536
static const float  A [22] = {
    (           0), (    0.0000762939453125f),
    (           0), (    3./65536),
    (    5./65536), (    7./65536),
    (    9./65536), (   11./65536),
    (   15./65536), (   21./65536),
    (   31./65536), (   63./65536),
    (  127./65536), (  255./65536),
    (  511./65536), ( 1023./65536),
    ( 2047./65536), ( 4095./65536),
    ( 8191./65536), (16383./65536),
    (32767./65536), (65535./65536),
};

// Requantisierungs-Koeffizienten: 65536/step bzw. 1/A[Res]
static const float  C [22] = {
    65536.f/    1, 13107.2f,
                0, 65536.f/    3,
    65536.f/    5, 65536.f/    7,
    65536.f/    9, 65536.f/   11,
    65536.f/   15, 65536.f/   21,
    65536.f/   31, 65536.f/   63,
    65536.f/  127, 65536.f/  255,
    65536.f/  511, 65536.f/ 1023,
    65536.f/ 2047, 65536.f/ 4095,
    65536.f/ 8191, 65536.f/16383,
    65536.f/32767, 65536.f/65535,
};


// Requantisierungs-Offset: 2*D+1 = steps of quantizer
static const int           Dplus  [22] = { 0, 0, 0, 1, 2, 3, 4,  5,  7, 10, 15, 31,  63, 127, 255,  511, 1023, 2047, 4095,  8191, 16383, 32767 };
static const int           Dminus [22] = { 0, 0, 0,-1,-2,-3,-4, -5, -7,-10,-15,-31, -64,-128,-256, -512,-1024,-2048,-4096, -8192,-16384,-32768 };
static const unsigned int  Dmax   [22] = { 0, 0, 0, 2, 4, 6, 8, 10, 14, 20, 30, 62, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535 };


static float  NoiseInjectionCompensation1D [22] = {
    1.f,
    1.f,
    1.f,
    0.884621f,
    0.935711f,
    0.970829f,
    0.987941f,
    (0.987941f+0.994315f)/2,
    0.994315f,
    (0.994315f+0.997826f)/2,
    0.997826f,
    0.999744f,
    1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
} ;


// Generierung der Skalenfaktoren und ihrer Inversen
void
Init_SCF ( void )
{
    int  n;

    for ( n = 0; n < 128; n++ ) {
        SCF[n]    = (float) pow ( 2., 0.25 * n - 30);
        invSCF[n] = 1. / SCF[n];
    }
}


void
UndoNoiseInjectionComp ( void )
{
    int  i;

    for ( i = 0; i < sizeof(NoiseInjectionCompensation1D)/sizeof(*NoiseInjectionCompensation1D); i++ )
        NoiseInjectionCompensation1D [i] = 1.f;
}


int
MinRes_Estimator ( float SMR )
{
    if ( SMR <     1.000 ) return  0;
    if ( SMR <    16.588 ) return  3;
    if ( SMR <    67.928 ) return  4;
    if ( SMR <    78.091 ) return  5;
    if ( SMR <   137.817 ) return  6;
    if ( SMR <   200.544 ) return  7;
    if ( SMR <   373.397 ) return  8;
    if ( SMR <   766.333 ) return  9;
    if ( SMR <  1538.355 ) return 10;
    if ( SMR <  6675.220 ) return 11;
    if ( SMR < 27341.691 ) return 12;
    return 13;
}


// Quantisiert ein Subband und berechnet SNR
float
SNR_Estimator ( const float* input, const int res )
{
    int    k;
    float  fac    = A [res];
    float  invfac = C [res];
    float  Signal = 1.e-30f;
    float  Fehler = 1.e-30f;
    float  tmp ;
    float  tmp2 ;

    // Summation der absoluten Leistung und des quadratischen Fehlers
    for ( k = 0; k < 36; k++ ) {
        tmp2    = input[k] * NoiseInjectionCompensation1D [res];
        Signal += tmp2 * tmp2;

        tmp = tmp2 * fac + 0xC00000;                                    // q = ftol(in), korrektes Runden
        tmp = (*(int*) & tmp - 0x4B400000) * invfac - tmp2;

        Fehler += tmp * tmp;
    }

    return Fehler / Signal;
}


// Quantisiert ein Subband und berechnet SNR
float
SNR_Estimator_Trans ( const float* input, const int res )
{
    int    k;
    float  fac    = A [res];
    float  invfac = C [res];
    float  Signal;
    float  Fehler;
    float  tmp ;
    float  tmp2 ;
    float  ret;

    // Summation der absoluten Leistung und des quadratischen Fehlers
    k = 0;
    Signal = Fehler = 1.e-30f;
    for ( ; k < 12; k++ ) {
        tmp2    = input[k] * NoiseInjectionCompensation1D [res];
        Signal += tmp2 * tmp2;

        tmp = tmp2 * fac + 0xC00000;                                    // q = ftol(in), korrektes Runden
        tmp = (*(int*) & tmp - 0x4B400000) * invfac - tmp2;

        Fehler += tmp * tmp;
    }
    tmp = Fehler / Signal;
    ret = tmp;

    Signal = Fehler = 1.e-30f;
    for ( ; k < 24; k++ ) {
        tmp2    = input[k] * NoiseInjectionCompensation1D [res];
        Signal += tmp2 * tmp2;

        tmp = tmp2 * fac + 0xC00000;                                    // q = ftol(in), korrektes Runden
        tmp = (*(int*) & tmp - 0x4B400000) * invfac - tmp2;

        Fehler += tmp * tmp;
    }
    tmp = Fehler / Signal;
    if ( tmp > ret ) ret = tmp;

    Signal = Fehler = 1.e-30f;
    for ( ; k < 36; k++ ) {
        tmp2    = input[k] * NoiseInjectionCompensation1D [res];
        Signal += tmp2 * tmp2;

        tmp = tmp2 * fac + 0xC00000;                                    // q = ftol(in), korrektes Runden
        tmp = (*(int*) & tmp - 0x4B400000) * invfac - tmp2;

        Fehler += tmp * tmp;
    }
    tmp = Fehler / Signal;
    if ( tmp > ret ) ret = tmp;

    return ret;
}


// Linearer Quantisierer für ein Subband
void
QuantizeSubband ( unsigned int* qu_output, const float* input, const int res, float* errors )
{
    int    n;
    float  mult    = A [res] * NoiseInjectionCompensation1D [res];
    float  invmult = C [res];
    float  tmp;
    unsigned int    quant;
    float  signal;

    for ( n = 0; n < 36 - MAX_ANS_ORDER; n++, input++, qu_output++ ) {
        // q = ftol(in), korrektes Runden
        tmp   = *input * mult + 0xC00000;
        quant = (unsigned int) (*(int*) & tmp - 0x4B400000 - Dminus[res]);

        // Begrenzung auf 0...2D
        if ( quant > Dmax[res] ) {
            if ( (int)quant < 0 )
                quant = 0;
            else
                quant = Dmax[res];
        }
        *qu_output  = quant;
    }

    for ( ; n < 36; n++, input++, qu_output++ ) {
        // q = ftol(in), korrektes Runden
        signal = *input * mult;
        tmp   =  signal + 0xC00000;
        quant = (unsigned int)(*(int*) & tmp - 0x4B400000 - Dminus[res]);

        // Berechnung des aktuellen Fehlers und Speichern für Fehlerrückführung
        errors [n + 6] = invmult * (quant + Dminus[res]) - signal * NoiseInjectionCompensation1D [res];

        // Begrenzung auf 0...2D
        if ( quant > Dmax[res] ) {
            if ( (int)quant < 0 )
                quant = 0;
            else
                quant = Dmax[res];
        }
        *qu_output  = quant;
    }
}


// NoiseShaper für ein Subband
void
QuantizeSubband_ANS ( unsigned int* qu_output, const float* input, const int res, float* errors, const float* FIR )
{
#define E(x) *((int*)errors+(x))

    float  signal;
    float  tmp;
    float  mult    = A [res];
    float  invmult = C [res];
    int    n;
    int    quant;

    E(0) = E(1) = E(2) = E(3) = E(4) = E(5) = 0;       // arghh, das gibt ja Knackser an jeder Framegrenze

    for ( n = 0; n < 36; n++, input++, qu_output++ ) {
        signal = *input * NoiseInjectionCompensation1D [res] - (FIR[5]*errors[n+0] + FIR[4]*errors[n+1] + FIR[3]*errors[n+2] + FIR[2]*errors[n+3] + FIR[1]*errors[n+4] + FIR[0]*errors[n+5]);

        // quant = ftol(signal), korrektes Runden
        tmp   = signal * mult + 0xC00000;
        quant = *(int*) & tmp - 0x4B400000;

        // Berechnung des aktuellen Fehlers und Speichern für Fehlerrückführung
        errors [n + 6] = invmult * quant - signal * NoiseInjectionCompensation1D [res];

        // Begrenzung auf +/-D
        quant = minf ( quant, Dplus [res] );
        quant = maxf ( quant, Dminus[res] );

        *qu_output = (unsigned int)(quant - Dminus[res]);
    }
}

/* end of quant.c */
