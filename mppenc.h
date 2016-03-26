#ifndef MPPENC_MPPENC_H
#define MPPENC_MPPENC_H


#define MAJOR_SV           15
#define MINOR_SV           15


#define FRAME_ADVANCE    1152
#define FOUR                3                     // number of FFT transforms per long block psychoacoustic
#define EIGHT              12
#define LONG_ADVANCE       (FRAME_ADVANCE / 3)
#define SHORT_ADVANCE      (LONG_ADVANCE / FOUR)
#define ULTRASHORT_ADVANCE (LONG_ADVANCE / EIGHT)


#ifdef _WIN32
# define CVD_FASTLOG
# define FAST_MATH
#endif

#include "mppdec.h"
#undef SCF
#include "codetable.h"
#include "codetable_data.h"
#include "minimax.h"

#define SCF_RES           -64

#define WIN32_MESSAGES      1                           // Unterstützung von Windows-Messaging zu Frontend

// ans.c
#define MAX_ANS_ORDER       6                           // maximale Ordnung des Adaptive Noise Shaping Filters (IIR)
#define MAX_ANS_BANDS      16
#define MAX_ANS_LINES    ((1024/2/32) * MAX_ANS_BANDS)  // maximale Anzahl an noiseshaped FFT-lines
#define MS2SPAT1            0.5f
#define MS2SPAT2            0.25f
#define MS2SPAT3            0.125f
#define MS2SPAT4            0.0625f

// cvd.c
#define CVD_UNPRED          0.040f              // unpredictability (cw) for CVD-detected bins, e33 (04)
#define MAX_CVD_LINE      300                   // maximaler FFT-Index für CVD
#define MIN_ANALYZED_IDX   13                   // maximum base-frequency = 44100/MIN_ANALYZED_IDX ^^^^^^
#define MED_ANALYZED_IDX   50                   // maximum base-frequency = 44100/MED_ANALYZED_IDX ^^^^^^ 882
#define MAX_ANALYZED_IDX  900                   // minimum base-frequency = 44100/MAX_ANALYZED_IDX         49

// mppenc.h
#define CENTER          (1024 + 2*384 - BLOCK)  // offset for centering current data in Main-array
#define BLOCK           1152                    // blocksize
#define ANABUFFER       (CENTER + BLOCK)        // size of PCM-data array for analysis

/*
****** old ******
                         CENTER                                                                 ANABUFFER
                            |                                                                       |
0      128     256     384     512     640     768     896    1024    1152    1280    1408    1536    1664    1792    1920    2048
+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
I«««««« Remaining PCM »»»»»»I«««««««««««««««««««««««««« WAVE Reader Block »»»»»»»»»»»»»»»»»»»»»»»»»»I   :       :       :       :  448
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
«« Remaining Input »»»»»»»»»I   :       :       :       :       :       :       :       :       :       :       :       :       :  -32
:       :       :       :   I«««««««««««««««««««««««« Input for Subbandfilter »»»»»»»»»»»»»»»»»»»»»»I   :       :       :       :  448
:       :      I««««« Subframe 1 »»»»»»I««««« Subframe 2 »»»»»»I««««« Subframe 3 »»»»»»I:       :       :       :       :       :  207
:       :      I««««««««««««««««««««««««««« Subband Splitter »»»»»»»»»»»»»»»»»»»»»»»»»»I:       :       :       :       :       :  207
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
I««««««««««««««««««««««««««« 1st FFT »»»»»»»»»»»»»»»»»»»»»»»»»»»I       :       :       :       :       :       :       :       :    0
:       :       :       :       :   I««««««««««««««««««««««««««« 2nd FFT »»»»»»»»»»»»»»»»»»»»»»»»»»»I   :       :       :       :  576
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
:I««««« 0th »»»»»I      :       :  I««««« 4th »»»»»I    :       :       :       :       :       :       :       :       :       :   24 + 576*n
:       : I««««« 1st »»»»»I     :       :   I««««« 5th »»»»»I   :       :       :       :       :       :       :       :       :  168 + 576*n
:       :       : I««««« 2nd »»»»»I     :       :   I««««« 6th »»»»»I   :       :       :       :       :       :       :       :  312 + 576*n
:       :       :       :  I««««« 3rd »»»»»I    :       :    I««««« 7th »»»»»I  :       :       :       :       :       :       :  456 + 576*n
:       :       :       :       :  I««««« 4th »»»»»I    :       :    I««««« 8th »»»»»I  :       :       :       :       :       :  600 + 576*n
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
I««««««««««««««««««««««««««««««««««««««« CVD Analyse Window »»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»I   :       :       :       :    0
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-
0      128     256     384     512     640     768     896    1024    1152    1280    1408    1536    1664    1792    1920    2048
          |
    SHORTFFT_OFFSET


****** new ******
                                      CENTER                                                                ANABUFFER
                                        |                                                                       |
0      128     256     384     512     640     768     896    1024    1152    1280    1408    1536    1664    1792    1920    2048
+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
I«««««««««««« Remaining PCM »»»»»»»»»»»»I«««««««««««««««««««««««««« WAVE Reader Block »»»»»»»»»»»»»»»»»»»»»»»»»»I       :       :  640
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
:       : I«««««« Remaining Input »»»»»»I       :       :       :       :       :       :       :       :       :       :       :  160
:       :       :       :       :       I«««««««««««««««««««««««« Input for Subbandfilter »»»»»»»»»»»»»»»»»»»»»»I       :       :  640
:       :       :       :  I««««« Subframe 1 »»»»»»I««««« Subframe 2 »»»»»»I««««« Subframe 3 »»»»»»I    :       :       :       :  399
:       :       :       :  I««««««««««««««««««««««««««« Subband Splitter »»»»»»»»»»»»»»»»»»»»»»»»»»I    :       :       :       :  399
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
I««««««««««««««««««««««««««« 1st FFT »»»»»»»»»»»»»»»»»»»»»»»»»»»I       :       :       :       :       :       :       :       :    0
:       :       :       I««««««««««««««««««««««««««« 2nd FFT »»»»»»»»»»»»»»»»»»»»»»»»»»»I       :       :       :       :       :  384
:       :       :       :       :       :       I««««««««««««««««««««««««««« 3rd FFT »»»»»»»»»»»»»»»»»»»»»»»»»»»I       :       :  576
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
:       :     I««««« 0th »»»»»I :     I««««« 3rd »»»»»I :     I««««« 6th »»»»»I :       :       :       :       :       :       :  232 + 384*n
:       :       :     I««««« 1st »»»»»I :     I««««« 4th »»»»»I :     I««««« 7th »»»»»I :       :       :       :       :       :  360 + 384*n
:       :       :       :     I««««« 2nd »»»»»I :     I««««« 5th »»»»»I :     I««««« 8th »»»»»I :       :       :       :       :  488 + 384*n
:       :       :       :       :     I««««« 3rd »»»»»I :     I««««« 6th »»»»»I :     I««««« 9th »»»»»I :       :       :       :  616 + 384*n
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
:       :       :       :|1«1|  :|2«1|  :|3«1|  :|4«1|  :|5«1|  :|6«1|  :|7«1|  :|8«1|  :|9«1|  :       :       :       :       :
:       :       :       :  |1«2|:  |2«2|:  |3«2|:  |4«2|:  |5«2|:  |6«2|:  |7«2|:  |8«2|:  |9«2|:       :       :       :       :
:       :       :    |0«3|   |1«3|   |2«3|   |3«3|   |4«3|   |5«3|   |6«3|   |7«3|   |8«3|   |9«3|      :       :       :       :
:       :       :      |0«4|   |1«4|   |2«4|   |3«4|   |4«4|   |5«4|   |6«4|   |7«4|   |8«4|   |9«4|    :       :       :       :
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
I««««««««««««««««««««««««««««««««««««««««««««« CVD Analyse Window »»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»I       :       :    0
:       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :       :
+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-
0      128     256     384     512     640     768     896    1024    1152    1280    1408    1536    1664    1792    1920    2048
                     |
               SHORTFFT_OFFSET
*/

// psy.c
#define SHORTFFT_OFFSET       (CENTER - 280)            // fft-offset for short FFT's
#define ULTRASHORTFFT_OFFSET  (SHORTFFT_OFFSET + 48)    // fft-offset for ultrashort FFT's
#define PREFAC_LONG           10.                       // preecho-factor for long partitions

// psy_tab.h
#define PART_LONG             60                        // Anzahl an Partitionen für long
#define PART_SHORT            (PART_LONG / 3)           // Anzahl an Partitionen für short
#define PART_ULTRASHORT       (PART_LONG / 6)           // Anzahl an Partitionen für ultrashort
#define PART_SHORT_START       1                        // erste benutzte Partitionen für short
#define PART_ULTRASHORT_START  3                        // erste benutzte Partitionen für ultrashort

// quant.h
#define SCFfac              0.840896415253714543031f    // = Quotient zweier benachbarter SCFs

// fast but maybe more inaccurate, use if you need speed
#ifdef __GNUC__
# define SIN(x)      sinf ((float)(x))
# define COS(x)      cosf ((float)(x))
# define ATAN2(x,y)  atan2f ((float)(x), (float)(y))
# define SQRT(x)     sqrtf ((float)(x))
# define LOG(x)      logf ((float)(x))
# define LOG10(x)    log10f ((float)(x))
# define EXP(x)      expf ((float)(x))
# define POW(x,y)    expf (logf(x) * (y))
# define POW10(x)    expf (M_LN10 * (x))
# define FLOOR(x)    floorf ((float)(x))
# define IFLOOR(x)   (int) floorf ((float)(x))
# define FABS(x)     fabsf ((float)(x))
#else
# define SIN(x)      (float) sin (x)
# define COS(x)      (float) cos (x)
# define ATAN2(x,y)  (float) atan2 (x, y)
# define SQRT(x)     (float) sqrt (x)
# define LOG(x)      (float) log (x)
# define LOG10(x)    (float) log10 (x)
# define EXP(x)      (float) exp (x)
# define POW(x,y)    (float) pow (x,y)
# define POW10(x)    (float) pow (10., (x))
# define FLOOR(x)    (float) floor (x)
# define IFLOOR(x)   (int)   floor (x)
# define FABS(x)     (float) fabs (x)
#endif

#define SQRTF(x)      SQRT (x)
#ifdef FAST_MATH
# define TABSTEP      64
# define COSF(x)      my_cos ((float)(x))
# define ATAN2F(x,y)  my_atan2 ((float)(x), (float)(y))
# define IFLOORF(x)   my_ifloor ((float)(x))
#else
# undef  TABSTEP
# define COSF(x)      COS (x)
# define ATAN2F(x,y)  ATAN2 (x,y)
# define IFLOORF(x)   IFLOOR (x)
#endif


typedef struct {
    float  Ch0 [32];
    float  Ch1 [32];
    float  Ch2 [32];
    float  Ch3 [32];
} SMR_t;

typedef struct {
    FILE*          fp;                   // FIle pointer to read data
    unsigned int   PCMOffset;            // File offset of PCM data
    long double    SampleFreq;           // Sample frequency in Hz
    unsigned int   BitsPerSample;        // used bits per sample, 8*BytesPerSample-7 <= BitsPerSample <= BytesPerSample
    unsigned int   BytesPerSample;       // allocated bytes per sample
    unsigned int   Channels;             // Number of channels, 1...8
    Uint32_t       ChannelMask;
    UintMax_t      PCMBytes;             // PCM Samples (in 8 bit units)
    UintMax_t      PCMSamples;           // PCM Samples per Channel
    int            type;                 // TYPE_FILE, TYPE_PIPE, TYPE_SPECIAL
} wave_t;

#define TYPE_UNKNOWN    0
#define TYPE_FILE       1                               // read via fread(), close via fclose()
#define TYPE_PIPE       2                               // read via fread(), close via pclose()
#define TYPE_SOCKET     3                               // read via readv(), close via shutdown()/close()
#define TYPE_SPECIAL    4                               // read via special code, close via special code

// subband.c
void   Init_AnalyseFilter ( void );
void   AnalyseFilter      ( const int MaxBand, const Float input[1152], float* tmpbuffer, Float output[32][36] );


// ans.c
extern unsigned char ANS_Order [MAXCH] [32];                       // Ordnung des Adaptiven Noiseshapings (0: off, 1...5: on)
extern float         FIR       [MAXCH] [32] [MAX_ANS_ORDER];        // enthält FIR-Filter für NoiseShaping

void   Init_ANS       ( void );
void   NS_Analyse     ( const int, const unsigned char* MS, const SMR_t, const int Transient [32] );
void   FindOptimalANS ( const int MaxBand, const unsigned char* ms, const float spec0 [] [16], const float spec1 [] [16], unsigned char* ANS, const unsigned char* ANSmaxOrder, float* snr_comp, float fir [] [MAX_ANS_ORDER], const float* smr0, const float* smr1, const scf_t scf [32][3], const int Transient[32] );


// bitstream.c
unsigned long
      GetWrittenBytes   ( void );
void  InitBitStream     ( void );
void  FlushBitstream    ( FILE* fp, int lenoffset );
void  WriteBits         ( const Uint32_t input, const unsigned int bits );
void  WriteBit          ( const unsigned int input );

// codetable.c */
void  Init_Decoder_Huffman_Tables ( void );
void  Init_Encoder_Huffman_Tables ( void );


// cvd.c
int   CVD2048 ( const float* spec, unsigned char* vocal );


// fastmath.c
void   Init_FastMath ( void );
extern const float  tabatan2   [] [2];
extern const float  tabcos     [] [2];
extern const float  tabsqrt_ex [];
extern const float  tabsqrt_m  [] [2];


// fft4g.c
void   rdft                ( const int, float*, int*, float* );
void   Generate_FFT_Tables ( const int, int*, float* );


// fft_routines.c
void   Init_FFT        ( float alpha0, float alpha1, float alpha2, float alpha3 );
void   PowSpec64       ( const float* input, float* );
void   PowSpec256      ( const float* input, float* );
void   PowSpec1024     ( const float* input, float* );
void   PowSpec1024_Mix ( const float* input0, const float* input1, float* m, float* s );
void   PowSpec2048     ( const float* input, float* );
void   PolarSpec1024   ( const float* input, float*, float* );
void   Cepstrum2048    ( float* cep, const int );


// fpu.c
void   Init_FPU        ( void );


// keyboard.c
int    WaitKey         ( void );
int    CheckKeyKeep    ( void );
int    CheckKey        ( void );


// mppenc.c
extern float         SNR_comp [MAXCH] [32];     // SNR-Kompensation nach SCF-Zusammenfassung und ANS-Gewinn
extern unsigned int  MS_Channelmode;            // globaler Flag für erweiterte Funktionalität
extern int           AuxParam [10];


// psy.c
extern unsigned int  CVD_used;          // globaler Flag für ClearVoiceDetection (weitere Switches für das Psychoakustische Modell)
extern float         varLtq;            // variable Ruhehörschwelle
extern unsigned int  tmpMask_used;      // globaler Flag für temporale Maskierung

void   ResetPsychoacoustic      ( void );
void   TonalitySetup            ( float fs, int advance, float tmn, float nmt, int minvalmodel );
void   TransDetectSetup         ( float fs,              float TD1, float TD2, float TD3, float TD4 );
void   Psychoakustisches_Modell ( SMR_t* const SMR, Float ANSspec[][2*MAXCH][MAX_ANS_LINES], const int, const Float pcm [] [ANABUFFER], float Threshold, float UThreshold, unsigned int Max_NS_Order, int Transient [MAXCH] [3] [PART_SHORT], int TransientU [MAXCH] [3] [PART_ULTRASHORT] );
void   TransientCalc            ( int Tdst [32], const int T [MAXCH] [3] [PART_SHORT], const int TU [MAXCH] [3] [PART_ULTRASHORT] );
void   RaiseSMR                 ( const int MaxBand, SMR_t* smr, float minSMR );
void   MS_LR_Entscheidung       ( const int, pack_t* MS, SMR_t*, Float S[][32][36] );


// psy_tab.c
extern float        fftLtq   [512];             // Ruhehörschwelle (FFT)
extern float        partLtq  [PART_LONG];       // Ruhehörschwelle (Partitionen)
extern float        invLtq   [PART_LONG];       // inverse Ruhehörschwelle (Partitionen, long)
extern float        Loudness [PART_LONG];       // Gewichtungsfaktoren für Berechnung der Lautstärke
extern float        MinVal   [PART_LONG];       // an Modell angepasste minimale Güte, minval für long
extern float        SPRD     [PART_LONG] [PART_LONG]; // tabellierte Spreadingfunktion

extern const float  Butfly    [7];              // Antialiasing für Berechnung der Subbandleistungen
extern const float  InvButfly [7];              // Antialiasing für Berechnung der Maskierungsschwellen
extern const int    wl        [PART_LONG];      // w_low  für long
extern const int    wh        [PART_LONG];      // w_low  für long
extern const float  iw        [PART_LONG];      // inverse partition-width for long
extern const int    wl_short  [PART_SHORT];     // w_low  für short
extern const int    wh_short  [PART_SHORT];     // w_low  für short
extern const float  iw_short  [PART_SHORT];     // inverse partition-width for short
extern const int    wl_ultrashort  [PART_ULTRASHORT];     // w_low  für ultrashort
extern const int    wh_ultrashort  [PART_ULTRASHORT];     // w_low  für ultrashort
extern const float  iw_ultrashort  [PART_ULTRASHORT];     // inverse partition-width for ultrashort

int   InitPsychoacousticTables ( float fs, float bw, float tmn, float nmt, int minvalmodel, int ltq_model, float ltq_offset, float ltq_max, float ltq_20kHz, float TD1, float TD2, float TD3, float TD4 );


// quant.c
extern float  SCF    [128];
extern float  invSCF [128];                      // tabellierte Skalenfaktoren (invertiert)

void    Init_SCF             ( void );
float   SNR_Estimator        ( const float* samples, const int res );
float   SNR_Estimator_Trans  ( const float* samples, const int res );
int     MinRes_Estimator     ( float SMR );
void    QuantizeSubband      ( unsigned int* qu_output, const float* input, const int res, float* errors );
void    QuantizeSubband_ANS  ( unsigned int* qu_output, const float* input, const int res, float* errors, const float* FIR );
void    UndoNoiseInjectionComp ( void );

// quantnew.c
float   NoiseEstimator36     ( const float* input, const float Scaler );
float   NoiseEstimator12     ( const float* input, const float Scaler );
float   NoiseEstimator36_ANS ( const float* input, const float Scaler, float feedback [6], float qerrors [6 + 36] );
float   NoiseEstimator12_ANS ( const float* input, const float Scaler, float feedback [2], float qerrors [2 + 12] );
void    QuantSubband12       ( int* output, const float* input, const float Scaler, float* qerrors );
void    QuantSubband12_ANS   ( int* output, const float* input, const float Scaler, float* qerrors, const float* FIR );


// encode_sv7.c
void    WriteHeader_SV7F     ( const unsigned int, const unsigned int, const unsigned int, const Uint32_t TotalFrames, const unsigned int SmaplesRest, const unsigned int StreamVersion, int SampleFreq );
void    WriteBitstream_SV7F  ( int           MaxBand,
                               const res_t   Res       [] [32],
                               const pack_t  MS           [32],
                               const scf_t   SCF_Index [] [32] [ 3],
                               const Uint    S         [] [32] [36] );
int     UpdateHeader_SV7F    ( FILE* fp, Uint32_t Frames, Uint ValidSamples );


// tags.c
void    Init_Tags        ( void );
int     FinalizeTags     ( FILE* fp, unsigned int Version );
int     addtag           ( const char* key, size_t keylen, const unsigned char* value, size_t valuelen, int converttoutf8, int flags );
int     gettag           ( const char* key, char* dst, size_t len );
int     CopyTags         ( const char* filename );


// wave_in.c
int     Open_WAV_Header  ( wave_t* type, const char* name );
size_t  Read_WAV_Samples ( wave_t* t, const size_t RequestedSamples, Float pcm [] [ANABUFFER], const float* scale, int* Silence );
int     Close_WAV_Header ( wave_t* type );
int     init_in          ( const int SampleCount, const int SampleFreq, const int Channels, const int  BitsPerSample );
int     init_out         ( const int SampleCount, const int SampleFreq, const int Channels, const int  BitsPerSample );
size_t  get_in           ( void*        DataPtr );
int     put_out          ( const void*  DataPtr, const size_t  Bytes );


// winmsg.c
#ifdef _WIN32
int    SearchForFrontend   ( void );
void   SendQuitMessage     ( void );
void   SendModeMessage     ( const int );
void   SendStartupMessage  ( const char* String );
void   SendProgressMessage ( const int, const float, const float );
#else
# undef  WIN32_MESSAGES
# define WIN32_MESSAGES                 0
# define SearchForFrontend()            (0)
# define SendQuitMessage()              (void)0
# define SendModeMessage(x)             (void)0
# define SendStartupMessage(s)          (void)0
# define SendProgressMessage(x,y,z)     (void)0
#endif /* _WIN32 */


// codetablemake.c
#if 1
void  Report_Huffgen ( const char* filename );
#else
# define Report_Huffgen(filename)
#endif


#endif /* MPPENC_MPPENC_H */

/* end of mppenc.h */
