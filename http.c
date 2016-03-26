/*
 *   http.c
 *   Adapted from: Oliver Fromme <oliver.fromme@heim3.tu-clausthal.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "mppdec.h"


#ifdef USE_HTTP

# define ACCEPT_HEAD  "Accept: audio/mpeg, audio/x-mpegurl, */*\r\n"
# define PROGRAMNAME  "Mozilla/4.72P4"
# define BUFFERLEN    1024


typedef Uint32_t  IP_t;
typedef Uint16_t  port_t;


# ifdef ZEISS_PROXY
char*         proxyurl       = "kdejenspi01.zeiss.de";
IP_t          proxyip        = 0;
char*         proxyport      = "8080";
char*         proxyuser      = "zjfkl";
char*         proxypasswd    = "hantel4";
# else
char*         proxyurl       = NULL;
IP_t          proxyip        = 0;
char*         proxyport      = NULL;
char*         proxyuser      = NULL;
char*         proxypasswd    = NULL;
# endif /* ZEISS_PROXY */
char*         httpauth       = NULL;
char          httpauth1 [256];
static char*  defaultportstr = "80";


# ifndef _WIN32
#  include <netdb.h>
#  include <sys/param.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
# else
#  include <sys/types.h>
# endif /* _WIN32 */

# ifndef INADDR_NONE
#  define INADDR_NONE  (IP_t)(-1)
# endif

#ifdef _WIN32

static int
Init_WinSocket ( void )
{
    WORD     VersionRequested;
    WSADATA  wsaData;
    int      err;

    VersionRequested = MAKEWORD (2, 2);

    err = WSAStartup ( VersionRequested, &wsaData );
    if ( err != 0 ) {                                     // Tell the user that we could not find a usable WinSock DLL
        stderr_printf ("Can't find WinSock DLL\n");
        return -1;
    }

    // Confirm that the WinSock DLL supports 2.2.
    // Note that if the DLL supports versions greater than 2.2 in addition to 2.2,
    // it will still return 2.2 in Version since that is the version we requested.

    if ( LOBYTE (wsaData.wVersion)  != 2  ||  HIBYTE (wsaData.wVersion) != 2 ) {
        // Tell the user that we could not find a usable WinSock DLL.
        stderr_printf ("Wrong version of WinSock DLL: %d.%d\n", HIBYTE (wsaData.wVersion), LOBYTE (wsaData.wVersion) );
        WSACleanup ();
        return -1;
    }
    return 0;
}

#endif

static char*
secure_calloc ( size_t size )
{
    char*  p = (char*) calloc ( size, 1 );

    if ( p == NULL ) {
        stderr_printf ("\n"PROG_NAME": Out of memory, aborting...\n");
        _exit (1);
    }
    return p;
}


static int
writestring ( int fd, const char* string )
{
    int     result;
    size_t  bytes = strlen (string);

    while ( bytes > 0 ) {
        if ( (result = WRITE_SOCKET (fd, string, bytes)) < 0  &&  errno != EINTR ) {
            stderr_printf ("\n"PROG_NAME": write to socket: %s\n", strerror (errno) );
            return -1;
        }
        else if (result == 0) {
            stderr_printf ("\n"PROG_NAME": write to socket: %s\n", "socket closed unexpectedly");
            return -1;
        }
        string += result;
        bytes  -= result;
    }
    return 0;
}

static int
readstring ( char* string, size_t maxlen, int fd )
{
    size_t  pos = 0;

    while (1) {
        if ( READ_SOCKET ( fd, string + pos, 1) == 1 ) {
            if ( string [pos++] == '\n' ) {
                string [pos]   = '\0';
                return 0;
            } else if ( pos+2 >= maxlen ) {
                string [pos++] = '\n';
                string [pos]   = '\0';
                return 0;
            }
        }
        else if ( errno != EINTR ) {
            stderr_printf ("\n"PROG_NAME": read from socket: Error reading from socket or unexpected EOF\n");
            return -1;
        }
    }
    return 0;
}


static void
encode64 ( const unsigned char* src, char* dst )
{
    static const char  Base64Digits [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int                ssiz            = strlen (src);
    int                i;
    Uint32_t           buf;

    for ( i = 0 ; i < ssiz ; i += 3 ) {
        buf      = src [i+0] << 16;
        if ( i+1 < ssiz )
            buf |= src [i+1] <<  8;
        if ( i+2 < ssiz )
            buf |= src [i+2] <<  0;

        *dst++ =                Base64Digits [(buf >> 18) & 63];
        *dst++ =                Base64Digits [(buf >> 12) & 63];
        *dst++ = i+1 < ssiz  ?  Base64Digits [(buf >>  6) & 63]  :  '=';
        *dst++ = i+2 < ssiz  ?  Base64Digits [(buf >>  0) & 63]  :  '=';
    }
    *dst++ = '\0';
    return;
}


/*
 *  Extracts a user name from URL while removing the user name from the string.
 *  Input  has the form "http://user@..."
 *  Output has the form "http://..."  and "user" is copied to auth
 */

static int
getauthfromURL ( char* url, char* auth )  /* VERY simple auth-from-URL grabber */
{
    char*  pos;
    int    i;

    *auth = '\0';

    if      ( 0 == strncasecmp (url, "http://", 7) )
        url += 7;
    else if ( 0 == strncasecmp (url, "ftp://" , 6) )
        url += 6;

    if ( (pos = strchr (url, '@')) != NULL ) {
        for ( i = 0; i < pos-url; i++ ) {
            if ( url [i] == '/' )
                return 0;
        }
        strncpy ( auth, url, pos-url );
        auth [pos - url] = '\0';
        strcpy ( url, pos+1 );
        return 1;
    }
    return 0;
}


static char*
url2hostport ( char* url, char** hostname, IP_t* hip, char** port )
{
    char*   h;
    char*   p;
    char*   hostptr;
    char*   r_hostptr;
    char*   pathptr;
    char*   portptr;
    char*   p0;
    size_t  stringlength;

    p = url;
    if       ( 0 == strncasecmp (p, "http://", 7) )
        p += 7;
    else  if ( 0 == strncasecmp (p, "ftp://" , 6) )
        p += 6;
    hostptr = p;                                        // hostptr points to the hostname, usually a "www..." or "ftp...."

    while ( *p != '\0'  &&  *p != '/' )
        p++;
    pathptr = p;                                        // pathptr points to the first '/' after the hostname, usually a "/pub/..."

    r_hostptr = --p;
    while ( p > hostptr  &&  *p != ':'  &&  *p != ']' ) // ']' for IPv6, IPv6 is separated by ':', so the host is ecapsulated by "[]", i.e. "http://[1080::8:800:200C:417A]:80/"
        p--;

    if ( p == hostptr  ||  *p != ':' ) {                // "http:/1080::8:800:200C:417A/..." is wrong parsed
        portptr   = NULL;
    }
    else {
        portptr   = p + 1;
        r_hostptr = p - 1;
    }

    if ( *hostptr == '['  &&  *r_hostptr == ']' ) {
        hostptr  ++;
        r_hostptr--;
    }

    stringlength = r_hostptr - hostptr + 1;
    h = secure_calloc ( stringlength + 1 );
    memcpy ( h, hostptr, stringlength );
    *hostname = h;

    if ( portptr != NULL ) {
        stringlength = pathptr - portptr;
        if (stringlength == 0)
            portptr = NULL;
    }

    if ( portptr == NULL ) {
        portptr      = defaultportstr;
        stringlength = strlen (defaultportstr);
    }

    p0 = secure_calloc (stringlength + 1);
    memcpy ( p0, portptr, stringlength );

    for ( p = p0; *p != '\0'  &&  isdigit (*p); p++ )
        ;

    *p    = '\0';
    *port = p0;

    return pathptr;
}


int
http_open ( const char* url )
{
    char*               purl;
    char*               hostname  = NULL;
    char*               request;
    char*               sptr;
    int                 linelength;
    IP_t                myip;
    char*               myport    = NULL;
    int                 sock;
    int                 relocate;
    int                 numrelocs = 0;
    int                 i;
    int                 j;
# ifdef USE_IPv4_6
    struct addrinfo     hints;
    struct addrinfo*    res;
    struct addrinfo*    res0;
    int                 error;
# else
    struct hostent*     hp;
    struct sockaddr_in  sin;
# endif

#ifdef _WIN32
    static int          init = 0;

    if ( init == 0  &&  Init_WinSocket () != 0 )
        return -1;
    init = 1;
#endif

    if ( strchr ( url, '/' ) == NULL )
        return -1;

    proxyport = NULL;

    if ( proxyip == 0L ) {
        if ( proxyurl == NULL )
            if ( (proxyurl = getenv ("MP3_HTTP_PROXY")) == NULL )
                if ( (proxyurl = getenv ("http_proxy")) == NULL )
                    proxyurl = getenv ("HTTP_PROXY");
        if ( proxyurl != NULL  &&  proxyurl[0]  &&  strcmp (proxyurl, "none") ) {
            if ( url2hostport (proxyurl, &hostname, &proxyip, &proxyport) == NULL ) {
                stderr_printf ("\n"PROG_NAME": Unknown proxy host \"%s\".\n", hostname ? hostname : "" );
                return -1;
            }
        }
        else {
            proxyip = INADDR_NONE;
        }
    }


    if ( proxyip == INADDR_NONE )
        if ( 0 == strncasecmp (url, "ftp://", 6) ) {
            stderr_printf ("\n"PROG_NAME": Downloading from ftp servers without PROXY not allowed\n" );
            return -1;
        }

    linelength = strlen (url) + BUFFERLEN;
    request    = secure_calloc (linelength);
    purl       = secure_calloc (BUFFERLEN);

    // copying url to purl while converting special characters
    i = j = 0;
    do {
        switch ( url[i] ) {
        case ' ': purl [j++] = '%', purl [j++] = '2', purl [j++] = '0'; break;
        default : purl [j++] = url [i];                                 break;
        }
    } while ( url [i++] != '\0' );

    getauthfromURL ( purl, httpauth1 );

    do {
        strcpy ( request, "GET ");
        if ( proxyip != INADDR_NONE ) {
            if ( strncasecmp (url, "http://", 7) != 0  &&  strncasecmp (url, "ftp://", 6) != 0 )
                strcat ( request, "http://");
            strcat ( request, purl);
            myport = proxyport;
            myip   = proxyip;
        }
        else {
            if ( hostname != NULL ) {
                free (hostname);
                hostname = NULL;
            }
            if ( proxyport != NULL ) {
                free (proxyport);
                proxyport = NULL;
            }
            if ( (sptr = url2hostport (purl, &hostname, &myip, &myport)) == NULL ) {
                stderr_printf ("\n"PROG_NAME": Unknown host \"%s\".\n", hostname ? hostname : "");
                return -1;
            }
            strcat ( request, sptr);
        }
        sprintf ( request + strlen (request), " HTTP/1.0\r\nUser-Agent: %s\r\n", PROGRAMNAME );
        if ( hostname != NULL )
            sprintf ( request + strlen(request), "Host: %s:%s\r\n", hostname, myport);

        strcat ( request, ACCEPT_HEAD);

# ifdef USE_IPv4_6

        memset ( &hints, 0, sizeof(hints) );
        hints.ai_socktype = SOCK_STREAM;
        error = getaddrinfo ( hostname, myport, &hints, &res0 );
        if ( error != 0 ) {
            stderr_printf ("\n"PROG_NAME": getaddrinfo: %s\n", gai_strerror (error) );
            return -1;
        }

        sock = -1;
        for ( res = res0; res != NULL; res = res->ai_next ) {
            if ((sock = socket (res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
                continue;

            if ( connect (sock, res->ai_addr, res->ai_addrlen) != 0 ) {
                close (sock);
                sock = -1;
                continue;
            }
            break;
        }

        freeaddrinfo (res0);

# else /* USE_IPv4_6 */

        if ( (hp = gethostbyname (hostname)) == NULL )
            goto fail;
        if ( hp->h_length != sizeof (sin.sin_addr) )
            goto fail;
        if ( (sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )) < 0 )
            goto fail;
        memset ( &sin, 0, sizeof(sin) );
        sin.sin_family = AF_INET;
        /* sin.sin_len = sizeof (struct sockaddr_in); */
        memcpy ( &sin.sin_addr, hp->h_addr, hp->h_length );
        sin.sin_port = htons ( (unsigned short) atoi (myport) );
        if ( connect ( sock, (struct sockaddr*)&sin, sizeof (struct sockaddr_in) ) < 0 ) {
            close (sock);
            goto fail;
        }

# endif /* USE_IPv4_6 */

        if ( sock < 0 ) {
            fail:
            stderr_printf ("\n"PROG_NAME": Could not open/connect socket: %s\n", strerror (errno) );
            return -1;
        }

        if ( strlen (httpauth1) > 0  ||  httpauth != NULL ) {
            char  buf [BUFFERLEN - 1];

            strcat ( request, "Authorization: Basic ");
            if ( strlen (httpauth1) > 0 )
                encode64 ( httpauth1, buf );
            else
                encode64 ( httpauth , buf );
            strcat ( request, buf );
            strcat ( request, "\r\n" );
        }
        strcat ( request, "\r\n" );

        writestring ( sock, request );
        *purl = '\0';
        readstring ( request, linelength-1, sock );
        relocate = 0;
        if ( (sptr = strchr ( request, ' ')) != NULL ) {
            switch ( sptr [1] ) {
            case '3':
                relocate = 1;
            case '2':
                break;
            default:
                stderr_printf ("\n"PROG_NAME": HTTP request failed:%s", sptr ); /* ' ' and '\n' is included */
                return -1;
            }
        }

        do {
            readstring ( request, linelength-1, sock );
            if ( 0 == strncmp ( request, "Location:", 9) )
                strncpy ( purl, request+10, BUFFERLEN - 1 );
        } while ( request [0] != '\r'  &&  request [0] != '\n' );

    } while ( relocate != 0  &&  purl[0] != '\0'  &&  numrelocs++ < 5 );

    if ( relocate ) {
        stderr_printf ("\n"PROG_NAME": Too many HTTP relocations.\n");
        return -1;
    }

    free (purl);
    free (request);
    free (hostname);
    free (proxyport);
    free (myport);

#ifdef _WIN32
    return sock + 0x4000;
#else
    return sock;
#endif
}

#endif /* USE_HTTP */

/* end of http.c */
