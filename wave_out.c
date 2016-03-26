#include <string.h>
#include <errno.h>
#include "mppdec.h"


static UintMax_t
minfi ( UintMax_t x, unsigned long y )
{
    return x < y  ?  (UintMax_t)x  :  (UintMax_t)y;
}


/*
 *  Write 'len' Bytes to a Stream
 *  Output Error Message when write fails and repeat until success
 */

static size_t
write_with_test ( FILE_T outputFile, const void* data, size_t len )
{
    ssize_t  written;
    size_t   done = 0;

    ENTER(200);

#ifdef USE_WIN_AUDIO
    if ( outputFile == WINAUDIO_FD ) {
        WIN_Play_Samples ( data, len );
    }
    else
#endif
#ifdef USE_IRIX_AUDIO
    if ( outputFile == IRIXAUDIO_FD ) {
        IRIX_Play_Samples ( data, len );
    }
    else
#endif
    if ( outputFile != NULL_FD )
        while ( len != done ) {
            written = (size_t) WRITE (outputFile, (char*)data+done, len-done);
            if ( written <= 0 ) {
                stderr_printf ( "\n"PROG_NAME": write error: %s, repeat once more...\a\r", strerror(errno) );
                sleep (10);
                continue;
            }
            done += written;
        }

    LEAVE(200);
    return len;
}


/*
 *  Write a WAV header for a simple RIFF-WAV file with 1 PCM-Chunk. Settings are passed via function parameters.
 */

Int
Write_WAVE_Header ( FILE_T     outputFile,
                    Ldouble    SampleFreq,
                    Uint       BitsPerSample,
                    Uint       Channels,
                    UintMax_t  SamplesPerChannel )
{
    Uint8_t   Header [44];
    Uint8_t*  p             = Header;
    Uint      Bytes         = (BitsPerSample + 7) / 8;
    UintMax_t PCMdataLength = Channels * Bytes * SamplesPerChannel;
    Uint32_t  word32;
    size_t    ret;

    ENTER(201);

    *p++ = 'R';
    *p++ = 'I';
    *p++ = 'F';
    *p++ = 'F';                                   // "RIFF" label

    word32 = minfi ( PCMdataLength + (44 - 8), 0xFFFFFFFFLU );
    *p++ = (Uint8_t)(word32 >>  0);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >> 24);               // Size of the next chunk

    *p++ = 'W';
    *p++ = 'A';
    *p++ = 'V';
    *p++ = 'E';                                   // "WAVE" label

    *p++ = 'f';
    *p++ = 'm';
    *p++ = 't';
    *p++ = ' ';                                   // "fmt " label

    *p++ = 0x10;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;                                  // length of the PCM data declaration = 2+2+4+4+2+2

#ifdef MAKE_32BIT_FPU
    *p++ = 0x03;
    *p++ = 0x00;                                  // ACM type 0x0003 = uncompressed FPU PCM
#else
    *p++ = 0x01;
    *p++ = 0x00;                                  // ACM type 0x0001 = uncompressed linear PCM
#endif

    *p++ = (Uint8_t)(Channels >> 0);
    *p++ = (Uint8_t)(Channels >> 8);              // Channels

    word32 = (Uint32_t) (SampleFreq + 0.5);
    *p++ = (Uint8_t)(word32 >>  0);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >> 24);               // Sample frequency

    word32 *= Bytes * Channels;
    *p++ = (Uint8_t)(word32 >>  0);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >> 24);               // Bytes per second in the data stream

    word32 = Bytes * Channels;
    *p++ = (Uint8_t)(word32 >>  0);
    *p++ = (Uint8_t)(word32 >>  8);               // Bytes per sample time

    *p++ = (Uint8_t)(BitsPerSample >> 0);
    *p++ = (Uint8_t)(BitsPerSample >> 8);         // Bits per single sample

    *p++ = 'd';
    *p++ = 'a';
    *p++ = 't';
    *p++ = 'a';                                   // "data" label

    word32 = minfi ( PCMdataLength, 0xFFFFFFFFLU );
    *p++ = (Uint8_t)(word32 >>  0);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >> 24);               // Größe der rohen PCM-Daten

    assert ( p == Header + sizeof Header );   // nix vergessen oder zuviel?

    ret = write_with_test ( outputFile, Header, sizeof Header );
    LEAVE(201);
    return ret;
}


/*
 *  Write a header for a headerless RAW PCM file. Settings are passed via function parameters.
 *  Of cause, this is only a dummy function.
 */

Int
Write_Raw_Header ( FILE_T     outputFile,
                   Ldouble    SampleFreq,
                   Uint       BitsPerSample,
                   Uint       Channels,
                   UintMax_t  SamplesPerChannel )
{
    (void) outputFile;
    (void) SampleFreq;
    (void) BitsPerSample;
    (void) Channels;
    (void) SamplesPerChannel;
    return 0;
}


/*
 *  Write a 80 bit IEEE854 big endian number as 10 octets. Destination is passed as pointer,
 *  End of destination (p+10) is returned.
 */

static Uint8_t*
Convert_to_80bit_BE_IEEE854_Float ( Uint8_t* p, Ldouble val )
{
#ifndef HAVE_IEEE854_LONGDOUBLE
    Uint32_t  word32 = 0x401E;

    if ( val > 0.L )
        while ( val < (Ldouble)0x80000000 )                    // scales value in the range 2^31...2^32
            word32--, val *= 2.L;                              // so you have the exponent

    *p++   = (Uint8_t)(word32 >>  8);
    *p++   = (Uint8_t)(word32 >>  0);                          // write exponent, sign is assumed as '+'
    word32 = (Uint32_t) val;
    *p++   = (Uint8_t)(word32 >> 24);
    *p++   = (Uint8_t)(word32 >> 16);
    *p++   = (Uint8_t)(word32 >>  8);
    *p++   = (Uint8_t)(word32 >>  0);                          // write the upper 32 bit of the mantissa
    word32 = (Uint32_t) ( (val - word32) * 4294967296.L );
    *p++   = (Uint8_t)(word32 >> 24);
    *p++   = (Uint8_t)(word32 >> 16);
    *p++   = (Uint8_t)(word32 >>  8);
    *p++   = (Uint8_t)(word32 >>  0);                          // write the lower 32 bit of the mantissa
#elif ENDIAN == HAVE_LITTLE_ENDIAN
    const Uint8_t*  q = (Uint8_t*) &val;

    *p++ = q[9];                                               // only change the endianess
    *p++ = q[8];
    *p++ = q[7];
    *p++ = q[6];
    *p++ = q[5];
    *p++ = q[4];
    *p++ = q[3];
    *p++ = q[2];
    *p++ = q[1];
    *p++ = q[0];
#elif defined MUST_ALIGNED
    const Uint8_t*  q = (Uint8_t*) &val;

    *p++ = q[0];                                               // only copy
    *p++ = q[1];
    *p++ = q[2];
    *p++ = q[3];
    *p++ = q[4];
    *p++ = q[5];
    *p++ = q[6];
    *p++ = q[7];
    *p++ = q[8];
    *p++ = q[9];
#else
    *(Ldouble*)p = val;                                        // copy directly
    p += 10;
#endif /* HAVE_IEEE854_LONGDOUBLE */

    return p;
}


/*
 *  Write an AIFF header for a simple AIFF file with 1 PCM-Chunk. Settings are passed via function parameters.
 */

Int
Write_AIFF_Header ( FILE_T     outputFile,
                    Ldouble    SampleFreq,
                    Uint       BitsPerSample,
                    Uint       Channels,
                    UintMax_t  SamplesPerChannel )
{
    Uint8_t      Header [54];
    Uint8_t*     p             = Header;
    Uint         Bytes         = (BitsPerSample + 7) / 8;
    UintMax_t    PCMdataLength = Channels * Bytes * SamplesPerChannel;
    Uint32_t     word32;
    size_t       ret;

    ENTER(203);

    // FORM chunk
    *p++ = 'F';
    *p++ = 'O';
    *p++ = 'R';
    *p++ = 'M';

    word32 = (Uint32_t) PCMdataLength + 0x2E;  // size of the AIFF chunk
    *p++ = (Uint8_t)(word32 >> 24);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >>  0);

    *p++ = 'A';
    *p++ = 'I';
    *p++ = 'F';
    *p++ = 'F';
    // end of FORM chunk

    // COMM chunk
    *p++ = 'C';
    *p++ = 'O';
    *p++ = 'M';
    *p++ = 'M';

    word32 = 0x12;                             // size of this chunk
    *p++ = (Uint8_t)(word32 >> 24);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >>  0);

    word32 = Channels;                         // channels
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >>  0);

    word32 = minfi ( SamplesPerChannel, 0xFFFFFFFFLU ); // so called "frames"
    *p++ = (Uint8_t)(word32 >> 24);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >>  0);

    word32 = BitsPerSample;                    // bits
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >>  0);

    p = Convert_to_80bit_BE_IEEE854_Float ( p, SampleFreq );  // sample frequency as big endian 80 bit IEEE854 float
    // End of COMM chunk

    // SSND chunk
    *p++ = 'S';
    *p++ = 'S';
    *p++ = 'N';
    *p++ = 'D';

    word32 = (Uint32_t) PCMdataLength + 0x08;  // chunk length
    *p++ = (Uint8_t)(word32 >> 24);
    *p++ = (Uint8_t)(word32 >> 16);
    *p++ = (Uint8_t)(word32 >>  8);
    *p++ = (Uint8_t)(word32 >>  0);

    *p++ = 0;                                  // offset
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;

    *p++ = 0;                                  // block size
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;

    assert ( p == Header + sizeof Header );     // nix vergessen oder zuviel?

    ret = write_with_test ( outputFile, Header, sizeof(Header) );
    LEAVE(203);
    return ret;
}


/***********************************************************************************
 *
 *  Write 16 bit PCM samples from 16 bit PCM data, needed if no MAKE_xxBIT is defined
 *
 ***********************************************************************************/

#if !defined MAKE_16BIT  &&  !defined MAKE_24BIT  &&  !defined MAKE_32BIT

static void
Change_Endian_16 ( Int16_t* dst, size_t words16bit )
{
    Uint8_t*  p = (Uint8_t*)dst;

    ENTER(202);

# if  INT_MAX >= 2147483647L  &&  !defined MUST_ALIGNED
    for ( ; words16bit > 1; words16bit -= 2, p += 4 ) {
        Uint32_t  tmp = *(Uint32_t*)p;
        *(Uint32_t*)p = ((tmp << 0x08) & 0xFF00FF00) | ((tmp >> 0x08) & 0x00FF00FF);
    }
    if ( words16bit > 0 ) {
        Uint16_t  tmp = *(Uint16_t*)p;
        *(Uint16_t*)p = ((tmp << 0x08) & 0xFF00) | ((tmp >> 0x08) & 0x00FF);
    }
# else
    for ( ; words16bit--; p += 2 ) {
        Uint8_t  tmp;
        tmp  = p[0];
        p[0] = p[1];
        p[1] = tmp;
    }
# endif

    LEAVE(202);
    return;
}


size_t
Write_PCM_16bit ( FILE_T fp, const Int16_t* data, size_t len )
{
    size_t  ret;

    ENTER(203);

    if ( output_endianess != machine_endianess )
        Change_Endian_16 ( (Int16_t*)data, len );

    ret = write_with_test ( fp, data, 16/8 * len ) / (16/8);
    LEAVE(203);
    return ret;
}

#endif


/***********************************************************************************
 *
 *  Write 16 bit PCM samples from 32 bit PCM data, needed if MAKE_16BIT is defined
 *
 ***********************************************************************************/

#ifdef MAKE_16BIT

size_t
Write_PCM_HQ_16bit ( FILE_T fp, const Int32_t* data, size_t len )
{
    size_t          ret;
    Uint8_t         buff [1152 * 8 * 2];
    Uint8_t*        p = buff;
    const Uint8_t*  q = (const Uint8_t*) data;
    size_t          i;

    ENTER(213);

# if ENDIAN == HAVE_LITTLE_ENDIAN
    if ( output_endianess == LITTLE ) {
        for ( i = 0; i < len; i++, q += 4 ) {
            ((Int16_t*)p)[0] = ((Int16_t*)q)[1];
            p += 2;
        }
    }
    else {
        for ( i = 0; i < len; i++, q += 4 ) {
            *p++ = q[3];
            *p++ = q[2];
        }
    }
# else
    if ( output_endianess == LITTLE ) {
        for ( i = 0; i < len; i++, q += 4 ) {
            *p++ = q[1];
            *p++ = q[0];
        }
    }
    else {
        for ( i = 0; i < len; i++, q += 4 ) {
            ((Int16_t*)p)[0] = ((Int16_t*)q)[0];
            p += 2;
        }
    }
# endif

    ret = write_with_test ( fp, buff, 16/8 * len ) / (16/8);
    LEAVE(213);
    return ret;
}

#endif /* MAKE_16BIT */


/***********************************************************************************
 *
 *  Write 24 bit PCM samples from 32 bit PCM data, needed if MAKE_24BIT is defined
 *
 ***********************************************************************************/

#ifdef MAKE_24BIT

size_t
Write_PCM_HQ_24bit ( FILE_T fp, const Int32_t* data, size_t len )
{
    size_t          ret;
    Uint8_t         buff [1152 * 8 * 3];
    Uint8_t*        p = buff;
    const Uint8_t*  q = (const Uint8_t*) data;
    size_t          i;

    ENTER(213);

# if ENDIAN == HAVE_LITTLE_ENDIAN
    if ( output_endianess == LITTLE ) {
        for ( i = 0; i < len; i++, q += 4 ) {
            *p++ = q[1];
            *p++ = q[2];
            *p++ = q[3];
        }
    }
    else {
        for ( i = 0; i < len; i++, q += 4 ) {
            *p++ = q[3];
            *p++ = q[2];
            *p++ = q[1];
        }
    }
# else
    if ( output_endianess == LITTLE ) {
        for ( i = 0; i < len; i++, q += 4 ) {
            *p++ = q[2];
            *p++ = q[1];
            *p++ = q[0];
        }
    }
    else {
        for ( i = 0; i < len; i++, q += 4 ) {
            *p++ = q[0];
            *p++ = q[1];
            *p++ = q[2];
        }
    }
# endif

    ret = write_with_test ( fp, buff, 24/8 * len ) / (24/8);
    LEAVE(213);
    return ret;
}

#endif /* MAKE_24BIT */


/***********************************************************************************
 *
 *  Write 32 bit PCM samples from 32 bit PCM data, needed if MAKE_32BIT is defined
 *
 ***********************************************************************************/

#ifdef MAKE_32BIT

static void
Change_Endian_32 ( Int32_t* dst, size_t words32bit )
{
    ENTER(212);

    for ( ; words32bit--; dst++ ) {
# if  INT_MAX >= 2147483647L
        Uint32_t  tmp;
        tmp                 = ((Uint32_t*)dst)[0];
        tmp                 = ((tmp << 0x10) & 0xFFFF0000) | ((tmp >> 0x10) & 0x0000FFFF);
        ((Uint32_t*)dst)[0] = ((tmp << 0x08) & 0xFF00FF00) | ((tmp >> 0x08) & 0x00FF00FF);
# else
        Uint8_t  tmp;
        tmp                 = ((Uint8_t*)dst)[0];
        ((Uint8_t*)dst)[0]  = ((Uint8_t*)dst)[3];
        ((Uint8_t*)dst)[3]  = tmp;
        tmp                 = ((Uint8_t*)dst)[1];
        ((Uint8_t*)dst)[1]  = ((Uint8_t*)dst)[2];
        ((Uint8_t*)dst)[2]  = tmp;
# endif
    }

    LEAVE(212);
    return;
}


size_t
Write_PCM_HQ_32bit ( FILE_T fp, const Int32_t* data, size_t len )
{
    size_t  ret;

    ENTER(213);

    if ( output_endianess != machine_endianess )
        Change_Endian_32 ( (Int32_t*)data, len );

    ret = write_with_test ( fp, data, 32/8 * len ) / (32/8);
    LEAVE(213);
    return ret;
}

#endif /* MAKE_32BIT */


/***********************************************************************************
 *
 *  Write 32 bit PCM samples from 32 bitfloat  PCM data, needed if MAKE_32BIT_FPU is defined
 *
 ***********************************************************************************/

#ifdef MAKE_32BIT_FPU

static void
Change_Endian_32 ( Float32_t* dst, size_t words32bit )
{
    ENTER(212);

    for ( ; words32bit--; dst++ ) {
# if  INT_MAX >= 2147483647L
        Uint32_t  tmp;
        tmp                 = ((Uint32_t*)dst)[0];
        tmp                 = ((tmp << 0x10) & 0xFFFF0000) | ((tmp >> 0x10) & 0x0000FFFF);
        ((Uint32_t*)dst)[0] = ((tmp << 0x08) & 0xFF00FF00) | ((tmp >> 0x08) & 0x00FF00FF);
# else
        Uint8_t  tmp;
        tmp                 = ((Uint8_t*)dst)[0];
        ((Uint8_t*)dst)[0]  = ((Uint8_t*)dst)[3];
        ((Uint8_t*)dst)[3]  = tmp;
        tmp                 = ((Uint8_t*)dst)[1];
        ((Uint8_t*)dst)[1]  = ((Uint8_t*)dst)[2];
        ((Uint8_t*)dst)[2]  = tmp;
# endif
    }

    LEAVE(212);
    return;
}


size_t
Write_PCM_32bit ( FILE_T fp, const Float32_t* data, size_t len )
{
    size_t  ret;

    ENTER(213);

    if ( output_endianess != machine_endianess )
        Change_Endian_32 ( (Int32_t*)data, len );

    ret = write_with_test ( fp, data, 32/8 * len ) / (32/8);
    LEAVE(213);
    return ret;
}

#endif /* MAKE_32BIT_FPU */


#if defined USE_OSS_AUDIO

Int
Set_DSP_OSS_Params ( FILE_T   outputFile,
                     Ldouble  SampleFreq,
                     Uint     BitsPerSample,
                     Uint     Channels )
{
    int  arg;
    int  org;
    int  fd = FILENO (outputFile);

    org = arg = Channels;
    if ( -1 == ioctl ( fd, SOUND_PCM_WRITE_CHANNELS, &arg ) )
        return -1;
    if (arg != org)
        return -1;

    org = arg = BitsPerSample;
    if ( -1 == ioctl ( fd, SOUND_PCM_WRITE_BITS, &arg ) )
        return -1;
    if (arg != org)
        return -1;

    org = arg = AFMT_S16_LE;
    if ( -1 == ioctl ( fd, SNDCTL_DSP_SETFMT, &arg ) )
        return -1;
    if ((arg & org) == 0)
        return -1;

    org = arg = SampleFreq + 0.5;
    if ( -1 == ioctl ( fd, SOUND_PCM_WRITE_RATE, &arg ) )
        return -1;
    if ( 23.609375 * fabs(arg-SampleFreq) > fabs(arg+SampleFreq) )    // Sample frequency: Accept 40.5...48.0 kHz for 44.1 kHz
        return -1;

    SetPriority (99);
    return 0;
}

#endif /* USE_OSS_AUDIO */


#if defined USE_SUN_AUDIO

Int
Set_DSP_Sun_Params ( FILE_T   outputFile,
                     Ldouble  SampleFreq,
                     Uint     BitsPerSample,
                     Uint     Channels )
{
    audio_info_t  audio_info;
    int           fd = FILENO (outputFile);


    AUDIO_INITINFO ( &audio_info );
    audio_info.play.sample_rate = (unsigned int) (SampleFreq + 0.5);
    audio_info.play.channels    = Channels;
    audio_info.play.precision   = BitsPerSample;
    audio_info.play.encoding    = AUDIO_ENCODING_LINEAR;

    if ( 0 != ioctl (fd, AUDIO_SETINFO, &audio_info) )
        return -1;
    if ( audio_info.play.channels  != Channels )
        return -1;
    if ( audio_info.play.precision != BitsPerSample )
        return -1;
    if ( audio_info.play.encoding  != AUDIO_ENCODING_LINEAR )
        return -1;
    if ( 23.609375 * fabs(audio_info.play.sample_rate-SampleFreq) > fabs(audio_info.play.sample_rate+SampleFreq) )    // Sample frequency: Accept 40.5...48.0 kHz for 44.1 kHz
        return -1;

    SetPriority (99);
    return 0;
}

#endif /* USE_SUN_AUDIO */


#ifdef __TURBOC__
/*
 *  Turbo-C assigns stdin/stdout with 512 byte buffers if they are
 *  associated with regular files and pipes. This links a lot of code (ca.
 *  6 KByte) which is not used in this program, because FILE I/O buffering
 *  is done by the program itself (8/4 Kbyte for input, 4.5 KByte for
 *  output). Defining the next two functions as dummy functions breaks this
 *  linking chain. This is especially important because we only have limited
 *  space for Code and Data/Stack (each 64 KByte).
 */
# pragma argsused
int  pascal near  __IOerror ( int no ) { return -1; }
void near         _setupio  ( void )   {}
#endif /* __TURBOC__ */


#ifdef USE_ESD_AUDIO

# define AUDIO_FORMAT_UNSIGNED_8       1
# define AUDIO_FORMAT_SIGNED_16        2

int
Set_ESD_Params ( FILE_T   dummyFile,
                 Ldouble  SampleFreq,
                 Uint     BitsPerSample,
                 Uint     Channels )
{
    static unsigned int  esd_rate     = 0;
    static unsigned int  esd_format   = 0;
    static unsigned int  esd_channels = 0;
    esd_server_info_t*   info;
    esd_format_t         format = ESD_STREAM | ESD_PLAY;
    esd_format_t         fmt;
    int                  esd;
    int                  aif;

    (void) dummyFile;

    if ( esd_rate == 0 ) {
        if ( (esd = esd_open_sound (NULL)) >= 0 ) {
            info     = esd_get_server_info (esd);
            esd_rate = info -> rate;
            fmt      = info -> format;
            esd_free_server_info (info);
            esd_close (esd);
        }
        else {
            esd_rate = esd_audio_rate;
            fmt      = esd_audio_format;
        }
        esd_format = AUDIO_FORMAT_UNSIGNED_8;
        if ( (fmt & ESD_MASK_BITS) == ESD_BITS16 )
            esd_format |= AUDIO_FORMAT_SIGNED_16;
        esd_channels = fmt & ESD_MASK_CHAN;
    }

    switch ( (BitsPerSample + 7) / 8 ) {
    case  1:
        aif = AUDIO_FORMAT_UNSIGNED_8;
        break;
    case  2:
        aif = AUDIO_FORMAT_SIGNED_16;
        break;
    default:
        stderr_printf ( "audio: Wrong number of bits: %d\n", BitsPerSample );
        return -1;
    }

    if ( (aif & esd_format) == 0 ) {
        stderr_printf ( "audio: Wrong number of bits: %d\n", BitsPerSample );
        return -1;
    }
    if      ( aif & AUDIO_FORMAT_SIGNED_16  )
        format |= ESD_BITS16;
    else if ( aif & AUDIO_FORMAT_UNSIGNED_8 )
        format |= ESD_BITS8;
    else
        assert (0);

    if ( Channels <= 0 )
        Channels = 2;
    else if ( Channels > esd_channels ) {
        stderr_printf ( "audio: Unsupported number of channels: %d\n", Channels );
        return -1;
    }

    if ( Channels == 1 )
        format |= ESD_MONO;
    else if ( Channels == 2 )
        format |= ESD_STEREO;
    else
        assert (0);

    if ( SampleFreq == -1 )
        SampleFreq = esd_rate;
    else if ( SampleFreq > esd_rate )
        return -1;

    SetPriority (99);
    return esd_play_stream_fallback ( format, (int)(SampleFreq + 0.5), NULL, PROG_NAME );
}

#endif /* USE_ESD_AUDIO */


#ifdef USE_WIN_AUDIO

static CRITICAL_SECTION  cs;
static HWAVEOUT          dev                    = NULL;
static int               ScheduledBlocks        = 0;
static int               PlayedWaveHeadersCount = 0;          // free index
static WAVEHDR*          PlayedWaveHeaders [MAX_WAVEBLOCKS];

/* Das ganze sollte auf einen festen Memorypool umgestellt werden, der nicht dauernd alloziert und dealloziert wird */

static int
Box ( const char* msg )
{
    MessageBox ( NULL, msg, " "PROG_NAME": Error Message . . .", MB_OK | MB_ICONEXCLAMATION );
    return -1;
}


/*
 *  This function registers already played WAVE chunks. Freeing is done by free_memory(),
 */

static void CALLBACK
wave_callback ( HWAVE hWave, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 )
{
    if ( uMsg == WOM_DONE ) {
        EnterCriticalSection ( &cs );
        PlayedWaveHeaders [PlayedWaveHeadersCount++] = (WAVEHDR*) dwParam1;
        LeaveCriticalSection ( &cs );
    }
}


static void
free_memory ( void )
{
    WAVEHDR*  wh;
    HGLOBAL   hg;

    EnterCriticalSection ( &cs );
    wh = PlayedWaveHeaders [--PlayedWaveHeadersCount];
    ScheduledBlocks--;                               // decrease the number of USED blocks
    LeaveCriticalSection ( &cs );

    waveOutUnprepareHeader ( dev, wh, sizeof (WAVEHDR) );

    hg = GlobalHandle ( wh -> lpData );       // Deallocate the buffer memory
    GlobalUnlock (hg);
    GlobalFree   (hg);

    hg = GlobalHandle ( wh );                 // Deallocate the header memory
    GlobalUnlock (hg);
    GlobalFree   (hg);
}


/*
WAVEFORMATPCMEX  waveFormatPCMEx;

waveFormatPCMEx.Format.wFormatTag       = WAVE_FORMAT_EXTENSIBLE;
waveFormatPCMEx.Format.nChannels        =  4;
waveFormatPCMEx.Format.nSamplesPerSec   =  44100L;
waveFormatPCMEx.Format.nAvgBytesPerSec  = 352800L;
waveFormatPCMEx.Format.nBlockAlign      =  8;                   // Same as the usual
waveFormatPCMEx.Format.wBitsPerSample   = 16;
waveFormatPCMEx.Format.cbSize           = 22;                   // After this to GUID
waveFormatPCMEx.wValidBitsPerSample     = 16;                   // All bits have data
waveFormatPCMEx.dwChannelMask           = 0x00000033;           // Quadraphonic setup
waveFormatPCMEx.guidSubFormat           = WAVE_FORMAT_PCM_EX;   // New format
*/

Int
Set_WIN_Params ( FILE_T   dummyFile ,
                 Ldouble  SampleFreq,
                 Uint     BitsPerSample,
                 Uint     Channels )
{
    WAVEFORMATEX  outFormat;
    UINT          deviceID = WAVE_MAPPER;

    (void) dummyFile;

    if ( waveOutGetNumDevs () == 0 )
        return Box ( "No audio device present." );

    outFormat.wFormatTag      = WAVE_FORMAT_PCM;
    outFormat.wBitsPerSample  = BitsPerSample;
    outFormat.nChannels       = Channels;
    outFormat.nSamplesPerSec  = (unsigned long)(SampleFreq + 0.5);
    outFormat.nBlockAlign     = (outFormat.wBitsPerSample + 7) / 8 * outFormat.nChannels;
    outFormat.nAvgBytesPerSec = outFormat.nSamplesPerSec * outFormat.nBlockAlign;

    switch ( waveOutOpen ( &dev, deviceID, &outFormat, (DWORD)wave_callback, 0, CALLBACK_FUNCTION ) ) {
    case MMSYSERR_ALLOCATED:   return Box ( "Device is already open." );
    case MMSYSERR_BADDEVICEID: return Box ( "The specified device is out of range." );
    case MMSYSERR_NODRIVER:    return Box ( "There is no audio driver in this system." );
    case MMSYSERR_NOMEM:       return Box ( "Unable to allocate sound memory." );
    case WAVERR_BADFORMAT:     return Box ( "This audio format is not supported." );
    case WAVERR_SYNC:          return Box ( "The device is synchronous." );
    default:                   return Box ( "Unknown media error." );
    case MMSYSERR_NOERROR:     break;
    }

    waveOutReset ( dev );
    InitializeCriticalSection ( &cs );
#if   defined USE_REALTIME
    SetPriorityClass ( GetCurrentProcess (), REALTIME_PRIORITY_CLASS );
#elif defined USE_NICE
    SetPriorityClass ( GetCurrentProcess (), HIGH_PRIORITY_CLASS );
#endif
    return 0;
}


int
WIN_Play_Samples ( const void* data, size_t len )
{
    HGLOBAL    hg;
    HGLOBAL    hg2;
    LPWAVEHDR  wh;
    void*      allocptr;

    do {
        while ( PlayedWaveHeadersCount > 0 )                        // free used blocks ...
            free_memory ();

        if ( ScheduledBlocks < sizeof(PlayedWaveHeaders)/sizeof(*PlayedWaveHeaders) ) // wait for a free block ...
            break;
        Sleep (26);
    } while (1);

    if ( (hg2 = GlobalAlloc ( GMEM_MOVEABLE, len )) == NULL )   // allocate some memory for a copy of the buffer
        return Box ( "GlobalAlloc failed." );

    allocptr = GlobalLock (hg2);
    CopyMemory ( allocptr, data, len );                         // Here we can call any modification output functions we want....

    if ( (hg = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof (WAVEHDR))) == NULL ) // now make a header and WRITE IT!
        return -1;

    wh                   = GlobalLock (hg);
    wh -> dwBufferLength = len;
    wh -> lpData         = allocptr;

    if ( waveOutPrepareHeader ( dev, wh, sizeof (WAVEHDR)) != MMSYSERR_NOERROR ) {
        GlobalUnlock (hg);
        GlobalFree   (hg);
        return -1;
    }

    if ( waveOutWrite ( dev, wh, sizeof (WAVEHDR)) != MMSYSERR_NOERROR ) {
        GlobalUnlock (hg);
        GlobalFree   (hg);
        return -1;
    }

    EnterCriticalSection ( &cs );
    ScheduledBlocks++;
    LeaveCriticalSection ( &cs );

    return len;
}


int
WIN_Audio_close ( void )
{
    if ( dev != NULL ) {

        while ( ScheduledBlocks > 0 ) {
            Sleep (ScheduledBlocks);
            while ( PlayedWaveHeadersCount > 0 )                        // free used blocks ...
                free_memory ();
        }

        waveOutReset (dev);      // reset the device
        waveOutClose (dev);      // close the device
        dev = NULL;
    }

    DeleteCriticalSection ( &cs );
    ScheduledBlocks = 0;
    return 0;
}

#endif /* USE_WIN_AUDIO */


#ifdef USE_IRIX_AUDIO

/*
 *  output_irix.c
 *
 *      Copyright (C) Aaron Holtzman - May 1999
 *      Port to IRIX by Jim Miller, SGI - Nov 1999
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <audio.h>


static int       init         = 0;
static ALport    alport       = 0;
static ALconfig  alconfig     = 0;
static int       bytesPerWord = 1;
static int       nChannels    = 2;


// open the audio device for writing to

int
Set_IRIX_Params ( FILE_T dummyFile , Ldouble SampleFreq, Uint BitsPerSample, Uint Channels );
{
    ALpv  params [2];
    int   dev   = AL_DEFAULT_OUTPUT;
    int   wsize = AL_SAMPLE_16;

    nChannels = Channels;

    if ( init == 0 ) {
        init     = 1;
        alconfig = alNewConfig ();

        if ( alSetQueueSize ( alconfig, BUFFER_SIZE) < 0 ) {
            stderr_printf ( "alSetQueueSize failed: %s\n", alGetErrorString(oserror()) );
            return -1;
        }

        if ( alSetChannels ( alconfig, Channels) < 0 ) {
            stderr_printf ( "alSetChannels(%d) failed: %s\n", Channels, alGetErrorString(oserror()) );
            return -1;
        }

        if ( alSetDevice ( alconfig, dev) < 0 ) {
            stderr_printf ( "alSetDevice failed: %s\n", alGetErrorString(oserror()) );
            return -1;
        }

        if ( alSetSampFmt ( alconfig, AL_SAMPFMT_TWOSCOMP) < 0 ) {
            stderr_printf ( "alSetSampFmt failed: %s\n", alGetErrorString(oserror()) );
            return -1;
        }

        alport = alOpenPort ("mppdec", "w", 0 );
        if ( alport == 0 ) {
            stderr_printf ( "alOpenPort failed: %s\n", alGetErrorString(oserror()) );
            return -1;
        }

        switch ( BitsPerSample ) {
        case 8:
            bytesPerWord = 1;
            wsize        = AL_SAMPLE_8;
            break;
        case 16:
            bytesPerWord = 2;
            wsize        = AL_SAMPLE_16;
            break;
        case 24:
            bytesPerWord = 4;
            wsize        = AL_SAMPLE_24;
            break;
        default:
            stderr_printf ( "Irix audio: unsupported bit with %d\n", BitsPerSample );
            return -1;
        }

        if ( alSetWidth ( alconfig, wsize) < 0 ) {
            stderr_printf ( "alSetWidth failed: %s\n", alGetErrorString(oserror()) );
            return -1;
        }

        params [0].param    = AL_RATE;
        params [0].value.ll = alDoubleToFixed ((double) SampleFreq);
        params [1].param    = AL_MASTER_CLOCK;
        params [1].value.i  = AL_CRYSTAL_MCLK_TYPE;
        if ( alSetParams ( dev, params, 1) < 0 ) {
            stderr_printf ( "alSetParams() failed: %s\n", alGetErrorString(oserror()) );
            return -1;
        }
    }

    return 0;
}


// play the sample to the already opened file descriptor

int
IRIX_Play_Samples ( const void* buff, size_t len )
{
    alWriteFrames ( alport, buff, len );
    return len;
}


int
IRIX_Audio_close ( void )
{
    alClosePort  ( alport );
    alFreeConfig ( alconfig );
    alport   = 0;
    alconfig = 0;
    init     = 0;
    return 0;
}

#endif /* USE_IRIX_AUDIO */


/* end of wave_out.c */
