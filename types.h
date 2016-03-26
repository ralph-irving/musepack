
#ifndef MPPDEC_TYPES_H
#define MPPDEC_TYPES_H


#include <limits.h>


#if   defined __GNUC__
# define inline                 __inline__
# define restrict
#elif defined _WIN32
# define inline                 __inline
# define restrict
#else
# define inline
# define restrict
#endif



//// 'Cdecl' forces to use standard C/C++ calling convention ///////
#if   defined _WIN32
# define Cdecl           __cdecl
#elif defined __ZTC__
# define Cdecl           _cdecl
#elif defined __TURBOC__
# define Cdecl           cdecl
#else
# define Cdecl
#endif


#if   CHAR_BIT == 8  &&  SCHAR_MAX == 127L
typedef unsigned char       Uint8_t;    // guaranteed  8 bit unsigned integer type with range 0...255
typedef signed   char       Int8_t;     // guaranteed  8 bit signed   integer type with range -128...127
#else
# error No  8 bit int type found. Tested: char
#endif


#if   SHRT_MAX == 32767L
typedef unsigned short int  Uint16_t;   // guaranteed 16 bit unsigned integer type with range 0...65535
typedef signed   short int  Int16_t;    // guaranteed 16 bit signed   integer type with range -32768...32767
#else
# error No 16 bit int type found. Tested: short
#endif


#if   INT_MAX == 2147483647L
typedef unsigned int        Uint32_t;   // guaranteed 32 bit unsigned integer type with range 0...4294967295
typedef signed   int        Int32_t;    // guaranteed 32 bit signed   integer type with range -2147483648...2147483647
#elif LONG_MAX == 2147483647L
typedef unsigned long int   Uint32_t;   // guaranteed 32 bit unsigned integer type with range 0...4294967295
typedef signed   long int   Int32_t;    // guaranteed 32 bit signed   integer type with range -2147483648...2147483647
#else
# error No 32 bit int type found. Tested: int, long
#endif


#if    defined __C99__                 // C9x has a type which is exact 64 bit
typedef int64_t             Int64_t;
typedef uint64_t            Uint64_t;
typedef intmax_t            IntMax_t;
typedef uintmax_t           UintMax_t;
# define IntMax_MIN        -9223372036854775808
# define IntMax_MAX         9223372036854775807
# define UintMax_MAX       18446744073709551615
# define UintMAX_FP(x)      (long double)(x)
#elif  defined __GNUC__                // GCC uses long long as 64 bit
typedef signed   long long  Int64_t;
typedef unsigned long long  Uint64_t;
typedef signed   long long  IntMax_t;
typedef unsigned long long  UintMax_t;
# define IntMax_MIN        -9223372036854775808LL
# define IntMax_MAX         9223372036854775807LL
# define UintMax_MAX       18446744073709551615LLU
# define UintMAX_FP(x)      (long double)(x)
#elif  defined LLONG_MAX               // long long (when existing) is normally 64 bit
typedef signed   long long  Int64_t;
typedef unsigned long long  Uint64_t;
typedef signed   long long  IntMax_t;
typedef unsigned long long  UintMax_t;
# define IntMax_MIN        -9223372036854775808LL
# define IntMax_MAX         9223372036854775807LL
# define UintMax_MAX       18446744073709551615LLU
# define UintMAX_FP(x)      (long double)(x)
#elif  LONG_MAX > 0xFFFFFFFFLU         // long is longer than 33 bit, assume 64 bit
typedef signed   long       Int64_t;
typedef unsigned long       Uint64_t;
typedef signed   long       IntMax_t;
typedef unsigned long       UintMax_t;
# define IntMax_MIN        -9223372036854775808L
# define IntMax_MAX         9223372036854775807L
# define UintMax_MAX       18446744073709551615LU
# define UintMAX_FP(x)      (long double)(x)
#elif  defined _WIN32                  // Microsoft and Intel call it __int64
typedef signed   __int64    Int64_t;
typedef unsigned __int64    Uint64_t;
typedef signed   __int64    IntMax_t;
typedef unsigned __int64    UintMax_t;
# define IntMax_MIN        -9223372036854775808I64
# define IntMax_MAX         9223372036854775807I64
# define UintMax_MAX       18446744073709551615UI64
# define UintMAX_FP(x)      (long double)(IntMax_t)(x)
#else
# define NO_INT64_T                    // no type mapped to 64 bit integer
typedef signed   long       IntMax_t;
typedef unsigned long       UintMax_t;
# define IntMax_MIN        -2147483648L
# define IntMax_MAX         2147483647L
# define UintMax_MAX        4294967295LU
# define UintMAX_FP(x)      (long double)(x)
#endif


#endif /* MPPDEC_TYPES_H */

/* end of types.h */
