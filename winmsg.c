/*
 *
 *  Send messages to a MPC Dispatcher
 *
 *
 *  (C) Copyright 1999-2000  Andree Buschmann. All rights reserved.
 *  (C) Copyright 2001-2002  Frank Klemm. All rights reserved.
 *
 *  For licencing see the licencing file which is part of this package.
 *
 *
 *  Principles:
 *    Send messages about the internal state of the encoder which can be captured by the program mpcdispatcher.
 *
 *
 *  History:
 *    ca. 1998    created
 *    2001        added central messaging function SendMsg() which is called by all the other functions
 *
 *  Global functions:
 *    - SearchForFrontend()
 *    - SendStartupMessage()
 *    - SendQuitMessage()
 *    - SendModeMessage()
 *    - SendProgressMessage()
 *
 *  TODO:
 *    - nothing
 *    - Do the program which use these messages still exist?
 *
 */


#if defined _WIN32

#include <windows.h>

#include "mppenc.h"


static HWND  FrontEndHandle;


int
SearchForFrontend ( void )
{
    FrontEndHandle = FindWindow ( NULL, "mpcdispatcher" );      // check for dispatcher window and (send startup-message???)

    return FrontEndHandle != 0;
}


static void
SendMsg ( const char* s )
{
    COPYDATASTRUCT  MsgData;

    MsgData.dwData = 3;                                                                 // build up message
    MsgData.lpData = (char*) s;
    MsgData.cbData = strlen (s) + 1;

    SendMessage ( FrontEndHandle, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &MsgData );      // send message
}


void
SendStartupMessage ( const char* Version )
{
    char  startup [256];

    sprintf ( startup, "#START#%s#", Version );
    SendMsg ( startup );
}


void
SendQuitMessage ( void )
{
    SendMsg ("#EOF#");
}


void
SendModeMessage ( const int Profile )
{
    char  message [32];

    sprintf ( message, "#PARAM#%d#", Profile-8 );
    SendMsg ( message );
}


void
SendProgressMessage ( const int    bitrate,
                      const float  speed,
                      const float  percent )
{
    char  message [64];

    sprintf ( message, "#STAT#%4ik %5.2fx %5.1f%%#", bitrate, speed, percent );
    SendMsg ( message );
}

#endif /* _WIN32 */

/* end of winmsg.c */
