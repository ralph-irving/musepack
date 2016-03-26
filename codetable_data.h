/*
 *  Header file for Handling with Canonical Huffman Codes: Data Section
 *
 *  (C) Frank Klemm 2002. All rights reserved.
 *
 *  Designed for multiple include.
 *  You can generate code for encoder and decoder, for data definition and functions for data initialization.
 *
 *  Behaviour depends on setting of one of those 4 defines:
 *
 *  GENERATORMODE_DEFINE_ENCODER:
 *      Generates Huffman encoder tables (not the source table which is much smaller (8 bit per entry)
 *      and identically for encoder and decoder!)
 *      Must be done before including with GENERATORMODE_INIT_ENCODER to define and declare variables.
 *
 *  GENERATORMODE_DEFINE_DECODER:
 *      Generates Huffman decoder tables (not the source table which is much smaller (8 bit per entry)
 *      and identically for encoder and decoder!)
 *      Must be done before including with GENERATORMODE_INIT_DECODER to define and declare variables.
 *
 *  GENERATORMODE_INIT_ENCODER:
 *      Generates code for the function Init_Encoder_Huffman_Tables()
 *
 *  GENERATORMODE_INIT_DECODER:
 *      Generates code for the function Init_Decoder_Huffman_Tables()
 *
 *  none:
 *      Declarate prototypes of the
 *        - Huffman source data
 *        - Huffman encoder tables
 *        - Huffman decoder tables
 *        - functions Init_Decoder_Huffman_Tables() and Init_Encoder_Huffman_Tables()
 *
 *  These defines (GENERATORMODE_*) will be reset at the end of this file.
 *
 *  History:
 *    2002-08-01  created
 */


#ifndef MAXLUTBITS
# define MAXLUTBITS        10                   // Decoder Lookup tables can be up to 1024 bytes long
#endif

#include "codetable.h"

#ifndef _CONCAT2
# define _CONCAT2(x,y)    x##y
#endif
#ifndef _CONCAT3
# define _CONCAT3(x,y,z)  x##y##z
#endif



#if   defined GENERATORMODE_INIT_ENCODER /*************************************/

# define MAKE_TABLE_PROTOTYPE( x ) \
    _CONCAT2(Encodertable_,x) = MakeHuffEncTable ( & _CONCAT3 ( Coding_, x, _src) );

void
Init_Encoder_Huffman_Tables ( void )
{

#elif defined GENERATORMODE_INIT_DECODER /*************************************/

# define MAKE_TABLE_PROTOTYPE( x )  \
    _CONCAT2 (Decodertable_,x) = MakeHuffDecTable ( & _CONCAT3 (Coding_,x,_src), MAXLUTBITS );

void
Init_Decoder_Huffman_Tables ( void )
{

#elif defined GENERATORMODE_DEFINE_ENCODER /***********************************/

# define MAKE_TABLE_PROTOTYPE( x )  \
    extern const HuffSourceTable_t  _CONCAT3 (Coding_,x,_src);  \
    HuffEncTable_t                  _CONCAT2 (Encodertable_,x);

#elif defined GENERATORMODE_DEFINE_DECODER /***********************************/

# define MAKE_TABLE_PROTOTYPE( x )  \
    extern const HuffSourceTable_t  _CONCAT3 (Coding_,x,_src);  \
    HuffDecTable_t                  _CONCAT2 (Decodertable_,x);

#elif defined GENERATORMODE_ARRAY_ENCODER /************************************/

# define MAKE_TABLE_PROTOTYPE( x )  &_CONCAT2 (Encodertable_,x),

const HuffEncTable_t*  const Encodertables [] = {

#elif defined GENERATORMODE_ARRAY_DECODER /************************************/

# define MAKE_TABLE_PROTOTYPE( x )  &_CONCAT2 (Decodertable_,x),

const HuffDecTable_t*  const Decodertables [] = {

#else /************************************************************************/

# define MAKE_TABLE_PROTOTYPE( x )  \
    extern const HuffSourceTable_t  _CONCAT3 (Coding_,x,_src);  \
    extern       HuffDecTable_t     _CONCAT2 (Decodertable_,x); \
    extern       HuffEncTable_t     _CONCAT2 (Encodertable_,x);

void  Init_Decoder_Huffman_Tables ( void );
void  Init_Encoder_Huffman_Tables ( void );

#endif /**********************************************************************/

    MAKE_TABLE_PROTOTYPE ( MAXSUB )
    MAKE_TABLE_PROTOTYPE ( SCFI   )
    MAKE_TABLE_PROTOTYPE ( DSCF   )
    MAKE_TABLE_PROTOTYPE ( HDR0   )
    MAKE_TABLE_PROTOTYPE ( DHDR1  )
    MAKE_TABLE_PROTOTYPE ( DHDR   )
    MAKE_TABLE_PROTOTYPE ( Q1     )
    MAKE_TABLE_PROTOTYPE ( Q11    )
    MAKE_TABLE_PROTOTYPE ( Q12    )
    MAKE_TABLE_PROTOTYPE ( Q13    )
    MAKE_TABLE_PROTOTYPE ( Q14    )
    MAKE_TABLE_PROTOTYPE ( Q2     )
    MAKE_TABLE_PROTOTYPE ( Q21    )
    MAKE_TABLE_PROTOTYPE ( Q22    )
    MAKE_TABLE_PROTOTYPE ( Q23    )
    MAKE_TABLE_PROTOTYPE ( Q24    )
    MAKE_TABLE_PROTOTYPE ( Q3     )
    MAKE_TABLE_PROTOTYPE ( Q31    )
    MAKE_TABLE_PROTOTYPE ( Q32    )
    MAKE_TABLE_PROTOTYPE ( Q33    )
    MAKE_TABLE_PROTOTYPE ( Q34    )
    MAKE_TABLE_PROTOTYPE ( Q41    )
    MAKE_TABLE_PROTOTYPE ( Q42    )
    MAKE_TABLE_PROTOTYPE ( Q51    )
    MAKE_TABLE_PROTOTYPE ( Q52    )
    MAKE_TABLE_PROTOTYPE ( Q71    )
    MAKE_TABLE_PROTOTYPE ( Q72    )
    MAKE_TABLE_PROTOTYPE ( Q101   )
    MAKE_TABLE_PROTOTYPE ( Q102   )
    MAKE_TABLE_PROTOTYPE ( Q151   )
    MAKE_TABLE_PROTOTYPE ( Q152   )
    MAKE_TABLE_PROTOTYPE ( Q311   )
    MAKE_TABLE_PROTOTYPE ( Q312   )
    MAKE_TABLE_PROTOTYPE ( BIGT   )
    MAKE_TABLE_PROTOTYPE ( BIG1   )
    MAKE_TABLE_PROTOTYPE ( BIG2   )
    MAKE_TABLE_PROTOTYPE ( BIG3   )
    MAKE_TABLE_PROTOTYPE ( BIG4   )
    MAKE_TABLE_PROTOTYPE ( BIGV1  )
    MAKE_TABLE_PROTOTYPE ( BIGV2  )
    MAKE_TABLE_PROTOTYPE ( BIGV3  )


#if defined GENERATORMODE_INIT_ENCODER  ||  defined GENERATORMODE_INIT_DECODER
}
#endif

#if defined GENERATORMODE_ARRAY_ENCODER  ||  defined GENERATORMODE_ARRAY_DECODER
    NULL,
};
#endif


#undef MAKE_TABLE_PROTOTYPE
#undef GENERATORMODE_INIT_ENCODER
#undef GENERATORMODE_INIT_DECODER
#undef GENERATORMODE_DEFINE_ENCODER
#undef GENERATORMODE_DEFINE_DECODER

/* end of codetable_data.h */
