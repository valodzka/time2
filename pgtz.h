/*-------------------------------------------------------------------------
 *
 * pgtz.h
 *	  Timezone Library Integration Functions
 *
 * Note: this file contains only definitions that are private to the
 * timezone library.  Public definitions are in pgtime.h.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL$
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PGTZ_H
#define _PGTZ_H

#include "tzfile.h"
#include "pgtime.h"


#define BIGGEST(a, b)	(((a) > (b)) ? (a) : (b))

struct ttinfo
{								/* time type information */
	long		tt_gmtoff;		/* UTC offset in seconds */
	int			tt_isdst;		/* used to set tm_isdst */
	int			tt_abbrind;		/* abbreviation list index */
	int			tt_ttisstd;		/* TRUE if transition is std time */
	int			tt_ttisgmt;		/* TRUE if transition is UTC */
};

struct lsinfo
{								/* leap second information */
	pg_time_t	ls_trans;		/* transition time */
	long		ls_corr;		/* correction to apply */
};

struct state
{
	int			leapcnt;
	int			timecnt;
	int			typecnt;
	int			charcnt;
	int			goback;
	int			goahead;
	pg_time_t	ats[TZ_MAX_TIMES];
	unsigned char types[TZ_MAX_TIMES];
	struct ttinfo ttis[TZ_MAX_TYPES];
	char		chars[BIGGEST(BIGGEST(TZ_MAX_CHARS + 1, 3 /* sizeof gmt */ ),
										  (2 * (TZ_STRLEN_MAX + 1)))];
	struct lsinfo lsis[TZ_MAX_LEAPS];
};


struct pg_tz
{
	/* TZname contains the canonically-cased name of the timezone */
	char		TZname[TZ_STRLEN_MAX + 1];
	struct state state;
};


/* in pgtz.c */
extern int	pg_open_tzfile(const char *name, char *canonname);

/* in localtime.c */
extern int	tzload(const char *name, char *canonname, struct state * sp, 
				   int doextend);
extern int	tzparse(const char *name, struct state * sp, int lastditch);

#define RB_ERRNO_RAISE(msg, det) rb_raise(rb_eRuntimeError, msg ": %d", det, errno)

#ifdef WIN32
#  define SYSTEMTZDIR "C:\\Program Files\\PostgreSQL\\8.3\\share\\timezone\\"
#else
#  define SYSTEMTZDIR "/usr/share/zoneinfo/"
#endif

/* Julian-date equivalents of Day 0 in Unix and Postgres reckoning */
#define UNIX_EPOCH_JDATE		2440588 /* == date2j(1970, 1, 1) */
#define POSTGRES_EPOCH_JDATE	2451545 /* == date2j(2000, 1, 1) */

/*
*	This doesn't adjust for uneven daylight savings time intervals or leap
*	seconds, and it crudely estimates leap years.  A more accurate value
*	for days per years is 365.2422.
*/
#define SECS_PER_YEAR	(36525 * 864)	/* avoid floating-point computation */
#define SECS_PER_DAY	86400
#define SECS_PER_HOUR	3600
#define SECS_PER_MINUTE 60
#define MINS_PER_HOUR	60

#endif   /* _PGTZ_H */
