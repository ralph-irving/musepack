/*
 *  Clear Vocal Detection functions
 *
 *  (C) Andree Buschmann 2000, Frank Klemm 2001,02. All rights reserved.
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


/*
 *  A list of different mixed tools
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Requantize_MidSideStereo(), Requantize_IntensityStereo()
 *      Requantisation of quantized samples for synthesis filter
 *  Resort_HuffTable(), Make_HuffTable(), Make_LookupTable()
 *      Generating and sorting Huffman tables, making fast lookup tables
 */

#include <string.h>
#include <errno.h>
#include "mppdec.h"


#if defined HAVE_INCOMPLETE_READ  &&  FILEIO != 1

size_t
complete_read ( int fd, void* dest, size_t bytes )
{
    size_t  bytesread = 0;
    size_t  ret;

    while ( bytes > 0 ) {
#if defined _WIN32  &&  defined USE_HTTP  &&  !defined MPP_ENCODER
        ret = fd & 0x4000  ?  recv ( fd & 0x3FFF, dest, bytes, 0)  :  read ( fd, dest, bytes );
#else
        ret = read ( fd, dest, bytes );
#endif
        if ( ret == 0  ||  ret == (size_t)-1 )
            break;
        dest       = (void*)(((char*)dest) + ret);
        bytes     -= ret;
        bytesread += ret;
    }
    return bytesread;
}

#endif


#ifndef MPP_ENCODER

/*
 *  This is the main requantisation routine which does the following things:
 *
 *      - rescaling the quantized values (int) to their original value (float)
 *      - recalculating both stereo channels for MS stereo
 *
 *  See also: Requantize_IntensityStereo()
 *
 *  For performance reasons all cases are programmed separately and the code
 *  is unrolled.
 *
 *  Input is:
 *      - Stop_Band:
 *          the last band using MS or LR stereo
 *      - used_MS[Band]:
 *          MS or LR stereo flag for every band (0...Stop_Band), Value is 1
 *          for MS and 0 for LR stereo.
 *      - Res [0..CH-1] [Band]:
 *          Quantisation resolution for every band (0...Stop_Band) and
 *          channels (L, R). Value is 0...17.
 *      - SCF_Index [0..CH-1] [Band] [3]:
 *          Scale factor for every band (0...Stop_Band), subframe (0...2)
 *          and channel (L, R).
 *      - QQ [0..CH-1] [Band] [36]:
 *          36 subband samples for every band (0...Stop_Band) and channel (L, R).
 *      - SCF[64], Cc7[18], Dc7[18]:
 *          Lookup tables for Scale factor and Quantisation resolution.
 *
 *   Output is:
 *     - YY [0]:  Left  channel subband signals
 *     - YY [1]:  Right channel subband signals
 *
 *   These signals are used for the synthesis filter in the synth*.[ch]
 *   files to generate the PCM output signal.
 */


static void
Requantize_MidSideStereo_SV7 ( Int Stop_Band, const Bool_t* used_MS )
{
    Int    Band;  // 0...Stop_Band
    Uint   k;     // 0...35
    Float  ML;
    Float  MR;
    Float  mid;
    Float  side;

    ENTER(162);

    for ( Band = 0; Band <= Stop_Band; Band++ ) {

        ///////////////////////////////////////////////////////////////////////////////////

        if ( used_MS[Band] ) {  // MidSide coded: left channel contains Mid signal, right channel Side signal
            if ( Res[0][Band] ) {
                if      ( Res[1][Band] < -1 ) {
                    k  = 0;
                    ML = +SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]];
                    MR = -SCF[SCF_Index[1][Band][0]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = +SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]];
                    MR = -SCF[SCF_Index[1][Band][1]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = +SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]];
                    MR = -SCF[SCF_Index[1][Band][2]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                else if ( Res[1][Band] != 0 ) {     //  M!=0, S!=0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][0]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = (mid = QQ [0] [Band] [k] * ML) - (side = QQ [1] [Band] [k] * MR);
                        YY [0][k][Band] = mid + side;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][1]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = (mid = QQ [0] [Band] [k] * ML) - (side = QQ [1] [Band] [k] * MR);
                        YY [0][k][Band] = mid + side;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][2]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = (mid = QQ [0] [Band] [k] * ML) - (side = QQ [1] [Band] [k] * MR);
                        YY [0][k][Band] = mid + side;
                    } while (++k < 36);
                }
                else {                 //  M!=0, S=0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]];
                    do {
                        YY [1][k][Band] =
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]];
                    do {
                        YY [1][k][Band] =
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]];
                    do {
                        YY [1][k][Band] =
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
            }
            else {
                if      ( Res[1][Band] < -1 ) {
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
                else if ( Res[1][Band] != 0 ) {     //  M==0, S!=0
                    k  = 0;
                    ML = SCF[SCF_Index[1][Band][0]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = - (
                        YY [0][k][Band] = QQ [1] [Band] [k] * ML );
                    } while (++k < 12);
                    ML = SCF[SCF_Index[1][Band][1]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = - (
                        YY [0][k][Band] = QQ [1] [Band] [k] * ML );
                    } while (++k < 24);
                    ML = SCF[SCF_Index[1][Band][2]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = - (
                        YY [0][k][Band] = QQ [1] [Band] [k] * ML );
                    } while (++k < 36);
                }
                else {                 //  M==0, S==0
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
            }

        }

        else {                  // Left/Right coded: left channel contains left, right the right signal

            if ( Res[0][Band] ) {
                if      ( Res[1][Band] < -1 ) {
                    k  = 0;
                    ML = +SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]];
                    MR = +SCF[SCF_Index[1][Band][0]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = +SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]];
                    MR = +SCF[SCF_Index[1][Band][1]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = +SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]];
                    MR = +SCF[SCF_Index[1][Band][2]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                else if ( Res[1][Band] != 0 ) {     //  L!=0, R!=0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][0]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][1]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][2]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                else {                 //  L!=0, R==0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]];
                    do {
                        YY [1][k][Band] = 0.f;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]];
                    do {
                        YY [1][k][Band] = 0.f;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]];
                    do {
                        YY [1][k][Band] = 0.f;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                        }
            else {
                if      ( Res[1][Band] < -1 ) {
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
                else if ( Res[1][Band] != 0 ) {     //  L==0, R!=0
                    k  = 0;
                    MR = SCF[SCF_Index[1][Band][0]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = 0.f;
                    } while (++k < 12);
                    MR = SCF[SCF_Index[1][Band][1]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = 0.f;
                    } while (++k < 24);
                    MR = SCF[SCF_Index[1][Band][2]] * Cc7[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = 0.f;
                    } while (++k < 36);
                }
                else {                 //  L==0, R==0
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
            }

        }
        ///////////////////////////////////////////////////////////////////////////////////////////

    }

    LEAVE(162);
    return;
}


static void
Requantize_MidSideStereo_SV7F ( Int Stop_Band, const Bool_t* used_MS )
{
    Int    Band;  // 0...Stop_Band
    Uint   k;     // 0...35
    Float  ML;
    Float  MR;
    Float  mid;
    Float  side;

    ENTER(162);

    for ( Band = 0; Band <= Stop_Band; Band++ ) {

        if ( used_MS[Band] ) {  // MidSide coded: left channel contains Mid signal, right channel Side signal
            if ( Res[0][Band] ) {
                if      ( Res[1][Band] < 0 ) {
                    k  = 0;
                    ML = +SCF[SCF_Index[0][Band][0]] * Cc8[Res[0][Band]];
                    MR = -SCF[SCF_Index[1][Band][0]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = +SCF[SCF_Index[0][Band][1]] * Cc8[Res[0][Band]];
                    MR = -SCF[SCF_Index[1][Band][1]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = +SCF[SCF_Index[0][Band][2]] * Cc8[Res[0][Band]];
                    MR = -SCF[SCF_Index[1][Band][2]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                else if ( Res[1][Band] != 0 ) {     //  M!=0, S!=0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]-2] * Cc8[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][0]-2] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = (mid = QQ [0] [Band] [k] * ML) - (side = QQ [1] [Band] [k] * MR);
                        YY [0][k][Band] = mid + side;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]-2] * Cc8[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][1]-2] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = (mid = QQ [0] [Band] [k] * ML) - (side = QQ [1] [Band] [k] * MR);
                        YY [0][k][Band] = mid + side;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]-2] * Cc8[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][2]-2] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = (mid = QQ [0] [Band] [k] * ML) - (side = QQ [1] [Band] [k] * MR);
                        YY [0][k][Band] = mid + side;
                    } while (++k < 36);
                }
                else {                 //  M!=0, S=0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]-2] * Cc8[Res[0][Band]];
                    do {
                        YY [1][k][Band] =
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]-2] * Cc8[Res[0][Band]];
                    do {
                        YY [1][k][Band] =
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]-2] * Cc8[Res[0][Band]];
                    do {
                        YY [1][k][Band] =
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
            }
            else {
                if      ( Res[1][Band] < 0 ) {
                    for (k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
                else if ( Res[1][Band] != 0 ) {     //  M==0, S!=0
                    k  = 0;
                    ML = SCF[SCF_Index[1][Band][0]-2] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = - (
                        YY [0][k][Band] = QQ [1] [Band] [k] * ML );
                    } while (++k < 12);
                    ML = SCF[SCF_Index[1][Band][1]-2] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = - (
                        YY [0][k][Band] = QQ [1] [Band] [k] * ML );
                    } while (++k < 24);
                    ML = SCF[SCF_Index[1][Band][2]-2] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = - (
                        YY [0][k][Band] = QQ [1] [Band] [k] * ML );
                    } while (++k < 36);
                }
                else {                 //  M==0, S==0
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
            }

        }
        ///////////////////////////////////////////////////////////////////////////////////////////////
        else {                  // Left/Right coded: left channel contains left, right the right signal

            if ( Res[0][Band] ) {
                if      ( Res[1][Band] < 0 ) {
                    k  = 0;
                    ML = +SCF[SCF_Index[0][Band][0]] * Cc8[Res[0][Band]];
                    MR = +SCF[SCF_Index[1][Band][0]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = +SCF[SCF_Index[0][Band][1]] * Cc8[Res[0][Band]];
                    MR = +SCF[SCF_Index[1][Band][1]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = +SCF[SCF_Index[0][Band][2]] * Cc8[Res[0][Band]];
                    MR = +SCF[SCF_Index[1][Band][2]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                else if ( Res[1][Band] != 0 ) {     //  L!=0, R!=0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]] * Cc8[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][0]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]] * Cc8[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][1]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]] * Cc8[Res[0][Band]];
                    MR = SCF[SCF_Index[1][Band][2]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
                else {                 //  L!=0, R==0
                    k  = 0;
                    ML = SCF[SCF_Index[0][Band][0]] * Cc8[Res[0][Band]];
                    do {
                        YY [1][k][Band] = 0.f;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 12);
                    ML = SCF[SCF_Index[0][Band][1]] * Cc8[Res[0][Band]];
                    do {
                        YY [1][k][Band] = 0.f;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 24);
                    ML = SCF[SCF_Index[0][Band][2]] * Cc8[Res[0][Band]];
                    do {
                        YY [1][k][Band] = 0.f;
                        YY [0][k][Band] = QQ [0] [Band] [k] * ML;
                    } while (++k < 36);
                }
            }
            else {
                if      ( Res[1][Band] < 0 ) {
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
                else if ( Res[1][Band] != 0 ) {     //  L==0, R!=0
                    k  = 0;
                    MR = SCF[SCF_Index[1][Band][0]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = 0.f;
                    } while (++k < 12);
                    MR = SCF[SCF_Index[1][Band][1]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = 0.f;
                    } while (++k < 24);
                    MR = SCF[SCF_Index[1][Band][2]] * Cc8[Res[1][Band]];
                    do {
                        YY [1][k][Band] = QQ [1] [Band] [k] * MR;
                        YY [0][k][Band] = 0.f;
                    } while (++k < 36);
                }
                else {                 //  L==0, R==0
                    for ( k = 0; k < 36; k++ ) {
                        YY [1][k][Band] =
                        YY [0][k][Band] = 0.f;
                    }
                }
            }
        }
    }

    LEAVE(162);
    return;
}


void
Requantize_MidSideStereo ( Int Stop_Band, const Bool_t* used_MS, unsigned int StreamVersion )
{
    if ( StreamVersion & 0x08 )
        Requantize_MidSideStereo_SV7F ( Stop_Band, used_MS );
    else
        Requantize_MidSideStereo_SV7  ( Stop_Band, used_MS );
}


/*
 *  This is the main requantisation routine for Intensity Stereo.
 *  It does the same as Requantize_MidSideStereo() but for IS.
 *
 *  Input is:
 *      - Stop_Band:
 *          the last band using MS or LR stereo
 *      - Res[0][Band]:
 *          Quantisation resolution for every band (0...Stop_Band) and
 *          the left channel which is used for both channels. Value is 0...17.
 *      - SCF_Index[3][Band].{L,R}:
 *          Scale factor for every band (0...Stop_Band), subframe (0...2)
 *          and channel (L, R).
 *      - QQ [0] [Band] [36]
 *          36 subband samples for every band (0...Stop_Band), both channels use
 *          the of the left channel
 *      - SCF[64], Cc7[18], Dc7[18]:
 *          Lookup tables for Scale factor and Quantisation resolution.
 *
 *   Output is:
 *     - YY [0]:  Left  channel subband signals
 *     - YY [1]:  Right channel subband signals
 *
 *   These signals are used for the synthesis filter in the synth*.[ch]
 *   files to generate the PCM output signal.
 */

void
Requantize_IntensityStereo ( Int Start_Band, Int Stop_Band )
{
    Int    Band;  // Start_Band...Stop_Band
    Uint   k;     // 0...35
    Float  ML;
    Float  MR;

    ENTER(163);

    for ( Band = Start_Band; Band <= Stop_Band; Band++ ) {

        if ( Res[0][Band] ) {
            k  = 0;
            ML = SCF[SCF_Index[0][Band][0]] * Cc7[Res[0][Band]] * SS05;
            MR = SCF[SCF_Index[1][Band][0]] * Cc7[Res[0][Band]] * SS05;
            do {
                YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                YY [0][k][Band] = QQ [0] [Band] [k] * ML;
            } while (++k < 12);
            ML = SCF[SCF_Index[0][Band][1]] * Cc7[Res[0][Band]] * SS05;
            MR = SCF[SCF_Index[1][Band][1]] * Cc7[Res[0][Band]] * SS05;
            do {
                YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                YY [0][k][Band] = QQ [0] [Band] [k] * ML;
            } while (++k < 24);
            ML = SCF[SCF_Index[0][Band][2]] * Cc7[Res[0][Band]] * SS05;
            MR = SCF[SCF_Index[1][Band][2]] * Cc7[Res[0][Band]] * SS05;
            do {
                YY [1][k][Band] = QQ [0] [Band] [k] * MR;
                YY [0][k][Band] = QQ [0] [Band] [k] * ML;
            } while (++k < 36);
        }
        else {
            for ( k = 0; k < 36; k++ ) {
                YY [1][k][Band] =
                YY [0][k][Band] = 0.f;
            }
        }

    }
    LEAVE(163);
    return;
}


/*
 *  Helper function for the qsort() in Resort_HuffTable() to sort a Huffman table
 *  by its codes.
 */

static int Cdecl
cmp_fn ( const void* p1, const void* p2 )
{
    if ( ((const Huffman_t*)p1) -> Code < ((const Huffman_t*)p2) -> Code ) return +1;
    if ( ((const Huffman_t*)p1) -> Code > ((const Huffman_t*)p2) -> Code ) return -1;
    return 0;
}


/*
 *  This functions sorts a Huffman table by its codes. It has also two other functions:
 *
 *    - The table contains LSB aligned codes, these are first MSB aligned.
 *    - The value entry is filled by its position plus 'offset' (Note that
 *      Make_HuffTable() don't fill this item. Offset can be used to offset
 *      range for instance from 0...6 to -3...+3.
 *
 *  Note, that this function generates trash if you call it twice!
 */

void
Resort_HuffTable ( Huffman_t* const Table, const size_t elements, Int offset )
{
    size_t  i;

    for ( i = 0; i < elements; i++ ) {
        Table[i].Value  = i + offset;
        Table[i].Code <<= (32 - Table[i].Length);
    }

    qsort ( Table, elements, sizeof(*Table), cmp_fn );
    return;
}

#endif /* MPP_ENCODER */


/*
 *  Fills out the items Code and Length (but not Value) of a Huffman table
 *  from a bit packed Huffman table 'src'. Table is not sorted, so this is
 *  the table which is suitable for an encoder. Be carefully: To get a table
 *  usable for a decoder you must use Resort_HuffTable() after this
 *  function. It's a little bit dangerous to divide the functionality, may
 *  be there is a more secure and handy solution of this problem.
 */

void
Make_HuffTable ( Huffman_t* dst, const HuffSrc_t* src, size_t len )
{
    size_t  i;

    for ( i = 0; i < len; i++,src++,dst++ ) {
        dst->Code   = src->Code  ;
        dst->Length = src->Length;
    }
}


/*
 *  Generates a Lookup table for quick Huffman decoding. This table must
 *  have a size of a power of 2. Input is the pre-sorted Huffman table,
 *  sorted by Resort_HuffTable() and its length, and the length of the
 *  lookup table. Output is the Lookup table. It can be used for table based
 *  decoding (Huffman_decode_fastest) which fully decodes by means of the
 *  LUT.  This is only handy for small huffman codes up to 9...10 bit
 *  maximum length.  For longer codes partial lookup is possible with
 *  Huffman_decode_faster() which first estimates possibles codes by means
 *  of LUT and then searches the exact code like the tableless version
 *  Huffman_decode().
 */

void
Make_LookupTable ( Uint8_t* LUT, size_t LUT_len, const Huffman_t* const Table, const size_t elements )
{
    size_t    i;
    size_t    idx  = elements;
    Uint32_t  dval = (Uint32_t)0x80000000L / LUT_len * 2;
    Uint32_t  val  = dval - 1;

    for ( i = 0; i < LUT_len; i++, val += dval ) {
        while ( idx > 0  &&  val >= Table[idx-1].Code )
            idx--;
        *LUT++ = (Uint8_t)idx;
    }

    return;
}

/* end of tools.c */
