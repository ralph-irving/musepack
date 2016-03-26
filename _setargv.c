#include "mppdec.h"

#ifndef _WIN32
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <dirent.h>
# include <ctype.h>
#else
# include <stdlib.h>
#endif

/*
 *  Function type which is called by treewalk.
 *  The function gets the next filename and an auxillary pointer.
 *  The function and the auxillary pointer are the last two parameters
 *  of treewalk. The function do the desired stuff and often you
 *  need a context for this function and for that this pointer can be used.
 */

typedef int (*leaffn) ( const char* filename, void* aux );


/*
 *  Array structure:
 *      length:  maximum number of elements you can store in the structure
 *      elems:   actual number of elements stored in the structure (elem <= length)
 *      array:   elements stored in the structure
 */

typedef struct {
    size_t    length;
    size_t    elems;
    char**    array;
} argc_t;


/*
 *  Check the 'filename' for the extention 'ext'.
 *  Returns 1 if filename has this extention, otherwise 0.
 *  Allowed wildcard in ext is currently the '?'.
 */

static int
extcompare ( const char* filename, const char* ext )
{
    if ( strlen (filename) < strlen (ext) )
        return 0;

    filename += strlen (filename) - strlen (ext);
    for ( ; *ext; ext++, filename++ ) {
        if ( *ext != '?' )
            if ( *ext != *filename )
                return 0;
    }
    return 1;
}


/*
 *  Walk trough a tree starting with 'start', searching for files with the extentions
 *  mask[0], mask[1], ... (NULL terminated list of extentions containing the ".").
 *  For each file 'f' is called with the filename and the parameter 'aux'.
 */

#ifdef _WIN32

long
treewalk ( const char* start, const char** mask, leaffn f, void* aux )
{
    struct _finddata_t  fileinfo;
    long                handle;
    char                all [4096];
    int                 i;
    long                ret = 0;
    const char*         name;
    int                 path_len = strlen (start);

    if ( path_len > 0  &&  start[path_len-1] == PATH_SEP )
        path_len--;

    sprintf ( all, "%.*s%c*", path_len, start, PATH_SEP );
    if ( ( handle = _findfirst ( all, &fileinfo ) ) < 0 )
        return 0;

    do {
        name = fileinfo.name;
        sprintf ( all, "%.*s%c%s", path_len, start, PATH_SEP, name );
        if ( ( fileinfo.attrib & _A_SUBDIR ) == 0 ) {   // file
            for ( i = 0; mask[i]; i++ )
                if ( extcompare (name, mask[i] ) ) {
                    f (all, aux);
                    ret++;
                    break;
                }
        }
        else {                                          // subdir
            if ( 0 != strcmp (name, "." )  &&  0 != strcmp (name, "..") ) {
                ret += treewalk ( all, mask, f, aux );
            }
        }

    } while ( 0 == _findnext ( handle, &fileinfo) );

    _findclose (handle);
    return ret;
}

#else

long
treewalk ( const char* start, const char** mask, leaffn f, void* aux )
{
    DIR*                handle;
    struct dirent*      de;
    struct stat         b;
    char                all [4096];
    int                 i;
    long                ret = 0;
    const char*         name;
    int                 path_len = strlen (start);

    if ( path_len > 0  &&  start[path_len-1] == PATH_SEP )
        path_len--;

    if ( ( handle = opendir (start) ) == NULL )
         return 0;

    while ( NULL != ( de = readdir (handle) ) ) {
        name = de -> d_name;
        sprintf ( all, "%.*s%c%s", path_len, start, PATH_SEP, name );

        if ( stat ( all, &b ) != 0 )
            continue;

        if     ( S_ISREG (b.st_mode) ) {                // file
            for ( i = 0; mask[i]; i++ )
                if ( extcompare (name, mask[i] ) ) {
                    f (all, aux);
                    ret++;
                    break;
                }
        }
        else if ( S_ISDIR (b.st_mode) ) {               // dir
            if ( 0 != strcmp (name, "." )  &&  0 != strcmp (name, "..") ) {
                ret += treewalk ( all, mask, f, aux );
            }
        }
    }

    closedir (handle);
    return ret;
}

#endif

/*
 *  A leaffn used by mysetargv(). aux is used as pointer to a argc_t structure,
 *  which is used for collecting all filenames in a list.
 */

static int
add_elem ( const char* filename, void* aux )
{
    argc_t*  p = (argc_t*) aux;

    if ( p->elems >= p->length ) {
        p->length += p->length / 2 + 512;
        p->array = realloc ( p->array, sizeof(char*) * p->length );
    }

    p->array [p->elems++] = filename  ?  strdup (filename)  :  NULL;
    // printf ("%s\n", filename );
    return 0;
}

/*
 *  Used by mysetargv to sort files by the filename in ascending order.
 */

static int Cdecl
comparefilename ( const void* p1, const void* p2 )
{
    return strcmp ( *(const char**)p1, *(const char**)p2 );
}


/*
 * Other playlist formats I found:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  *.txt
 *    |Location|Name|Artist|Album|Length|Genre|Year|Type|Size|Comment|Bitrate|Track #|Date|Custom 1|Custom 2|Custom 3|
 *    |D:\Audio\Video Streams\A Bug's Life.avi|A Bug's Life|||01:31:02|||avi|647.391MB||||7/3/2001 9:21:18 AM||||
 *
 * *.mpl:
 *    <Item Location="D:\Audio\Bohinta\[08] broken_one.mp3" Type="mp3">
 *
 * *.htm*:
 *    <TD ALIGN="left" ><FONT SIZE="2" COLOR="#000000" FACE="Arial">D:\Audio\Bohinta\[09] lament.mp3</FONT></TD>
 *
 */


/*
 *  Read out all files in a M3U file.
 *  Relative file names are converted to absolute filenames
 *  using the path of the M3U file.
 *  'f' and 'aux' have the same meaning as in treewalk.
 *  'fp' is a descriptor to the source file, name is it's name
 *  (used for converting relative to absolute file names).
 */

static void
readm3u ( FILE_T fp, const char* name, leaffn f, void* aux )
{
    unsigned char   ch;
    unsigned char*  p;
    unsigned char*  q = strrchr ( name, PATH_SEP );
    unsigned char   buff [4096];

    if (q == NULL) {
        q = buff;
    }
    else {
        memcpy ( buff, name, (char*)q-(char*)name+1 );
        q = buff + ((char*)q-(char*)name+1);
    }

    while ( 1 == READ1 (fp, &ch) ) {
        if ( ch != '#'  &&  ch >= ' ' ) {
            p = q;
            do {
                *p++ = ch;
                if ( 1 != READ1 (fp, &ch)  ||  ch < ' ' )
                    break;
            } while (1);

            *p = '\0';
            if ( q[1] == DRIVE_SEP  ||  q[0] == PATH_SEP )              // Das ist ein ekliger Hack
                f (q, aux );
            else
                f (buff, aux);
        }

        while ( ch != '\n' )
            if ( 1 != READ1 (fp, &ch) )
                return;
    }
}


/*
 *  mysetargv() is the interface function for this module.
 *  You give the original _argv[_argc] list and a list of allowed file
 *  extentions for the directory search (this is a NULL terminated
 *  list of file extentions including the dot ".").
 *
 *  The function generates a new _argv[_argc] list and stores it
 *  parameters back to callers variables.
 *  Normal files are simple copied ti the new list, files ending
 *  with the directory separator ("/" or "\") will be treated as
 *  directory trees and all files with allowed extentions are
 *  added to the list (instead of the directory). Files ending with
 *  ".m3u" will be read and the contents is added to the list.
 *
 *  What do not work?
 *     - m3u files inside a m3u file
 *     - playlist files in *.txt format
 *     - playlist files in *.mpl format
 *     - playlist files in *.htm/*.html format
 */

void
mysetargv ( int* _argc, char*** _argv, const char** extentions )
{
    const char*  m3u = ".m3u";
    argc_t       arg;
    char*        p;
    int          i;
    size_t       j;
    FILE_T       fp;

    arg.length = 0;
    arg.elems  = 0;
    arg.array  = NULL;

    for ( i = 0; i < *_argc; i++ ) {
        p = (*_argv) [i];
        if ( strlen(p) >= strlen(m3u)  &&  0 == strcasecmp (p+strlen(p)-strlen(m3u), m3u) ) {
            fp = OPEN (p);
            readm3u ( fp, p, add_elem, &arg );
            CLOSE (fp);
        }
        else if ( p[0] == '\0'  ||  p[strlen(p)-1] != PATH_SEP ) {
            add_elem ( p, &arg );
        }
        else {
            j = arg.elems;
            treewalk ( p, extentions, add_elem, &arg );
            qsort ( arg.array + j, arg.elems - j, sizeof(char*), comparefilename );
        }
    }

    add_elem ( NULL, &arg );
    arg.array = realloc ( arg.array, sizeof(char*) * arg.elems );

    *_argc = arg.elems - 1;
    *_argv = arg.array;
    return;
}

/* end of _setargv.c */
