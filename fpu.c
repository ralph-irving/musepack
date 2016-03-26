/*
 *  Initialization of a FPU.
 *
 *  (C) Frank Klemm 2002. All rights reserved.
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


#include "mppdec.h"


void
Init_FPU ( void )
{
    Uint16_t  cw;

#if   defined __i386__  &&  defined _FPU_GETCW  &&  defined _FPU_SETCW
    _FPU_GETCW ( cw );
    cw  &=  ~0x300;
    _FPU_SETCW ( cw );
#elif defined __i386__  &&  defined  FPU_GETCW  &&  defined  FPU_SETCW
    FPU_GETCW ( cw );
    cw  &=  ~0x300;
    FPU_SETCW ( cw );
#elif defined _WIN32
    _asm { fstcw cw };
    cw  &=  ~0x300;
    _asm { fldcw cw };
#else
    ;
#endif
}

/* end of fpu.c */
