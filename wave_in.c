/*
 *  Read of PCM data from files and devices
 *
 *  (C) Frank Klemm 2002. All rights reserved.
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
 *    -
 */

#include "mppenc.h"


static unsigned short
Read16 ( FILE* fp )
{
    unsigned char  buff [2];

    if ( fread ( buff, 1, 2, fp ) != 2 )
        return -1;
    return buff[0] | (buff[1] << 8);
}

static unsigned long
Read32 ( FILE* fp )
{
    unsigned char  buff [4];

    if ( fread ( buff, 1, 4, fp ) != 4 )
        return -1;
    return (buff[0] | (buff[1] << 8)) | ((unsigned long)(buff[2] | (buff[3] << 8)) << 16);
}

// read WAVE header

static int
Read_WAV_Header ( wave_t* type )
{
    FILE*  fp = type -> fp;

    if ( fp == NULL ) {
        stderr_printf ("fp == NULL\n");
        return -1;
    }

    fseek ( fp, 0, SEEK_SET );
    if ( Read32 (fp) != 0x46464952 ) {                  // 4 Byte: check for "RIFF"
        stderr_printf ( Read32(fp) == -1  ?  " ERROR: Empty file or no data from coprocess!\n\n"
                                          :  " ERROR: 'RIFF' not found in WAVE header!\n\n");
        return -1;
    }
    Read32 (fp);                                        // 4 Byte: chunk size (ignored)
    if ( Read32 (fp) != 0x45564157 ) {                  // 4 Byte: check for "WAVE"
        stderr_printf ( " ERROR: 'WAVE' not found in WAVE header!\n\n");
        return -1;
    }
    if ( Read32 (fp) != 0x20746D66 ) {                  // 4 Byte: check for "fmt "
        stderr_printf ( " ERROR: 'fmt ' not found in WAVE header!\n\n");
        return -1;
    }
    Read32 (fp);                                        // 4 Byte: read chunk-size (ignored)
    if ( Read16 (fp) != 0x0001 ) {                      // 2 Byte: check for linear PCM
        stderr_printf ( " ERROR: WAVE file has no linear PCM format!\n\n");
        return -1;
    }
    type -> Channels    = Read16 (fp);                  // 2 Byte: read no of channels
    type -> SampleFreq  = Read32 (fp);                  // 4 Byte: read sampling frequency
    Read32 (fp);                                        // 4 Byte: read avg. blocksize (fs*channels*bytepersample)
    Read16 (fp);                                        // 2 Byte: read byte-alignment (channels*bytepersample)
    type -> BitsPerSample = Read16 (fp);                  // 2 Byte: read bits per sample
    type -> BytesPerSample= (type -> BitsPerSample + 7) / 8;
    while ( 1 ) {                                       // search for "data"
        if ( feof (fp) )
            return -1;
        if ( Read16 (fp) != 0x6164 )
            continue;
        if ( Read16 (fp) == 0x6174 )
            break;
    }
    type -> PCMBytes      = Read32 (fp);                  // 4 Byte: no. of byte in file
    if ( feof (fp) ) return -1;
    type -> PCMSamples    = type -> PCMBytes >= 0xFFFFFF00  ||  type -> PCMBytes == 0  ||  (UintMax_t)type -> PCMBytes % (type -> Channels * type -> BytesPerSample) != 0  ?  // finally calculate number of samples
                            (UintMax_t)(36000000 * type -> SampleFreq + 0.5)  :
                            type -> PCMBytes / (type -> BytesPerSample * type -> Channels);
    type -> PCMOffset     = ftell (fp);
    return 0;
}


#define EXT(x)  (0 == strcasecmp (ext, #x))

int
Open_WAV_Header ( wave_t* type, const char* filename )
{
    const char*  ext = strrchr ( filename, '.');
    int          tmp;

    if ( 0 == strcmp ( filename, "-")  ||  0 == strcmp ( filename, "/dev/stdin") ) {
        type -> type = TYPE_FILE;
        type -> fp   = SETBINARY_IN ( stdin );
        return Read_WAV_Header (type);
    }
#ifndef _WIN32
    else if ( 0 == strncmp ( filename, "/dev/", 5) ) {
        int     fd;
        int     arg;
        int     org;
        char    devname [128];
        int     fs  = 44100;
        double  dur = 86400.;

        sscanf ( filename, "%64[^:]:%u:%lf", devname, &fs, &dur );

        fd = open (devname, O_RDONLY);
        if ( fd < 0 )
            return -1;

        type -> Channels = org = arg = 2;
        if ( -1 == ioctl ( fd, SOUND_PCM_WRITE_CHANNELS, &arg ) )
            return -1;
        if (arg != org)
            return -1;

        type -> BitsPerSample = org = arg = 16;
        type -> BytesPerSample = 2;
        if ( -1 == ioctl ( fd, SOUND_PCM_WRITE_BITS, &arg ) )
            return -1;
        if (arg != org)
            return -1;

        org = arg = AFMT_S16_LE;
        if ( -1 == ioctl ( fd, SNDCTL_DSP_SETFMT, &arg ) )
            return -1;
        if ((arg & org) == 0)
            return -1;

        type -> SampleFreq = org = arg = (int) floor (fs + 0.5);
        if ( -1 == ioctl ( fd, SOUND_PCM_WRITE_RATE, &arg ) )
            return -1;
        if ( 23.609375 * abs(arg-org) > abs(arg+org) )    // Sample frequency: Accept 40.5...48.0 kHz for 44.1 kHz
            return -1;

        type -> PCMOffset      =  0;
        type -> PCMBytes       = 0xFFFFFFFF;
        type -> PCMSamples     = (unsigned int) (dur * type -> SampleFreq);

        type -> fp = fdopen (fd, "rb");
        SetPriority (99);
        return 0;
    }
#else
    else if ( 0 == strncmp ( filename, "/dev/audio", 10 ) ) {
        int     fs  = 44100;
        double  dur = 86400.;

        sscanf ( filename, "%*[^:]:%u:%lf", &fs, &dur );

        type -> type           = TYPE_SPECIAL;
        type -> fp             = (FILE*)-1;
        type -> Channels       =  2;
        type -> BitsPerSample  = 16;
        type -> BytesPerSample =  2;
        type -> SampleFreq     = fs;
        type -> PCMOffset      =  0;
        type -> PCMBytes       = 0xFFFFFFFF;
        type -> PCMSamples     = dur * type -> SampleFreq;
        tmp  = init_in ( FRAME_ADVANCE, (int) floor (type -> SampleFreq + 0.5), type -> Channels, type -> BitsPerSample );
            if ( tmp )
                return -1;
        SetPriority (99);
        return 0;
    }
#endif
    else if ( ext == NULL ) {
        type -> type           = TYPE_UNKNOWN;
        type -> fp             = NULL;
        return -1;
    }
    else if ( EXT(.wv) ) {                              // wavpack (www.wavpack.com)
        type -> type           = TYPE_FILE;
        type -> fp             = pipeopen ( "wvunpack # -", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.la) ) {                              // lossless-audio (www.lossless-audio.com)
        type -> type           = TYPE_FILE;
        type -> fp             = pipeopen ( "la -console #", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.wav) ) {
        type -> type           = TYPE_FILE;
        type -> fp             = fopen ( filename, "rb" );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.raw)  ||  EXT(.cdr)  ||  EXT(.pcm) ) {
        type -> type           = TYPE_FILE;
        type -> fp             = fopen ( filename, "rb" );
        type -> Channels       =  2;
        type -> BitsPerSample  = 16;
        type -> BytesPerSample =  2;
        type -> SampleFreq     = 44100.;
        type -> PCMOffset      =  0;
        type -> PCMBytes       = 0xFFFFFFFF;
        type -> PCMSamples     = 86400 * type -> SampleFreq;
        return type -> fp == NULL  ?  -1  :  0;
    }
    else if ( EXT(.pac)  ||  EXT(.lpac)  ||  EXT(.lpa) ) {
        type -> type           = TYPE_PIPE;
#if 1
        type -> fp             = pipeopen ( "lpac -o -x #", filename );
#else
        type -> fp             = pipeopen ( "lpac -x # -", filename );
#endif
        return Read_WAV_Header (type);
    }
    else if ( EXT(.fla)  ||  EXT(.flac) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "flac -d -s -c - < #", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.rka)  ||  EXT(.rkau) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "rkau # -", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.sz) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "szip -d < #", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.sz2) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "szip2 -d < #", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.ofr) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "optimfrog d # -", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.ape) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "mac # - -d", filename );
        return Read_WAV_Header (type);
    }
    else if ( EXT(.shn)  ||  EXT(.shorten) ) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "shorten -x # -", filename );
        if ( type -> fp == NULL )
            type -> fp = pipeopen ( "shortn32 -x # -", filename );
        return Read_WAV_Header (type);
    }
    else if (EXT(.mod)) {
        type -> type           = TYPE_PIPE;
        type -> fp             = pipeopen ( "xmp -b16 -c -f44100 --stereo -o- #", filename );
        type -> Channels       = 2;
        type -> BitsPerSample  = 16;
        type -> BytesPerSample = 2;
        type -> SampleFreq     = 44100.;
        type -> PCMOffset      = 0;
        type -> PCMBytes       = 0xFFFFFFFF;
        type -> PCMSamples     = 86400 * type -> SampleFreq;
        return type -> fp == NULL  ?  -1  :  0;
    }
    else {
        type -> type           = TYPE_UNKNOWN;
        type -> fp             = NULL;
        return -1;
    }
}

#undef EXT


int
Close_WAV_Header ( wave_t* type )
{
    switch ( type -> type ) {
    case TYPE_PIPE:
        return PCLOSE (type -> fp);
    case TYPE_FILE:
        return fclose (type -> fp);
    }
    return 0;
}






static float f0  ( const void* p )
{
    return (void)p, 0.;
}

static float f8  ( const void* p )
{
    return (((unsigned char*)p)[0] - 128) * 256.;
}

static float f16 ( const void* p )
{
    return ((unsigned char*)p)[0] + 256. * ((signed char*)p)[1];
}

static float f24 ( const void* p )
{
    return ((unsigned char*)p)[0]*(1./256) + ((unsigned char*)p)[1] + 256 * ((signed char*)p)[2];
}

static float f32 ( const void* p )
{
    return ((unsigned char*)p)[0]*(1./65536) + ((unsigned char*)p)[1]*(1./256) + ((unsigned char*)p)[2] + 256 * ((signed char*)p)[3];
}


typedef float (*rf_t) (const void*);

static int
DigitalSilence ( void* buffer, size_t len )
{
    unsigned long*  pl;
    unsigned char*  pc;
    size_t          loops;

    for ( pl = buffer, loops = len >> 3; loops--; pl += 2 )
        if ( pl[0] | pl[1] )
            return 0;

    for ( pc = (unsigned char*)pl, loops = len & 7; loops--; pc++ )
        if ( pc[0] )
            return 0;

    return 1;
}

/*
   1. Front Left - FL
   2. Front Right - FR
   3. Front Center - FC
   4. Low Frequency - LF
   5. Back Left - BL
   6. Back Right - BR
   7. Front Left of Center - FLC
   8. Front Right of Center - FRC
   9. Back Center - BC
  10. Side Left - SL
  11. Side Right - SR
  12. Top Center - TC
  13. Top Front Left - TFL
  14. Top Front Center - TFC
  15. Top Front Right - TFR
  16. Top Back Left - TBL
  17. Top Back Center - TBC
  18. Top Back Right - TBR
 */

size_t
Read_WAV_Samples ( wave_t*       t,
                   const size_t  RequestedSamples,
                   Float         p [] [ANABUFFER],
                   const float*  scale,
                   int*          Silence )
{
    static const rf_t  rf [5] = { f0, f8, f16, f24, f32 };
    short              Buffer [8 * 32/16 * BLOCK];          // read buffer, up to 8 channels, up to 32 bit
    short*             b              = (short*) Buffer;
    char*              c              = (char*)  Buffer;
    size_t             ReadSamples;                         // returns number of read samples
    size_t             i;
    size_t             j;

    ENTER(120);

    // Read PCM data
#ifdef _WIN32
    if ( t->fp != (FILE*)-1 ) {
        ReadSamples = fread ( b, t->BytesPerSample * t->Channels, RequestedSamples, t->fp );
    }
    else {
        while (1) {
            ReadSamples = get_in (b) / ( t->Channels * t->BytesPerSample );
            if ( ReadSamples != 0 )
                break;
            Sleep (10);
        }
    }
#else
    ReadSamples = fread ( b, t->BytesPerSample * t->Channels, RequestedSamples, t->fp );
#endif

    *Silence    = DigitalSilence ( b, ReadSamples * t->BytesPerSample * t->Channels );

    // Add Null Samples if EOF is reached
    if ( ReadSamples != RequestedSamples )
        memset ( b + ReadSamples * t->Channels, 0, (RequestedSamples - ReadSamples) * (sizeof(short) * t->Channels) );

#if ENDIAN == HAVE_LITTLE_ENDIAN
    if ( t->BytesPerSample == 2 ) {
        switch ( t->Channels ) {
        case 1:
            for ( i = 0; i < RequestedSamples; i++, b++ ) {
                p[0][i] = \
                p[1][i] = b[0] * scale[0];
            }
            break;
        case 2:
            for ( i = 0; i < RequestedSamples; i++, b += 2 ) {
                p[0][i] = b[0] * scale[0];
                p[1][i] = b[1] * scale[1];
            }
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            for ( i = 0; i < RequestedSamples; i++ )
                for ( j = 0; j < t->Channels; j++, b++ )
                    p[j][i] = b[0] * scale[j];
            break;
        default:
            assert (0);
        }
    }
    else
#endif
    {
        unsigned int  bytes = t->BytesPerSample;
        rf_t          f     = rf [bytes];

        c = (char*)b;
        switch ( t->Channels ) {
        case 1:
            for ( i = 0; i < RequestedSamples; i++, c += bytes ) {
                p[0][i] = \
                p[1][i] = f(c) * scale[0];
            }
            break;
        case 2:
            for ( i = 0; i < RequestedSamples; i++, c += 2*bytes ) {
                p[0][i] = f(c)       * scale[0];
                p[1][i] = f(c+bytes) * scale[1];
            }
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            for ( i = 0; i < RequestedSamples; i++ )
                for ( j = 0; j < t->Channels; j++, c += bytes )
                    p[j][i] = f(c) * scale[j];
            break;
        default:
            assert (0);
        }
    }

    LEAVE(120);
    return ReadSamples;
}




#ifdef _WIN32

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <io.h>
#include <fcntl.h>


#define NBLK  383               // 10 sec of audio


typedef struct {
    int      active;
    char*    data;
    size_t   datalen;
    WAVEHDR  hdr;
} oblk_t;

static HWAVEIN       Input_WAVHandle;
static HWAVEOUT      Output_WAVHandle;
static size_t        BufferBytes;
static WAVEHDR       whi    [NBLK];
static char*         data   [NBLK];
static oblk_t        array  [NBLK];
static unsigned int  NextInputIndex;
static unsigned int  NextOutputIndex;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int
init_in ( const int  SampleCount,
          const int  SampleFreq,
          const int  Channels,
          const int  BitsPerSample )
{

    WAVEFORMATEX  pwf;
    MMRESULT      r;
    int           i;

    pwf.wFormatTag      = WAVE_FORMAT_PCM;
    pwf.nChannels       = Channels;
    pwf.nSamplesPerSec  = SampleFreq;
    pwf.nAvgBytesPerSec = SampleFreq * Channels * ((BitsPerSample + 7) / 8);
    pwf.nBlockAlign     = Channels * ((BitsPerSample + 7) / 8);
    pwf.wBitsPerSample  = BitsPerSample;
    pwf.cbSize          = 0;

    r = waveInOpen ( &Input_WAVHandle, WAVE_MAPPER, &pwf, 0, 0, CALLBACK_EVENT );
    if ( r != MMSYSERR_NOERROR ) {
        fprintf ( stderr, "waveInOpen failed: ");
        switch (r) {
        case MMSYSERR_ALLOCATED:   fprintf ( stderr, "resource already allocated\n" );                                  break;
        case MMSYSERR_INVALPARAM:  fprintf ( stderr, "invalid Params\n" );                                              break;
        case MMSYSERR_BADDEVICEID: fprintf ( stderr, "device identifier out of range\n" );                              break;
        case MMSYSERR_NODRIVER:    fprintf ( stderr, "no device driver present\n" );                                    break;
        case MMSYSERR_NOMEM:       fprintf ( stderr, "unable to allocate or lock memory\n" );                           break;
        case WAVERR_BADFORMAT:     fprintf ( stderr, "attempted to open with an unsupported waveform-audio format\n" ); break;
        case WAVERR_SYNC:          fprintf ( stderr, "device is synchronous but waveOutOpen was\n" );                   break;
        default:                   fprintf ( stderr, "unknown error code: %#X\n", r );                                  break;
        }
        return -1;
    }

    BufferBytes = SampleCount * Channels * ((BitsPerSample + 7) / 8);

    for ( i = 0; i < NBLK; i++ ) {
        whi [i].lpData         = data [i] = malloc (BufferBytes);
        whi [i].dwBufferLength = BufferBytes;
        whi [i].dwFlags        = 0;
        whi [i].dwLoops        = 0;

        r = waveInPrepareHeader ( Input_WAVHandle, whi + i, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveInPrepareHeader  (%u) failed\n", i );  return -1; }
        r = waveInAddBuffer     ( Input_WAVHandle, whi + i, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveInAddBuffer      (%u) failed\n", i );  return -1; }
    }
    NextInputIndex = 0;
    waveInStart (Input_WAVHandle);
    return 0;
}


size_t
get_in ( void* DataPtr )
{
    MMRESULT  r;
    size_t    Bytes;

    if ( whi [NextInputIndex].dwFlags & WHDR_DONE ) {
        Bytes = whi [NextInputIndex].dwBytesRecorded;
        memcpy ( DataPtr, data [NextInputIndex], Bytes );

        r = waveInUnprepareHeader ( Input_WAVHandle, whi + NextInputIndex, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveInUnprepareHeader (%d) failed\n", NextInputIndex ); return -1; }
        whi [NextInputIndex].lpData         = data [NextInputIndex];
        whi [NextInputIndex].dwBufferLength = BufferBytes;
        whi [NextInputIndex].dwFlags        = 0;
        whi [NextInputIndex].dwLoops        = 0;
        r = waveInPrepareHeader   ( Input_WAVHandle, whi + NextInputIndex, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveInPrepareHeader   (%d) failed\n", NextInputIndex ); return -1; }
        r = waveInAddBuffer       ( Input_WAVHandle, whi + NextInputIndex, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveInAddBuffer       (%d) failed\n", NextInputIndex ); return -1; }
        NextInputIndex = (NextInputIndex + 1) % NBLK;
        return  Bytes;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int
init_out ( const int  SampleCount,
           const int  SampleFreq,
           const int  Channels,
           const int  BitsPerSample )
{
    WAVEFORMATEX  pwf;
    MMRESULT      r;
    int           i;

    pwf.wFormatTag      = WAVE_FORMAT_PCM;
    pwf.nChannels       = Channels;
    pwf.nSamplesPerSec  = SampleFreq;
    pwf.nAvgBytesPerSec = SampleFreq * Channels * ((BitsPerSample + 7) / 8);
    pwf.nBlockAlign     = Channels * ((BitsPerSample + 7) / 8);
    pwf.wBitsPerSample  = BitsPerSample;
    pwf.cbSize          = 0;

    r = waveOutOpen ( &Output_WAVHandle, WAVE_MAPPER, &pwf, 0, 0, CALLBACK_EVENT );
    if ( r != MMSYSERR_NOERROR ) {
        fprintf ( stderr, "waveOutOpen failed\n" );
        switch (r) {
        case MMSYSERR_ALLOCATED:   fprintf ( stderr, "resource already allocated\n" );                                  break;
        case MMSYSERR_INVALPARAM:  fprintf ( stderr, "invalid Params\n" );                                              break;
        case MMSYSERR_BADDEVICEID: fprintf ( stderr, "device identifier out of range\n" );                              break;
        case MMSYSERR_NODRIVER:    fprintf ( stderr, "no device driver present\n" );                                    break;
        case MMSYSERR_NOMEM:       fprintf ( stderr, "unable to allocate or lock memory\n" );                           break;
        case WAVERR_BADFORMAT:     fprintf ( stderr, "attempted to open with an unsupported waveform-audio format\n" ); break;
        case WAVERR_SYNC:          fprintf ( stderr, "device is synchronous but waveOutOpen was\n" );                   break;
        default:                   fprintf ( stderr, "unknown error code: %#X\n", r );                                  break;
        }
        return -1;
    }

    BufferBytes = SampleCount * Channels * ((BitsPerSample + 7) / 8);

    for ( i = 0; i < NBLK; i++ ) {
        array [i].active = 0;
        array [i].data   = malloc (BufferBytes);
    }
    NextOutputIndex = 0;
    return 0;
}


int
put_out ( const void*   DataPtr,
          const size_t  Bytes )
{
    MMRESULT  r;
    int       i = NextOutputIndex;

    if ( array [i].active )
        while ( ! (array [i].hdr.dwFlags & WHDR_DONE) )
            Sleep (26);

    r = waveOutUnprepareHeader ( Output_WAVHandle, &(array [i].hdr), sizeof (array [i].hdr) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveOutUnprepareHeader (%d) failed\n", i ); return -1; }

    array [i].active             = 1;
    array [i].hdr.lpData         = array [i].data;
    array [i].hdr.dwBufferLength = Bytes;
    array [i].hdr.dwFlags        = 0;
    array [i].hdr.dwLoops        = 0;
    memcpy ( array [i].data, DataPtr, Bytes );

    r = waveOutPrepareHeader   ( Output_WAVHandle, &(array [i].hdr), sizeof (array [i].hdr) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveOutPrepareHeader   (%d) failed\n", i ); return -1; }
    r = waveOutWrite           ( Output_WAVHandle, &(array [i].hdr), sizeof (array [i].hdr) ); if ( r != MMSYSERR_NOERROR ) { fprintf ( stderr, "waveOutAddBuffer       (%d) failed\n", i ); return -1; }

    NextInputIndex = (NextInputIndex + 1) % NBLK;
    return Bytes;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif

/* end of wave_in.c */
