/* bei ShowProgress können die databits überlaufen (ca. 7 Stunden) */

static void
ShowParameters ( const char* inDatei,
                 const char* outDatei )
{
    static const char        unk      []       = "???";
    static const char*       EarModel []       = { "ISO (bad!!!)", "Busch", "Filburt", "Klemm", "Busch/Klemm Mix", "min(Busch,Klemm)" };
    static const char        th       [ 7] [4] = { "no", "1st", "2nd", "3rd", "4th", "5th", "6th" };
    static const char        able     [ 3] [9] = { "Disabled", "Enabled", "Dual" };
    static const char*       stereo   [16]     = {
        "Simple uncoupled Stereo",
        "Mid/Side Stereo + Intensity Stereo 2 bit",
        "Mid/Side Stereo + Intensity Stereo 4 bit",
        "Mid/Side Stereo, destroyed imaging (unusable)",
        "Mid/Side Stereo, much reduced imaging",
        "Mid/Side Stereo, reduced imaging (-3 dB)",
        "Mid/Side Stereo when superior",
        unk, unk, unk,
        "Mid/Side Stereo when superior + enhanced (1.5/3 dB)",
        "Mid/Side Stereo when superior + enhanced (2/6 dB)",
        "Mid/Side Stereo when superior + enhanced (2.5/9 dB)",
        "Mid/Side Stereo when superior + enhanced (3/12 dB)",
        unk,
        "Mid/Side Stereo when superior + enhanced (3/oo dB)"
    };
    static const char* const Profiles [16]     = {
        "n.a", "Unstable/Experimental", unk, unk, unk, "below Telephone", "below Telephone", "Telephone",
        "Thumb", "Radio", "Standard", "Xtreme", "Insane", "BrainDead", "above BrainDead", "above BrainDead"
    };

    stderr_printf ( "\n"
                    " encoding file '%s'\n"
                    "       to file '%s'\n"
                    "\n"
                    " SV %u.%u, Profile '%s'\n",
                    inDatei, outDatei, MAJOR_SV, MINOR_SV, Profiles [MainQual] );

    if ( verbose > 0 ) {
        stderr_printf ( "\n" );
        if ( FadeInTime != 0.  ||  FadeOutTime != 0.  ||  verbose > 1 )
            stderr_printf ( " PCM fader                : fade-in: %.2f s, fade-out: %.2f s, shape: %g\n", FadeInTime, FadeOutTime, FadeShape );
        if ( ScalingFactor[0] != 1.  ||  ScalingFactor[1] != 1.  ||  verbose > 1 )
            stderr_printf ( " Scaling input by         : left %.5f, right: %.5f\n", ScalingFactor[0], ScalingFactor[1] );
        stderr_printf ( " Maximum encoded bandwidth: %4.1f kHz\n", (MaxSubBand+1) * (SampleFreq/32./2000.) );
        stderr_printf ( " Adaptive Noise Shaping   : max. %s order\n", th [Max_ANS_Order] );
        stderr_printf ( " Clear Voice Detection    : %s\n", able [CVD_used] );
        stderr_printf ( " Mid/Side Stereo          : %s\n", stereo [MS_Channelmode] );
        stderr_printf ( " Threshold of Hearing     : Model: %s (%u), Max ATH: %2.0f dB, Offset: %+1.0f dB, +Offset@20kHz:%1.0f dB\n",
                        Ltq_model < sizeof(EarModel)/sizeof(*EarModel) ? EarModel [Ltq_model] : unk,
                        Ltq_model,
                        Ltq_max,
                        Ltq_offset,
                        Ltq_20kHz );
        if ( NMT !=  6.5 || verbose > 1 )
            stderr_printf ( " Noise masks Tone Ratio   : %4.1f dB\n", NMT );
        if ( TMN != 18.0 || verbose > 1 )
            stderr_printf ( " Tone masks Noise Ratio   : %4.1f dB\n", TMN );
        if ( PNS > 0 )
            stderr_printf ( " PNS Threshold            : %4.2f\n", PNS );
        if ( !tmpMask_used )
            stderr_printf ( " No exploitation of temporal post masking\n" );
        else if ( verbose > 1 )
            stderr_printf ( " Exploitation of temporal post masking\n" );
        if ( minSMR > 0. )
            stderr_printf ( " Minimum Signal-to-Mask   : %4.1f dB\n", minSMR );
        else if ( verbose > 1 )
            stderr_printf ( " No minimum SMR (psycho model controlled filtering)\n" );
        if ( DelInput == 0xAFFEDEAD )
            stderr_printf ( " Deleting input file after encoding\n" );
        else if ( verbose > 1 )
            stderr_printf ( " No deleting of input file after encoding\n" );
    }
    stderr_printf ( "\n" );
}


/*
 *  Print out the time to stderr with a precision of 10 ms always using
 *  12 characters. Time is represented by the sample count. An additional
 *  prefix character (normally ' ' or '-') is prepended before the first
 *  digit.
 */

static const char*
PrintTime ( UintMax_t samples, int sign )
{
    static char  ret [32];
    Ulong        tmp  = (Ulong) ( UintMAX_FP(samples) * 100. / SampleFreq );
    Uint         hour = (Uint) ( tmp /360000       );
    Uint         min  = (Uint) ( tmp /  6000 %  60 );
    Uint         sec  = (Uint) ( tmp /   100 %  60 );
    Uint         csec = (Uint) ( tmp         % 100 );

    if ( UintMAX_FP(samples) >= SampleFreq * 360000. )
        return "            ";
    else if ( hour > 9 )
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


static void
ShowProgress ( UintMax_t  samples,
               UintMax_t  total_samples,
               UintMax_t  WrittenBytes )
{
    static clock_t  start;
    clock_t         curr;
    float           percent;
    float           kbps;
    float           speed;
    float           total_estim;

    if ( samples == 0 ) {
        if ( DisplayUpdateTime >= 0 ) {
            stderr_printf ("    %%|avg.bitrate| speed|play time (proc/tot)| CPU time (proc/tot)| ETA\n"
                            "  -.-    -.- kbps  -.--x     -:--.-    -:--.-     -:--.-    -:--.-     -:--.-\r" );
        }
        start = clock ();
        return;
    }
    curr    = clock ();
    if ( (unsigned long)(curr - start) == 0 )
        return;

    percent     = 100. * UintMAX_FP(samples) / UintMAX_FP(total_samples);
    kbps        = 8.e-3 * UintMAX_FP(WrittenBytes) * SampleFreq / UintMAX_FP(samples);
    speed       = UintMAX_FP(samples) * (CLOCKS_PER_SEC/SampleFreq) / (unsigned long)(curr - start) ;
    total_estim = UintMAX_FP(total_samples) / UintMAX_FP(samples) * (unsigned long)(curr - start);

    // progress percent
    if ( total_samples < IntMax_MAX )
        stderr_printf ("\r%5.1f ", percent );
    else
        stderr_printf ("\r      " );

    // average data rate
    stderr_printf ( "%6.1f kbps ", kbps );

    // encoder speed
    stderr_printf ( "%5.2fx ", speed );

    // 2x duration in WAVE file time (encoded/total)
    stderr_printf ("%10.10s" , PrintTime (samples      , ' ')+1 );
    stderr_printf ("%10.10s ", PrintTime (total_samples, ' ')+1 );

    // 2x coding time (encoded/total)
    stderr_printf ("%10.10s" , PrintTime ((curr - start) * (SampleFreq/CLOCKS_PER_SEC), ' ')+1 );
    stderr_printf ("%10.10s ", PrintTime ( total_estim   * (SampleFreq/CLOCKS_PER_SEC), ' ')+1 );

    // ETA
    stderr_printf ( "%10.10s\r", samples < total_samples  ?  PrintTime ((total_estim - curr + start) * (SampleFreq/CLOCKS_PER_SEC), ' ')+1  :  "" );
    fflush ( stderr );

    if ( WIN32_MESSAGES  &&  FrontendPresent )
        SendProgressMessage ( kbps, speed, percent );
}
