/*
mppdec: --dumpselect xxx

0:     no dump
Bit 0: maximum used band (-1...31)
Bit 1: M/S switching bits
Bit 2: subband resolution (-2...17)
Bit 3: scale Band Factors (-2...127)
Bit 4: subband samples
Bit 5: data rate
Bit 6: bit usage of the sections
Bit 7: requantized subband samples
*/


static Int
digits ( Int no )
{
    if (no >= 0) {
        if ( no <=     9 ) return 1;
        if ( no <=    99 ) return 2;
        if ( no <=   999 ) return 3;
        if ( no <=  9999 ) return 4;
        return 5;
    }
    else {
        if ( no >=    -9 ) return 2;
        if ( no >=   -99 ) return 3;
        if ( no >=  -999 ) return 4;
        if ( no >= -9999 ) return 5;
        return 6;
    }
}


static void
Dump ( Int                Max_Used_Band,
       Int                Channels,
       const Bool_t*      MS_Band,
       /*const*/ res_t    Res       [] [32],
       /*const*/ scf_t    SCF_Index [] [32] [ 3],
       /*const*/ quant_t  Q         [] [32] [36],
       Int                SV,
       const Ulong*       bitpostab )
{
    static Ulong     Frame= 0;
    static FILE*     fp0  = NULL;
    static FILE*     fp1  = NULL;
    static FILE*     fp2  = NULL;
    static FILE*     fp3  = NULL;
    static FILE*     fp4  = NULL;
    static FILE*     fp5  = NULL;
    static FILE*     fp6  = NULL;
    static FILE*     fp7  = NULL;
    static Int       Init = 0;
    static Uint32_t  Bits = (Uint32_t)-1;
    Int              Band;
    Int              ch;
    Int              i;
    Int              k;
    Int              d;

    if ( Init == 0 ) {
        fp0  = fopen ( LOGPATH "report-maxband.txt", "a" );
        fp1  = fopen ( LOGPATH "report-msbits.txt" , "a" );
        fp2  = fopen ( LOGPATH "report-resol.txt"  , "a" );
        fp3  = fopen ( LOGPATH "report-scf.txt"    , "a" );
        fp4  = fopen ( LOGPATH "report-quant.txt"  , "a" );
        fp5  = fopen ( LOGPATH "report-rate.txt"   , "a" );
        fp6  = fopen ( LOGPATH "report-usage.txt"  , "a" );
        fp7  = fopen ( LOGPATH "report-requant.txt", "a" );
        Init = 1;
    }

    // Stop if 400 MByte has been dumped for each log file
    if ( fp0 != NULL  &&  ftell (fp0) > 409600000 ) fp0 = NULL;
    if ( fp1 != NULL  &&  ftell (fp1) > 409600000 ) fp1 = NULL;
    if ( fp2 != NULL  &&  ftell (fp2) > 409600000 ) fp2 = NULL;
    if ( fp3 != NULL  &&  ftell (fp3) > 409600000 ) fp3 = NULL;
    if ( fp4 != NULL  &&  ftell (fp4) > 409600000 ) fp4 = NULL;
    if ( fp5 != NULL  &&  ftell (fp5) > 409600000 ) fp5 = NULL;
    if ( fp6 != NULL  &&  ftell (fp6) > 409600000 ) fp6 = NULL;

    // last used band
    if ((DUMPSELECT & 1)  &&  fp0 != NULL) {
        fprintf ( fp0, "%4lu\t%2d\n", Frame, Max_Used_Band );
    }

    // last used band + MS bits for every band
    if ((DUMPSELECT & 2)  &&  fp1 != NULL) {
        fprintf ( fp1, "%4lu\t%2d\t", Frame, Max_Used_Band );
        for ( Band = 0; Band <= Max_Used_Band; Band++ )
            if ( Res[0][Band]  ||  Res[1][Band] )
                fprintf ( fp1, MS_Band[Band] ? "#" : "." );
            else
                fprintf ( fp1, " " );
        fprintf ( fp1, "\n" );
    }

    // Resolution for every band and channel AND also MS bits
    if ((DUMPSELECT & 4)  &&  fp2 != NULL) {
        fprintf ( fp2, "--- %4lu ---\n", Frame );

        for ( ch = 0; ch < Channels; ch++ ) {
            for ( Band = 0; Band <= Max_Used_Band; Band++ )
                if ( Res[ch][Band] )
                    fprintf ( fp2, " %2d", Res[ch][Band] );
                else
                    fprintf ( fp2, "   " );
            fprintf ( fp2, "\n" );
        }

        for ( Band = 0; Band <= Max_Used_Band; Band++ )
            if ( Res[0][Band]  ||  Res[1][Band] )
                fprintf ( fp2, "  %c", MS_Band[Band] ? '#' : '.' );
            else
                fprintf ( fp2, "   " );
        fprintf ( fp2, "\n\n" );
    }

    // SCF for every channel, subframe and subband
    if ((DUMPSELECT & 8)  &&  fp3 != NULL) {
        fprintf ( fp3, "--- %4lu --- (SV%u.%u) ---\n", Frame, SV&15, SV>>4 );

        for (ch = 0; ch < Channels; ch++ ) {
            for ( i = 0; i < 3; i++ ) {
                for ( Band = 0; Band <= Max_Used_Band; Band++ )
                    if ( Res[ch][Band] )
                        fprintf ( fp3, " %3d", SV > 7  ?  SCF_Index[ch][i][Band]  :  63 - SCF_Index[ch][i][Band] );
                    else
                        fprintf ( fp3, "    " );
                fprintf ( fp3, "\n" );
            }
        }

        fprintf ( fp3, "\n" );
    }

    // quantized subband samples
    if ((DUMPSELECT & 16)  &&  fp4 != NULL) {
        int  max = 0;

        fprintf ( fp4, "--- %4lu ---\n", Frame );

        for ( ch = 0; ch < Channels; ch++ ) {
            for ( k = 0; k < 36; k++ ) {
                d = digits (Q [ch] [0] [k]); if ( d > max ) max = d;
                for ( Band = 1; Band <= Max_Used_Band; Band++ ) {
                    d = 1 + digits (Q [ch] [Band] [k]); if ( d > max ) max = d;
                }
            }
        }

        for ( ch = 0; ch < Channels; ch++ ) {
            for ( k = 0; k < 36; k++ ) {
                for ( Band = 0; Band <= Max_Used_Band; Band++ )
                    if ( Res[ch][Band] )
                        fprintf ( fp4, "%*d", max, Q [ch] [Band] [k] );
                    else
                        fprintf ( fp4, "%*s", max, "" );
                fprintf ( fp4, "\n");
            }
            fprintf ( fp4, "\n");
        }

        fprintf ( fp4, "\n\n");

    }

    // Blockgröße und Datenrate
    if ((DUMPSELECT & 32)  &&  fp5 != NULL) {
        if ( Bits > BitsRead () ) Bits = 200;
        k    = BitsRead () - Bits;
        Bits = BitsRead ();
        fprintf ( fp5, "%4lu\t%5u bit\t%5.1f kbps\n", Frame, k, k * (44.1/1152) );
    }

    // Bitverbrauch der einzelnen Sektionen
    if ((DUMPSELECT & 64)  &&  fp6 != NULL) {

        fprintf ( fp6, "%4lu %4lu%5lu%5lu%6lu\n", Frame,
                  bitpostab[1] - bitpostab[0], bitpostab[2] - bitpostab[1],
                  bitpostab[3] - bitpostab[2], bitpostab[4] - bitpostab[3] );
    }

    // requantized subband samples
    if ((DUMPSELECT & 128)  &&  fp7 != NULL) {

        fprintf ( fp7, "--- %4lu ---\n", Frame );

        for ( ch = 0; ch < Channels; ch++ ) {
            for ( k = 0; k < 36; k++ ) {
                for ( Band = 0; Band <= Max_Used_Band; Band++ )
                    fprintf ( fp7, "%9.1f", Q [ch] [Band] [k] * SCF[SCF_Index[ch][k/12][Band]] * Cc7[Res[ch][Band]] );
                fprintf ( fp7, "\n");
            }
            fprintf ( fp7, "\n");
        }
    }

    Frame++;
    return;
}

/* end of dump.c */
