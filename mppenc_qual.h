
typedef struct {
    float            ShortThr;
    float            UShortThr;
    unsigned char    MinVal_model;
    unsigned char    Ltq_model;
    signed char      Ltq_20kHz;
    signed char      Ltq_offset;
    float            TMN;
    float            NMT;
    signed char      minSMR;
    signed char      Ltq_max;
    unsigned short   BandWidth;
    unsigned char    tmpMask_used;
    unsigned char    CVD_used;
    float            varLtq;
    unsigned char    MS_Channelmode;
    unsigned char    CombPenalities;
    unsigned char    Max_ANS_Order;
    float            PNS;
    float            TransDetect1;      
    float            TransDetect2;       
    float            TransDetect3;       
    float            TransDetect4;       
} Profile_Setting_t;


#define PROFILE_PRE2_TELEPHONE   5      // --quality  0
#define PROFILE_PRE_TELEPHONE    6      // --quality  1
#define PROFILE_TELEPHONE        7      // --quality  2
#define PROFILE_THUMB            8      // --quality  3
#define PROFILE_RADIO            9      // --quality  4
#define PROFILE_STANDARD        10      // --quality  5
#define PROFILE_XTREME          11      // --quality  6
#define PROFILE_INSANE          12      // --quality  7
#define PROFILE_BRAINDEAD       13      // --quality  8
#define PROFILE_POST_BRAINDEAD  14      // --quality  9
#define PROFILE_POST2_BRAINDEAD 15      // --quality 10


static const Profile_Setting_t  Profiles [16] = {
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
/*    Short UShort   MinVal  Ltq-  offset  Ltq_               min  Ltq_  Band-  tmpMask  CVD_  varLtq   MS    Comb   NS_        Trans  */
/*    Thr    Thr     Choice  Model @20kHz  incr    TMN   NMT  SMR  max   Width  _used    used  _used  channel Penal used   PNS  Detect */
    { 1.e9f, 1.e9f,  1,      3,     30,   30,     3.0, -1.0,  0,  106,   4820,   1,      1,     1.,      3,     24,  6,  20.00, 1e9, 1e9, 1e9, 1e9 },  // 0: pre-Telephone
    { 80.0f, 1.e9f,  1,      3,     30,   24,     6.0,  0.5,  0,  100,   7570,   1,      1,     1.,      3,     20,  6,   8.00, 500, 999, 1e9, 1e9 },  // 1: pre-Telephone
    { 40.0f, 3.20f,  1,      4,     30,   18,     9.0,  2.0,  0,   94,  10300,   1,      1,     1.,      4,     18,  6,   3.00, 250, 500, 441, 441 },  // 2: Telephone
    { 20.0f, 1.60f,  2,      4,     12,   12,    12.0,  3.5,  0,   88,  13090,   1,      1,     1.,      5,     15,  6,   1.00, 167, 333, 196, 196 },  // 3: Thumb
    { 10.0f, 0.80f,  2,      4,      6,    6,    15.0,  5.0,  0,   82,  15800,   1,      1,     1.,      6,     10,  6,   0.25, 125, 250, 110, 110 },  // 4: Radio
    {  5.0f, 0.40f,  2,      5,      0,    0,    18.0,  6.5,  1,   76,  22000,   1,      2,     1.,     11,      9,  6,   0.00, 100, 200,  71,  71 },  // 5: Standard
    {  4.0f, 0.32f,  2,      5,     -6,   -6,    21.0,  8.0,  2,   70,  24000,   1,      2,     1.,     12,      7,  6,   0.00,  83, 167,  49,  49 },  // 6: Xtreme
    {  3.2f, 0.25f,  2,      5,    -12,  -12,    24.0,  9.5,  3,   64,  24000,   1,      2,     2.,     13,      5,  6,   0.00,  71, 143,  36,  36 },  // 7: Insane
    {  2.5f, 0.20f,  2,      5,    -18,  -18,    27.0, 11.0,  4,   58,  24000,   1,      2,     4.,     13,      4,  6,   0.00,  63, 125,  28,  28 },  // 8: BrainDead
    {  2.0f, 0.16f,  2,      5,    -24,  -24,    30.0, 12.5,  5,   52,  24000,   1,      2,     8.,     13,      4,  6,   0.00,  55, 111,  21,  21 },  // 9: post-BrainDead
    {  1.6f, 0.12f,  2,      5,    -30,  -30,    33.0, 14.0,  6,   46,  24000,   1,      2,    16.,     15,      2,  6,   0.00,  50, 100,  18,  18 },  //10: post-BrainDead
};


static int
TestProfileParams ( void )
{
    int  i;

    MainQual = PROFILE_PRE2_TELEPHONE;

    for ( i = PROFILE_PRE2_TELEPHONE; i <= PROFILE_POST2_BRAINDEAD; i++ ) {
        if ( ShortThr     > Profiles [i].ShortThr     ) continue;
        if ( UShortThr    > Profiles [i].UShortThr    ) continue;
        if ( MinVal_model < Profiles [i].MinVal_model ) continue;
        if ( Ltq_model    < Profiles [i].Ltq_model    ) continue;
        if ( Ltq_offset   > Profiles [i].Ltq_offset   ) continue;
        if ( Ltq_max      > Profiles [i].Ltq_max      ) continue;       // Hier müßte noch Offset berücksichtigt werden
        if ( TMN          < Profiles [i].TMN          ) continue;
        if ( NMT          < Profiles [i].NMT          ) continue;
        if ( minSMR       < Profiles [i].minSMR       ) continue;
        if ( Bandwidth    < Profiles [i].BandWidth    ) continue;
        if ( tmpMask_used < Profiles [i].tmpMask_used ) continue;
        if ( CVD_used     < Profiles [i].CVD_used     ) continue;
        if ( varLtq       > Profiles [i].varLtq       ) continue;
     // if ( Max_ANS_Order< Profiles [i].Max_ANS_Order) continue;
        if ( PNS          > Profiles [i].PNS          ) continue;
        MainQual = i;
    }
    return MainQual;
}


static void
SetQualityParams ( float qual )
{
    const Profile_Setting_t*  p;
    float                     mix;
    int                       i;

    if      ( qual <  0. ) {
        qual =  0.;
    }
    if      ( qual > 10. ) {
        qual = 10.;
#ifdef _WIN32
        stderr_printf ( "\nmppenc: Can't open MACDll.dll, quality set to 10.00\n" );
#else
        stderr_printf ( "\nmppenc: Can't open libMAC.so, quality set to 10.00\n" );
#endif
    }

    i   = (int) qual + PROFILE_PRE2_TELEPHONE;
    p   = Profiles + i;
    mix = qual - (int) qual;

    MainQual       = i;
    ShortThr       = p[0].ShortThr    * (1-mix) + p[1].ShortThr    * mix;
    UShortThr      = p[0].UShortThr   * (1-mix) + p[1].UShortThr   * mix;
    MinVal_model   = p[0].MinVal_model  ;
    Ltq_model      = p[0].Ltq_model     ;
    Ltq_offset     = p[0].Ltq_offset  * (1-mix) + p[1].Ltq_offset  * mix;
    Ltq_20kHz      = p[0].Ltq_20kHz   * (1-mix) + p[1].Ltq_20kHz   * mix;
    Ltq_max        = p[0].Ltq_max     * (1-mix) + p[1].Ltq_max     * mix;
    TMN            = p[0].TMN         * (1-mix) + p[1].TMN         * mix;
    NMT            = p[0].NMT         * (1-mix) + p[1].NMT         * mix;
    minSMR         = p[0].minSMR        ;
    Bandwidth      = p[0].BandWidth   * (1-mix) + p[1].BandWidth   * mix;
    tmpMask_used   = p[0].tmpMask_used  ;
    CVD_used       = p[0].CVD_used      ;
    varLtq         = p[0].varLtq      * (1-mix) + p[1].varLtq      * mix;
    MS_Channelmode = p[0].MS_Channelmode;
    CombPenalities = p[0].CombPenalities;
    Max_ANS_Order  = p[0].Max_ANS_Order ;
    PNS            = p[0].PNS         * (1-mix) + p[1].PNS         * mix;
    TransDetect1   = p[0].TransDetect1* (1-mix) + p[1].TransDetect1* mix;
    TransDetect2   = p[0].TransDetect2* (1-mix) + p[1].TransDetect2* mix;
    TransDetect3   = p[0].TransDetect3* (1-mix) + p[1].TransDetect3* mix;
    TransDetect4   = p[0].TransDetect4* (1-mix) + p[1].TransDetect4* mix;
}
