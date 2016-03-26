#include "fileio.h"

unsigned long Read_LE_Uint32 ( const unsigned char* p )
{
    return ((unsigned long)p[0] <<  0) |
           ((unsigned long)p[1] <<  8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);

}

unsigned long Read_LE_Uint16 ( const unsigned char* p )
{
    return ((unsigned long)p[0] <<  0) |
           ((unsigned long)p[1] <<  8);
}

int main ( int argc, char** argv )
{
    IO*         _if = NULL;
    IO*         _of = NULL;
    AudioIO*    _oa = NULL; // output audio file/device
    AudioIO*    _ia = NULL; // input audio file/device

    if ( argc < 2 || strcmp (argv[1], "*") != 0 ) {
        if ( 0 == strcmp (argv[1], "-") )
            _if = IO_OpenStdin ();
        else
            _if = IO_OpenFileRead ( argv[1] );

        _ia = AudioIO_OpenAudioFileIn ( _if, Filetype_WAV );
    } else {
        _ia = AudioIO_OpenAudioDeviceIn ( 0 );

        if ( _ia != NULL ) {    // set recording settings
            AudioIO_SetBitsPerSample   ( _ia, 16    );
            AudioIO_SetChannelCount    ( _ia, 2     );
            AudioIO_SetSampleFrequency ( _ia, 44100 );
        }
    }

    if ( _ia == NULL ) {
        if ( _if ) _if -> Close ( _if );
        fprintf ( stderr, "Failed to open input.\n" );
        return 1;
    }

    if ( argc > 2 ) {
        if ( 0 == strcmp (argv[2], "-") )
            _of = IO_OpenStdout ();
        else
            _of = IO_OpenFile ( argv[2] );

        _oa = AudioIO_OpenAudioFileOut ( _of, Filetype_WAV );

    } else {
        _oa = AudioIO_OpenAudioDeviceOut ( 0 );
    }

    if ( _oa == NULL ) {
        if ( _ia ) _ia -> Close ( _ia );
        if ( _of ) _of -> Close ( _of );
        fprintf ( stderr, "Failed to open output.\n" );
        return 1;
    }

    {
        char buf[44100 * (32/8)*2];   // 100 samples, max 32bps 2 channels
        bool init = false;

        //while ( ! _ia -> File -> IsAtEOF (_ia -> File) ) {
        while ( 1 ) {
            size_t  items_read;
            char*   data = (char *)buf;

            items_read = _ia -> Read  ( _ia, buf, 100 );
            //if ( items_read == 0 )
            //    break;

            AudioIO_SetBitsPerSample   ( _oa, AudioIO_GetBitsPerSample   (_ia) );
            AudioIO_SetChannelCount    ( _oa, AudioIO_GetChannelCount    (_ia) );
            AudioIO_SetSampleFrequency ( _oa, AudioIO_GetSampleFreq      (_ia) );

            if ( _oa -> Write (_oa, data, items_read) == 0 ) ;
                //break;
        }
    }

    _ia -> Close ( _ia );
    _oa -> Close ( _oa );

    return 0;
}
