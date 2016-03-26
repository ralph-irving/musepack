#ifndef MPPDEC_PROFILE_H
#define MPPDEC_PROFILE_H

#ifdef PROFILE

# if   defined _WIN32
typedef /*unsigned*/ __int64  uintmax_t;
#pragma warning ( disable: 4035 )
static __inline uintmax_t  rdtscll ( void ) { __asm { rdtsc }; }
#pragma warning ( default: 4035 )
#  define RDTSC()  timetemp = rdtscll ()
#  define __FUNCTION__  "?"
# elif defined __TURBOC__
typedef signed long  uintmax_t;
uintmax_t readtime ( void );
#  define RDTSC()  timetemp = readtime ()
#  define __FUNCTION__  "?"
# else
typedef unsigned long long  uintmax_t;
#  include <asm/msr.h>
#  define RDTSC()  rdtscll (timetemp)
# endif /* _WIN32 */


# define _STR(x)    #x
# define __STR(x)   _STR(x)

# define ENTER(x)  do {                                                             \
                     uintmax_t  timetemp;                                           \
                     RDTSC();                                                       \
                     timecounter[*functionstack_pointer]       += timetemp;         \
                     timecounter[*++functionstack_pointer = x] -= timetemp;         \
                     timename[x] = __FUNCTION__ "()|" __FILE__ "|" __STR(__LINE__); \
                   } while (0)

# define NEXT(x,n) do {                                                      \
                     uintmax_t  timetemp;                                    \
                     RDTSC();                                                \
                     timecounter[*functionstack_pointer]     += timetemp;    \
                     timecounter[*functionstack_pointer = x] -= timetemp;    \
                     timename[x] = __FUNCTION__ "-" __STR(n) "|" __FILE__ "|" __STR(__LINE__); \
                   } while (0)

# define LEAVE(x)  do {                                                  \
                     uintmax_t  timetemp;                                \
                     RDTSC();                                            \
                     timecounter[x]                        += timetemp;  \
                     timecounter[*--functionstack_pointer] -= timetemp;  \
                   } while (0)

# define START()   set_signal ()
# define REPORT()  report ()

extern uintmax_t       timecounter    [256];
extern const char*     timename       [256];
extern unsigned char   functionstack [1024];
extern unsigned char*  functionstack_pointer;

void  set_signal ( void );
void  report     ( void );

#else

/* M A K R O S */
# define START()
# define ENTER(x)
# define NEXT(x,n)
# define LEAVE(x)
# define REPORT()

#endif /* PROFILE */

#endif /* MPPDEC_PROFILE_H */

/* end of profile.h */
