/* list, wp, wav_korr, replaygain */
/* RKAU + APE */

#include <stdio.h>
#include "mppdec.h"

int  html = 0;


const char*
color ( int bitrate )
{
    if ( bitrate <   8000 )
        return "<font color=\"#1111FF\">";
    if ( bitrate <  72000 )
        return "<font color=\"#33CCFF\">";
    if ( bitrate <= 320000 )
        return "";
    if ( bitrate < 1400000 )
        return "<font color=\"#FFD468\">";
    return "<font color=\"#FF1100\">";
}


const char*
colorend ( int bitrate )
{
    if ( bitrate <  72000  ||  bitrate > 320000 )
        return "</font>";
    return "";
}


void
report ( const char*  name,
         Int64_t      orglen,
         Int64_t      packlen,
         long double  duration,
         int          bits,
         int          chan,
         double       freq )
{
    static Int64_t      orgtotlen   = 0;
    static Int64_t      packtotlen  = 0;
    static long double  totduration = 0.;
    static unsigned int filecnt = 0;
    Int64_t             ms;
    double              tmp;
    const char*         html2;
    char                files [128];

    if ( name == NULL ) {
        name = files;
        sprintf ( files, "--- %u files ---", filecnt), orglen = orgtotlen, packlen = packtotlen, duration = totduration;
    }
    else {
        orgtotlen += orglen, packtotlen += packlen, totduration += duration;
    }

    ms = floor ( 1000 * duration + 0.5 );

    if ( html ) printf ("  <tr> <td align=\"right\">" );

    // Report of original length and compressed length in MByte (works up to 10^15 Byte)
    html2 = html ? "</td> <td align=\"right\">" : " ";

    if ( orglen < 99999999500  &&  packlen < 99999999500 )
        printf ("%5u.%03u%s%5u.%03u", (int)(orglen/1000000), (int)(orglen/1000%1000), html2, (int)(packlen/1000000), (int)(packlen/1000%1000) );
    else if ( orglen < 999999995000  &&  packlen < 999999995000 )
        printf ("%6u.%02u%s%6u.%02u", (int)(orglen/1000000), (int)(orglen/10000%100), html2, (int)(packlen/1000000), (int)(packlen/10000%100) );
    else if ( orglen < 9999999950000  &&  packlen < 9999999950000 )
        printf ("%7u.%01u%s%7u.%01u", (int)(orglen/1000000), (int)(orglen/100000%10), html2, (int)(packlen/1000000), (int)(packlen/100000%10) );
    else
        printf ("%9u%s%9u", (int)(orglen/1000000), html2, (int)(packlen/1000000) );

    if ( html ) printf ("</td> <td align=\"right\">" );

    // Compression ratio (if ratio < 10)
    if ( packlen > 0 ) {
        tmp = (double) orglen / packlen;
        if ( fabs (tmp - 1.) < 0.0005 )
            printf ("  1.0  ");
        else
            printf ( tmp < 9.9995  ?  "%7.3f"  :  "       ", tmp );
    }
    else {
        printf ("       ");
    }


    if ( html ) printf ("</td> <td align=\"right\">%s", color (packlen * 8 / duration) );

    // Bitrate (kbps) (works up to 100 Mbps)
    if ( duration > 0 ) {
        tmp = packlen * 0.008 / duration;
        printf ( tmp < 99.95  ?  tmp < 9.995  ?  "  %4.2f"  :  "  %4.1f"  :  " %5.0f", tmp );
    }
    else {
        printf ("      ");
    }

    if ( html ) printf ("%s</font></td> <td align=\"right\">", colorend (packlen * 8 / duration) );

    // Duration (works up to
    if      ( ms <  600000000 )
        printf (" %4u:%02u.%03u  ", (int)(ms/60000), (int)(ms/1000%60), (int)(ms%1000) );
    else if ( ms < 6000000000 )
        printf (" %5u:%02u.%02u  ", (int)(ms/60000), (int)(ms/1000%60), (int)(ms%1000/10) );
    else if ( ms < 60000000000 )
        printf (" %6u:%02u.%01u  ", (int)(ms/60000), (int)(ms/1000%60), (int)(ms%1000/100) );
    else
        printf (" %8u:%02u  ", (int)(ms/60000), (int)(ms/1000%60) );

    if ( html ) printf ("</td> <td align=\"right\"><tt>" );

    // technische Parameter
    if ( chan > 1  &&  bits > 0  )
        printf ("(%ux%2u", chan, bits );
    else if ( chan == 1  &&  bits > 0  )
        printf ("(  %2u", bits );
    else
        printf ("     ");

    // Abtastfrequenz
    if ( freq <= 0 )
        printf ("%c           ", chan>0  && bits > 0  ?  ')'  :  ' ' );
    else if ( freq / 1000 == (int)(freq/1000) )
        printf ("%5.0f kHz)  ", freq/1000. );
    else if ( freq / 100 == (int)(freq/100) )
        printf ("%5.1f kHz)  ", freq/1000. );
    else
        printf ("%6.0f Hz)  ", freq );

    if ( html ) printf ("</tt></td> <td align=\"left\">" );

    // Name
    printf ("%s", name );

    if ( html ) printf ("</td> </tr>" );

    printf ("\n" );
    fflush (stdout);
    filecnt ++;
}


#define EXT(x)  (0 == strcasecmp (ext+1, #x))


static int
analyse ( const char* name )
{
    const char*    ext = strrchr ( name, '.');
    FILE*          fp;
    unsigned char  buff [44];
    unsigned long  freq;
    unsigned long  len;             // Länge der Festplattendatei
    unsigned long  pcmlen;
    unsigned int   channels;
    unsigned int   bits;

    fprintf ( stderr, "\r%s    \b\b\b\b", name );

    pcmlen = -1;

    if ( ext == NULL ) {
        goto noext;

    }
    else if ( EXT(wav) ) { wave:
        fp = fopen ( name, "rb" );
    }
    else if ( EXT(raw)  ||  EXT(cdr)  ||  EXT(pcm) ) {
        fp = fopen ( name, "rb" );
        channels = 2;
        bits     = 16;
        freq     = 44100;
        goto skip;
    }
    else if ( EXT(pac)  ||  EXT(lpac)  ||  EXT(lpa) ) { lpac:
        fp = pipeopen ( "lpac -x -o #", name );
    }
    else if ( EXT(fla)  ||  EXT(flac) ) { flac:
        fp = pipeopen ( "flac -d -s -c - < #", name );
    }
    else if ( EXT(rka)  ||  EXT(rkau) ) { rkau:
        fp = pipeopen ( "rkau # -", name );
    }
    else if ( EXT(sz) ) {
        fp = pipeopen ( "szip -d < #", name );
    }
    else if ( EXT(sz2) ) { szip2:
        fp = pipeopen ( "szip2 -d < #", name );
        if ( fp == NULL )
            fp = pipeopen ( "szip -d < #", name );
    }
    else if ( EXT(ofr) ) { optimfrog:
        fp = pipeopen ( "optimfrog d # -", name );
    }
    else if ( EXT(ape)  ||  EXT(mac) ) { ape:
        fp = pipeopen ( "mac # - -d", name );
    }
    else if ( EXT(la) ) {
        fp = pipeopen ( "la -console #", name );
    }
    else if ( EXT(shn)  ||  EXT(shorten) ) { shorten:
        fp = pipeopen ( "shorten -x # -", name );           // Testen, ob okay !!!!
        if ( fp == NULL )
            fp = pipeopen ( "shortn32 -x # -", name );
    }
    else if ( EXT(mp3)  ||  EXT(mp1)  ||  EXT(mp2)  ||  EXT(mpt)  ||  EXT(mp3pro) ) {
        fp = pipeopen ( "madplay --output=wave:/dev/fd/1 # 2> /dev/null", name );
    }
    else if ( EXT(mpc)  ||  EXT(mp+)  ||  EXT(mpp) ) {
        fp = pipeopen ( "mppdec --silent --scale 0 # -", name );
    }
    else if ( EXT(ogg) ) {
        fp = pipeopen ( "ogg123 -q -d wav -f /dev/fd/1 #", name );
        pcmlen = -2;
    }
    else if ( EXT(mod) ) {
        fp = pipeopen ( "xmp -b16 -c -f44100 --stereo -o- #", name );
        channels = 2;
        bits     = 16;
        freq     = 44100;
        goto skip;
    }
    else if ( EXT(ac3) ) {
        fp = pipeopen ( "ac3dec #", name );
    }
    else if ( EXT(aac) ) {
        fp = pipeopen ( "faad -w # 2> /dev/null", name );
        pcmlen = -2;
    }
    else {
        char buff [512];
noext:
        fp = fopen ( name, "rb" );
        if ( fp == NULL )
            return 0;
        memset ( buff, 0, sizeof buff );
        fread ( buff, 1, sizeof buff, fp );
        fclose (fp);
        if ( 0 == memcmp (buff, "MAC ", 4)  &&  0 == memcmp (buff+40, "RIFF", 4) ) goto ape;
        if ( 0 == memcmp (buff, "fLaC", 4)                                       ) goto flac;
        if ( 0 == memcmp (buff, "*"   , 1)  &&  0 == memcmp (buff+ 1, "RIFF", 4) ) goto optimfrog;
        if ( 0 == memcmp (buff, "LPAC", 4)  &&  0 == memcmp (buff+14, "RIFF", 4) ) goto lpac;
        if ( 0 == memcmp (buff, "RKA7", 4)                                       ) goto rkau;
        if ( 0 == memcmp (buff, "ajkg", 4)                                       ) goto shorten;
        if ( 0 == memcmp (buff, "SZ\012\004", 4)                                 ) goto szip2;
        if ( 0 == memcmp (buff, "RIFF", 4)  &&  0 == memcmp (buff+44, "wvpk", 4) ) goto wavepack;
        if ( 0 == memcmp (buff, "RIFF", 4)                                       ) goto wave;

        wavepack:
        fp = NULL;
    }

    if ( fp == NULL )
        return 0;

    if ( sizeof(buff) != fread ( buff, 1, sizeof(buff), fp ) ) {
        PCLOSE (fp);
        return 0;
    }
    if ( pcmlen != -2 )
        pcmlen   = buff[40]+(buff[41]<<8)+(buff[42]<<16)+(buff[43]<<24);

    freq     = buff[24]+(buff[25]<<8)+(buff[26]<<16)+(buff[27]<<24);
    bits     = buff[34] + (buff[35]<<8);
    channels = buff[22] + (buff[23]<<8);

skip:
    if ( (unsigned long)pcmlen >= 0x7FFFFFFF  ||  pcmlen == 0 ) {
        int   tmp;
        char  buff [4096];

        pcmlen = 0;
        while ( (tmp = fread (buff, 1, sizeof(buff), fp)) > 0 )
            pcmlen += tmp;
    }
    PCLOSE (fp);

    if (memcmp (buff, "RIFF", 4) != 0 )
        return 0;

    fp = fopen ( name, "rb" );
    if ( fp == NULL )
        return 0;
    fseek ( fp, 0l, SEEK_END );
    len = ftell (fp);
    fclose (fp);

    if ( freq > 0  &&  bits > 0  &&  channels > 0 ) {
        fprintf ( stderr, "\r" );
        report ( name, pcmlen+44, len, pcmlen/channels/((bits+7)/8)/(double)freq, bits, channels, freq );
        return 1;
    }
    return 0;
}


int
main ( int argc, char** argv )
{
    static const char*  extentions [] = {
        ".mpc", ".mpp", ".mp+", ".mp1", ".mp2", ".mp3", ".mpg", ".mpeg", ".lqt", ".aac",
        ".wav", ".raw", ".cdr", ".lpac", ".lpa", ".pac", ".fla", ".flac",
        ".rka", ".rkau", ".sz", ".sz2", ".ofr", ".mac", ".ape", ".shn", ".ogg",
        ".mid", ".ac3", ".vqf", ".dts", ".sdds", ".mpv", ".mp3pro", ".wv", ".mod",
        ".la",
        NULL
    };

    if ( argv[1] != NULL  &&  0 == strcmp (argv[1], "--html") )
        html++, argv++, argc--;

    if ( !html ) {
        printf ( " PCM size File size  Ratio  kbps    Duration  Param Frequency  Name\n");
    }
    else {
        printf ( "<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\n"
                 "<html>\n"
                 "<head>\n"
                 "    <title>File List</title>\n"
                 "</head>\n\n"
                 "<body text=\"#FFFFFF\" bgcolor=\"#254E31\" link=\"#33CCFF\" vlink=\"#33CCFF\" alink=\"#FF0000\" background=\"img/back-2.gif\">\n\n" );

        printf ( "<table border=\"1\" bgcolor=\"20442B\">\n" );
        printf ( "  <tr> <td>PCM size</td><td>File size</td><td>Ratio</td><td>kbps</td><td>Duration</td><td>Param&nbsp;Frequency</td><td>Name</td> </tr>\n");
        printf ( "  <tr></tr>\n" );
    }
    fflush (stdout);

#if   defined _OS2
    _wildcard ( &argc, &argv );
#elif defined USE_ARGV
    mysetargv ( &argc, &argv, extentions );
#endif

    while ( *++argv )
        analyse (*argv);

    if ( html )
        printf ( "  <tr></tr>\n" );

    report ( NULL, 0, 0, 0., 0, 0, 0 );

    if (html) {
        printf ( "</table>\n\n</body>\n</html>\n" );
    }

    return 0;
}

/* end of list.c */
