/*
 *
 *  Decoder Huffman tables for SV4...6
 *
 *
 *  (C) Copyright 1999-2001  Andree Buschmann. All rights reserved.
 *  (C) Copyright 2001-2002  Frank Klemm. All rights reserved.
 *
 *  For licencing see the licencing file which is part of this package.
 *
 *
 *  Principles:
 *    Original AB encoder/decoder hat a big function Init_Huffman_Encoder_SV4_6, which
 *    filled big tables of the type Huffman_t element by element using a lot of simple code lines.
 *    This tables had the form you need for the encoder:
 *       - length_of_huffman_code
 *       - huffman_code_right_adjusted
 *     Example for Table[1]
 *       - BitCount = 5
 *       - BitValue = 0x1A
 *     These parameters were used to write the bits 11010.
 *     For the encoder the table is sorted by the encoded value. So you write the bits by
 *       - WriteBits ( Table[Code].BitValue, Table[Code].BitCount );
 *     For the decoder the
 *        - Huffman codes are left adjusted
 *        - The table is sorted by the codes in descending order
 *        - Because the table is reordered, the original code must be additional stored in the table
 *     Example for Table[1]
 *       - Code     = 1
 *       - BitValue = 0xD0000000
 *       - BitCount = 5
 *
 *  History:
 *    1999-2002     programmed by Frank Klemm and Andree Buschmann
 *    2003-02-03    the file got an header
 *
 *  Global functions:
 *    - Init_Huffman_Decoder_SV4_6 ()
 *    - some global variables, which are initialized once and which are only read from outside
 *
 *  TODO:
 *   - because there are no changes in SV4...6 anymore and because these tables are not used by the encoder anymore,
 *     this code can be replaced by some fixed tables.
 *   - For portable players with lot of DRAM and little Flash RAM this may be suboptimal, because you need much more
 *     Flash RAM, although total cost of DRAM+Flash RAM decreases.
 *
 */


#include "mppdec.h"


// Huffman tables, generated from the compact form in HuffSrc_t
static Huffman_t  Entropie_1    [ 3];
static Huffman_t  Entropie_2    [ 5];
static Huffman_t  Entropie_3    [ 7];
static Huffman_t  Entropie_4    [ 9];
static Huffman_t  Entropie_5    [15];
static Huffman_t  Entropie_6    [31];
static Huffman_t  Entropie_7    [63];
static Huffman_t  Region_A      [16];
static Huffman_t  Region_B      [ 8];
static Huffman_t  Region_C      [ 4];
Huffman_t         SCFI_Bundle   [ 8];
Huffman_t         DSCF_Entropie [13];

const Huffman_t*  Entropie      [18] = {
    NULL      , Entropie_1, Entropie_2, Entropie_3, Entropie_4, Entropie_5,
    Entropie_6, Entropie_7, NULL      , NULL      , NULL      , NULL      ,
    NULL      , NULL      , NULL      , NULL      , NULL      , NULL      ,
};

const Huffman_t*  Region        [32] = {
    Region_A, Region_A, Region_A, Region_A, Region_A, Region_A, Region_A, Region_A,
    Region_A, Region_A, Region_A, Region_B, Region_B, Region_B, Region_B, Region_B,
    Region_B, Region_B, Region_B, Region_B, Region_B, Region_B, Region_B, Region_C,
    Region_C, Region_C, Region_C, Region_C, Region_C, Region_C, Region_C, Region_C
};


// Huffman source tables
static const HuffSrc_t   SCFI_Bundle_src [8] = {
    { 11, 6 }, { 7, 5 }, { 6, 5 }, { 1, 2 }, { 4, 5 }, { 0, 3 }, { 10, 6 }, { 1, 1 },
};

static const HuffSrc_t   Region_A_src   [16] = {
    {   2, 3 }, {   1, 1 }, {   0,  2 }, {  15,  5 }, {   29,  6 }, {   13,  5 }, {   12,  5 }, {   57,  7 },
    { 113, 8 }, { 225, 9 }, { 449, 10 }, { 897, 11 }, { 1793, 12 }, { 3585, 13 }, { 7169, 14 }, { 7168, 14 },
};

static const HuffSrc_t   Region_B_src   [ 8] = {
    { 1, 2 }, { 1, 1 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 1, 7 }, { 0, 7 },
};

static const HuffSrc_t   Region_C_src   [ 4] = {
    { 1, 1 }, { 1, 2 }, { 1, 3 }, { 0, 3 },
};

static const HuffSrc_t   DSCF_Entropie_src [13] = {
    { 20, 6 }, { 11, 5 }, {  4, 4 }, { 24, 5 }, { 11, 4 }, {  4, 3 }, { 0, 2 },
    {  7, 3 }, {  3, 3 }, { 13, 4 }, { 10, 4 }, { 25, 5 }, { 21, 6 },
};

static const HuffSrc_t   Entropie_1_src [ 3] = {
    { 1, 2 }, { 1, 1 }, { 0, 2 },
};

static const HuffSrc_t   Entropie_2_src [ 5] = {
    { 4, 3 }, { 0, 2 }, { 3, 2 }, { 1, 2 }, { 5, 3 },
};

static const HuffSrc_t   Entropie_3_src [ 7] = {
    { 17, 5 }, { 5, 3 }, { 1, 2 }, { 3, 2 }, { 0, 2 }, { 9, 4 }, { 16, 5 },
};

static const HuffSrc_t   Entropie_4_src [ 9] = {
    { 5, 4 }, { 14, 4 }, { 3, 3 }, { 5, 3 }, { 0, 2 }, { 6, 3 }, { 4, 3 }, { 15, 4 }, { 4, 4 },
};

static const HuffSrc_t   Entropie_5_src [15] = {
    { 57, 6 }, { 23, 5 }, { 29, 5 }, {  3, 4 }, { 13, 4 }, { 15, 4 }, {  2, 3 }, { 4, 3 },
    {  3, 3 }, {  0, 3 }, { 12, 4 }, { 10, 4 }, {  2, 4 }, { 22, 5 }, { 56, 6 },
};

static const HuffSrc_t   Entropie_6_src [31] = {
    {  8, 7 }, { 88, 7 }, { 40, 6 }, {  5, 6 }, { 45, 6 }, { 56, 6 }, {  9, 5 }, { 23, 5 },
    { 24, 5 }, { 27, 5 }, { 29, 5 }, { 31, 5 }, {  3, 4 }, {  2, 4 }, {  7, 4 }, {  9, 4 },
    {  8, 4 }, {  5, 4 }, {  6, 4 }, {  0, 4 }, { 30, 5 }, { 26, 5 }, { 25, 5 }, { 21, 5 },
    {  3, 5 }, { 57, 6 }, { 41, 6 }, { 17, 6 }, { 16, 6 }, { 89, 7 }, {  9, 7 },
};

static const HuffSrc_t   Entropie_7_src [63] = {
    {  17, 8 }, { 151, 8 }, { 212, 8 }, {   9, 7 }, {  80, 7 }, {  10, 7 }, { 11, 7 }, { 63, 7 },
    {  89, 7 }, {  97, 7 }, { 107, 7 }, { 126, 7 }, {  18, 6 }, {  36, 6 }, { 38, 6 }, { 42, 6 },
    {  43, 6 }, {  45, 6 }, {  51, 6 }, {  50, 6 }, {  56, 6 }, {  57, 6 }, { 60, 6 }, { 61, 6 },
    {   6, 5 }, {   0, 5 }, {   8, 5 }, {   7, 5 }, {  14, 5 }, {  10, 5 }, { 17, 5 }, { 11, 5 },
    {  16, 5 }, {   5, 5 }, {  13, 5 }, {   4, 5 }, {  12, 5 }, {   1, 5 }, {  3, 5 }, { 62, 6 },
    {  59, 6 }, {  55, 6 }, {  54, 6 }, {  49, 6 }, {  52, 6 }, {  47, 6 }, { 46, 6 }, { 41, 6 },
    {  39, 6 }, {  30, 6 }, {  19, 6 }, { 117, 7 }, { 116, 7 }, {  96, 7 }, { 88, 7 }, { 74, 7 },
    {  62, 7 }, { 254, 8 }, {  81, 7 }, { 255, 8 }, { 213, 8 }, { 150, 8 }, { 16, 8 },
};


#define MAKE(d,s)    Make_HuffTable   ( (d), (s), sizeof(s)/sizeof(*(s)) )
#define SORT(x,o)    Resort_HuffTable ( (x), sizeof(x)/sizeof(*(x)), -(Int)(o) )
#define LOOKUP(x,q)  Make_LookupTable ( (q), sizeof(q), (x), sizeof(x)/sizeof(*(x)) )


void
Init_Huffman_Decoder_SV4_6 ( void )
{
    // generate the tables in the way the SV7 encoder needed it
    MAKE ( SCFI_Bundle  , SCFI_Bundle_src   );          // Subframe coupling of scale factors in a frame, direct or differential encoding
    MAKE ( Region_A     , Region_A_src      );          // subband resolution encoding (Subbands  0...10)
    MAKE ( Region_B     , Region_B_src      );          // subband resolution encoding (Subbands 11...22)
    MAKE ( Region_C     , Region_C_src      );          // subband resolution encoding (Subbands 23...31)
    MAKE ( DSCF_Entropie, DSCF_Entropie_src );          // differential scale factor encoding (differential in time)
    MAKE ( Entropie_1   , Entropie_1_src    );          //  3-step quantizer, single samples
    MAKE ( Entropie_2   , Entropie_2_src    );          //  5-step quantizer, single samples
    MAKE ( Entropie_3   , Entropie_3_src    );          //  7-step quantizer, single samples
    MAKE ( Entropie_4   , Entropie_4_src    );          //  9-step quantizer, single samples
    MAKE ( Entropie_5   , Entropie_5_src    );          // 15-step quantizer, single samples
    MAKE ( Entropie_6   , Entropie_6_src    );          // 31-step quantizer, single samples
    MAKE ( Entropie_7   , Entropie_7_src    );          // 63-step quantizer, single samples

    // adds the Code, right adjusts the BitValue and resort the tables for decoder usage
    SORT ( Entropie_1   , Dc7[1] );
    SORT ( Entropie_2   , Dc7[2] );
    SORT ( Entropie_3   , Dc7[3] );
    SORT ( Entropie_4   , Dc7[4] );
    SORT ( Entropie_5   , Dc7[5] );
    SORT ( Entropie_6   , Dc7[6] );
    SORT ( Entropie_7   , Dc7[7] );
    SORT ( SCFI_Bundle  ,     0  );
    SORT ( DSCF_Entropie,     6  );
    SORT ( Region_A     ,     0  );
    SORT ( Region_B     ,     0  );
    SORT ( Region_C     ,     0  );
}

/* end of huffsv46.c */
