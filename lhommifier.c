#include <stdio.h>
#include <string.h>


// #define BIDIR_LEN                    // 1)
#define MAX_FRAME_LEN   8191            // 2)


static void
writeoutput ( const char* basename, long no, const void* data, unsigned int len )
{
    char   name [2048];
    FILE*  fp;

    switch ( no ) {
    case -2:
        sprintf ( name, "%s.footer", basename );
        break;
    case -1:
        sprintf ( name, "%s.header", basename );
        break;
    default:
        sprintf ( name, "%s.%06ld", basename, no );
        break;
    }

    if ( ( fp = fopen ( name, "wb" ) ) == NULL ) {
        fprintf ( stderr, "\nlhommifier: Can't create '%s'\n", name );
        exit (126);
    }

    fprintf ( stderr, "Writing '%s'...", name );
    fwrite ( data, 1, len, fp );
    fclose ( fp );
    fprintf ( stderr, "\b\b\b   \r" );
}


int
main ( int argc, char** argv )
{
    unsigned char  buff [65536];        // Read/Write buffer
    FILE*          fp;                  // input file pointer
    unsigned int   len;                 // length information before a block
    unsigned int   bidir_len;           // length information after a block (when BIDIR_LEN is defined)
    long           frames;
    long           i;

    // usage
    if ( argc < 2 ) {
        fprintf ( stderr, "usage: lhommifier filename\n" );
        return 1;
    }

    // opening of source file
    if ( ( fp = fopen ( argv [1], "rb" ) ) == NULL ) {
        fprintf ( stderr, "lhommifier: Can't open '%s'\n", argv [1] );
        return 2;
    }

    // analysis of 4 byte magic ID + first 2 bytes of header
    fread ( buff, 1, 6, fp );
    if ( 0 != memcmp ( buff, "MP+\xFF", 4 ) ) {                 // 3)
        fprintf ( stderr, "lhommifier: Input file '%s' is not StreamVersion 15.15\n", argv [1] );
        return 3;
    }
    len = buff[4] * 256 + buff[5];

    if ( len > MAX_FRAME_LEN ) {
        fprintf ( stderr, "lhommifier: Header length is out of range [0,%u]: %u\n", MAX_FRAME_LEN, len );
        return 4;
    }

    // analysis and copying of header
    len = fread ( buff, 1, len, fp );
    writeoutput ( argv[1], -1, buff, len );

    frames = ((long)buff [2] << 24) |                           // 4)
             ((long)buff [3] << 16) |
             ((long)buff [4] <<  8) |
             ((long)buff [5] <<  0);

#ifdef BIDIR_LEN
    fread ( buff, 1, 2, fp );
    bidir_len = buff[4] * 256 + buff[5];
    if ( len != bidir_len ) {
        fprintf ( stderr, "lhommifier: bidir length mismatch in header: %u != %u\n", len, bidir_len );
        return 5;
    }
#endif

    // analysis of frames
    fprintf ( stderr, "lhommifier: Decomposing frames 0...%lu\n", frames-1 );

    for ( i = 0; i < frames; i++ ) {
        fread ( buff, 1, 2, fp );
        len = buff[0] * 256 + buff[1];

        if ( len > MAX_FRAME_LEN ) {
            fprintf ( stderr, "\nlhommifier: Frame #%ld length is out of range [0,%u]: %u\n", i, MAX_FRAME_LEN, len );
            return 6;
        }
        len = fread ( buff, 1, len, fp );
        writeoutput ( argv[1], i, buff, len );

#ifdef BIDIR_LEN
        fread ( buff, 1, 2, fp );
        bidir_len = buff[4] * 256 + buff[5];
        if ( len != bidir_len ) {
            fprintf ( stderr, "lhommifier: bidir length mismatch in frame #ld: %u != %u\n", i, len, bidir_len );
            return 7;
        }
#endif
    }
    fprintf ( stderr, "\n" );

    // remaining data are tags (this only copies tags up to 64 KBytes correctly)
    len = fread ( buff, 1, sizeof buff, fp );
    if ( 0 == memcmp ( buff, "APETAGEX", 8 ) ) fprintf ( stderr, "informational: APE tags with header\n" );
    if ( 0 == memcmp ( buff, "TAG"     , 3 ) ) fprintf ( stderr, "informational: ID3V1 tags\n"           );
    writeoutput ( argv[1], -2, buff, len );
    fprintf ( stderr, "\n" );

    fclose ( fp );
    return 0;
}

/* end of lhommifier.c */

/*
 *  To be defined:
 *    1) use of BIDIR_LEN: yes, no, optional ?
 *         + can be used for seek forward and backward
 *         + can be used to detect errors in the stream
 *         + can be used for resync
 *         - needs 0.6 kbps
 *
 *    2) MAX_FRAME_LEN: 8191 or 16383 ?
 *         o  8191 = 2.5 Mbps
 *         o 16383 = 5 Mbps
 *         o more than 14 bit are not possible, otherwise there are collisions with tags, headers, etc. which do start with a capital letter
 *
 *    3) StreamVersion
 *         o major stream version is stored in the lower 4 bit
 *         o minor stream version is stored in the upper 4 bit
 *         o final format (with different versions) will be something like 8.0...8.3, which ends up with
 *           a \x08, \x18, \x28 or \x38 here
 *
 *    4) number of frames is stored as internal data in the header, but this information
 *       is also a very interesting global information, so the offset of this info should be fixed.
 *       Currently at offset 2, but may be offset 0 is better and less arbitrary ???
 *
 *    x) For A/V the number of generated PCM samples of one frame may be of interest.
 *       Currently these are 0...1152 (may be also 1536). But this information is not
 *       public available. Is this a problem ???
 *
 *    y) also the following information are not available: channel count, sampling frequency.
 *       Is this a problem ore must this be fixed ???
 *
 */
