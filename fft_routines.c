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

#define CX0     -1.
#define CX1      0.5

#define SX1     -1.
#define SX2      (2./9/  1)
#define SX3      (2./9/  4)
#define SX4      (2./9/ 10)
#define SX5      (2./9/ 20)
#define SX6      (2./9/ 35)
#define SX7      (2./9/ 56)
#define SX8      (2./9/ 84)
#define SX9      (2./9/120)
#define SX10     (2./9/165)


#ifdef EXTRA_DECONV
# define DECONV \
    {  \
    tmp      = (CX0*aix[0] + CX1*aix[2]) * (1./(CX0*CX0+CX1*CX1)); \
    aix[ 0] -= CX0*tmp; \
    aix[ 2] -= CX1*tmp; \
    tmp      = (SX1*aix[3] + SX2*aix[5] + SX3*aix[7] + SX4*aix[9] + SX5*aix[11]) * (1./(SX1*SX1+SX2*SX2+SX3*SX3+SX4*SX4+SX5*SX5)); \
    aix[ 3] -= SX1*tmp; \
    aix[ 5] -= SX2*tmp; \
    aix[ 7] -= SX3*tmp; \
    aix[ 9] -= SX4*tmp; \
    aix[11] -= SX5*tmp; \
    }
#elif 0
# define DECONV \
    {  \
    float A[20]; \
    int   i; \
    memcpy (A, aix, 20*sizeof(aix)); \
    tmp      = (CX0*aix[0] + CX1*aix[2]) * (1./(CX0*CX0+CX1*CX1)); \
    aix[ 0] -= CX0*tmp; \
    aix[ 2] -= CX1*tmp; \
    tmp      = (SX1*aix[3] + SX2*aix[5] + SX3*aix[7] + SX4*aix[9] + SX5*aix[11]) * (1./(SX1*SX1+SX2*SX2+SX3*SX3+SX4*SX4+SX5*SX5)); \
    aix[ 3] -= SX1*tmp; \
    aix[ 5] -= SX2*tmp; \
    aix[ 7] -= SX3*tmp; \
    aix[ 9] -= SX4*tmp; \
    aix[11] -= SX5*tmp; \
    for ( i=0; i<10; i++) \
        printf ("%u%9.0f%7.0f%9.0f%7.0f\n",i, A[i+i], A[i+i+1], aix[i+i], aix[i+i+1] ); \
    }
#else
# define DECONV
#endif


static float  Hann_64   [  64];
static float  Hann_256  [ 256];
static float  Hann_1024 [1024];
static float  Hann_1792 [1792];
static int    ip        [4096];   // bitinverse für maximal 2048er FFT
static float  w         [4096];   // butterfly-koeffizienten für maximal 2048er FFT
static float  a         [4096];   // holds real input for FFT


//////////////////////////////
//
// BesselI0 -- Regular Modified Cylindrical Bessel Function (Bessel I).
//

static double
Bessel_I_0 ( double x )
{
    double  denominator;
    double  numerator;
    double  z;

    if (x == 0.)
        return 1.;

    z = x * x;
    numerator = z* (z* (z* (z* (z* (z* (z* (z* (z* (z* (z* (z* (z* (z*
                   0.210580722890567e-22  + 0.380715242345326e-19 ) +
                   0.479440257548300e-16) + 0.435125971262668e-13 ) +
                   0.300931127112960e-10) + 0.160224679395361e-07 ) +
                   0.654858370096785e-05) + 0.202591084143397e-02 ) +
                   0.463076284721000e+00) + 0.754337328948189e+02 ) +
                   0.830792541809429e+04) + 0.571661130563785e+06 ) +
                   0.216415572361227e+08) + 0.356644482244025e+09 ) +
                   0.144048298227235e+10;

    denominator = z* (z* (z - 0.307646912682801e+04) + 0.347626332405882e+07) - 0.144048298227235e+10;

    return - numerator / denominator;
}

static double
residual ( double x )
{
    return sqrt ( 1. - x*x );
}

//////////////////////////////
//
// KBDWindow -- Kaiser Bessel Derived Window
//      fills the input window array with size samples of the
//      KBD window with the given tuning parameter alpha.
//


static void
KBDWindow ( float* window, unsigned int size, float alpha )
{
    double        sumvalue = 0.;
    double        scale;
    unsigned int  i;

    scale = 0.25 / sqrt (size);
    for ( i = 0; i < size/2; i++ )
        window [i] = sumvalue += Bessel_I_0 ( M_PI * alpha * residual (4.*i/size - 1.) );

    // need to add one more value to the nomalization factor at size/2:
    sumvalue += Bessel_I_0 ( M_PI * alpha * residual (4.*(size/2)/size-1.) );

    // normalize the window and fill in the righthand side of the window:
    for ( i = 0; i < size/2; i++ )
        window [size-1-i] = window [i] = /*sqrt*/ ( window [i] / sumvalue ) * scale;
}

static void
CosWindow ( float* window, unsigned int size )
{
    double        x;
    double        scale;
    unsigned int  i;

    scale = 0.25 / sqrt (size);
    for ( i = 0; i < size/2; i++ ) {
        x = cos ( (i+0.5) * (M_PI / size) );
        window [size/2-1-i] = window [size/2+i] = scale * x * x;
    }
}

static void
CosSinWindow ( float* window, unsigned int size )
{
    double        x;
    double        scale;
    unsigned int  i;

    scale = 0.25 / sqrt (size);
    for ( i = 0; i < size/2; i++ ) {
        x = sin ( (i+0.5) * (M_PI / size) );
        x = cos (x * x * M_PI/2);
        window [size/2-1-i] = window [size/2+i] = scale * x;
    }
}

static void
Window ( float* window, unsigned int size, float alpha )
{
    if ( alpha < -1. )
        CosSinWindow ( window, size );
    else if ( alpha <  0. )
        CosWindow ( window, size ) ;
    else
        KBDWindow ( window, size, alpha );
}



// generates FFT lookup-tables
void
Init_FFT ( float alpha0, float alpha1, float alpha2, float alpha3 )
{
    // normalized windows
    Window ( Hann_64  ,   64, alpha0 );
    Window ( Hann_256 ,  256, alpha1 );
    Window ( Hann_1024, 1024, alpha2 );
    Window ( Hann_1792, 1792, alpha3 );

    Generate_FFT_Tables ( 2048, ip, w );
}

// input : Signal *x
// output: Leistungsspektrum *erg
void
PowSpec64 ( const float* x, float* erg )
{
    const float*  win = Hann_64;
    float*        aix = a;
    int           i;

    ENTER(40);
    // windowing
    i = 64;
    while (i--)
        *aix++ = *x++ * *win++;

    // perform FFT
    rdft ( 64, a, ip, w );

    // calculate power
    aix = a;    // reset pointer
    i   = 32;
    while (i--) {
        *erg++ = aix[0]*aix[0] + aix[1]*aix[1];
        aix += 2;
    }
    LEAVE(40);
}

// input : Signal *x
// output: Leistungsspektrum *erg
void
PowSpec256 ( const float* x, float* erg )
{
    const float*  win = Hann_256;
    float*        aix = a;
    int           i;

    ENTER(40);
    // windowing
    i = 256;
    while (i--)
        *aix++ = *x++ * *win++;

    // perform FFT
    rdft ( 256, a, ip, w );

    // calculate power
    aix = a;    // reset pointer
    i   = 128;
    while (i--) {
        *erg++ = aix[0]*aix[0] + aix[1]*aix[1];
        aix += 2;
    }
    LEAVE(40);
}

// input : Signal *x
// output: Leistungsspektrum *erg
void
PowSpec1024 ( const float* x, float* erg )
{
    const float*  win = Hann_1024;
    float*        aix = a;
    int           i;

    ENTER(41);
    i = 1024;                   // windowing
    while (i--)
        *aix++ = *x++ * *win++;

    rdft ( 1024, a, ip, w );    // perform FFT

    aix = a;                    // calculate power
    i   = 512;

    DECONV;
    while (i--) {
        *erg++ = aix[0]*aix[0] + aix[1]*aix[1];
        aix += 2;
    }
    LEAVE(41);
}


// input : Signal *x
// output: Leistungsspektrum *erg
void
PowSpec1024_Mix ( const float* x0, const float* x1, float* ergsum, float* ergdif )
{
    const float*  win = Hann_1024;
    float*        aix = a;
    int           i;

    ENTER(41);
    i = 1024;                   // windowing
    while (i--)
        *aix++ = (*x0++ + *x1++) * *win++;

    rdft ( 1024, a, ip, w );    // perform FFT

    aix = a;                    // calculate power
    i   = 512;

    while (i--) {
        *ergsum++ = 0.5f * (aix[0]*aix[0] + aix[1]*aix[1]);
        aix += 2;
    }

    win = Hann_1024;
    aix = a;
    x0 -= 1024;
    x1 -= 1024;

    i = 1024;                   // windowing
    while (i--)
        *aix++ = (*x0++ - *x1++) * *win++;

    rdft ( 1024, a, ip, w );    // perform FFT

    aix = a;                    // calculate power
    i   = 512;

    while (i--) {
        *ergdif++ = 0.5f * (aix[0]*aix[0] + aix[1]*aix[1]);
        aix += 2;
    }

    LEAVE(41);
}


// input : Signal *x
// output: Leistungsspektrum *erg
void
PowSpec2048 ( const float* x, float* erg )
{
    const float*  win = Hann_1792;
    float*        aix = a;
    int           i;

    ENTER(42);
    // windowing (only 1600 samples available -> centered in 2048!)
    memset ( a     , 0, 128 * sizeof(*a) );
    aix = a + 128;
    i   = 1792;
    while ( i-- )
        *aix++ = *x++ * *win++;
    memset ( a+1920, 0, 128 * sizeof(*a) );

    rdft ( 2048, a, ip, w );    // perform FFT

    aix = a;                    // calculate power
    i   = 1024;
    while (i--) {
        *erg++ = aix[0]*aix[0] + aix[1]*aix[1];
        aix += 2;
    }
    LEAVE(42);
}

#include "fastmath.h"

// input : Signal *x
// output: Leistungsspektrum *erg und Phasenspektrum *phs
void
PolarSpec1024 ( const float* x, float* erg, float* phs )
{
    const float*  win = Hann_1024;
    float*        aix = a;
    int           i;

    ENTER(43);
    i = 1024;                   // windowing
    while (i--)
        *aix++ = *x++ * *win++;

    rdft ( 1024, a, ip, w );    // perform FFT

    // calculate power and phase
    aix = a;    // reset pointer
    i   = 512;
    while (i--) {
        *erg++ = aix[0]*aix[0] + aix[1]*aix[1];
        *phs++ = ATAN2F (aix[1], aix[0]);
        aix += 2;
    }
    LEAVE(43);
}

// input : logarithmiertes Leistungsspektrum *cep
// output: Cepstrum *cep (in-place)
void
Cepstrum2048 ( float* cep, const int MaxLine )
{
    float*  aix = cep;
    float*  bix = cep + 2048;
    int     i;

    ENTER(44);
    // generate real, even spectrum (symmetric around 1024, cep[2048-i] = cep[i])
    for ( i = 0; i < 1024; i++ )
        *bix-- = *aix++;

    // perform IFFT
    rdft ( 2048, cep, ip, w );

    // nur Realteil als Ausgang (alle geraden Indizes von cep[])
    aix = cep;
    bix = cep;
    i   = MaxLine + 1;
    while (i--) {
        *aix = *bix * (1.f / 2048.f);
        aix ++;
        bix += 2;
    }
    LEAVE(44);
}

/* end of fft_routines.c */
