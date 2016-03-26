/*
 *  File IO function
 *
 *  (C) Frank Klemm, Janne Hyvärinen 2002. All rights reserved.
 *
 *  Principles:
 *    This module tries to encapsulate all IO stuff behind a common interface.
 *    Basicly it should start for regular file IO. Later it should be extended
 *    to pipes, inter-process communication, sockets and hardware audio devices.
 *    Although written in C the idea is taken from C++.
 *    Constructor functions (IO_Open*) create a structure containing all helper
 *    information (HANDLE, attributes, ...) and pointer to functions.
 *    These functions are called by
 *
 *        p -> function ( p, ... );
 *
 *    Returning a -1 indicates an error, other values indicates normal operation.
 *    It was tried to always return useful information.
 *
 *  History:
 *    2002-08-23    created
 *    2002-11-07    /dev/null support added
 *    2002-11-09    AudioIO started
 *
 *  Global functions:
 *    -
 *
 *  TODO:
 *    - which functions are still needed (Tell, ...)
 *    - missing sockets and audio devices
 *    - Coding for POSIX, ANSI, Turbo-C
 *
 *  REMARKS:
 *    - when this works, mppdec(Win32) can be simplified by using this code for PCM data output
 *    - when this works, mppenc(Win32) can be simplified by using this code for Musepack Bitstream output
 *    - for usage for Linux/BSD/MacOS/... the POSIX I/O must be programmed (priority 1)
 *    - for usage for Linux mppdec the audio file output must be programmed (priority 2)
 *    - for usage as input the HTTP stuff is still missing (priority 3)
 *    - for usage for mppenc the audio file input must be programmed (priority 3)
 *    - for usage for mppenc the pipe support must be added (priority 3)
 *
 *  REMARKS_2:
 *    - May be we don't need a virtual -> FullInit () functions ???
 *    - static prototypes: nice to read
 *                         eases porting because you don't need to write all functions at once
 *    - TODO: Moving Windows independen stuff befor the #if defined _WIN32
 *    - Why starting implementation with Windows?
 *      - can't test it at home
 *      - Audio device is very different from file output, which is not the case in Linux
 *    - should be compilable with C++ and Objective C
 *    - Some AudioIO function (not using the AudioIO were associated with IO (use WinDiff)
 *
 * REMARK_3:
 *    - If there's a problem which you can't solve, explain it with some more words so I understand it
 *      immediately.
 */

#include "fileio.h"
#include "fileio_endian.h"
#include "fileio_header.h"

static size_t   Calc_RIFFHeader         ( const AudioIO* _this, unsigned char* buffer );
static size_t   Calc_AIFFHeader         ( const AudioIO* _this, unsigned char* buffer );
static pcm_filetype_t
                Calc_InputFileFormat    ( const char* filename );
static size_t   IO_ReadFile             ( IO* _this, void*       dst, size_t bytes );
static size_t   IO_WriteFile            ( IO* _this, const void* src, size_t bytes );
static int      IO_CloseFile            ( IO* _this );
static offset_t IO_GetSize              ( IO* _this );
static offset_t IO_SeekFile             ( IO* _this, offset_t newpos, seek_method_t whence );
static offset_t IO_Tell                 ( IO* _this );
static offset_t IO_SetSize              ( IO* _this, offset_t newsize );
static bool     IO_IsAtEOF              ( IO* _this );
static bool     IO_IsATTYInit           ( HANDLE handle );
static bool     IO_IsSeekableInit       ( HANDLE handle );
static IO*      IO_Init                 ( HANDLE handle, uint16_t attr );
static size_t   IO_ReadDummy            ( IO* _this, void* dst, size_t bytes );
static size_t   IO_WriteDummy           ( IO* _this, const void* src, size_t bytes );
static int      IO_CloseDummy           ( IO* _this );
static offset_t IO_SetSizeDummy         ( IO* _this, offset_t newsize );
static offset_t IO_GetSizeDummy         ( IO* _this );
static offset_t IO_SeekFileDummy        ( IO* _this, offset_t pos, seek_method_t whence );
static offset_t IO_TellDummy            ( IO* _this );
static bool     IO_IsAtEOFDummy         ( IO* _this );
static bool     IO_SetAsync             ( IO* _this, bool async );
static bool     AudioIO_AllParametersSet( const AudioIO* _this );
static int      AudioIO_InitOutput      ( AudioIO* _this );
static int      AudioIO_InitInput       ( AudioIO* _this );
static int      AudioIO_UnInitOutput    ( AudioIO* _this );
static offset_t AudioIO_Seek            ( AudioIO* _this, offset_t samplepos, seek_method_t whence );
static offset_t AudioIO_Tell            ( AudioIO* _this );
static size_t   AudioIO_Read            ( AudioIO* _this, void*       dst, size_t items );
static size_t   AudioIO_Write           ( AudioIO* _this, const void* src, size_t items );
static int      AudioIO_Close           ( AudioIO* _this );
static bool     AudioIO_SetFloatingPoint( AudioIO* _this, bool flag );

// These functions are actually working in the IO arena, so I renamed it and use IO* _this
// Is this okay?
static size_t   IO_ReadAudioDevice      ( IO*  _this, void*       dst, size_t bytes );
static size_t   IO_WriteAudioDevice     ( IO*  _this, const void* src, size_t bytes );
static int      IO_CloseAudioDevice     ( IO*  _this );


// attribute flags used in IO.attr

#define IO_FLAG_READABLE      0x0001    // IO stream is readable
#define IO_FLAG_WRITABLE      0x0002    // IO stream is writable
#define IO_FLAG_SEEKABLE      0x0004    // IO stream is seekable
#define IO_FLAG_ISATTY        0x0008    // IO stream is a teletype
#define IO_FLAG_AT_EOF        0x0010    // IO stream is at its end
#define IO_FLAG_REWRITABLE    0x0020    // IO stream can be reopened without the loss of old data

/************************************************************************/
/************************************************************************/
/****                                                                ****/
/****                 Non-static portable functions                  ****/
/****                                                                ****/
/************************************************************************/
/************************************************************************/


bool
IO_IsReadable ( const IO* _this )
{
    return _this -> attr & IO_FLAG_READABLE;
}

bool
IO_IsWritable ( const IO* _this )
{
    return _this -> attr & IO_FLAG_WRITABLE;
}

bool
IO_IsSeekable ( const IO* _this )
{
    return _this -> attr & IO_FLAG_SEEKABLE;
}

bool
IO_IsATTY ( const IO* _this )
{
    return _this -> attr & IO_FLAG_ISATTY;
}

bool
AudioIO_IsReadable ( const AudioIO* _this )
{
    return _this -> File -> attr & IO_FLAG_READABLE;
}

bool
AudioIO_IsWritable ( const AudioIO* _this )
{
    return _this -> File -> attr & IO_FLAG_WRITABLE;
}

bool
AudioIO_IsSeekable ( const AudioIO* _this )
{
    return _this -> File -> attr & IO_FLAG_SEEKABLE;
}

unsigned int
AudioIO_GetBitsPerSample      ( const AudioIO* _this )
{
    return _this -> BitsPerSample;
}

unsigned int
AudioIO_GetBytesPerSample     ( const AudioIO* _this )
{
    return _this -> BytesPerSample;
}

unsigned int
AudioIO_GetBytesPerSampleTime ( const AudioIO* _this )
{
    return _this -> BytesPerSampleTime;
}

unsigned int
AudioIO_GetChannelCount ( const AudioIO* _this )
{
    return _this -> ChannelCount;
}

long double
AudioIO_GetSampleFreq ( const AudioIO* _this )
{
    return _this -> SampleFreq;
}



/************************************************************************/
/************************************************************************/
/****                                                                ****/
/****                   static portable functions                    ****/
/****                                                                ****/
/************************************************************************/
/************************************************************************/

static bool
IO_IsAtEOF ( IO* _this )
{
    return _this -> attr & IO_FLAG_AT_EOF  ?  true  :  false;
}


static size_t
IO_ReadDummy ( IO* _this, void* dst, size_t bytes )
{
    memset ( dst, 0, bytes );
    _this -> offset += bytes;
    return bytes;
}


static size_t
IO_WriteDummy ( IO* _this, const void* src, size_t bytes )
{
    (void) src;
    _this -> offset += bytes;
    return bytes;
}


static int
IO_CloseDummy ( IO* _this )
{
    free ( _this );
    return 0;
}


static offset_t
IO_SetSizeDummy ( IO* _this, offset_t newsize )
{
    return _this -> offset = newsize;
}


static offset_t
IO_GetSizeDummy ( IO* _this )
{
    return _this -> offset;
}


static offset_t
IO_SeekFileDummy ( IO* _this, offset_t pos, seek_method_t whence )
{
    switch ( whence ) {
    case Seek_Set:
        _this -> offset  = pos;
        break;
    case Seek_Cur:
    case Seek_End:
        _this -> offset += pos;
        break;
    }

    return _this -> offset;
}


static offset_t
IO_TellDummy ( IO* _this )
{
    return _this -> offset;
}


static bool
IO_IsAtEOFDummy ( IO* _this )
{
    (void) _this;
    return 1;
}


IO*
IO_OpenNull ( void )
{
    IO*  ret;

    ret = (IO*) calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;

    ret -> handle  = NULL;
    ret -> attr    = IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_SEEKABLE;
    ret -> Read    = IO_ReadDummy;
    ret -> Write   = IO_WriteDummy;
    ret -> Close   = IO_CloseDummy;
    ret -> GetSize = IO_GetSizeDummy;
    ret -> SetSize = IO_SetSizeDummy;
    ret -> Seek    = IO_SeekFileDummy;
    ret -> IsAtEOF = IO_IsAtEOFDummy;

    return ret;
}

/*
 *  This creates an alien IO structure with foreign Read, Write, Seek and Close function.
 *  Can be used in the same way as normal file based IO with user defined functions.
 */

IO*
IO_OpenExternal  ( size_t   (*ReadFn ) ( IO* _this, void*       dst, size_t bytes ),
                   size_t   (*WriteFn) ( IO* _this, const void* src, size_t bytes ),
                   offset_t (*SeekFn ) ( IO* _this, offset_t pos, seek_method_t whence ),
                   int      (*CloseFn) ( IO* _this ) )
{
    IO*  ret;

    ret = (IO*) calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;

    ret -> handle  = NULL;
    ret -> attr    = ( ReadFn  != NULL  ?  IO_FLAG_READABLE  :  0 ) |
                     ( WriteFn != NULL  ?  IO_FLAG_WRITABLE  :  0 ) |
                     ( SeekFn  != NULL  ?  IO_FLAG_SEEKABLE  :  0 );
    ret -> Read    = ReadFn;
    ret -> Write   = WriteFn;
    ret -> Seek    = SeekFn;
    ret -> Close   = CloseFn;

    return ret;
}


static IO*
IO_Init ( HANDLE handle, uint16_t attr )
{
    IO*  ret;

    if ( handle == INVALID_HANDLE_VALUE )
        return NULL;

    ret = (IO*) calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;

    ret -> handle  = handle;
    ret -> attr    = attr
                     | ( IO_IsSeekableInit (handle)  ?  IO_FLAG_SEEKABLE  :  0 )
                     | ( IO_IsATTYInit     (handle)  ?  IO_FLAG_ISATTY    :  0 );
    ret -> Read    = IO_ReadFile;
    ret -> Write   = IO_WriteFile;
    ret -> Close   = IO_CloseFile;
    ret -> GetSize = IO_GetSize;
    ret -> SetSize = IO_SetSize;
    ret -> Seek    = IO_SeekFile;
    ret -> IsAtEOF = IO_IsAtEOF;

    return ret;
}


static pcm_filetype_t
Calc_InputFileFormat    ( const char* filename )
{
    const char*  ext = strrchr ( filename, '.' );

    if ( ext == NULL )
        return Filetype_Unknown;
    if ( 0 == stricmp ( ext, ".raw" ) )
        return Filetype_RAW;
    if ( 0 == stricmp ( ext, ".wav" ) )
        return Filetype_WAV;
    if ( 0 == stricmp ( ext, ".ofr" ) )
        return Filetype_OPTIMFROG;
    if ( 0 == stricmp ( ext, ".aif" )  ||  0 == stricmp ( ext, ".aiff" ) )
        return Filetype_AIFF;
    if ( 0 == stricmp ( ext, ".ape" )  ||  0 == stricmp ( ext, ".mac"  ) )
        return Filetype_APE;
    if ( 0 == stricmp ( ext, ".fla" )  ||  0 == stricmp ( ext, ".flac" ) )
        return Filetype_FLAC;
    if ( 0 == stricmp ( ext, ".pac" )  ||  0 == stricmp ( ext, ".lpac" ) )
        return Filetype_LPAC;
    if ( 0 == stricmp ( ext, ".wv" ) )
        return Filetype_WAVPACK;
    if ( 0 == stricmp ( ext, ".la" ) )
        return Filetype_LA;

    return Filetype_Unknown;
}



#if defined _WIN32

/************************************************************************/
/************************************************************************/
/****                                                                ****/
/****                   Code for 32 bit Windows API                  ****/
/****                                                                ****/
/************************************************************************/
/************************************************************************/


#include "fileio_winaudio.h"

#include <sys/stat.h>

#ifndef S_ISDIR
# if   defined S_IFDIR
#  define S_ISDIR(x)            ((x) &   S_IFDIR)
# elif defined _S_IFDIR
#  define S_ISDIR(x)            ((x) &  _S_IFDIR)
# elif defined __S_IFDIR
#  define S_ISDIR(x)            ((x) & __S_IFDIR)
# else
#  error Cannot find a way to test for a directory
# endif
#endif


static bool
IO_IsDirectory ( const char* path )
{
    struct _stat  st;

    if ( _stat ( path, &st ) != 0 )
        return false;

    return S_ISDIR ( st.st_mode )  ?  true  :  false;
}



// Read data from an object

static size_t
IO_ReadFile ( IO* _this, void* dst, size_t bytes )
{
    DWORD  result;

    if ( bytes == 0 )                   // there often trouble writing 0 bytes. Instead of writing 0 bytes, often an error is signaled
        return 0;

    if ( 0 == ReadFile ( _this -> handle, dst, bytes, &result, NULL ) ) {
        if ( GetLastError () == ERROR_HANDLE_EOF )
            _this -> attr |= IO_FLAG_AT_EOF;
        return -1;
    }

    if ( result == 0 )
        _this -> attr |= IO_FLAG_AT_EOF;

    _this -> offset += result;
    return result;
}

// Write data to an object

static size_t
IO_WriteFile ( IO* _this, const void* src, size_t bytes )
{
    DWORD  result;

    if ( bytes == 0 )
        return 0;

    // current encoder has here some penetration mode which repeats this until all bytes are actually be written
    if ( 0 == WriteFile ( _this -> handle, src, bytes, &result, NULL ) )
        return -1;
    _this -> offset += result;
    return result;
}


static int
IO_CloseFile ( IO* _this )
{
    int  ret;

    ret = CloseHandle ( _this -> handle );
    free ( _this );
    return ret  ?  0  :  -1;
}


static offset_t
IO_GetSize ( IO* _this )
{
    DWORD  ret0;
    DWORD  ret1;

    if ( !IO_IsSeekable (_this) )
        return -1;

    ret0 = GetFileSize ( _this -> handle, &ret1 );
    if ( ret0 == 0xFFFFFFFF  &&  GetLastError () != NO_ERROR )
        return -1;

    return _this -> offset = ( (__int64) ret1 << 32 ) | ret0;
}


static offset_t
IO_SeekFile ( IO* _this, offset_t newpos, seek_method_t whence )
{
    DWORD  low;
    LONG   high;

    if ( !IO_IsSeekable (_this) )
        return -1;

    high = (LONG) ( newpos >> 32 );
    low  = SetFilePointer ( _this -> handle, (ULONG) newpos, &high, whence );
    if ( low == 0xFFFFFFFF  &&  GetLastError () != NO_ERROR )
        return -1;

    _this -> attr &= ~IO_FLAG_AT_EOF;
    return ( (__int64) high << 32 ) | low;
}


static offset_t
IO_Tell ( IO* _this )
{
#if 0
    DWORD  low;
    LONG   high = 0;

    low = SetFilePointer ( _this -> handle, 0, &high, FILE_CURRENT );
    if ( low == 0xFFFFFFFF  &&  GetLastError () != NO_ERROR )
        return -1;

    return ( (__int64) high << 32 ) | low;
#else
    return _this -> offset;
#endif
}


static offset_t
IO_SetSize ( IO* _this, offset_t newsize )
{
    int  ret;

    if ( !IO_IsSeekable (_this) )
        return -1;

    if ( !IO_IsWritable (_this) )
        return -1;

    if ( IO_SeekFile ( _this, newsize, Seek_Set ) != newsize )
        return -1;

    ret = SetEndOfFile ( _this -> handle );
    if ( ret == 0 )
        return -1;

    _this -> attr &= ~IO_FLAG_AT_EOF;
    return newsize;
}


static bool
IO_IsSeekableInit ( HANDLE handle )
{
    DWORD  result;

    result = GetFileType ( handle );

    switch ( result ) {
    case FILE_TYPE_DISK:
        return true;
    case FILE_TYPE_UNKNOWN:
    case FILE_TYPE_CHAR:
    case FILE_TYPE_PIPE:
    default:
        return false;
    }
}


static bool
IO_IsATTYInit ( HANDLE handle )
{
    DWORD  result;

    result = GetFileType ( handle );

    switch ( result ) {
    case FILE_TYPE_CHAR:
        return true;
    case FILE_TYPE_DISK:
    case FILE_TYPE_UNKNOWN:
    case FILE_TYPE_PIPE:
    default:
        return false;
    }
}


IO*
IO_OpenFileRead ( const char* filename )
{
    HANDLE   handle;
    IO*  ret;

    handle = CreateFile (
             filename,                              // lpFileName
             GENERIC_READ,                          // dwDesiredAccess: 0, GENERIC_READ, GENERIC_WRITE
             FILE_SHARE_READ,                       // dwShareMode: FILE_SHARE_DELETE, FILE_SHARE_READ, FILE_SHARE_WRITE
             NULL,                                  // lpSecurityAttributes
             OPEN_EXISTING,                         // dwCreationDisposition: CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING, OPEN_ALWAYS, TRUNCATE_EXISTING
             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                                    // dwFlagsAndAttributes: FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_TEMPORARY
                                                    // FILE_FLAG_WRITE_THROUGH, FILE_FLAG_OVERLAPPED, FILE_FLAG_NO_BUFFERING, FILE_FLAG_RANDOM_ACCESS, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_DELETE_ON_CLOSE, FILE_FLAG_BACKUP_SEMANTICS, FILE_FLAG_POSIX_SEMANTICS, FILE_FLAG_OPEN_REPARSE_POINT, FILE_FLAG_OPEN_NO_RECALL
                                                    // SECURITY_ANONYMOUS, SECURITY_IDENTIFICATION, SECURITY_IMPERSONATION, SECURITY_DELEGATION, SECURITY_CONTEXT_TRACKING, SECURITY_EFFECTIVE_ONLY
             NULL );                                // hTemplateFile

    if ( ( ret = IO_Init ( handle, IO_FLAG_READABLE ) ) == NULL )
        return NULL;

    ret -> Write   = NULL;
    return ret;
}


IO*
IO_OpenFileWrite ( const char* filename )
{
    HANDLE   handle;
    IO*  ret;

    handle = CreateFile (
             filename,                              // lpFileName
             GENERIC_WRITE,                         // dwDesiredAccess: 0, GENERIC_READ, GENERIC_WRITE
             FILE_SHARE_READ,                       // dwShareMode: FILE_SHARE_DELETE, FILE_SHARE_READ, FILE_SHARE_WRITE
             NULL,                                  // lpSecurityAttributes
             CREATE_ALWAYS,                         // dwCreationDisposition: CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING, OPEN_ALWAYS, TRUNCATE_EXISTING
             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                                    // dwFlagsAndAttributes: FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_TEMPORARY
                                                    // FILE_FLAG_WRITE_THROUGH, FILE_FLAG_OVERLAPPED, FILE_FLAG_NO_BUFFERING, FILE_FLAG_RANDOM_ACCESS, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_DELETE_ON_CLOSE, FILE_FLAG_BACKUP_SEMANTICS, FILE_FLAG_POSIX_SEMANTICS, FILE_FLAG_OPEN_REPARSE_POINT, FILE_FLAG_OPEN_NO_RECALL
                                                    // SECURITY_ANONYMOUS, SECURITY_IDENTIFICATION, SECURITY_IMPERSONATION, SECURITY_DELEGATION, SECURITY_CONTEXT_TRACKING, SECURITY_EFFECTIVE_ONLY
             NULL );                                // hTemplateFile

    if ( ( ret = IO_Init ( handle, IO_FLAG_WRITABLE ) ) == NULL )
        return NULL;

    ret -> Read    = NULL;
    return ret;
}


IO*
IO_OpenFile ( const char* filename )
{
    HANDLE   handle;
    IO*  ret;

    handle = CreateFile (
             filename,                              // lpFileName
             GENERIC_READ | GENERIC_WRITE,          // dwDesiredAccess: 0, GENERIC_READ, GENERIC_WRITE
             FILE_SHARE_READ,                       // dwShareMode: FILE_SHARE_DELETE, FILE_SHARE_READ, FILE_SHARE_WRITE
             NULL,                                  // lpSecurityAttributes
             OPEN_ALWAYS,                           // dwCreationDisposition: CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING, OPEN_ALWAYS, TRUNCATE_EXISTING
             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                                    // dwFlagsAndAttributes: FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_TEMPORARY
                                                    // FILE_FLAG_WRITE_THROUGH, FILE_FLAG_OVERLAPPED, FILE_FLAG_NO_BUFFERING, FILE_FLAG_RANDOM_ACCESS, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_DELETE_ON_CLOSE, FILE_FLAG_BACKUP_SEMANTICS, FILE_FLAG_POSIX_SEMANTICS, FILE_FLAG_OPEN_REPARSE_POINT, FILE_FLAG_OPEN_NO_RECALL
                                                    // SECURITY_ANONYMOUS, SECURITY_IDENTIFICATION, SECURITY_IMPERSONATION, SECURITY_DELEGATION, SECURITY_CONTEXT_TRACKING, SECURITY_EFFECTIVE_ONLY
             NULL );                                // hTemplateFile

    if ( ( ret = IO_Init ( handle, IO_FLAG_READABLE | IO_FLAG_WRITABLE ) ) == NULL )
        return NULL;

    return ret;
}


IO*
IO_OpenStdin  ( void )
{
    HANDLE   handle;
    IO*  ret;

    handle = GetStdHandle ( STD_INPUT_HANDLE );

    if ( ( ret = IO_Init ( handle, IO_FLAG_READABLE ) ) == NULL )
        return NULL;

    ret -> Write   = NULL;
    return ret;
}


IO*
IO_OpenStdout ( void )
{
    HANDLE   handle;
    IO*  ret;

    handle = GetStdHandle ( STD_OUTPUT_HANDLE );

    if ( ( ret = IO_Init ( handle, IO_FLAG_WRITABLE ) ) == NULL )
        return NULL;

    ret -> Read    = NULL;
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////

static void
AudioIO_InitValues ( AudioIO* _this )
{
    // This can be done everytime without any problems
    _this -> BytesPerSample     = (_this -> BitsPerSample + 7) / 8;
    _this -> BytesPerSampleTime =  _this -> BytesPerSample * _this -> ChannelCount;
}


long double
AudioIO_SetSampleFrequency ( AudioIO* _this, long double samplefreq )
{
    if ( _this -> SampleFreq != samplefreq ) {
        if ( _this -> FullInitDone == true )
            if ( AudioIO_UnInitOutput (_this) )
                return -1.;

        _this -> SampleFreq = samplefreq;

        AudioIO_InitValues ( _this );       // actually not necessary
    }

    return _this -> SampleFreq;
}


int
AudioIO_SetChannelCount ( AudioIO* _this, unsigned int channels )
{
    if ( _this -> ChannelCount != channels ) {
        if ( _this -> FullInitDone == true )
            if ( AudioIO_UnInitOutput (_this) )
                    return -1;

        _this -> ChannelCount = channels;
        AudioIO_InitValues ( _this );
    }

    return _this -> ChannelCount;
}


int
AudioIO_SetBitsPerSample ( AudioIO* _this, unsigned int bits )
{
    if ( _this -> BitsPerSample != bits ) {
        if ( _this -> FullInitDone == true )
            if ( AudioIO_UnInitOutput (_this) )
                    return -1;

        _this -> BitsPerSample = bits;
        AudioIO_InitValues ( _this );
    }

    return _this -> BitsPerSample;
}

/*
  When AudioIO_OpenAudioGenericOut() gets a directory name as argument, this will treated in the following way:
  - Directory name is stored, but no file is opened
  - AudioIO_SetFilename() sets the full filename and opens the file
  - otherwise AudioIO_SetFilename() is a dummy function
  - this is used for things like "mppdec *.mpc dirname"
*/


const char*
AudioIO_SetFilename ( AudioIO* _this, const char* filename )
{
    char  fullname [2048];

    if ( _this -> DirectoryName == NULL )
        return filename;

    if ( _this -> FullInitDone == true ) {
        if ( IO_IsSeekable ( _this -> File ) ) {
            _this -> File -> Seek ( _this -> File, 0L, Seek_Set );
            _this -> FullInit ( _this );
        }
        _this -> File -> Close ( _this -> File );
        _this -> FullInitDone = false;
    }

    sprintf ( fullname, "%s%c%s", _this -> DirectoryName, '\\', filename );
    _this -> File = IO_OpenFileRead ( fullname);

    return filename;
}


static bool
AudioIO_AllParametersSet ( const AudioIO* _this )
{
    return _this -> SampleFreq    > 0.  &&
           _this -> ChannelCount  > 0   &&
           _this -> BitsPerSample > 0;
}


static int
AudioIO_InitOutput ( AudioIO* _this )
{
    uint8_t  HeaderBuffer [256];          // ugly

    _this -> EndianSwapper  = NULL;
    _this -> HeaderSize     = 0;

    switch ( _this -> FileType ) {
    case Filetype_RAW:
        _this -> EndianSwapper = NULL;
        // No header to write
        break;
    case Filetype_RAW_LE:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_little, _this -> BitsPerSample );
        // No header to write
        break;
    case Filetype_RAW_BE:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_big, _this -> BitsPerSample );
        // No header to write
        break;
    case Filetype_WAV:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_little, _this -> BitsPerSample );
        _this -> HeaderSize    = Calc_RIFFHeader ( _this, HeaderBuffer );
        break;
    case Filetype_AIFF:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_big, _this -> BitsPerSample );
        _this -> HeaderSize    = Calc_AIFFHeader ( _this, HeaderBuffer );
        break;
    case Filetype_WAVEout:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_little, _this -> BitsPerSample );
        // No header to write
        if ( WindowsAudio_SetupInitOutput ( _this -> DeviceNo,
                                            _this -> SampleFreq,
                                            _this -> BitsPerSample,
                                            _this -> ChannelCount ) != 0 )
            return -1;
        break;
    }

    if ( _this -> HeaderSize != 0 )
        if ( _this -> File -> Write (_this -> File, HeaderBuffer, _this -> HeaderSize) != _this -> HeaderSize )
            return -1;

    return 0;
}

static int
AudioIO_InitInput ( AudioIO* _this )
{
    _this -> EndianSwapper  = NULL;
    _this -> HeaderSize     = 0;

    switch ( _this -> FileType ) {
    case Filetype_RAW:
        _this -> EndianSwapper = NULL;
        // No header to read
        break;
    case Filetype_RAW_LE:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_little, _this -> BitsPerSample );
        // No header to read
        break;
    case Filetype_RAW_BE:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_big, _this -> BitsPerSample );
        // No header to read
        break;
    case Filetype_WAV:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_little, _this -> BitsPerSample );
        //_this -> HeaderSize    = Calc_RIFFHeader ( _this, HeaderBuffer );
        break;
    case Filetype_AIFF:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_big, _this -> BitsPerSample );
        //_this -> HeaderSize    = Calc_AIFFHeader ( _this, HeaderBuffer );
        break;
    case Filetype_WAVEin:
        _this -> EndianSwapper = Calc_EndianSwapper ( Endian_little, _this -> BitsPerSample );
        // No header to read
        if ( WindowsAudio_SetupInitInput  ( _this -> DeviceNo,
                                            _this -> SampleFreq,
                                            _this -> BitsPerSample,
                                            _this -> ChannelCount ) != 0 )
            return -1;
        break;
    }

    /*
    if ( _this -> HeaderSize != 0 )
        if ( _this -> File -> Write (_this -> File, HeaderBuffer, _this -> HeaderSize) != _this -> HeaderSize )
            return -1;
    */

    return 0;
}


static int
AudioIO_UnInitOutput ( AudioIO* _this )
{
    switch ( _this -> FileType ) {
    case Filetype_WAVEout:
        WindowsAudio_close ();
        _this -> FullInitDone = false;
        return 0;
    }

    return -1;
}


static offset_t
AudioIO_Seek ( AudioIO* _this, offset_t samplepos, seek_method_t whence )
{
    if ( !AudioIO_IsSeekable (_this) )
        return -1;

    switch ( whence ) {
    case Seek_Set:
        _this -> SamplePos  = samplepos;
        break;
    case Seek_Cur:
        _this -> SamplePos += samplepos;
        break;
    case Seek_End:
        _this -> SamplePos  = _this -> SampleCount + samplepos;
        break;
    }

    return _this -> File -> Seek ( _this -> File,
                                   _this -> SamplePos * _this -> BytesPerSampleTime  +  _this -> HeaderSize,
                                   Seek_Set );
}


static offset_t
AudioIO_Tell ( AudioIO* _this )
{
    return _this -> SamplePos;
}


static offset_t
AudioIO_GetSize ( AudioIO* _this )
{
    return _this -> SampleCount;
}


static bool
AudioIO_AllParametersAuto ( AudioIO* _this )            // very first hack for first tests
{
    uint8_t  buff [44];

    _this -> File -> Read ( _this -> File, buff, sizeof buff );
    _this -> SampleFreq    = 44100.;
    _this -> BitsPerSample =    16;
    _this -> ChannelCount  =     2;

    _this -> HeaderSize    =    44;

    return true;
}


static size_t
AudioIO_Read ( AudioIO* _this, void* dst, size_t items )
{
    size_t      bytes;
    int         bytes2;

    if ( _this -> FullInitDone == false ) {

        // ???????????????????????????????
        if ( _this -> DeviceNo == (unsigned int)-1 ) {
            if ( ! AudioIO_AllParametersAuto (_this) )              // for files
                return -1;
        }
        _this -> FullInit ( _this );                            // for audio device must be called a different functiuon, how to switch between them ???

        AudioIO_InitValues ( _this );
        _this -> FullInitDone = true;
    }

    bytes = items * _this -> BytesPerSampleTime;
    bytes = _this -> File -> Read ( _this -> File, dst, bytes );
    if ( bytes == 0 )
        return 0;

    while ( bytes % _this -> BytesPerSampleTime != 0 ) {
        bytes2 = _this -> File -> Read ( _this -> File, (char *)dst + bytes, _this -> BytesPerSampleTime - bytes % _this -> BytesPerSampleTime );
        if ( bytes2 <= 0 ) {
            // set eof ????
            break;
        }
        bytes += bytes2;
    }

    if ( _this -> EndianSwapper != NULL )
        _this -> EndianSwapper ( (void*) dst, bytes / _this -> BytesPerSample );

    items = bytes / _this -> BytesPerSampleTime;
    _this -> SamplePos += items;
    if ( _this -> SamplePos > _this -> SampleCount )
        _this -> SampleCount = _this -> SamplePos;

    return items;
}


static size_t
AudioIO_Write ( AudioIO* _this, const void* src, size_t items )
{
    size_t      bytes;

    if ( _this -> FullInitDone == false ) {
        if ( ! AudioIO_AllParametersSet (_this) )
            return -1;
        if ( _this -> FullInit ( _this ) != 0 )
            return -1;

        _this -> FullInitDone = true;
    }

    if ( _this -> EndianSwapper != NULL )
        _this -> EndianSwapper ( (void*) src, items * _this -> ChannelCount );

    bytes = items * _this -> BytesPerSampleTime;

    if ( _this -> File -> Write ( _this -> File, src, bytes ) != bytes )
        return -1;

    _this -> SampleCount += items;
    if ( _this -> SamplePos > _this -> SampleCount )
        _this -> SampleCount = _this -> SamplePos;
    return items;
}


static int
AudioIO_Close ( AudioIO* _this )
{
    if ( IO_IsSeekable ( _this -> File ) ) {
        _this -> File -> Seek ( _this -> File, 0L, Seek_Set );
        _this -> FullInit ( _this );
    }
    _this -> File -> Close ( _this -> File );
    _this -> File = NULL;

    free ( _this );
    return 0;
}

static size_t
IO_WriteAudioDevice ( IO* _this, const void* src, size_t bytes )
{
    if ( WindowsAudio_play ( src, bytes ) != bytes )
        return -1;

    _this -> offset += bytes;
    return bytes;
}


static size_t
IO_ReadAudioDevice ( IO* _this, void* dst, size_t bytes )
{
    /*
    if ( WindowsAudio_record ( dst, bytes ) != bytes )
        return -1;

    _this -> offset += bytes;
    return bytes;
    */

    size_t bytes_rec;
    bytes_rec = WindowsAudio_record ( dst, bytes );

    _this -> offset += bytes_rec;
    return bytes_rec;
}


static int
IO_CloseAudioDevice ( IO* _this )
{
    WindowsAudio_close ();
    return 0;
}


AudioIO*
AudioIO_OpenAudioDeviceOut ( unsigned int deviceno )
{
    AudioIO*  ret;

    ret = calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;

    ret -> File     = IO_OpenExternal ( NULL,
                                        IO_WriteAudioDevice,
                                        NULL,
                                        IO_CloseAudioDevice );

    if ( ret -> File == NULL ) {
        free ( ret );
        return NULL;
    }

    ret -> FileType = Filetype_WAVEout;
    ret -> DeviceNo = deviceno;
    ret -> FullInit = AudioIO_InitOutput;
    ret -> Write    = AudioIO_Write;
    ret -> Close    = AudioIO_Close;
    ret -> Seek     = AudioIO_Seek;
    ret -> Tell     = AudioIO_Tell;

    return ret;
}


AudioIO*
AudioIO_OpenAudioDeviceIn ( unsigned int deviceno )
{
    AudioIO*  ret;

    ret = calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;

    ret -> File     = IO_OpenExternal ( IO_ReadAudioDevice,
                                            NULL,
                                            NULL,
                                            IO_CloseAudioDevice );

    if ( ret -> File == NULL ) {
        free ( ret );
        return NULL;
    }

    ret -> FileType = Filetype_WAVEin;
    ret -> DeviceNo = deviceno;
    ret -> FullInit = AudioIO_InitInput;
    ret -> Read     = AudioIO_Read;
    ret -> Close    = AudioIO_Close;
    ret -> Seek     = AudioIO_Seek;
    ret -> Tell     = AudioIO_Tell;

    return ret;
}


AudioIO*
AudioIO_OpenAudioFileOut ( IO* basefile, pcm_filetype_t filetype )
{
    AudioIO*  ret;

    if ( basefile == NULL )
        return NULL;

    ret = calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;


    ret -> File     = basefile;
    ret -> FileType = filetype;
    ret -> DeviceNo = -1;
    ret -> FullInit = AudioIO_InitOutput;
    ret -> Write    = AudioIO_Write;
    ret -> Close    = AudioIO_Close;
    ret -> Seek     = AudioIO_Seek;
    ret -> Tell     = AudioIO_Tell;

    return ret;
}


AudioIO*
AudioIO_OpenAudioFileIn ( IO* basefile, pcm_filetype_t filetype )
{
    AudioIO*  ret;

    if ( basefile == NULL )
        return NULL;

    ret = calloc ( 1, sizeof (*ret) );
    if ( ret == NULL )
        return NULL;


    ret -> File     = basefile;
    ret -> FileType = filetype;
    ret -> DeviceNo = -1;
    ret -> FullInit = AudioIO_InitInput;
    ret -> Read     = AudioIO_Read;
    ret -> Close    = AudioIO_Close;
    ret -> Seek     = AudioIO_Seek;
    ret -> Tell     = AudioIO_Tell;

    return ret;
}


#elif defined __linux__

/************************************************************************/
/************************************************************************/
/****                                                                ****/
/****                      Code for POSIX 1.0 API                    ****/
/****                                                                ****/
/************************************************************************/
/************************************************************************/

#include "fileio_oss.h"
#include "fileio_esd.h"


static size_t
IO_ReadFile ( IO* _this, void* dst, size_t bytes )
{
    size_t  result;

    if ( bytes == 0 )                   // there often trouble writing 0 bytes. Instead of writing 0 bytes, often an error is signaled
        return 0;

    result = fread ( dst, 1, bytes, _this -> handle );
    if ( result == 0 ) {
            _this -> attr |= IO_FLAG_AT_EOF;
        return -1;
    }

    if ( bytes == 0 )
        _this -> attr |= IO_FLAG_AT_EOF;

    _this -> offset += result;
    return result;
}

// Write data to an object

static size_t
IO_WriteFile ( IO* _this, const void* src, size_t bytes )
{
    size_t  result;

    if ( bytes == 0 )
        return 0;

    // current encoder has here some penetration mode which repeats this until all bytes are actually be written
    result = fwrite ( src, 1, bytes, _this -> handle );
    if ( result == 0 )
        return -1;
    _this -> offset += result;
    return result;
}


static int
IO_CloseFile ( IO* _this )
{
    int  ret;

    ret = fclose ( _this -> handle );
    free ( _this );
    return ret == 0  ?  0  :  -1;
}


static offset_t
IO_GetSize ( IO* _this )
{
    offset_t  pos;
    offset_t  ret;

    if ( !IO_IsSeekable (_this) )
        return -1;

    pos = ftell ( _this -> handle );
    if ( pos == -1 )
        return pos;
    if ( fseek ( _this -> handle, 0L, SEEK_END ) )
        return -1;
    ret = ftell ( _this -> handle );
    fseek ( _this -> handle, pos, SEEK_SET );

    return ret;
}


static offset_t
IO_SeekFile ( IO* _this, offset_t newpos, seek_method_t whence )
{

    if ( !IO_IsSeekable (_this) )
        return -1;

    if ( fseek ( _this -> handle, newpos, whence ) )
        return -1;

    _this -> attr &= ~IO_FLAG_AT_EOF;
    return ftell ( _this -> handle );
}


static offset_t
IO_Tell ( IO* _this )
{
#if 0
    return ftell ( _this -> handle );
#else
    return _this -> offset;
#endif
}


static offset_t
IO_SetSize ( IO* _this, offset_t newsize )
{
    if ( !IO_IsSeekable (_this) )
        return -1;

    if ( !IO_IsWritable (_this) )
        return -1;

    fflush ( _this -> handle );
    if ( ftruncate ( fileno (_this -> handle), newsize ) )
        return -1;

    fseek ( _this -> handle, newsize, SEEK_SET );
    _this -> attr &= ~IO_FLAG_AT_EOF;
    return newsize;
}


static bool
IO_IsSeekableInit ( HANDLE handle )
{
    if ( fseek ( handle, 0L, SEEK_CUR ) == 0 )
        return true;

    return false;
}


#elif defined __TURBOC__

/************************************************************************/
/************************************************************************/
/****                                                                ****/
/****                 Code for Turbo and Borland C                   ****/
/****                                                                ****/
/************************************************************************/
/************************************************************************/



#else

# error Unknown Compiler and Operating System

#endif


IO*
IO_OpenGenericRead ( const char* name )
{
    if ( 0 == strcmp ( name, "/dev/null"   ) )
        return NULL;

    if ( 0 == strcmp ( name, "/dev/stdin"  )  ||
         0 == strcmp ( name, "-"           ) )
        return NULL;

    if ( 0 == strncmp ( name, "ftp://" , 6 ) )
        return NULL;

    if ( 0 == strncmp ( name, "http://", 7 ) )
        return NULL;

    // what else ????


    return NULL;
}


AudioIO*
AudioIO_OpenAudioGenericOut ( const char* name )
{
    IO*           file;
    unsigned int  tmp;

    if ( 0 == strcmp ( name, "/dev/null"   ) ) {
        file = NULL;
        return NULL /* ( file ) */;
    }

    if ( 0 == strcmp ( name, "/dev/stdout" )  ||
         0 == strcmp ( name, "-"           )  ||
         0 == strcmp ( name, ""            ) ) {
        file = NULL;
        if ( IO_IsATTY ( file ) ) {
            // makes no sense
            return NULL;
        }
        return NULL;
    }

    if ( 1 == sscanf ( name, "/dev/audio%u", &tmp )  ||
         1 == sscanf ( name, "/dev/dsp%u",   &tmp ) )
        return AudioIO_OpenAudioDeviceOut ( tmp );

    if ( 0 == strcmp ( name, "/dev/esd"    ) )
        return AudioIO_OpenAudioDeviceOut ( 0xFFFFFFFF );

    if ( IO_IsDirectory ( name ) ) {
        // store directory name
        // we need a function which is called at the beginning of every file and which normally do nothing except this case
        // where this functions generation the real output files
        return NULL;
    }

    tmp = name [strlen(name) - 1];
    if ( tmp == '/'  ||
         tmp == '\\' ) {
        // make directory (if not existing)
        // store directory name
        // we need a function which is called at the beginning of every file and which normally do nothing except this case
        // where this functions generation the real output files
        return NULL;
    }

    file = NULL;
    if ( IO_IsATTY ( file ) ) {
        // makes no sense
        // close and return NULL
        return NULL;
    }

    // what else ????

    return NULL;
}


/* end of fileio.c */
