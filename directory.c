/*
 *  System specific directory functions
 *
 *  (C) Frank Klemm 2001, Janne Hyvärinen 2002. All rights reserved.
 *
 *  Principles:
 *
 *  History:
 *    2002-08-15    created
 *
 *  Global functions:
 *    - IsDirectory
 *
 *  TODO:
 *    -
 */

#if defined _WIN32  ||  defined __TURBOC__
# include <io.h>
# include <sys/stat.h>
#else
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif


#ifndef S_ISDIR
# if   defined S_IFDIR
#  define S_ISDIR(x)            ((x) &   S_IFDIR)
# elif defined _S_IFDIR
#  define S_ISDIR(x)            ((x) &  _S_IFDIR)
# elif defined __S_IFDIR
#  define S_ISDIR(x)            ((x) & __S_IFDIR)
# else
#  error Cannot find a way to test for a directory
# endif
#endif


#ifdef _WIN32

/*
int
IsDirectory ( const char* path )
{
    struct _finddata_t  finddata;
    long                hFile;
    int                 ret = 0;

    if ( ( hFile = _findfirst (path, &finddata) ) != -1L ) {
        if ( finddata.attrib & _A_SUBDIR )
            ret = 1;
        _findclose ( hFile );
    }
    return ret;
}
*/

int
IsDirectory ( const char* path )
{
    struct _stat  st;

    if ( _stat ( path, &st ) != 0 )
        return 0;
    return S_ISDIR ( st.st_mode );
}

#else

int
IsDirectory ( const char* path )
{
    struct stat  st;

    if ( stat ( path, &st ) != 0 )
        return 0;
    return S_ISDIR ( st.st_mode );
}

#endif

/* end of directory.c */
