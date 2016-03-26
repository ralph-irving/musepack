/*
 *  Clear Vocal Detection functions
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

/*
 *  Priority is scaled from 0...100 including 0 and 100.
 */

#include "mppdec.h"

#if   defined __TURBOC__

int
SetPriority ( unsigned int prio )
{
    return 0;
}

void
DisableSUID ( void )
{
}

void
EnableSUID ( void )
{
}


#elif defined _WIN32

#ifndef _CONCAT2
# define _CONCAT2(x,y)    x##y
#endif

#define SETP( pclass, threadp )                                     \
    SetPriorityClass  ( pid, _CONCAT2 (pclass,_PRIORITY_CLASS  ) ); \
    SetThreadPriority ( tid, _CONCAT2 (THREAD_PRIORITY_,threadp) )


int
SetPriority ( unsigned int prio )
{
    HANDLE  pid = GetCurrentProcess ();
    HANDLE  tid = GetCurrentThread  ();

    switch ( prio * 30 / 100 ) {
    case  0:
    case  1:
        SETP ( IDLE, IDLE              ); break;
    case  2:
    case  3:
        SETP ( IDLE, LOWEST            ); break;
    case  4:
    case  5:
        SETP ( IDLE, BELOW_NORMAL      ); break;
    case  6:
    case  7:
        SETP ( IDLE, NORMAL            ); break;
    case  8:
    case  9:
        SETP ( IDLE, ABOVE_NORMAL      ); break;
    case 10:
    case 11:
        SETP ( IDLE, HIGHEST           ); break;

    case 12:
    case 13:
        SETP ( NORMAL, LOWEST          ); break;
    case 14:
        SETP ( NORMAL, BELOW_NORMAL    ); break;
    default:
    case 15:
        SETP ( NORMAL, NORMAL          ); break;
    case 16:
        SETP ( NORMAL, ABOVE_NORMAL    ); break;
    case 17:
        SETP ( NORMAL, HIGHEST         ); break;

    case 18:
        SETP ( HIGH, LOWEST            ); break;
    case 19:
        SETP ( HIGH, BELOW_NORMAL      ); break;
    case 20:
        SETP ( HIGH, NORMAL            ); break;
    case 21:
        SETP ( HIGH, ABOVE_NORMAL      ); break;
    case 22:
        SETP ( HIGH, HIGHEST           ); break;
    case 23:
        SETP ( HIGH, TIME_CRITICAL     ); break;

    case 24:
        SETP ( REALTIME, IDLE          ); break;
    case 25:
        SETP ( REALTIME, LOWEST        ); break;
    case 26:
        SETP ( REALTIME, BELOW_NORMAL  ); break;
    case 27:
        SETP ( REALTIME, NORMAL        ); break;
    case 28:
        SETP ( REALTIME, ABOVE_NORMAL  ); break;
    case 29:
        SETP ( REALTIME, HIGHEST       ); break;
    case 30:
        SETP ( REALTIME, TIME_CRITICAL ); break;
    }

    return 0;
}

void
DisableSUID ( void )
{
}

void
EnableSUID ( void )
{
}

#else

# include <errno.h>

static uid_t
GetUID ( void )
{
#  if defined _HPUX_SOURCE
    uid_t  user;
    gid_t  group;
    uid_t  saved;

    getresuid ( &user, &group, &saved );
    return user;
#  else
    return getuid ();
#  endif
}

static void
SetEUID ( uid_t uid )
{
#  if defined _HPUX_SOURCE
    setresuid (-1, uid, -1);
#  else
    seteuid (uid);
#  endif
}

void
DisableSUID ( void )
{
    SetEUID ( GetUID () );
}

void
EnableSUID ( void )
{
    SetEUID (0);
}

int
SetPriority ( unsigned int prio )
{
# if defined USE_REALTIME               // geht f¸r alle POSIX 1b-konformen Systeme, auﬂerdem sollte Speicher noch gelockt werden
    struct sched_param  sp;
    int                 ret;
    int                 err;

    memset      ( &sp, 0, sizeof sp );

    if ( prio >= 99 ) {
        EnableSUID  ();
        sp.sched_priority = sched_get_priority_min ( SCHED_FIFO );  // das mal ansehen, auﬂerdem goodness()
        ret               = sched_setscheduler ( 0, SCHED_RR, &sp );
        err               = errno;
        DisableSUID ();
    }
    if ( prio <=  0 ) {
        EnableSUID  ();
        //sp.sched_priority = sched_get_priority_min ( SCHED_FIFO );
        //ret               = sched_setscheduler ( 0, SCHED_IDLE, &sp );
        //err               = errno;
        DisableSUID ();
    }
# endif

# if defined USE_NICE
    EnableSUID  ();
    setpriority ( PRIO_PROCESS, getpid (), 20 - prio * 2 / 5 );
    DisableSUID ();
# endif

    return 0;
}

#endif

/* end of priority.c */
