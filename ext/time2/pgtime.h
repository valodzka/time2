/*-------------------------------------------------------------------------
 *
 * pgtime.h
 *	  PostgreSQL internal timezone library
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PGTIME_H
#define _PGTIME_H

/*
 * The API of this library is generally similar to the corresponding
 * C library functions, except that we use pg_time_t which (we hope) is
 * 64 bits wide, and which is most definitely signed not unsigned.
 */

typedef time_t pg_time_t;

struct pg_tm
{
	int			tm_sec;
	int			tm_min;
	int			tm_hour;
	int			tm_mday;
	int			tm_mon;			/* origin 0, not 1 */
	int			tm_year;		/* relative to 1900 */
	int			tm_wday;
	int			tm_yday;
	int			tm_isdst;
	long int	tm_gmtoff;
	const char *tm_zone;
};

typedef struct pg_tz pg_tz;
typedef struct pg_tzenum pg_tzenum;

extern struct pg_tm *pg_localtime_r(const pg_time_t *timep,
                                    const pg_tz *tz,
                                    struct pg_tm *tm);
extern struct pg_tm *pg_gmtime_r(const pg_time_t *timep, struct pg_tm *tm);
extern char * pg_asctime_r(const struct pg_tm *tm,  char * buf);
extern pg_time_t pg_mktime(struct pg_tm *tmp, struct pg_tz const * tz);

extern int pg_next_dst_boundary(const pg_time_t *timep,
					 long int *before_gmtoff,
					 int *before_isdst,
					 pg_time_t *boundary,
					 long int *after_gmtoff,
					 int *after_isdst,
					 const pg_tz *tz);
extern size_t pg_strftime(char *s, size_t max, const char *format,
			const struct pg_tm * tm);

extern const char *
pg_strptime(const char * buf,
			const char * fmt,
			struct pg_tm * tm,
			long *nsec,
			char **tz,
			int *tzlen);

extern pg_tz *pg_tzset(const char *tzname);
extern bool tz_acceptable(pg_tz *tz);
extern bool pg_get_timezone_offset(const pg_tz *tz, long int *gmtoff);
extern const char *pg_get_timezone_name(pg_tz *tz);

extern pg_tzenum *pg_tzenumerate_start(void);
extern pg_tz *pg_tzenumerate_next(pg_tzenum *dir);
extern void pg_tzenumerate_end(pg_tzenum *dir);

extern pg_tz *select_default_timezone(void);
/* Maximum length of a timezone name (not including trailing null) */
#define TZ_STRLEN_MAX 255
#define STD_ASCTIME_BUF_SIZE	26

#endif   /* _PGTIME_H */
