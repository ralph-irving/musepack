/*
 *  Bitstream writer function
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
 *    - Transition from 32 bit little endian word stream to 8 bit octet stream
 */


#include "mppenc.h"


static Uint32_t        Buffer [4096+2]; // Puffer für Bitstromdatei
static Uint32_t        dword;           // 32-bit-Wort für Bitstrom-I/O
static int             filled;          // Position im aktuell zu füllenden 32-bit-Wortes
static unsigned int    Zaehler;         // Positionszeiger das bearbeitete Bitstromwort (32 bit)
static unsigned long   WrittenBytes;


unsigned long
GetWrittenBytes ( void )
{
    return WrittenBytes;
}


void
InitBitStream ( void )
{
    dword        =  0;                     // 32-bit-Wort für Bitstrom-I/O
    filled       = 32;                     // Position im aktuell zu füllenden 32-bit-Wortes
    Zaehler      =  0;                     // Positionszeiger das bearbeitete Bitstromwort (32 bit)
    WrittenBytes =  0;                     // Zähler für Anzahl geschriebener Bits im Bitstrom
}


static void
FlushData ( FILE*           fp,
            const Uint8_t*  buffer,
            size_t          bytes )
{
    size_t  WrittenBytes;

    do {
        WrittenBytes = fwrite ( buffer, 1, bytes, fp );
        if ( WrittenBytes <= 0 ) {
            stderr_printf ( "\b\n WARNING: Disk full?, retry after 10 sec ...\a" );
            sleep (10);
            continue;
        }
        buffer += WrittenBytes;
        bytes  -= WrittenBytes;
    } while ( bytes != 0 );
}


void
FlushBitstream ( FILE* fp, int lenoffset )
{
    int       bytes;
    Uint8_t*  p = (Uint8_t*)Buffer;

    ReadBE32 ( Buffer [Zaehler], & dword );         // Zuweisung des "letzten" Worts
    bytes = 4 * Zaehler + 4 - (filled >> 3);

    p[lenoffset+0] = ( bytes - 2 - lenoffset ) >> 8;
    p[lenoffset+1] = ( bytes - 2 - lenoffset ) >> 0;

    FlushData ( fp, (const Uint8_t*)Buffer, bytes );
    dword         =  0;                     // 32-bit-Wort für Bitstrom-I/O
    filled        = 32;                     // Position im aktuell zu füllenden 32-bit-Wortes
    Zaehler       =  0;                     // Positionszeiger das bearbeitete Bitstromwort (32 bit)
    WrittenBytes += bytes;
}


void
WriteBits ( const Uint32_t      input,
            const unsigned int  bits )
{
    filled       -= bits;

    if      ( filled > 0 ) {
        dword  |= input << filled;
    }
    else if ( filled < 0 ) {
        dword  |= input >> -filled ;
        ReadBE32 ( Buffer [Zaehler], & dword );
        Zaehler++;
        filled += 32;
        dword   = input << filled;
    }
    else {
        dword  |= input;
        ReadBE32 ( Buffer [Zaehler], & dword );
        Zaehler++;
        filled  = 32;
        dword   =  0;
    }
}


void
WriteBit ( const unsigned int input )
{
    dword  |= input << --filled;
    if      ( filled == 0 ) {
        ReadBE32 ( Buffer [Zaehler], & dword );
        Zaehler++;
        filled  = 32;
        dword   =  0;
    }
}

/* end of bitstream.c */
