#if 1
# define ROUND32(x)   ( floattmp = (x) + (int)0x00FD8000L, *(int*)(&floattmp) - (int)0x4B7D8000L )
#else
# define ROUND32(x)   ( (int) floor ((x) + 0.5) )
#endif

#ifdef FAST_MATH

static __inline float
my_atan2 ( float x, float y )
{
    float  t;
    int    i;
    float  ret;
    float  floattmp;

    if ( (*(int*)&x & 0x7FFFFFFF) < (*(int*)&y & 0x7FFFFFFF) ) {
        i   = ROUND32 (t = TABSTEP * (x / y));
        ret = tabatan2 [1*TABSTEP+i][0] + tabatan2 [1*TABSTEP+i][1] * (t-i);
        if ( *(int*)&y < 0 )
           ret = (float)(ret - M_PI);
    }
    else if ( *(int*)&x < 0) {
        i   = ROUND32 (t = TABSTEP * (y / x));
        ret = - M_PI/2 - tabatan2 [1*TABSTEP+i][0] + tabatan2 [1*TABSTEP+i][1] * (i-t);
    }
    else if ( *(int*)&x > 0) {
        i   = ROUND32 (t = TABSTEP * (y / x));
        ret = + M_PI/2 - tabatan2 [1*TABSTEP+i][0] + tabatan2 [1*TABSTEP+i][1] * (i-t);
    }
    else {
        ret = 0.;
    }
    return ret;
}


static __inline float
my_cos ( float x )
{
    float  t;
    int    i;
    float  ret;
    float  floattmp;

    i   = ROUND32 (t = TABSTEP * x);
    ret = tabcos [13*TABSTEP+i][0] + tabcos [13*TABSTEP+i][1] * (t-i);
    return ret;
}


static __inline int
my_ifloor ( float x )
{
    x = x + (0x0C00000L + 0.500000001);
    return *(int*)&x - 1262485505;
}


static __inline float
my_sqrt ( float x )
{
    float  ret;
    int    i;
    int    ex = *(int*)&x >> 23;                                // Exponent rausholen
    float  floattmp;

    *(int*)&x = (*(int*)&x & 0x7FFFFF) | 0x42800000;            // Exponent löschen
    i    = ROUND32 (x);                                         // Integer-Anteil der Mantisse  (round ????????????)
    ret  = tabsqrt_m [i-TABSTEP][0] + tabsqrt_m [i-TABSTEP][1] * (x-i); // Wert ausrechnen
    ret *= tabsqrt_ex [ex];
    return ret;
}

#endif
