// c00000 4B400000
/*
 *  New Quantization Routines without inverse quantization
 *
 *  (C) Frank Klemm 2002. All rights reserved.
 *
 *  Principles:
 *
 *  History:
 *    2002-08-13        created
 *
 *  Global functions:
 *    -
 *
 *  TODO:
 *    -
 */

#include "mppenc.h"


float
NoiseInjectionComp ( float SMR )
{
    return 1.f;
}


float
NoiseEstimator36 ( const float*  input,
                   const float   Scaler )
{
    float  invScaler = 1. / Scaler;
    float  error     = 1.e-37f;
    float  tmp;
    int    k;

    for ( k = 0; k < 36; k++ ) {
        tmp    = input[k] * invScaler + 0xFF8000;                                  // q = ftol(in), korrektes Runden
        tmp    = (*(int*) & tmp - 0x4B7F8000) * Scaler - input[k];
        error += tmp * tmp;
    }

    return error;
}


float
NoiseEstimator12 ( const float*  input,
                   const float   Scaler )
{
    float  invScaler = 1. / Scaler;
    float  error     = 1.e-37f;
    float  tmp;
    int    k;

    for ( k = 0; k < 12; k++ ) {
        tmp    = input[k] * invScaler + 0xFF8000;                                  // q = ftol(in), korrektes Runden
        tmp    = (*(int*) & tmp - 0x4B7F8000) * Scaler - input[k];
        error += tmp * tmp;
    }

    return error;
}


float
NoiseEstimator36_ANS ( const float*  input,
                       const float   Scaler,
                       float         feedback [6],
                       float         qerrors  [6 + 36] )
{
    float  invScaler = 1. / Scaler;
        float  error     = 1.e-37f;
        float  tmp;
        int    k;

    for ( k = 0; k < 36; k++ ) {
        tmp = input[k] * invScaler + 0xFF8000;                                  // q = ftol(in), korrektes Runden
        tmp = (*(int*) & tmp - 0x4B7F8000) * Scaler - input[k];
                error += tmp * tmp;
    }

    return error;
}


float
NoiseEstimator12_ANS ( const float*  input,
                       const float   Scaler,
                       float         feedback [2],
                       float         qerrors  [2 + 12] )
{
    float  invScaler = 1. / Scaler;
    float  error     = 1.e-37f;
    float  tmp;
    int    k;

    for ( k = 0; k < 12; k++ ) {
        tmp    = input[k] * invScaler + 0xFF8000;                                  // q = ftol(in), korrektes Runden
        tmp    = (*(int*) & tmp - 0x4B7F8000) * Scaler - input[k];
        error += tmp * tmp;
    }

    return error;
}


void
QuantSubband12     ( int*          output,
                     const float*  input,
                     const float   Scaler,
                     float*        qerrors )
{
}


void
QuantSubband12_ANS ( int*          output,
                     const float*  input,
                     const float   Scaler,
                     float*        qerrors,
                     const float*  FIR )
{
}

/* end of quantnew.c */
