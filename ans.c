/*
 *  (Spectral) Adaptive Noise Shaping (ANS) filter calculation
 *
 *  (C) Andree Buschmann 2000, Frank Klemm 2001,02. All rights reserved.
 *
 *  Principles:
 *    FindOptimalANS() calculates optimal feedback coefficients from the mask
 *    threshold which is given as 16 spectral coefficents. The 16 spectral lines
 *    comes from the 1024 pt psycho model FFT (gives 512 spectral coefficients)
 *    and the 32 subbands. Calculation is done for all subband frames from 0...MaxBand.
 *
 *  History:
 *    ca. 1998    created
 *    2001-09     added unrolled versions for durbin_akf_to_h[1,2,3]
 *                Removed tables, tables are now calculated by Init_ANS
 *    2002-06     increased ANS order up to possible 6, but there is an additional
 *                table to restrict order separate for each subband.
 *    2002-08-09  Moved setup of fir and NS into the function itself.
 *    2002-08-11  Some optimizations in the durbin_akf_to_h[1,2,3] functions.
 *    2002-08-17  Allows more noise shaping; "if(gain>ANS_Gain && ans_loss<actSMR)"
 *                is replaced by "if(gain>ANS_Gain)" -- RETARDED
 *                SinTab/CosTab calculation changed from 2*n+1 to 2*n
 *    2003-02-16  Explicitely disabled ANS on transient signals
 *
 *  TODO:
 *    - Je nach Transientität kann diese noch reduziert werden (bis hin zu 0=No ANS)
 *    - Koeffizient für Rückkopplung bei Order=1 über Mask_fu - Mask_fo abschätzen.
 *    - 3 Quantisierungsroutinen: Order=0, Order=1, Order=2...6
 *    - Ordnung gibt nicht die Stärke der Rauschformung an, sondern die Flexiblität der Form
 *    - "Reste"-Verwertung nicht an den Framegrenzen zurücksetzen, "Reste"-Verwertung als
 *    - scalenfaktorenunabhängige Werte, damit Verwertung über Subframe/Framegrenzen überhaupt möglich.
 */

#include "mppenc.h"


static float  InvFourier [MAX_ANS_ORDER + 1] [16];
static float  CosTab     [16] [MAX_ANS_ORDER + 1];
static float  SinTab     [16] [MAX_ANS_ORDER + 1];

/*
 *  Calculate helper tables InvFourier, SinTab, CosTab needed by FindOptimalANS()
 */

void
Init_ANS ( void )
{
    int  n;
    int  k;

    for ( k = 0; k <= MAX_ANS_ORDER; k++ ) {
        for ( n = 0; n < 16; n++ ) {
            InvFourier [k] [n] = (float) cos ( +2*M_PI/64 * (2*n)   *  k    ) / 16.;
            CosTab     [n] [k] = (float) cos ( -2*M_PI/64 * (2*n+0) * (k+1) );
            SinTab     [n] [k] = (float) sin ( -2*M_PI/64 * (2*n+0) * (k+1) );
        }
    }
}


// durbin_akf_to_h[1,2,3,]
// calculates optimal reflection coefficients and time response of a prediction filter in LPC analysis
//

static __inline void
durbin_akf_to_h1 ( float*        h,     // out: time response
                   const float*  akf )  // in : autocorrelation function (0..1 used)
{
    h[0] = akf [1] / akf [0];
}

static __inline void
durbin_akf_to_h2 ( float*        h,     // out: time response
                   const float*  akf )  // in : autocorrelation function (0..2 used)
{
    float  tk;

    tk    = akf [1] / akf[0];
    h[0]  = tk * (1. - (h[1]  = (akf[2] - tk * akf[1]) / (akf[0] * (1. - tk*tk))));
}

static __inline void
durbin_akf_to_h3 ( float*        h,     // out: time response
                   const float*  akf )  // in : autocorrelation function (0..3 used)
{
    float  a, b, tk, e;

    h[0]  = tk = akf[1] / akf[0];
    e     = akf[0] * (1. - tk*tk);

    h[1]  = tk = (akf[2] - tk * akf[1]) / e;
    e    *= 1. - tk*tk;
    h[0] *= 1. - tk;
    h[2]  = tk = (akf[3] - h[0] * akf[2] - h[1] * akf[1]) / e;

    h[0]  = (a=h[0]) - (b=h[1])*tk;
    h[1]  = b - a*tk;
}


static __inline void
durbin_akf_to_h ( float*        h,     // out: time response
                  const float*  akf,   // in : autocorrelation function (0..n used)
                  const int     n )    // in : number of parameters to calculate
{
    int     i, j;
    float   s, a, b, tk, e;
    float*  p;
    float*  q;

    e = akf [0];
    for ( i = 0; i < n; i++ ) {
        s = 0.f;
        p = h;
        q = akf + i;
        j = i;
        while ( j-- )
            s += *p++ * *q--;

        tk   = (akf[i+1] - s) / e;
        e   *= 1. - tk*tk;
        h[i] = tk;
        p    = h;
        q    = h + i - 1;

        for ( ; p < q; p++, q-- ) {
            a  = *p;
            b  = *q;
            *p = a - b*tk;
            *q = b - a*tk;
        }
        if ( p == q )
            *p *= 1. - tk;
    }
}


void
FindOptimalANS ( const int             MaxBand,
                 const unsigned char*  ms,
                 const float           spec0 [32] [16],
                 const float           spec1 [32] [16],
                 unsigned char*        ANS,
                 const unsigned char*  NSmaxOrder,
                 float*                snr_comp,
                 float                 fir   [32] [MAX_ANS_ORDER],
                 const float*          smr0,
                 const float*          smr1,
                 const scf_t           scf   [32] [3],
                 const int             Transient [32] )
{
    int           Band;
    int           n;
    int           k;
    int           order;
    float         akf   [MAX_ANS_ORDER + 1];
    float         h     [MAX_ANS_ORDER];
    float         spec  [16];
    float         ispec [16];
    float         norm;
    float         ans_loss;
    float         min_spec;
    float         min_diff;
    float         re;
    float         im;
    float         ans_energy;
    float         gain;
    float         ANS_Gain;
    float         actSMR;
    const float*  tmp;

    ENTER(235);

    memset ( fir , 0, sizeof (*FIR)       );          // reset FIR
    memset ( ANS , 0, sizeof (*ANS_Order) );          // reset Flags

    for ( Band = 0; Band <= MaxBand  &&  NSmaxOrder[Band]; Band++ ) {

        if ( Transient [Band] )
            continue;

        if ( scf[Band][0] != scf[Band][1]  ||  scf[Band][1] != scf[Band][2] )
            continue;

        if ( ms[Band] ) {                           // setting pointer and SMR in relation to the M/S-flag
            tmp    = spec1 [Band];                  // pointer to MS-data
            actSMR = smr1  [Band];                  // selecting SMR
        }
        else {
            tmp    = spec0 [Band];                  // pointer to LR-data
            actSMR = smr0  [Band];                  // selecting SMR
        }

        if ( actSMR >= 1. ) {
            ANS_Gain =     1.f;                      // reset gain
            norm     = 1.e-30f;

            // Selektion der Maskierungsschwelle des aktuellen Teilbands inklusive Berücksichtigung der Frequenzinversion in jedem zweiten Subband
            if ( Band & 1 )
                for ( n = 0; n < 16; n++ )
                    norm += spec[n] = tmp [15 - n];
            else
                for ( n = 0; n < 16; n++ )
                    norm += spec[n] = tmp [n];

            // Vorverarbeitung: Normierung der Leistung von spec[] auf 1 und Suche nach Minimum der Maskierungsschwelle
            norm     = 16.f / norm;
            min_spec = 1.e+12f;
            for ( n = 0; n < 16; n++ ) {
                ispec [n] = 1.f / (spec [n] *= norm);
                if ( spec [n] < min_spec )                          // normalize spec[]
                    min_spec = spec [n];
            }

            // Berechnung der Autokorrelationsfunktion
            tmp = InvFourier [0];
            for ( k = 0; k <= NSmaxOrder [Band]; k++, tmp += 16 ) {
                akf [k] = tmp[ 0]*ispec[ 0] + tmp[ 1]*ispec[ 1] + tmp[ 2]*ispec[ 2] + tmp[ 3]*ispec[ 3] +
                          tmp[ 4]*ispec[ 4] + tmp[ 5]*ispec[ 5] + tmp[ 6]*ispec[ 6] + tmp[ 7]*ispec[ 7] +
                          tmp[ 8]*ispec[ 8] + tmp[ 9]*ispec[ 9] + tmp[10]*ispec[10] + tmp[11]*ispec[11] +
                          tmp[12]*ispec[12] + tmp[13]*ispec[13] + tmp[14]*ispec[14] + tmp[15]*ispec[15];
            }

            // Suche nach Noise-Shaper mit maximalem Gewinn
            for ( order = 1; order <= NSmaxOrder [Band]; order++ ) {
                switch ( order ) {                                  // Berechne optimales FIR-Filter für Rückführung
                case  1: durbin_akf_to_h1 (h, akf);        break;
                case  2: durbin_akf_to_h2 (h, akf);        break;
                case  3: durbin_akf_to_h3 (h, akf);        break;
                default: durbin_akf_to_h  (h, akf, order); break;
                }

                ans_loss = 1.e-30f;                                 // Abschätzung des Gewinns
                min_diff = 1.e+12f;

                for ( n = 0; n < 16; n++ ) {
                    switch (order) {
                    case 1:
                        re = 1.f - h[0] * CosTab [n][0];
                        im =       h[0] * SinTab [n][0];
                        break;
                    case 2:
                        re = 1.f - h[0] * CosTab [n][0] - h[1] * CosTab [n][1];
                        im =       h[0] * SinTab [n][0] + h[1] * SinTab [n][1];
                        break;
                    case 3:
                        re = 1.f - h[0] * CosTab [n][0] - h[1] * CosTab [n][1] - h[2] * CosTab [n][2];
                        im =       h[0] * SinTab [n][0] + h[1] * SinTab [n][1] + h[2] * SinTab [n][2];
                        break;
                    default:
                        re = 1.f - h[0] * CosTab [n][0];
                        im =       h[0] * SinTab [n][0];
                        for ( k = 1; k < order; k++ ) {
                            re -= h[k] * CosTab [n][k];
                            im += h[k] * SinTab [n][k];
                        }
                        break;
                    }

                    ans_loss += ans_energy = re*re + im*im;         // calculated spectral shaped noise; noise energy increases with shaping

                    if ( spec [n] < min_diff * ans_energy )          // Suche nach minimalem Abstand von geformtem Rauschen zur Maskierungsschwelle
                        min_diff = spec [n] / ans_energy;
                }

                // Update des Filters, falls neuer Gewinn größer als der alte und die zusätzliche Rauschleistung durch Formung kleiner dem SMR dieses Bands
                gain = 16. * min_diff / (min_spec * ans_loss);
#if 1
                if ( gain > ANS_Gain  &&  ans_loss < actSMR )
#else
                if ( gain > ANS_Gain )
#endif
                {
                    ANS [Band] = order;
                    ANS_Gain   = gain;
                    memcpy ( fir [Band], h, order * sizeof(*h) );
                }
            }

            if ( ANS_Gain > 1.f )                                    // Aktivierung von ANS, falls Gewinn vorhanden
                snr_comp [Band] *= ANS_Gain;
        }
    }

    LEAVE(235);
    return;
}

/* end of ans.c */
