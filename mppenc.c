/*
 *  Main encoder module.
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

// Überdeckung der Quellen von enc + dec überprüfen, ggf. Make beschleunigen

// Perfect Gapless encoding
// ATH: 6 dB mehr in den Tiefen, 9 dB mehr in den Höhen, 3 dB mehr reduzierbar durch AdaptLTQ, --ltq_gain + --ltq_gain20
// bewirkt AdaptThresholds() überhaupt was ???
// CVD Spektrallinienbereich der Abtastfrequenz anpassen
// Attention: Can be NANs !!!!
// Dekodierte Dateilänge kontrollieren
// Short-Block-Akustik ???
// Wild card support
// Prediction ohne atan, sqrt, cos et all.

#include <memory.h>
#include <time.h>
#include <errno.h>
#include "mppenc.h"


#define MODE_OVERWRITE          0
#define MODE_NEVER_OVERWRITE    1
#define MODE_ASK_FOR_OVERWRITE  2


// Pillory for global variables
unsigned int    MS_Channelmode;
unsigned int    CVD_used;                               // globaler Flag für ClearVoiceDetection
float           varLtq;                                 // variable Ruhhehörschwelle
unsigned int    tmpMask_used;                           // globaler Flag für temporale Maskierung
float           SPRD     [PART_LONG] [PART_LONG];       // tabellierte Spreadingfunktion
float           MinVal   [PART_LONG];                   // enthält minimale Tonalitätsoffsets
float           Loudness [PART_LONG];                   // Gewichtungsfaktoren für Berechnung der Lautstärke
float           partLtq  [PART_LONG];                   // Ruhehörschwelle (Partitionen)
float           invLtq   [PART_LONG];                   // inverse Ruhehörschwelle (Partitionen, long)
float           fftLtq   [512];                         // Ruhehörschwelle (FFT)
float           SNR_comp  [MAXCH] [32];                 // SNR-Kompensation nach SCF-Zusammenfassung und ANS-Gewinn
unsigned char   ANS_Order [MAXCH] [32];                 // frameweise Ordnung des Noiseshapings (0: off, 1...6: on)
float           FIR       [MAXCH] [32] [MAX_ANS_ORDER]; // enthält FIR-Filter für NoiseShaping
float           SCF      [128];                         // tabellierte Skalenfaktoren
float           invSCF   [128];                         // invertierte Skalenfaktoren
int             AuxParam [ 10];


// weitere allgemeine globale Variablen
static int           DisplayUpdateTime = 1;
static int           APE_Version       = 2000;
static float         Bandwidth;
static int           CombPenalities    = -1;
static float         PNS;
static float         TMN;
static float         NMT;
static int           MaxSubBand;                        //
static float         Ltq_offset;                        // Offset für Ruhehörschwelle
static float         Ltq_max;                           // maximaler Pegel für Ruhehörschwelle
static unsigned int  Ltq_model;
static float         Ltq_20kHz;
static int           MinVal_model;
static float         SampleFreq;
static int           MainQual;                          // Profile
static unsigned int  DelInput        = 0;               // Löschen des Inputfiles nach Kodierung
static unsigned int  WriteMode       = MODE_ASK_FOR_OVERWRITE;      // Überschreiben einer möglichweise existierenden MPC Datei
static unsigned int  verbose         = 0;               // mehr Infos bei Ausgabe
static float         ScalingFactor [MAXCH]; // Skalierung des Eingangssignals
static float         FadeShape       = 1.f;             // Form des Fadens
static float         FadeInTime      = 0.f;             // Dauer des Fadein in sec
static float         FadeOutTime     = 0.f;             // Dauer des Fadeout in sec
static float         SkipTime        = 0.f;             // Skip the beginning of the file (sec)
static double        Duration        = 1.e+99;          // Maximum encoded audio length
static Bool_t        FrontendPresent = 0;               // Flag für Frontend-Detektion
static int           PredictionBands = 0;
static float         TransDetect1;
static float         TransDetect2;
static float         TransDetect3;
static float         TransDetect4;
static float         alpha0          =  4.;
static float         alpha1          =  2.;
static float         alpha2          = -1.;
static float         minSMR;                            // minimaler SMR für alle Subbänder
static float         ShortThr;                          // Faktor zur Berechnung der Maskierungschwelle bei Transienten
static float         UShortThr;                         // Faktor zur Berechnung der Maskierungschwelle bei Transienten
static unsigned int  Max_ANS_Order;                     // Maximale Ordnung für ANS
static const char    About []        = "MPC Encoder  " MPPENC_VERSION "  " MPPENC_BUILD "   (C) 1999-2002 Buschmann/Case/Klemm/Piecha";


#include "mppenc_help.h"
#include "mppenc_fade.h"
#include "mppenc_qual.h"
#include "mppenc_show.h"


static int
YesNo ( void )
{
    fflush (stdout);

    while (1)
        switch ( WaitKey () ) {
        case 'Y': case 'y':
            return 1;
        case 'N': case 'n':
            return 0;
        }
}


typedef struct {
    unsigned long  n;
    double         x;
    double         x2;
    double         y;
    double         y2;
    double         xy;
} korr_t;


static void
analyze_stereo ( const float*  p1,
                 const float*  p2,
                 size_t        len,
                 korr_t*       k )
{
    double  _x = 0, _x2 = 0, _y = 0, _y2 = 0, _xy = 0;
    double  t1;
    double  t2;

    k -> n  += len;

    for ( ; len--; ) {
        _x  += (t1 = *p1++); _x2 += t1 * t1;
        _y  += (t2 = *p2++); _y2 += t2 * t2;
                             _xy += t1 * t2;
    }

    k -> x  += _x ;
    k -> x2 += _x2;
    k -> y  += _y ;
    k -> y2 += _y2;
    k -> xy += _xy;
}


static void
report ( korr_t* k )
{
    double  r;
    double  sx;
    double  sy;
    double  x;
    double  y;
    double  b;

    r  = (k->x2*k->n - k->x*k->x) * (k->y2*k->n - k->y*k->y);
    r  = r  > 0.   ?  (k->xy*k->n - k->x*k->y) / sqrt (r)  :  1.;
    sx = k->n > 1  ?  sqrt ( (k->x2 - k->x*k->x/k->n) / (k->n - 1) )  :  0.;
    sy = k->n > 1  ?  sqrt ( (k->y2 - k->y*k->y/k->n) / (k->n - 1) )  :  0.;
    x  = k->n > 0  ?  k->x/k->n  :  0.;
    y  = k->n > 0  ?  k->y/k->n  :  0.;

    b  = sx != 0   ?  r >= 0 ? +sy/sx : -sy/sx  :  0.;
}


static unsigned int
maxia ( const int* p, size_t len )
{
    unsigned int ret = abs (*p);
    unsigned int tmp;

    while ( --len ) {
        p++;
        tmp = abs(*p);
        if (tmp > ret ) ret = tmp;
    }

    return ret;
}


static const unsigned char  Penalty [256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
      0,  2,  5,  9, 15, 23, 36, 54, 79,116,169,246,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

#define P(new,old)  Penalty [128 - (old) + (new)]


static unsigned char
extractSCF ( float value )
{
    value *= value;
    value *= value;
    value *= value;
    value *= (1./32);

#if ENDIAN == HAVE_BIG_ENDIAN
    return ((unsigned char*)&value)[0];
#else
    return ((unsigned char*)&value)[3];
#endif
}


static unsigned char
extractSCF_pow ( float value )
{
    value *= value;
    value *= value;
    value *= (1./32);

#if ENDIAN == HAVE_BIG_ENDIAN
    return ((unsigned char*)&value)[0];
#else
    return ((unsigned char*)&value)[3];
#endif
}


static void
SCF_Extraction ( const int  MaxBand,
                 scf_t      scf      [32] [ 3],
                 Float      S        [32] [36],
                 float      SNR_comp [32],
                 Float      Power    [32] [ 3] )
{
    static float   inv [] = {
        0.3333333333333333f, 0.4804065322396179f, 0.6923713086554851f, 0.9978590982401633f, 1.4381340871479272f,
        2.0726670291069724f, 2.9871683398220155f, 4.3051655500496129f, 6.2046889578504052f, 8.9423193175990858f,
    };
    int      Band;
    int      n;
    scf_t    comp [3];
    float    fac;
    float    max;
    float    mx2;

    ENTER(4);

    for ( Band = 0; Band <= MaxBand; Band++ ) {       // Suche nach Maxima

        max = FABS (S[Band][ 0]);
        mx2 = S[Band][ 0] * S[Band][ 0];
        for ( n = 1; n < 12; n++ ) {
            if (max < FABS (S[Band][n])) max = FABS (S[Band][n]);
            mx2 += S[Band][n] * S[Band][n];
        }
        Power [Band][0] = mx2;
        comp[0] = scf [Band][0] = extractSCF (max);
        //if ( comp[0] > 123 )
        //    printf ( "%2u: %9.2f %3u\n", Band, comp[0], max );

        max = FABS (S[Band][12]);
        mx2 = S[Band][12] * S[Band][12];
        for ( n = 13; n < 24; n++ ) {
            if (max < FABS (S[Band][n])) max = FABS (S[Band][n]);
            mx2 += S[Band][n] * S[Band][n];
        }
        Power [Band][1] = mx2;
        comp[1] = scf [Band][1] = extractSCF (max);
        //if ( comp[1] > 123 )
        //    printf ( "%2u: %9.2f %3u\n", Band, comp[1], max );

        max = FABS (S[Band][24]);
        mx2 = S[Band][24] * S[Band][24];
        for ( n = 25; n < 36; n++ ) {
            if (max < FABS (S[Band][n])) max = FABS (S[Band][n]);
            mx2 += S[Band][n] * S[Band][n];
        }
        Power [Band][2] = mx2;
        comp[2] = scf [Band][2] = extractSCF (max);
        //if ( comp[2] > 123 )
        //    printf ( "%2u: %9.2f %3u\n", Band, comp[2], max );

        // Sicherung alter Werte zur Berechnung der Kompensationsberechnung
        if ( CombPenalities > 0 ) {
            if      ( P(scf[Band][0],scf[Band][1]) + P(scf[Band][0],scf[Band][2]) <= CombPenalities ) scf[Band][2] = scf[Band][1] = scf[Band][0];
            else if ( P(scf[Band][1],scf[Band][0]) + P(scf[Band][1],scf[Band][2]) <= CombPenalities ) scf[Band][0] = scf[Band][2] = scf[Band][1];
            else if ( P(scf[Band][2],scf[Band][0]) + P(scf[Band][2],scf[Band][1]) <= CombPenalities ) scf[Band][0] = scf[Band][1] = scf[Band][2];
            else if ( P(scf[Band][0],scf[Band][1])                                <= CombPenalities ) scf[Band][1] = scf[Band][0];
            else if ( P(scf[Band][1],scf[Band][0])                                <= CombPenalities ) scf[Band][0] = scf[Band][1];
            else if ( P(scf[Band][1],scf[Band][2])                                <= CombPenalities ) scf[Band][2] = scf[Band][1];
            else if ( P(scf[Band][2],scf[Band][1])                                <= CombPenalities ) scf[Band][1] = scf[Band][2];
        }

        // SNR-Kompensationsberechnung
        // Kompensationsrechnung sollte an Hand der wirklichen Maxima erfolgen, nicht auf Grundlage der
        // quantisierten SCF !!!
        SNR_comp [Band] = inv [scf[Band][0] - comp[0]] + inv [scf[Band][1] - comp[1]] + inv [scf[Band][2] - comp[2]];

        // Normierung der Subbandsamples
        fac = invSCF [scf[Band][0]]; for ( n = 0; n < 12; n++ ) S[Band][n] *= fac;
        fac = invSCF [scf[Band][1]]; for (      ; n < 24; n++ ) S[Band][n] *= fac;
        fac = invSCF [scf[Band][2]]; for (      ; n < 36; n++ ) S[Band][n] *= fac;

        //printf ("%2u| %3u %3u %3u\n", Band, scf[Band][0], scf[Band][1], scf[Band][2] );
    }

    LEAVE(4);
    return;
}


typedef float  FIR_t [MAX_ANS_ORDER];


static inline
Is ( const Uint* S, const Uint val )
{
    int  i;
    for ( i = 0; i < 12; i++ )
        if ( S[i] != val )
            return 0;
    return 1;
}


static const int Dminus [22] = {
    0, 0, 0,-1,-2,-3,-4,-5,-7,-10,-15,-31,-64,-128,-256,-512,-1024,-2048,-4096,-8192,-16384,-32768
};

static void
Quantisierung ( const int               MaxBand,
                res_t                   Res       [32],
                const Float             Sf        [32] [36],
                Uint                    S         [32] [36],
                pack_t                  ANS_Order [32],
                FIR_t                   FIR       [32],
                scf_t                   scf       [32] [ 3] )
{
    static Float  error /* CH */ [32] [36 + MAX_ANS_ORDER];
    int           Band;

    ENTER(5);

    // Quantisierung Subband- und Subframesamples
    for ( Band = 0; Band <= MaxBand; Band++ ) {
        if ( Res[Band] > 1 ) {
            if ( ANS_Order[Band] > 0 ) {
                QuantizeSubband_ANS ( S[Band], Sf[Band], Res[Band], error [Band], FIR [Band] );
                //memcpy ( error [Band], error[Band] + 36, MAX_NS_ORDER * sizeof (**error) );
            }
            else {
                QuantizeSubband     ( S[Band], Sf[Band], Res[Band], error [Band] );
                //memcpy ( error [Band], error[Band] + 36, MAX_NS_ORDER * sizeof (**error) );
            }
            if ( Is ( S[Band] +  0, -Dminus [Res[Band]] ) ) scf [Band][0] = SCF_RES;
            if ( Is ( S[Band] + 12, -Dminus [Res[Band]] ) ) scf [Band][1] = SCF_RES;
            if ( Is ( S[Band] + 24, -Dminus [Res[Band]] ) ) scf [Band][2] = SCF_RES;
            if ( scf [Band][0] == SCF_RES  &&  scf [Band][1] == SCF_RES  &&  scf [Band][2] == SCF_RES )  Res[Band] = 0;
        }
    }

    LEAVE(5);
    return;
}


static int
PNS_SCF ( scf_t  scf [3],
          float  S0,
          float  S1,
          float  S2 )
{

//    printf ("%7.1f %7.1f %7.1f  ", sqrt(S0/12), sqrt(S1/12), sqrt(S2/12) );

#if 1
    if ( S0 < 0.5 * S1  ||  S1 < 0.5 * S2  ||  S0 < 0.5 * S2 )
        return 0;

    if ( S1 < 0.25 * S0  ||  S2 < 0.25 * S1  ||  S2 < 0.25 * S0 )
        return 0;
#endif

    if ( S0 >= 0.8 * S1 ) {
        if ( S0 >= 0.8 * S2  &&  S1 > 0.8 * S2 )
            S0 = S1 = S2 = 0.33333333333f * (S0 + S1 + S2);
        else
            S0 = S1 = 0.5f * (S0 + S1);
    }
    else {
        if ( S1 >= 0.8 * S2 )
            S1 = S2 = 0.5f * (S1 + S2);
    }

    scf [0] = extractSCF_pow ( S0 * (4./12/1.1892071150027210667174999705605) );
    scf [1] = extractSCF_pow ( S1 * (4./12/1.1892071150027210667174999705605) );
    scf [2] = extractSCF_pow ( S2 * (4./12/1.1892071150027210667174999705605) );
    return 1;
}


static void
PNS_SCF_force ( scf_t  scf [3],
                float  S0,
                float  S1,
                float  S2 )
{
    if ( S0 >= 0.8 * S1 ) {
        if ( S0 >= 0.8 * S2  &&  S1 > 0.8 * S2 )
            S0 = S1 = S2 = 0.33333333333f * (S0 + S1 + S2);
        else
            S0 = S1 = 0.5f * (S0 + S1);
    }
    else {
        if ( S1 >= 0.8 * S2 )
            S1 = S2 = 0.5f * (S1 + S2);
    }

    scf [0] = extractSCF_pow ( S0 * (4./12/1.1892071150027210667174999705605) );
    scf [1] = extractSCF_pow ( S1 * (4./12/1.1892071150027210667174999705605) );
    scf [2] = extractSCF_pow ( S2 * (4./12/1.1892071150027210667174999705605) );
}


static void
Allocate ( const int      MaxBand,
           res_t          res [32],
           Float          S   [32] [36],
           scf_t          scf [32] [ 3],
           const float    comp[32],
           const float    smr [32],
           const float    Pow [32] [ 3],
           const int      Transient [32] )
{
    float  save [36];   // für Anpassung der Skalenfaktoren
    float  MNR;         // Mask-to-Noise ratio
    float  tmpMNR;      // für Anpassung der Skalenfaktoren
    float  SMR;
    int    Band;
    int    k;
    int    r;

    ENTER(6);

    for ( Band = 0; Band <= MaxBand; Band++ ) {
        SMR = smr [Band] / comp [Band];

        if ( SMR <= 1. ) {
            r = 0;
        }
        else if ( Band > 0  &&  res [Band-1] < 5  &&  SMR < Band * PNS  &&
            PNS_SCF ( scf [Band], Pow [Band][0], Pow [Band][1], Pow [Band][2] ) ) {
            r = 1;
        }
        else {
#if 0
            r = MinRes_Estimator (SMR) - 1;
#else
            r = 2;
#endif
            for ( MNR = SMR; MNR > 1.  &&  r != 21; ) {
                MNR = SMR * (Transient[Band]  ?  SNR_Estimator_Trans  :  SNR_Estimator) ( S [Band], ++r );
            }
        }
        res [Band] = r;
        // printf ("%2u %12.3f\n", r, SMR );

        // Fine adapt SCF's (MNR > 0 prevents adaption of zero samples, which is nonsense) only apply to Huffman-coded samples (otherwise no savings in bitrate)
        if ( r > 2  &&  MNR < 1.  &&  MNR > 0. ) {
            while ( scf [Band][0] < 127  &&  scf [Band][1] < 127  &&  scf [Band][2] < 127  &&  !Transient[Band] ) {

                ++scf [Band][2]; ++scf [Band][1]; ++scf [Band][0];
                memcpy ( save, S [Band], sizeof save );
                for ( k = 0; k < 36; k++ )
                    S [Band][k] *= SCFfac;

                tmpMNR = SMR * (Transient[Band]  ?  SNR_Estimator_Trans  :  SNR_Estimator) ( S [Band], r ); // recalculate MNR

                if ( tmpMNR <= 1. ) {
                    MNR = tmpMNR;
                }
                else {
                    --scf [Band][0]; --scf [Band][1]; --scf [Band][2];
                    memcpy ( S [Band], save, sizeof save );
                    break;
                }
            }
        }

    }

    LEAVE(6);
    return;
}


#define P1      (*(float*)(p1  + i))
#define P2      (*(float*)(p2  + i))
#define P3      (*(float*)(p3  + i))
#define DST     (*(float*)(dst + i))

static void
minf3a ( float* dst, const float* p1, const float* p2, const float* p3, size_t len )
{
    size_t  i;

    for ( i = 0; i < len; i++ ) {
        DST = P1 <= P2  ?  (P1 <= P3  ?  P1  :  P3)  :  (P2 <= P3  ?  P2  :  P3);
    }
}

static void
maxf3a ( float* dst, const float* p1, const float* p2, const float* p3, size_t len )
{
    size_t  i;

    for ( i = 0; i < len; i++ ) {
        DST = P1 >= P2  ?  (P1 >= P3  ?  P1  :  P3)  :  (P2 >= P3  ?  P2  :  P3);
    }
}

#undef P1
#undef P2
#undef P3
#undef DST


static int
PossibleOutputFile ( const char *s )
{
    size_t  len;

    if ( s == NULL )                        return 0;
    if ( strcmp ( s, "/dev/null"   ) == 0 ) return 1;
    if ( strcmp ( s, "/dev/stdout" ) == 0 ) return 1;
    if ( strcmp ( s, "-"           ) == 0 ) return 1;

    len = strlen (s);
    if ( len < 4 )                          return 0;

    s  += len - 4;
    if ( strcasecmp (s, ".MPC") == 0 )      return 1;
    if ( strcasecmp (s, ".MPP") == 0 )      return 1;
    if ( strcasecmp (s, ".MP+") == 0 )      return 1;

    return 0;
}


/*
 *  As input you got a pointer to the remaining command lines (and you have to return YOUR
 *  remaining command line) and a destination proposal (NULL, directory name, or /dev/null).
 *  You have to return useful input and output file names.
 */

static char**
EvalNames ( char**       argv,                  // remaining command line
            const char*  OutputDir,             // output directory proposal
            char** const InputFile,             // input file name
            char** const OutputFile )           // output file name
{
    static char  output [2048];
    char*        p = output;

    if ( PossibleOutputFile (argv [1]) ) {
        *InputFile  = *argv++;
        *OutputFile = *argv++;
    }
    else if ( PossibleOutputFile (OutputDir) ) {
        *InputFile  = *argv++;
        *OutputFile = OutputDir;
    }
    else {
        *InputFile  = *argv++;
        *OutputFile = p;
        if ( OutputDir != NULL ) {
            strcpy ( p, OutputDir  );
            p += strlen (p);
            if ( p[-1] != PATH_SEP )
                *p++ = PATH_SEP;
        }
        strcpy ( p, *InputFile );
        p += strlen (p);
        if ( p > output+4  &&  p [-4] == '.' )
            p -= 4;
        strcpy ( p, ".mpc" );
    }

    return argv;
}

/*
 *  EvalParameters ()
 *
 *
 *
 */

static int
ReadScalingFactor ( Float* ScalingFactor, const char* arg )
{
    int    i;
    Float  f = 1.f;

    for ( i = 0; i < MAXCH; i++ ) {
        if ( arg != NULL )
            f = (float) atof ( arg );
        *ScalingFactor++ = f;
        if ( arg != NULL )
            if ( (arg = strchr ( arg, ',' )) != NULL )
                arg++;
    }

    return 0;
}


static char**
EvalParameters ( char** argv )
{
    static char  errmsg [] = "\nERROR: Missing argument for option '--%s'\n\n";
    const char*  arg;

#define EQUAL(string)   ( 0 == strcmp ( arg, string ) )
#define NEXTARG         if ( *argv == NULL ) { stderr_printf ( errmsg, arg ); return NULL; } arg = *argv++;

    // search for options
    while ( (arg = *argv++) != NULL ) {

        if ( arg[0] != '-'  ||  arg[1] != '-' )
            return --argv;

        arg += 2;

        if      ( EQUAL ("verbose") ) {
            verbose++;
        }
        else if ( EQUAL ("telephone") ) {
            SetQualityParams ( 2.0 );
        }
        else if ( EQUAL ("thumb") ) {
            SetQualityParams ( 3.0 );
        }
        else if ( EQUAL ("radio"   ) ) {
            SetQualityParams ( 4.0 );
        }
        else if ( EQUAL ("standard")  ||  EQUAL ("normal") ) {
            SetQualityParams ( 5.0 );
        }
        else if ( EQUAL ("xtreme")    ||  EQUAL ("extreme") ) {
            SetQualityParams ( 6.0 );
        }
        else if ( EQUAL ("insane") ) {
            SetQualityParams ( 7.0 );
        }
        else if ( EQUAL ("braindead") ) {
            SetQualityParams ( 8.0 );
        }
        else if ( EQUAL ("quality") ) {
            NEXTARG;
            SetQualityParams ( atof (arg) );
        }
        else if ( EQUAL ("neveroverwrite") ) {
            WriteMode = MODE_NEVER_OVERWRITE;
        }
        else if ( EQUAL ("forcewrite")  ||  EQUAL ("overwrite") ) {
            WriteMode = MODE_OVERWRITE;
        }
        else if ( EQUAL ("interactive")  ) {
            WriteMode = MODE_ASK_FOR_OVERWRITE;
        }
        else if ( EQUAL ("delinput")  ||  EQUAL ("delete")  ||  EQUAL ("deleteinput") ) {
            DelInput = 0xAFFEDEAD;
        }
        else if ( EQUAL ("scale") ) {
            NEXTARG;
            if ( ReadScalingFactor ( ScalingFactor, arg ) < 0 )
                { stderr_printf ("--scale: missing argument(s)" ); return NULL; }
        }
        else if ( EQUAL ("kbd") ) {
            NEXTARG;
            if ( 3 != sscanf ( arg, "%f,%f,%f", &alpha0, &alpha1, &alpha2 ))
                { stderr_printf ("--kbd: missing 3 arguments" ); return NULL; }
            Init_FFT ( alpha0, alpha1, alpha2, -1 );
        }
        else if ( EQUAL ("fadein") ) {
            NEXTARG;
            FadeInTime = (float) atof (arg);
            if ( FadeInTime  < 0.f ) FadeInTime  = 0.f;
        }
        else if ( EQUAL ("fadeout") ) {
            NEXTARG;
            FadeOutTime = (float) atof (arg);
            if ( FadeOutTime < 0.f ) FadeOutTime = 0.f;
        }
        else if ( EQUAL ("fade") ) {
            NEXTARG;
            FadeOutTime = (float) atof (arg);
            if ( FadeOutTime < 0.f ) FadeOutTime = 0.f;
            FadeInTime = FadeOutTime;
        }
        else if ( EQUAL ("fadeshape") ) {
            NEXTARG;
            FadeShape = (float) atof (arg);
            if ( FadeShape < 0.001f  ||  FadeShape > 1000.f ) FadeShape = 1.f;
            setbump ( FadeShape );
        }
        else if ( EQUAL ("skip")  ||  EQUAL ("start") ) {
            NEXTARG;
            SkipTime = (float) atof (arg);
        }
        else if ( EQUAL ("dur")  ||  EQUAL ("duration") ) {
            NEXTARG;
            Duration = atof (arg);
        }
        else if ( EQUAL ("ans") ) {
            NEXTARG;
            Max_ANS_Order = atoi (arg);
            Max_ANS_Order = mini ( Max_ANS_Order, MAX_ANS_ORDER );
        }
        else if ( EQUAL ("predict") ) {
            NEXTARG;
            PredictionBands = atoi (arg);
            PredictionBands = mini ( PredictionBands, 32 );
        }
        else if ( EQUAL ("ltq_var")  ||  EQUAL ("ath_var") ) {
            NEXTARG;
            varLtq = atof (arg);
        }
        else if ( EQUAL ("pns") ) {
            NEXTARG;
            PNS = atof (arg);
        }
        else if ( EQUAL ("minval") ) {
            NEXTARG;
            MinVal_model = atoi (arg);
        }
        else if ( EQUAL ("transdetect") ) {
            NEXTARG;
            if ( 4 != sscanf ( arg, "%f:%f,%f:%f", &TransDetect1, &TransDetect2, &TransDetect3, &TransDetect4 ))
                { stderr_printf ("--transdetect: missing 4 arguments" ); return NULL; }
        }
        else if ( EQUAL ("shortthr") ) {
            NEXTARG;
            if ( 2 != sscanf ( arg, "%f,%f", &ShortThr, &UShortThr ))
                { stderr_printf ("--shortthr: missing 2 arguments" ); return NULL; }
        }
        else if ( EQUAL ("aux") ) {
            NEXTARG;
            if (  sscanf ( arg, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", AuxParam+0, AuxParam+1, AuxParam+2, AuxParam+3, AuxParam+4, AuxParam+5, AuxParam+6, AuxParam+7, AuxParam+8, AuxParam+9 ) < 1 )
                { stderr_printf ("--aux: needs 1...10 arguments" ); return NULL; }
        }
        else if ( EQUAL ("nmt") ) {
            NEXTARG;
            NMT = (float) atof (arg);
        }
        else if ( EQUAL ("tmn") ) {
            NEXTARG;
            TMN = (float) atof (arg);
        }
        else if ( EQUAL ("cvd") ) {
            NEXTARG;
            CVD_used = atoi (arg);
            if ( CVD_used == 0 )
                stderr_printf ("\nDisabling CVD always reduces quality!\a\n");
        }
        else if ( EQUAL ("ms") ) {
            NEXTARG;
            MS_Channelmode = atoi (arg);
        }
        else if ( EQUAL ("minSMR") ) {
            NEXTARG;
            if ( minSMR > (float) atof (arg) )
                stderr_printf ("Use of this option may reduce quality!\a\n");
            minSMR = (float) atof (arg);
        }
        else if ( EQUAL ("tmpMask") ) {
            NEXTARG;
            tmpMask_used = atoi (arg);
        }
        else if ( EQUAL ("ltq_max")  ||  EQUAL ("ath_max") ) {
            NEXTARG;
            Ltq_max = (float) atof (arg);
        }
        else if ( EQUAL ("ltq_gain")  ||  EQUAL ("ath_gain") ) {
            NEXTARG;
            Ltq_offset = (float) atof (arg);
        }
        else if ( EQUAL ("silent")  ||  EQUAL ("quiet") ) {
            SetStderrSilent (1);
        }
        else if ( EQUAL ("stderr") ) {
            NEXTARG;
            freopen ( arg, "a", stderr );
        }
        else if ( EQUAL ("ltq")  ||  EQUAL ("ath") ) {
            NEXTARG;
            Ltq_model = atoi (arg);
        }
        else if ( EQUAL ("ltq20")  ||  EQUAL ("ath20") ) {
            NEXTARG;
            Ltq_20kHz = atof (arg);
        }
        else if ( EQUAL ("noco") ) {
            UndoNoiseInjectionComp ();
        }
        else if ( EQUAL ("newcomb") ) {
            NEXTARG;
            CombPenalities = atoi (arg);
        }
        else if ( EQUAL ("ape1") ) {
            APE_Version = 1000;
        }
        else if ( EQUAL ("ape2") ) {
            APE_Version = 2000;
        }
        else if ( EQUAL ("priority") ) {
            NEXTARG;
            SetPriority ( atoi (arg) );
        }
        else if ( EQUAL ("bw")  ||  EQUAL ("lowpass") ) {
            NEXTARG;
            Bandwidth = atof (arg);
        }
        else if ( EQUAL ("displayupdatetime") ) {
            NEXTARG;
            DisplayUpdateTime = atoi (arg);
        }
        else if ( EQUAL ("artist") ) {
            NEXTARG;
            addtag ("Artist", 0, arg, strlen (arg), 1, 0 );
        }
        else if ( EQUAL ("album") ) {
            NEXTARG;
            addtag ("Album", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("debutalbum") ) {
            NEXTARG;
            addtag ("Debut Album", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("publisher") ) {
            NEXTARG;
            addtag ("Publisher", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("conductor") ) {
            NEXTARG;
            addtag ("Conductor", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("title") ) {
            NEXTARG;
            addtag ("Title", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("subtitle") ) {
            NEXTARG;
            addtag ("Subtitle", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("track") ) {
            NEXTARG;
            addtag ("Track", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("comment") ) {
            NEXTARG;
            addtag ("Comment", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("composer") ) {
            NEXTARG;
            addtag ("Composer", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("copyright") ) {
            NEXTARG;
            addtag ("Copyright", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("publicationright") ) {
            NEXTARG;
            addtag ("Publicationright", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("filename") ) {
            NEXTARG;
            addtag ("File", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("recordlocation") ) {
            NEXTARG;
            addtag ("Record Location", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("recorddate") ) {
            NEXTARG;
            addtag ("Record Date", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("ean/upc") ) {
            NEXTARG;
            addtag ("EAN/UPC", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("year")  ||  EQUAL ("releasedate") ) {
            NEXTARG;
            addtag ("Year", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("genre") ) {
            NEXTARG;
            addtag ("Genre", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("media") ) {
            NEXTARG;
            addtag ("Media", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("index") ) {
            NEXTARG;
            addtag ("Index", 0, arg, strlen(arg), 3, 0 );
        }
        else if ( EQUAL ("isrc") ) {
            NEXTARG;
            addtag ("ISRC", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("abstract") ) {
            NEXTARG;
            addtag ("Abstract", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("bibliography") ) {
            NEXTARG;
            addtag ("Bibliography", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("introplay") ) {
            NEXTARG;
            addtag ("Introplay", 0, arg, strlen(arg), 3, 0 );
        }
        else if ( EQUAL ("media") ) {
            NEXTARG;
            addtag ("Media", 0, arg, strlen(arg), 1, 0 );
        }
        else if ( EQUAL ("tag") ) {
            char*  p;
            NEXTARG;
            p = strchr ( arg, '=' );
            if ( p == NULL )
                addtag ( arg, strlen (arg), "", 0, 1, 0 );
            else
                addtag ( arg, p-arg, p+1, strlen(p+1), 1, 0 );
        }
        else if ( EQUAL ("tagfile") ) {
            char    buff [32768];
            char*   p;
            size_t  len;

            NEXTARG;
            p = strchr ( arg, '=' );
            if ( p == NULL ) {
                stderr_printf (" Enter value for tag key '%s': ", arg );
                fgets ( buff, sizeof buff, stdin );
                len = strlen (buff);
                while ( len > 0  &&  (buff [len-1] == '\r'  ||  buff [len-1] == '\n') )
                    len--;
                addtag ( arg, strlen(arg), buff, len, 6, 0 );
            }
            else {
                FILE*  fp = fopen ( p+1, "rb");
                if ( fp == NULL ) {
                    fprintf ( stderr, "Can't open file '%s'.\n", p+1 );
                }
                else {
                    addtag ( arg, p-arg, buff, fread (buff,1,sizeof buff,fp), 2, 3 );
                    fclose (fp);
                }
            }
        }
        else {
            stderr_printf ( "\nERROR: unknown option '--%s' !\n", arg );
            stderr_printf ( "\nNevertheless continue with encoding (Y/n)? \a" );
            if ( YesNo () == 0 ) {
                stderr_printf ( "\n\n*** Abort ***\n" );
                return NULL;
            }
            stderr_printf ( "YES\n" );
        }
    }

    return argv;
}


static int
myfeof ( FILE* fp )
{
    int  ch;

    if ( fp != (FILE*)-1 )
        return feof (fp);

    ch = CheckKeyKeep ();
    if ( ch == 'q'  ||  ch == 'Q' )
        return 1;
    return 0;
}


static FILE*
CreateOutputFile ( const char* OutputName,
                   int         WriteMode )
{
    FILE*  fp;

    if ( 0 == strcmp ( OutputName, "/dev/null") ) {
        return fopen (DEV_NULL, "wb");
    }

    if ( 0 == strcmp ( OutputName, "-")  ||  0 == strcmp ( OutputName, "/dev/stdout") ) {
        return SETBINARY_OUT (stdout);
    }

    switch ( WriteMode ) {
    case MODE_NEVER_OVERWRITE:
        fp = fopen ( OutputName, "rb" );
        if ( fp != NULL ) {
            fclose (fp);
            stderr_printf ( "ERROR: Output file '%s' already exists\n", OutputName );
            return NULL;
        }
        // fall through
    case MODE_OVERWRITE:
        return fopen ( OutputName, "wb" );

    case MODE_ASK_FOR_OVERWRITE:
        fp = fopen ( OutputName, "rb" );
        if ( fp != NULL ) {
            fclose (fp);
            stderr_printf ( "\nmppenc: Output file '%s' already exists, overwrite (Y/n)? \a", OutputName );
            if ( YesNo () == 0 ) {
                stderr_printf ( "No!!!\n\n*** Canceled overwrite ***\n" );
                return NULL;
            }
            stderr_printf ( "YES\n" );
        }
        return fopen ( OutputName, "wb" );
    }
    stderr_printf ( "ERROR: Invalid Write mode, internal error\n" );
    return NULL;
}


static const unsigned char  max_ANS_Order [32] = {        // estimated by Barks per Subband
    6, 5, 4, 3, 2, 2, 2, 2,
    2, 2, 2, 2, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};


static int
mainloop ( char** argv, const char* destdir )
{
    Float         Main      [MAXCH] [ANABUFFER];// contains PCM data for 1600 samples
    SMR_t         SMR                    [ 3];  // contains SMRs for the given frame
    float         ANSspec                [ 3] [2*MAXCH] [MAX_ANS_LINES];// L/R-Maskierungsschwellen für ANS
    Float         Sf        [MAXCH] [32] [36];  // Subbandsamples als float()
    Uint          S         [MAXCH] [32] [36];  // Subbandsamples nach Quantisierung
    pack_t        MS_Flag           [32];       // Flag zur Speicherung, ob Subband MS oder LR-kodiert wurde
    res_t         Res       [MAXCH] [32];       // Quantisierungsgenauigkeit der Subbänder
    scf_t         SCF_Index [MAXCH] [32] [ 3];  // Skalenfaktorindex für quantisierte Subbandwerte
    float         Power     [MAXCH] [32] [ 3];
    float         var_X     [MAXCH]           [FRAME_ADVANCE + 480];
    wave_t        Wave;                      // contains WAV-files arguments
    UintMax_t     AllSamplesRead;            // insgesamt gelesene Samples pro Kanal
    UintMax_t     SamplesInWAVE;
    unsigned int  CurrentRead;               // aktuell gelesene Samples pro Kanal
    unsigned int  N;                         // Zähler für verarbeitete Frames
    unsigned int  LastValidSamples;          // number of valid samples for the last frame
    unsigned int  LastValidFrame;            // overall number of frames
    char*         InputName;                 // Name der WAVE-Datei
    char*         OutputName;                // Name der Bitstromdatei
    FILE*         OutputFile;                // Filepointer auf Ausgangsdatei
    int           Silence;
    int           OldSilence = 0;
    time_t        T;
    int           ERRORS = 0;
    int           Transient  [MAXCH] [3] [PART_SHORT];
    int           TransientU [MAXCH] [3] [PART_ULTRASHORT];
    int           Transientdst [32];

    ENTER(2);

    // initialize PCM data and temporal space for subband splitter
    memset ( &Main, 0, sizeof Main  );
    memset ( var_X, 0, sizeof var_X );
    SetQualityParams  ( 5.0 );
    ReadScalingFactor ( ScalingFactor, NULL );


    while (*argv) {

        AllSamplesRead = 0.;

        ResetPsychoacoustic ();

        // Read parameters
        if ( ( argv = EvalParameters (argv) ) == NULL ) {
            stderr_printf ( "ERROR: Unable to parse parameters correctly.\n" );
            ERRORS++;
            return ERRORS;
        }

        // Read file names
        if ( ( argv = EvalNames ( argv, destdir, &InputName, &OutputName ) ) == NULL ) {
            stderr_printf ( "ERROR: Unable to parse file names correctly.\n" );
            ERRORS++;
            return ERRORS;
        }

        if ( Open_WAV_Header ( &Wave, InputName ) < 0 ) {
            stderr_printf ( "ERROR: Unable to read or decode: '%s'\n", InputName );
            ERRORS++;
            continue;
        }

        TitleBar ( InputName );
        CopyTags ( InputName );

        SampleFreq    = Wave.SampleFreq;
        SamplesInWAVE = Wave.PCMSamples;

        if ( UintMAX_FP(SamplesInWAVE) >= Wave.SampleFreq * (SkipTime + Duration) ) {
            SamplesInWAVE = Wave.SampleFreq * (SkipTime + Duration);
        }

        if ( Wave.SampleFreq > 64000.  ||  Wave.SampleFreq < 16000. ) {
            stderr_printf ( "ERROR: Sampling frequency of %g kHz is not supported!\n\n", (double)(Wave.SampleFreq * 1.e-3) );
            ERRORS++;
            continue;
        }

        // check fade-length
        if ( FadeInTime + FadeOutTime > UintMAX_FP(SamplesInWAVE) / SampleFreq ) {
            stderr_printf ( "WARNING: Duration of fade in + out exceeds file length!\n");
            FadeInTime = FadeOutTime = 0.5 * UintMAX_FP(SamplesInWAVE) / Wave.SampleFreq;
        }

        if ( Wave.BitsPerSample < 8  ||  Wave.BitsPerSample > 32 ) {
            stderr_printf ( "ERROR: %d bits per sample are not supported!\n\n", Wave.BitsPerSample );
            ERRORS++;
            continue;
        }

        if ( Wave.Channels < 1  ||  Wave.Channels > MAXCH ) {
            stderr_printf ( "ERROR: %d channels are not supported!\n\n", Wave.Channels );
            ERRORS++;
            continue;
        }

        TestProfileParams ();
        MaxSubBand = InitPsychoacousticTables ( SampleFreq, Bandwidth, TMN, NMT, MinVal_model, Ltq_model, Ltq_offset, Ltq_max, Ltq_20kHz, TransDetect1, TransDetect2, TransDetect3, TransDetect4 );

        // Create destination file (MPC bitstream)
        if ( (OutputFile = CreateOutputFile ( OutputName, WriteMode )) == NULL ) {
            stderr_printf ( "ERROR: Could not create output file '%s'\n", OutputName );
            ERRORS++;
            continue;
        }

        // Display all important parameters
        ShowParameters ( InputName, OutputName );

        if ( WIN32_MESSAGES  &&  FrontendPresent )
            SendModeMessage (MainQual);

        // Skipping samples when a skip time is given (BLOCK = 1152, CENTER = 1024+768-1152 = 640, ANABUFF = 1024+768)
        if ( SkipTime > 0. ) {
            unsigned long  SkipSamples = SampleFreq * SkipTime + 0.5;
            ssize_t        read;

            while ( SkipSamples > 0 ) {
                read = Read_WAV_Samples ( &Wave,
                                         SkipSamples < BLOCK
                                           ?  SkipSamples
                                           :  BLOCK,
                                         SkipSamples <= BLOCK + CENTER
                                           ?  (Float (*) [ANABUFFER]) & (Main [0][ANABUFFER-SkipSamples])
                                           :  (Float (*) [ANABUFFER]) & (Main [0][0]),
                                         ScalingFactor,
                                         &Silence );
                if ( read <= 0 )
                    break;
                SkipSamples   -= read;
                SamplesInWAVE -= read;
            }
        }

        InitBitStream    ();

        LastValidFrame   = (SamplesInWAVE + BLOCK - 1) / BLOCK;
        LastValidSamples = (SamplesInWAVE + BLOCK - 1) - BLOCK * LastValidFrame + 1;
        WriteHeader_SV7F ( MaxSubBand, MainQual, MS_Channelmode > 0, LastValidFrame, LastValidSamples, MAJOR_SV + 16*MINOR_SV, SampleFreq );
        FlushBitstream   ( OutputFile, 4 );

        ShowProgress ( 0., SamplesInWAVE, GetWrittenBytes () );
        T            = time ( NULL );                               // initialize timer

        for ( N = 0; (UintMax_t)N * BLOCK < SamplesInWAVE + DECODER_DELAY; N++ ) {

            // move old samples from BLOCK...BLOCK+CENTER to 0...CENTER
            memmove ( Main[0], Main[0] + BLOCK, CENTER * sizeof(float) );
            memmove ( Main[1], Main[1] + BLOCK, CENTER * sizeof(float) );

            // read new samples to CENTER...CENTER+BLOCK
            if ( SamplesInWAVE != AllSamplesRead ) {
                CurrentRead     = Read_WAV_Samples ( &Wave,
                                                    (int)minf (BLOCK, SamplesInWAVE - AllSamplesRead),
                                                    (Float (*) [ANABUFFER]) & (Main [0][CENTER]),
                                                    ScalingFactor,
                                                    &Silence );
                AllSamplesRead += CurrentRead;
            }
            else {
                CurrentRead     = 0;
            }

            // erase remaining data when not a complete block can be read
            if ( CurrentRead < BLOCK ) {
                memset ( Main[0] + (CENTER + CurrentRead), 0, (BLOCK - CurrentRead) * sizeof(float) );
                memset ( Main[1] + (CENTER + CurrentRead), 0, (BLOCK - CurrentRead) * sizeof(float) );
                SamplesInWAVE = AllSamplesRead;
            }

            if ( myfeof (Wave.fp) ) {                                                               // adapt SamplesInWAVE to the really contained number of samples
                stderr_printf ( "WAVE file has incorrect header: header: %.3f s, contents: %.3f s    \n",
                                UintMAX_FP(AllSamplesRead) / SampleFreq, UintMAX_FP(SamplesInWAVE) / SampleFreq );
                SamplesInWAVE = AllSamplesRead;
            }

            if ( FadeInTime  > 0 )
                if ( FadeInTime  > UintMAX_FP(BLOCK        + (UintMax_t)N*BLOCK) / Wave.SampleFreq )     // Sind die Einsatzzeiten richtig ???
                    Fading_In  ( Main,              + N*BLOCK, Wave.SampleFreq, Wave.Channels );
            if ( FadeOutTime > 0 )
                if ( FadeOutTime > UintMAX_FP(SamplesInWAVE - (UintMax_t)N*BLOCK) / Wave.SampleFreq )     // Sind die Einsatzzeiten richtig ???
                    Fading_Out ( Main, (unsigned int)(SamplesInWAVE - (UintMax_t)N*BLOCK), Wave.SampleFreq, Wave.Channels );

            if ( !Silence  ||  !OldSilence ) {      // nur wenn auch der letzte Frame nur Nullen enthalten hat, erhält man Nullsamples am Ausgang der Filterbank, Und der Encoder-Kern arbeitet fehlerhaft bei Nur-Nullsamples (Division durch 0)

                AnalyseFilter ( MaxSubBand, Main[0] + CENTER, var_X[0], Sf[0] );
                AnalyseFilter ( MaxSubBand, Main[1] + CENTER, var_X[1], Sf[1] );

                Psychoakustisches_Modell ( SMR, ANSspec, 31, Main, ShortThr, UShortThr, Max_ANS_Order, Transient, TransientU );   // Psychoakustik liefert SMR's für Eingangsdaten 'Main'

#if 0
                { int i, k;
                  static long  z = 0;

                  for ( k = 0; k < 3; k++ ) {
                  printf ("%5u %7.3f ", z, z*1152./SampleFreq );
                  for ( i = 0; i < PART_SHORT; i++ ) printf ("%c", Transient[0][k][i] ? '#' : '.' );
                  printf ("    ");
                  for ( i = 0; i < PART_ULTRASHORT; i++ ) printf ("%c", TransientU[0][k][i] ? '#' : '.' );
                  printf ("\n");
                  printf ("%5u %7.3f ", z, z*1152./SampleFreq );
                  for ( i = 0; i < PART_SHORT; i++ ) printf ("%c", Transient[1][k][i] ? '#' : '.' );
                  printf ("    ");
                  for ( i = 0; i < PART_ULTRASHORT; i++ ) printf ("%c", TransientU[1][k][i] ? '#' : '.' );
                  printf ("\n");
                  }
                  z++;
                }
#endif

                if ( Max_ANS_Order > 0 ) {  // Attention: Can be NANs !!!!
                    minf3a ( ANSspec[0][0], ANSspec[0][0], ANSspec[1][0], ANSspec[2][0], MAX_ANS_LINES );
                    minf3a ( ANSspec[0][1], ANSspec[0][1], ANSspec[1][1], ANSspec[2][1], MAX_ANS_LINES );
                    minf3a ( ANSspec[0][2], ANSspec[0][2], ANSspec[1][2], ANSspec[2][2], MAX_ANS_LINES );
                    minf3a ( ANSspec[0][3], ANSspec[0][3], ANSspec[1][3], ANSspec[2][3], MAX_ANS_LINES );
                }

                maxf3a ( SMR[0].Ch0, SMR[0].Ch0, SMR[1].Ch0, SMR[2].Ch0, MaxSubBand+1 );
                maxf3a ( SMR[0].Ch1, SMR[0].Ch1, SMR[1].Ch1, SMR[2].Ch1, MaxSubBand+1 );
                maxf3a ( SMR[0].Ch2, SMR[0].Ch2, SMR[1].Ch2, SMR[2].Ch2, MaxSubBand+1 );
                maxf3a ( SMR[0].Ch3, SMR[0].Ch3, SMR[1].Ch3, SMR[2].Ch3, MaxSubBand+1 );

                if ( minSMR > 0 )
                    RaiseSMR ( MaxSubBand, SMR, minSMR );

                if ( MS_Channelmode > 0 )
                    MS_LR_Entscheidung ( MaxSubBand, MS_Flag, SMR, Sf );                            // Auswahl von M/S- oder L/R-Coding

                SCF_Extraction ( MaxSubBand, SCF_Index[0], Sf[0], SNR_comp[0], Power[0] );          // Extraktion der Skalenfaktoren und Normierung der Subbandsamples
                SCF_Extraction ( MaxSubBand, SCF_Index[1], Sf[1], SNR_comp[1], Power[1] );          // Extraktion der Skalenfaktoren und Normierung der Subbandsamples

                TransientCalc ( Transientdst, Transient, TransientU );
                if ( Max_ANS_Order > 0 ) {
                    FindOptimalANS ( MaxSubBand, MS_Flag, ANSspec[0][0], ANSspec[0][2], ANS_Order[0], max_ANS_Order, SNR_comp[0], FIR[0], SMR[0].Ch0, SMR[0].Ch2, SCF_Index[0], Transientdst );     // für L bzw. M
                    FindOptimalANS ( MaxSubBand, MS_Flag, ANSspec[0][1], ANSspec[0][3], ANS_Order[1], max_ANS_Order, SNR_comp[1], FIR[1], SMR[0].Ch1, SMR[0].Ch3, SCF_Index[1], Transientdst );     // für R bzw. S
                }

                Allocate ( MaxSubBand, Res[0], Sf[0], SCF_Index[0], SNR_comp[0], SMR[0].Ch0, Power[0], Transientdst );   // allocate bits for left + right channel
                Allocate ( MaxSubBand, Res[1], Sf[1], SCF_Index[1], SNR_comp[1], SMR[0].Ch1, Power[1], Transientdst );

                Quantisierung ( MaxSubBand, Res[0], Sf[0], S[0], ANS_Order[0], FIR[0], SCF_Index[0] );     // quantize samples
                Quantisierung ( MaxSubBand, Res[1], Sf[1], S[1], ANS_Order[1], FIR[1], SCF_Index[1] );     // quantize samples
            }

            WriteBits           ( 0, 16 );                                                           // Reserviere 16 bit für Sprung-Info
            WriteBitstream_SV7F ( !Silence  ||  !OldSilence  ?  MaxSubBand  :  -1, Res, MS_Flag, SCF_Index, S );    // Schreibe SV7-Bitstrom
            FlushBitstream      ( OutputFile, 0 );

            OldSilence         = Silence;

            if ( (Int)(time (NULL) - T) >= 0 ) {                                                    // Ausgabe
                T += labs (DisplayUpdateTime);
                ShowProgress ( (N+1)*(UintMax_t)BLOCK, SamplesInWAVE, GetWrittenBytes () );
            }

        }
        LEAVE(2);

        // Schreibe letztes unvollständiges Wort in den Puffer, so daß beim nächsten Flush auch dieses geschrieben wird
        ShowProgress ( SamplesInWAVE, SamplesInWAVE, GetWrittenBytes () );

        LastValidFrame   = (SamplesInWAVE + BLOCK - 1) / BLOCK;
        LastValidSamples = (SamplesInWAVE + BLOCK - 1) - BLOCK * LastValidFrame + 1;      // in the case of broken wav-header, recalculate the overall frames and the valid samples for the last frame
        UpdateHeader_SV7F ( OutputFile, LastValidFrame, LastValidSamples );

        FinalizeTags ( OutputFile, APE_Version );
        fclose ( OutputFile );

        Close_WAV_Header ( &Wave );

        if ( DelInput == 0xAFFEDEAD  &&  remove (InputName) == -1 )
            stderr_printf ( "\n\nERROR: Could not delete input file '%s'\n", InputName );

        if ( WIN32_MESSAGES  &&  FrontendPresent )
            SendQuitMessage ();

        stderr_printf ( "\n" );
        Report_Huffgen ("codetable_codes.cc");
    }

    return ERRORS;
}


/************ The main() function *****************************/
int Cdecl
main ( int argc, char** argv )
{
    const char*  lastarg;
    int          ret;

    DisableSUID ();

#ifdef _OS2
    _wildcard ( &argc, &argv );
#endif

    if ( WIN32_MESSAGES  &&  (FrontendPresent = SearchForFrontend ()) != 0 )    // search for presence of Windows Frontend
        SendStartupMessage ( "MPC V" MPPENC_VERSION );

    START();
    ENTER(1);

    // Welcome message
    if ( argc < 2  ||  ( 0 != strcmp ( argv[1], "--silent")  &&  0 != strcmp ( argv[1], "--quiet")) )
        (void) stderr_printf ("\r\x1B[1m\r%s\n\x1B[0m\r     \r", About );

    // no arguments or call for help
    if ( argc < 2  ||  0 == strcmp ( argv[1], "-h")  ||  0 == strcmp ( argv[1], "-?")  ||  0 == strcmp ( argv[1], "--help") ) {
        SetQualityParams ( 5.0 );
        dup2 ( 1, 2 );
        shorthelp ();
        return 1;
    }

    if ( 0 == strcmp ( argv[1], "--longhelp")  ||  0 == strcmp ( argv[1], "-??") ) {
        SetQualityParams ( 5.0 );
        dup2 ( 1, 2 );
        longhelp ();
        return 1;
    }

#ifdef FAST_MATH
    Init_FastMath ();
#endif
    Init_SCF      ();
    Init_FPU      ();
    Init_ANS      ();
    Init_AnalyseFilter  ();
    Init_Encoder_Huffman_Tables ();

    lastarg = argv [argc-1];
    if ( IsDirectory (lastarg)  ||  0 == strcmp (lastarg, "/dev/null") ) {
        argv [argc-1] = NULL;
        ret = mainloop ( argv + 1, lastarg );        // analyze command line and do the requested work
    }
    else {
        ret = mainloop ( argv + 1, NULL    );        // analyze command line and do the requested work
    }

    if ( ret != 0 ) {
        stderr_printf ( "%u errors occured.\n\a", ret );
    }

    LEAVE(1);
    REPORT();

    return ret;
}

/* end of mppenc.c */
