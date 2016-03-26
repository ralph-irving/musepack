/* Modus, der statt mit Huffman-bits mit arithmischer Entropie rechnet, für bessere Optimumsuche */

#include <stdio.h>
#include <math.h>
#include <memory.h>
#include <stdlib.h>
#include "codetable.h"


typedef struct _Code_t {
    long double      Probability;
    unsigned long    CodeWord;
    unsigned short   CodeNumber;
    unsigned char    Bits;
    struct _Code_t*  l1;
    struct _Code_t*  l2;
} Code_t;


static int Cdecl
cmpfn_1 ( const void* p1, const void* p2 )
{
    if ( ((const Code_t*)p1)->Probability < ((const Code_t*)p2)->Probability )
        return +1;
    if ( ((const Code_t*)p1)->Probability > ((const Code_t*)p2)->Probability )
        return -1;
    return 0;
}


static int Cdecl
cmpfn_2 ( const void* p1, const void* p2 )
{
    if ( ((const Code_t*)p1)->Bits < ((const Code_t*)p2)->Bits )
        return -1;
    if ( ((const Code_t*)p1)->Bits > ((const Code_t*)p2)->Bits )
        return +1;
    if ( ((const Code_t*)p1)->CodeNumber < ((const Code_t*)p2)->CodeNumber )
        return -1;
    if ( ((const Code_t*)p1)->CodeNumber > ((const Code_t*)p2)->CodeNumber )
        return +1;
    return 0;
}


static int Cdecl
cmpfn_3 ( const void* p1, const void* p2 )
{
    if ( ((const Code_t*)p1)->CodeNumber < ((const Code_t*)p2)->CodeNumber )
        return -1;
    if ( ((const Code_t*)p1)->CodeNumber > ((const Code_t*)p2)->CodeNumber )
        return +1;
    return 0;
}


static void
CountBits ( Code_t* p, int no )
{
    if ( p -> l1  &&  p -> l2 ) {
        CountBits ( p -> l1, no+1 );
        CountBits ( p -> l2, no+1 );
    }
    else if ( p -> l1  ||  p -> l2 ) {
        fprintf ( stderr, "Error in Huffman Tree\n" );
    }
    else {
        p -> Bits = no;
    }
}


static void
BuildHuffman ( Code_t* Codes, unsigned int len )
{
    unsigned long  Code;
    unsigned int   i;

    for ( i = 0; i < 2*len; i++ ) {
        Codes [i].CodeNumber  = i;
        Codes [i].Bits        = 255;
        Codes [i].CodeWord    = 0x00000000;
        Codes [i].l1          = NULL;
        Codes [i].l2          = NULL;
    }
    for ( i = len; i < 2*len; i++ ) {
        Codes [i].Probability = 0.;
    }

    qsort ( Codes, len, sizeof(*Codes), cmpfn_1 );

    for ( i = len-2; (int)i >= 0; i-- ) {
        Codes [i+i+2]            = Codes   [i+0];
        Codes [i+i+3]            = Codes   [i+1];
        Codes [i+0].Probability += Codes   [i+1].Probability;
        Codes [i+0].l1           = Codes + (i+i+2);
        Codes [i+0].l2           = Codes + (i+i+3);
        qsort ( Codes, i+1, sizeof(*Codes), cmpfn_1 );
    }
    CountBits ( Codes, 0 );

    qsort ( Codes, 2*len, sizeof(*Codes), cmpfn_2 );

    Code = 0x00000000;
    for ( i = 0; i < len; i++ ) {
        Codes [i].CodeWord = Code;
        Code              += 0x80000000 >> (Codes [i].Bits - 1);
    }

    qsort ( Codes, len, sizeof(*Codes), cmpfn_3 );
}


/**********************************************************************************/

#ifdef COUNT_TABLE_USAGE
static int
Report_Huff ( FILE*                  const fp,
              const HuffEncTable_t*  const Table )
{
    Code_t    T [512];
    Int64_t   sum;
    Int64_t   tmp;
    int       len;
    int       div;
    int       j;
    int       ret = 0;

    len = Table -> tablelen;
    sum = 0;
    memset ( T, 0, sizeof T );

    fprintf ( fp, "\n/*\n  table: %s  (%d...%d)\n\n", Table -> tablename, Table -> firstval, Table -> lastval );
    fflush (fp);

    for ( j = 0; j < len; j++ ) {
        tmp  = (Table -> table_no_offs) [j].usage;
        sum += tmp;
    }

    for ( j = 0; j < len; j++ ) {
        tmp  = (Table -> table_no_offs) [j].usage;
        T [j].Probability = tmp  ?  tmp / (double)sum  :  1.e-37;
    }

    BuildHuffman ( T, len );

    for ( j = 0; j < len; j++ ) {
        fprintf ( fp, "%3u: %3d%10lu=%11.8f%%  %2u\n",
                  j,
                  Table -> firstval + j,
                  (Table -> table_no_offs) [j].usage,
                  (double)(100. * T[j].Probability),
                  T[j].Bits );
    }

    fprintf ( fp, "------------------\n" );
    fprintf ( fp, "Sum: %13llu  (%.3f Million)\n\n", sum, sum * 1.e-6 );

    if ( Table -> usage  ) {
        fprintf ( fp, "Table %lu* used, bit advance %llu, average bit advance: %.2f bits, bit usage (huffman): %.2f bits\n\n",
                  Table -> usage,
                  Table -> bits1 - Table -> bits0,
                  (double)UintMAX_FP(Table -> bits1 - Table -> bits0) / Table -> usage,
                  (double)UintMAX_FP(Table -> bits0                 ) / Table -> usage );
    }
    else {
        fprintf ( fp, "  No usage information\n" );
    }

    fprintf ( fp, "*/\n\n" );

    switch ( len ) {
    case  25: div =  5; break;
    case  36: div =  6; break;
    case 226:
    case 216: div =  6; break;
    case  81: div =  9; break;
    case 125: div = 25; break;
    case  49: div =  7; break;
    case 121: div = 11; break;
    case 225: div = 15; break;
    default : div = 16; break;
    }

    fprintf ( fp, "MAKE_TABLE_VEC (%s) {\n", Table -> tablename );
    for ( j = 0; j < len; j++ ) {
        if ( j % div == 0 )
            fprintf ( fp, "    " );
        fprintf ( fp, "%2u,", T[j].Bits );
        if ( j % div == div - 1  ||  j == len - 1 )
            fprintf ( fp, "\n" );
        if ( T[j].Bits > ret )
            ret = T[j].Bits;
    }
    fprintf ( fp, "};\n\n\n" );

    return ret;
}


static void
Report_Footer ( FILE*                  const fp,
                const HuffEncTable_t*  const Table,
                unsigned int                 maxbits )
{
    fprintf ( fp, "MAKE_TABLE_SRC ( %-8s, %3d, %2u );\n",
              Table -> tablename,
              Table -> firstval,
              maxbits );
}


void
Report_Huffgen ( const char* filename )          // weitere Statistiken
{
    unsigned char  maxbits [500];
    FILE*          fp;
    int            i;

    if ( ( fp = fopen (filename, "w" ) ) == NULL )
        return;

    fprintf ( fp, "/*\n *\n *  This code is generated automatically.\n *  PLEASE DO NOT EDIT !!!\n *\n */\n" );
    fflush (fp);

    for ( i = 0; Encodertables[i] != NULL; i++ ) {
        maxbits[i] = Report_Huff ( fp, Encodertables[i] );
        fflush (fp);
    }

    fprintf ( fp, "\n/* Table structures */\n" );

    for ( i = 0; Encodertables[i] != NULL; i++ ) {
        Report_Footer ( fp, Encodertables[i], maxbits[i] );
    }

    fprintf ( fp, "\n/* end of codetable_codes.h */\n" );

    fclose (fp);
}

#else

void
Report_Huffgen ( const char* filename )          // weitere Statistiken
{
    (void)filename;
}

#endif

/* end of codetablemake.c */
