#include <string.h>
#include "mppdec.h"

/*
 *  List of known Genres. 256 genres are possible with version 1/1.1 tags,
 *  but not yet used.
 */

static const char*  GenreList [] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
    "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
    "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
    "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative",
    "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
    "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
    "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap",
    "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
    "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
    "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll",
    "Hard Rock", "Folk", "Folk/Rock", "National Folk", "Swing", "Fast-Fusion",
    "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
    "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock",
    "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour",
    "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony",
    "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam", "Club",
    "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul",
    "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A capella", "Euro-House",
    "Dance Hall", "Goa", "Drum & Bass", "Club House", "Hardcore", "Terror",
    "Indie", "BritPop", "NegerPunk", "Polsk Punk", "Beat", "Christian Gangsta",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary C",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "SynthPop"
};


/*
 *  Copies src to dst. Copying is stopped at `\0' char is detected or if
 *  len chars are copied.
 *  Trailing blanks are removed and the string is `\0` terminated.
 */

static void
memcpy_crop ( char* dst, const char* src, size_t len )
{
    size_t  i;

    for ( i = 0; i < len; i++ )
        if  ( src[i] != '\0' )
            dst[i] = src[i];
        else
            break;

    // dst[i] points behind the string contents

    while ( i > 0  &&  dst [i-1] == ' ' )
        i--;
    dst [i] = '\0';
}


/*
 *  Evaluate ID version 1/1.1 tags of a file given by 'fp' and fills out Tag
 *  information in 'tip'. Tag information also contains the effective file
 *  length (without the tags if tags are present). Return 1 if there is
 *  usable information inside the file. Note there's also the case possible
 *  that the file contains empty tags, the the file size is truncated by the
 *  128 bytes but the function returns 0.
 *
 *  If there're no tags, all strings containing '\0', the Genre pointer is
 *  NULL and GenreNo and TrackNo are -1.
 */

Int
Read_ID3V1_Tags ( FILE_T fp, TagInfo_t* tip )
{
    Uint8_t      tmp [128];
    OFF_T        file_pos;
    const char*  q;

    memset ( tip, 0, sizeof(*tip) );
    tip->GenreNo = -1;
    tip->TrackNo = -1;

    if ( -1 == (file_pos = FILEPOS (fp)) )
        return 0;
    if ( -1 == SEEK ( fp, -128L, SEEK_END ) )
        return 0;

    tip->FileSize = FILEPOS (fp);
    if ( 128 != READ ( fp, tmp, 128 ) )
        return 0;
    SEEK ( fp, file_pos, SEEK_SET );

    if ( 0 != memcmp ( tmp, "TAG", 3 ) ) {
        tip->FileSize += 128;
        return 0;
    }

    if ( !tmp[3]  &&  !tmp[33]  &&  !tmp[63]  &&  !tmp[93]  &&  !tmp[97] )
        return 0;

    memcpy_crop  ( tip->Title   = realloc (tip->Title  , 31), tmp +  3, 30 );
    memcpy_crop  ( tip->Artist  = realloc (tip->Artist , 31), tmp + 33, 30 );
    memcpy_crop  ( tip->Album   = realloc (tip->Album  , 31), tmp + 63, 30 );
    memcpy_crop  ( tip->Year    = realloc (tip->Year   ,  5), tmp + 93,  4 );
    memcpy_crop  ( tip->Comment = realloc (tip->Comment, 31), tmp + 97, 30 );

    q = tmp[127] < sizeof(GenreList)/sizeof(*GenreList)  ?
                         GenreList [tip->GenreNo = tmp[127]]  :  "???";
    strcpy ( tip->Genre = realloc (tip->Genre, strlen(q)+1), q );

    // Index 0 may be true if file is very short
    tip->Track = realloc (tip->Track, 6);
    if ( tmp[125] == 0  &&  (tmp[126] != 0  ||  tip->FileSize < 66000 ) )
        sprintf ( tip->Track, "[%02d]", tip->TrackNo = tmp[126] );
    else
        strcpy ( tip->Track, "    " );

    return 1;
}


struct APETagFooterStruct {
    Uint8_t   ID       [8];    // should equal 'APETAGEX'
    Uint8_t   Version  [4];    // currently 1000 (version 1.000)
    Uint8_t   Length   [4];    // the complete size of the tag, including this footer
    Uint8_t   TagCount [4];    // the number of fields in the tag
    Uint8_t   Flags    [4];    // the tag flags (none currently defined)
    Uint8_t   Reserved [8];    // reserved for later use
};


static Uint32_t
Read_LE_Uint32 ( const Uint8_t* p )
{
    return ((Uint32_t)p[0] <<  0) |
           ((Uint32_t)p[1] <<  8) |
           ((Uint32_t)p[2] << 16) |
           ((Uint32_t)p[3] << 24);
}


#define TAG_ANALYZE(item,elem)                      \
    if ( 0 == memcmp (p, #item, sizeof #item ) ) {  \
        p += sizeof #item;                          \
        tip->elem = realloc (tip->elem, len+1);     \
        memcpy ( tip->elem, p, len );               \
        tip->elem [len] = '\0';                     \
        p += len;                                   \
    } else


Int
Read_APE_Tags ( FILE_T fp, TagInfo_t* tip )
{
    OFF_T                      file_pos;
    Uint32_t                   len;
    Uint32_t                   flags;
    unsigned char              buff [8192];
    unsigned char*             p;
    unsigned char*             end;
    struct APETagFooterStruct  T;
    Uint32_t                   TagLen;
    Uint32_t                   TagCount;
    Uint32_t                   tmp;

    memset ( tip, 0, sizeof(*tip) );
    tip->GenreNo = -1;
    tip->TrackNo = -1;

    if ( -1 == (file_pos = FILEPOS (fp)) )
        goto notag;
    if ( -1 == SEEK ( fp, 0L, SEEK_END ) )
        goto notag;
    tip->FileSize = FILEPOS (fp);
    if ( -1 == SEEK ( fp, -(long)sizeof T, SEEK_END ) )
        goto notag;
    if ( sizeof(T) != READ ( fp, &T, sizeof T ) )
        goto notag;
    if ( memcmp ( T.ID, "APETAGEX", sizeof(T.ID) ) != 0 )
        goto notag;
    tmp = Read_LE_Uint32 (T.Version);
    if (  tmp != 1000  &&  tmp != 2000 )
        goto notag;
    TagLen = Read_LE_Uint32 (T.Length);
    if ( TagLen <= sizeof T )
        goto notag;
    if ( -1 == SEEK ( fp, -(long)TagLen, SEEK_END ) )
        goto notag;
    tip->FileSize = FILEPOS (fp);
    memset ( buff, 0, sizeof(buff) );
    if ( TagLen - sizeof T != READ ( fp, buff, TagLen - sizeof T ) )
        goto notag;
    SEEK ( fp, file_pos, SEEK_SET );

    TagCount = Read_LE_Uint32 (T.TagCount);
    end = buff + TagLen - sizeof T;
    for ( p = buff; p < end  &&  TagCount--; ) {
        len   = Read_LE_Uint32 ( p ); p += 4;
        flags = Read_LE_Uint32 ( p ); p += 4;
        TAG_ANALYZE ( Title  , Title   )
        TAG_ANALYZE ( Album  , Album   )
        TAG_ANALYZE ( Artist , Artist  )
        TAG_ANALYZE ( Album  , Album   )
        TAG_ANALYZE ( Comment, Comment )
        TAG_ANALYZE ( Track  , Track   )
        TAG_ANALYZE ( Year   , Year    )
        TAG_ANALYZE ( Genre  , Genre   )
        {
            p += strlen(p) + 1 + len;
        }
    }

    if ( tip->Track != NULL  &&  tip->Track[0] != '\0' ) {
        tip->TrackNo = atoi (tip->Track);
        tip->Track   = realloc (tip->Track, 13 );
        sprintf ( tip->Track, "[%02d]", tip->TrackNo );
    }
    else {
        tip->Track   = realloc (tip->Track, 5 );
        strcpy ( tip->Track, "    " );
    }

    /* Genre wird noch nicht ausdekodiert */
    return 1;

notag:
    SEEK ( fp, file_pos, SEEK_SET );
    return 0;
}

/* end of id3tag.c */
