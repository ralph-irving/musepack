/*
 *  Header file for Handling with Canonical Huffman Codes: Basic Section
 *
 *  (C) Frank Klemm 2002. All rights reserved.
 *
 *  - Defines types needed for encoding and decoding huffman codes:
 *      HuffEncCode_t, HuffDecCode_t, HuffSourceTable_t
 *
 *  - Prototypes of function generating encoder and decoder objects
 *    (HuffEncCode_t, HuffDecCode_t) from source tables (HuffSourceTable_t):
 *      MakeHuffEncTable(), Init_Encoder_Huffman_Tables(),
 *      MakeHuffDecTable(), Init_Decoder_Huffman_Tables()
 *
 *  - Some useful macros:
 *      MAKE_TABLE_VEC( name )
 *      MAKE_TABLE_SRC( name, min_elem, max_bits )
 *      WriteHuffman( name, value )
 *      WriteHuffman2( tabpre, value )
 *      WriteHuffman3( tabpre, value )
 *
 *  History:
 *    2002-08-01  created
 */

#ifndef MPP_CODETABLE_H
#define MPP_CODETABLE_H

#define COUNT_TABLE_USAGE               // Count usage of the table codes !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#include <stddef.h>
#include "types.h"

#ifndef _CONCAT2
# define _CONCAT2(x,y)    x##y
#endif
#ifndef _CONCAT3
# define _CONCAT3(x,y,z)  x##y##z
#endif


////// Huffman Code source seeds ////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    const unsigned char*        bitlength;      // List of the length (in bits) of the Huffman codes H(0)...H(tablen-1)
    const unsigned short        tablelen;       // Number of Huffman codes
    const signed short          firstval;       // numeric value encoded with H(0); H(tablen-1) encodes firstval+tablelen-1
    const unsigned char         maxlen;         // Number of bits of the longest Huffman code
    const char* const           tablename;      // name of the table
} HuffSourceTable_t;


#define STRINGIFY(x)    #x

#define MAKE_TABLE_SRC( name, min_elem, max_bits ) \
    extern const unsigned char  _CONCAT3 ( Coding_, name, _bits ) []; \
    const HuffSourceTable_t     _CONCAT3 ( Coding_, name, _src  ) = {                           \
        (_CONCAT3 ( Coding_, name, _bits )),                                                    \
        sizeof(_CONCAT3 ( Coding_, name, _bits ))/sizeof(*(_CONCAT3 ( Coding_, name, _bits ))), \
        (min_elem),                                                                             \
        (max_bits),                                                                             \
        STRINGIFY (name)                                                                        \
    }

#define MAKE_TABLE_VEC( name ) \
    static const unsigned char  _CONCAT3 ( Coding_, name, _bits ) [] =


////// Encoder //////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned int          pattern;          // up to 32 bit, LSB adjusted: pattern < (1LU << bits)
    unsigned int          bits;             // up to  6 bit, 1 <= bits <= 32
#ifdef COUNT_TABLE_USAGE
    unsigned long         usage;
#endif
} HuffEncCode_t;


typedef struct {
    const HuffEncCode_t*  table;
    const HuffEncCode_t*  table_no_offs;
    //
    const unsigned char*  bitlength;
    size_t                tablelen;
    int                   firstval;
    int                   lastval;
    int                   maxlen;
    const char*           tablename;
#ifdef COUNT_TABLE_USAGE
    Uint64_t              bits0;
    Uint64_t              bits1;
    Uint32_t              usage;
#endif
} HuffEncTable_t;

typedef HuffEncTable_t*   HuffEncTablePtr_t;


extern const HuffEncTable_t*  const Encodertables [];
void            Init_Encoder_Huffman_Tables ( void );
HuffEncTable_t  MakeHuffEncTable            ( const HuffSourceTable_t* src );


#ifdef COUNT_TABLE_USAGE ////////////////////////////////////////////////////////

# define WriteHuffman( name, value )                                            \
{                                                                               \
    const HuffEncCode_t*  p = _CONCAT2 (Encodertable_,name).table + (value);    \
    WriteBits ( p -> pattern, p -> bits );                                      \
    (((HuffEncCode_t*)p) -> usage)++;                                           \
}

# define WriteHuffman2( tabptr, value )                                         \
{                                                                               \
    const HuffEncCode_t*  p = (tabptr -> table) + (value);                      \
    WriteBits ( p -> pattern, p -> bits );                                      \
    (((HuffEncCode_t*)p) -> usage)++;                                           \
}

# define WriteHuffman3( tabptr, value )                                         \
{                                                                               \
    const HuffEncCode_t*  p = (tabptr -> table_no_offs) + (value);              \
    WriteBits ( p -> pattern, p -> bits );                                      \
    (((HuffEncCode_t*)p) -> usage)++;                                           \
}

#else ///////////////////////////////////////////////////////////////////////////

# define WriteHuffman( name, value )                                            \
{                                                                               \
    const HuffEncCode_t*  p = _CONCAT2 (Encodertable_,name).table + (value);    \
    WriteBits ( p -> pattern, p -> bits );                                      \
}

# define WriteHuffman2( tabptr, value )                                         \
{                                                                               \
    const HuffEncCode_t*  p = (tabptr -> table) + (value);                      \
    WriteBits ( p -> pattern, p -> bits );                                      \
}

# define WriteHuffman3( tabptr, value )                                         \
{                                                                               \
    const HuffEncCode_t*  p = (tabptr -> table_no_offs) + (value);              \
    WriteBits ( p -> pattern, p -> bits );                                      \
}

#endif //////////////////////////////////////////////////////////////////////////

////// Decoder //////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned int          pattern;          // 32 bit
    signed short          value;            // 16 bit
    unsigned char         bits;             //  6 bit
} HuffDecCode_t;


typedef struct {
    const HuffDecCode_t*  table;
    const unsigned char*  lut;
    int                   lutbits;
    int                   nolutbits;
    //
    const unsigned char*  bitlength;
    size_t                tablelen;
    int                   firstval;
    int                   maxlen;
    const char*           tablename;
#ifdef COUNT_TABLE_USAGE
    Uint64_t              bits0;
    Uint64_t              bits1;
    Uint32_t              usage;
#endif
} HuffDecTable_t;


extern const HuffDecTable_t*  const Decodertables [];
HuffDecTable_t  MakeHuffDecTable            ( const HuffSourceTable_t* src, unsigned int fastdecodebits );
void            Init_Decoder_Huffman_Tables ( void );


#ifdef COUNT_TABLE_USAGE
void            Advantage                   ( HuffEncTable_t* const p, unsigned int bits0, unsigned int bits1, unsigned int coded_values );
#else
# define Advantage(p,x,y,z)
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif /* MPP_CODETABLE_H */
