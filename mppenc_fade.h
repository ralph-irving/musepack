/*
 *  Wünsche für das Faden (gerade benötigt für'n Kumpel):
 *
 *            _____________________
 *           /|                   |\
 *         /  |                   |  \
 *        /   |                   |   \
 *  ____/     |                   |     \______________
 *  |  |      |                   |      |  |
 *  |t1|  t2  |                   |  t4  |t5|
 *  |                 t3                    |
 *     |<-------------- M P C ------------->|
 *  |<-------------------- W A V E ------------------>|
 *
 *   t1: StartTime   (Standard: 0, positiv: vom Anfang der Datei, negativ: vom Ende der Datei)
 *   t2: FadeInTime  (Standard: 0, positiv: Fadetime)
 *   t3: EndTime     (Standard: 0, nichtpositiv: vom Ende der Datei, positiv: vom Anfang der Datei)
 *   t4: FadeOutTime (Standard: 0, positiv: Fadetime)
 *   t5: PostGapTime (Standard: 0, positiv: zusätzliche Silence)
 *
 * Der Beginn der Phase t4 kann weiterhin per Signal SIGINT ausgelöst werden.
 * Bei SIGTERM wird der aktuelle Frame zuendekodiert und dann beendet.
 *
 * Weitere Frage ist, ob man t1 nicht vor die Null rutschen lassen können sollte, gleiches mit t3+t5
 * (stückeübergreifendes Schneiden). Als gute Fadefunktion wird gerade die parametrisierte Bumpfunktion
 * an Unschuldigen getestet.
 */


static float  bump_exp   = 1.f;
static float  bump_start = 0.040790618517f;


static void
setbump ( double e )
{
    bump_exp   = e;
    bump_start = 1 - sqrt (1 - 1 / (1 - log(1.e-5) / e));
}


static double
bump ( double x )
{
    x = bump_start + x * (1. - bump_start);
    if ( x <= 0.) return 0.;
    if ( x >= 1.) return 1.;
    return exp ( (1. - 1./(x * (2. - x))) * bump_exp);
}


static void
Fading_In  ( Float         pcm [] [ANABUFFER],
             unsigned int  N,
             const float   fs,
             unsigned int  channels )
{
    float         inv_fs = 1.f / fs;
    float         fadein_pos;
    float         scale;
    int           n;
    unsigned int  ch;
    int           idx;

    for ( n = 0; n < BLOCK; n++, N++ ) {
        fadein_pos     = N * inv_fs;
        scale          = fadein_pos / FadeInTime;
        scale          = bump (scale);
        idx            = n + CENTER;
        for ( ch = 0; ch < channels; ch++ )
            pcm [ch] [idx] *= scale;
    }
}


static void
Fading_Out ( Float         pcm [] [ANABUFFER],
             unsigned int  N,
             const float   fs,
             unsigned int  channels )
{
    float         inv_fs = 1.f / fs;
    float         fadeout_pos;
    float         scale;
    int           n;
    unsigned int  ch;
    int           idx;

    for ( n = 0; n < BLOCK; n++, N-- ) {
        fadeout_pos    = N * inv_fs;
        scale          = fadeout_pos / FadeOutTime;
        scale          = bump (scale);
        idx            = n + CENTER;
        for ( ch = 0; ch < channels; ch++ )
            pcm [ch] [idx] *= scale;
    }
}
