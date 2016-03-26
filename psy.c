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
 *  Prediction
 *  Short-Block-Erkennung mit weichem Einsatz
 *  CalcMSThreshold überarbeiten
 *  PNS/IS überarbeiten
 *  CVS mit weicherem Einsatz
 *  ANS bei sich ändernden SCFs überarbeiten

  * No IS
  * PNS estimation very rough, also IS should be used to reduce data rate in the side channel
  * ANS problems at Frame boundaries when resolution changes
  * ANS problems at Subframe boundaries when SCF changes
  * CVS+ with smoother transition

----------------------------------------

Tabelle[18] optimieren (zweite Tabelle benutzen)
CVS+

- Bei der Suche des besten Res wird ANS nicht berücksichtigt.
- ANS macht murks, wenn res (alle 36 samples) und/oder SCF (alle 12 samples) sich ändert.
- PNS nicht im Differenzsignal.

- IS in Decoder einbauen
- Experimenteller Quantisierer mit vollständiger Energieerhaltung.
  - 1D, berechnet
  - 2D, berechnet
  - 2D, manuell nachbearbeitet, auf 1.f gesetzte Koeffs

 */

#include "mppenc.h"
#include "fastmath.h"


/* weitere Switches für das Psychoakustische Modell */
static float         O_MAX;
static float         O_MIN;
static float         FAC1;
static float         FAC2;

static float         integra  [MAXCH] [2] [PART_LONG];            // Integrationen für tmpMask
static float         tau      [MAXCH]     [PART_LONG];            // für Post-Maskierung L/R
static float         FFT_Ampl [MAXCH] [3 * 512];                  // FFT-Amplituden L/R
static float         FFT_Phas [MAXCH] [3 * 512];                  // FFT-Phasen L/R
static float         pre_erg  [MAXCH] [2] [PART_SHORT];           // Preecho-Kontrolle short
static float         pre_ergU [MAXCH] [2] [PART_ULTRASHORT];      // Preecho-Kontrolle ultrashort
static float         PreThr   [MAXCH]     [PART_LONG];            // für Pre-Echo-Kontrolle L/R
static float         tmp_Mask [MAXCH]     [PART_LONG];            // für Post-Maskierung L/R
static unsigned char Vocal    [MAXCH]     [MAX_CVD_LINE + 4];     // FFT-Linie zu Harmonischer gehörig?
static float         loud     [MAXCH];

static unsigned int  const52;           // for 44.1 kHz: 52     (or 37 ... 46, 48, 57)
static int           const5;            // for 44.1 kHz: 5
static int           const8;            // for 44.1 kHz: 8
static float         const0_0574;       // for 44.1 kHz: 0.05737540597
static float         const0_5871;       // for 44.1 kHz: 0.58710116030
static float         const0_02;         // for 44.1 kHz: 0.02
static float         const0_15;         // for 44.1 kHz: 0.15
static float         const0_50;         // for 44.1 kHz: 0.50
static float         TransD1;
static float         TransD2;
static float         TransD3;
static float         TransD4;
static float         cvd_unpred;


static float
MinValue ( float f, float tmn, float nmt, float bass )
{
    static unsigned char  lfe [11] = { 120, 100, 80, 60, 50, 40, 30, 20, 15, 10, 5 };
    int                   tmp      = (int) ( 1024/44100. * f + 0.5 );

    switch ( tmp ) {
    case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7: case  8: case  9: case 10:  // <  450 Hz
        return tmn + bass * lfe [tmp];
    case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18:                             // <  800 Hz
        return tmn;
    case 19: case 20: case 21: case 22:                                                                 // <  970 Hz
        return 0.75*tmn + 0.25*nmt;
    case 23: case 24:                                                                                   // < 1050 Hz
        return 0.50*tmn + 0.50*nmt;
    case 25: case 26:                                                                                   // < 1140 Hz
        return 0.25*tmn + 0.75*nmt;
    default:                                                                                            // > 1140 Hz
        return nmt;
    }
}


// Berechnung der Koeffizienten für Anwendung des Tonalitätsoffset, abhängig von TMN und NMT
void
TonalitySetup ( float  fs,
                int    advance,
                float  tmn,
                float  nmt,
                int    minvalmodel )
{
    double  tmp;
    int     n;
    float   bass;

    // Subband which goes up to 14.6 kHz
    for ( const52 = PART_LONG; wh [const52-1] * fs > 14600. * 1024; )
        const52--;

    const5      = (int) ceil ( 5 * 44100. / fs );
    const8      = (int) ceil ( 8 * 44100. / fs );
    const0_0574 = 0.05737540597f * 44100. / fs * advance / 576.;
    const0_5871 = 0.58710116030f * 44100. / fs * advance / 576.;
    const0_02   = 1. - pow ( 0.98, 44100. / fs * advance / 576. );
    const0_15   = 1. - pow ( 0.85, 44100. / fs * advance / 576. );
    const0_50   = 1. - pow ( 0.50, 44100. / fs * advance / 576. );

    cvd_unpred  = CVD_UNPRED * 44100. / fs * advance / 576.;

    bass = 0.1/8 * nmt;
    if ( minvalmodel <= 2  &&  bass > 0.1f )
        bass = 0.1f;
    if ( minvalmodel <= 1 )
        bass = 0.0f;

    // alternativ: Berechnung der minval-Werte in Abhängigkeit von TMN und TMN
    for ( n = 0; n < PART_LONG; n++ ) {
        tmp        = MinValue ( (wl [n] + wh [n]) / 2048. * fs, tmn, nmt, bass );
        MinVal [n] = POW10 ( -0.1 * tmp );                      // Umrechnung in Leistung
    }

    // Berechnung der Konstanten für "Tonalitätsoffset"
    O_MAX = POW10 ( -0.1 * tmn );
    O_MIN = POW10 ( -0.1 * nmt );
    FAC2  = ( log(O_MAX) - log(O_MIN) ) / ( log(const0_0574) - log(const0_5871) );
    FAC1  = O_MAX * exp ( -log(const0_0574) * FAC2 ) ;
}


void
TransDetectSetup ( float  fs,
                   float  TransDetect1,
                   float  TransDetect2,
                   float  TransDetect3,
                   float  TransDetect4 )
{                                       //                           44.1 kHz
    TransD1 = TransDetect1 * fs/44100;  // 100: 20  dB/128 Samples    6.89 dB/ms
    TransD2 = TransDetect2 * fs/44100;  // 200: 23  dB/256 Samples    3.96 dB/ms
    TransD3 = TransDetect3 * fs/44100;  //  50: 17  dB/ 32 Samples   23.43 dB/ms
    TransD4 = TransDetect4 * fs/44100;  //  71: 18.5dB/ 64 Samples   12.75 dB/ms
}


// Resets Arrays
void
ResetPsychoacoustic ( void )
{
    int  i;

    ENTER(200);

#if 1
    Init_FFT ( -1, -1, -1, -1 );            // generate FFT lookup-tables with largest FFT-size of 256, 1024, 2048
#else
    Init_FFT ( 4., 2., -1, -1 );            // generate FFT lookup-tables with largest FFT-size of 256, 1024, 2048
#endif

    // setting pre-echo variables to Ltq
    for ( i = 0; i < PART_LONG; i++ ) {
        pre_ergU [0] [0] [i/5] = pre_ergU [1] [0] [i/5] =
        pre_ergU [0] [1] [i/5] = pre_ergU [1] [1] [i/5] =
        pre_erg  [0] [0] [i/3] = pre_erg  [1] [0] [i/3] =
        pre_erg  [0] [1] [i/3] = pre_erg  [1] [1] [i/3] =
        tmp_Mask [0] [i]       = tmp_Mask [1] [i]       =
        PreThr   [0] [i]       = PreThr   [1] [i]       = partLtq [i];
    }

    // initializing arrays with zero
    memset ( FFT_Ampl, 0, sizeof FFT_Ampl );
    memset ( FFT_Phas, 0, sizeof FFT_Phas );
    memset ( integra , 0, sizeof integra  );
    memset ( tau     , 0, sizeof tau      );
    memset ( loud    , 0, sizeof loud     );

    LEAVE(200);
    return;
}


// VBRmode 1: Anpassung aller SMR's mittels eines Faktors (Offset von SMRoffset dB)
// VBRmode 2: SMR's haben ein Minimum von minSMR dB
static void
RaiseSMR_Signal ( const int  MaxBand,
                  float*     signal,
                  float      tmp )
{
    int    Band;
    float  z = 0.;

    for ( Band = MaxBand; Band >= 0; Band-- ) {
        if ( z < signal [Band]  ) z = signal [Band];
        if ( z > tmp            ) z = tmp;
        if ( signal [Band]  < z ) signal [Band] = z;
    }
}


void
RaiseSMR ( const int  MaxBand,
           SMR_t*     smr,
           float      minSMR )
{
    float  tmp = POW10 ( 0.1 * minSMR );

    ENTER(201);
    RaiseSMR_Signal ( MaxBand, smr->Ch0, tmp );
    RaiseSMR_Signal ( MaxBand, smr->Ch1, tmp );
    RaiseSMR_Signal ( MaxBand, smr->Ch2, tmp );
    RaiseSMR_Signal ( MaxBand, smr->Ch3, tmp * 0.5 );

    LEAVE(201);
    return;
}


// input : *smr
// output: *smr, *ms, *x        (nur die Einträge für L/R enthalten relevante Daten)
// Überprüfe, ob M/S- oder L/R-coding die geringere perceptual entropy aufweist
// Wähle den günstigeren Mode aus, kopiere die entsprechenden Daten in die
// zu L und R gehörigen Arrays und setze das ms-Flag entsprechend
void
MS_LR_Entscheidung ( const int   MaxBand,
                     pack_t*     ms,
                     SMR_t*      smr,
                     Float       S [] [32] [36] )
{
    int     Band;
    int     n;
    float   PE_MS;
    float   PE_LR;
    float   tmpM;
    float   tmpS;
    float*  l;
    float*  r;

    ENTER(202);

    memset ( ms, 0, (MaxBand+1) * sizeof (*ms) );

    for ( Band = 0; Band <= MaxBand; Band++ ) {        // Berechne perceptual entropy

        PE_LR = PE_MS = 1.f;
        if ( smr -> Ch0 [Band] > 1.) PE_LR *= smr -> Ch0 [Band];
        if ( smr -> Ch1 [Band] > 1.) PE_LR *= smr -> Ch1 [Band];
        if ( smr -> Ch2 [Band] > 1.) PE_MS *= smr -> Ch2 [Band];
        if ( smr -> Ch3 [Band] > 1.) PE_MS *= smr -> Ch3 [Band];

        if ( PE_MS < PE_LR ) {
            ms [Band] = 1;

            // calculate M/S-signal and copies it to L/R-array
            l = S[0][Band];
            r = S[1][Band];
            for ( n = 0; n < 36; n++, l++, r++ ) {
                tmpM = (*l + *r) * 0.70710678119f;
                tmpS = (*l - *r) * 0.70710678119f;
                *l   = tmpM;
                *r   = tmpS;
            }

            // copy M/S-SMR to L/R-SMR
            smr -> Ch0[Band] = smr -> Ch2[Band];
            smr -> Ch1[Band] = smr -> Ch3[Band];
        }
    }

    LEAVE(202);
    return;
}

// input : FFT-Spektren *spec0 und *spec1
// output: Leistung in den einzelnen Subbändern *erg0 und *erg1
// Mittels Butfly[] kann die Auswirkung von Aliasing bei der Berechnung der Subbandenergien
// aus den FFT-Spektren berücksichtigt werden.
static void
SubbandEnergy ( const int     MaxBand,
                float*        erg,
                const float*  spec )
{
    int    n;
    int    k;
    int    alias;
    float  tmp;

    ENTER(203);

    // Ist das hier richtig für FFT basierte Daten (von welchen eigentlich?) oder gilt diese Rechenvorschrift nur für MDCTs ???

    for ( k = 0; k <= MaxBand; k++ ) {                  // subband index
        tmp = 0.f;
        for ( n = 0; n < 16; n++, spec++ ) {            // spectral index
            tmp += *spec;

            // Berücksichtigung von Aliasing zwischen den Teilbändern
            if      ( n <   +sizeof(Butfly)/sizeof(*Butfly)  &&  k !=  0 ) {
                alias = -1 - (n<<1);
                tmp  += Butfly [n]    * (spec[alias] - *spec);
            }
            else if ( n > 15-sizeof(Butfly)/sizeof(*Butfly)  &&  k != 31 ) {
                alias = 31 - (n<<1);
                tmp  += Butfly [15-n] * (spec[alias] - *spec);
            }
        }
        *erg++ = tmp;
    }

    LEAVE(203);
    return;
}


// input : FFT-Spektren *spec0 und *spec1
// output: Leistung in den einzelnen Partitionen *erg0 und *erg1
static void
PartitionEnergy ( float*        erg,
                  const float*  spec )
{
    unsigned int  n;
    unsigned int  k;
    float         e;

    ENTER(204);

#if 000000
    for ( n = 0; n < PART_LONG; n++ ) {
        k = wh[n] - wl[n];
        e = *spec++;
        while ( k-- ) {
            e += *spec++;
        }
        *erg++ = e;
    }
#else
    n = 0;

    for ( ; n < 26; n++ ) {
        k = wh[n] - wl[n];
        e = *spec++;
        while ( k-- ) {
            e += *spec++;
        }
        *erg++ = e;
    }

    for ( ; n < const52; n++ ) {
        k = wh[n] - wl[n];
        e = sqrt (*spec++);
        while ( k-- ) {
            e += sqrt (*spec++);
        }
        *erg++ = e*e * iw[n];
    }

    for ( ; n < PART_LONG; n++ ) {
        k  = wh[n] - wl[n];
        e = *spec++;
        while ( k-- ) {
            e += *spec++;
        }
        *erg++ = e;
    }

#endif

    LEAVE(204);
    return;
}


// input : FFT-Spektren *spec0, *spec1 und unpredictability *cw0 und *cw1
// output: gewichtete Leistung in den einzelnen Partitionen *erg0, *erg1
static void
WeightedPartitionEnergy ( float*        erg,
                          const float*  spec,
                          const float*  cw )
{
    unsigned int  n;
    unsigned int  k;
    float         e;

    ENTER(205);

#if 000000
    for ( n = 0; n < PART_LONG; n++ ) {
        e = *spec++ * *cw++;
        k = wh[n] - wl[n];
        while ( k-- ) {
            e += *spec++ * *cw++;
        }
        *erg++ = e;
    }
#else
    n = 0;

    for ( ; n < 26; n++ ) {
        e = *spec++ * *cw++;
        k = wh[n] - wl[n];
        while ( k-- ) {
            e += *spec++ * *cw++;
        }
        *erg++ = e;
    }

    for ( ; n < const52; n++ ) {
        e = sqrt (*spec++ * *cw++);
        k = wh[n] - wl[n];
        while ( k-- ) {
            e += sqrt (*spec++ * *cw++);
        }
        *erg++ = e*e * iw[n];
    }

    for ( ; n < PART_LONG; n++ ) {
        e = *spec++ * *cw++;
        k = wh[n] - wl[n];
        while ( k-- ) {
            e += *spec++ * *cw++;
        }
        *erg++ = e;
    }
#endif

    LEAVE(205);
    return;
}

// input : Maskierungsschwellen,  erste Hälfte der arrays *shaped0 und *shaped1
// output: Maskierungsschwellen, zweite Hälfte der arrays *shaped0 und *shaped1
// Mittels InvButfly[] wird die Auswirkung von Aliasing berücksichtigt.
// Der Input *thr0, *thr1 wird durch Adreßberechnung aus *shaped0, *shaped1
// gewonnen.
static void
AdaptThresholds ( const int  MaxLine,           // Bewirkt die Funktion überhaupt was ????
                  float*     thr0 )
{
    int           n;
    int           mod;
    int           alias;
    float         tmp;
    const float*  invb = InvButfly;
    float         tmp0;
    float         T [512];
    float*        shaped = T;
    float*        p = thr0;

    ENTER(206);

    // sollte durch Ausrollen deutlich optimierbar sein.  [ 9 ] + n * [ 7 + 7 + 2 ] + [ 7 ]
    //                                                    Schleife    Schl Schl Ausr  Schleife
    for ( n = 0; n < MaxLine; n++, p++ ) {
        mod  = n & 15;
        tmp0 = *p;

        if      ( mod <   +sizeof(InvButfly)/sizeof(*InvButfly)  &&  n >  12 ) {
            alias = -1 - (mod<<1);
            tmp   = p[alias] * invb[mod];
            if ( tmp < tmp0 ) tmp0 = tmp;
        }
        else if ( mod > 15-sizeof(InvButfly)/sizeof(*InvButfly)  &&  n < 499 ) {
            alias = 31 - (mod<<1);
            tmp   = p[alias] * invb[15-mod];
            if ( tmp < tmp0 ) tmp0 = tmp;
        }
        *shaped++ = tmp0;
    }

    memcpy ( thr0, T, MaxLine * sizeof (*thr0) );

    LEAVE(206);
    return;
}


// input : aktuelles Spektrum in Form von Leistung *spec und Phase *phase,
//         die letzten beiden vorausgegangenen Spektren sind an Position
//         512 und 1024 der jeweiligen Input-Arrays abgelegt.
//         Array *vocal, welches eine FFT_Linie als harmonisch kennzeichnen kann
// output: aktuelle Amplitude *amp und unpredictability *cw
static void
CalcUnpred ( const int             MaxLine,
             const float*          spec,
             const float*          phase,
             const unsigned char*  vocal,
             float*                amp0,
             float*                phs0,
             float*                cw )
{
    int     n;
    float   amp;
    float   tmp;
#define amp1  ((amp0) +  512)           // amp[ 512...1023] contains data of frame-1
#define amp2  ((amp0) + 1024)           // amp[1024...1535] contains data of frame-2
#define phs1  ((phs0) +  512)           // phs[ 512...1023] contains data of frame-1
#define phs2  ((phs0) + 1024)           // phs[1024...1535] contains data of frame-2

    ENTER(207);

    memmove    ( amp0 + 512, amp0, 1024 * sizeof (*amp0) );
    memmove    ( phs0 + 512, phs0, 1024 * sizeof (*phs0) );

    for ( n = 0; n < MaxLine; n++ ) {
        tmp     = COSF  ((phs0[n] = phase[n]) - 2*phs1[n] + phs2[n]);   // copy phase to output-array, predict phase and calculate predictive error
        amp0[n] = SQRTF (spec[n]);                                      // calculate and set amplitude
        amp     = 2*amp1[n] - amp2[n];                                  // predict amplitude

        // calculate unpredictability
        cw[n] = SQRTF (spec[n] + amp * (amp - 2*amp0[n] * tmp)) / (amp0[n] + FABS(amp));
    }

    // postprocessing of harmonic FFT-lines (*cw is set to CVD_UNPRED)
    if ( CVD_used  &&  vocal != NULL ) {
        for ( n = 0; n < MAX_CVD_LINE; n++, cw++, vocal++ )
            if ( *vocal != 0  &&  *cw > cvd_unpred * 0.01 * *vocal )
                *cw = cvd_unpred * 0.01 * *vocal;
    }

    LEAVE(207);
    return;
}
#undef amp1
#undef amp2
#undef phs1
#undef phs2


// input : Energie *erg, geeichtete Energie *werg
// output: spreaded energy *res, spreaded weighted energy *wres
// SPRD beschreibt die Spreadingfunktion, wie sie in psy_tab.c berechnet wird.
static void
SpreadingSignal ( const float*  erg,
                  const float*  werg,
                  float*        res,
                  float*        wres )
{
    int           n;
    int           k;
    int           start;
    int           stop;
    const float*  sprd;
    float         e;
    float         ew;

    ENTER(208);

    memset ( res , 0, PART_LONG * sizeof (*res ) );
    memset ( wres, 0, PART_LONG * sizeof (*wres) );

    for ( k = 0; k < PART_LONG; k++, erg++, werg++ ) {          // Quelle (maskierende Partition)
        start = maxi ( k - const5, 0         );                 // minimum affected partition
        stop  = mini ( k + const8, PART_LONG );                 // maximum affected partition
        sprd  = SPRD [k] + start;                               // load vector
        e     = *erg;
        ew    = *werg;

        for ( n = start; n < stop; n++, sprd++ ) {
            res [n] += *sprd * e;                               // spreading signal
            wres[n] += *sprd * ew;                              // spreading weighted signal
        }
    }

    LEAVE(208);
    return;
}

// input : spreaded weighted energy *werg, spreaded energy *erg
// output: Maskierungsschwelle *erg nach Anwendung des Tonlitätsoffsets
static void
ApplyTonalityOffset ( float*        erg,
                      const float*  werg )
{
    int    n;
    float  Offset;
    float  quot;

    ENTER(230);

    // Berechnung der Mithörschwelle im Partitionsbereich
    for ( n = 0; n < PART_LONG; n++ ) {
        quot = *werg++ / *erg;
        if      (quot <= const0_0574 ) Offset = O_MAX;
        else if (quot <  const0_5871 ) Offset = FAC1 * POW ( quot, FAC2 );
        else                           Offset = O_MIN;
        *erg++ *= iw[n] * minf ( MinVal [n], Offset );
    }

    LEAVE(230);
    return;
}

// input : bisherige Lautheit *loud, Leistungen *erg, Ruhehörschwelle *adapted_ltq
// output: nachgeführte Lautheit *loud, angepasste Ruhehörschwelle <Return value>
static float
AdaptLtq ( float*        loud,
           const float*  erg,
           float         varLtq )
{
    float*  weight = Loudness;
    float   sum    = 0.f;
    int     n;

    for ( n = 0; n < PART_LONG; n++ )       // Berechne Loudness
        sum += *erg++ * *weight++;

    *loud += const0_02 * ( sum - *loud );   // Anwendung der Zeitkonstanten

    // Berechne dynamischen Offset für Ruhehörschwelle, 0...+20 dB. Bei 96 dB Lautstärke wird Offset von +20 dB angenommen
    return 1.f + *loud * varLtq * 50.23772e-09f;
}

// input : simultane Maskierungssschwelle *frqthr,
//         vorherige Maskierungsschwelle *tmpthr,
//         Integrationen *a (short-time) und *b (long-time)
// output: nachgeführte Interagtionen *a und *b, Zeitkonstante *tau
static void
CalcTemporalThreshold ( float*  A,
                        float*  B,
                        float*  T,
                        float*  frqthr,
                        float*  tmpthr )
{
    int    n;
    float  tmp;

    ENTER(220);

    for ( n = 0; n < PART_LONG; n++ ) {         // nachfolgende Berechnungen relativ zur Ruhehörschwelle
        frqthr[n] *= invLtq[n];
        tmpthr[n] *= invLtq[n];

        tmp = tmpthr[n] > 1.f  ?  POW ( tmpthr[n], T[n] )  :  1.f;      // neue Nachmaskierung 'tmp' mittels Zeitkonstante tau, falls alte Nachmaskierung > Ltq (=1)

        // Berechne Zeitkonstante für Nachmaskierung im nächsten Frame, falls neue Zeitkonstante berechnet werden muß (neue tmpMask < frqMask)
        A[n] += const0_50 * ( frqthr[n] - A[n] );     // short time integrator
        B[n] += const0_15 * ( frqthr[n] - B[n] );     // long  time integrator
        if ( tmp < frqthr[n] )
            T[n] = A[n] <= B[n]  ?  0.8f  :  0.2f + 0.6f * B[n]/A[n];

        // Nutze Nachmaskierung aus (Re-Normierung)
        tmpthr [n] = partLtq [n] * maxf ( frqthr [n], tmp );
    }

    memcpy ( frqthr, tmpthr, PART_LONG * sizeof (*frqthr) );

    LEAVE(220);
    return;
}

// input : L/R-Maskierungsschwellen in Partitionen *thrL, *thrR
//         L/R-Subbandenergien *ergL, *ergR
//         M/S-Subbandenergien *ergM, *ergS
// output: M/S-Maskierungsschwellen in Partitionen *thrM, *thrS
static void
CalcMSThreshold ( const float*  const ergL,
                  const float*  const ergR,
                  const float*  const ergM,
                  const float*  const ergS,
                  float*        const thrL,
                  float*        const thrR,
                  float*        const thrM,
                  float*        const thrS )
{
    int    n;
    float  norm;
    float  tmp;

    // Alle hier hart einkodierten Zahlen sollten irgendwo rausgezogen werden:
    // Die "4.", die -2 dB, die 0.0625 und die 0.9375, sowie die Bändern, bei denen das gemacht wird.

    for ( n = 0; n < PART_LONG; n++ ) {
        // estimate M/S thresholds out of L/R thresholds and M/S and L/R energies
        thrS[n] = thrM[n] = maxf (ergM[n], ergS[n]) / maxf (ergL[n], ergR[n]) * minf (thrL[n], thrR[n]);

        switch ( MS_Channelmode ) { // preserve 'near-mid' signal components
        case 3:
            if ( n > 0 ) {
                double ratioMS = ergM[n] > ergS[n] ? ergS[n] / ergM[n]  :  ergM[n] / ergS[n];
                double ratioLR = ergL[n] > ergR[n] ? ergR[n] / ergL[n]  :  ergL[n] / ergR[n];
                if ( ratioMS < ratioLR ) {              // MS
                    if ( ergM[n] > ergS[n] )
                        thrS[n] = thrL[n] = thrR[n] = 1.e18f;
                    else
                        thrM[n] = thrL[n] = thrR[n] = 1.e18f;
                }
                else {                                  // LR
                    if ( ergL[n] > ergR[n] )
                        thrR[n] = thrM[n] = thrS[n] = 1.e18f;
                    else
                        thrL[n] = thrM[n] = thrS[n] = 1.e18f;
                }
            }
            break;
        case 4:
            if ( n > 0 ) {
                double ratioMS = ergM[n] > ergS[n] ? ergS[n] / ergM[n]  :  ergM[n] / ergS[n];
                double ratioLR = ergL[n] > ergR[n] ? ergR[n] / ergL[n]  :  ergL[n] / ergR[n];
                if ( ratioMS < ratioLR ) {              // MS
                    if ( ergM[n] > ergS[n] )
                        thrS[n] = 1.e18f;
                    else
                        thrM[n] = 1.e18f;
                }
                else {                                  // LR
                    if ( ergL[n] > ergR[n] )
                        thrR[n] = 1.e18f;
                    else
                        thrL[n] = 1.e18f;
                }
            }
            break;
        case 5:
            thrS[n] *= 2.;      // +3 dB
            break;
        case 6:
            break;
        default:
            fprintf ( stderr, "Unknown stereo mode\n");
        case 10:
            if ( 4. * ergL[n] > ergR[n]   &&  ergL[n] < 4. * ergR[n] ) {// Energy between both channels differs by less than 6 dB
                norm = 0.70794578f * iw[n];  // -1.5 dB * iwidth
                if        ( ergM[n] > ergS[n] ) {
                    tmp = ergS[n] * norm;
                    if ( thrS[n] > tmp )
                        thrS[n] = MS2SPAT1 * thrS[n] + (1.f-MS2SPAT1) * tmp;    // Hebt Maskierungsschwelle um bis zu 3 dB an
                } else if ( ergS[n] > ergM[n] ) {
                    tmp = ergM[n] * norm;
                    if ( thrM[n] > tmp )
                        thrM[n] = MS2SPAT1 * thrM[n] + (1.f-MS2SPAT1) * tmp;
                }
            }
            break;
        case 11:
            if ( 4. * ergL[n] > ergR[n]   &&  ergL[n] < 4. * ergR[n] ) {// Energy between both channels differs by less than 6 dB
                norm = 0.63095734f * iw[n];  // -2.0 dB * iwidth
                if        ( ergM[n] > ergS[n] ) {
                    tmp = ergS[n] * norm;
                    if ( thrS[n] > tmp )
                        thrS[n] = MS2SPAT2 * thrS[n] + (1.f-MS2SPAT2) * tmp;    // Hebt Maskierungsschwelle um bis zu 6 dB an
                } else if ( ergS[n] > ergM[n] ) {
                    tmp = ergM[n] * norm;
                    if ( thrM[n] > tmp )
                        thrM[n] = MS2SPAT2 * thrM[n] + (1.f-MS2SPAT2) * tmp;
                }
            }
            break;
        case 12:
            if ( 4. * ergL[n] > ergR[n]   &&  ergL[n] < 4. * ergR[n] ) {// Energy between both channels differs by less than 6 dB
                norm = 0.56234133f * iw[n];  // -2.5 dB * iwidth
                if        ( ergM[n] > ergS[n] ) {
                    tmp = ergS[n] * norm;
                    if ( thrS[n] > tmp )
                        thrS[n] = MS2SPAT3 * thrS[n] + (1.f-MS2SPAT3) * tmp;    // Hebt Maskierungsschwelle um bis zu 9 dB an
                } else if ( ergS[n] > ergM[n] ) {
                    tmp = ergM[n] * norm;
                    if ( thrM[n] > tmp )
                        thrM[n] = MS2SPAT3 * thrM[n] + (1.f-MS2SPAT3) * tmp;
                }
            }
            break;
        case 13:
            if ( 4. * ergL[n] > ergR[n]   &&  ergL[n] < 4. * ergR[n] ) {// Energy between both channels differs by less than 6 dB
                norm = 0.50118723f * iw[n];  // -3.0 dB * iwidth
                if        ( ergM[n] > ergS[n] ) {
                    tmp = ergS[n] * norm;
                    if ( thrS[n] > tmp )
                        thrS[n] = MS2SPAT4 * thrS[n] + (1.f-MS2SPAT4) * tmp;    // Hebt Maskierungsschwelle um bis zu 12 dB an
                } else if ( ergS[n] > ergM[n] ) {
                    tmp = ergM[n] * norm;
                    if ( thrM[n] > tmp )
                        thrM[n] = MS2SPAT4 * thrM[n] + (1.f-MS2SPAT4) * tmp;
                }
            }
            break;
        case 15:
            if ( 4. * ergL[n] > ergR[n]   &&  ergL[n] < 4. * ergR[n] ) {// Energy between both channels differs by less than 6 dB
                norm = 0.50118723f * iw[n];  // -3.0 dB * iwidth
                if        ( ergM[n] > ergS[n] ) {
                    tmp = ergS[n] * norm;
                    if ( thrS[n] > tmp )
                        thrS[n] = tmp;                                  // Hebt Maskierungsschwelle um bis zu +oo dB an
                } else if ( ergS[n] > ergM[n] ) {
                    tmp = ergM[n] * norm;
                    if ( thrM[n] > tmp )
                        thrM[n] = tmp;
                }
            }
            break;
        case 22:
            if ( 4. * ergL[n] > ergR[n]   &&  ergL[n] < 4. * ergR[n] ) {// Energy between both channels differs by less than 6 dB
                norm = 0.56234133f * iw[n];  // -2.5 dB * iwidth
                if        ( ergM[n] > ergS[n] ) {
                    tmp = ergS[n] * norm;
                    if ( thrS[n] > tmp )
                        thrS[n] = maxf (tmp, ergM[n]*iw[n]*0.025);              // +/- 1.414°
                } else if ( ergS[n] > ergM[n] ) {
                    tmp = ergM[n] * norm;
                    if ( thrM[n] > tmp )
                        thrM[n] = maxf (tmp, ergS[n]*iw[n]*0.025);              // +/- 1.414°
                }
            }
            break;
        }
    }

    return;
}

// input : Maskierungsschwellen in Partitionen *partThr0, *partThr1
//         Ruhehörschwelle *ltq in FFT-Auflösung
// output: Maskierungsschwellen in FFT-Auflösung *thr0, *thr1
// inline, da 4x aufgerufen
static void
ApplyLtq ( float*        thr,
           const float*  partThr,
           const float   AdaptedLTQ )
{
    int    n;
    int    k;
    float  tmp;

    for ( n = 0; n < PART_LONG; n++ ) {
        for ( k = wl[n]; k <= wh[n]; k++, thr++ ) {                 // Ruhehörschwelle (Partition)
            tmp  = sqrt (partThr [n]) + sqrt (AdaptedLTQ * fftLtq [k]);     // Applies a much more gentle ATH rolloff
            *thr = tmp * tmp;
        }
    }

    return;
}


// input : Subbandenergien *erg0, *erg1
//         Maskierungsschwellen in FFT-Auflösung *thr0, *thr1
// output: SMR je Subband *smr0, *smr1
static void
CalculateSMR ( const int     MaxBand,
               const float*  erg,
               const float*  thr,
               float*        smr )
{
    int    n;
    int    k;
    float  tmp;

    // Berechnung der Mithörschwellen in den Teilbändern
    for ( n = 0; n <= MaxBand; n++ ) {
        tmp = *thr++;
        for ( k = 1; k < 16; k++, thr++ ) {
            if (*thr < tmp)
                tmp = *thr;
        }
        *smr++ = 0.0625f * 1.44121959671885364405f * *erg++ / tmp;
    }

    return;
}

// input : Leistungsspektren erg[4][128] (4 zeitversetzte FFTs)
//         Energie des letzten short Blocks *preerg in short Partitionen
//         PreechoFac gibt erlaubten Hub des Maskierungsschwelle an
// output: Maskierungsschwelle *thr in short Partitionen
//         Energie des letzten short Blocks *preerg in short Partitionen


// Zweite Schwelle für Schortblock in Betrieb nehmen
// KBD-Fenster verwenden.
static void
CalcShortThreshold256 ( const float  erg [FOUR] [128],
                        const float  ShortThr,
                        const float  TransDetect1,
                        const float  TransDetect2,
                        float*       thr,
                        float        old_erg [2][PART_SHORT],
                        int*         transient )
{
    const int*    index_lo = wl_short; // lower FFT-index
    const int*    index_hi = wh_short; // upper FFT-index
    const float*  iwidth   = iw_short; // inverse partition-width
    int           k;
    int           n;
    int           m;
    float         new_erg;
    float         th;
    const float*  ep;

    for ( k = PART_SHORT_START; k < PART_SHORT; k++ ) {
        transient [k] = 0;
        th            = old_erg [0][k];
        //printf ("S %9.0f%9.0f", old_erg [1][k], old_erg [0][k] );
        for ( n = 0; n < FOUR; n++ ) {
            ep   = erg[n] + index_lo [k];
            m    = index_hi [k] - index_lo [k];

            new_erg = *ep++;
            while (m--)
                new_erg += *ep++;               // e = Short_Partitionsenergie in Teilstück n

            //printf ("%9.0f", new_erg );
            if ( new_erg > old_erg [0][k] ) {           // größer als alte?

                if ( new_erg > old_erg [0][k] * TransDetect1  ||
                     new_erg > old_erg [1][k] * TransDetect2 )  // is signal transient?
                    transient [k] = 1;
            }
            else {
                th = minf ( th, new_erg );          // assume short threshold = engr*PreechoFac
            }

            old_erg [1][k] = old_erg [0][k];
            old_erg [0][k] = new_erg;           // speichern der aktuellen
        }
        thr [k] = th * ShortThr * *iwidth++;  // rauszeihen und Multiplikation nur machen, wenn transient[k]=1
        //printf ("\n");
    }

    return;
}


// input : Leistungsspektren erg[4][128] (4 zeitversetzte FFTs)
//         Energie des letzten short Blocks *preerg in short Partitionen
//         PreechoFac gibt erlaubten Hub des Maskierungsschwelle an
// output: Maskierungsschwelle *thr in short Partitionen
//         Energie des letzten short Blocks *preerg in short Partitionen


// Zweite Schwelle für Schortblock in Betrieb nehmen
// KBD-Fenster verwenden.
static void
CalcShortThreshold64  ( const float  erg [EIGHT] [32],
                        const float  ShortThr,
                        const float  TransDetect3,
                        const float  TransDetect4,
                        float*       thr,
                        float        old_erg [2][PART_ULTRASHORT],
                        int*         transient )
{
    const int*    index_lo = wl_ultrashort; // lower FFT-index
    const int*    index_hi = wh_ultrashort; // upper FFT-index
    const float*  iwidth   = iw_ultrashort; // inverse partition-width
    int           k;
    int           n;
    int           m;
    float         new_erg;
    float         th;
    const float*  ep;

    for ( k = PART_ULTRASHORT_START; k < PART_ULTRASHORT; k++ ) {
        transient [k] = 0;
        th            = old_erg [0][k];
        //printf ("U %9.0f%9.0f", old_erg [1][k], old_erg [0][k] );
        for ( n = 0; n < EIGHT; n++ ) {
            ep   = erg[n] + index_lo [k];
            m    = index_hi [k] - index_lo [k];

            new_erg = *ep++;
            while (m--)
                new_erg += *ep++;               // e = Short_Partitionsenergie in Teilstück n

            //printf ("%9.0f", new_erg );
            if ( new_erg > old_erg [0][k] ) {           // größer als alte?

                if ( new_erg > old_erg [0][k] * TransDetect3  ||
                     new_erg > old_erg [1][k] * TransDetect4 )  // is signal transient?
                    transient [k] = 1;
            }
            else {
                th = minf ( th, new_erg );          // assume short threshold = engr*PreechoFac
            }

            old_erg [1][k] = old_erg [0][k];
            old_erg [0][k] = new_erg;           // speichern der aktuellen
        }
        //printf ("\n");
        thr [k] = th * ShortThr * *iwidth++;  // rausziehen und Multiplikation nur machen, wenn transient[k]=1
    }

    return;
}


// input : vorherige simultane Maskierungsschwelle *preThr,
//         aktuelle simultane Maskierungsschwelle *simThr
// output: update von *preThr für nächsten Aufruf,
//         aktuelle Maskierungsschwelle *partThr
static void
PreechoControl ( float*        partThr,
                 float*        preThr,
                 const float*  simThr )
{
    int  n;

    for ( n = 0; n < PART_LONG; n++ ) {
        *partThr++ = minf ( *simThr, *preThr * PREFAC_LONG );
        *preThr++  = *simThr++;
    }
    return;
}


void
TransientCalc ( int        Tdst [32],
                const int  T    [MAXCH] [3] [PART_SHORT],
                const int  TU   [MAXCH] [3] [PART_ULTRASHORT] )
{
    int  i;
    int  x1;
    int  x2;

    memset ( Tdst, 0, 32*sizeof(*Tdst) );

    for ( i = 0; i < PART_SHORT; i++ )
        if ( T[0][0][i] != 0  ||  T[1][0][i] != 0 ||
             T[0][1][i] != 0  ||  T[1][1][i] != 0 ||
             T[0][2][i] != 0  ||  T[1][2][i] != 0 ) {
            x1 = wl_short[i] >> 2;
            x2 = wh_short[i] >> 2;
            while ( x1 <= x2 )
                Tdst [x1++] = 1;
        }

    for ( i = 0; i < PART_ULTRASHORT; i++ )
        if ( TU[0][0][i] != 0  ||  TU[1][0][i] != 0 ||
             TU[0][1][i] != 0  ||  TU[1][1][i] != 0 ||
             TU[0][2][i] != 0  ||  TU[1][2][i] != 0 ) {
            x1 = wl_ultrashort[i] >> 4;
            x2 = wh_ultrashort[i] >> 4;
            while ( x1 <= x2 )
                Tdst [x1++] = 1;
        }
}


// input : PCM-Daten *data
// output: SMRs für die Eingangsdaten
void
Psychoakustisches_Modell ( SMR_t* const  SMR,
                           Float         ANSspec [] [2*MAXCH] [MAX_ANS_LINES],
                           const int     MaxBand,
                           const Float   data [] [ANABUFFER],
                           float         ShortThr,
                           float         UShortThr,
                           unsigned int  Max_ANS_Order,
                           int           Transient  [MAXCH] [3] [PART_SHORT],
                           int           TransientU [MAXCH] [3] [PART_ULTRASHORT] )
{
    float   Xi       [2*MAXCH] [  32];             // Schalldruck je Subband
    float   cw         [MAXCH] [ 512];             // unpredictability (nur L/R)
    float   erg        [MAXCH] [ 512];             // holds energy spectrum of long FFT
    float   phs        [MAXCH] [ 512];             // holds phase  spectrum of long FFT
    float   Thr      [2*MAXCH] [ 512];             // Maskierungsschwellen L/R/M/S
    float   F_256      [FOUR ] [ 128];             // holds energies of short FFTs (L/R only)
    float   F_64       [EIGHT] [  32];             // holds energies of short FFTs (L/R only)
    float   Xerg               [1024];             // holds energy spectrum of very long FFT
    float   Ls       [2*MAXCH] [PART_LONG];        // Schalldruck in Partition
    float   PartThr  [2*MAXCH] [PART_LONG];        // Maskierungsschwellen (Partition)
    float   sim_Mask   [MAXCH] [PART_LONG];        // simultane Maskierung (nur L/R)
    float   clow       [MAXCH] [PART_LONG];        // spreaded, weighted energy (nur L/R)
    float   cLs        [MAXCH] [PART_LONG];        // weighted partition energy (nur L/R)
    float   shortThr   [MAXCH] [PART_SHORT];       // threshold for short FFT (L/R only)
    float   shortThrU  [MAXCH] [PART_ULTRASHORT];  // threshold for short FFT (L/R only)

    int     isvoc      [MAXCH] = { 0 };
    float   factorLTQ  [MAXCH];                    // Offset nach variablem Ltq
    int     MaxLine = (MaxBand + 1) * 16;          // Setze FFT-Auflösung entsprechend MaxBand
    float   tmp;
    int     n;
    int     k;

    ENTER(50);

    if ( CVD_used ) {                              // 'ClearVocalDetection'-Prozeß
        PowSpec2048 ( data[0], Xerg );
        isvoc[0] = CVD2048 ( Xerg, Vocal[0] );
        PowSpec2048 ( data[1], Xerg );
        isvoc[1] = CVD2048 ( Xerg, Vocal[1] );
    }

    for ( k = 0; k < 3; k++ ) {                    // für jeden Subframe

        // Berechnung der spektralen Leistung mittels FFT
        PolarSpec1024   ( data[0] + k * (LONG_ADVANCE), erg[0], phs[0] );
        PolarSpec1024   ( data[1] + k * (LONG_ADVANCE), erg[1], phs[1] );

        // Berechnung des Schalldrucks je Teilband für L/R-Signale
        SubbandEnergy   ( MaxBand, Xi[0], erg[0] );
        SubbandEnergy   ( MaxBand, Xi[1], erg[1] );

        // Berechnung des Schalldrucks je Partition
        PartitionEnergy ( Ls[0], erg[0] );
        PartitionEnergy ( Ls[1], erg[1] );

        // Berechnung der Vorhersehbarkeit des Signals
        CalcUnpred ( MaxLine, erg[0], phs[0], isvoc[0]  ?  Vocal[0]  :  NULL, FFT_Ampl [0], FFT_Phas [0], cw[0] );
        CalcUnpred ( MaxLine, erg[1], phs[1], isvoc[1]  ?  Vocal[1]  :  NULL, FFT_Ampl [1], FFT_Phas [1], cw[1] );

        // Berechnung des gewichteten Schalldrucks je Partition
        WeightedPartitionEnergy ( cLs[0], erg[0], cw[0] );
        WeightedPartitionEnergy ( cLs[1], erg[1], cw[1] );

        // Spreading Signal & weighted unpredictability-signal
        SpreadingSignal ( Ls[0], cLs[0], sim_Mask[0], clow[0] );
        SpreadingSignal ( Ls[1], cLs[1], sim_Mask[1], clow[1] );

        // Offset depending on tonality
        ApplyTonalityOffset ( sim_Mask[0], clow[0] );
        ApplyTonalityOffset ( sim_Mask[1], clow[1] );

        // Behandlung transienter Signale
        for ( n = 0; n < FOUR; n++ )
            PowSpec256 ( data[0] + k*(LONG_ADVANCE) + SHORT_ADVANCE*n + SHORTFFT_OFFSET + AuxParam[0], F_256[n] );
        CalcShortThreshold256 ( F_256, ShortThr, TransD1, TransD2, shortThr[0], pre_erg[0], Transient[0][k] );

        for ( n = 0; n < FOUR; n++ )
            PowSpec256 ( data[1] + k*(LONG_ADVANCE) + SHORT_ADVANCE*n + SHORTFFT_OFFSET + AuxParam[0], F_256[n] );
        CalcShortThreshold256 ( F_256, ShortThr, TransD1, TransD2, shortThr[1], pre_erg[1], Transient[1][k] );

        // Behandlung transienter Signale
        for ( n = 0; n < EIGHT; n++ )
            PowSpec64 ( data[0] + k*(LONG_ADVANCE) + ULTRASHORT_ADVANCE*n + ULTRASHORTFFT_OFFSET + AuxParam[1], F_64[n] );
        CalcShortThreshold64 ( F_64, UShortThr, TransD3, TransD4, shortThrU[0], pre_ergU[0], TransientU[0][k] );

        for ( n = 0; n < EIGHT; n++ )
            PowSpec64 ( data[1] + k*(LONG_ADVANCE) + ULTRASHORT_ADVANCE*n + ULTRASHORTFFT_OFFSET + AuxParam[1], F_64[n] );
        CalcShortThreshold64 ( F_64, UShortThr, TransD3, TransD4, shortThrU[1], pre_ergU[1], TransientU[1][k] );

        // dynamische Anpassung der Ruhehörschwelle an Lautstärke der aktuellen Sequenz
        if ( varLtq > 0. ) {
            factorLTQ [0] = AdaptLtq ( loud + 0, Ls [0], varLtq );
            factorLTQ [1] = AdaptLtq ( loud + 1, Ls [1], varLtq );
        }
        else {
            factorLTQ [0] = \
            factorLTQ [1] = 1.f;
        }

        // Anwendung der temporalen Nachmaskierung
        if ( tmpMask_used ) {
            CalcTemporalThreshold ( integra[0][0], integra[0][1], tau[0], sim_Mask[0], tmp_Mask[0] );
            CalcTemporalThreshold ( integra[1][0], integra[1][1], tau[1], sim_Mask[1], tmp_Mask[1] );
        }

        // transientes Signal?
        for ( n = PART_SHORT_START; n < PART_SHORT; n++ ) {
            if ( Transient [0] [k] [n] ) {
                tmp = shortThr [0] [n];
                sim_Mask[0] [3*n+0] = minf ( sim_Mask[0] [3*n+0], tmp );
                sim_Mask[0] [3*n+1] = minf ( sim_Mask[0] [3*n+1], tmp );
                sim_Mask[0] [3*n+2] = minf ( sim_Mask[0] [3*n+2], tmp );
            }
            if ( Transient [1] [k] [n] ) {
                tmp = shortThr [1] [n];
                sim_Mask[1] [3*n+0] = minf ( sim_Mask[1] [3*n+0], tmp );
                sim_Mask[1] [3*n+1] = minf ( sim_Mask[1] [3*n+1], tmp );
                sim_Mask[1] [3*n+2] = minf ( sim_Mask[1] [3*n+2], tmp );
            }
        }

        // ultra-transientes Signal?
        for ( n = PART_ULTRASHORT_START; n < PART_ULTRASHORT; n++ ) {
            if ( TransientU [0] [k] [n] ) {
                tmp = shortThrU [0] [n];
                sim_Mask[0] [6*n+0] = minf ( sim_Mask[0] [6*n+0], tmp );
                sim_Mask[0] [6*n+1] = minf ( sim_Mask[0] [6*n+1], tmp );
                sim_Mask[0] [6*n+2] = minf ( sim_Mask[0] [6*n+2], tmp );
                sim_Mask[0] [6*n+3] = minf ( sim_Mask[0] [6*n+3], tmp );
                sim_Mask[0] [6*n+4] = minf ( sim_Mask[0] [6*n+4], tmp );
                sim_Mask[0] [6*n+5] = minf ( sim_Mask[0] [6*n+5], tmp );
            }
            if ( TransientU [1] [k] [n] ) {
                tmp = shortThrU [1] [n];
                sim_Mask[1] [6*n+0] = minf ( sim_Mask[1] [6*n+0], tmp );
                sim_Mask[1] [6*n+1] = minf ( sim_Mask[1] [6*n+1], tmp );
                sim_Mask[1] [6*n+2] = minf ( sim_Mask[1] [6*n+2], tmp );
                sim_Mask[1] [6*n+3] = minf ( sim_Mask[1] [6*n+3], tmp );
                sim_Mask[1] [6*n+4] = minf ( sim_Mask[1] [6*n+4], tmp );
                sim_Mask[1] [6*n+5] = minf ( sim_Mask[1] [6*n+5], tmp );
            }
        }

        // Pre-Echo Kontrolle
        PreechoControl ( PartThr[0], PreThr[0], sim_Mask[0] );
        PreechoControl ( PartThr[1], PreThr[1], sim_Mask[1] );

        // Anwendung der Ruhehörschwelle
        ApplyLtq ( Thr[0], PartThr[0], factorLTQ[0] );
        ApplyLtq ( Thr[1], PartThr[1], factorLTQ[1] );

        // Berücksichtigung von Aliasing zwischen den Teilbändern (Rauschen verschmiert)
        AdaptThresholds ( MaxLine, Thr[0] );
        AdaptThresholds ( MaxLine, Thr[1] );

        // Berechnung der Signal-to-Mask-Ratio
        CalculateSMR ( MaxBand, Xi[0], Thr[0], SMR [k].Ch0 );
        CalculateSMR ( MaxBand, Xi[1], Thr[1], SMR [k].Ch1 );

        if ( MS_Channelmode > 0 ) {
            // Berechnung der spektralen Leistung mittels FFT
            PowSpec1024_Mix ( data[0] + k*(LONG_ADVANCE), data[1] + k*(LONG_ADVANCE), erg[0], erg[1] );          // mid+side

            // Berechnung des Schalldrucks je Teilband für M/S-Signale
            SubbandEnergy ( MaxBand, Xi[2], erg[0] );
            SubbandEnergy ( MaxBand, Xi[3], erg[1] );

            // Berechnung des Schalldrucks je Partition
            PartitionEnergy ( Ls[2], erg[0] );
            PartitionEnergy ( Ls[3], erg[1] );

            // Berechne Maskierungsschwellen für M/S
            CalcMSThreshold ( Ls[0], Ls[1], Ls[2], Ls[3], PartThr[0], PartThr[1], PartThr[2], PartThr[3] );
            ApplyLtq ( Thr[2], PartThr[2], 0.5 * (factorLTQ[0] + factorLTQ[1]) );
            ApplyLtq ( Thr[3], PartThr[3], 0.5 * (factorLTQ[0] + factorLTQ[1]) );

            // Berücksichtigung von Aliasing zwischen den Teilbändern (Rauschen verschmiert)
            AdaptThresholds ( MaxLine, Thr[2] );
            AdaptThresholds ( MaxLine, Thr[3] );

            // Berechnung der Signal-to-Mask-Ratio
            CalculateSMR ( MaxBand, Xi[2], Thr[2], SMR [k].Ch2 );
            CalculateSMR ( MaxBand, Xi[3], Thr[3], SMR [k].Ch3 );
        }

        if ( Max_ANS_Order > 0 ) {                                   // Bereitstellung der Noise Shaping Schwellen
            memcpy ( ANSspec[k][0], Thr[0], sizeof ANSspec[0][0] );
            memcpy ( ANSspec[k][1], Thr[1], sizeof ANSspec[0][1] );
            memcpy ( ANSspec[k][2], Thr[2], sizeof ANSspec[0][2] );
            memcpy ( ANSspec[k][3], Thr[3], sizeof ANSspec[0][3] );
        }
    }

    LEAVE(50);
}

/* end of psy.c */
