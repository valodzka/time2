#ifndef PRIVATE_H
#define PRIVATE_H

/*
 * This file is in the public domain, so clarified as of
 * 1996-06-05 by Arthur David Olson.
 *
 */
/*
 * This header is for use ONLY with the time conversion code.
 * There is no guarantee that it will remain unchanged,
 * or that it will remain at all.
 * Do NOT copy it to any system include directory.
 * Thank you!
 */

#include <limits.h>				/* for CHAR_BIT et al. */
#include <sys/stat.h>
//#include <sys/wait.h>			/* for WIFEXITED and WEXITSTATUS */

#include "pgtime.h"

#define GRANDPARENTED	"Local time zone must be set--see zic manual page"

#ifndef WIFEXITED
#define WIFEXITED(status)	(((status) & 0xff) == 0)
#endif   /* !defined WIFEXITED */
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#endif   /* !defined WEXITSTATUS */

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX. */
#define is_digit(c) ((unsigned)(c) - '0' <= 9)

/*
 * SunOS 4.1.1 libraries lack remove.
 */

#ifndef remove
extern int	unlink(const char *filename);

#define remove	unlink
#endif   /* !defined remove */

/*
 * Private function declarations.
 */
extern char *icalloc(int nelem, int elsize);
extern char *icatalloc(char *old, const char *new);
extern char *icpyalloc(const char *string);
extern char *imalloc(int n);
extern void *irealloc(void *pointer, int size);
extern void icfree(char *pointer);
extern void ifree(char *pointer);
extern const char *scheck(const char *string, const char *format);


#if HAVE_STDINT_H
#  include "stdint.h"
#endif /* !HAVE_STDINT_H */

/* Pre-C99 GCC compilers define __LONG_LONG_MAX__ instead of LLONG_MAX.  */
#if defined LLONG_MAX || defined __LONG_LONG_MAX__
typedef long long pg_int64_t;
#else /* ! (defined LLONG_MAX || defined __LONG_LONG_MAX__) */
#  if (LONG_MAX >> 31) < 0xffffffff
#    error Please use a compiler that supports a 64-bit integer type (or wider); you may need to compile with "-DHAVE_STDINT_H".
#  endif /* (LONG_MAX >> 31) < 0xffffffff */
typedef long pg_int64_t;
#endif /* ! (defined LLONG_MAX || defined __LONG_LONG_MAX__) */

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif /* !defined INT32_MAX */
#ifndef INT32_MIN
#define INT32_MIN (-1 - INT32_MAX)
#endif /* !defined INT32_MIN */


/*
 * Finally, some convenience items.
 */

#ifndef TRUE
#define TRUE	1
#endif   /* !defined TRUE */

#ifndef FALSE
#define FALSE	0
#endif   /* !defined FALSE */

#ifndef TYPE_BIT
#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif   /* !defined TYPE_BIT */

#ifndef TYPE_SIGNED
#define TYPE_SIGNED(type) (((type) -1) < 0)
#endif   /* !defined TYPE_SIGNED */

/*
 * Since the definition of TYPE_INTEGRAL contains floating point numbers,
 * it cannot be used in preprocessor directives.
 */

#ifndef TYPE_INTEGRAL
#define TYPE_INTEGRAL(type) (((type) 0.5) != 0.5)
#endif /* !defined TYPE_INTEGRAL */

#ifndef INT_STRLEN_MAXIMUM
/*
 * 302 / 1000 is log10(2.0) rounded up.
 * Subtract one for the sign bit if the type is signed;
 * add one for integer division truncation;
 * add one more for a minus sign if the type is signed.
 */
#define INT_STRLEN_MAXIMUM(type) \
	((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302 / 1000 + 1 + TYPE_SIGNED(type))
#endif   /* !defined INT_STRLEN_MAXIMUM */

#undef _
#define _(msgid) (msgid)

#ifndef YEARSPERREPEAT
#define YEARSPERREPEAT          400L     /* years before a Gregorian repeat */
#endif /* !defined YEARSPERREPEAT */

/*
** The Gregorian year averages 365.2425 days, which is 31556952 seconds.
*/

#ifndef AVGSECSPERYEAR
#define AVGSECSPERYEAR          31556952L
#endif /* !defined AVGSECSPERYEAR */

#ifndef SECSPERREPEAT
#define SECSPERREPEAT           (YEARSPERREPEAT * AVGSECSPERYEAR)
#endif /* !defined SECSPERREPEAT */

#ifndef SECSPERREPEAT_BITS
#define SECSPERREPEAT_BITS      34      /* ceil(log2(SECSPERREPEAT)) */
#endif /* !defined SECSPERREPEAT_BITS */

/*
 * UNIX was a registered trademark of The Open Group in 2003.
 */

#endif   /* !defined PRIVATE_H */
