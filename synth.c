/*
 *  PCM Synthesis (quantized subband samples => PCM output)
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  - synth.c/synth.h
 *      portable synthesis routines
 *      nonportable synthesis routines using subroutines in synth_asm.nas
 *
 *  - synth_tab.c/synth_tab.h
 *      2 tables for synthesis routines in C
 *
 *  - synth_asm.nas/synth.h
 *      special subroutines for AMD's 3DNow! and Intels SIMD
 *      another 2 tables for synthesis routines (reordered tables from synth_tab.c)
 *
 *  Note: To fully understand this module you must understand the math behind
 *  subband analysis and subband synthesis.
 */

#include <string.h>
#include "mppdec.h"


#ifdef HAVE_IEEE754_FLOAT
# define ROUND32(x)   ( floattmp = (x) + (Int32_t)0x00FD8000L, *(Int32_t*)(&floattmp) - (Int32_t)0x4B7D8000L )
#else
# define ROUND32(x)   ( (Int32_t) floor ((x) + 0.5) )
#endif

#ifdef HAVE_IEEE754_DOUBLE
# define ROUND64(x)   ( doubletmp = (x) + Dither.Add + (Int64_t)0x001FFFFD80000000L, *(Int64_t*)(&doubletmp) - (Int64_t)0x433FFFFD80000000L )
#else
# define ROUND64(x)   ( (Int64_t) floor ((x) + Dither.Add) )
#endif


static dither_t  Dither;

#if !defined USE_ASM  ||  defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT  ||  defined MAKE_32BIT_FPU

static void
Calculate_New_V ( register const Float* Sample, register Float* V )
{
    // Calculating of new V-buffer values for left channel (see ISO-11172-3, p. 39)
    // based on an algorithm by Byeong Gi Lee

    register const Float*  C = Cos64;
    Float         A00, A01, A02, A03, A04, A05, A06, A07, A08, A09, A10, A11, A12, A13, A14, A15;
    Float         B00, B01, B02, B03, B04, B05, B06, B07, B08, B09, B10, B11, B12, B13, B14, B15;
    Float         tmp;
    Float         tmp2;
    Uint32_t      togg;

    ENTER(163);

    V += 32;

    A00 = Sample[ 0] + Sample[31];
    A01 = Sample[ 1] + Sample[30];
    A02 = Sample[ 3] + Sample[28];
    A03 = Sample[ 2] + Sample[29];
    A04 = Sample[ 7] + Sample[24];
    A05 = Sample[ 6] + Sample[25];
    A06 = Sample[ 4] + Sample[27];
    A07 = Sample[ 5] + Sample[26];
    A08 = Sample[15] + Sample[16];
    A09 = Sample[14] + Sample[17];
    A10 = Sample[12] + Sample[19];
    A11 = Sample[13] + Sample[18];
    A12 = Sample[ 8] + Sample[23];
    A13 = Sample[ 9] + Sample[22];
    A14 = Sample[11] + Sample[20];
    A15 = Sample[10] + Sample[21];

    B00 =  A00 + A08;
    B01 =  A01 + A09;
    B02 =  A02 + A10;
    B03 =  A03 + A11;
    B04 =  A04 + A12;
    B05 =  A05 + A13;
    B06 =  A06 + A14;
    B07 =  A07 + A15;
    B08 = (A00 - A08) * C[ 2];
    B09 = (A01 - A09) * C[ 6];
    B10 = (A02 - A10) * C[14];
    B11 = (A03 - A11) * C[10];
    B12 = (A04 - A12) * C[30];
    B13 = (A05 - A13) * C[26];
    B14 = (A06 - A14) * C[18];
    B15 = (A07 - A15) * C[22];

    A00 =  B00 + B04;
    A01 =  B01 + B05;
    A02 =  B02 + B06;
    A03 =  B03 + B07;
    A04 = (B00 - B04) * C[ 4];
    A05 = (B01 - B05) * C[12];
    A06 = (B02 - B06) * C[28];
    A07 = (B03 - B07) * C[20];
    A08 =  B08 + B12;
    A09 =  B09 + B13;
    A10 =  B10 + B14;
    A11 =  B11 + B15;
    A12 = (B08 - B12) * C[ 4];
    A13 = (B09 - B13) * C[12];
    A14 = (B10 - B14) * C[28];
    A15 = (B11 - B15) * C[20];

    B00 =  A00 + A02;
    B01 =  A01 + A03;
    B02 = (A00 - A02) * C[ 8];
    B03 = (A01 - A03) * C[24];
    B04 =  A04 + A06;
    B05 =  A05 + A07;
    B06 = (A04 - A06) * C[ 8];
    B07 = (A05 - A07) * C[24];
    B08 =  A08 + A10;
    B09 =  A09 + A11;
    B10 = (A08 - A10) * C[ 8];
    B11 = (A09 - A11) * C[24];
    B12 =  A12 + A14;
    B13 =  A13 + A15;
    B14 = (A12 - A14) * C[ 8];
    B15 = (A13 - A15) * C[24];

    A00 =  B00 + B01;
    A01 = (B00 - B01) * C[16];
    A02 =  B02 + B03;
    A03 = (B02 - B03) * C[16];
    A04 =  B04 + B05;
    A05 = (B04 - B05) * C[16];
    A06 =  B06 + B07;
    A07 = (B06 - B07) * C[16];
    A08 =  B08 + B09;
    A09 = (B08 - B09) * C[16];
    A10 =  B10 + B11;
    A11 = (B10 - B11) * C[16];
    A12 =  B12 + B13;
    A13 = (B12 - B13) * C[16];
    A14 =  B14 + B15;
    A15 = (B14 - B15) * C[16];

    V[48-32] = -A00;
    V[ 0-32] =  A01;
    V[40-32] = -A02 - (V[ 8-32] = A03);

    V[36-32] = -((V[ 4-32] = A05 + (V[12-32] = A07)) + A06);
    V[44-32] = - A04 - A06 - A07;

    V[ 6-32] = (V[10-32] = A11 + (V[14-32] = A15)) + A13;
    V[38-32] = (V[34-32] = -(V[ 2-32] = A09 + A13 + A15) - A14) + A09 - A10 - A11;
    V[46-32] = (tmp = -(A12 + A14 + A15)) - A08;
    V[42-32] = tmp - A10 - A11;

    A00 = (Sample[ 0] - Sample[31]) * C[ 1];
    A01 = (Sample[ 1] - Sample[30]) * C[ 3];
    A02 = (Sample[ 3] - Sample[28]) * C[ 7];
    A03 = (Sample[ 2] - Sample[29]) * C[ 5];
    A04 = (Sample[ 7] - Sample[24]) * C[15];
    A05 = (Sample[ 6] - Sample[25]) * C[13];
    A06 = (Sample[ 4] - Sample[27]) * C[ 9];
    A07 = (Sample[ 5] - Sample[26]) * C[11];
    A08 = (Sample[15] - Sample[16]) * C[31];
    A09 = (Sample[14] - Sample[17]) * C[29];
    A10 = (Sample[12] - Sample[19]) * C[25];
    A11 = (Sample[13] - Sample[18]) * C[27];
    A12 = (Sample[ 8] - Sample[23]) * C[17];
    A13 = (Sample[ 9] - Sample[22]) * C[19];
    A14 = (Sample[11] - Sample[20]) * C[23];
    A15 = (Sample[10] - Sample[21]) * C[21];

    B00 =  A00 + A08;
    B01 =  A01 + A09;
    B02 =  A02 + A10;
    B03 =  A03 + A11;
    B04 =  A04 + A12;
    B05 =  A05 + A13;
    B06 =  A06 + A14;
    B07 =  A07 + A15;
    B08 = (A00 - A08) * C[ 2];
    B09 = (A01 - A09) * C[ 6];
    B10 = (A02 - A10) * C[14];
    B11 = (A03 - A11) * C[10];
    B12 = (A04 - A12) * C[30];
    B13 = (A05 - A13) * C[26];
    B14 = (A06 - A14) * C[18];
    B15 = (A07 - A15) * C[22];

    A00 =  B00 + B04;
    A01 =  B01 + B05;
    A02 =  B02 + B06;
    A03 =  B03 + B07;
    A04 = (B00 - B04) * C[ 4];
    A05 = (B01 - B05) * C[12];
    A06 = (B02 - B06) * C[28];
    A07 = (B03 - B07) * C[20];
    A08 =  B08 + B12;
    A09 =  B09 + B13;
    A10 =  B10 + B14;
    A11 =  B11 + B15;
    A12 = (B08 - B12) * C[ 4];
    A13 = (B09 - B13) * C[12];
    A14 = (B10 - B14) * C[28];
    A15 = (B11 - B15) * C[20];

    B00 =  A00 + A02;
    B01 =  A01 + A03;
    B02 = (A00 - A02) * C[ 8];
    B03 = (A01 - A03) * C[24];
    B04 =  A04 + A06;
    B05 =  A05 + A07;
    B06 = (A04 - A06) * C[ 8];
    B07 = (A05 - A07) * C[24];
    B08 =  A08 + A10;
    B09 =  A09 + A11;
    B10 = (A08 - A10) * C[ 8];
    B11 = (A09 - A11) * C[24];
    B12 =  A12 + A14;
    B13 =  A13 + A15;
    B14 = (A12 - A14) * C[ 8];
    B15 = (A13 - A15) * C[24];

    A00 =  B00 + B01;
    A01 = (B00 - B01) * C[16];
    A02 =  B02 + B03;
    A03 = (B02 - B03) * C[16];
    A04 =  B04 + B05;
    A05 = (B04 - B05) * C[16];
    A06 =  B06 + B07;
    A07 = (B06 - B07) * C[16];
    A08 =  B08 + B09;
    A09 = (B08 - B09) * C[16];
    A10 =  B10 + B11;
    A11 = (B10 - B11) * C[16];
    A12 =  B12 + B13;
    A13 = (B12 - B13) * C[16];
    A14 =  B14 + B15;
    A15 = (B14 - B15) * C[16];

    V[ 5-32] = (V[11-32] = (V[13-32] = A07 + (V[15-32] = A15)) + A11) + A05 + A13;
    V[ 7-32] = (V[ 9-32] = A03 + A11 + A15) + A13;
    V[33-32] = -(V[ 1-32] = A01 + (tmp = A09 + A13 + A15)) - A14;
    V[35-32] = -(V[ 3-32] = A05 + A07 + tmp) - A06 - A14;

    V[37-32] = (tmp = -(A10 + A11 + A13 + A14 + A15)) - A05 - A06 - A07;
    V[39-32] = tmp - A02 - A03;
    V[41-32] = (tmp += A13 - A12) - A02 - A03;
    V[43-32] = tmp - (tmp2 = A04 + A06 + A07);

    V[47-32] = (tmp = -(A08 + A12 + A14 + A15)) - A00;
    V[45-32] = tmp - tmp2;

#if 1  &&  (SIZEOF_Float == 4)
    // a little improvement in speed is possible if both values directly stored by the code above
    // this would make this code unnecessary, but enlarges code above.
    togg = (Uint32_t)0x80000000L;
    ((Uint32_t*)V)[32-32] = togg + ((Uint32_t*)V)[ 0-32];
    ((Uint32_t*)V)[31-32] = togg + ((Uint32_t*)V)[ 1-32];
    ((Uint32_t*)V)[30-32] = togg + ((Uint32_t*)V)[ 2-32];
    ((Uint32_t*)V)[29-32] = togg + ((Uint32_t*)V)[ 3-32];
    ((Uint32_t*)V)[28-32] = togg + ((Uint32_t*)V)[ 4-32];
    ((Uint32_t*)V)[27-32] = togg + ((Uint32_t*)V)[ 5-32];
    ((Uint32_t*)V)[26-32] = togg + ((Uint32_t*)V)[ 6-32];
    ((Uint32_t*)V)[25-32] = togg + ((Uint32_t*)V)[ 7-32];
    ((Uint32_t*)V)[24-32] = togg + ((Uint32_t*)V)[ 8-32];
    ((Uint32_t*)V)[23-32] = togg + ((Uint32_t*)V)[ 9-32];
    ((Uint32_t*)V)[22-32] = togg + ((Uint32_t*)V)[10-32];
    ((Uint32_t*)V)[21-32] = togg + ((Uint32_t*)V)[11-32];
    ((Uint32_t*)V)[20-32] = togg + ((Uint32_t*)V)[12-32];
    ((Uint32_t*)V)[19-32] = togg + ((Uint32_t*)V)[13-32];
    ((Uint32_t*)V)[18-32] = togg + ((Uint32_t*)V)[14-32];
    ((Uint32_t*)V)[17-32] = togg + ((Uint32_t*)V)[15-32];

    ((Uint32_t*)V)[63-32] = ((Uint32_t*)V)[33-32];
    ((Uint32_t*)V)[62-32] = ((Uint32_t*)V)[34-32];
    ((Uint32_t*)V)[61-32] = ((Uint32_t*)V)[35-32];
    ((Uint32_t*)V)[60-32] = ((Uint32_t*)V)[36-32];
    ((Uint32_t*)V)[59-32] = ((Uint32_t*)V)[37-32];
    ((Uint32_t*)V)[58-32] = ((Uint32_t*)V)[38-32];
    ((Uint32_t*)V)[57-32] = ((Uint32_t*)V)[39-32];
    ((Uint32_t*)V)[56-32] = ((Uint32_t*)V)[40-32];
    ((Uint32_t*)V)[55-32] = ((Uint32_t*)V)[41-32];
    ((Uint32_t*)V)[54-32] = ((Uint32_t*)V)[42-32];
    ((Uint32_t*)V)[53-32] = ((Uint32_t*)V)[43-32];
    ((Uint32_t*)V)[52-32] = ((Uint32_t*)V)[44-32];
    ((Uint32_t*)V)[51-32] = ((Uint32_t*)V)[45-32];
    ((Uint32_t*)V)[50-32] = ((Uint32_t*)V)[46-32];
    ((Uint32_t*)V)[49-32] = ((Uint32_t*)V)[47-32];
#else
    V[32-32] = -V[ 0-32];
    V[31-32] = -V[ 1-32];
    V[30-32] = -V[ 2-32];
    V[29-32] = -V[ 3-32];
    V[28-32] = -V[ 4-32];
    V[27-32] = -V[ 5-32];
    V[26-32] = -V[ 6-32];
    V[25-32] = -V[ 7-32];
    V[24-32] = -V[ 8-32];
    V[23-32] = -V[ 9-32];
    V[22-32] = -V[10-32];
    V[21-32] = -V[11-32];
    V[20-32] = -V[12-32];
    V[19-32] = -V[13-32];
    V[18-32] = -V[14-32];
    V[17-32] = -V[15-32];

    V[63-32] =  V[33-32];
    V[62-32] =  V[34-32];
    V[61-32] =  V[35-32];
    V[60-32] =  V[36-32];
    V[59-32] =  V[37-32];
    V[58-32] =  V[38-32];
    V[57-32] =  V[39-32];
    V[56-32] =  V[40-32];
    V[55-32] =  V[41-32];
    V[54-32] =  V[42-32];
    V[53-32] =  V[43-32];
    V[52-32] =  V[44-32];
    V[51-32] =  V[45-32];
    V[50-32] =  V[46-32];
    V[49-32] =  V[47-32];
#endif
    LEAVE(163);
}

#endif


#if defined MAKE_32BIT_FPU

void
Synthese_Filter_32FP_C ( register Float32_t* Stream, Int* const offset, Float* Vi, const Float Yi[][32], Uint channels )
{
    Int                 n;
    register Int        k;
    register Float32_t  Sum;
    const Float       (*D)[16];
    const Float*        V;

    ENTER(164);
    for ( n = 0; n < 36; n++, Yi++ ) {
        // shifting 64 indices upwards
        if ( (*offset -= 64) < 0 ) {
            *offset += 64*VIRT_SHIFT;
            ENTER(170);
            memmove ( Vi+64*VIRT_SHIFT, Vi, (16-1)*64*sizeof(*Vi) );
            LEAVE(170);
        }

        Calculate_New_V ( *Yi, (Float*)(V = Vi + *offset) );

        // vectoring & windowing & calculating PCM-Output
        D = Di_opt;
        for ( k = 0; k < 32; k++, V++, D++ ) {
            *Stream =     ( V [  0] * D [0][ 0]
                          + V [ 96] * D [0][ 1]
                          + V [128] * D [0][ 2]
                          + V [224] * D [0][ 3]
                          + V [256] * D [0][ 4]
                          + V [352] * D [0][ 5]
                          + V [384] * D [0][ 6]
                          + V [480] * D [0][ 7]
                          + V [512] * D [0][ 8]
                          + V [608] * D [0][ 9]
                          + V [640] * D [0][10]
                          + V [736] * D [0][11]
                          + V [768] * D [0][12]
                          + V [864] * D [0][13]
                          + V [896] * D [0][14]
                          + V [992] * D [0][15] ) * (1./32768);

            Stream += channels;
        }
    }
    LEAVE(164);
    return;
}

#elif !defined USE_ASM  &&  !defined MAKE_16BIT  &&  !defined MAKE_24BIT  &&  !defined MAKE_32BIT

void
Synthese_Filter_16_C ( register Int16_t* Stream, Int* const offset, Float* Vi, const Float Yi[][32], Uint channels )
{
    Int               n;
    register Int      k;
    register Int32_t  Sum;
    const Float       (*D)[16];
    const Float*      V;
#ifdef HAVE_IEEE754_FLOAT
    Float32_t         floattmp;
#endif

    ENTER(164);
    for ( n = 0; n < 36; n++, Yi++ ) {
        // shifting 64 indices upwards
        if ( (*offset -= 64) < 0 ) {
            *offset += 64*VIRT_SHIFT;
            ENTER(170);
            memmove ( Vi+64*VIRT_SHIFT, Vi, (16-1)*64*sizeof(*Vi) );
            LEAVE(170);
        }

        Calculate_New_V ( *Yi, (Float*)(V = Vi + *offset) );

        // vectoring & windowing & calculating PCM-Output
        D = Di_opt;
        for ( k = 0; k < 32; k++, V++, D++ ) {
            Sum = ROUND32 ( V [  0] * D [0][ 0]
                          + V [ 96] * D [0][ 1]
                          + V [128] * D [0][ 2]
                          + V [224] * D [0][ 3]
                          + V [256] * D [0][ 4]
                          + V [352] * D [0][ 5]
                          + V [384] * D [0][ 6]
                          + V [480] * D [0][ 7]
                          + V [512] * D [0][ 8]
                          + V [608] * D [0][ 9]
                          + V [640] * D [0][10]
                          + V [736] * D [0][11]
                          + V [768] * D [0][12]
                          + V [864] * D [0][13]
                          + V [896] * D [0][14]
                          + V [992] * D [0][15] );

            // copy to PCM
            if (Sum != (Int16_t)Sum ) {            // prevent from wrap around
                Dither.Overdrives++;
                if ( Sum > +Dither.MaxLevel ) Dither.MaxLevel = +Sum;
                if ( Sum < -Dither.MaxLevel ) Dither.MaxLevel = -Sum;
                Sum = (Sum >> 31) ^ 0x7FFF;
            }

            *Stream = (Int16_t)Sum;
            Stream += channels;
        }
    }
    LEAVE(164);
    return;
}


#endif


#if defined USE_ASM  &&  !defined MAKE_16BIT  &&  !defined MAKE_24BIT  &&  !defined MAKE_32BIT

static void
Synthese_Filter_16_i387 ( Int16_t* Stream, Int* const offset, Float* Vi, const Float Yi[][32], Uint channels )
{
    Int           n;
    Int           k;
    register Int32_t  Sum;
    const Float*  V;
    Int32_t       scratch [32+8];
    Int32_t*      scratchptr = (Int32_t*) ALIGN ( scratch, 0x20 );

    ENTER(171);
    for ( n = 0; n < 36; n++, Yi++ ) {
        // shifting 64 indices upwards
        if ( (*offset -= 64) < 0 ) {
            *offset += 64*VIRT_SHIFT;
            ENTER(170);
            memmove ( Vi+64*VIRT_SHIFT, Vi, (16-1)*64*sizeof(*Vi) );
            LEAVE(170);
        }

        ENTER(172);
        Calculate_New_V_i387 ( *Yi, (Float*)(V = Vi + *offset) );
        LEAVE(172);

        // vectoring & windowing & calculating PCM-Output
        VectorMult_i387 ( scratchptr, V );
        for ( k = 0; k < 32; k++ ) {
            Sum = scratchptr[k] - (Int32_t)0x4B7D8000L;
            if (Sum != (Int16_t)Sum ) {            // prevent from wrap around
                Dither.Overdrives++;
                if ( Sum > +Dither.MaxLevel ) Dither.MaxLevel = +Sum;
                if ( Sum < -Dither.MaxLevel ) Dither.MaxLevel = -Sum;
                Sum = (Sum >> 31) ^ 0x7FFF;
            }

            *Stream = (Int16_t) Sum;
            Stream += channels;
        }
    }

    LEAVE(171);
    return;
}


static void
Synthese_Filter_16_3DNow ( Int16_t* Stream, Int* const offset, Float* Vi, const Float Yi[][32], Uint channels )
{
    Int           n;
    Int           k;
    const Float*  V;
    Int32_t       scratch [32+8];
    Int32_t*      scratchptr = (Int32_t*) ALIGN ( scratch, 0x20 );

    ENTER(171);
    for ( n = 0; n < 36; n++, Yi++ ) {
        // shifting 64 indices upwards
        if ( (*offset -= 64) < 0 ) {
            *offset += 64*VIRT_SHIFT;
            ENTER(170);
            memcpy_dn_MMX ( Vi+64*VIRT_SHIFT, Vi, (16-1)*sizeof(*Vi) );
            //memmove ( Vi+64*VIRT_SHIFT, Vi, (16-1)*64*sizeof(*Vi) );
            LEAVE(170);
        }

        ENTER(172);
        Calculate_New_V_3DNow ( *Yi, (Float*)(V = Vi + *offset) );
        LEAVE(172);

        // vectoring & windowing & calculating PCM-Output
        VectorMult_3DNow ( scratchptr, V );
        for ( k = 0; k < 32; k++ ) {
            *Stream = (Int16_t)(scratchptr[k] >> 16);
            Stream += channels;
        }
    }
    Reset_FPU_3DNow ();

    LEAVE(171);
    return;
}


static void
Calculate_New_V_SIMD ( const Float* Sample, Float* V )
{
    // Calculating of new V-buffer values for left channel (see ISO-11172-3, p. 39)
    // based on an algorithm by Byeong Gi Lee

    Float     __A [32 + 8];
    Float*    A = (Float*) ALIGN ( __A, 0x20 );
    Float     tmp;
    Float     tmp2;
    Uint32_t  togg;

    ENTER(163);

    V += 32;

    New_V_Helper2 ( A, Sample );

    V[48-32] = -A[ 0];
    V[ 0-32] =  A[ 1];
    V[40-32] = -A[ 2] - (V[ 8-32] = A[ 3]);

    V[36-32] = -((V[ 4-32] = A[ 5] + (V[12-32] = A[ 7])) + A[ 6]);
    V[44-32] = - A[ 4] - A[ 6] - A[ 7];

    V[ 6-32] = (V[10-32] = A[11] + (V[14-32] = A[15])) + A[13];
    V[38-32] = (V[34-32] = -(V[ 2-32] = A[ 9] + A[13] + A[15]) - A[14]) + A[ 9] - A[10] - A[11];
    V[46-32] = (tmp = -(A[12] + A[14] + A[15])) - A[ 8];
    V[42-32] = tmp - A[10] - A[11];

    New_V_Helper3 ( A, Sample );

    V[ 5-32] = (V[11-32] = (V[13-32] = A[ 7] + (V[15-32] = A[15])) + A[11]) + A[ 5] + A[13];
    V[ 7-32] = (V[ 9-32] = A[ 3] + A[11] + A[15]) + A[13];
    V[33-32] = -(V[ 1-32] = A[ 1] + (tmp = A[ 9] + A[13] + A[15])) - A[14];
    V[35-32] = -(V[ 3-32] = A[ 5] + A[ 7] + tmp) - A[ 6] - A[14];

    V[37-32] = (tmp = -(A[10] + A[11] + A[13] + A[14] + A[15])) - A[ 5] - A[ 6] - A[ 7];
    V[39-32] = tmp - A[ 2] - A[ 3];
    V[41-32] = (tmp += A[13] - A[12]) - A[ 2] - A[ 3];
    V[43-32] = tmp - (tmp2 = A[ 4] + A[ 6] + A[ 7]);

    V[47-32] = (tmp = -(A[ 8] + A[12] + A[14] + A[15])) - A[ 0];
    V[45-32] = tmp - tmp2;

#if 0
    New_V_Helper4 ( V );                                            // slower
#elif 1
    // a little improvement in speed is possible if both values directly stored by the code above
    // this would make this code unnecessary, but enlarges code above.
    togg = (Uint32_t)0x80000000L;
    ((Uint32_t*)V)[32-32] = togg + ((Uint32_t*)V)[ 0-32];
    ((Uint32_t*)V)[31-32] = togg + ((Uint32_t*)V)[ 1-32];
    ((Uint32_t*)V)[30-32] = togg + ((Uint32_t*)V)[ 2-32];
    ((Uint32_t*)V)[29-32] = togg + ((Uint32_t*)V)[ 3-32];
    ((Uint32_t*)V)[28-32] = togg + ((Uint32_t*)V)[ 4-32];
    ((Uint32_t*)V)[27-32] = togg + ((Uint32_t*)V)[ 5-32];
    ((Uint32_t*)V)[26-32] = togg + ((Uint32_t*)V)[ 6-32];
    ((Uint32_t*)V)[25-32] = togg + ((Uint32_t*)V)[ 7-32];
    ((Uint32_t*)V)[24-32] = togg + ((Uint32_t*)V)[ 8-32];
    ((Uint32_t*)V)[23-32] = togg + ((Uint32_t*)V)[ 9-32];
    ((Uint32_t*)V)[22-32] = togg + ((Uint32_t*)V)[10-32];
    ((Uint32_t*)V)[21-32] = togg + ((Uint32_t*)V)[11-32];
    ((Uint32_t*)V)[20-32] = togg + ((Uint32_t*)V)[12-32];
    ((Uint32_t*)V)[19-32] = togg + ((Uint32_t*)V)[13-32];
    ((Uint32_t*)V)[18-32] = togg + ((Uint32_t*)V)[14-32];
    ((Uint32_t*)V)[17-32] = togg + ((Uint32_t*)V)[15-32];

    ((Uint32_t*)V)[63-32] = ((Uint32_t*)V)[33-32];
    ((Uint32_t*)V)[62-32] = ((Uint32_t*)V)[34-32];
    ((Uint32_t*)V)[61-32] = ((Uint32_t*)V)[35-32];
    ((Uint32_t*)V)[60-32] = ((Uint32_t*)V)[36-32];
    ((Uint32_t*)V)[59-32] = ((Uint32_t*)V)[37-32];
    ((Uint32_t*)V)[58-32] = ((Uint32_t*)V)[38-32];
    ((Uint32_t*)V)[57-32] = ((Uint32_t*)V)[39-32];
    ((Uint32_t*)V)[56-32] = ((Uint32_t*)V)[40-32];
    ((Uint32_t*)V)[55-32] = ((Uint32_t*)V)[41-32];
    ((Uint32_t*)V)[54-32] = ((Uint32_t*)V)[42-32];
    ((Uint32_t*)V)[53-32] = ((Uint32_t*)V)[43-32];
    ((Uint32_t*)V)[52-32] = ((Uint32_t*)V)[44-32];
    ((Uint32_t*)V)[51-32] = ((Uint32_t*)V)[45-32];
    ((Uint32_t*)V)[50-32] = ((Uint32_t*)V)[46-32];
    ((Uint32_t*)V)[49-32] = ((Uint32_t*)V)[47-32];
#else
    V[32-32] = -V[ 0-32];
    V[31-32] = -V[ 1-32];
    V[30-32] = -V[ 2-32];
    V[29-32] = -V[ 3-32];
    V[28-32] = -V[ 4-32];
    V[27-32] = -V[ 5-32];
    V[26-32] = -V[ 6-32];
    V[25-32] = -V[ 7-32];
    V[24-32] = -V[ 8-32];
    V[23-32] = -V[ 9-32];
    V[22-32] = -V[10-32];
    V[21-32] = -V[11-32];
    V[20-32] = -V[12-32];
    V[19-32] = -V[13-32];
    V[18-32] = -V[14-32];
    V[17-32] = -V[15-32];

    V[63-32] =  V[33-32];
    V[62-32] =  V[34-32];
    V[61-32] =  V[35-32];
    V[60-32] =  V[36-32];
    V[59-32] =  V[37-32];
    V[58-32] =  V[38-32];
    V[57-32] =  V[39-32];
    V[56-32] =  V[40-32];
    V[55-32] =  V[41-32];
    V[54-32] =  V[42-32];
    V[53-32] =  V[43-32];
    V[52-32] =  V[44-32];
    V[51-32] =  V[45-32];
    V[50-32] =  V[46-32];
    V[49-32] =  V[47-32];
#endif
    LEAVE(163);
}


static void
Synthese_Filter_16_SIMD ( Int16_t* Stream, Int* const offset, Float* Vi, const Float Yi[][32], Uint channels )
{
    Int           n;
    Int           k;
    Int32_t       Sum;
    const Float*  V;
    Int32_t       scratch [32+8];
    Int32_t*      scratchptr = (Int32_t*) ALIGN ( scratch, 0x20 );

    ENTER(241);
    for ( n = 0; n < 36; n++, Yi++ ) {
        // shifting 64 indices upwards
        if ( (*offset -= 64) < 0 ) {
            *offset += 64*VIRT_SHIFT;
            ENTER(240);
#if 0
            memcpy_dn_SIMD ( Vi+64*VIRT_SHIFT, Vi, (16-1)*sizeof(*Vi)/2 );  // is slower!!!
#else
            memcpy_dn_MMX ( Vi+64*VIRT_SHIFT, Vi, (16-1)*sizeof(*Vi) );
            Reset_FPU ();
#endif
            LEAVE(240);
        }

        ENTER(242);
        Calculate_New_V_SIMD ( *Yi, (Float*)(V = Vi + *offset) );
        LEAVE(242);

        // vectoring & windowing & calculating PCM-Output
        VectorMult_SIMD ( scratchptr, V );
        for ( k = 0; k < 32; k++ ) {
            Sum = scratchptr [k] - (Int32_t)0x537D8000L;
            if (Sum != (Int16_t)Sum ) {            // prevent from wrap around
                Dither.Overdrives++;
                if ( Sum > +Dither.MaxLevel ) Dither.MaxLevel = +Sum;
                if ( Sum < -Dither.MaxLevel ) Dither.MaxLevel = -Sum;
                Sum = (Sum >> 31) ^ 0x7FFF;
            }

            *Stream = (Int16_t) Sum;
            Stream += channels;
        }
    }

    LEAVE(241);
    return;
}

#pragma warning ( disable: 4550 )
SyntheseFilter16_t
Get_Synthese_Filter ( void )
{
#ifdef USE_ASM
    if ( Has_3DNow () )
        return Synthese_Filter_16_3DNow;
    if ( Has_SIMD  () )
        return Synthese_Filter_16_SIMD;
    return Synthese_Filter_16_i387;
#else
    return Synthese_Filter_16_C;
#endif
}
#pragma warning ( default: 4550 )


#endif


static const  Uchar    Parity [256] = {  // parity
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0
};

static Uint32_t  __r1 = 1;
static Uint32_t  __r2 = 1;


/*
 *  This is a simple random number generator with good quality for audio purposes.
 *  It consists of two polycounters with opposite rotation direction and different
 *  periods. The periods are coprime, so the total period is the product of both.
 *
 *     -------------------------------------------------------------------------------------------------
 * +-> |31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10: 9: 8: 7: 6: 5: 4: 3: 2: 1: 0|
 * |   -------------------------------------------------------------------------------------------------
 * |                                                                             |  |  |  |     |     |
 * |                                                                             +--+--+-XOR-+--------+
 * |                                                                                      |
 * +--------------------------------------------------------------------------------------+
 *
 *     -------------------------------------------------------------------------------------------------
 *     |31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10: 9: 8: 7: 6: 5: 4: 3: 2: 1: 0| <-+
 *     -------------------------------------------------------------------------------------------------   |
 *       |  |           |  |                                                                               |
 *       +--+----XOR----+--+                                                                               |
 *                |                                                                                        |
 *                +----------------------------------------------------------------------------------------+
 *
 *
 *  The first has an period of 3*5*17*257*65537, the second of 7*47*73*178481,
 *  which gives a period of 18.410.713.077.675.721.215. The result is the
 *  XORed values of both generators.
 */

static void
set_seed ( Uint32_t  seed )
{
    __r1 = seed != 0 ? seed : 1;                // 0 is a forbidden value which locks the generator
    __r2 = 1;                                   // the are several (8389119) forbidden codes. 1 is not one of them
}


Uint32_t
random_int ( void )
{
#if 1
    Uint32_t  t1, t2, t3, t4;

    t3   = t1 = __r1;   t4   = t2 = __r2;       // Parity calculation is done via table lookup, this is also available
    t1  &= 0xF5;        t2 >>= 25;              // on CPUs without parity, can be implemented in C and avoid unpredictable
    t1   = Parity [t1]; t2  &= 0x63;            // jumps and slow rotate through the carry flag operations.
    t1 <<= 31;          t2   = Parity [t2];

    return (__r1 = (t3 >> 1) | t1 ) ^ (__r2 = (t4 + t4) | t2 );
#else
    return (__r1 = (__r1 >> 1) | ((Uint32_t)Parity [__r1 & 0xF5] << 31) ) ^
           (__r2 = (__r2 << 1) |  (Uint32_t)Parity [(__r2 >> 25) & 0x63] );
#endif
}


#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT

/***********************************************************************************************************************/

static Double
Random_Equi ( Double mult )                     // gives a equal distributed random number
{                                               // between -2^31*mult and +2^31*mult
    return mult * (Int32_t) random_int ();
}

static Double
Random_Triangular ( Double mult )               // gives a triangular distributed random number
{                                               // between -2^32*mult and +2^32*mult
    return mult * ( (Double) (Int32_t) random_int () + (Double) (Int32_t) random_int () );
}


/*********************************************************************************************************************/

static const Float  F44_0 [16 + 32] = {
    (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0,
    (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0,

    (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0,
    (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0,

    (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0,
    (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0, (Float)0
};


static const Float  F44_1 [16 + 32] = {  /* SNR(w) = 4.843163 dB, SNR = -3.192134 dB */
    (Float) 0.85018292704024355931, (Float) 0.29089597350995344721, (Float)-0.05021866022121039450, (Float)-0.23545456294599161833,
    (Float)-0.58362726442227032096, (Float)-0.67038978965193036429, (Float)-0.38566861572833459221, (Float)-0.15218663390367969967,
    (Float)-0.02577543084864530676, (Float) 0.14119295297688728127, (Float) 0.22398848581628781612, (Float) 0.15401727203382084116,
    (Float) 0.05216161232906000929, (Float)-0.00282237820999675451, (Float)-0.03042794608323867363, (Float)-0.03109780942998826024,

    (Float) 0.85018292704024355931, (Float) 0.29089597350995344721, (Float)-0.05021866022121039450, (Float)-0.23545456294599161833,
    (Float)-0.58362726442227032096, (Float)-0.67038978965193036429, (Float)-0.38566861572833459221, (Float)-0.15218663390367969967,
    (Float)-0.02577543084864530676, (Float) 0.14119295297688728127, (Float) 0.22398848581628781612, (Float) 0.15401727203382084116,
    (Float) 0.05216161232906000929, (Float)-0.00282237820999675451, (Float)-0.03042794608323867363, (Float)-0.03109780942998826024,

    (Float) 0.85018292704024355931, (Float) 0.29089597350995344721, (Float)-0.05021866022121039450, (Float)-0.23545456294599161833,
    (Float)-0.58362726442227032096, (Float)-0.67038978965193036429, (Float)-0.38566861572833459221, (Float)-0.15218663390367969967,
    (Float)-0.02577543084864530676, (Float) 0.14119295297688728127, (Float) 0.22398848581628781612, (Float) 0.15401727203382084116,
    (Float) 0.05216161232906000929, (Float)-0.00282237820999675451, (Float)-0.03042794608323867363, (Float)-0.03109780942998826024,
};


static const Float  F44_2 [16 + 32] = {  /* SNR(w) = 10.060213 dB, SNR = -12.766730 dB */
    (Float) 1.78827593892108555290, (Float) 0.95508210637394326553, (Float)-0.18447626783899924429, (Float)-0.44198126506275016437,
    (Float)-0.88404052492547413497, (Float)-1.42218907262407452967, (Float)-1.02037566838362314995, (Float)-0.34861755756425577264,
    (Float)-0.11490230170431934434, (Float) 0.12498899339968611803, (Float) 0.38065885268563131927, (Float) 0.31883491321310506562,
    (Float) 0.10486838686563442765, (Float)-0.03105361685110374845, (Float)-0.06450524884075370758, (Float)-0.02939198261121969816,

    (Float) 1.78827593892108555290, (Float) 0.95508210637394326553, (Float)-0.18447626783899924429, (Float)-0.44198126506275016437,
    (Float)-0.88404052492547413497, (Float)-1.42218907262407452967, (Float)-1.02037566838362314995, (Float)-0.34861755756425577264,
    (Float)-0.11490230170431934434, (Float) 0.12498899339968611803, (Float) 0.38065885268563131927, (Float) 0.31883491321310506562,
    (Float) 0.10486838686563442765, (Float)-0.03105361685110374845, (Float)-0.06450524884075370758, (Float)-0.02939198261121969816,

    (Float) 1.78827593892108555290, (Float) 0.95508210637394326553, (Float)-0.18447626783899924429, (Float)-0.44198126506275016437,
    (Float)-0.88404052492547413497, (Float)-1.42218907262407452967, (Float)-1.02037566838362314995, (Float)-0.34861755756425577264,
    (Float)-0.11490230170431934434, (Float) 0.12498899339968611803, (Float) 0.38065885268563131927, (Float) 0.31883491321310506562,
    (Float) 0.10486838686563442765, (Float)-0.03105361685110374845, (Float)-0.06450524884075370758, (Float)-0.02939198261121969816,
};


static const Float  F44_3 [16 + 32] = {  /* SNR(w) = 15.382598 dB, SNR = -29.402334 dB */
    (Float) 2.89072132015058161445, (Float) 2.68932810943698754106, (Float) 0.21083359339410251227, (Float)-0.98385073324997617515,
    (Float)-1.11047823227097316719, (Float)-2.18954076314139673147, (Float)-2.36498032881953056225, (Float)-0.95484132880101140785,
    (Float)-0.23924057925542965158, (Float)-0.13865235703915925642, (Float) 0.43587843191057992846, (Float) 0.65903257226026665927,
    (Float) 0.24361815372443152787, (Float)-0.00235974960154720097, (Float) 0.01844166574603346289, (Float) 0.01722945988740875099,

    (Float) 2.89072132015058161445, (Float) 2.68932810943698754106, (Float) 0.21083359339410251227, (Float)-0.98385073324997617515,
    (Float)-1.11047823227097316719, (Float)-2.18954076314139673147, (Float)-2.36498032881953056225, (Float)-0.95484132880101140785,
    (Float)-0.23924057925542965158, (Float)-0.13865235703915925642, (Float) 0.43587843191057992846, (Float) 0.65903257226026665927,
    (Float) 0.24361815372443152787, (Float)-0.00235974960154720097, (Float) 0.01844166574603346289, (Float) 0.01722945988740875099,

    (Float) 2.89072132015058161445, (Float) 2.68932810943698754106, (Float) 0.21083359339410251227, (Float)-0.98385073324997617515,
    (Float)-1.11047823227097316719, (Float)-2.18954076314139673147, (Float)-2.36498032881953056225, (Float)-0.95484132880101140785,
    (Float)-0.23924057925542965158, (Float)-0.13865235703915925642, (Float) 0.43587843191057992846, (Float) 0.65903257226026665927,
    (Float) 0.24361815372443152787, (Float)-0.00235974960154720097, (Float) 0.01844166574603346289, (Float) 0.01722945988740875099
};


static Double
scalar16 ( const Float* x, const Float* y )
{
    return x[ 0]*y[ 0] + x[ 1]*y[ 1] + x[ 2]*y[ 2] + x[ 3]*y[ 3]
         + x[ 4]*y[ 4] + x[ 5]*y[ 5] + x[ 6]*y[ 6] + x[ 7]*y[ 7]
         + x[ 8]*y[ 8] + x[ 9]*y[ 9] + x[10]*y[10] + x[11]*y[11]
         + x[12]*y[12] + x[13]*y[13] + x[14]*y[14] + x[15]*y[15];
}


void
Init_Dither ( Int bits, int shapingtype, Double dither )
{
    static Uint8_t        default_dither [] = { 92, 92, 88, 84, 81, 78, 74, 67,  0,  0 };
    static const Float*   F              [] = { F44_0, F44_1, F44_2, F44_3 };
    int                   index;

    if (bits > SAMPLE_SIZE)
        bits = SAMPLE_SIZE;

    if (shapingtype < 0) shapingtype = 0;
    if (shapingtype > 3) shapingtype = 3;
    index = bits - 11 - shapingtype;
    if (index < 0) index = 0;
    if (index > 9) index = 9;

    memset ( Dither.ErrorHistory , 0, sizeof (Dither.ErrorHistory ) );
    memset ( Dither.DitherHistory, 0, sizeof (Dither.DitherHistory) );

    Dither.FilterCoeff = F [shapingtype];
    Dither.Mask   = ((Uint64_t)-1) << (32 - bits);
#ifdef HAVE_IEEE754_DOUBLE
    Dither.Add    = 0.5     * ((1L << (32 - bits)) - 1);
#else
    Dither.Add    = 0.5     * ((1L << (32 - bits)) - 0);
#endif
    Dither.Dither = (dither >= 0.00 ? dither : 0.01*default_dither[index]) / (((Int64_t)1) << bits);
    Dither.NoShaping = shapingtype == 0;
}


void
Synthese_Filter_32_C ( register Int32_t* Stream, Int* const offset, Float* Vi, const Float Yi[][32], Uint channels, Uint channel )
{
    Int               n;
    register Int      k;
    Double            Sum;
    Double            Sum2;
    Int64_t           val;
    const Float       (*D)[16];
    const Float*      V;
#ifdef HAVE_IEEE754_DOUBLE
    Float64_t         doubletmp;
#endif

    ENTER(167);
    for ( n = 0; n < 36; n++, Yi++ ) {

        // shifting um 64 Indices aufwärts
        if ( (*offset -= 64) < 0 ) {
            *offset += 64*VIRT_SHIFT;
            ENTER(170);
            memmove ( Vi+64*VIRT_SHIFT, Vi, (16-1)*64*sizeof(*Vi) );
            LEAVE(170);
        }

        Calculate_New_V ( *Yi, (Float*)(V = Vi + *offset) );

        // vectoring & windowing & calculating PCM-Output
        D = Di_opt;
        for ( k = 0; k < 32; k++, V++, D++ ) {
            Sum = ( V [  0] * D [0][ 0]
                  + V [ 96] * D [0][ 1]
                  + V [128] * D [0][ 2]
                  + V [224] * D [0][ 3]
                  + V [256] * D [0][ 4]
                  + V [352] * D [0][ 5]
                  + V [384] * D [0][ 6]
                  + V [480] * D [0][ 7]
                  + V [512] * D [0][ 8]
                  + V [608] * D [0][ 9]
                  + V [640] * D [0][10]
                  + V [736] * D [0][11]
                  + V [768] * D [0][12]
                  + V [864] * D [0][13]
                  + V [896] * D [0][14]
                  + V [992] * D [0][15] ) * 65536.;

#if 1
            // Noise shaping and dithering
            if ( Dither.NoShaping ) {
                Double  tmp = Random_Equi ( Dither.Dither );
                Sum2 = tmp - Dither.LastRandomNumber [channel];
                Dither.LastRandomNumber [channel] = tmp;
                Sum2 = Sum += Sum2;
            }
            else {
                Sum2  = Random_Triangular ( Dither.Dither ) - scalar16 ( Dither.DitherHistory[channel], Dither.FilterCoeff + k );
                Sum  += Dither.DitherHistory [channel] [(-1-k)&15] = Sum2;
                Sum2  = Sum + scalar16 ( Dither.ErrorHistory [channel], Dither.FilterCoeff + k );
            }

            val = ROUND64 (Sum2)  &  Dither.Mask;
            if ( val != (Int32_t) val ) {
                memset ( Dither.ErrorHistory [channel], 0, sizeof (Dither.ErrorHistory[0]) );
                Dither.Overdrives++;
                if ( val > +Dither.MaxLevel ) Dither.MaxLevel = +val;
                if ( val < -Dither.MaxLevel ) Dither.MaxLevel = -val;
                val = (val >> 63) ^ 0x7FFFFFFF;
            }
            else {
                Dither.ErrorHistory [channel] [(-1-k)&15] = (Float)(Sum - val);
            }
#else
            // Noise shaping and dithering
            Sum2  = Random ( Dither.Dither ) - scalar16 ( Dither.DitherHistory[channel], Dither.FilterCoeff );
            memmove ( Dither.DitherHistory [channel] + 1, Dither.DitherHistory [channel] + 0, sizeof(Dither.DitherHistory[0]) - sizeof(Dither.DitherHistory[0][0]) );
            Dither.DitherHistory [channel] [0] = Sum2;

            Sum   = Sum + Dither.DitherHistory [channel] [0];
            Sum2  = Sum + scalar16 ( Dither.ErrorHistory [channel], Dither.FilterCoeff );
            val   = (Int64_t) (floor ( Sum2 + Dither.Add ))  &  Dither.Mask;
            if ( val != (Int32_t) val ) {
                memset ( Dither.ErrorHistory [channel], 0, sizeof (Dither.ErrorHistory[0]) );
                Dither.Overdrives++;
                if ( val > +Dither.MaxLevel ) Dither.MaxLevel = +val;
                if ( val < -Dither.MaxLevel ) Dither.MaxLevel = -val;
                val = (val >> 63) ^ 0x7FFFFFFF;
            }
            else {
                memmove ( Dither.ErrorHistory [channel] + 1, Dither.ErrorHistory [channel] + 0, sizeof(Dither.ErrorHistory[0]) - sizeof(Dither.ErrorHistory[0][0]) );
                Dither.ErrorHistory [channel] [0] = Sum - val;
            }
#endif

            // copy to PCM
            *Stream = (Int32_t)val;
            Stream += channels;
        }
    }
    LEAVE(167);
    return;
}

#endif /* defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT */


void
OverdriveReport ( void )
{
    if ( Dither.Overdrives > 0 ) {
#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT
        stderr_printf ( "\n%5lu Overdrive%s, maximum level %.1f, rerun with --scale %.5f\n",
                        (Ulong)Dither.Overdrives, Dither.Overdrives==1 ? "" : "s", (Double)(Int64_t)Dither.MaxLevel*(1/65536.), (Int64_t)(Dither.Mask & 0x7FFFFFFFL)/((Double)(Int64_t)Dither.MaxLevel+1) - 0.5e-5 );
#else
        stderr_printf ( "\n%5lu Overdrive%s, maximum level %lu, rerun with --scale %.5f\n",
                        (Ulong)Dither.Overdrives, Dither.Overdrives==1 ? "" : "s", (Ulong)Dither.MaxLevel, (Double)0x7FFF/(Dither.MaxLevel+1) - 0.5e-5 );
#endif
        Dither.Overdrives = 0;
        Dither.MaxLevel   = 0;
    }
}

/* end of synth.c */
