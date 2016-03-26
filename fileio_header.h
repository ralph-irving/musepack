/*
Doku
*/

static size_t
Calc_RIFFHeader ( const AudioIO* _this, unsigned char* buffer )
{
    unsigned long   MAXWAVESIZE   = 4294967040LU;
    unsigned char*  p             = buffer;
    double          PCMdataLength = (double) _this -> BytesPerSampleTime * _this -> SampleCount;
    unsigned int    word32;
    if ( _this -> SampleCount == 0 )
        PCMdataLength = 4*44100; // MAXWAVESIZE;  // a hack to get some length for wav header when stopped with CTRL-C
                                                  // should be done in the reader function, if data len == 0, use file length instead

    *p++ = 'R';
    *p++ = 'I';
    *p++ = 'F';
    *p++ = 'F';                                   // "RIFF" label

    word32 = PCMdataLength + (44 - 8) < (double)MAXWAVESIZE  ?
             (unsigned int)PCMdataLength + (44 - 8)  :  (unsigned int)MAXWAVESIZE;
    *p++ = (unsigned char)(word32 >>  0);
    *p++ = (unsigned char)(word32 >>  8);
    *p++ = (unsigned char)(word32 >> 16);
    *p++ = (unsigned char)(word32 >> 24);         // Size of the next chunk

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

    *p++ = 0x01;
    *p++ = 0x00;                                  // ACM type 0x0001 = uncompressed linear PCM

    *p++ = (unsigned char)(_this -> ChannelCount >> 0);
    *p++ = (unsigned char)(_this -> ChannelCount >> 8); // Channels

    word32 = (unsigned int) (_this -> SampleFreq + 0.5);
    *p++ = (unsigned char)(word32 >>  0);
    *p++ = (unsigned char)(word32 >>  8);
    *p++ = (unsigned char)(word32 >> 16);
    *p++ = (unsigned char)(word32 >> 24);         // Sample frequency

    word32 *= _this -> BytesPerSampleTime;
    *p++ = (unsigned char)(word32 >>  0);
    *p++ = (unsigned char)(word32 >>  8);
    *p++ = (unsigned char)(word32 >> 16);
    *p++ = (unsigned char)(word32 >> 24);         // Bytes per second in the data stream

    word32 = _this -> BytesPerSampleTime;
    *p++ = (unsigned char)(word32 >>  0);
    *p++ = (unsigned char)(word32 >>  8);         // Bytes per sample time

    *p++ = (unsigned char)(_this -> BitsPerSample >> 0);
    *p++ = (unsigned char)(_this -> BitsPerSample >> 8);   // Bits per single sample

    *p++ = 'd';
    *p++ = 'a';
    *p++ = 't';
    *p++ = 'a';                                   // "data" label

    word32 = PCMdataLength < MAXWAVESIZE  ?  (unsigned int)PCMdataLength  :  (unsigned int)MAXWAVESIZE;
    *p++ = (unsigned char)(word32 >>  0);
    *p++ = (unsigned char)(word32 >>  8);
    *p++ = (unsigned char)(word32 >> 16);
    *p++ = (unsigned char)(word32 >> 24);               // Größe der rohen PCM-Daten

    return p - buffer;
}


/*
 *  Write an AIFF header for a simple AIFF file with 1 PCM-Chunk. Settings are passed via function parameters.
 */

static size_t
Calc_AIFFHeader ( const AudioIO* _this, unsigned char* buffer )
{
    uint8_t      Header [54];
    uint8_t*     p             = Header;
    double       PCMdataLength = (double) _this -> BytesPerSampleTime * _this -> SampleCount;
    uint32_t     word32;

    // FORM chunk
    *p++ = 'F';
    *p++ = 'O';
    *p++ = 'R';
    *p++ = 'M';

    word32 = (uint32_t) PCMdataLength + 0x2E;  // size of the AIFF chunk
    *p++ = (uint8_t)(word32 >> 24);
    *p++ = (uint8_t)(word32 >> 16);
    *p++ = (uint8_t)(word32 >>  8);
    *p++ = (uint8_t)(word32 >>  0);

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
    *p++ = (uint8_t)(word32 >> 24);
    *p++ = (uint8_t)(word32 >> 16);
    *p++ = (uint8_t)(word32 >>  8);
    *p++ = (uint8_t)(word32 >>  0);

    word32 = _this -> ChannelCount;            // channels
    *p++ = (uint8_t)(word32 >>  8);
    *p++ = (uint8_t)(word32 >>  0);

    word32 = _this -> SampleCount < 0xFFFFFFFFLU  ?  (uint32_t)_this -> SampleCount  :  (uint32_t)0xFFFFFFFFLU;  // so called "frames"
    *p++ = (uint8_t)(word32 >> 24);
    *p++ = (uint8_t)(word32 >> 16);
    *p++ = (uint8_t)(word32 >>  8);
    *p++ = (uint8_t)(word32 >>  0);

    word32 = _this -> BitsPerSample;           // bits
    *p++ = (uint8_t)(word32 >>  8);
    *p++ = (uint8_t)(word32 >>  0);

    p = Write_to_80bit_BE_IEEE854_Float ( p, _this -> SampleFreq );  // sample frequency as big endian 80 bit IEEE854 float
    // End of COMM chunk

    // SSND chunk
    *p++ = 'S';
    *p++ = 'S';
    *p++ = 'N';
    *p++ = 'D';

    word32 = (uint32_t) PCMdataLength + 0x08;  // chunk length
    *p++ = (uint8_t)(word32 >> 24);
    *p++ = (uint8_t)(word32 >> 16);
    *p++ = (uint8_t)(word32 >>  8);
    *p++ = (uint8_t)(word32 >>  0);

    *p++ = 0;                                  // offset
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;

    *p++ = 0;                                  // block size
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;

    return p - buffer;
}

/* end of fileio_header.h */
