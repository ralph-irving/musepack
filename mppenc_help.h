static void
longhelp ( void )
{
    stderr_printf (
            "\n"
             "\033[1m\rusage:\033[0m\n"
             "  mppenc [--options] <Input_File>\n"
             "  mppenc [--options] <Input_File> <Output_File>\n"
             "  mppenc [--options] <List_of_Input_Files>\n"
             "  mppenc [--options] <List_of_Input_Files> <Output_File>        (not yet supp.)\n"
             "  mppenc [--options] <List_of_Input_Files> <Output_Directory>   (not yet supp.)\n"
             "\n" );

    stderr_printf (
             "\033[1m\rInput_File must be:\033[0m\n"
             "  -                for stdin (only RIFF WAVE files)\n"
             "  /dev/audio:samplefreq:duration\n"
             "                   record from soundcard (OSS + Windows)\n"
             "                   samplefreq in Hz (default: 44100 Hz)\n"
             "                   duration in sec (default: 86400 sec)\n"
             "  *.wav            RIFF WAVE file\n"
             "  *.raw/cdr        Raw PCM, 2 channels, 16 bit, 44.1 kHz, little endian\n"
             "  *.pac/lpac       LPAC file                (needs LPAC 1.36+ for Windows)\n"
             "  *.fla/flac       FLAC file                (needs FLAC 1.03+ for Windows)\n"
             "  *.ape            Monkey's Audio APE file  (needs MAC 3.96b2...7)\n"
             "  *.rka/rkau       RK Audio file            (offical binaries do not work)\n"
             "  *.sz             SZIP file\n"
             "  *.shn            Shorten file             (needs Shorten 3.4+ for Windows)\n"
             "  *.ofr            OptimFROG file\n"
             "\n"
             "Currently 16...64 kHz, 1...8 channels, 8...32 bit linear PCM\n"
             "is supported. When using one of the lossless compressed formats, a proper binary\n"
             "must be installed within the system's search path.\n"
             "\n"
             "\033[1m\rOutput_File must be (otherwise file name is generated from Input_File):\033[0m\n"
             "  *.mpc            Musepack file name \n"
             "  *.mp+/mpp        old extentions known as MPEGplus\n"
             "  -                for stdout\n"
             "  /dev/null        for trash can\n"
             "\n" );

    stderr_printf (
             "\033[1m\rProfile Options (Quality Presets):\033[0m\n"
             "  --telephone      lowest quality,       (typ.  32... 48 kbps)\n"
             "  --thumb          low quality/internet, (typ.  58... 86 kbps)\n"
             "  --radio          medium (MP3) quality, (typ. 112...152 kbps)\n"
             "  --standard       high quality (dflt),  (typ. 142...184 kbps)\n"
             "  --xtreme         extreme high quality, (typ. 168...212 kbps)\n"
             "  --insane         extreme high quality, (typ. 232...268 kbps)\n"
             "  --braindead      extreme high quality, (typ. 232...278 kbps)\n"
             "  --quality x.xx   set quality to x.xx, useful is 4.5...6.5\n"
             "\n" );

    stderr_printf (
             "\033[1m\rFile/Message handling:\033[0m\n"
             "  --silent         do not write any message to the console\n"
             "  --verbose        increase verbosity (dflt: off)\n"
             "  --longhelp       print this help text\n"
             "  --stderr fn      append messages to file 'fn'\n"
             "  --neveroverwrite never overwrite existing destination file\n"
             "  --interactive    ask before overwrite existing destination file (dflt)\n"
             "  --overwrite      overwrite existing destination file\n"
             "  --deleteinput    delete input file after encoding (dflt: off)\n"
             "\n" );

    stderr_printf (
             "\033[1m\rTagging (uses APE 2.0 tags):\033[0m\n"
             "  --tag key=value  Add tag \"key\" with \"value\" as contents\n"
             "  --tagfile key=f  dto., take value from a file 'f'\n"
             "  --tagfile key    dto., take value from console\n"
             "  --artist 'value' shortcut for --tag 'Artist=value'\n"
             "  --album 'value'  shortcut for --tag 'Album=value'\n"
             "                   other possible keys are: debutalbum, publisher, conductor,\n"
             "                   title, subtitle, track, comment, composer, copyright,\n"
             "                   publicationright, filename, recordlocation, recorddate,\n"
             "                   ean/upc, year, releasedate, genre, media, index, isrc,\n"
             "                   abstract, bibliography, introplay, media, language, ...\n"
             "\n" );

    stderr_printf (
             "\033[1m\rAudio processing:\033[0m\n" );
    stderr_printf (
             "  --skip x         skip the first x seconds  (dflt: %3.1f)\n",            SkipTime );
    stderr_printf (
             "  --dur x          stop encoding after at most x seconds of encoded audio\n" );
    stderr_printf (
             "  --fade x         fadein+out in seconds\n" );
    stderr_printf (
             "  --fadein x       fadein  in seconds (dflt: %3.1f)\n",                   FadeInTime );
    stderr_printf (
             "  --fadeout x      fadeout in seconds (dflt: %3.1f)\n",                   FadeOutTime );
    stderr_printf (
             "  --fadeshape x    fade shape (dflt: %3.1f),\n"
             "                   see http://www.uni-jena.de/~pfk/mpc/img/fade.png\n",   FadeShape );
    stderr_printf (
             "  --scale x1       scale input signal by x (dflt: %7.5f)\n",              ScalingFactor[0] );
    stderr_printf (
             "  --scale x1,x2,.. scale input signal, separate for each channel\n" );

    stderr_printf (
             "\033[1m\rExpert settings:\033[0m\n" );
    stderr_printf (
             "==Masking thresholds======\n" );
    stderr_printf (
             "  --quality x      set Quality to x (dflt: 5.00)\n" );
    stderr_printf (
             "  --nmt x          set NMT value to x dB (dflt: %4.1f)\n", NMT );
    stderr_printf (
             "  --tmn x          set TMN value to x dB (dflt: %4.1f)\n", TMN );
    stderr_printf (
             "  --pns x          set PNS value to x dB (dflt: %4.1f)\n", PNS );
    stderr_printf (
             "==ATH/Bandwidth settings==\n" );
    stderr_printf (
             "  --bw x           maximum bandwidth in Hz (dflt: %4.1f kHz)\n", (MaxSubBand + 1) * (SampleFreq / 32000.) );
    stderr_printf (
             "  --minSMR x       minimum SMR of x dB over encoded bandwidth (dflt: %2.1f)\n",  minSMR );
    stderr_printf (
             "  --ltq x          0: ISO threshold in quiet (not recommended)\n"
             "                   1: more sensitive threshold in quiet (Buschmann)\n"
             "                   2: even more sensitive threshold in quiet (Filburt)\n"
             "                   3: Klemm\n"
             "                   4: Klemm-Buschmann Mix\n"
             "                   5: minimum of Klemm and Buschmann (dflt)\n" );
    stderr_printf (
             "  --ltq20 x        add offset of x dB at 20 kHz\n" );
    stderr_printf (
             "  --ltq_max x      maximum level for ltq (dflt: %4.1f dB)\n",                Ltq_max      );
    stderr_printf (
             "  --ltq_var x      adaptive threshold in quiet: 0: off, >0: on (dflt: %g)\n",varLtq       );
    stderr_printf (
             "  --tmpMask x      exploit postmasking: 0: off, 1: on (dflt: %d)\n",         tmpMask_used );
    stderr_printf (
             "==Stuff settings==========\n" );
    stderr_printf (
             "  --ms x           Mid/Side Stereo, 0: off, 1: reduced, 2: on, 3: decoupled,\n"
             "                   10: enhanced 1.5/3 dB, 11: 2/6 dB, 12: 2.5/9 dB,\n"
             "                   13: 3/12 dB, 15: 3/oo dB (dflt: %d)\n",                        MS_Channelmode );
    stderr_printf (
             "  --ans x          Adaptive Noise Shaping Order: 0: off, 1...6: on (dflt: %d)\n", Max_ANS_Order );
    stderr_printf (
             "  --cvd x          ClearVocal-Detection, 0: off, 1: on, 2: dual (dflt: %d)\n",    CVD_used );
    stderr_printf (
             "  --shortthr x,y   short FFT threshold (dflt: %4.1f,%4.1f)\n",                    ShortThr, UShortThr );
    stderr_printf (
             "  --transdetect x1:x2,y1:y2\n"
             "                   slewrate for transient detection (dflt: %g:%g,%g:%g)\n",       TransDetect1, TransDetect2, TransDetect3, TransDetect4 );
    stderr_printf (
             "  --aux x1[,x2[,x3[,x4...]]]\n"
             "                   auxiliary experimental parameters (dflt: all 0)\n" );
    stderr_printf (
             "  --kbd x1,x2,x3   Window shape for psycho analysis (dflt: %g,%g,%g)\n", alpha0, alpha1, alpha2 );
    stderr_printf (
             "  --minval x       calculation of MinVal (1:Buschmann, 2,3:Klemm)\n" );
    stderr_printf (
             "\n" );

    stderr_printf (
             "\033[1m\rExamples:\033[0m\n"
             "  mppenc inputfile.wav\n"
             "  mppenc inputfile.wav outputfile.mpc\n"
             "  mppenc --xtreme inputfile.pac outputfile.mpc\n"
             "  mppenc --silent --radio --pns 0.25 inputfile.pac outputfile.mpc\n"
             "  mppenc --nmt 12 --tmn 28 inputfile.pac outputfile.mpc\n"
             "\n"
             "For further information see the file 'MANUAL.TXT'.\n" );
}


static void
shorthelp ( void )
{
    stderr_printf (
             "\n"
             "\x1B[1m\rusage:\n"
             "\x1B[0m\r  mppenc [--options] <Input_File>\n"
             "  mppenc [--options] <Input_File> <Output_File>\n"
             "\n"

             "\033[1m\rStandard options:\033[0m\n"
             "  --silent       do not write any message to the console (dflt: off)\n"
             "  --deleteinput  delete input file after encoding        (dflt: off)\n"
             "  --overwrite    overwrite existing destination file     (dflt: off)\n"
             "  --fade sec     fade in and out with 'sec' duration     (dflt: 0.0)\n"
             "\n"

             "\033[1m\rProfile Options (Quality Presets):\033[0m\n"
             "  --thumb        low quality/internet, (typ.  58... 86 kbps)\n"
             "  --radio        medium (MP3) quality, (typ. 112...152 kbps)\n"
             "  --standard     high quality (dflt),  (typ. 142...184 kbps)\n"
             "  --xtreme       extreme high quality, (typ. 168...212 kbps)\n"
             "\n"

             "\033[1m\rExamples:\033[0m\n"
             "  mppenc inputfile.wav\n"
             "  mppenc inputfile.wav outputfile.mpc\n"
             "  mppenc --xtreme inputfile.pac outputfile.mpc\n"
             "  mppenc --silent --radio inputfile.pac outputfile.mpc\n"
             "\n"
             "For further information see the file 'MANUAL.TXT' or use option --longhelp.\n" );
}
