/*
 *  Generator functions for Look-Up Tables for fast huffman encoding and decoding
 *
 *  (C) Frank Klemm 2002. All rights reserved.
 *
 *  Principles:
 *    This module generates the tables needed for Huffman encoding and decoding
 *    from the Huffman code source tables. Input is a pointer to a HuffSourceTable_t
 *    structure, output is a HuffEncTable_t or HuffDecTable_t structure.
 *    The built routine for decoding also needs a second argument about the length of the fast lookup table.
 *    Size is 2^fastdecodebits bytes.
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

#include <stdio.h>
#include "mppdec.h"
#include "codetable.h"


static int
testbit ( const Uint8_t* bitlength, size_t len, int maxbits_proposal, const char* tablename )
{
    size_t    i;
    int       max = 0;
    Uint64_t  sum = 0;

    for ( i = 0; i < len; i++ ) {
        if ( bitlength [i] > max )
            max = bitlength [i];
        sum += (Uint64_t)0x100000000LU >> bitlength [i];
    }

    if ( max != maxbits_proposal  ||  sum != (Uint64_t)0x100000000LU ) {
        fprintf ( stderr, "*** Error in Huffman Source Table '%s' ***\n", tablename );
        if ( max != maxbits_proposal )
            fprintf ( stderr, "  Max bitlength proposal wrong: %2u (correct: %2u)\n", maxbits_proposal, max );
        if ( sum != (Uint64_t)0x100000000LU )
            fprintf ( stderr, "  Incomplete code table (%lX.%08lX)\n", (unsigned long)(sum >> 32), (unsigned long)(sum & 0xFFFFFFFFLU) );

        fprintf ( stderr, "\alen=%u: ", len );
        for ( i = 0; i < len; i++ )
            fprintf ( stderr, "%2u ", bitlength[i] );
        sleep (4);
        fprintf ( stderr, "\n\n" );
    }

    return max;
}


HuffEncTable_t
MakeHuffEncTable ( const HuffSourceTable_t* src )
{
    HuffEncTable_t  ret;
    HuffEncCode_t*  p = calloc ( src -> tablelen, sizeof(HuffEncCode_t) );
    int             bits;
    int             maxbits = src -> maxlen;
    size_t          j;
    unsigned int    code = 0x00000000;

    maxbits = testbit ( src -> bitlength, src -> tablelen, maxbits, src -> tablename );

    memset ( &ret, 0, sizeof ret );
    ret.tablename     = src -> tablename;
    ret.bitlength     = src -> bitlength;
    ret.tablelen      = src -> tablelen;
    ret.firstval      = src -> firstval;
    ret.lastval       = src -> firstval + src -> tablelen - 1;
    ret.maxlen        = maxbits;
    ret.table_no_offs = p;
    ret.table         = p - src -> firstval;

    for ( bits = maxbits; bits >= 1; bits-- ) {
        for ( j = 0; j < src -> tablelen; j++ )
            if ( src -> bitlength[j] == bits ) {
                p[j].pattern = code >> (32 - bits);
                p[j].bits    = bits;
                code        += 1 << (32 - bits);
            }
    }

    if ( code != 0x00000000 )
        fprintf ( stderr, "\a" );

    return ret;
}


static int Cdecl
cmp_fn ( const void* p1, const void* p2 )
{
    if ( ((const HuffDecCode_t*)p1) -> pattern < ((const HuffDecCode_t*)p2) -> pattern ) return +1;
    if ( ((const HuffDecCode_t*)p1) -> pattern > ((const HuffDecCode_t*)p2) -> pattern ) return -1;
    return 0;
}


HuffDecTable_t
MakeHuffDecTable ( const HuffSourceTable_t* src, unsigned int fastdecodebits )
{
    HuffDecTable_t  ret;
    HuffDecCode_t*  p;
    unsigned char*  q;
    unsigned char*  q1;
    unsigned char*  q2;
    int             bits;
    unsigned int    maxbits = src -> maxlen;
    size_t          j;
    unsigned int    code  = 0x00000000;
    int             index = src -> tablelen;

    maxbits = testbit ( src -> bitlength, src -> tablelen, maxbits, src -> tablename );

    if ( fastdecodebits > maxbits )
        fastdecodebits = maxbits;

    p  = calloc ( src -> tablelen, sizeof(HuffDecCode_t) );
    q  = fastdecodebits > 0  ?  calloc (1, 1 << fastdecodebits)  :  NULL;
    q1 = q;

    memset ( &ret, 0, sizeof ret );
    ret.tablename = src -> tablename;
    ret.bitlength = src -> bitlength;
    ret.tablelen  = src -> tablelen;
    ret.firstval  = src -> firstval;
    ret.maxlen    = maxbits;
    ret.table     = p;

    ret.lut       = q;
    ret.lutbits   = fastdecodebits;
    ret.nolutbits = 32 - fastdecodebits;

    for ( bits = maxbits; bits >= 1; bits-- ) {
        for ( j = 0; j < src -> tablelen; j++ )
            if ( src -> bitlength[j] == bits ) {
                p[j].pattern = code;
                p[j].bits    = bits;
                p[j].value   = j + src -> firstval;
                code        += 1 << (32 - bits);
                if ( q != NULL ) {
                    q2 = q + ( code >> ret.nolutbits );
                    index--;
                    while ( q1 < q2 )
                        *q1++ = index;
                }
            }
    }

    if ( code != 0x00000000 )
        fprintf ( stderr, "\a" );

    qsort ( p, src -> tablelen, sizeof(*p), cmp_fn );
    return ret;
}


#ifdef COUNT_TABLE_USAGE
void
Advantage ( HuffEncTable_t* const  p,
            unsigned int           bits0,
            unsigned int           bits1,
            unsigned int           coded_values )
{
    if ( bits1 - bits0 > 256 ) {
        fprintf ( stderr, "Bitadvance may be out of range: 1.=%u 2.=%u (table %s)\n", bits0, bits1, p -> tablename );
    }
    p -> usage += coded_values;
    p -> bits0 += bits0;
    p -> bits1 += bits1;
}
#endif

/* end of codetable.c */
