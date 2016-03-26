
/* Determine Endianess of the machine */

#define HAVE_LITTLE_ENDIAN  1234
#define HAVE_BIG_ENDIAN     4321

#define ENDIAN              HAVE_LITTLE_ENDIAN


/* Test the fast float-to-int rounding trick works */

#define HAVE_IEEE754_FLOAT
#define HAVE_IEEE754_DOUBLE


/* Test the presence of a 80 bit floating point type for writing AIFF headers */

#define HAVE_IEEE854_LONGDOUBLE


/* parsed values from file "version" */

#ifndef MPPDEC_VERSION
# define MPPDEC_VERSION   "1.95e"
#endif

#define MPPDEC_BUILD  "--Alpha--"

#ifndef MPPENC_VERSION
# define MPPENC_VERSION   "1.95e"
#endif

#define MPPENC_BUILD  "--Alpha--"

/* end of config.h */
