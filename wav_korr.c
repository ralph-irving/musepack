/*
 *   nasm -f elf wav_korr_asm.asm; gcc -O2 -s -DMPP_ENCODER -o wav_korr wav_korr.c wav_korr_asm.o -lm
 *      or:
 *   gcc -DNONASM -O2 -s -DMPP_ENCODER -o wav_korr wav_korr.c -lm
 */

#include "mppenc.h"


typedef signed short stereo [2];
typedef signed short mono;
typedef struct {
    unsigned long long  n;
    long double         x;
    long double         x2;
    long double         y;
    long double         y2;
    long double         xy;
} korr_t;


#if defined NONASM

void
analyze_stereo ( const stereo* p, size_t len, korr_t* k )
{
    long double  _x = 0, _x2 = 0, _y = 0, _y2 = 0, _xy = 0;
    double       t1;
    double       t2;

    k -> n  += len;

    for ( ; len--; p++ ) {
        _x  += (t1 = (*p)[0]); _x2 += t1 * t1;
        _y  += (t2 = (*p)[1]); _y2 += t2 * t2;
                               _xy += t1 * t2;
    }

    k -> x  += _x ;
    k -> x2 += _x2;
    k -> y  += _y ;
    k -> y2 += _y2;
    k -> xy += _xy;
}


void
analyze_dstereo ( const stereo* p, size_t len, korr_t* k )
{
    static double l0 = 0;
    static double l1 = 0;
    long double   _x = 0, _x2 = 0, _y = 0, _y2 = 0, _xy = 0;
    double        t1;
    double        t2;

    k -> n  += len;

    for ( ; len--; p++ ) {
        _x  += (t1 = (*p)[0] - l0);  _x2 += t1 * t1;
        _y  += (t2 = (*p)[1] - l1);  _y2 += t2 * t2;
                                     _xy += t1 * t2;
    l0   = (*p)[0];
    l1   = (*p)[1];
    }

    k -> x  += _x ;
    k -> x2 += _x2;
    k -> y  += _y ;
    k -> y2 += _y2;
    k -> xy += _xy;
}


void
analyze_mono   ( const mono* p, size_t len, korr_t* k )
{
    long double   _x = 0, _x2 = 0;
    double        t1;

    k -> n  += len;

    for ( ; len--; p++ ) {
        _x  += (t1 = (*p)); _x2 += t1 * t1;
    }

    k -> x  += _x ;
    k -> x2 += _x2;
    k -> y  += _x ;
    k -> y2 += _x2;
    k -> xy += _x2;
}


void
analyze_dmono   ( const mono* p, size_t len, korr_t* k )
{
    static double l0 = 0;
    long double   _x = 0, _x2 = 0;
    double        t1;

    k -> n  += len;

    for ( ; len--; p++ ) {
        _x  += (t1 = (*p) - l0); _x2 += t1 * t1;
    l0   = *p;
    }

    k -> x  += _x ;
    k -> x2 += _x2;
    k -> y  += _x ;
    k -> y2 += _x2;
    k -> xy += _x2;
}

#else

extern void  __analyze_stereo ( const stereo* p, size_t len, korr_t* dst );
extern void  __analyze_mono   ( const mono*   p, size_t len, korr_t* dst );

# define analyze_stereo(ptr,len,k)  __analyze_stereo ( ptr, len, k )
# define analyze_mono(ptr,len,k)    __analyze_mono   ( ptr, len, k )

#endif


static const char*
type ( double r, int channels )
{
    if ( channels==1 ) return "1 channel";
    if ( r > +0.98   ) return "Mono";
    if ( r > +0.91   ) return "?";
    if ( r > +0.3025 ) return "MS-Stereo";
    if ( r > -0.3025 ) return "Stereo";
    return "???";
}


static void
report_init ( void )
{
    printf ( " x[AC]     y[AC]     r         type        x[DC]     y[DC]   sy/sx  File\n");
}


static int
sgn ( long double x )
{
    if ( x > 0 ) return +1;
    if ( x < 0 ) return -1;
    return 0;
}


static char
mono_or_stereo ( korr_t* k )
{
    long double  scale = sqrt ( 1.e5 / (1<<29) ); // Sine Full Scale is +10 dB, 7327 = 100%
    long double  r;
    long double  rd;
    long double  sx;
    long double  sy;
    long double  x;
    long double  y;
    long double  b;
    static char  last = 'm';

    r  = (k->x2*k->n - k->x*k->x) * (k->y2*k->n - k->y*k->y);
    r  = r  > 0.l  ?  (k->xy*k->n - k->x*k->y) / sqrt (r)  :  1.l;
    sx = k->n > 1  ?  sqrt ( (k->x2 - k->x*k->x/k->n) / (k->n - 1) )  :  0.l;
    sy = k->n > 1  ?  sqrt ( (k->y2 - k->y*k->y/k->n) / (k->n - 1) )  :  0.l;
    x  = k->n > 0  ?  k->x/k->n  :  0.l;
    y  = k->n > 0  ?  k->y/k->n  :  0.l;

    b  = atan2 ( sy, sx * sgn (r) ) * ( 8 / M_PI);

//    6       5        4        3        2
//      _______________________________
//      |\             |             /|
//    7 |   \          |          /   |  1
//      |      \       |       /      |
//      |         \    |    /         |
//      |            \ | /            |
//    8 |--------------+--------------|  0
//      |            / | \            |
//      |         /    |    \         |
//   -7 |      /       |       \      | -1
//      |   /          |          \   |
//      |/_____________|_____________\|
//
//   -6       -5      -4      -3        -2

#define P1  0.500
#define P2  0.125
#define P3  1.000

    rd = P2 + (P3-P2) * (1 - r*r);


    switch (last) {
    case 'm': if (r > 0.99  &&  fabs (b-2) < 0.02   ) return last = 'm';
              if (r > 0.99                          ) return last = 'i';
              if ( fabs(b-2) < P1   ||  fabs(b-6) < P1 ) return last = 'j';
              return 's';
    case 'i': if (r > 0.999  &&  fabs (b-2) < 0.002 ) return last = 'm';
              if (r > 0.99                          ) return last = 'i';
              if ( fabs(b-2) < P1   ||  fabs(b-6) < P1 ) return last = 'j';
              return 's';
    case 'j': if (r > 0.999  &&  fabs (b-2) < 0.002 ) return last = 'm';
              if (r > 0.999                         ) return last = 'i';
              if ( fabs(b-2) < P1 || fabs(b-6) < P1 ) return last = 'j';
              return 's';
    case 's': if (r > 0.999  &&  fabs (b-2) < 0.002 ) return last = 'm';
              if (r > 0.999                         ) return last = 'i';
              if ( fabs(b-2) < P1-rd || fabs(b-6) < P1-rd) return last = 'j';
              return 's';
    }
    fprintf (stderr, "Internal error\n");
}


static void
report ( int channels, korr_t* k, int supress_DC_report )
{
    long double  scale = sqrt ( 1.e5 / (1<<29) ); // Sine Full Scale is +10 dB, 7327 = 100%
    long double  r;
    long double  sx;
    long double  sy;
    long double  x;
    long double  y;
    long double  b;

    // printf ("n=%Lu (7036596)\nx=%Lf (-1136345)\ny=%Lf (-783749)\nxy=%Lf (103252784558988)\nx²=%Lf (182857029624921)\ny²=%Lf (201045032621475)\n\n",k.n,k.x,k.y,k.xy,k.x2,k.y2);

    r  = (k->x2*k->n - k->x*k->x) * (k->y2*k->n - k->y*k->y);
    r  = r  > 0.l  ?  (k->xy*k->n - k->x*k->y) / sqrt (r)  :  1.l;
    sx = k->n > 1  ?  sqrt ( (k->x2 - k->x*k->x/k->n) / (k->n - 1) )  :  0.l;
    sy = k->n > 1  ?  sqrt ( (k->y2 - k->y*k->y/k->n) / (k->n - 1) )  :  0.l;
    x  = k->n > 0  ?  k->x/k->n  :  0.l;
    y  = k->n > 0  ?  k->y/k->n  :  0.l;

    b  = sx != 0   ?  sy/sx * sgn(r)  :  0.l;

    printf ( "%7.3Lf%%%9.3Lf%%%9.3Lf%%   %-9s",
             sx * scale, sy * scale, 100. * r, type (r, channels) );
    if ( supress_DC_report )
        printf ( "                  " );
    else
        printf ( "%7.3Lf%%%9.3Lf%%%", x * scale, y * scale );
    printf ( "  %6.3Lf\n", b );
    fflush ( stdout );
}


static void
readfile ( const char* name, wave_t* t )
{
    stereo          s [1152];
    mono            m [1152];
    size_t          samples;
    korr_t          k0;
    korr_t          k1;

    memset ( &k0, 0, sizeof(k0) );
    memset ( &k1, 0, sizeof(k1) );

    switch ( t->Channels ) {
    case 1:
        printf ("\n%s\n", name);
        while  ( ( samples = fread (m, 1, sizeof m, t->fp) ) > 0 ) {
            analyze_mono    ( m, samples / sizeof (*m), &k0 );
            analyze_dmono   ( m, samples / sizeof (*m), &k1 );
    }
        report ( t->Channels, &k0, 0 );
        report ( t->Channels, &k1, 1 );
        break;

    case 2:
        printf ("\n%s\n", name);
        while  ( ( samples = fread ( s, 1, sizeof s, t->fp) ) > 0 ) {
            analyze_stereo  ( s, samples / sizeof (*s), &k0 );
            analyze_dstereo ( s, samples / sizeof (*s), &k1 );
    }
        report ( t->Channels, &k0, 0 );
        report ( t->Channels, &k1, 1 );
        break;

    default:
        fprintf ( stderr, "%u Channels not supported: %s\n", t->Channels, name );
        break;
    }

    fflush ( stdout);
    return;
}


int
main ( int argc, char** argv )
{
    char*   name;
    FILE*   fp;
    wave_t  t;

    report_init ();

    if ( argc < 2 ) {
        name = "-";
    if ( Open_WAV_Header ( &t, name ) >= 0 ) {
            readfile ( name, &t );
        fclose ( t.fp );
    } else {
        fprintf ( stderr, "Can't open: %s\n", name );
    }
    }
    else {
        while ( (name = *++argv) != NULL ) {
        if ( Open_WAV_Header ( &t, name ) >= 0 ) {
                readfile ( name, &t );
        fclose ( t.fp );
        } else {
            fprintf ( stderr, "Can't open: %s\n", name );
        }
        }
    }

    return 0;
}

/* end of wav_korr.c */
