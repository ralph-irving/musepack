/*
 *
 *  Main Decoder Loop
 *
 *
 *  (C) Copyright 1999-2001  Andree Buschmann. All rights reserved.
 *  (C) Copyright 2001-2003  Frank Klemm. All rights reserved.
 *
 *  For licencing see the licencing file which is part of this package.
 *
 *
 *  Principles:
 *    Current decoder uses the so called pull model. The decoder is started, take full
 *    control of the current thread and pulls input data when it need input data and
 *    writes the decoded data to the destination when it has decoded data.
 *    A decoder using this principle is difficult to use by other programs, because it can't
 *    be controlled by another program, it can only control other programs.
 *
 *    Most work for changing this model must be done in Decode() and DecodeFile().
 *
 *  History:
 *    1999-2003     programmed by Frank Klemm and Andree Buschmann
 *    2003-02-02    the file got an header
 *
 *  Global functions:
 *    - main()
 *    - but also some remaining global variables
 *
 *  TODO:
 *   - removal of global variables
 *   - multithreading support
 *   - fast forward using skip information instead of decoding (although seeking speed is hard disk read speed limited on current computers).
 *   - usage of IO and AudioIO lib for input and output
 *   - push pull transformation
 *
 */

#include <time.h>
#include <string.h>
#include <errno.h>

#include "mppdec.h"


// global variables (ugly, not killed yet)
quant_t           QQ       [MAXCH] [32] [36];                   // quantized matrixed subband samples
Float             YY       [MAXCH] [36] [32];                   // scaled dematrixed subband samples
Float             VV       [MAXCH] [1024 + 64*(VIRT_SHIFT-1)];  // 1st stage of the subband synthesizer
scf_t             SCF_Index[MAXCH] [32] [ 3];                   // scale factors for transforming QQ -> YY
res_t             Res      [MAXCH] [32];                        // sample resolution for decoding Bitstream -> QQ
Bool_t            MS_Band          [32];                        // dematrixing information for transforming QQ -> YY

static Int        V_offset [MAXCH];

static Uint       MS_bits            =  0;                      // 0: all is LR coded, 1: MS or LR coding per subband
Bool_t            IS_used            =  0;                      // is IS used (if yes, a fixed number of subbands is IS coded)
static Float      Scale              =  1.;                     // user defined scale factor
static Bool_t     ClipPrev           =  0;                      // if set, clipping is prevented if needed information is available
static int        ReplayGainType     =  0;                      // 0: no additional gain, 1: CD based gain correction, 2: title based gain correction

Bool_t            TrueGaplessPresent =  0;                      // is true gapless used?
Int               LastValidSamples   =  0;                      // number of valid samples within last frame
Uint              DUMPSELECT         =  0;
unsigned int      SampleFreq         =  44100;
static int        Channels           =  2;
static const Uint16_t
                  sftable [4] = { 44100, 48000, 37800, 32000 };

#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT
static int        Bits               = SAMPLE_SIZE;
static int        NoiseShapeType     =  0;
static Float      Dither             = -1;                      // Dithering if not Noise shaping
#endif

static Uint       StreamVersion;
static Ulong      InputBuffRead;
#ifdef USE_ASM
static SyntheseFilter16_t
                  Synthese_Filter_16;
#endif
Bool_t            output_endianess   = LITTLE_ENDIAN;
Ulong             errors             = 0;


const char        About        []    = "MPC Decoder  " MPPDEC_VERSION "  " BUILD "   " COPYRIGHT;
const char        CompileFlags []    = COMPILER_FLAGS;
const char        Date         []    = __DATE__ " " __TIME__;


/**********************************************************************************************************************************
 *
 *  Functions: helper functions for Decode()
 *
 *  PrintTime():
 *    computes a 12 character long time string (10 ms precision) from a given
 *    sample count. An optional character can be prefixed.
 *  FrameSize():
 *    Read the size of the next frame in bits from the bit stream.
 *  ReadAndVerifyFrame():
 *    Reads a frame and verify the correct contents by checking the decoded length by the length read via FrameSize().
 *    This is not a secure test (especially for SV8), but typical errors you find on hard disks and which occured
 *    using FTP or HTTP downstream can be recognized.
 *    Argument IgnoreErrors can be used to read known defect frames (SV4/5 last frame ist mostly corrupted),
 *    the last two arguments are only used to write a useful error message.
 *  Requantize():
 *    Compute subband samples from quantized subband samples. Do also MS dematrixing.
 *    Arguments MinBand and MaxBand are handling the stuff which bands are MS and which are IS encoded.
 *    These are all AB names and should be replaced by useful names.
 */

static const char*
PrintTime ( UintMax_t  samples,
            int        sign )
{
    static char  ret [16];
    Ulong        tmp  = (Ulong)( UintMAX_FP(samples) * 100.f / SampleFreq + 0.5 );  // unit: 1/100th second
    Uint         hour = (Uint) ( tmp / 360000       );
    Uint         min  = (Uint) ( tmp /   6000 %  60 );
    Uint         sec  = (Uint) ( tmp /    100 %  60 );
    Uint         csec = (Uint) ( tmp          % 100 );

    if      ( hour > 9 )
        sprintf ( ret,  "%c%2u:%02u", sign, hour, min );
    else if ( hour > 0 )
        sprintf ( ret, " %c%1u:%02u", sign, hour, min );
    else if ( min  > 9 )
        sprintf ( ret,    "   %c%2u", sign,       min );
    else
        sprintf ( ret,   "    %c%1u", sign,       min );

    sprintf ( ret + 6,   ":%02u.%02u", sec, csec );
    return ret;
}


static Uint
FrameSize ( int StreamVersion )
{
    if ( StreamVersion & 0x08 )
        return BitstreamBE_read (16) * 8;
    else
        return BitstreamLE_read (20);
}


static int
ReadAndVerifyFrame ( int    StreamVersion,
                     int    IgnoreErrors,
                     Ulong  CurrentFrame,
                     Ulong  FrameCount )
{
    Uint32_t  CurrBlkSize;
    Uint32_t  StartBitPos;
    Uint32_t  EndBitPos;

    CurrBlkSize = FrameSize (StreamVersion);

    StartBitPos = BitsRead ();
    Read_Bitstream ( StreamVersion, MS_bits, Channels );        // decode bitstream (see decode.c)
    EndBitPos   = BitsRead ();

    // check skip information against value determined by decoding (buggy for CBR)
    if ( EndBitPos - StartBitPos != CurrBlkSize  &&  !IgnoreErrors ) {
        errors++;
        if ( BitsRead() < InputBuffRead * (Ulong)(CHAR_BIT * sizeof(*InputBuff)) )
            stderr_printf ( "\n\n"PROG_NAME": broken frame %lu/%lu (decoded size=%lu, size in stream=%lu)\a\n\n",
                            CurrentFrame, FrameCount, (Ulong)(EndBitPos - StartBitPos), (Ulong)CurrBlkSize );
        else
            stderr_printf ("\n\n"PROG_NAME": unexpected end of file after frame %lu/%lu\a\n\n", CurrentFrame, FrameCount );
        return -1;
    }

    return 0;
}


static void
Requantize ( const Bool_t*  MSBand,
             int            ISused,
             int            MinBand,
             int            MaxBand,
             int            StreamVersion )
{
    if ( ISused ) {
        Requantize_MidSideStereo   ( MinBand-1, MS_Band, StreamVersion );
        Requantize_IntensityStereo ( MinBand,  MaxBand );
    }
    else {
        Requantize_MidSideStereo   ( MaxBand,   MSBand, StreamVersion );
    }
}


/**********************************************************************************************************************************
 *
 *  Function: Decode()
 *
 *  This is the main decoder loop, which decodes the file frame by frame.
 *
 *  This function has the following things to do:
 *   - Read the stream frame by frame
 *   - refill the bitstream buffer, so there's always enough data in the buffer to decode a frame (even when it is a monster frame with 34 kbits).
 *   - requantization and synthesis of the PCM signal
 *   - Outputing some messages from time to time about the progress of decoding
 *   - compensation of analysis+synthesis filter delay (481 samples).
 *   - last block handling, so the decoded file is exactly as long as the source file.
 *
 *  Some remarks (please read this and ask when you don't understand something!):
 *   - analysis + synthesis filter together has a delay of exactly 481 samples - MP1/MP2 has the same delay
 *   - all windowed transform based codecs (Musepack is also one) have such a delay
 *   - that musepack has not such a delay is that only one encoder and one decoder exist, which do compensate this perfectly,
 *     it is not a property of musepack as proecess itself.
 *   - decoder discards the first 481 samples of the decoded PCM signal of the first block.
 *     The remaining blocks are decoded fully, the last block must generate 481 samples more than the
 *     bitstreams says.
 *   - Original AB stream syntax:
 *     + header, contains number N of frames in the file
 *     + N frames
 *     + 11 bits, how many samples R the last frame contains without the delay compensation
 *   - two bugs
 *     + when R is 1152, it was encoded as 0. Decoded file was 1152 samples too short
 *     + in the worst case the last frame must produce 1152+481 samples. AB assumed,
 *       that a single encoded frame can produce 1152+481 samples, but it can't.
 *       From the mathematical point it can only produce 1152 samples, from the practical 1152+160...170,
 *       after 1152+240 samples the output amplitude has dropped by 6 dB, after 1152+481 samples by 100 dB.
 *     + So version 1.01 and above encode an additional frame after these 11 bits when the last frame
 *       must produce more than 1152 samples.
 *   - Current stream syntax:
 *     + header, contains number N of frames in the file
 *     + N frames
 *     + 11 bits, how many samples R the last frame contains without the delay compensation
 *     + optional frame, when R+481 > 1152
 *   - That such an additional frame CAN be possible, is marked in the header by a special bit.
 *   - In this case the 11 bit word R is also stored in the header, but currently not used in SV7.
 *     (but can be used to determine the exact length of the file by only reading the header,
 *     otherwise you must read the whole file).
 *   - This fixes the problem of these gaps without breaking compatibility.
 *
 *  Nevertheless Musepack is not absolutely gapless. For an absolutely gapless lossy encoder
 *  you have still the problem of swinging in and out of the analysis and synthesis filter.
 *  You can see this effect when you feed pure DC to any encoder. To solve this problem there
 *  are two ways:
 *    - start and end must be encoded lossless
 *    - you must also feed the encoder with the end samples of the predecessor and the beginning
 *      of the sucessor. From the mathematical view 481 samples are enough, from the practical
 *      240...256 samples (-96...-100 dB).
 *    - current pre SV8 encoder can be called via:
 *        mppenc *.wav destdir
 *      which can handle this problem perfectly (when the GUI calls the program right).
 *      Currently this code is not programmed.
 *    - when this method is used, 481+481 samples from the first block must be discarded.
 *      1152-481-481=190 samples remaining. May be I discard the whole first block in the decoder
 *      and adjust the encoder so this gives the right result.
 *    - SV7 contains a lot of implicit information about this process. It SV8 they shoudl bes tored explicitely.
 *      Should this be done:
 *      - in the audioframes internally
 *      - or as additional information between the frames?
 */

static UintMax_t
Decode ( FILE_T     OutputFile,
         FILE_T     InputFile,
         Uint32_t   TotalFrames,
         UintMax_t  Start,
         UintMax_t  Duration,
         size_t     valid_end )
{
    IntSample_t  Stream  [ BLK_SIZE * MAXCH ];
    size_t       ring;
    size_t       valid;
    Uint32_t     FrameNo;
    UintMax_t    ret = 0;
    time_t       T   = time (NULL);

    ENTER(3);


    memset ( Stream, 0, sizeof Stream );


    for ( FrameNo = 0; FrameNo < TotalFrames  &&  Duration > 0; FrameNo++ ) {    // decode frame by frame, note some differences in decoding the last frame (TotalFrames-1)
        ring = InputCnt;

        if ( ReadAndVerifyFrame ( StreamVersion, FrameNo == TotalFrames-1  &&  StreamVersion <= 5, FrameNo, TotalFrames ) < 0 ) {
            LEAVE(3);
            return ret;
        }

        // reload data if more than 50% of the buffer is decoded
        if ( (ring ^ InputCnt) & IBUFSIZE2 )
            InputBuffRead += READ ( InputFile, InputBuff + (ring & IBUFSIZE2), IBUFSIZE2 * 4 ) / 4;

        // Subband synthesizer
        if ( Start <= BLK_SIZE ) {
            if ( Scale != 0. ) {
                Requantize      ( MS_Band, IS_used, Min_Band, Max_Band, StreamVersion );
                Synthese_Filter ( Stream + 0, V_offset+0, VV [0], YY [0], Channels, 0 );
                Synthese_Filter ( Stream + 1, V_offset+1, VV [1], YY [1], Channels, 1 );
            }

            // write PCM data to destination (except the data in the last frame, this is done behind the for loop
            if ( FrameNo < TotalFrames-1 ) {
                valid     = BLK_SIZE - Start;
                if ( valid > Duration )
                    valid = Duration;
                ret      += Write_PCM ( OutputFile, Stream + Start * Channels, valid * Channels ) / Channels;
                Duration -= valid;
                Start     = 0;
            }
        }
        else {
            Start -= BLK_SIZE;
        }

        // output report if a real time second is over sinse the last report
        if ( (Int)(time (NULL) - T) >= 0 ) {
            T += 1;
            stderr_printf ("\r%s/", PrintTime ( Start  ?  Start  :  ret, Start  ?  '-'  :  ' ' ) );
            stderr_printf ("%s %s (%4.1f%%)", PrintTime ( (UintMax_t)TotalFrames * BLK_SIZE, ' ') + 1, Start  ?  "jumping"  :  "decoded", 100. * FrameNo / TotalFrames );
        }
    }


    if ( Duration > 0 ) {                                               // write PCM data to destination for the last frame
        switch ( StreamVersion ) {                                      // reconstruct exact size for SV6 and higher (for SV4...5 this is unknown)
        default:
            assert (0);

        case 0x04:
        case 0x05:
            valid_end = 0;
            break;

        case 0x06:
        case 0x07:
        case 0x17:
            valid_end = (Int) BitstreamLE_read (11);
            if (valid_end == 0)
                valid_end = BLK_SIZE;               // Old encoder writes a 0 instead of a 1152, Bugfix
            /* fall through */

        case 0xFF:
            valid_end += DECODER_DELAY - Start;
            if ( valid_end > Duration ) valid_end = Duration;

            if ( Start + valid_end > BLK_SIZE ) {
                // write out data for the last frame
                if ( Start < BLK_SIZE ) {
                    ret   += Write_PCM ( OutputFile, Stream + Start * Channels, (size_t)((BLK_SIZE - Start) * Channels) ) / Channels;
                    valid_end -= BLK_SIZE - Start;
                    Start  = 0;
                }
                else {
                    Start -= BLK_SIZE;
                }

                if ( ! TrueGaplessPresent ) {
                    memset ( YY, 0, sizeof YY );
                }
                else {
                    if ( ReadAndVerifyFrame ( StreamVersion, 0, FrameNo, TotalFrames ) < 0 ) {
                        LEAVE(3);
                        return ret;
                    }
                    if ( Scale != 0. )
                        Requantize ( MS_Band, IS_used, Min_Band, Max_Band, StreamVersion );
                }
                Synthese_Filter ( Stream + 0, V_offset+0, VV [0], YY [0], Channels, 0 );
                Synthese_Filter ( Stream + 1, V_offset+1, VV [1], YY [1], Channels, 1 );
            }
            break;
        }
        // write PCM data to destination from the "very last" frame
        ret += Write_PCM ( OutputFile, Stream + Start * Channels, valid_end * Channels ) / Channels;
    }

    stderr_printf ("\r%s", PrintTime ( ret, (char)' ' ) );                              // time report

    LEAVE(3);
    return ret;
}


/**********************************************************************************************************************************
 *
 *  Functions: helper functions for DecodeFile()
 *
 *  JumpID3v2():
 *    Determines the size of a ID3v2 tag without footer and with Run Length Restriction.
 *    This si a very special kind of ID3v2 tags, but it looks like all ID3v2 tags in reality
 *    have this propertity. <JOKE>Generate some other ID3v2 files and check how clean current ID3v2 readers are</JOKE>.
 *    Function returns 0 on error and the total size of the tag otherwise.
 *    Function should only be called when a tag is available, because it read a unpredictable amount of data from the stream, so you must reset the bitstream decoder after a wrong call of this function.
 *  EncoderName():
 *    Returns a version string of the used encoder for encoding the stream (1.06 and above).
 *  ProfileName():
 *    Return a encoder profile name as string.
 */

static Int32_t
JumpID3v2 ( void )
{
    Int32_t   ret = 10;

    if ( (BitstreamLE_read (32) & 0xFFFFFF) != 0x334449L )
        return 0;

    if ( BitstreamLE_read (1) )
        return 0;
    ret += BitstreamLE_read (7) << 14;
    if ( BitstreamLE_read (1) )
        return 0;
    ret += BitstreamLE_read (7) << 21;
    BitstreamLE_read (1);
    if ( BitstreamLE_read (1) )
        ret += 10;
    BitstreamLE_read (14);
    BitstreamLE_read (16);
    if ( BitstreamLE_read (1) )
        return 0;
    ret += BitstreamLE_read (7) <<  0;
    if ( BitstreamLE_read (1) )
        return 0;
    ret += BitstreamLE_read (7) <<  7;

    return ret;
}


static const char*
EncoderName ( int encoderno )           // odd version numbers are alpha, even are beta, dividable by 10 are release versions
{
    static char  Name [32];

    if ( encoderno <= 0 )
        Name [0] = '\0';
    else if ( encoderno % 10 == 0 )
        sprintf ( Name, " (Release %u.%u)",     encoderno/100, encoderno/10%10 );
    else if ( (encoderno & 1) == 0 )
        sprintf ( Name, " (Beta %u.%02u)",      encoderno/100, encoderno%100 );
    else
        sprintf ( Name, " (--Alpha-- %u.%02u)", encoderno/100, encoderno%100 );

    return Name;
}


static const char*
ProfileName ( Uint profile )            // profile is 0...15, where 1, 5...15 is currently used
{
    static const char   na    [] = "n.a.";
    static const char*  Names [] = {
        na        , "Unstable/Experimental", na                 , na                 ,
        na        , "below 'Telephone'"    , "below 'Telephone'", "'Telephone'"      ,
        "'Thumb'" , "'Radio'"              , "'Standard'"       , "'Xtreme'"         ,
        "'Insane'", "'BrainDead'"          , "above 'BrainDead'", "above 'BrainDead'",
    };

    return profile >= sizeof(Names)/sizeof(*Names)  ?  na  :  Names [profile];
}

/**********************************************************************************************************************************
 *
 *  Function: DecodeFile()
 *
 *  This function do all the header analysing stuff, calls then Decode() and pretends at the end about the decoding speed.
 *
 *  This function has the following things to do:
 *   - Inits bitstream decoder + fills bitstream decoder buffer for the first time
 *   - Analyse first 32 bit of the bitsream to determine file type
 *   - Gives a lot of useful, but unnecessary messages when the input file seems to be a non musepack file and returns.
 *   - analyse the header of SV4, SV5, SV6, SV7.0, SV7.1 and current experimental SV8 and initializes some
 *     global variables.
 *   - Calls two tag analysing functions and if they report success, it prints most important tag information on stderr.
 *   - Tag analysing functions also return the file size, this is used to calculate and print the bitrate for VBR files.
 *   - Analysing of replaygain/clip prevention and printing of results.
 *     These results are used to call Init_QuantTab(), i.e. SCF scalers are adjusted for replaygain/clip prevention.
 *     So you don't need any CPU power to do replaygain/clip prevention.
 *   - Compute Start + Duration from seconds into samples, do the special percent handling for negative values.
 *   - Resets some arrays.
 *   - Call Decode()
 *   - Calculate Decoding speed and print result.
 *
 */

static UintMax_t
DecodeFile ( FILE_T  OutputFile,
             FILE_T  InputFile,
             Double  Start,
             Double  Duration )
{
    Int                 MaxBandDesired = 0;
    Uint32_t            TotalFrames    = 0;
    UintMax_t           DecodedSamples = 0;
    Uint                Profile        = (Uint)-1;
    Float               AverageBitrate;
    static TagInfo_t    taginfo;
    clock_t             T;
    Int32_t             ID3v2;
    Uint16_t            PeakTitle = 0;
    Uint16_t            PeakAlbum = 0;
    Uint16_t            Peak;
    Uint16_t            tmp;
    Int16_t             GainTitle = 0;
    Int16_t             GainAlbum = 0;
    Int16_t             Gain;
    int                 Encoder;
    Bool_t              SecurePeakTitle = 0;
    Float               ReplayGain;             // 0...1...+oo
    Float               ClipCorr;               // 0...1
    size_t              HeaderLen;

    ENTER(2);

    // Fill the bitstream buffer for the first time
resume:
    Bitstream_init ();
    InputBuffRead = READ ( InputFile, InputBuff, IBUFSIZE * 4 ) / 4;

    // Test the first 4 bytes ("MP+": SV7+, "ID3": ID3V2, other: may be SV4...6)
    switch ( BitstreamLE_preview (32) ) {
    case (Uint32_t)0x01334449L:                                         /* ID3 V2.1...2.4 */
    case (Uint32_t)0x02334449L:
    case (Uint32_t)0x03334449L:
    case (Uint32_t)0x04334449L:
        stderr_printf ("\n"PROG_NAME": Stream was corrupted by an ID3 Version 2 tagger\n\n" );
        ID3v2 = JumpID3v2 ();
        if ( SEEK ( InputFile, ID3v2, SEEK_SET ) < 0 ) {
            stderr_printf ( "\n\nSorry, recovering fails.\n\a" );
            return 0;
        }
        sleep (1);
        stderr_printf ("\b\b\b\b, ignore %lu words and %u bits ...\n\a", (unsigned long)ID3v2 >> 2, (int)(ID3v2&3) << 3 );
        goto resume;

    case (Uint32_t)0x072B504DL:                                         /* MP+ SV7 */
    case (Uint32_t)0x172B504DL:                                         /* MP+ SV7.1 */
        StreamVersion  = (Int) BitstreamLE_read (8);
        (void) BitstreamLE_read (24);
        break;

    case (Uint32_t)0xFF2B504DL:                                         /* MP+ SVF.F */
        (void) BitstreamBE_read (24);
        StreamVersion  = (Int) BitstreamBE_read (8);
        break;

    case (Uint32_t)0x2043414DL:                                         /* MAC  */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "Monkey's Audio" );
        return 0;

    case (Uint32_t)0x7961722E:                                         /* Real Audio */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "Real Audio" );
        return 0;

    case (Uint32_t)0x46464952L:                                         /* WAV  */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "Microsoft WAVE" );
        return 0;

    case (Uint32_t)0x43614C66L:                                         /* FLAC */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "FLAC" );
        return 0;

    case (Uint32_t)0x4341504CL:                                         /* LPAC */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "LPAC" );
        return 0;

    case (Uint32_t)0x37414B52L:                                         /* RKAU */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "RKAU" );
        return 0;

    case (Uint32_t)0x676B6A61L:                                         /* Shorten */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "Shorten" );
        return 0;

    case (Uint32_t)0x040A5A53L:                                         /* SZIP 1.12 */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "szip" );
        return 0;

    case (Uint32_t)0x5367674FL:                                         /* OggS */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "Ogg Stream" );
        return 0;

    case (Uint32_t)0x46494441L:                                         /* AAC-ADIF */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "AAC Audio Data Interchange Format" );
        return 0;

    case (Uint32_t)0x75B22630L:                                         /* MS WMA */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "MS Windows Media Audio" );
        return 0;

    case (Uint32_t)0xBA010000L:                                         /* MPEG system stream */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "MPEG system stream/VOB" );
        return 0;

    case (Uint32_t)0x0180FE7FL:                                         /* DTS */
        stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "Digital Theater System" );
        return 0;

    default:
        StreamVersion = (Uint32_t) BitstreamLE_preview (32);
        if ( ( StreamVersion & 0x00FFFFFF ) == (Uint32_t)0x002B504DL ) {
            StreamVersion >>= 24;
            stderr_printf ( "\n"PROG_NAME": Input File seems to be a MPC file StreamVersion %u.%u\nVisit http://www.uni-jena.de/~pfk/mpc/ and update your software.\n\n", StreamVersion & 15, StreamVersion >> 4 );
            return 0;
        }

        if ( ( (BitstreamLE_preview (32)) & 0x0000FFFF ) == (Uint32_t)0x0000770BL ) {
            stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "AC3/Dolby Digital" );
            return 0;
        }

        if ( ( (BitstreamLE_preview (32)) & 0x0000E6FF ) == (Uint32_t)0x0000E6FFL ) {
            stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "MPEG Layer 1" );
            return 0;
        }

        if ( ( (BitstreamLE_preview (32)) & 0x0000E6FF ) == (Uint32_t)0x0000E4FFL ) {
            stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "MPEG Layer 2" );
            return 0;
        }

        if ( ( (BitstreamLE_preview (32)) & 0x0000E6FF ) == (Uint32_t)0x0000E2FFL ) {
            stderr_printf ("\n"PROG_NAME": Input File is a %s file\n", "MPEG Layer 3" );
            return 0;
        }

        StreamVersion = (Int) (BitstreamLE_preview (21) & 0x3FF);
        if ( StreamVersion < 4  ||  StreamVersion > 6 ) {
            stderr_printf ("\n"PROG_NAME": Input File is not a MPC file, neither SV 4...6 nor SV 7 (SV %u)\n", StreamVersion );
            return 0;
        }
        break;
    }


    // decode the header for SV4...6 or SV7 or SV8
    switch ( StreamVersion ) {
    case 0x04:
    case 0x05:
    case 0x06:
        Bitrate        = (Int)  BitstreamLE_read ( 9);
        IS_used        = (Int)  BitstreamLE_read ( 1);
        MS_bits        = (Uint) BitstreamLE_read ( 1);
        StreamVersion  = (Int)  BitstreamLE_read (10);
        MaxBandDesired = (Int)  BitstreamLE_read ( 5);
        if ( BitstreamLE_read (6) != 1 ) {
            fprintf ( stderr, "Blocksize of != 1 is not supported any longer\n" );
            return 0;
        }
        TotalFrames    = BitstreamLE_read ( StreamVersion < 5  ?  16  :  32 );
        SampleFreq     = 44100;
        Encoder        = -1;
        Channels       =  2;

        if ( StreamVersion >= 4  &&  StreamVersion <= 6 )
            break;

    default:  // it should be impossible to execute the following code
        stderr_printf ("\n"PROG_NAME": Internal error\n" );
        stderr_printf ("\n"PROG_NAME": Not a MPC file, neither SV 4...6 nor SV 7 (SV %u.%u)\n", StreamVersion & 15, StreamVersion >> 4 );
        return 0;

    case 0x07:
    case 0x17:
        Bitrate        = 0;
        Channels       = 2;
        TotalFrames    =        BitstreamLE_read (32);
        IS_used        = (Int)  BitstreamLE_read ( 1);
        MS_bits        = (Uint) BitstreamLE_read ( 1);
        MaxBandDesired = (Int)  BitstreamLE_read ( 6);

        // reading the profile
        Profile = (Int) BitstreamLE_read (4);
        (void) BitstreamLE_read ( 2);
        SampleFreq = sftable [ BitstreamLE_read ( 2) ];

        // reading peak and gain values from the file (or use useful values if they are absent)
        PeakTitle = (Uint16_t)(1.18 * (Uint32_t)BitstreamLE_read (16));
        GainTitle = BitstreamLE_read (16);
        tmp       = BitstreamLE_read (16);
        if ( SecurePeakTitle = (tmp != 0) )
            PeakTitle = tmp;
        GainAlbum = BitstreamLE_read (16);
        PeakAlbum = BitstreamLE_read (16);
        if ( PeakAlbum == 0 )
            PeakAlbum = PeakTitle;

        // reading true gapless
        TrueGaplessPresent = BitstreamLE_read ( 1);
        LastValidSamples   = BitstreamLE_read (11);
        (void) BitstreamLE_read (20);

        // reserved bytes for future use
        Encoder = BitstreamLE_read ( 8);
        break;

    case 0xFF:
        Bitrate            = 0;
        TrueGaplessPresent = 1;

        HeaderLen        = (Uint16_t) BitstreamBE_read (16);
        Profile          = (Uint8_t)  BitstreamBE_read ( 8);
        Encoder          = (Uint8_t)  BitstreamBE_read ( 8);

        TotalFrames      = (Uint32_t) BitstreamBE_read (32);

        // reading peak and gain values from the file (or use useful values if they are absent)
        SecurePeakTitle  = 1;

        PeakTitle        = (Uint16_t) BitstreamBE_read (16);
        GainTitle        = (Int16_t)  BitstreamBE_read (16);

        PeakAlbum        = (Uint16_t) BitstreamBE_read (16);
        GainAlbum        = (Int16_t)  BitstreamBE_read (16);
        if ( PeakAlbum == 0 )
            PeakAlbum = PeakTitle;

        MaxBandDesired   = (Uint)     BitstreamBE_read ( 6);
        MS_bits          = (Uint)     BitstreamBE_read ( 4);
                           (void)     BitstreamBE_read ( 6);
        LastValidSamples = (Uint)     BitstreamBE_read (16);

        Channels         = (Uint8_t)  BitstreamBE_read ( 8);
                           (void)     BitstreamBE_read (24);

        SampleFreq       = (Uint16_t) BitstreamBE_read (16);
                           (void)     BitstreamBE_read (16);

        for ( ; HeaderLen > 26; HeaderLen-- )
                           (void)     BitstreamBE_read ( 8);
        break;
    }


    // check for ID3V1(.1) tags or APE tags, output information if available
    if ( Read_ID3V1_Tags ( InputFile, &taginfo )  ||  Read_APE_Tags ( InputFile, &taginfo ) ) {
#define GET(x)       ( taginfo.x  ?  taginfo.x  :  "" )
#define HAVE_GENRE   ( GET(Genre)[0] != '?'  &&  GET(Genre)[0] != '\0' )
#define HAVE_YEAR    ( GET(Year)[0] != '\0' )
        stderr_printf ("\n         %s: %s  ", GET(Artist), GET(Album) );
        stderr_printf ( HAVE_GENRE  ||  HAVE_YEAR  ?  "  (" :  "" );
        stderr_printf ( HAVE_GENRE  ?  "%0.0s"  : "%s", GET(Genre) );
        stderr_printf ( HAVE_GENRE  &&  HAVE_YEAR  ?  ", "  :  "" );
        stderr_printf ( "%s", GET(Year) );
        stderr_printf ( HAVE_GENRE  ||  HAVE_YEAR  ?  ")\n" : "\n" );
        stderr_printf ("    %s %s", GET(Track), GET(Title) );
        stderr_printf ( GET(Comment)[0] != '\0'  ?  "  (%s)\n"  :  "%s\n", GET(Comment) );
#undef GET
#undef HAVE_GENRE
#undef HAVE_YEAR
    }

    // calculate bitrate for informational purpose
    if ( Bitrate == 0 ) {
        AverageBitrate = ( SampleFreq / 1000. / BLK_SIZE * 8 ) * taginfo.FileSize / TotalFrames  ;
    }
    else {
        AverageBitrate = Bitrate;
    }
    stderr_printf ("\n");
    if ( AverageBitrate > 0. )
        stderr_printf ("%7.1f kbps,", AverageBitrate );

    // Output total time, Streamversion, Profile
    stderr_printf ("%s, SV %u.%u, Profile %s%s", PrintTime ((UintMax_t)TotalFrames * BLK_SIZE, ' ') + 1, StreamVersion & 15, StreamVersion >> 4, ProfileName(Profile), EncoderName(Encoder) );

    // Choose the select type of Peak and Gain values
    switch ( ReplayGainType ) {
    case  0: // no replay gain, use title peak for clipping prevention
        Gain = 0;
        Peak = PeakTitle;
        break;
    case  1: // no replay gain, use album peak for clipping prevention
        Gain = 0;
        Peak = PeakAlbum;
        break;
    default: // title replay gain
        Gain = GainTitle;
        Peak = PeakTitle;
        break;
    case  3: // album replay gain
        Gain = GainAlbum;
        Peak = PeakAlbum;
        break;
    }

    // calculate the multiplier from the original integer peak and gain data
    ReplayGain = (Float) exp ( (M_LN10/2000.) * (Int16_t)Gain );
    ClipCorr   = (Float) (32767. / ( (Uint32_t)Peak + 1 ));             // avoid divide by 0

    // Perform or not perform clipping prevention, that is here the question
    if ( ClipPrev ) {
        stderr_printf (", ClipDamp " );
        if        ( Peak == 0 ) {
            stderr_printf ("1 ???" );
            ClipCorr = 1.f;
        }
        else if ( ReplayGain * fabs(Scale) > ClipCorr ) {
            stderr_printf (".%04d%s", (int)(1.e4 * ClipCorr / (ReplayGain * Scale) + 0.5), SecurePeakTitle  ?  ""  :  "?" );
            ClipCorr = ClipCorr / (ReplayGain * Scale);
        }
        else {
            stderr_printf ("1%s", SecurePeakTitle  ?  ""  :  "?" );
            ClipCorr = 1.f;
        }
    }
    else {
        ClipCorr = 1.f;
    }

    // report replay gain if != 1.
    if ( ReplayGain != 1. )
        stderr_printf (", Gain %.4f", ReplayGain );
    stderr_printf ("\n\n");

    // init subband structure (MaxBandDesired, StreamVersion, bitrate) and Scale factors
    Init_QuantTab ( MaxBandDesired, IS_used, ClipCorr * ReplayGain * Scale, StreamVersion );

    // calculate Start and Duration if they are given in precent as negative values
    if ( Start    < 0. )
        Start    *= TotalFrames * -(0.01 * BLK_SIZE / SampleFreq);
    if ( Duration < 0. )
        Duration *= TotalFrames * -(0.01 * BLK_SIZE / SampleFreq);

    // reset arrays to avoid HF noise if recent MaxBand > current MaxBand
    memset ( YY      , 0, sizeof YY       );
    memset ( QQ      , 0, sizeof QQ       );
    memset ( VV      , 0, sizeof VV       );
    memset ( V_offset, 0, sizeof V_offset );

    // decoding kernel with time measurement
    T = clock ();
    DecodedSamples  = Decode ( OutputFile, InputFile, TotalFrames,
                               floor (Start * SampleFreq + 0.5) + DECODER_DELAY,
                               Duration <= 0.  ?  1.e18  :  floor (Duration * SampleFreq + 0.5),
                               LastValidSamples );
    T = clock () - T;
#ifdef __TURBOC__       // wraps around at midnight
    if ( (Long)T < 0 ) {
        T += (time_t) (86400. * CLOCKS_PER_SEC);
    }
#endif

    // output at the end of a decoded title
    (void) stderr_printf ( " (runtime: %.2f s  speed: %.2fx)\n", (Double) (T * (1. / CLOCKS_PER_SEC )),
                           T  ?  (Double) ((CLOCKS_PER_SEC/(Float)SampleFreq) * UintMAX_FP(DecodedSamples) / T)  :  (Double)0. );

    LEAVE(2);

    return DecodedSamples;
}


/**********************************************************************************************************************************
 *
 *  Function: Create_Extention()
 *
 *  Creates a output file name by a given destination path, the input file name and a given extention
 *
 *  - Path is taken
 *  - a directory separator is appended
 *  - the file name of the source file is taken
 *  - and the extention is added.
 *
 *  Decoding the source file bar.mpc with the path foo will create the destination file
 *  foo/bar.mpc.wav . The double extention mpc.wav is intended, on my computer such "lossy" WAV files has
 *  always such extentions.
 *
 *    foo.wav      Original
 *    foo.mpc      Encoded file, normal behave can be to overwrite existing output files, you still have the original file.
 *    foo.mpc.wav  1st generation decoded file, no danger to overwite foo.wav and you see it is a "lossy" WAV file.
 *    foo.mpc.mpc  2nd generation encoded file
 *    ...
 */

static char*
Create_Extention ( const char*  Path,
                   const char*  Name,
                   const char*  Extention )
{
    static char  ret [PATHLEN_MAX + 3];
    char*        p = strrchr ( Name, PATH_SEP );

    if ( p != NULL )
        Name = p + 1;

    if ( strlen(Path) + strlen(Name) + strlen(Extention) > sizeof(ret) - 3 ) {
        stderr_printf (PROG_NAME":\tTarget buffer for new file name too short,\n\tincrease PATHLEN_MAX and recompile.\n\a" );
        exit (4);
    }

    sprintf ( ret, "%s%c%s.%s", Path, PATH_SEP, Name, Extention );
    return ret;
}


/**********************************************************************************************************************************
 *
 *  Function: Unintended_OutputFile()
 *
 *  Checks the sense of using a file name as an output file.
 *
 *   - Identifier starting with http:// or ftp:// are very likely not a useful output file, because the program do not support writing via http or ftp
 *   - When file can be opened, there's at least no danger to use the file as output file, because it is (besides from some races) not possible to destroy data.
 *   - When the first 3 bytes of the file are "MP+", it is very likely that this is not an intended output file
 *   - When the file ends with a path separator, it also can't be an input file.
 *   - When the file ends with one of the below mentioned extentions, it is also not a candidate for an file with PCM data.
 *
 *  This is pure heuristic, but saves you from damaging files. And I don't found any case, where this automatic
 *  made nonsense.
 *
 */

static Int
Unintended_OutputFile ( const char* Name )
{
    static  char* BadExt [] = { "MPC", "MPP", "MP+", "M3U", "PAC", "APE", "OFR", "RKA", "AC3", "MP4", "MPT", "MP3", "MP2", "MP1", "MP3PRO", "OGG", "AAC", "LQT", "SHN", "FLA", "FLAC", "MOD", "LA", "VOB", "MPEG", "MPG", "AVI", "AU", "MOD", "MIDI", "MID", "VQF", "WMA", "WMV" };
    char    buff [3];
    FILE_T  fp;
    int     i;

#ifdef USE_HTTP
    if ( 0 == strncasecmp (Name, "http://", 7)  ||  0 == strncasecmp (Name, "ftp://", 6) )      // HTTP/FTP: not possible
        return 1;
#endif

    if ( (fp = OPEN ( Name )) == INVALID_FILEDESC )                                             // file not exist: no danger
        return 0;

    READ ( fp, buff, 3 );
    CLOSE (fp);
    if ( memcmp (buff, "MP+", 3) == 0 )
        return 1;                                                                               // Destination is very likely a MPC file (SV7+)
    if ( strlen (Name) < 1 )
        return 0;

    if ( Name[strlen(Name)-1] == PATH_SEP )                                                     // Ends with a path separator
        return 1;

    Name = strrchr ( Name, '.' );
    if ( Name == NULL )
        return 0;                                                                               // No dot in the path, no extention

    for ( i = 0; i < sizeof(BadExt)/sizeof(*BadExt); i++ )                                      // Known extentions
        if ( 0 == strcasecmp ( Name+1, BadExt [i] ) )
            return 1;

    return 0;
}


/**********************************************************************************************************************************
 *
 *  Function: RandomizeList()
 *            SortList()
 *
 *  Randomizes/Sorts rests of the argument line pointed by argv.
 *
 *  Bug: also options are randomized/sorted.
 *
 */

static void
RandomizeList ( const char** argv )
{
    int          argc;
    int          i;
    int          j;
    const char*  tmp;

    for ( argc = 0; argv[argc] != NULL; argc++ )
        ;
    srand ( time (NULL) );
    for ( i = 0; i < argc; i++ ) {
        j       = rand() % argc;
        tmp     = argv[i];
        argv[i] = argv[j];
        argv[j] = tmp;
    }
}


static int Cdecl
cmpfn ( const void* p1, const void* p2 )
{
    return strcmp ( *((const char**)p1), *((const char**)p2) );
}


static void
SortList ( const char** argv )
{
    int  argc;

    for ( argc = 0; argv[argc] != NULL; argc++ )
        ;

    qsort ( (void*)argv, argc, sizeof(*argv), cmpfn );
}


/**********************************************************************************************************************************
 *
 *  Function: decode_html()
 *
 *  Writes the file name of a file to the file handle and do some convertions which I use for naming my audio files.
 *
 *  This function has the following things to do:
 *   - Writes normal characters as normal characters
 *   - convert '_' to a space
 *   - convert %XX to the ISO 8859-1 charcter with the hex value XX. Only capital letetrs are allowed.
 *   - For unicode support it is also planed to support %00XXXX, %0000XXXXXX and %000000XXXXXXXX for ISO-10646 code above 255.
 *
 *  Do_you_understand_this%3F
 */

static int
hexdigit ( const char s )
{
    if ( (unsigned char)(s-'0') < 10u ) return s-'0';
    if ( (unsigned char)(s-'A') <  6u ) return s-'A'+10;
    return -1;
}


static void
decode_html ( FILE_T       fp,
              const char*  src )
{
    char  ch;

    if ( GetStderrSilent() )
        return;

    for ( ; src[0] != '\0' ; src++) {
        if      ( src[0] == '_' )
            WRITE (fp, " ", 1 );
        else if ( src[0] != '%'  ||  hexdigit(src[1]) < 0  ||  hexdigit(src[2]) < 0 ) {
            WRITE ( fp, src, 1);
        }
        else {
            ch = hexdigit(src[1]) * 16 + hexdigit(src[2]), src += 2;
            WRITE ( fp, &ch, 1 );
        }
    }
}


/**********************************************************************************************************************************
 *
 *  Function: Analyze_fs()
 *
 *  The ugly peek functions.
 *
 *  This function has the following things to do:
 *   - Open a given source file
 *   - when this is a MPC file, it has to guess the sampling frequency and has to set the global variable SampleFreq
 *   - In SV7(new) 4 possible sampling frequencies are stored in 2 bits
 *   - In SV8(pre) 2 bytes are used to store 32*width of subband as 16 bit integer.
 *     When a 32-PQF is used, this is the sampling frequency, when a 64-PQF is used (96 kHz support, low bitrate support) this is
 *     half of the sampling frequency.
 *
 *  Should become superfluous with anew IO lib.
 */

static void
Analyze_fs ( const char* filename )
{
    FILE_T         f = OPEN (filename);
    unsigned char  buff [32];
    int            bytes;

    SampleFreq = 44100;

    if ( f == INVALID_FILEDESC )
        return;

    bytes = READ ( f, buff, sizeof buff );
    CLOSE (f);

    if ( sizeof buff != bytes )
        return;

    if ( buff[0] != 'M' || buff[1] != 'P' || buff[2] != '+' )
        return;

    if ( buff[3] & 0x80 )
        SampleFreq = buff[28] * 256 + buff[29];
    else
        SampleFreq = sftable [ buff[10] & 3 ];
}


/**********************************************************************************************************************************
 *
 *  Function: mainloop()
 *
 *  Remark: Please don't tell me that you understand this function!
 *
 *  This function has the following things to do:
 *   - Opening and closing source and destination files/devices
 *   - Write (and adjust at the end of decoding) PCM file headers
 *   - Avoid that you write PCM files in a file, which never can be a PCM files (fool proof checks: mppdec f1.mpc f2.mpc plays both files instead of writing decoded f1.mpc to f2.mpc)
 *   - Avoid that decoded files are written to consoles
 *   - Count written PCM samples to do this right
 *   - Write messages, what the program currently do
 *   - option parsing, file name parsing
 *   - check for validy of overwriting destination files
 *   - Title bar handling
 *   - mainloop() calls DecodeFile(OutputFile,InputFile,Start,Duration) with two open file descriptors
 *     and a start and end time. It returns the number of written PCM samples per Channel.
 *     Start    time can be >=0, then this is the start    in seconds, otherwise it is the negative percentage of the whole file (mid = -50)
 *     Duration time can be  >0, then this is the duration in seconds, otherwise duration time is +oo.
 *
 *  Beside from the size of this function there's a big bug in it.
 *  Output device is opened before the source files are opened. After introducing fs != 44100 Hz, there
 *  was a peek function necessary to determine fs of the first file. This is really ugly and do also
 *  not work on streams/devices/...
 *
 *  Also the lot of #ifdef's make this function unreadable. An IO class (in C) has be started to overcome
 *  this stuff, but it is not finished. I sugegst first to move this program to the IO lib before starting any
 *  other general changes and modifications.
 *
 */

enum NameMode_t  {                      // possible output modes
    automode,
    nullmode,
    filemode, filemode_noinit,
    pipemode, pipemode_noinit,
    dspmode , dspmode_noinit ,
    esdmode , esdmode_noinit ,
    sunmode , sunmode_noinit ,
    winmode , winmode_noinit ,
    irixmode, irixmode_noinit,
};


static int
mainloop ( int     argc,
           char**  argv )
{
    enum NameMode_t  OutputMode;
    FILE_T           InputFile;
    FILE_T           OutputFile;
    const char*      OutputName;
    const char*      OutputDir;
    const char*      OutputComment  = "";
    Double           Start          = 0.;   // Startzeit, wenn > 0
    Double           Duration       = 0.;   // zu dekodierende Länge, wenn > 0
    UintMax_t        DecodedSamples = 0;
#ifdef USE_ESD_AUDIO
    int              ESDFileHandle;
#endif
    HeaderWriter_t   HeaderWriter  = Write_WAVE_Header;
    const char*      OutputFileExt = "wav";
    const char*      arg;

    // Um die Ausgabedatei kümmern (hier noch mal alles durchdenken)
    OutputName = argv [argc-1];
    if      ( 0 == strcmp (OutputName, "-")  ||  0 == strcmp (OutputName, "/dev/stdout") ) {
        if ( argc > 2  ||  ISATTY (FILENO(STDIN)) )
            argv [argc-1]  = NULL;
  pipe: if ( ISATTY (FILENO (STDOUT)) ) {
            argc++;
#if   defined USE_OSS_AUDIO
            OutputName = "/dev/audio";
            goto ossjump;
#elif defined USE_ESD_AUDIO
            goto esdjump;
#elif defined USE_SUN_AUDIO
            OutputName = "/dev/audio";
            goto sunjump;
#elif defined USE_IRIX_AUDIO
            OutputName = "/dev/audio";
            goto irixjump;
#elif defined USE_WIN_AUDIO
            OutputName = "/dev/audio";
            goto winjump;
#endif
        }
        OutputMode = pipemode_noinit;
        OutputName = "/dev/stdout";
    }
    else if ( 0 == strcmp (OutputName, "/dev/null") ) {
        OutputMode = nullmode;
        OutputFile = NULL_FD;
        OutputComment = " (Null Device)";
        argv [argc-1]  = NULL;
    }
#ifdef USE_SUN_AUDIO
    else if ( 0 == strcmp (OutputName, "/dev/audio") ) { sunjump:
        OutputMode       = sunmode_noinit;
        argv [argc-1]  = NULL;
    }
#endif /* USE_SUN_AUDIO */
#ifdef USE_IRIX_AUDIO
    else if ( 0 == strcmp (OutputName, "/dev/audio") ) { irixjump:
        OutputMode       = irixmode_noinit;
        argv [argc-1]  = NULL;
    }
#endif /* USE_IRIX_AUDIO */
#ifdef USE_ESD_AUDIO
    else if ( 0 == strcmp (OutputName, "/dev/esd") ) { esdjump:
        OutputMode = esdmode_noinit;
        argv [argc-1]  = NULL;
    }
#endif /* USE_ESD_AUDIO */
#ifdef USE_OSS_AUDIO
    else if ( 0 == strncmp (OutputName, "/dev/", 5) ) { ossjump:
        OutputMode = dspmode_noinit;
        argv [argc-1]  = NULL;
    }
#endif /* USE_OSS_AUDIO */
#ifdef USE_WIN_AUDIO
    else if ( 0 == strcmp (OutputName, "/dev/audio") ) { winjump:
        OutputMode = winmode_noinit;
        argv [argc-1]  = NULL;
    }
#endif /* USE_WIN_AUDIO */
    else if ( Unintended_OutputFile (OutputName) ) {
        goto pipe;
    }
    else {
        OutputMode = filemode_noinit;
        if ( (OutputFile = CREATE ( OutputName )) != INVALID_FILEDESC ) {
            argv [argc-1]  = NULL;
        }
        else if ( IsDirectory ( OutputName ) ) {
            OutputMode = automode;
            OutputFile = INVALID_FILEDESC;
            OutputDir  = OutputName;
            argv [argc-1]  = NULL;
        }
        else {
            stderr_printf ("\n"PROG_NAME": Can't create output file '%s': %s\n", OutputName, strerror(errno) );
            return 3;
        }
    }


    while ( *++argv != NULL ) {

        // Optionen dekodieren
        if ( argv[0][0] == '-'  &&  argv[0][1] == '-' ) {
            arg = argv[0] + 2;

            if ( 0 == strncmp (arg, "random", 3) ) {
                RandomizeList ( argv + 1 );
                continue;
            }
            else if ( 0 == strncmp (arg, "sort", 2) ) {
                SortList ( argv + 1 );
                continue;
            }
            else if ( 0 == strncmp (arg, "start", 2)  ||  0 == strncmp (arg, "skip", 2) ) {     // Start
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "start" );
                    return 1;
                }
                else {
                    if ( 0 == strncmp (*argv, "mid", 1) )
                        Start = -50.f;                                                          // 50%, negative Zahlen sind Prozente
                    else
                        Start = atof (*argv);
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "duration", 3) ) {  // Duration
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "duration" );
                    return 1;
                }
                else {
                    Duration = atof (*argv);
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "scale", 2) ) {     // Level scaling
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "scale" );
                    return 1;
                }
                else {
                    Scale = (Float) atof (*argv);
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "noprev", 3)  ||  0 == strncmp (arg, "noclip", 3) ) {    // Clipping prevention disabled
                ClipPrev = 0;
                continue;
            }
            else if ( 0 == strncmp (arg, "prev"  , 3)  ||  0 == strncmp (arg, "clip"  , 3) ) {    // Clipping prevention enabled
                ClipPrev = 1;
                continue;
            }
            else if ( 0 == strncmp (arg, "priority", 3) ) {      // Disable replay gain
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "gain" );
                    return 1;
                }
                else {
                    SetPriority ( atoi (*argv) );
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "gain", 1) ) {      // Disable replay gain
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "gain" );
                    return 1;
                }
                else {
                    ReplayGainType = atoi (*argv);
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "dumpselect", 10) ) {      // Dump select
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "dumpselect" );
                    return 1;
                }
                else {
                    DUMPSELECT = atoi (*argv);
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "silent", 2)  ||  0 == strncmp (arg, "quiet", 1) ) {    // Disable display
                SetStderrSilent (1);
                continue;
            }
#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT
            else if ( 0 == strncmp (arg, "bits", 1) ) {      // Output bits
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "bits" );
                    return 1;
                }
                else {
                    Init_Dither ( Bits = atoi (*argv), NoiseShapeType, Dither );
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "dither", 2) ) {    // Output bits
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "dither" );
                    return 1;
                }
                else {
                    Init_Dither ( Bits, NoiseShapeType, Dither = (Float)atof (*argv) );
                    continue;
                }
            }
            else if ( 0 == strncmp (arg, "shape", 2) ) {     // Noise shaping type
                if ( *++argv == NULL ) {
                    stderr_printf ("\n"PROG_NAME": --%s x?\n\n", "shape" );
                    return 1;
                }
                else {
                    Init_Dither ( Bits, NoiseShapeType = atoi(*argv), Dither );
                    continue;
                }
            }
#endif
            else if ( 0 == strncmp (arg, "wav", 1) ) {       // Microsoft's WAVE
                HeaderWriter     = Write_WAVE_Header;
                output_endianess = LITTLE_ENDIAN;
                OutputFileExt    = "wav";
                continue;
            }
            else if ( 0 == strncmp (arg, "aiff", 1) ) {      // Apple's AIFF
                HeaderWriter     = Write_AIFF_Header;
                output_endianess = BIG_ENDIAN;
                OutputFileExt    = "aiff";
                continue;
            }
            else if ( 0 == strncmp (arg, "raw-le", 5) ) {    // Raw PCM, little endian
                HeaderWriter     = Write_Raw_Header;
                output_endianess = LITTLE_ENDIAN;
                OutputFileExt    = "pcm.le";
                continue;
            }
            else if ( 0 == strncmp (arg, "raw-be", 5) ) {    // Raw PCM big endian
                HeaderWriter     = Write_Raw_Header;
                output_endianess = BIG_ENDIAN;
                OutputFileExt    = "pcm.be";
                continue;
            }
            else if ( 0 == strcmp (arg, "raw") ) {           // Raw PCM native endian
                HeaderWriter     = Write_Raw_Header;
                output_endianess = ENDIAN == HAVE_LITTLE_ENDIAN  ?  LITTLE_ENDIAN  :  BIG_ENDIAN;
                OutputFileExt    = "pcm";
                continue;
            }
        }

        // Input-Datei aufmachen
        if ( 0 == strcmp (*argv, "-")  ||  0 == strcmp (*argv, "/dev/stdin") ) {
            InputFile = SETBINARY_IN (STDIN);
            if ( ISATTY (FILENO(InputFile)) ) {
                stderr_printf ("\n"PROG_NAME": Can't decode data from a terminal\n" );
                return 2;
            }
            TitleBar ("<stdin>");
            Analyze_fs ("");
            stderr_printf ("\ndecoding of <stdin>\n");
        }
        else {
            if ( (InputFile = OPEN (*argv)) == INVALID_FILEDESC ) {
#if defined USE_HTTP
                if ( (InputFile = FDOPEN ( http_open (*argv), "rb" )) == INVALID_FILEDESC ) {
#endif

                    stderr_printf ("\n"PROG_NAME": Can't open input file '%s': %s\n", *argv, strerror(errno) );
                    return 1;
#if defined USE_HTTP
                }
#endif
            }
            stderr_printf ("\ndecoding of file '");
            decode_html ( STDERR, *argv);
            TitleBar (*argv);
            Analyze_fs (*argv);
            stderr_printf ( "'" );
            if      ( SampleFreq != 44100  &&  Channels != 2 )
                stderr_printf ( " (%g kHz, %d Channels)", 1.e-3*SampleFreq, Channels );
            else if ( SampleFreq != 44100 )
                stderr_printf ( " (%g kHz)", 1.e-3*SampleFreq );
            else if ( Channels != 2 )
                stderr_printf ( " (%d Channels)", Channels );
            stderr_printf ("\n");
        }

        // Output-Datei aufmachen oder weiterverwenden
        switch ( OutputMode ) {
        case automode:
            OutputName = Create_Extention ( OutputDir, *argv, OutputFileExt );
            if ( (OutputFile = CREATE ( OutputName )) == INVALID_FILEDESC ) {
                stderr_printf ("\n"PROG_NAME": Can't create output file '%s': %s\n", OutputName, strerror(errno) );
                return 3;
            }
            DecodedSamples = 0;
            goto filemode2;
        case filemode_noinit:
            OutputMode = filemode;
        filemode2:
            HeaderWriter ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels, 1.e18 );
        case filemode:
            stderr_printf ("         to file '%s'" SAMPLE_SIZE_STRING "\n", OutputName );
            break;
        case pipemode_noinit:
            OutputMode = pipemode;
            OutputFile = SETBINARY_OUT (STDOUT);

#if defined USE_OSS_AUDIO
            if ( 0 == Set_DSP_OSS_Params ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels ) )
                OutputComment = " (Open Sound System)";
            else
#elif defined USE_SUN_AUDIO
            if ( 0 == Set_DSP_Sun_Params ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels ) )
                OutputComment = " (Sun Onboard-Audio)";
            else
#endif

            HeaderWriter ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels, 1.e18 );
        case pipemode:
            stderr_printf ("         to <stdout>%s" SAMPLE_SIZE_STRING "\n", OutputComment );
            break;
        case sunmode_noinit:
#ifdef USE_SUN_AUDIO
            output_endianess = BIG_ENDIAN;
            if ( (OutputFile = CREATE ( OutputName )) == INVALID_FILEDESC ) {
                stderr_printf ("\n"PROG_NAME": Can't access device %s: %s\n", OutputName, strerror(errno) );
                return 3;
            }
            if ( 0 == Set_DSP_Sun_Params ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels ) )
                OutputComment = " (Sun Onboard-Audio)";
#endif
            OutputMode = sunmode;
            goto init_done;
        case dspmode_noinit:
#ifdef USE_OSS_AUDIO
            output_endianess = ENDIAN == HAVE_LITTLE_ENDIAN  ?  LITTLE_ENDIAN  :  BIG_ENDIAN;
            if ( (OutputFile = CREATE ( OutputName )) == INVALID_FILEDESC ) {
                stderr_printf ("\n"PROG_NAME": Can't access device %s: %s\n", OutputName, strerror(errno) );
                return 3;
            }
            if ( 0 == Set_DSP_OSS_Params ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels ) )
                OutputComment = " (Open Sound System)";
            OutputMode = dspmode;
#endif
            goto init_done;
        case esdmode_noinit:
#ifdef USE_ESD_AUDIO
            output_endianess = ENDIAN == HAVE_LITTLE_ENDIAN  ?  LITTLE_ENDIAN  :  BIG_ENDIAN;
            OutputComment = (ESDFileHandle = Set_ESD_Params ( INVALID_FILEDESC, SampleFreq, SAMPLE_SIZE, Channels )) < 0  ?  ""  :  " (Enlightenment Sound Daemon)";
            if ( (OutputFile = FDOPEN ( ESDFileHandle, "wb" )) == INVALID_FILEDESC ) {
                stderr_printf ("\n"PROG_NAME": Can't access %s: %s\n", OutputName, strerror(errno) );
                return 3;
            }
            OutputMode = esdmode;
#endif
            goto init_done;
        case winmode_noinit:
#ifdef USE_WIN_AUDIO
            output_endianess = ENDIAN == HAVE_LITTLE_ENDIAN  ?  LITTLE_ENDIAN  :  BIG_ENDIAN;
            if ( Set_WIN_Params ( INVALID_FILEDESC, SampleFreq, SAMPLE_SIZE, Channels ) < 0 ) {
                stderr_printf ("\n"PROG_NAME": Can't access %s: %s\n", "WAVE OUT", strerror(errno) );
                return 3;
            }
            OutputFile = WINAUDIO_FD;
            OutputComment = " (Windows WAVEOUT Audio)";
            OutputMode = winmode;
#endif
            goto init_done;
        case irixmode_noinit:
#ifdef USE_IRIX_AUDIO
            output_endianess = BIG_ENDIAN;
            if ( Set_IRIX_Params ( INVALID_FILEDESC, SampleFreq, SAMPLE_SIZE, Channels ) < 0 ) {
                stderr_printf ("\n"PROG_NAME": Can't access %s: %s\n", "SGI Irix Audio", strerror(errno) );
                return 3;
            }
            OutputFile = IRIXAUDIO_FD;
            OutputComment = " (SGI IRIX Audio)";
            OutputMode = irixmode;
#endif
            goto init_done;

        case sunmode:
        case dspmode:
        case esdmode:
        case winmode:
        case irixmode:
        case nullmode:
init_done:  stderr_printf ("         to device %s%s" SAMPLE_SIZE_STRING "\n", OutputName, OutputComment );
            break;
        }

        // Dekodieren
        DecodedSamples += DecodeFile ( OutputFile, InputFile, Start, Duration );

        // Output-Datei zumachen, falls notwendig (inner loop)
        switch ( OutputMode ) {
        case automode:
            if ( SEEK (OutputFile, 0L, SEEK_SET) != -1 )
                if ( FEOF ( OutputFile ) == 0 )
                    HeaderWriter ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels, DecodedSamples );
            CLOSE ( OutputFile );
            break;
        case filemode:
        case pipemode:
        case pipemode_noinit:
        case filemode_noinit:
        case dspmode:
        case esdmode:
        case sunmode:
        case winmode:
        case nullmode:
        case irixmode:
            break;
        }

    }

    // Output-Datei zumachen, falls notwendig (final)
    switch ( OutputMode ) {
    case pipemode:
    case filemode:
        if ( SEEK (OutputFile, 0L, SEEK_SET) != -1 ) {
            if ( FEOF ( OutputFile ) == 0 )
                HeaderWriter ( OutputFile, SampleFreq, SAMPLE_SIZE, Channels, DecodedSamples );
        }
        // fall through
    case pipemode_noinit:
    case filemode_noinit:
    case dspmode:
    case esdmode:
        CLOSE ( OutputFile );
        break;
    case sunmode:
    case automode:
    case nullmode:
        break;
    case winmode:
#ifdef USE_WIN_AUDIO
        TitleBar ("Flushing and Closing audio device");
        WIN_Audio_close ();
#endif
        break;
    case irixmode:
#ifdef USE_IRIX_AUDIO
        IRIX_Audio_close ();
#endif
        break;
    }

    TitleBar ("");
    return 0;
}


/**********************************************************************************************************************************
 *
 *  Function: usage()
 *
 *  Used by main(). This function do return. No implicit exit()!
 *  (Such implicit exits are evil!)
 *
 */

static void
usage ( void )
{
    stderr_printf (
        "\n"
        "\x1B[1m\rusage:\n"
        "\x1B[0m\r  "PROG_NAME" [--options] <Input_File> <Output_File>\n"
        "  "PROG_NAME" [--options] <List_of_Input_Files> <Output_File>\n"
        "  "PROG_NAME" [--options] <List_of_Input_Files> <Output_Directory>\n"
        "\n"
        "\x1B[1m\roptions:\n"
        "\x1B[0m\r  --start x   start decoding at x sec (x>0) or at |x|%% of the file (x<0)\n"
        "  --dur x     decode a sequence of x sec duration (dflt: 100%%)\n"
        "  --prev      activate clipping prevention (gain=0,2:title based; 1,3:album based)\n"
        "  --noprev    deactivate clipping prevention (dflt)\n"
        "  --scale x   additional scale signal by x (dflt: 1)\n"
        "  --gain x    replay gain control (0,1:off (dflt), 2:title, 3:album)\n"
        "  --silent    no messages to the terminal\n"
        "  --priority x set priority to x (0: idle, 100: realtime)\n"
        "  --wav       write Microsoft's WAVE file (dflt)\n"
        "  --aiff      write Apple's AIFF file\n"
        "  --raw       write RAW file in native byte order\n"
        "  --raw-le    write RAW file in little endian byte order\n"
        "  --raw-be    write RAW file in big endian byte order\n"
        "  --random    random play order (don't use options after this one)\n"
        "  --sort      sort play order (don't use options after this one)\n"
#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT
        "  --bits x    output with x bits (dflt: " STR(SAMPLE_SIZE) ")\n"
        "  --dither x  dithering factor (dflt: auto, useful 0.00...1.00)\n"
        "  --shape x   set shaping type (0:off (dflt), 1:light, 2:medium, 3:heavy)\n"
#endif
        "\n"
        "\x1B[1m\rspecial files:\n"
        "\x1B[0m\r  -           standard input or standard output\n"
        "  /dev/null   device null, the trash can\n"
#ifdef USE_OSS_AUDIO
        "  /dev/dsp*   use Open Sound System (OSS)" SAMPLE_SIZE_STRING "\n"
#endif
#ifdef USE_ESD_AUDIO
        "  /dev/esd    use Enlightenment Sound Daemon (EsounD)" SAMPLE_SIZE_STRING "\n"
#endif
#ifdef USE_SUN_AUDIO
        "  /dev/audio  use Sun Onboard-Audio" SAMPLE_SIZE_STRING "\n"
#endif
#ifdef USE_IRIX_AUDIO
        "  /dev/audio  use SGI IRIX Onboard-Audio" SAMPLE_SIZE_STRING "\n"
#endif
#ifdef USE_WIN_AUDIO
        "  /dev/audio  use Windows WAVEOUT Audio" SAMPLE_SIZE_STRING "\n"
#endif
#ifdef USE_HTTP
        "  protocol://[username[:password]@]servername[:port]/directories/file\n"
        "              file addressed via URL; username, password and port are optional,\n"
        "              protocol can be ftp, http, rtp. servername can be DNS, IP4 or IP6\n"
        // other interesting protocols are file:// und https://
#endif
        "\n"
        "\x1B[1m\rexamples:\n"
        "\x1B[0m\r  "PROG_NAME" Overtune.mpc Overtune.wav\n"
#if PATH_SEP == '/'
        "  "PROG_NAME" \"/Archive/Rossini/Wilhelm Tell -- [01] Overtune.mpc\" Overtune.wav\n"
        "  "PROG_NAME" \"/Archive/Rossini/*.mpc\" - | wavplay -\n"

        "  "PROG_NAME" --start -50%% --duration -5%% *.mpc - | wavplay -\n"
        "  "PROG_NAME" --prev *.mp+ .  &&  cdrecord -v -dao dev=sony -audio *.wav\n"
#else
        "  cd \\Archive\\Rossini; "PROG_NAME" \"Wilhelm Tell -- [01] Overtune.mpc\" Overtune.wav\n"
        "  cd \\Archive\\Rossini; "PROG_NAME" *.mpc - | wavplay -\n"

        "  "PROG_NAME" --start -50%% --duration -5%% *.mpc - | wavplay -\n"
        "  "PROG_NAME" --prev *.mp+ . ; cdrecord -v -dao dev=sony -audio *.wav\n"
#endif
#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT
        "  "PROG_NAME" --bits " STR(SAMPLE_SIZE) " --shape 2 *.mpc .\n"
#endif
#ifdef USE_OSS_AUDIO
        "  "PROG_NAME" */*.mpc /dev/dsp ; "PROG_NAME" */*.mpc /dev/dsp1\n"
#endif
#ifdef USE_SUN_AUDIO
        "  "PROG_NAME" */*.mpc /dev/audio\n"
#endif
#ifdef USE_IRIX_AUDIO
        "  "PROG_NAME" */*.mpc /dev/audio\n"
#endif
#ifdef USE_WIN_AUDIO
        "  "PROG_NAME" *.mpc /dev/audio\n"
#endif
#ifdef USE_HTTP
        "  "PROG_NAME" http://www.uni-jena.de/~pfk/mpp/audio/Maire_10bit_48kHz_Dithered.mpc\n"
#endif
#ifdef USE_ARGV
# if PATH_SEP == '/'
        "  "PROG_NAME" --gain 2 --prev --random /Archive/Audio/  /dev/audio\n"
# else
        "  "PROG_NAME" --gain 2 --prev --random C:\\AUDIO\\ D:\\AUDIO\\  /dev/audio\n"
# endif
#endif
        "  "PROG_NAME" playlist.m3u  /dev/audio\n"
        "\n"
        "For further information see the file \"MANUAL.TXT\".\n" );
}


/**********************************************************************************************************************************
 *
 *  Function: main()
 *
 *  This function has the following things to do:
 *   - Restricting access abilities when program is compiled for realtime support
 *     (you need SUID root rights to do this, but we only need the ability to switch priority, not to read and write alien files)
 *   - extended Wild card expansion
 *   - printing the copyright line when first argument is not --silent or --quiet
 *   - printing help when called without arguments or when help is requested
 *   - initialization of helper tables and decoder tables for SV4...8 (check that Init_Huffman_Decoder_SV*() can be called more than once)
 *   - search usable synthese filter for the current CPU
 *   - init dither engine with default settings when necessary
 *   - call mainloop(), which returns the number of decoding errors
 *   - print Overdrive report
 *   - print warnings, when errors occured
 *   - START(), ENTER(), LEAVE(), REPORT() are profiling macros. When profiling is disabled, they are dummy commands.
 *
 */

int Cdecl
main ( int     argc,
       char**  argv )
{
    static const char*  extentions [] = { ".mpc", ".mpp", ".mp+", NULL };
    int                 ret;

#if (defined USE_OSS_AUDIO  ||  defined USE_ESD_AUDIO  ||  defined USE_SUN_AUDIO)  &&  (defined USE_REALTIME  ||  defined USE_NICE)
    DisableSUID ();
#endif

#if   defined _OS2
    _wildcard ( &argc, &argv );
#elif defined USE_ARGV
    mysetargv ( &argc, &argv, extentions );
#endif

    START();
    ENTER(1);

    // Copyright message
    if ( argc < 2  ||  ( 0 != strcmp (argv[1], "--silent")  &&  0 != strcmp (argv[1], "--quiet")) )
        (void) stderr_printf ("\r\x1B[1m\r%s\n\x1B[0m\r     \r", About );

    // No arguments or call for help
    if ( argc < 2  ||  0 == strcmp (argv[1], "-h")  ||  0 == strcmp (argv[1], "-?")  ||  0 == strcmp (argv[1], "--help") ) {
        usage ();
        return 1;
    }

    // initialize tables which must initialized once and only once
    Init_Huffman_Decoder_SV4_6  ();
    Init_Huffman_Decoder_SV7    ();
    Init_Decoder_Huffman_Tables ();

#ifdef USE_ASM
    Synthese_Filter_16 = Get_Synthese_Filter ();
#endif

#if defined MAKE_16BIT  ||  defined MAKE_24BIT  ||  defined MAKE_32BIT
    Init_Dither ( Bits, NoiseShapeType, Dither );       // initialize dither parameter with standard settings
#endif

    ret = mainloop ( argc, argv );                      // analyze command line and do the requested work

    OverdriveReport ();                                 // output a report if clipping was necessary

    switch ( errors ) {
    case 0:
        break;
    case 1:
        (void) stderr_printf ( "\n*** 1 decoded file has errors ***\a\n\n" );
        break;
    default:
        (void) stderr_printf ( "\n*** %u decoded files have errors ***\a\n\n", errors );
        break;
    }

    LEAVE(1);
    REPORT();
    return ret;
}

/* end of mppdec.c */
