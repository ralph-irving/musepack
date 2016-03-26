#ifndef FILEIO_H
#define FILEIO_H

/**********************************************************************************************************/
/**********************************************************************************************************/


#if defined _WIN32              // actually on MSC or ICL

# include <stdio.h>
# include <stdlib.h>
# include <windows.h>
# include <mmsystem.h>
//# include <winbase.h>
//# include <mmreg.h>
//# include <io.h>
//# include <fcntl.h>

typedef unsigned __int64        uint64_t;
typedef signed   __int64        int64_t;
typedef unsigned __int32        uint32_t;
typedef signed   __int32        int32_t;
typedef unsigned __int16        uint16_t;
typedef signed   __int16        int16_t;
typedef unsigned __int8         uint8_t;
typedef signed   __int8         int8_t;
typedef int                     bool;
typedef int64_t                 offset_t;
# define false     0
# define true      1

#elif defined __GNUC__

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdint.h>
# include <sys/types.h>
# include <unistd.h>

typedef int                     bool;
typedef off_t                   offset_t;
typedef FILE*                   HANDLE;
# define INVALID_HANDLE_VALUE   NULL
# define false     0
# define true      1

#endif /* _WIN32 */


//#define _       _this ->

/**********************************************************************************************************/
/**********************************************************************************************************/


typedef struct _IO   IO;


typedef enum {
#ifdef _WIN32
    Seek_Set = FILE_BEGIN,
    Seek_Cur = FILE_CURRENT,
    Seek_End = FILE_END
#else
    Seek_Set = SEEK_SET,
    Seek_Cur = SEEK_CUR,
    Seek_End = SEEK_END
#endif
} seek_method_t;


struct _IO {
// private:
    HANDLE        handle;
    int           socket;
    uint16_t      attr;
    offset_t      offset;
    char*         filename;

// public:
    size_t      (* Read )       ( IO* _this, void*       dst, size_t bytes );
    size_t      (* Write)       ( IO* _this, const void* src, size_t bytes );
    int         (* Close)       ( IO* _this );
    offset_t    (* GetSize)     ( IO* _this );
    offset_t    (* SetSize)     ( IO* _this, offset_t newsize );
    offset_t    (* Seek )       ( IO* _this, offset_t newpos, seek_method_t whence );
    offset_t    (* Tell )       ( IO* _this );
    int         (* Lock )       ( IO* _this, offset_t first, offset_t length );
    int         (* UnLock)      ( IO* _this, offset_t first, offset_t length );
    bool        (* IsAtEOF)     ( IO* _this );
};


bool          IO_IsReadable     ( const IO* _this );
bool          IO_IsWritable     ( const IO* _this );
bool          IO_IsSeekable     ( const IO* _this );
bool          IO_IsATTY         ( const IO* _this );


IO*           IO_OpenFile       ( const char* filename );
IO*           IO_OpenFileRead   ( const char* filename );
IO*           IO_OpenFileWrite  ( const char* filename );
IO*           IO_OpenGenericRead( const char* name );
IO*           IO_OpenGenericWrite(const char* name );
IO*           IO_OpenMemory     ( void*       data, size_t len );
IO*           IO_OpenMemoryRead ( const void* data, size_t len );
IO*           IO_OpenMemoryWrite( void*       data, size_t len );
IO*           IO_OpenStdin      ( void );
IO*           IO_OpenStdout     ( void );
IO*           IO_OpenNull       ( void );
IO*           IO_OpenPipe       ( const char* cmd_template, const char* filename );
IO*           IO_OpenUDPRead    ( const char* hostname, uint16_t port );
IO*           IO_OpenTCPRead    ( const char* hostname, uint16_t port );
IO*           IO_OpenHTTPRead   ( const char* URL );
IO*           IO_OpenExternal   ( size_t   (*ReadFn ) ( IO* _this, void*       dst, size_t bytes ),
                                  size_t   (*WriteFn) ( IO* _this, const void* src, size_t bytes ),
                                  offset_t (*SeekFn ) ( IO* _this, offset_t pos, seek_method_t whence ),
                                  int      (*CloseFn) ( IO* _this ) );


/**********************************************************************************************************/
/**********************************************************************************************************/


typedef struct _AudioIO  AudioIO;


typedef enum {
    Filetype_Unknown   = -1,
    Filetype_Auto      =  0,
    //  1... 31 uncompressed linear PCM
    Filetype_RAW       =  1,
    Filetype_RAW_LE    =  2,
    Filetype_RAW_BE    =  3,
    Filetype_WAV       =  4,
    Filetype_AIFF      =  5,
    // 32... 63 compressed linear PCM
    Filetype_APE       = 32,
    Filetype_FLAC      = 33,
    Filetype_LPAC      = 34,
    Filetype_WAVPACK   = 35,
    Filetype_LA        = 36,
    Filetype_OPTIMFROG = 37,
    Filetype_SHORTEN   = 38,
    Filetype_SZIP      = 39,
    Filetype_BZIP2     = 40,
    Filetype_GZIP      = 41,
    // 64... 95 lossy formats
    // 96...127 special devices
    Filetype_WAVEout   = 96,
    Filetype_WAVEin    = 97,
} pcm_filetype_t;


typedef enum {
    Endian_unknown = -1,
    Endian_big     =  0,
    Endian_little  =  1,
} endian_t;


struct _AudioIO {
// private:
    IO*             File;
    pcm_filetype_t  FileType;
    unsigned int    DeviceNo;
    bool            FullInitDone;
    offset_t        SampleCount;                // Total Count of available samples
    offset_t        SamplePos;                  // Current file position
                                                // values for 5x 24 bit x 48 kHz
    unsigned int    ChannelCount;               //  5
    unsigned int    BitsPerSample;              // 24
    unsigned int    BytesPerSample;             //  3
    unsigned int    BytesPerSampleTime;         // 15
    long double     SampleFreq;                 // 48000
    size_t          HeaderSize;                 // 44
    void         (* EndianSwapper )             ( void* array, size_t items );
    const char*     DirectoryName;

// public:
    size_t       (* Read )                      ( AudioIO* _this, void*       dst, size_t bytes );
    size_t       (* Write)                      ( AudioIO* _this, const void* src, size_t bytes );
    offset_t     (* Seek )                      ( AudioIO* _this, offset_t newpos, seek_method_t whence );       // I would propose to seek and tell in Samples, HeaderSize + BytesPerSampleTime * newpos
    offset_t     (* Tell )                      ( AudioIO* _this );
    int          (* FullInit)                   ( AudioIO* _this );
    int          (* Close)                      ( AudioIO* _this );
};


bool              AudioIO_IsReadable            ( const AudioIO* _this );
bool              AudioIO_IsWritable            ( const AudioIO* _this );
bool              AudioIO_IsSeekable            ( const AudioIO* _this );

unsigned int      AudioIO_GetBitsPerSample      ( const AudioIO* _this );
unsigned int      AudioIO_GetBytesPerSample     ( const AudioIO* _this );
unsigned int      AudioIO_GetBytesPerSampleTime ( const AudioIO* _this );
unsigned int      AudioIO_GetChannelCount       ( const AudioIO* _this );
long double       AudioIO_GetSampleFreq         ( const AudioIO* _this );


long double       AudioIO_SetSampleFrequency    ( AudioIO* _this, long double  samplefreq );
int               AudioIO_SetChannelCount       ( AudioIO* _this, unsigned int channels   );
int               AudioIO_SetBitsPerSample      ( AudioIO* _this, unsigned int bits       );
const char*       AudioIO_SetFileName           ( AudioIO* _this, const char* filename    );


AudioIO*          AudioIO_OpenAudioFileOut      ( IO* basefile, pcm_filetype_t filetype );
AudioIO*          AudioIO_OpenAudioFileIn       ( IO* basefile, pcm_filetype_t filetype );
AudioIO*          AudioIO_OpenAudioDeviceOut    ( unsigned int deviceno );
AudioIO*          AudioIO_OpenAudioDeviceIn     ( unsigned int deviceno );
AudioIO*          AudioIO_OpenAudioGenericOut   ( const char* name );
AudioIO*          AudioIO_OpenAudioGenericIn    ( const char* name );


/**********************************************************************************************************/
/**********************************************************************************************************/

#endif /* FILEIO_H */

/* end of fileio.h */
