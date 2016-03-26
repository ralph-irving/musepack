/*
 *  Fast Math routines
 *
 *  (C) Frank Klemm 2001. All rights reserved.
 *
 *  Principles:
 *    This modulke provides fast, but less accurate versions of the functions atan2(),
 *    cos() and sqrt(). These file responsible for providing the function Init_FastMath()
 *    which setups some look-up tables for the inline functions provided in fatsmath.h.
 *
 *  History:
 *    2001     created
 *    2002
 *
 *  Global functions:
 *    -
 *
 *  TODO:
 *    -
 */

#include "mppenc.h"

#ifdef FAST_MATH

const float  tabatan2   [ 2*TABSTEP+1] [2];
const float  tabcos     [26*TABSTEP+1] [2];
const float  tabsqrt_ex [256];
const float  tabsqrt_m  [   TABSTEP+1] [2];


void
Init_FastMath ( void )
{
    int     i;
    float   X;
    float   Y;
    double  xm;
    double  x0;
    double  xp;
    double  x;
    double  y;
    float*  p;

    p = (float*) tabatan2;
    for ( i = -TABSTEP; i <= TABSTEP; i++ ) {
        xm = atan ((i-0.5)/TABSTEP);
        x0 = atan ((i+0.0)/TABSTEP);
        xp = atan ((i+0.5)/TABSTEP);
        x  = x0/2 + (xm + xp)/4;
        y  = xp - xm;
        *p++ = x;
        *p++ = y;
    }

    p = (float*) tabcos;
    for ( i = -13*TABSTEP; i <= 13*TABSTEP; i++ ) {
        xm = cos ((i-0.5)/TABSTEP);
        x0 = cos ((i+0.0)/TABSTEP);
        xp = cos ((i+0.5)/TABSTEP);
        x  = x0/2 + (xm + xp)/4;
        y  = xp - xm;
        *p++ = x;
        *p++ = y;
    }

    p = (float*) tabsqrt_ex;
    for ( i = 0; i < 255; i++ ) {
        *(int*)&X = (i << 23);
        *(int*)&Y = (i << 23) + (1<<23) - 1;
        *p++ = sqrt(X);
    }
    *(int*)&X = (255 << 23) - 1;
    *p++ = sqrt(X);

    p = (float*) tabsqrt_m;
    for ( i = 1*TABSTEP; i <= 2*TABSTEP; i++ ) {
        xm = sqrt ((i-0.5)/TABSTEP);
        x0 = sqrt ((i+0.0)/TABSTEP);
        xp = sqrt ((i+0.5)/TABSTEP);
        x  = x0/2 + (xm + xp)/4;
        y  = xp - xm;
        *p++ = x;
        *p++ = y;
    }
}

#endif
