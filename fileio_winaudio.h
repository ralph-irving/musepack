
#define MAX_BUFFERS_WAVOUT    40
#define MAX_BUFFERS_WAVIN    400


static CRITICAL_SECTION  cs;
static bool              cs_init                = false;
static HWAVEOUT          dev                    = NULL;
static int               ScheduledBlocks        = 0;
static int               PlayedWaveHeadersCount = 0;          // free index
static WAVEHDR*          PlayedWaveHeaders [MAX_BUFFERS_WAVOUT];

/* Das ganze sollte auf einen festen Memorypool umgestellt werden, der nicht dauernd alloziert und dealloziert wird */

static int
Box ( const char* msg )
{
    MessageBox ( NULL, msg, "Error Message . . .", MB_OK | MB_ICONEXCLAMATION );
    return -1;
}

static int
PrintError ( const char* funcname, MMRESULT res )
{
    char  msg [256];

    switch ( res ) {
    case MMSYSERR_ALLOCATED:   sprintf ( msg, "%s failed:\nresource already allocated (device already open)\n"           , funcname );      break;
    case MMSYSERR_INVALPARAM:  sprintf ( msg, "%s failed:\ninvalid Params\n"                                             , funcname );      break;
    case MMSYSERR_BADDEVICEID: sprintf ( msg, "%s failed:\ndevice identifier out of range\n"                             , funcname );      break;
    case MMSYSERR_NODRIVER:    sprintf ( msg, "%s failed:\nno device driver present (no audio driver)\n"                 , funcname );      break;
    case MMSYSERR_NOMEM:       sprintf ( msg, "%s failed:\nunable to allocate or lock memory (for audio)\n"              , funcname );      break;
    case WAVERR_BADFORMAT:     sprintf ( msg, "%s failed:\nattempted to open with an unsupported waveform-audio format\n", funcname );      break;
    case WAVERR_SYNC:          sprintf ( msg, "%s failed:\ndevice is synchronous but waveOutOpen was\n"                  , funcname );      break;
    default:                   sprintf ( msg, "%s failed:\nunknown error code: 0x%02X\n"                                 , funcname, res ); break;
    case MMSYSERR_NOERROR:     return 0;
    }

    return Box ( msg );
}


/*
 *  This function registers already played WAVE chunks. Freeing is done by wave_free_memory (),
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
wave_free_memory  ( void )
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


static int
WindowsAudio_SetupInitOutput ( unsigned int  deviceno,
                               long double   SampleFreq,
                               unsigned int  BitsPerSample,
                               unsigned int  Channels )
{
    WAVEFORMATEX  outFormat;
    UINT          deviceID; // = WAVE_MAPPER;
    UINT          devices;
    MMRESULT      r;

    if ( (devices = waveOutGetNumDevs ()) == 0 )
        return Box ( "No audio device present." );

    deviceID = deviceno < devices  ?  deviceno  :  WAVE_MAPPER;

    outFormat.wFormatTag      = WAVE_FORMAT_PCM;
    outFormat.wBitsPerSample  = BitsPerSample;
    outFormat.nChannels       = Channels;
    outFormat.nSamplesPerSec  = (unsigned long)(SampleFreq + 0.5);
    outFormat.nBlockAlign     = (outFormat.wBitsPerSample + 7) / 8 * outFormat.nChannels;
    outFormat.nAvgBytesPerSec = outFormat.nSamplesPerSec * outFormat.nBlockAlign;
    outFormat.cbSize          = 0;

    r = waveOutOpen ( &dev, deviceID, &outFormat, (DWORD)wave_callback, 0, CALLBACK_FUNCTION );
    if ( r != MMSYSERR_NOERROR )
        return PrintError ( "waveOutOpen", r );

    waveOutReset ( dev );
    InitializeCriticalSection ( &cs );
    cs_init = true;

    // SetPriority ( 99 );
    return 0;
}


static size_t
WindowsAudio_play ( const void* data, size_t len )
{
    HGLOBAL    hg;
    HGLOBAL    hg2;
    LPWAVEHDR  wh;
    void*      allocptr;

    do {
        while ( PlayedWaveHeadersCount > 0 )                        // free used blocks ...
            wave_free_memory  ();

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


static int
WindowsAudio_close ( void )
{
    if ( dev != NULL ) {

        while ( ScheduledBlocks > 0 ) {
            Sleep (ScheduledBlocks);
            while ( PlayedWaveHeadersCount > 0 )                        // free used blocks ...
                wave_free_memory  ();
        }

        waveOutReset (dev);      // reset the device
        waveOutClose (dev);      // close the device
        dev = NULL;
    }

    if ( cs_init == true ) {
        cs_init = false;
        DeleteCriticalSection ( &cs );
    }

    ScheduledBlocks = 0;
    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////

/*
This code is initially from a different source and works a little bit different.
It contains recording and playback code. In the final version record and playback should work
very similar, so a little bit more work must be done.

Strategies I prefer:
  - play:     playback of all data
  - record:   program works until the whole request buffer is filled, no more and not less
*/


typedef struct {
    int      active;
    char*    data;
    size_t   datalen;
    WAVEHDR  hdr;
} oblk_t;


static HWAVEIN       Input_WAVHandle;
static HWAVEOUT      Output_WAVHandle;
static size_t        BufferBytes;
static WAVEHDR       whi    [MAX_BUFFERS_WAVIN];
static char*         data   [MAX_BUFFERS_WAVIN];
static oblk_t        array  [MAX_BUFFERS_WAVIN];
static unsigned int  NextInputIndex;
static unsigned int  NextOutputIndex;


//////////////////////////////////////////////////////////////////////////////////////////////////////

static int
WindowsAudio_SetupInitInput ( unsigned int  deviceno,
                              long double   SampleFreq,
                              unsigned int  BitsPerSample,
                              unsigned int  Channels )
{
    WAVEFORMATEX  inFormat;
    UINT          deviceID;
    UINT          devices;
    MMRESULT      r;
    UINT          i;

    if ( (devices = waveInGetNumDevs ()) == 0 )
        return Box ( "No audio device present." );

    deviceID = deviceno < devices  ?  deviceno  :  WAVE_MAPPER;

    inFormat.wFormatTag      = WAVE_FORMAT_PCM;
    inFormat.wBitsPerSample  = BitsPerSample;
    inFormat.nChannels       = Channels;
    inFormat.nSamplesPerSec  = (unsigned long)(SampleFreq + 0.5);
    inFormat.nBlockAlign     = (inFormat.wBitsPerSample + 7) / 8 * inFormat.nChannels;
    inFormat.nAvgBytesPerSec = inFormat.nSamplesPerSec * inFormat.nBlockAlign;
    inFormat.cbSize          = 0;

    r = waveInOpen ( &Input_WAVHandle, deviceID, &inFormat, 0, 0, CALLBACK_EVENT );
    if ( r != MMSYSERR_NOERROR )
        return PrintError ( "waveInOpen", r );

    BufferBytes = 44100 /*SampleCount*/ * Channels * ((BitsPerSample + 7) / 8);

    for ( i = 0; i < MAX_BUFFERS_WAVIN; i++ ) {
        whi [i].lpData         = data [i] = malloc (BufferBytes);
        whi [i].dwBufferLength = BufferBytes;
        whi [i].dwFlags        = 0;
        whi [i].dwLoops        = 0;

        r = waveInPrepareHeader ( Input_WAVHandle, whi + i, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveInPrepareHeader", r );
        r = waveInAddBuffer     ( Input_WAVHandle, whi + i, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveInAddBuffer"    , r );
    }
    NextInputIndex = 0;
    waveInStart (Input_WAVHandle);
    return 0;
}


static size_t
WindowsAudio_record ( void* DataPtr, size_t Bytes_to_Write )        // !!!!!!!!!!!!!!!!!!!!! Bytes_to_Write currently unused, danger !!!!!!!!!!!!!
{
    MMRESULT  r;
    size_t    Bytes;

    if ( whi [NextInputIndex].dwFlags & WHDR_DONE ) {
        Bytes = whi [NextInputIndex].dwBytesRecorded;
        memcpy ( DataPtr, data [NextInputIndex], Bytes );

        r = waveInUnprepareHeader ( Input_WAVHandle, whi + NextInputIndex, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveInUnprepareHeader", r );
        whi [NextInputIndex].lpData         = data [NextInputIndex];
        whi [NextInputIndex].dwBufferLength = BufferBytes;
        whi [NextInputIndex].dwFlags        = 0;
        whi [NextInputIndex].dwLoops        = 0;
        r = waveInPrepareHeader   ( Input_WAVHandle, whi + NextInputIndex, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveInPrepareHeader", r );
        r = waveInAddBuffer       ( Input_WAVHandle, whi + NextInputIndex, sizeof (*whi) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveInAddBuffer"    , r );
        NextInputIndex = (NextInputIndex + 1) % MAX_BUFFERS_WAVIN;
        return  Bytes;
    }

    return 0;
}

static int
WindowsAudio_closeInput ( void )
{
    waveInReset ( Input_WAVHandle );
    waveInClose ( Input_WAVHandle );
    Input_WAVHandle = NULL;

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
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
    if ( r != MMSYSERR_NOERROR )
        return PrintError ( "waveOutOpen", r );

    BufferBytes = SampleCount * Channels * ((BitsPerSample + 7) / 8);

    for ( i = 0; i < MAX_BUFFERS_WAVIN; i++ ) {
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

    r = waveOutUnprepareHeader ( Output_WAVHandle, &(array [i].hdr), sizeof (array [i].hdr) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveOutUnprepareHeader", r );

    array [i].active             = 1;
    array [i].hdr.lpData         = array [i].data;
    array [i].hdr.dwBufferLength = Bytes;
    array [i].hdr.dwFlags        = 0;
    array [i].hdr.dwLoops        = 0;
    memcpy ( array [i].data, DataPtr, Bytes );

    r = waveOutPrepareHeader   ( Output_WAVHandle, &(array [i].hdr), sizeof (array [i].hdr) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveOutPrepareHeader", r );
    r = waveOutWrite           ( Output_WAVHandle, &(array [i].hdr), sizeof (array [i].hdr) ); if ( r != MMSYSERR_NOERROR ) return PrintError ( "waveOutWrite"        , r );

    NextInputIndex = (NextInputIndex + 1) % MAX_BUFFERS_WAVIN;
    return Bytes;
}
*/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* end of fileio_winaudio.h */
