#include <string.h>
#include <assert.h>
#include "mppdec.h"
#include "codetable.h"
#include "codetable_data.h"


// InputBuff-Pufferüberläufe seltener abfangen, dafür etwas Rasen dahinter ???
// Funktionen mal wieder sauber durchnumerieren
// Calculate_New_V: Am Ende Werte gleich an allen notwendigen Stellen speichern
// 3DNow!-Code (Assembleranweisungen) besser voneinander unabhängig machen (reordern)
// Bitbedarf der einzelnen Bänder bestimmen, um RES/SFI/QQ-Bitbedarf gegeneinander abzustimmen


#define BITS     (CHAR_BIT * sizeof(*InputBuff))      // Bits per InputBuff-Word
#define INC      InputCnt = (InputCnt + 1) & IBUFMASK


Ibuf_t           InputBuff [IBUFSIZE /* +128 */ ];  // enthält den Lese-Puffer
static Uint32_t  mask      [32 + 1];
size_t           InputCnt;             // aktuelle Position im Lese-Puffer
static Ibuf_t    dword;                // BITS Bit-Wort für Bitstrom-I/O
static Uint      pos;                  // Position im aktuell decodierten BITS-bit-Wort
static size_t    LastInputCnt = 0;
static Uint      Wraps        = 0;


/*
 *  Initialisieren aller Tabellen und Variablen
 */

void
Bitstream_init ( void )
{
    Int       i;
    Uint32_t  val;

    InputCnt     = (size_t)-1;
    pos          = BITS;
    dword        = 0;     // Werte werden so initialisiert, daß beim nächsten Lesen von mindestens 1 bit das erste dword automatisch eingezogen wird
    LastInputCnt = 0;
    Wraps        = 0;

    for ( val = 0, i = 0; i <= 32; i++, val += val+1 )
        mask [i] = val;
}

/*
 *  Lesen einer festen Anzahl von Bits aus dem Bitstrom. Es können garantiert 0...16 bit gelesen werden,
 *  bei auf 16 bit alignten Zugriffen bis 32 bit.
 */

Uint32_t
BitstreamLE_read ( Int bits )
{
    Uint32_t  ret;

    ENTER(78);

    ret = dword;
    if ( (pos += bits) < BITS ) {
        ret >>= BITS - pos;
    }
    else {
        pos  -= BITS;
        INC; ReadLE32 ( dword,  InputBuff+ InputCnt );
        if ( pos > 0 ) {
            ret <<= pos;
            ret  |= dword >> (BITS - pos);
        }
    }
    ret &= mask [bits];

    LEAVE(78);
    REP (printf ( "read(%2u) = %u\n", bits, ret ));
    return ret;
}


/*
 *  Schnellere Form für BitstreamLE_read(1)
 */

static Uint
BitstreamLE_read1 ( void )
{
    Uint  ret;

    ENTER(93);

    ret = (Uint)( dword >> ( BITS - ++pos) ) & 1;
    if ( pos >= BITS ) {
        INC; ReadLE32 ( dword,  InputBuff+ InputCnt );
        pos  -= BITS;
    }

    LEAVE(93);
    REP (printf ( "read( 1) = %u\n", ret ));
    return ret;
}


/*
 *  Lesen von n bits (Restriktionen siehe BitstreamLE_read), ohne diese zu quitieren
 */

Uint32_t
BitstreamLE_preview ( Int bits )
{
    Uint      new_pos = pos + bits;
    Uint32_t  ret     = dword;
    Uint32_t  tmp;

    if ( new_pos < BITS ) {
        ret >>= BITS - new_pos;
    }
    else if ( new_pos > BITS ) {
        ret <<= new_pos - BITS;
        ReadLE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        ret  |= tmp >> (2*BITS - new_pos);
    }
    return ret /* & mask[bits] */;
}


/*
 *  Decodiere Huffman-Kode, der maximal 14 bit lang sein darf
 *  Das Dekodieren erfolgt durch reines Scannen der Tabelle.
 */

static Int
HuffmanLE_Decode ( const Huffman_t* Table )
{
    Uint32_t  code;
    Uint32_t  tmp;

    ENTER(79);

    code = dword << pos;
    if ( pos > BITS - 14 ) {
        ReadLE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        code |= tmp >> (BITS - pos);
    }

    while ( code < Table->Code )
        Table++;

    // Setze Bitstromposition ohne dummy-read
    if ( (pos += Table->Length) >= BITS ) {
        pos   -= BITS;
        INC; ReadLE32 ( dword,  InputBuff+ InputCnt );
    }

    LEAVE(79);
    REP (printf ( "decode() = %d\n", Table->Value ));
    return Table->Value;
}


/*
 *  Decodiere Huffman-Kode, der maximal 14 bit lang sein darf
 *  Das Dekodieren erfolgt durch Grobpositionierung mittels Helpertabelle (tab,unused_bits),
 *  danach erfolgt ein weiteres Scannen, bis der Wert erreicht ist.
 */

static Int
HuffmanLE_Decode_faster ( const Huffman_t* Table, const Uint8_t* tab, Int unused_bits )
{
    Uint32_t  code;
    Uint32_t  tmp;

    ENTER(93);

    code = dword << pos;
    if ( pos > BITS - 14 ) {
        ReadLE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        code |= tmp >> (BITS - pos);
    }

    Table += tab [(size_t)(code >> unused_bits) ];

    while ( code < Table->Code )
        Table++;

    // Setze Bitstromposition ohne dummy-read
    if ( (pos += Table->Length) >= BITS ) {
        pos   -= BITS;
        INC; ReadLE32 ( dword,  InputBuff+ InputCnt );
    }

    LEAVE(93);
    REP (printf ( "decode() = %d\n", Table->Value ));
    return Table->Value;
}


/*
 *  Decodiere Huffman-Kode, der maximal 16 bit lang sein darf
 *  Das Dekodieren erfolgt allein durch Tabellenlookup, damit nur für "kurze" Codes verwendbar,
 *  weil sonst riesige Tabellen notwendig werden
 */

static Int
HuffmanLE_Decode_fastest ( const Huffman_t* Table, const Uint8_t* tab, Int unused_bits )
{
    Uint32_t  code;
    Uint32_t  tmp;

    ENTER(91);

    code = dword << pos;
    // ist die folgende Zeile optimal?
    if ( pos > unused_bits + BITS - 32 ) {
        ReadLE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        code |= tmp >> (BITS - pos);
    }

    Table += tab [ (size_t) (code >> unused_bits) ];

    // Setze Bitstromposition ohne dummy-read
    if ( (pos += Table->Length) >= BITS ) {
        pos   -= BITS;
        INC; ReadLE32 ( dword,  InputBuff+ InputCnt );
    }

    LEAVE(91);
    REP (printf ( "decode() = %d\n", Table->Value ));
    return Table->Value;
}


#define HUFFMAN_DECODE_FASTER(a,b,c)    HuffmanLE_Decode_faster  ( (a), (b), 32-(c) )
#define HUFFMAN_DECODE_FASTEST(a,b,c)   HuffmanLE_Decode_fastest ( (a), (b), 32-(c) )
#define Decode_DSCF()                   HUFFMAN_DECODE_FASTEST   ( HuffDSCF, LUTDSCF, 6 )


/******************/

Uint32_t
BitstreamBE_read ( Int bits )
{
    Uint32_t  ret;

    ENTER(78);

    ret = dword;
    if ( (pos += bits) < BITS ) {
        ret >>= BITS - pos;
    }
    else {
        pos  -= BITS;
        INC;
        ReadBE32 ( dword, InputBuff + InputCnt );
        if ( pos > 0 ) {
            ret <<= pos;
            ret  |= dword >> (BITS - pos);
        }
    }
    ret &= mask [bits];

    LEAVE(78);
    REP (printf ( "read(%2u) = %u\n", bits, ret ));
    return ret;
}


/*
 *  Schnellere Form für BitstreamBE_read(1)
 */

static Uint
BitstreamBE_read1 ( void )
{
    Uint  ret;

    ENTER(93);

    ret = (Uint)( dword >> ( BITS - ++pos) ) & 1;
    if ( pos >= BITS ) {
        INC;
        ReadBE32 ( dword, InputBuff + InputCnt );
        pos  -= BITS;
    }

    LEAVE(93);
    REP (printf ( "read( 1) = %u\n", ret ));
    return ret;
}


static Int
HuffDecode ( const HuffDecTable_t* T )
{
    const HuffDecCode_t*  p = T -> table;
    Uint32_t              code;
    Uint32_t              tmp;

    ENTER(79);

    code = dword << pos;
    if ( pos > BITS - 29 ) {                                            // !!!, dynamisch bestimmen
        ReadBE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        code |= tmp >> (BITS - pos);
    }

    while ( code < p -> pattern )
        p++;

    if ( (pos += p -> bits) >= BITS ) {
        pos   -= BITS;
        INC; ReadBE32 ( dword, InputBuff + InputCnt );   // tmp benutzen!
    }

    LEAVE(79);
    REP (printf ( "decode() = %d\n", p -> value ));
    return p -> value;
}


static Int
HuffDecode_LUT ( const HuffDecTable_t* T )
{
    const HuffDecCode_t*  p = T -> table;
    Uint32_t              code;
    Uint32_t              tmp;

    ENTER(93);

    code = dword << pos;
    if ( pos > BITS - 29 ) {                                            // !!!, dynamisch bestimmen
        ReadBE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        code |= tmp >> (BITS - pos);
    }

    p += (T -> lut) [(size_t)(code >> T -> nolutbits) ];

    while ( code < p -> pattern )
        p++;

    if ( (pos += p -> bits) >= BITS ) {
        pos   -= BITS;
        INC; ReadBE32 ( dword, InputBuff + InputCnt );   // tmp benutzen!
    }

    LEAVE(93);
    REP (printf ( "decode() = %d\n", p -> value ));
    return p -> value;
}


static Int
HuffDecode_onlyLUT ( const HuffDecTable_t* T )
{
    const HuffDecCode_t*  p = T -> table;
    Uint32_t              code;
    Uint32_t              tmp;

    ENTER(91);

#ifdef DEBUG                                    // Test, ob Lookup-Table lang genug ist für HuffDecode_onlyLUT()
#endif

    code = dword << pos;
    if ( pos > BITS - 12 ) {                                            // !!!
        ReadBE32 ( tmp, & InputBuff [(InputCnt+1) & IBUFMASK] );
        code |= tmp >> (BITS - pos);
    }

    p += T->lut [ (size_t) (code >> T -> nolutbits) ];

    if ( (pos += p -> bits) >= BITS ) {
        pos   -= BITS;
        INC; ReadBE32 ( dword, InputBuff + InputCnt );   // tmp benutzen!
    }

    LEAVE(91);
    REP (printf ( "decode() = %d\n", p -> value ));
    return p -> value;
}


/******************/


Ulong
BitsRead ( void )
{
    if ( LastInputCnt > InputCnt )
        Wraps++;
    LastInputCnt = InputCnt;

    return ((Ulong)Wraps*IBUFSIZE + InputCnt) * BITS + pos;
}


#include "dump.c"
Ulong              __x[8];
#define BITPOS(x)  __x[x] = BitsRead ()


/*
 *  Höhere Auflösungen (ab Stufe 8) sind nicht mehr huffmankodiert, Anzahl der Bits, die dann direkt gelesen werden
 *  Bits pro Sample für gewählte Auflösungsstufe, nur für die hochaufösenden ohne Huffman-Kodierung
 */

/******************************************************************************************/
/****************************************** SV 6 ******************************************/
/******************************************************************************************/
static int
Read_Bitstream_SV6 ( Uint MS_bits )
{
    Int                Band;
    Uint               k;
    const Huffman_t*   Table;
    const Huffman_t*   xL;
    const Huffman_t*   xR;
    Int                Max_Used_Band = -1;              // ????????????? vorher 0 ??????????????
    pack_t             SCFI [2] [32];

    ENTER(6);

    /********* Lese Auflösung und LR/MS für alle Subbänder und bestimme letztes Subband *********************/

    BITPOS(0);
    for ( Band = 0; Band <= Max_Band; Band++ ) {
        Table = Region [Band];

        Res[0][Band] = Q_res[Band][Bitrate <= 128  ?  HuffmanLE_Decode(Table)  :  (Int) BitstreamLE_read(Q_bit[Band])];
        Res[1][Band] = 0;

        // Nicht lesen für IS für Bänder ab MinBand+1
        if ( !IS_used  ||  Band < Min_Band ) {
            MS_Band [Band] = 0;
            if ( MS_bits )
                MS_Band [Band] = BitstreamLE_read1 ();
            Res[1][Band] = Q_res[Band][Bitrate <= 128  ?  HuffmanLE_Decode(Table)  :  (Int) BitstreamLE_read(Q_bit[Band])];
        }
        // Bestimme letztes benutztes Subband (folgende Operationen werden nur noch bis zu diesem ausgeführt)
        if ( Res[0][Band]  ||  Res[1][Band] )
            Max_Used_Band = Band;
    }

    /********* Lese verwendetes Scalebandfactor-Splitting der 36 Samples pro Subband und Werteaddressierung (abs./rel.) */

    BITPOS(1);
    for ( Band = 0; Band <= Max_Used_Band; Band++ ) {
        if ( Res[0][Band] )
            SCFI[0][Band] = HuffmanLE_Decode (SCFI_Bundle);
        if ( Res[1][Band]  ||  (Res[0][Band]  &&  IS_used  &&  Band >= Min_Band) )
            SCFI[1][Band] = HuffmanLE_Decode (SCFI_Bundle);
    }

    /********* Lese Scalefaktors für alle Subbänder dreimal für jeweils 12 Samples **************************/

    BITPOS(2);
    Table = DSCF_Entropie;
    for ( Band = 0; Band <= Max_Used_Band; Band++ ) {
        if ( Res[0][Band] ) {
            switch ( SCFI[0][Band] ) {
            case 0:                                     // ohne Differential SCF
                SCF_Index[0][Band][0] = (Int) BitstreamLE_read(6);
                SCF_Index[0][Band][1] = (Int) BitstreamLE_read(6);
                SCF_Index[0][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 2:
                SCF_Index[0][Band][0] = (Int) BitstreamLE_read(6);
                SCF_Index[0][Band][1] =
                SCF_Index[0][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 4:
                SCF_Index[0][Band][0] =
                SCF_Index[0][Band][1] = (Int) BitstreamLE_read(6);
                SCF_Index[0][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 6:
                SCF_Index[0][Band][0] =
                SCF_Index[0][Band][1] =
                SCF_Index[0][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 1:                                     // mit Differential SCF
                SCF_Index[0][Band][0] = SCF_Index[0][Band][2] + HuffmanLE_Decode(Table);
                SCF_Index[0][Band][1] = SCF_Index[0][Band][0] + HuffmanLE_Decode(Table);
                SCF_Index[0][Band][2] = SCF_Index[0][Band][1] + HuffmanLE_Decode(Table);
                break;
            default:
                assert (0);
            case 3:
                SCF_Index[0][Band][0] = SCF_Index[0][Band][2] + HuffmanLE_Decode(Table);
                SCF_Index[0][Band][1] =
                SCF_Index[0][Band][2] = SCF_Index[0][Band][0] + HuffmanLE_Decode(Table);
                break;
            case 5:
                SCF_Index[0][Band][0] =
                SCF_Index[0][Band][1] = SCF_Index[0][Band][2] + HuffmanLE_Decode(Table);
                SCF_Index[0][Band][2] = SCF_Index[0][Band][1] + HuffmanLE_Decode(Table);
                break;
            case 7:
                SCF_Index[0][Band][0] =
                SCF_Index[0][Band][1] =
                SCF_Index[0][Band][2] = SCF_Index[0][Band][2] + HuffmanLE_Decode(Table);
                break;
            }
        }

        if ( Res[1][Band]  ||  (Res[0][Band]  &&  IS_used  &&  Band >= Min_Band) ) {
            switch ( SCFI[1][Band] ) {
            case 0:
                SCF_Index[1][Band][0] = (Int) BitstreamLE_read(6);
                SCF_Index[1][Band][1] = (Int) BitstreamLE_read(6);
                SCF_Index[1][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 2:
                SCF_Index[1][Band][0] = (Int) BitstreamLE_read(6);
                SCF_Index[1][Band][1] =
                SCF_Index[1][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 4:
                SCF_Index[1][Band][0] =
                SCF_Index[1][Band][1] = (Int) BitstreamLE_read(6);
                SCF_Index[1][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 6:
                SCF_Index[1][Band][0] =
                SCF_Index[1][Band][1] =
                SCF_Index[1][Band][2] = (Int) BitstreamLE_read(6);
                break;
            case 1:
                SCF_Index[1][Band][0] = SCF_Index[1][Band][2] + HuffmanLE_Decode(Table);
                SCF_Index[1][Band][1] = SCF_Index[1][Band][0] + HuffmanLE_Decode(Table);
                SCF_Index[1][Band][2] = SCF_Index[1][Band][1] + HuffmanLE_Decode(Table);
                break;
            default:
                assert (0);
            case 3:
                SCF_Index[1][Band][0] = SCF_Index[1][Band][2] + HuffmanLE_Decode(Table);
                SCF_Index[1][Band][1] =
                SCF_Index[1][Band][2] = SCF_Index[1][Band][0] + HuffmanLE_Decode(Table);
                break;
            case 5:
                SCF_Index[1][Band][0] =
                SCF_Index[1][Band][1] = SCF_Index[1][Band][2] + HuffmanLE_Decode(Table);
                SCF_Index[1][Band][2] = SCF_Index[1][Band][1] + HuffmanLE_Decode(Table);
                break;
            case 7:
                SCF_Index[1][Band][0] =
                SCF_Index[1][Band][1] =
                SCF_Index[1][Band][2] = SCF_Index[1][Band][2] + HuffmanLE_Decode(Table);
                break;
            }
        }
    }

    /********* Lese die quantisierten Samples pro Subband (ohne Offsets, d.h. Werte liegen nullsymmetrisch) */

    BITPOS(3);
    for ( Band = 0; Band <= Max_Used_Band; Band++ ) {
        xL = Entropie [Res[0][Band]];
        xR = Entropie [Res[1][Band]];

        if ( xL != NULL  ||  xR != NULL )
            for ( k = 0; k < 36; k++ ) {
                if ( xL != NULL )
                    QQ [0] [Band] [k] = HuffmanLE_Decode (xL);
                if ( xR != NULL )
                    QQ [1] [Band] [k] = HuffmanLE_Decode (xR);
            }

        if ( Res[0][Band] >= 8  ||  Res[1][Band] >= 8 )
            for ( k = 0; k < 36; k++ ) {
                if ( Res[0][Band] >= 8 )
                    QQ [0] [Band] [k] = (Int) BitstreamLE_read (Res[0][Band]-1) - Dc7[Res[0][Band]];
                if ( Res[1][Band] >= 8 )
                    QQ [1] [Band] [k] = (Int) BitstreamLE_read (Res[1][Band]-1) - Dc7[Res[1][Band]];
            }
    }

    BITPOS(4);

    if ( DUMPSELECT > 0 )
        Dump ( Max_Used_Band, 2, MS_Band, Res, SCF_Index, QQ, 0x06, __x );

    LEAVE(6);
    return Max_Used_Band;
}



#if 0
static void
CalculateTNS ( Float TNS [36], const res_t Res[], const quant_t Q [] [36], int Band )     // For a non-shaped output the vector should be all 65536 / 5 = 13170.2
{
    int    i;
    int    j;
    Float  Sum;

    for ( i = 0; i < 36; i++ )
        TNS [i] = 3.;

    for ( j = (Band+1)/2; j-- > 0; ) {
        do {
            Q--;
            if (Band-- == 0) goto cont;
        } while ( *--Res <= 0 );
        for ( i = 0; i < 36; i++ )
            TNS [i] += Q [0] [i] * Q [0] [i];
    }
cont:

    for ( j = 0; j < 3; j++ ) {
        Sum = 0;
        for ( i = 0; i < 12; i++ )
            Sum += TNS [12*j + i];
        Sum = sqrt (12. / Sum) * 13170.2;
        for ( i = 0; i < 12; i++ )
            TNS [12*j + i] = sqrt (TNS [12*j + i]) * Sum;
    }
}
#endif

/******************************************************************************************/
/****************************************** SV 7 ******************************************/
/******************************************************************************************/

static int
Read_Bitstream_SV7 ( Uint MS_bits )
{
    static Schar  tab30 [3*3*3] = { -1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1 };
    static Schar  tab31 [3*3*3] = { -1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1 };
    static Schar  tab32 [3*3*3] = { -1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1 };
    static Schar  tab50 [5*5  ] = { -2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2 };
    static Schar  tab51 [5*5  ] = { -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2 };
    // Float               TNS [CH] [36];
    Int                 Band;
    Uint                k;
    quant_t*            pp;
    const Huffman_t*    Table;
    Int                 diff;
    Uint                idx;
    Uint32_t            tmp;
    Int                 Max_Used_Band = -1;
    pack_t              SCFI [2] [32];

    ENTER(7);

    /********* Lese Auflösung und LR/MS für Subband 0 *******************************************************/

    BITPOS(0);
    Res[0][0] = (Int) BitstreamLE_read(4);
    Res[1][0] = (Int) BitstreamLE_read(4);
    MS_Band [0] = 0;
    if ( Res[0][0]  ||  Res[1][0] ) {
        Max_Used_Band = 0;                                      // Bestimme letztes benutztes Subband (folgende Operationen werden nur noch bis zu diesem ausgeführt)
        if ( MS_bits )
            MS_Band [0] = BitstreamLE_read1 ();
    }

    /********* Lese Auflösung und LR/MS für folgende Subbänder und bestimme letztes Subband *****************/

    Table = HuffHdr;
    for ( Band = 1; Band <= Max_Band; Band++ ) {

        diff = HuffmanLE_Decode (Table);
        Res[0][Band] = diff != 4  ?  Res[0][Band-1] + diff  :  (Int) BitstreamLE_read(4);
        diff = HuffmanLE_Decode (Table);
        Res[1][Band] = diff != 4  ?  Res[1][Band-1] + diff  :  (Int) BitstreamLE_read(4);
        MS_Band [Band] = 0;

        if ( Res[0][Band]  ||  Res[1][Band] ) {
            Max_Used_Band = Band;                       // Bestimme letztes benutztes Subband (folgende Operationen werden nur noch bis zu diesem ausgeführt)
            if ( MS_bits )
                MS_Band [Band] = BitstreamLE_read1 ();
            }
    }

    /********* Lese verwendetes Scalebandfactor-Splitting der 36 Samples pro Subband ************************/

    BITPOS(1);
    Table = HuffSCFI;
    for ( Band = 0; Band <= Max_Used_Band; Band++ ) {
        if ( Res[0][Band] )
            SCFI[0][Band] = HuffmanLE_Decode (Table);
        if ( Res[1][Band] )
            SCFI[1][Band] = HuffmanLE_Decode (Table);
    }

    /********* Lese Scalefaktors für alle Subbänder dreimal für jeweils 12 Samples **************************/

    BITPOS(2);
    Table = HuffDSCF;
    for ( Band = 0; Band <= Max_Used_Band; Band++ ) {

        if ( Res[0][Band] ) {

            switch ( SCFI[0][Band] ) {
            case 0:
                diff = Decode_DSCF ();
                SCF_Index[0][Band][0] = diff!=8  ?  SCF_Index[0][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[0][Band][1] = diff!=8  ?  SCF_Index[0][Band][0] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[0][Band][2] = diff!=8  ?  SCF_Index[0][Band][1] + diff  :  (Int) BitstreamLE_read(6);
                break;
            case 1:
                diff = Decode_DSCF ();
                SCF_Index[0][Band][0] = diff!=8  ?  SCF_Index[0][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[0][Band][1] =
                SCF_Index[0][Band][2] = diff!=8  ?  SCF_Index[0][Band][0] + diff  :  (Int) BitstreamLE_read(6);
                break;
            case 2:
                diff = Decode_DSCF ();
                SCF_Index[0][Band][0] =
                SCF_Index[0][Band][1] = diff!=8  ?  SCF_Index[0][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[0][Band][2] = diff!=8  ?  SCF_Index[0][Band][1] + diff  :  (Int) BitstreamLE_read(6);
                break;
            default:
                assert (0);
            case 3:
                diff = Decode_DSCF ();
                SCF_Index[0][Band][0] =
                SCF_Index[0][Band][1] =
                SCF_Index[0][Band][2] = diff!=8  ?  SCF_Index[0][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                break;
            }
        }

        if ( Res[1][Band] ) {

            switch ( SCFI[1][Band] ) {
            case 0:
                diff = Decode_DSCF ();
                SCF_Index[1][Band][0] = diff!=8  ?  SCF_Index[1][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[1][Band][1] = diff!=8  ?  SCF_Index[1][Band][0] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[1][Band][2] = diff!=8  ?  SCF_Index[1][Band][1] + diff  :  (Int) BitstreamLE_read(6);
                break;
            case 1:
                diff = Decode_DSCF ();
                SCF_Index[1][Band][0] = diff!=8  ?  SCF_Index[1][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[1][Band][1] =
                SCF_Index[1][Band][2] = diff!=8  ?  SCF_Index[1][Band][0] + diff  :  (Int) BitstreamLE_read(6);
                break;
            case 2:
                diff = Decode_DSCF ();
                SCF_Index[1][Band][0] =
                SCF_Index[1][Band][1] = diff!=8  ?  SCF_Index[1][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                diff = Decode_DSCF ();
                SCF_Index[1][Band][2] = diff!=8  ?  SCF_Index[1][Band][1] + diff  :  (Int) BitstreamLE_read(6);
                break;
            default:
                assert (0);
            case 3:
                diff = Decode_DSCF ();
                SCF_Index[1][Band][0] =
                SCF_Index[1][Band][1] =
                SCF_Index[1][Band][2] = diff!=8  ?  SCF_Index[1][Band][2] + diff  :  (Int) BitstreamLE_read(6);
                break;
            }
        }
    }

    /********* Lese die quantisierten Samples pro Subband (ohne Offsets, d.h. Werte liegen nullsymmetrisch) */

    BITPOS(3);
    for ( Band = 0; Band <= Max_Used_Band; Band++ ) {

        pp = QQ [0] [Band];
        switch ( Res[0][Band] ) {
        case -2:
            for ( k = 0; k < 36; k++ )
                *pp++ = 0;
            break;
        case -1:
#if 0
            if ( Res[0][Band-1] != -1 )
                CalculateTNS ( TNS[0], Res[0] + Band, (const quant_t(*)[36]) pp, Band );
            tmp = random_int ();
            for (k=0; k<36/2; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2)) * TNS [0][k];
            tmp = random_int ();
            for (k=36/2; k<36; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2)) * TNS [0][k];
#elif 0
            tmp = random_int ();
            for (k=0; k<36/2; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2));
            tmp = random_int ();
            for (k=36/2; k<36; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2));
#else
            for (k=0; k<36; k++ ) {
                tmp  = random_int ();
                *pp++ = ((tmp >> 24) & 0xFF) + ((tmp >> 16) & 0xFF) + ((tmp >>  8) & 0xFF) + ((tmp >>  0) & 0xFF) - 510;
            }
#endif
            break;
        case 0:
            // Subband samples are not used in this case, see Requant Routines
            break;
        case 1:
            if ( BitstreamLE_read1 () )
                for (k=0; k<36/3; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ1[1], LUT1_1,  9 );
                    *pp++ = tab30[idx];
                    *pp++ = tab31[idx];
                    *pp++ = tab32[idx];
                }
            else
                for (k=0; k<36/3; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ1[0], LUT1_0,  6 );
                    *pp++ = tab30[idx];
                    *pp++ = tab31[idx];
                    *pp++ = tab32[idx];
                }
            break;
        case 2:
            if ( BitstreamLE_read1 () )
                for (k=0; k<36/2; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ2[1], LUT2_1, 10 );
                    *pp++ = tab50[idx];
                    *pp++ = tab51[idx];
                }
            else
                for (k=0; k<36/2; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ2[0], LUT2_0,  7 );
                    *pp++ = tab50[idx];
                    *pp++ = tab51[idx];
                }
            break;
        case 3:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ3[1], LUT3_1,  5 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ3[0], LUT3_0,  4 );
            break;
        case 4:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ4[1], LUT4_1,  5 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ4[0], LUT4_0,  4 );
            break;
        case 5:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ5[1], LUT5_1,  8 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ5[0], LUT5_0,  6 );
            break;
        case 6:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTER  ( HuffQ6[1], LUT6_1,  7 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ6[0], LUT6_0,  7 );
            break;
        case 7:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTER  ( HuffQ7[1], LUT7_1,  8 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ7[0], LUT7_0,  8 );
            break;
        case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
            tmp = Dc7 [ Res[0][Band] ];
            for ( k = 0; k < 36; k++ )
                *pp++ = (Int) BitstreamLE_read (Res[0][Band]-1) - tmp;
            break;
        default:
            return -1;
        }

        pp = QQ [1] [Band];
        switch ( Res[1][Band] ) {
        case -2:
            for ( k = 0; k < 36; k++ )
                *pp++ = QQ [0] [Band] [k];
            break;
                case -1:
#if 0
            if ( Res[1][Band-1] != -1 )
                CalculateTNS ( TNS[1], Res[1] + Band, (const quant_t(*)[36]) pp, Band );
            tmp = random_int ();
            for (k=0; k<36/2; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2)) * TNS [1][k];
            tmp = random_int ();
            for (k=36/2; k<36; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2)) * TNS [1][k];
#elif 0
            tmp = random_int ();
            for (k=0; k<36/2; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2));
            tmp = random_int ();
            for (k=36/2; k<36; k++, tmp >>= 1)
                *pp++ = (int)(1 - (tmp & 2));
#else
            for (k=0; k<36; k++ ) {
                tmp  = random_int ();
                *pp++ = ((tmp >> 24) & 0xFF) + ((tmp >> 16) & 0xFF) + ((tmp >>  8) & 0xFF) + ((tmp >>  0) & 0xFF) - 510;
            }
#endif
            break;
        case 0:
            // Subband samples are not used in this case, see Requant Routines
            break;
        case 1:
            if ( BitstreamLE_read1 () )
                for (k=0; k<36/3; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ1[1], LUT1_1,  9 );
                    *pp++ = tab30[idx];
                    *pp++ = tab31[idx];
                    *pp++ = tab32[idx];
                }
            else
                for (k=0; k<36/3; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ1[0], LUT1_0,  6 );
                    *pp++ = tab30[idx];
                    *pp++ = tab31[idx];
                    *pp++ = tab32[idx];
                }
            break;
        case 2:
            if ( BitstreamLE_read1 () )
                for (k=0; k<36/2; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ2[1], LUT2_1, 10 );
                    *pp++ = tab50[idx];
                    *pp++ = tab51[idx];
                }
            else
                for (k=0; k<36/2; k++) {
                    idx  = HUFFMAN_DECODE_FASTEST ( HuffQ2[0], LUT2_0,  7 );
                    *pp++ = tab50[idx];
                    *pp++ = tab51[idx];
                }
            break;
        case 3:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ3[1], LUT3_1,  5 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ3[0], LUT3_0,  4 );
            break;
        case 4:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ4[1], LUT4_1,  5 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ4[0], LUT4_0,  4 );
            break;
        case 5:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ5[1], LUT5_1,  8 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ5[0], LUT5_0,  6 );
            break;
        case 6:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTER  ( HuffQ6[1], LUT6_1,  7 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ6[0], LUT6_0,  7 );
            break;
        case 7:
            if ( BitstreamLE_read1 () )
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTER  ( HuffQ7[1], LUT7_1,  8 );
            else
                for ( k = 0; k < 36; k++ )
                    *pp++ = HUFFMAN_DECODE_FASTEST ( HuffQ7[0], LUT7_0,  8 );
            break;
        case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
            tmp = Dc7 [ Res[1][Band] ];
            for ( k = 0; k < 36; k++ )
                *pp++ = (Int) BitstreamLE_read (Res[1][Band]-1) - tmp;
            break;
        default:
            return -1;
        }
    }

    BITPOS(4);

    if ( DUMPSELECT > 0 )
        Dump ( Max_Used_Band, 2, MS_Band, Res, SCF_Index, QQ, 0x07, __x );

    LEAVE(7);
    return Max_Used_Band;
}


/******************************************************************************************/
/****************************************** SV 7.F ****************************************/
/******************************************************************************************/


static int
bastelH ( Uchar mode, unsigned int additionalbits )
{
    static Schar  linbits [16] = {  4, 3, 2, 1, 0, 0,-1,-2,-3,-4,-5,-6,-7,-8,-9,-10 };
    static Uchar  offset  [16] = { 16, 8, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0 };
    int           tmp;

    tmp = BitstreamBE_read (additionalbits + linbits[mode]) + (offset[mode] << additionalbits);

    return BitstreamBE_read1 ()  ?  ~tmp  :  tmp;
}


static int
Read_Bitstream_SV7F ( Uint MS_bits, Uint channels )
{
    static Schar  tab30 [3* 3* 3* 3] = { -1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1,-1, 0,+1 };
    static Schar  tab31 [3* 3* 3* 3] = { -1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1,-1,-1,-1, 0, 0, 0,+1,+1,+1 };
    static Schar  tab32 [3* 3* 3* 3] = { -1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1 };
    static Schar  tab33 [3* 3* 3* 3] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1 };
    static Schar  tab50   [ 5* 5* 5] = { -2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2,-2,-1, 0,+1,+2 };
    static Schar  tab51   [ 5* 5* 5] = { -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2 };
    static Schar  tab52   [ 5* 5* 5] = { -2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2 };
    static Schar  tab70      [ 7* 7] = { -3,-2,-1, 0,+1,+2,+3,-3,-2,-1, 0,+1,+2,+3,-3,-2,-1, 0,+1,+2,+3,-3,-2,-1, 0,+1,+2,+3,-3,-2,-1, 0,+1,+2,+3,-3,-2,-1, 0,+1,+2,+3,-3,-2,-1, 0,+1,+2,+3 };
    static Schar  tab71      [ 7* 7] = { -3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,+2,+2,+3,+3,+3,+3,+3,+3,+3 };
    static Schar  tab90      [ 9* 9] = { -4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4,-4,-3,-2,-1, 0,+1,+2,+3,+4 };
    static Schar  tab91      [ 9* 9] = { -4,-4,-4,-4,-4,-4,-4,-4,-4,-3,-3,-3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,+2,+2,+2,+2,+3,+3,+3,+3,+3,+3,+3,+3,+3,+4,+4,+4,+4,+4,+4,+4,+4,+4 };
    static Schar  tab110     [11*11] = { -5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5 };
    static Schar  tab111     [11*11] = { -5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5 };
    static Schar  tab150     [15*15] = { -7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7,-7,-6,-5,-4,-3,-2,-1, 0,+1,+2,+3,+4,+5,+6,+7 };
    static Schar  tab151     [15*15] = { -7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-7,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+2,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+3,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+4,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+5,+6,+6,+6,+6,+6,+6,+6,+6,+6,+6,+6,+6,+6,+6,+6,+7,+7,+7,+7,+7,+7,+7,+7,+7,+7,+7,+7,+7,+7,+7 };
    static Uchar  tab1S0  [6*6*6+10] = {  0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 };
    static Uchar  tab1S1  [6*6*6+10] = {  0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 7, 8, 9,10,11,12,13,14,15 };
    static Uchar  tab1S2  [6*6*6+10] = {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 7, 8, 9,10,11,12,13,14,15 };

//  Float               TNS [2] [36];
    Int                 Band;
    Uint                ch;
    Int                 Max_Used_Band;
    Uint                k;
    quant_t*            pp;
    Int                 diff;
    Uint                idx;
    Uint32_t            tmp;

    ENTER(7);

    Max_Used_Band = HuffDecode_LUT ( &Decodertable_MAXSUB );
    memset ( Res, 0, sizeof Res );

    /********* Lese Auflösung und LR/MS ********************************************************************/

    BITPOS(0);

    if ( Max_Used_Band >= 0 ) {
        for ( ch = 0; ch < channels; ch++ ) {
            Band = 0;
            tmp  = 0;
            diff = HuffDecode_LUT ( &Decodertable_HDR0 );
            if ( diff == 23 ) {
   setallbands: for ( ; Band <= Max_Used_Band; Band++ )
                    Res[ch][Band] = tmp;
                continue;
            }
            Res[ch][Band++] = tmp = diff;

            if ( Max_Used_Band >= 1 ) {
                diff = HuffDecode ( &Decodertable_DHDR1 );
                diff = diff != 5  ?  tmp + diff  :  (Int) BitstreamBE_read (5)-8;
                if ( diff == 23 )
                    goto setallbands;
                Res[ch][Band++] = tmp = diff;

                for ( ; Band <= Max_Used_Band; ) {
                    diff = HuffDecode ( &Decodertable_DHDR );
                    diff = diff != 8  ?  tmp + diff  :  (Int) BitstreamBE_read (5)-8;
                    if ( diff == 23 )
                        goto setallbands;
                    Res[ch][Band++] = tmp = diff;
                }
            }
        }
    }

    memset ( MS_Band, 0, sizeof MS_Band );
    if ( MS_bits > 0 ) {
        for ( Band = 0; Band <= Max_Used_Band; Band++ )
            if ( Res[0][Band]  ||  Res[1][Band] )
                 MS_Band [Band] = BitstreamBE_read ( MS_bits );
    }

    /********* Lese Scalefaktors für alle Subbänder dreimal für jeweils 12 Samples **************************/

    BITPOS(1);
    BITPOS(2);

    for ( ch = 0; ch < channels; ch++ ) {
        for ( Band = 0; Band <= Max_Used_Band; Band++ ) {
            if ( Res[ch][Band] ) {
                switch ( HuffDecode_onlyLUT ( &Decodertable_SCFI ) ) {
                case 0:
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][0] = diff!=11  ?  SCF_Index[ch][Band][2] + diff  :  (Int) BitstreamBE_read(7);
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][1] = diff!=11  ?  SCF_Index[ch][Band][0] + diff  :  (Int) BitstreamBE_read(7);
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][2] = diff!=11  ?  SCF_Index[ch][Band][1] + diff  :  (Int) BitstreamBE_read(7);
                    break;
                case 1:
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][0] = diff!=11  ?  SCF_Index[ch][Band][2] + diff  :  (Int) BitstreamBE_read(7);
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][1] =
                    SCF_Index[ch][Band][2] = diff!=11  ?  SCF_Index[ch][Band][0] + diff  :  (Int) BitstreamBE_read(7);
                    break;
                case 2:
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][0] =
                    SCF_Index[ch][Band][1] = diff!=11  ?  SCF_Index[ch][Band][2] + diff  :  (Int) BitstreamBE_read(7);
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][2] = diff!=11  ?  SCF_Index[ch][Band][1] + diff  :  (Int) BitstreamBE_read(7);
                    break;
                default:
                    assert (0);
                case 3:
                    diff = HuffDecode_onlyLUT ( &Decodertable_DSCF );
                    SCF_Index[ch][Band][0] =
                    SCF_Index[ch][Band][1] =
                    SCF_Index[ch][Band][2] = diff!=11  ?  SCF_Index[ch][Band][2] + diff  :  (Int) BitstreamBE_read(7);
                    break;
                }
            }
        }
    }

    /********* Lese die quantisierten Samples pro Subband (ohne Offsets, d.h. Werte liegen nullsymmetrisch) */

    BITPOS(3);
    for ( ch = 0; ch < channels; ch++ ) {
        for ( Band = 0; Band <= Max_Used_Band; Band++ ) {
            pp = QQ [ch] [Band];
            switch ( Res[ch][Band] ) {
            case -8:
            case -7:
            case -6:
            case -5:
            case -4:
            case -3:
            case -2:
            case -1:
                idx = ch + Res[ch][Band];
                for ( k = 0; k < 36; k++ )
                    *pp++ = QQ [idx] [Band] [k];
                break;
            case  0:
                // Subband samples are not used in this case, see Requant Routines
                break;
            case  1:
                for ( k = 0; k < 36; k++ ) {
                    tmp  = random_int ();
                    *pp++ = ((tmp >> 24) & 0xFF) + ((tmp >> 16) & 0xFF) + ((tmp >>  8) & 0xFF) + ((tmp >>  0) & 0xFF) - 510;
                }
                break;
            case  2:
                for ( k = 0; k < 36; k++ ) {
                    *pp++ = BitstreamBE_read1 () & 1  ?  +1  :  -1;
                }
                break;
            case  3:
                switch ( HuffDecode_onlyLUT ( &Decodertable_Q1 ) ) {
                case 0:
                    for ( k = 0; k < 36/4; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q11 );
                        *pp++ = tab30 [idx];
                        *pp++ = tab31 [idx];
                        *pp++ = tab32 [idx];
                        *pp++ = tab33 [idx];
                    }
                    break;
                case 1:
                    for ( k = 0; k < 36/4; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q12 );
                        *pp++ = tab30 [idx];
                        *pp++ = tab31 [idx];
                        *pp++ = tab32 [idx];
                        *pp++ = tab33 [idx];
                    }
                    break;
                case 2:
                    for ( k = 0; k < 36/4; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q13 );
                        *pp++ = tab30 [idx];
                        *pp++ = tab31 [idx];
                        *pp++ = tab32 [idx];
                        *pp++ = tab33 [idx];
                    }
                    break;
                case 3:
                    for ( k = 0; k < 36/4; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q14 );
                        *pp++ = tab30 [idx];
                        *pp++ = tab31 [idx];
                        *pp++ = tab32 [idx];
                        *pp++ = tab33 [idx];
                    }
                    break;
                }
                break;
            case  4:
                switch ( HuffDecode_onlyLUT ( &Decodertable_Q2 ) ) {
                case 0:
                    for ( k = 0; k < 36/3; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q21 );
                        *pp++ = tab50 [idx];
                        *pp++ = tab51 [idx];
                        *pp++ = tab52 [idx];
                    }
                    break;
                case 1:
                    for ( k = 0; k < 36/3; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q22 );
                        *pp++ = tab50 [idx];
                        *pp++ = tab51 [idx];
                        *pp++ = tab52 [idx];
                    }
                    break;
                case 2:
                    for ( k = 0; k < 36/3; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q23 );
                        *pp++ = tab50 [idx];
                        *pp++ = tab51 [idx];
                        *pp++ = tab52 [idx];
                    }
                    break;
                case 3:
                    for ( k = 0; k < 36/3; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q24 );
                        *pp++ = tab50 [idx];
                        *pp++ = tab51 [idx];
                        *pp++ = tab52 [idx];
                    }
                    break;
                }
                break;
            case  5:
                switch ( HuffDecode_onlyLUT ( &Decodertable_Q3 ) ) {
                case 0:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q31 );
                        *pp++ = tab70 [idx];
                        *pp++ = tab71 [idx];
                    }
                    break;
                case 1:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q32 );
                        *pp++ = tab70 [idx];
                        *pp++ = tab71 [idx];
                    }
                    break;
                case 2:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q33 );
                        *pp++ = tab70 [idx];
                        *pp++ = tab71 [idx];
                    }
                    break;
                case 3:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q34 );
                        *pp++ = tab70 [idx];
                        *pp++ = tab71 [idx];
                    }
                    break;
                }
                break;
            case  6:
                switch ( BitstreamBE_read1 () ) {
                case 0:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q41 );
                        *pp++ = tab90 [idx];
                        *pp++ = tab91 [idx];
                    }
                    break;
                case 1:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q42 );
                        *pp++ = tab90 [idx];
                        *pp++ = tab91 [idx];
                    }
                    break;
                }
                break;
            case  7:
                switch ( BitstreamBE_read1 () ) {
                case 0:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q51 );
                        *pp++ = tab110 [idx];
                        *pp++ = tab111 [idx];
                    }
                    break;
                case 1:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q52 );
                        *pp++ = tab110 [idx];
                        *pp++ = tab111 [idx];
                    }
                    break;
                }
                break;
            case  8:
                switch ( BitstreamBE_read1 () ) {
                case 0:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q71 );
                        *pp++ = tab150 [idx];
                        *pp++ = tab151 [idx];
                    }
                    break;
                case 1:
                    for ( k = 0; k < 36/2; k++ ) {
                        idx  = HuffDecode_LUT ( &Decodertable_Q72 );
                        *pp++ = tab150 [idx];
                        *pp++ = tab151 [idx];
                    }
                    break;
                }
                break;
            case  9:
                switch ( BitstreamBE_read1 () ) {
                case 0:
                    for ( k = 0; k < 36; k++ )
                        *pp++ = HuffDecode_LUT ( &Decodertable_Q101 );
                    break;
                case 1:
                    for ( k = 0; k < 36; k++ )
                        *pp++ = HuffDecode_LUT ( &Decodertable_Q102 );
                    break;
                }
                break;
            case 10:
                switch ( BitstreamBE_read1 () ) {
                case 0:
                    for ( k = 0; k < 36; k++ )
                        *pp++ = HuffDecode_LUT ( &Decodertable_Q151 );
                    break;
                case 1:
                    for ( k = 0; k < 36; k++ )
                        *pp++ = HuffDecode_LUT ( &Decodertable_Q152 );
                    break;
                }
                break;
            case 11:
                switch ( BitstreamBE_read1 () ) {
                case 0:
                    for ( k = 0; k < 36; k++ )
                        *pp++ = HuffDecode_LUT ( &Decodertable_Q311 );
                    break;
                case 1:
                    for ( k = 0; k < 36; k++ )
                        *pp++ = HuffDecode_LUT ( &Decodertable_Q312 );
                    break;
                }
                break;
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 21:
                switch ( HuffDecode_onlyLUT ( &Decodertable_BIGT ) ) {
                case 0:
                    tmp = Res[ch][Band] - 11;
                    for ( k = 0; k < 36; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIG1 );
                        *pp++ = ( idx << tmp ) | BitstreamBE_read ( tmp );
                    }
                    break;
                case 1:
                    tmp = Res[ch][Band] - 11;
                    for ( k = 0; k < 36; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIG2 );
                        *pp++ = ( idx << tmp ) | BitstreamBE_read ( tmp );
                    }
                    break;
                case 2:
                    tmp = Res[ch][Band] - 9;
                    //printf ("2: ");
                    for ( k = 0; k < 36/2; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIG3 );
                        *pp++ = ( ((idx>>4)-8) << tmp ) | BitstreamBE_read ( tmp );
                        *pp++ = ( ((idx&15)-8) << tmp ) | BitstreamBE_read ( tmp );
                        //printf ("%7d%7d", pp[-2],pp[-1]);
                    }
                    //printf ("\n");
                    break;
                case 3:
                    tmp = Res[ch][Band] - 9;
                    //printf ("3: ");
                    for ( k = 0; k < 36/2; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIG4 );
                        *pp++ = ( ((idx>>4)-8) << tmp ) | BitstreamBE_read ( tmp );
                        *pp++ = ( ((idx&15)-8) << tmp ) | BitstreamBE_read ( tmp );
                        //printf ("%7d%7d", pp[-2],pp[-1]);
                    }
                    //printf ("\n");
                    break;
                case 4:
                    tmp = Res[ch][Band] - 11;
                    for ( k = 0; k < 36/3; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIGV1 );
                        *pp++  = bastelH ( tab1S0 [idx], tmp );
                        *pp++  = bastelH ( tab1S1 [idx], tmp );
                        *pp++  = bastelH ( tab1S2 [idx], tmp );
                    }
                    break;
                case 5:
                    tmp = Res[ch][Band] - 11;
                    for ( k = 0; k < 36/3; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIGV2 );
                        *pp++  = bastelH ( tab1S0 [idx], tmp );
                        *pp++  = bastelH ( tab1S1 [idx], tmp );
                        *pp++  = bastelH ( tab1S2 [idx], tmp );
                    }
                    break;
                case 6:
                    tmp = Res[ch][Band] - 11;
                    for ( k = 0; k < 36/3; k++ ) {
                        idx   = HuffDecode_LUT ( &Decodertable_BIGV3 );
                        *pp++  = bastelH ( tab1S0 [idx], tmp );
                        *pp++  = bastelH ( tab1S1 [idx], tmp );
                        *pp++  = bastelH ( tab1S2 [idx], tmp );
                    }
                    break;
                }
                break;
            default:
                return -1;
            }
        }
    }
    BITPOS(4);

    if ( BitsRead() & 7 )
        (void) BitstreamBE_read ( 8 - (BitsRead() & 7) );

    if ( DUMPSELECT > 0 )
        Dump ( Max_Used_Band, channels, MS_Band, Res, SCF_Index, QQ, 0xF7, __x );

    LEAVE(7);
    return Max_Used_Band;
}


/******************************************************************************************/
/********************************** External Interface ************************************/
/******************************************************************************************/


int
Read_Bitstream ( Uint StreamVersion, Uint MS_bits, Uint channels )
{
    switch ( StreamVersion ) {
    case 0x04:
    case 0x05:
    case 0x06:
        return Read_Bitstream_SV6  ( MS_bits );
    case 0x07:
    case 0x17:
    case 0x27:
        return Read_Bitstream_SV7  ( MS_bits );
    case 0xF7:
    case 0xFF:
        return Read_Bitstream_SV7F ( MS_bits, channels );
    default:
        assert (0);
        return -1;
    }
}

/* end of decode.c */
