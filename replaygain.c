/*
 *   Should also set the ClipPrev Header by means of decoding
 *   Should clipprev automatically activated for ReplayGain != 0.0 dB ?
 *
 *   TODO:
 *     - better filter with more bass (bass is currently understimated)
 *     - clear up gain_analysis
 *     - mppdec: also blends between 0, 1 and 2
 *     - Ausgabe der album-Werte bei --list und --listdB nur, wenn alle identisch sind!
 *     - ID3v2 Tags müssen überlesen werden
 * Unterdrücken können der automatischen Albumeinteilung  ?????
 * Fehler mit /[] bei list oder listall ??? auto geht aber ??????
 *
 * --smart macht Unsinn
 *
 * Ausgabe der Dynamik des Stückes
 * Sonderbehandlung für kurze Stücke < 30 Sekunden (Bestimmen aus den Werten der Nachbarstücke)
 * Titel ohne Pause/Silence zusammenfassen
 * Andere Albumbenennung zulassen
 *
 * madplay -d -o wav:- -a -6.0205999132796239042 -q dateiname > 11.wav
 */

// Bugs: Assume fs=44.1 kHz, which is not true anymore for MPC

#define VERSION "0.84"

#include <ctype.h>
#include <math.h>
#include "mppdec.h"
#include "gain_analysis.h"

#define LEVEL_THR       0.f

int dBp     = 0;
int smart   = 0;
int Kreport = 0;

typedef struct {
    const char*   FileName;
    float         TitleGain;
    float         AlbumGain;
    Uint32_t      TitlePeak;
    Uint32_t      AlbumPeak;
    unsigned int  Silence;
} gain_info_t;


#define AUTO      (Int32_t)0x80000000
#define LIST      (Int32_t)0x80000001
#define LISTALL   (Int32_t)0x80000002

#if defined HAVE_INCOMPLETE_READ  &&  FILEIO != 1

size_t
complete_read ( int fd, void* dest, size_t bytes )
{
    size_t  bytesread = 0;
    size_t  ret;

    while ( bytes > 0 ) {
        ret = read ( fd, dest, bytes );
        if ( ret == 0  ||  ret == (size_t)-1 )
            break;
        dest       = (void*)(((char*)dest) + ret);
        bytes     -= ret;
        bytesread += ret;
    }
    return bytesread;
}

#endif


static Float_t
OverSample ( Int16_t* src, size_t len )
{
    size_t  i;
    float   S;
    float   max = 0.;

#if 1
    // Legato Link
    for ( i = 3; i < len-4; i++ ) {
        S = + 0.597025167f    * (src[i  ] + src[i+1])
            - 0.117586444f    * (src[i-1] + src[i+2])
            + 0.0228418825f   * (src[i-2] + src[i+3])
            - 0.002280605625f * (src[i-3] + src[i+4]);
        S = fabs (S);
        if ( S > max )
            max = S;
    }
#else

/*
constant gain factor     3.4736680887566E-02
z plane Denominator      Numerator
 0   1.000000000E+00   3.473668089E-02
 1  -9.080904743E-01   1.516197951E-01
 2   1.325659333E+00   2.841012960E-01
 3  -7.693859648E-01   2.841012960E-01
 4   3.797256375E-01   1.516197951E-01
 5  -8.699298700E-02   3.473668089E-02

---------------------------------------------
constant gain factor     1.1780713261043E-02
z plane Denominator      Numerator
 0   1.000000000E+00   1.178071326E-02
 1  -1.802715403E+00   5.120893530E-02
 2   3.025911944E+00   1.163859288E-01
 3  -2.988454267E+00   1.697423622E-01
 4   2.358550858E+00   1.697423622E-01
 5  -1.273440642E+00   1.163859288E-01
 6   4.710392622E-01   5.120893530E-02
 7  -9.265587323E-02   1.178071326E-02
 */

/*
    0.0000000000, -0.0004061767, -0.0006554074, -0.0005603479,
    0.0000000000, +0.0010332072, +0.0024067306, +0.0038511274, +0.0049972718, +0.0054446460, +0.0048504364, +0.0030230276,
    0.0000000000, -0.0039090311, -0.0081300062, -0.0118895363, -0.0143338396, -0.0146862233, -0.0124177060, -0.0073988790,
    0.0000000000, +0.0088885044, +0.0179316783, +0.0255223974, +0.0300328067, +0.0301098647, +0.0249669378, +0.0146174761,
    0.0000000000, -0.0170435762, -0.0339530545, -0.0477884869, -0.0556838859, -0.0553529653, -0.0455668957, -0.0265189851,
    0.0000000000, +0.0306695549, +0.0609699140, +0.0857542392, +0.1000000000, +0.0996415654, +0.0823628662, +0.0482228773,
    0.0000000000, -0.0568263967, -0.1144916118, -0.1637363938, -0.1948936008, -0.1991420526, -0.1697652726, -0.1032409730,
    0.0000000000, +0.1352696593, +0.2938416526, +0.4636919170, +0.6306889405, +0.7800976241, +0.8982140690, +0.9739261298,
    1.0000000000, +0.9739261298, +0.8982140690, +0.7800976241, +0.6306889405, +0.4636919170, +0.2938416526, +0.1352696593,
    0.0000000000, -0.1032409730, -0.1697652726, -0.1991420526, -0.1948936008, -0.1637363938, -0.1144916118, -0.0568263967,
    0.0000000000, +0.0482228773, +0.0823628662, +0.0996415654, +0.1000000000, +0.0857542392, +0.0609699140, +0.0306695549,
    0.0000000000, -0.0265189851, -0.0455668957, -0.0553529653, -0.0556838859, -0.0477884869, -0.0339530545, -0.0170435762,
    0.0000000000, +0.0146174761, +0.0249669378, +0.0301098647, +0.0300328067, +0.0255223974, +0.0179316783, +0.0088885044,
    0.0000000000, -0.0073988790, -0.0124177060, -0.0146862233, -0.0143338396, -0.0118895363, -0.0081300062, -0.0039090311,
    0.0000000000, +0.0030230276, +0.0048504364, +0.0054446460, +0.0049972718, +0.0038511274, +0.0024067306, +0.0010332072,
    0.0000000000, -0.0005603479, -0.0006554074, -0.0004061767,
    0.0000000000,

 */

    for ( i = 7; i < len-8; i++ ) {
        S = + 0.50000f * (src[i  ] + src[i+1])
            - 0.00000f * (src[i-1] + src[i+2])
            + 0.00000f * (src[i-2] + src[i+3])
            - 0.00000f * (src[i-3] + src[i+4])
            + 0.00000f * (src[i-4] + src[i+5])
            - 0.00000f * (src[i-5] + src[i+6])
            + 0.00000f * (src[i-6] + src[i+7])
            - 0.00000f * (src[i-7] + src[i+8]);
        S = fabs (S);
        if ( S > max )
            max = S;
    }
#endif
    return S;
}

static Int32_t
ReadReplayGain ( const char* p )
{
    Int32_t  ret =  0;
    int      sgn = +1;

    if ( 0 == strcmp ( p, "--auto") )
        return AUTO;
    if ( 0 == strcmp ( p, "--list") )
        return LIST;
    if ( 0 == strcmp ( p, "--listall") )
        return LISTALL;
    if ( 0 == strcmp ( p, "--listreport") )
        return Kreport = 1, LIST;
    if ( 0 == strcmp ( p, "--listallreport") )
        return Kreport = 1, LISTALL;
    if ( 0 == strcmp ( p, "--autodB") )
        return dBp = 1, AUTO;
    if ( 0 == strcmp ( p, "--listdB") )
        return dBp = 1, LIST;
    if ( 0 == strcmp ( p, "--listalldB") )
        return dBp = 1, LISTALL;
    if ( 0 == strcmp ( p, "--listreportdB") )
        return dBp = 1, Kreport = 1, LIST;
    if ( 0 == strcmp ( p, "--listallreportdB") )
        return dBp = 1, Kreport = 1, LISTALL;

    if (*p == '+')
        p++;
    else if (*p == '-')
        sgn = -sgn, p++;

    while ( (unsigned int)(*p - '0') < 10u )
        ret = 10 * ret + (unsigned int)(*p++ - '0');

    ret *= 100;
    switch (*p) {
    case '.':
        if ( p[1] == '\0' )
            break;
        else if ( (unsigned int)(p[1] - '0') < 10u )
            ret += (p[1] - '0') * 10;
        else
            goto error;

        if ( p[2] == '\0' )
            break;
        else if ( (unsigned int)(p[2] - '0') < 10u )
            ret += (p[2] - '0');
        else
            goto error;

        if ( p[3] == '\0' )
            break;
        else if ( (unsigned int)(p[3] - '0') < 10u )
            ret += (p[2] >= '5');
        else
            goto error;
        break;

    default:
    error:
        stderr_printf ("Illegal string in number: %s\n", p );
        exit (1);

    case '\0':
        break;
    }

    return ret * sgn;
}


void sh ( const char* name, float level)
{
#if 0
    FILE* fp = fopen ("/tmp/silence", "a+");
    if ( fp == NULL ) {
        fprintf (stderr, "Can't append on '/tmp/silence'\n");
        return;
    }
    if (name)
        fprintf (fp, "%8.2f  %s\n", level, name );
    else
        fprintf (fp, "%8.2f ", level );
    fclose (fp);
#endif
}

#define NO   (size_t)(44100 * 0.05)

static void
CalcReplayGain ( const char* filename, gain_info_t* G )
{
    FILE*    fp;
    float    buffl [NO];
    float    buffr [NO];
    Int16_t  buff  [NO] [2];
    size_t   i;
    size_t   len;
    size_t   lastlen = 0;
    unsigned int max = 0;
    float    level;
    float    mult;

    if ((fp = pipeopen ( "mppdec --silent --scale 0.5 --gain 0 --raw - - < #", filename)) == NULL) {
        stderr_printf ( "Can't decode '%s'\n", filename );
        exit (9);
    }

    memset ( buff, 0, sizeof(buff) );
    G->Silence = 0;

    lastlen = len = fread (buff, 4, NO, fp);
    for ( i = 0; i < len; i++ ) {
        buffl [i] = 2. * buff [i] [0];
        buffr [i] = 2. * buff [i] [1];
        if ( abs(buff[i][0]) > max ) max = abs(buff[i][0]);
        if ( abs(buff[i][1]) > max ) max = abs(buff[i][1]);
    }
    AnalyzeSamples ( buffl, buffr, len, 2 );

    level = 0.;
    mult  = 1.;
    for ( i = 0; i < len; i++ ) {
        level += mult * (buff [i] [0] * buff [i] [0] + buff [i] [1] * buff [i] [1]);
        mult  *= 0.95;
    }
    level = 2*sqrt(level * 0.05);
    if ( level > LEVEL_THR )
        G->Silence |= 2;

    sh ( NULL, level );

    while (( len = fread (buff, 4, NO, fp) ) > 0 ) {
        lastlen = len;
        for ( i = 0; i < len; i++ ) {
            buffl [i] = 2. * buff [i] [0];
            buffr [i] = 2. * buff [i] [1];
            if ( abs(buff[i][0]) > max ) max = abs(buff[i][0]);
            if ( abs(buff[i][1]) > max ) max = abs(buff[i][1]);
        }
        AnalyzeSamples ( buffl, buffr, len, 2 );
    }

    level = 0.;
    mult  = 1.;
    for ( i = 1; i <= NO; i++ ) {
        int  idx = (lastlen + NO - i) % NO;
        level += mult * (buff [idx] [0] * buff [idx] [0] + buff [idx] [1] * buff [idx] [1]);
        mult  *= 0.95;
    }
    level = 2*sqrt(level * 0.05);
    if ( level > LEVEL_THR )
        G->Silence |= 1;

    sh(filename,level);

    PCLOSE (fp);
#if 0
    GetTitleDynamics ();
#endif
    G -> FileName  = filename;
    G -> TitleGain = GetTitleGain ();
    G -> TitlePeak = 2 * max + 1;
    G -> AlbumGain = GetAlbumGain ();
    G -> AlbumPeak = G->AlbumPeak < G->TitlePeak  ?  G->TitlePeak  :  G->AlbumPeak;
    return;
}


int
ModifyFile ( const gain_info_t* G, unsigned int mask )
{
    unsigned char  buff [20];
    Int32_t        val;
    FILE_T         fd = OPENRW ( G -> FileName );

    if ( fd == INVALID_FILEDESC ) {
        stderr_printf ("Can't patch '%s'\n", G -> FileName );
        return 1;
    }
    if ( READ ( fd, buff, sizeof(buff) ) != sizeof(buff) ) {
        stderr_printf ("Can't read header of '%s'\n", G -> FileName );
        return 2;
    }
    if ( 0 != memcmp ( buff, "MP+", 3)  ||  (buff[3] & 15) != 7 ) {
        stderr_printf ("Not a MPC file SV7: '%s'\n", G -> FileName );
        return 3;
    }

    if ( mask & 1 ) {   // Title Peak level
        val       = G -> TitlePeak;
        buff [12] = (Uint8_t)(val >> 0);
        buff [13] = (Uint8_t)(val >> 8);
        val       = (Int32_t)(G -> TitlePeak / 1.18);
        buff [ 8] = (Uint8_t)(val >> 0);
        buff [ 9] = (Uint8_t)(val >> 8);
    }
    if ( mask & 2 ) {   // Album Peak level
        val       = G -> AlbumPeak;
        buff [16] = (Uint8_t)(val >> 0);
        buff [17] = (Uint8_t)(val >> 8);
    }
    if ( mask & 4 ) {   // Title RMS
        val       = 100. * G -> TitleGain;
        buff [14] = (Uint8_t)(val >> 0);
        buff [15] = (Uint8_t)(val >> 8);
    }
    if ( mask & 8 ) {   // Album RMS
        val       = 100. * G -> AlbumGain;
        buff [18] = (Uint8_t)(val >> 0);
        buff [19] = (Uint8_t)(val >> 8);
    }

    if ( SEEK  ( fd, 0L, SEEK_SET ) < 0 ) {
        stderr_printf ("Seek error in '%s'\n", G -> FileName );
        return 4;
    }
    if ( WRITE ( fd, buff, sizeof(buff) ) != sizeof(buff) ) {
        stderr_printf ("Can't write data in '%s'\n", G -> FileName );
        return 5;
    }
    if ( CLOSE (fd) < 0 ) {
        stderr_printf ("Error closing '%s'\n", G -> FileName );
        return 6;
    }
    return 0;
}


char* dB ( double val )
{
    static unsigned int x = 0;
    static char buff[8][12];

    x = (x+1) & 7;

    if (dBp) {
        double dB = 20. * log10 (val/32767.);
        if (dB <= -60)
            return "     ";
        sprintf ( buff[x], fabs(dB) < 9.995 ? "%+5.2f" : "%+5.1f", dB  );
    }
    else {
        sprintf ( buff[x], "%5u", (int)val );
    }
    return buff[x];
}


// " -- [xx] "          9
// " -- [xx]."          9
// "/[xx] -- "          9
// "/[xx] --."          9
// " -- xx -- "        10
// " -- xx --."        10
// "/xx -- "            7
// "/xx --."            7

static size_t
AlbumNameLen ( const char* filename )
{
    const char*  p = filename + strlen (filename) - 9;

    while ( p >= filename ) {
        if ( 0 == memcmp ( p, " -- [", 5)  &&  isdigit (p[5])  &&  isdigit(p[6])  &&  p[7] == ']'  &&  (p[8]=='.' || p[8]==' ') )
            return p - filename;
        if ( (p[0]=='/' || p[0]=='\\')  &&  p[1] == '[' &&  isdigit (p[2])  &&  isdigit(p[3])  &&  0 == memcmp (p+4, "] --", 4)  &&  (p[8]=='.' || p[8]==' ') )
            return p - filename;
        if ( 0 == memcmp ( p, " -- ", 4)  &&  isdigit (p[4])  &&  isdigit(p[5])  &&  0 == memcmp (p+6, " --", 3)  &&  (p[9]=='.' || p[9]==' ') )
            return p - filename;
        if ( (p[0]=='/' || p[0]=='\\')  &&  isdigit (p[1])  &&  isdigit(p[2])  &&  0 == memcmp (p+3, " --", 3)  &&  (p[6]=='.' || p[6]==' ') )
            return p - filename;
        p--;
    }

    return 0;
}


int Cdecl
main ( int argc, char** argv )
{
    static const char*
                 extentions [] = { ".mpc", ".mpp", ".mp+", NULL };
    Int32_t      mode;
    Int32_t      title_gain;
    Int32_t      title_peak;
    Int32_t      album_gain;
    Int32_t      album_peak;
    double       title_peak_max     = 0.;
    double       title_peak_adj_max = 0.;
    double       album_peak_max     = 0.;
    double       album_peak_adj_max = 0.;
    FILE_T       fd;
    Uint8_t      buff [20];
    const char*  name;
    gain_info_t  Gain;
    int          i;
    int          ilast;

#ifdef USE_ARGV
    mysetargv ( &argc, &argv, extentions );
#endif

    stderr_printf ( "Replaygain " VERSION "     (C) 2001-2002 Klemm/Robinson/Sawyer\n\n" );

    if ( argv[1] != NULL  &&  0 == strncmp (argv[1], "--9", 3) )
        SetPercentile (0.01 * atoi (*++argv + 2) );

    if ( argv[1] != NULL  &&  0 == strcmp (argv[1], "--smart" ) )
        smart++, argv++;

    memset ( &Gain, 0, sizeof(Gain) );
    InitGainAnalysis  ( CD_SAMPLE_FREQ );

    if ( argc < 3 ) {
        stderr_printf ("usage: ReplayGain level MPC_Title_01 [MPC_Title_02 MPC_Title_03 ...]\n"
                       "\n"
                       "percentile can be:\n"
                       "  --92 | --93 | --94 | --95 | --96 | --97 | --98\n"
                       "\n"
                       "level can be:\n"
                       "  * value in the range -300 dB...+300 dB, set as title based replay gain\n"
                       "  * --auto   | --autodB    : auto determine gains and peak values\n"
                       "  * --list   | --listdB    : list title based values\n"
                       "  * --listall| --listalldB : list title and album based values\n"
                       "  * --listreport           : list title based values + K suggestion\n"
                       "  * --listallreport        : list title and album based values + K suggestion\n" );
        return 1;
    }

    argv++;
    mode = ReadReplayGain (*argv);
    if ( mode != AUTO  &&  mode != LIST  &&  mode != LISTALL ) {
        if ( mode != (Int16_t) mode ) {
            stderr_printf ("level can only be in the range -327.68 dB...+327.67 dB\n" );
            return 2;
        }
        Gain.TitleGain = 0.01 * mode;
    }

    switch (mode) {
    case AUTO:
    default:
        printf ("   Level Adjustment   |   Peak Level   (Adjst)|  Filename\n"
                "----------------------+-----------------------+-------------------------------\n");
        break;
    case LIST:
        printf ("  Level-  |       (Peak+)|\n"
                "Adjustment|  Peak (Adjst)|  Filename\n"
                "----------+--------------+-----------------------------------------------------\n");
        break;
    case LISTALL:
        printf ("        Title            |        Album            |\n"
                "  Level-  |       (Peak+)|  Level-  |       (Peak+)|\n"
                "Adjustment|  Peak (Adjst)|Adjustment|  Peak (Adjst)|  Filename\n"
                "----------+--------------+----------+--------------+---------------------------\n");
        break;
    }
    argv++;

repeat:
    ilast = -1;
    for ( i = 0; argv[i]  &&  (mode != AUTO || (AlbumNameLen (argv[0])==AlbumNameLen (argv[i])  &&  0 == memcmp (argv[0], argv[i], AlbumNameLen (argv[0])))); i++ ) {
        ilast = i;
        name = argv [i];
        Gain.FileName = name;
        if ( mode == AUTO ) {
            if (smart) {
                fd = OPEN (name);
                if ( fd == INVALID_FILEDESC ) {
                    stderr_printf ("Can't open: %s\n", name );
                    continue;
                }
                if ( READ ( fd, buff, sizeof(buff) ) != sizeof(buff) ) {
                    stderr_printf ("Can't read header: %s\n", name );
                    continue;
                }
                if ( CLOSE (fd) < 0 ) {
                    stderr_printf ("Error closing file: %s\n", name );
                    continue;
                }
                if ( buff[12] || buff[13] || buff[14] || buff[15] || buff[16] || buff[17] || buff[18] || buff[19])
                    continue;
            }
            if ( ISATTY (FILENO(STDOUT)) ) {
                printf ("%s\r", name );
                FLUSH (stdout);
                fflush (stdout);
            }
            fd = OPENRW (name);
            if ( fd == INVALID_FILEDESC ) {
                stderr_printf ("Can't patch '%s'\n", name );
                return 1;
            }
            if ( CLOSE (fd) < 0 ) {
                stderr_printf ("Error closing '%s'\n", name );
                return 6;
            }
            CalcReplayGain ( name, &Gain );
            if ( Gain.TitleGain > +36. )
                Gain.TitleGain = 0.;
            if ( Gain.AlbumGain > +24. )
                Gain.AlbumGain = 0.;
        }
        fd = OPEN (name);
        if ( fd == INVALID_FILEDESC ) {
            stderr_printf ("Can't open: %s\n", name );
            continue;
        }
        if ( READ ( fd, buff, sizeof(buff) ) != sizeof(buff) ) {
            stderr_printf ("Can't read header: %s\n", name );
            continue;
        }
        if ( CLOSE (fd) < 0 ) {
            stderr_printf ("Error closing file: %s\n", name );
            continue;
        }
        if ( 0 != memcmp ( buff, "MP+", 3)  ||  (buff[3] & 15) != 7 ) {
            stderr_printf ("Not a MPC file SV7: '%s'\n", name );
            continue;
        }

        title_gain = (Uint8_t)buff[14] + 256 * (Int8_t) buff[15];
        album_gain = (Uint8_t)buff[18] + 256 * (Int8_t) buff[19];
        title_peak = (Uint8_t)buff[12] + 256 * (Uint8_t)buff[13];
        album_peak = (Uint8_t)buff[16] + 256 * (Uint8_t)buff[17];

        switch (mode) {
        case LIST:
            printf ("%+6.2f dB | %s (%s)| %s\n",
                    0.01 * title_gain, dB(title_peak), dB(title_peak * pow (10., title_gain/2000.)),
                    name );
            if ( argv[i+1] == NULL  ||  0 != memcmp (argv[i], argv[i+1], AlbumNameLen (argv[i])) )  {
                int len = AlbumNameLen (argv[i]);
                printf ("%+6.2f dB | %s (%s)| %*.*s\n"
                        "----------+--------------+-----------------------------------------------------\n",
                    0.01 * album_gain, dB(album_peak), dB(album_peak * pow (10., album_gain/2000.)),
                    len, len, argv[i] );
            }
            break;
        case LISTALL:
            printf ("%+6.2f dB | %s (%s)|%+6.2f dB | %s (%s)| %s\n",
                    0.01 * title_gain, dB(title_peak), dB(title_peak * pow (10., title_gain/2000.)),
                    0.01 * album_gain, dB(album_peak), dB(album_peak * pow (10., album_gain/2000.)),
                    name );
            break;
        default:
            Gain.TitlePeak = title_peak;
        case AUTO:
            ModifyFile ( &Gain, mode==AUTO  ?  4|1  :  4);
            printf ("%+6.2f dB =>%+6.2f dB | %s => %s (%s)| %s\n",
                    0.01 * title_gain, Gain.TitleGain,
                    dB(title_peak), dB(Gain.TitlePeak),
                    dB(Gain.TitlePeak * pow (10., Gain.TitleGain/20.)),
                    name );
            break;
        }
        fflush (stdout);

        if (mode == LIST || mode == LISTALL) {
            if (title_peak > title_peak_max) title_peak_max = title_peak;
            if (album_peak > album_peak_max) album_peak_max = album_peak;
            if (title_peak * pow (10., title_gain/2000.) > title_peak_adj_max) title_peak_adj_max = title_peak * pow (10., title_gain/2000.);
            if (album_peak * pow (10., album_gain/2000.) > album_peak_adj_max) album_peak_adj_max = album_peak * pow (10., album_gain/2000.);
        }

    }

    if ( mode == AUTO ) {
        int len = AlbumNameLen (argv[0]);
        printf ("          =>%+6.2f dB |       => %s (%s)| %*.*s\n",
                Gain.AlbumGain,
                dB(Gain.AlbumPeak),
                dB(Gain.AlbumPeak * pow (10., Gain.AlbumGain/20.)), len, len, *argv );
        fflush (stdout);
        for ( i = 0; i <= ilast; i++ ) {
            name = argv [i];
            Gain.FileName = name;
            ModifyFile ( &Gain, 2|8 );
        }
        argv += ilast + 1;
        if ( argv[0] != NULL ) {
            printf ( "----------------------+-----------------------+-------------------------------\n");
            memset ( &Gain, 0, sizeof(Gain) );
            InitGainAnalysis  ( CD_SAMPLE_FREQ );
            goto repeat;
        }
    }

    switch (mode) {
    case LIST:
        printf ("          | %s (%s)| %s\n",
                dB(title_peak_max), dB(title_peak_adj_max),
                "--- maximum (title based) ---" );
        printf ("          | %s (%s)| %s\n",
                dB(album_peak_max), dB(album_peak_adj_max),
                "--- maximum (album based) ---" );
        break;
    case LISTALL:
        printf ("          | %s (%s)|          | %s (%s)| %s\n",
                dB(title_peak_max), dB(title_peak_adj_max),
                dB(album_peak_max), dB(album_peak_adj_max),
                "--- maximum ---" );
        break;
    }

    if ( Kreport ) {
        printf ("\n\nSuggested mode for\n");
        printf ("  title based replay gain:   K%+d\n", -14 - (int) ceil (20.*log10(title_peak_adj_max/32767.)) );
        printf ("  album based replay gain:   K%+d\n", -14 - (int) ceil (20.*log10(album_peak_adj_max/32767.)) );
        printf ("  title based clipping prev: K%+d\n", -14 - (int) ceil (20.*log10(title_peak_max    /32767.)) );
        printf ("  album based clipping prev: K%+d\n", -14 - (int) ceil (20.*log10(album_peak_max    /32767.)) );
        printf ("\nLast two setting only possible in plugins using K setting also for\nnon-replaygain modes.\n");
    }

    return 0;
}

/* end of replaygain.c */
