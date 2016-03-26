/*
 *
 *  Decoder Huffman tables for SV7
 *
 *
 *  (C) Copyright 1999-2001  Andree Buschmann. All rights reserved.
 *  (C) Copyright 2001-2002  Frank Klemm. All rights reserved.
 *
 *  For licencing see the licencing file which is part of this package.
 *
 *
 *  Principles:
 *    Original AB encoder/decoder hat a big function Init_Huffman_Encoder_SV7, which
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
 *     Last but not least fast lookup tables are generated. These are tables of the length 2^n,
 *     You preview the next n bits in the bitstream and make a lookup in this table.
 *     The result is a offset in the resorted decoder huffman table.
 *     When n is as big as the largest huffman code, the code is completely decoded.
 *     When n is smaller than the largest huffman code, you must still crawl through the table, but most work is still done.
 *
 *  History:
 *    1999-2002     programmed by Frank Klemm and Andree Buschmann
 *    2003-02-03    the file got an header
 *
 *  Global functions:
 *    - Init_Huffman_Decoder_SV7 ()
 *    - some global variables, which are initialized once and which are only read from outside
 *
 *  TODO:
 *   - because there are no changes in SV7 anymore and because these tables are not used by the encoder anymore,
 *     this code can be replaced by some fixed tables.
 *   - For portable players with lot of DRAM and little Flash RAM this may be suboptimal, because you need much more
 *     Flash RAM, although total cost of DRAM+Flash RAM decreases.
 *
 */


#include "mppdec.h"


// Huffman tables, generated from the compact form in HuffSrc_t
Huffman_t  HuffHdr    [10];            // maximum length: 9 bit
Huffman_t  HuffSCFI   [ 4];            // maximum length: 3 bit
Huffman_t  HuffDSCF   [16];            // maximum length: 6 bit
Huffman_t  HuffQ1 [2] [ 3*3*3];        // maximum length: 6/ 9 bit
Huffman_t  HuffQ2 [2] [ 5*5];          // maximum length: 7/10 bit
Huffman_t  HuffQ3 [2] [ 7];            // maximum length: 4/ 5 bit
Huffman_t  HuffQ4 [2] [ 9];            // maximum length: 4/ 5 bit
Huffman_t  HuffQ5 [2] [15];            // maximum length: 6/ 8 bit
Huffman_t  HuffQ6 [2] [31];            // maximum length: 7/13 bit
Huffman_t  HuffQ7 [2] [63];            // maximum length: 8/14 bit

// Fast lookup tables for huffman decoding
Uint8_t    LUT1_0  [1<< 6];
Uint8_t    LUT1_1  [1<< 9];
Uint8_t    LUT2_0  [1<< 7];
Uint8_t    LUT2_1  [1<<10];
Uint8_t    LUT3_0  [1<< 4];
Uint8_t    LUT3_1  [1<< 5];
Uint8_t    LUT4_0  [1<< 4];
Uint8_t    LUT4_1  [1<< 5];
Uint8_t    LUT5_0  [1<< 6];
Uint8_t    LUT5_1  [1<< 8];
Uint8_t    LUT6_0  [1<< 7];
Uint8_t    LUT6_1  [1<< 7];            // incomplete decode
Uint8_t    LUT7_0  [1<< 8];
Uint8_t    LUT7_1  [1<< 8];            // incomplete decode
Uint8_t    LUTDSCF [1<< 6];            // Sizes of all fast lookup tables: 2976 bytes


// Huffman source tables
static const HuffSrc_t   HuffSCFI_src [4] = {
    { 2, 3 }, { 1, 1 }, { 3, 3 }, { 0, 2 }
};

static const HuffSrc_t   HuffDSCF_src [16] = {
    { 32, 6 }, {  4, 5 }, { 17, 5 }, { 30, 5 }, { 13, 4 }, {  0, 3 }, {  3, 3 }, {  9, 4 },
    {  5, 3 }, {  2, 3 }, { 14, 4 }, {  3, 4 }, { 31, 5 }, {  5, 5 }, { 33, 6 }, { 12, 4 }
};

static const HuffSrc_t   HuffHdr_src [10] = {
    {  92, 8 }, {  47, 7 }, {  10, 5 }, {   4, 4 }, {   0, 2 },
    {   1, 1 }, {   3, 3 }, {  22, 6 }, { 187, 9 }, { 186, 9 }
};

static const HuffSrc_t   HuffQ1_src [2] [3*3*3] = { {
    { 54, 6 }, {  9, 5 }, { 32, 6 }, {  5, 5 }, { 10, 4 }, {  7, 5 }, { 52, 6 }, {  0, 5 }, { 35, 6 },
    { 10, 5 }, {  6, 4 }, {  4, 5 }, { 11, 4 }, {  7, 3 }, { 12, 4 }, {  3, 5 }, {  7, 4 }, { 11, 5 },
    { 34, 6 }, {  1, 5 }, { 53, 6 }, {  6, 5 }, {  9, 4 }, {  2, 5 }, { 33, 6 }, {  8, 5 }, { 55, 6 }
}, {
    { 103, 8 }, {  62, 7 }, { 225, 9 }, {  55, 7 }, {   3, 4 }, {  52, 7 }, { 101, 8 }, {  60, 7 }, { 227, 9 },
    {  24, 6 }, {   0, 4 }, {  61, 7 }, {   4, 4 }, {   1, 1 }, {   5, 4 }, {  63, 7 }, {   1, 4 }, {  59, 7 },
    { 226, 9 }, {  57, 7 }, { 100, 8 }, {  53, 7 }, {   2, 4 }, {  54, 7 }, { 224, 9 }, {  58, 7 }, { 102, 8 }
} };

static const HuffSrc_t   HuffQ2_src [2] [5*5] = { {
    {  89,  7 }, {  47,  6 }, { 15, 5 }, {   0, 5 }, {  91,  7 },
    {   4,  5 }, {   6,  4 }, { 13, 4 }, {   4, 4 }, {   5,  5 },
    {  20,  5 }, {  12,  4 }, {  4, 3 }, {  15, 4 }, {  14,  5 },
    {   3,  5 }, {   3,  4 }, { 14, 4 }, {   5, 4 }, {   1,  5 },
    {  90,  7 }, {   2,  5 }, { 21, 5 }, {  46, 6 }, {  88,  7 }
}, {
    { 921, 10 }, { 113,  7 }, { 51, 6 }, { 231, 8 }, { 922, 10 },
    { 104,  7 }, {  30,  5 }, {  0, 3 }, {  29, 5 }, { 105,  7 },
    {  50,  6 }, {   1,  3 }, {  2, 2 }, {   3, 3 }, {  49,  6 },
    { 107,  7 }, {  27,  5 }, {  2, 3 }, {  31, 5 }, { 112,  7 },
    { 920, 10 }, { 106,  7 }, { 48, 6 }, { 114, 7 }, { 923, 10 }
} };

static const HuffSrc_t   HuffQ3_src [2] [ 7] = { {
    { 12, 4 }, { 4, 3 }, { 0, 2 }, { 1, 2 }, { 7, 3 }, { 5, 3 }, { 13, 4 }
}, {
    {  4, 5 }, { 3, 4 }, { 2, 2 }, { 3, 2 }, { 1, 2 }, { 0, 3 }, {  5, 5 }
} };

static const HuffSrc_t   HuffQ4_src [2] [ 9] = { {
    { 5, 4 }, {  0, 3 }, { 4, 3 }, { 6, 3 }, { 7, 3 }, { 5, 3 }, {  3, 3 }, { 1, 3 }, { 4, 4 }
}, {
    { 9, 5 }, { 12, 4 }, { 3, 3 }, { 0, 2 }, { 2, 2 }, { 7, 3 }, { 13, 4 }, { 5, 4 }, { 8, 5 }
} };

static const HuffSrc_t   HuffQ5_src [2] [15] = { {
    {  57, 6 }, { 23, 5 }, {  8, 4 }, { 10, 4 }, { 13, 4 }, {   0, 3 }, {   2, 3 }, { 3, 3 },
    {   1, 3 }, { 15, 4 }, { 12, 4 }, {  9, 4 }, { 29, 5 }, {  22, 5 }, {  56, 6 }
}, {
    { 229, 8 }, { 56, 6 }, {  7, 5 }, {  2, 4 }, {  0, 3 }, {   3, 3 }, {   5, 3 }, { 6, 3 },
    {   4, 3 }, {  2, 3 }, { 15, 4 }, { 29, 5 }, {  6, 5 }, { 115, 7 }, { 228, 8 },
} };

static const HuffSrc_t   HuffQ6_src [2] [31] = { {
    {   65,  7 }, {    6,  6 }, {  44,  6 }, {  45, 6 }, {   59,  6 }, {   13,  5 }, {   17,  5 }, { 19, 5 },
    {   23,  5 }, {   21,  5 }, {  26,  5 }, {  30, 5 }, {    0,  4 }, {    2,  4 }, {    5,  4 }, {  7, 4 },
    {    3,  4 }, {    4,  4 }, {  31,  5 }, {  28, 5 }, {   25,  5 }, {   27,  5 }, {   24,  5 }, { 20, 5 },
    {   18,  5 }, {   12,  5 }, {   2,  5 }, {  58, 6 }, {   33,  6 }, {    7,  6 }, {   64,  7 },
}, {
    { 6472, 13 }, { 6474, 13 }, { 808, 10 }, { 405, 9 }, {  203,  8 }, {  102,  7 }, {   49,  6 }, {  9, 5 },
    {   15,  5 }, {   31,  5 }, {   2,  4 }, {   6, 4 }, {    8,  4 }, {   11,  4 }, {   13,  4 }, {  0, 3 },
    {   14,  4 }, {   10,  4 }, {   9,  4 }, {   5, 4 }, {    3,  4 }, {   30,  5 }, {   14,  5 }, {  8, 5 },
    {   48,  6 }, {  103,  7 }, { 201,  8 }, { 200, 8 }, { 1619, 11 }, { 6473, 13 }, { 6475, 13 },
} };

static const HuffSrc_t   HuffQ7_src [2] [63] = { {
    { 103, 8 }, { 153, 8 }, { 181, 8 }, { 233, 8 }, {  64, 7 }, {  65, 7 }, {  77, 7 }, {  81, 7 }, {  91, 7 },
    { 113, 7 }, { 112, 7 }, {  24, 6 }, {  29, 6 }, {  35, 6 }, {  37, 6 }, {  41, 6 }, {  44, 6 }, {  46, 6 },
    {  51, 6 }, {  49, 6 }, {  54, 6 }, {  55, 6 }, {  57, 6 }, {  60, 6 }, {   0, 5 }, {   2, 5 }, {  10, 5 },
    {   5, 5 }, {   9, 5 }, {   6, 5 }, {  13, 5 }, {   7, 5 }, {  11, 5 }, {  15, 5 }, {   8, 5 }, {   4, 5 },
    {   3, 5 }, {   1, 5 }, {  63, 6 }, {  62, 6 }, {  61, 6 }, {  53, 6 }, {  59, 6 }, {  52, 6 }, {  48, 6 },
    {  47, 6 }, {  43, 6 }, {  42, 6 }, {  39, 6 }, {  36, 6 }, {  33, 6 }, {  28, 6 }, { 117, 7 }, { 101, 7 },
    { 100, 7 }, {  80, 7 }, {  69, 7 }, {  68, 7 }, {  50, 7 }, { 232, 8 }, { 180, 8 }, { 152, 8 }, { 102, 8 },
}, {
    { 14244, 14 }, { 14253, 14 }, { 14246, 14 }, { 14254, 14 }, {  3562, 12 }, {   752, 10 }, {   753, 10 },
    {   160,  9 }, {   162,  9 }, {   444,  9 }, {   122,  8 }, {   223,  8 }, {    60,  7 }, {    73,  7 },
    {   110,  7 }, {    14,  6 }, {    24,  6 }, {    25,  6 }, {    34,  6 }, {    37,  6 }, {    54,  6 },
    {     3,  5 }, {     9,  5 }, {    11,  5 }, {    16,  5 }, {    19,  5 }, {    21,  5 }, {    24,  5 },
    {    26,  5 }, {    29,  5 }, {    31,  5 }, {     2,  4 }, {     0,  4 }, {    30,  5 }, {    28,  5 },
    {    25,  5 }, {    22,  5 }, {    20,  5 }, {    14,  5 }, {    13,  5 }, {     8,  5 }, {     6,  5 },
    {     2,  5 }, {    46,  6 }, {    35,  6 }, {    31,  6 }, {    21,  6 }, {    15,  6 }, {    95,  7 },
    {    72,  7 }, {    41,  7 }, {   189,  8 }, {   123,  8 }, {   377,  9 }, {   161,  9 }, {   891, 10 },
    {   327, 10 }, {   326, 10 }, {  3560, 12 }, { 14255, 14 }, { 14247, 14 }, { 14252, 14 }, { 14245, 14 },
} };


#define MAKE(d,s)     Make_HuffTable   ( (d), (s), sizeof(s)/sizeof(*(s)) )
#define SORT(x,o)     Resort_HuffTable ( (x), sizeof(x)/sizeof(*(x)), -(Int)(o) )
#define LOOKUP(x,q)   Make_LookupTable ( (q), sizeof(q), (x), sizeof(x)/sizeof(*(x)) )


void
Init_Huffman_Decoder_SV7 ( void )
{
    // generate the tables in the way the SV7 encoder needed it
    MAKE ( HuffSCFI , HuffSCFI_src  );          // Subframe coupling of scale factors in a frame
    MAKE ( HuffDSCF , HuffDSCF_src  );          // differential scale factor encoding (differential in time)
    MAKE ( HuffHdr  , HuffHdr_src   );          // differential subband resolution encoding (differential in frequency)
    MAKE ( HuffQ1[0], HuffQ1_src[0] );          //  3-step quantizer, 3 bundled samples: less shaped, book 0
    MAKE ( HuffQ1[1], HuffQ1_src[1] );          //  3-step quantizer, 3 bundled samples: more shaped, book 1
    MAKE ( HuffQ2[0], HuffQ2_src[0] );          //  5-step quantizer, 2 bundled samples: less shaped, book 0
    MAKE ( HuffQ2[1], HuffQ2_src[1] );          //  5-step quantizer, 2 bundled samples: more shaped, book 1
    MAKE ( HuffQ3[0], HuffQ3_src[0] );          //  7-step quantizer, single samples:    less shaped, book 0
    MAKE ( HuffQ3[1], HuffQ3_src[1] );          //  7-step quantizer, single samples:    more shaped, book 1
    MAKE ( HuffQ4[0], HuffQ4_src[0] );          //  9-step quantizer, single samples:    less shaped, book 0
    MAKE ( HuffQ4[1], HuffQ4_src[1] );          //  9-step quantizer, single samples:    more shaped, book 1
    MAKE ( HuffQ5[0], HuffQ5_src[0] );          // 15-step quantizer, single samples:    less shaped, book 0
    MAKE ( HuffQ5[1], HuffQ5_src[1] );          // 15-step quantizer, single samples:    more shaped, book 1
    MAKE ( HuffQ6[0], HuffQ6_src[0] );          // 31-step quantizer, single samples:    less shaped, book 0
    MAKE ( HuffQ6[1], HuffQ6_src[1] );          // 31-step quantizer, single samples:    more shaped, book 1
    MAKE ( HuffQ7[0], HuffQ7_src[0] );          // 63-step quantizer, single samples:    less shaped, book 0
    MAKE ( HuffQ7[1], HuffQ7_src[1] );          // 63-step quantizer, single samples:    more shaped, book 1

    // adds the Code, right adjusts the BitValue and resort the tables for decoder usage
    SORT   ( HuffHdr  ,    5   );
    SORT   ( HuffSCFI ,    0   );
    SORT   ( HuffDSCF ,    7   );
    SORT   ( HuffQ1[0],    0   );
    SORT   ( HuffQ1[1],    0   );
    SORT   ( HuffQ2[0],    0   );
    SORT   ( HuffQ2[1],    0   );
    SORT   ( HuffQ3[0], Dc7[3] );
    SORT   ( HuffQ3[1], Dc7[3] );
    SORT   ( HuffQ4[0], Dc7[4] );
    SORT   ( HuffQ4[1], Dc7[4] );
    SORT   ( HuffQ5[0], Dc7[5] );
    SORT   ( HuffQ5[1], Dc7[5] );
    SORT   ( HuffQ6[0], Dc7[6] );
    SORT   ( HuffQ6[1], Dc7[6] );
    SORT   ( HuffQ7[0], Dc7[7] );
    SORT   ( HuffQ7[1], Dc7[7] );

    // generate fast lookup tables
    LOOKUP ( HuffQ1[0], LUT1_0 );
    LOOKUP ( HuffQ1[1], LUT1_1 );
    LOOKUP ( HuffQ2[0], LUT2_0 );
    LOOKUP ( HuffQ2[1], LUT2_1 );
    LOOKUP ( HuffQ3[0], LUT3_0 );
    LOOKUP ( HuffQ3[1], LUT3_1 );
    LOOKUP ( HuffQ4[0], LUT4_0 );
    LOOKUP ( HuffQ4[1], LUT4_1 );
    LOOKUP ( HuffQ5[0], LUT5_0 );
    LOOKUP ( HuffQ5[1], LUT5_1 );
    LOOKUP ( HuffQ6[0], LUT6_0 );
    LOOKUP ( HuffQ6[1], LUT6_1 );
    LOOKUP ( HuffQ7[0], LUT7_0 );
    LOOKUP ( HuffQ7[1], LUT7_1 );
    LOOKUP ( HuffDSCF , LUTDSCF);
}

/* end of huffsv7.c */
