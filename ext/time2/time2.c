
#include <time.h>

#include "c.h"
#include "pgtime.h"
#include "private.h"
#include "pgtz.h"
#include "tzfile.h"

#ifndef RUBY_TIME_18_COMPAT
#  include <ruby/ruby.h>
#  include <ruby/st.h>
#  include <ruby/intern.h>
#  include <ruby/encoding.h>
#else
#  include <ruby.h>
#  include <st.h>
#  include <intern.h>
#  define rb_enc_str_asciicompat_p(x) (1)
#endif

#ifndef HAVE_RB_TIME_NANO_NEW
#  define rb_time_nano_new(x, y) (rb_time_new((x),(y)/1000))
#endif

static ID id_year, id_mon,
	id_day, id_mday, id_wday, id_yday,
	id_hour, id_min, id_sec,
	id_dst, id_isdst, id_tz,
	id_utc, id_mul;

#define GetTZ(obj, tobj) do {								\
		tobj = timezone_cached_get(StringValueCStr(obj));	\
	} while(0)

static st_table *tz_cache = NULL;
static pg_tz *fast_cache_tz = NULL;

struct timespec rb_time_timespec(VALUE time);

#ifdef HAVE_GMTIME_R
#  define IF_HAVE_GMTIME_R(x) x
#  define ASCTIME(tm, buf) asctime_r(tm, buf)
#  define GMTIME(tm, result) gmtime_r(tm, &result)
#  define LOCALTIME(tm, result) (tzset(),localtime_r(tm, &result))
#else
#  define IF_HAVE_GMTIME_R(x)   /* nothing */
#  define ASCTIME(tm, buf) asctime(tm)
#  define GMTIME(tm, result) gmtime(tm)
#  define LOCALTIME(tm, result) localtime(tm)
#endif

#ifndef STRCASECMP(a,b)
#  define STRCASECMP(a,b) (strcasecmp(a,b))
#endif

#ifdef HAVE_TM_ZONE
#  define COPY_TM_TZ(expr) expr
#else
#  define COPY_TM_TZ(expr) (0)
#endif

#ifdef HAVE_STRUCT_TM_TM_GMTOFF
#  define COPY_TM_GMTOFF(expr) expr
#else
#  define COPY_TM_GMTOFF(expr) (0)
#endif

#define COPY_TM_TO_ORIG(tm, system_tm) do {							\
    	system_tm.tm_sec = (tm).tm_sec;								\
		system_tm.tm_min = (tm).tm_min;								\
		system_tm.tm_hour = (tm).tm_hour;							\
		system_tm.tm_mday = (tm).tm_mday;							\
		system_tm.tm_mon = (tm).tm_mon;								\
		system_tm.tm_year = (tm).tm_year;							\
		system_tm.tm_wday = (tm).tm_wday;							\
		system_tm.tm_yday = (tm).tm_yday;							\
		system_tm.tm_isdst = (tm).tm_isdst;							\
		COPY_TM_GMTOFF(system_tm.tm_gmtoff = (tm).tm_gmtoff);		\
		COPY_TM_TZ(system_tm.tm_zone = (tm).tm_zone);				\
    } while(0)
#define COPY_ORIG_TO_TM(system_tm, tm) do {						\
		tm.tm_sec = (system_tm).tm_sec;							\
		tm.tm_min = (system_tm).tm_min;							\
		tm.tm_hour = (system_tm).tm_hour;						\
		tm.tm_mday = (system_tm).tm_mday;						\
		tm.tm_mon = (system_tm).tm_mon;							\
		tm.tm_year = (system_tm).tm_year;						\
		tm.tm_wday = (system_tm).tm_wday;						\
		tm.tm_yday = (system_tm).tm_yday;						\
		tm.tm_isdst = (system_tm).tm_isdst;						\
		COPY_TM_GMTOFF(tm.tm_gmtoff = (system_tm).tm_gmtoff);	\
		COPY_TM_TZ(tm.tm_zone = (system_tm).tm_zone);			\
	} while(0)


static struct pg_tz*
timezone_cached_get(const char *tzname)
{
    st_data_t key = (st_data_t)tzname, value = 0;

	if (fast_cache_tz && strcmp(tzname, fast_cache_tz->TZname) == 0)
		return fast_cache_tz;

    if(!tz_cache)
        tz_cache = st_init_strtable();

    st_lookup(tz_cache, key, &value);

    if (!value) {
        struct pg_tz* tz = pg_tzset(tzname);

        if (!tz) rb_raise(rb_eArgError, "Time zone \"%s\" not found", tzname);

		value = (st_data_t)tz;
        st_insert(tz_cache, (st_data_t)tz->TZname, value);
    }

    return (fast_cache_tz = (struct pg_tz*)value);
}

static int
leap_year_p(long y)
{
	return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

#define NDIV(x,y) (-(-((x)+1)/(y))-1)
#define NMOD(x,y) ((y)-(-((x)+1)%(y))-1)
#define DIV(n,d) ((n)<0 ? NDIV((n),(d)) : (n)/(d))

static time_t
timegm_noleapsecond(struct tm *tm)
{
	static const int common_year_yday_offset[] = {
		-1,
		-1 + 31,
		-1 + 31 + 28,
		-1 + 31 + 28 + 31,
		-1 + 31 + 28 + 31 + 30,
		-1 + 31 + 28 + 31 + 30 + 31,
		-1 + 31 + 28 + 31 + 30 + 31 + 30,
		-1 + 31 + 28 + 31 + 30 + 31 + 30 + 31,
		-1 + 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
		-1 + 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
		-1 + 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
		-1 + 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30
		/* 1    2    3    4    5    6    7    8    9    10   11 */
	};
	static const int leap_year_yday_offset[] = {
		-1,
		-1 + 31,
		-1 + 31 + 29,
		-1 + 31 + 29 + 31,
		-1 + 31 + 29 + 31 + 30,
		-1 + 31 + 29 + 31 + 30 + 31,
		-1 + 31 + 29 + 31 + 30 + 31 + 30,
		-1 + 31 + 29 + 31 + 30 + 31 + 30 + 31,
		-1 + 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31,
		-1 + 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
		-1 + 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
		-1 + 31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30
		/* 1    2    3    4    5    6    7    8    9    10   11 */
	};

	long tm_year = tm->tm_year;
	int tm_yday = tm->tm_mday;
	if (leap_year_p(tm_year + 1900))
		tm_yday += leap_year_yday_offset[tm->tm_mon];
	else
		tm_yday += common_year_yday_offset[tm->tm_mon];

	/*
	 *  `Seconds Since the Epoch' in SUSv3:
	 *  tm_sec + tm_min*60 + tm_hour*3600 + tm_yday*86400 +
	 *  (tm_year-70)*31536000 + ((tm_year-69)/4)*86400 -
	 *  ((tm_year-1)/100)*86400 + ((tm_year+299)/400)*86400
	 */
	return tm->tm_sec + tm->tm_min*60 + tm->tm_hour*3600 +
		(time_t)(tm_yday +
			 (tm_year-70)*365 +
			 DIV(tm_year-69,4) -
			 DIV(tm_year-1,100) +
			 DIV(tm_year+299,400))*86400;
}

static int
tmcmp(struct tm *a, struct tm *b)
{
	if (a->tm_year != b->tm_year)
		return a->tm_year < b->tm_year ? -1 : 1;
	else if (a->tm_mon != b->tm_mon)
		return a->tm_mon < b->tm_mon ? -1 : 1;
	else if (a->tm_mday != b->tm_mday)
		return a->tm_mday < b->tm_mday ? -1 : 1;
	else if (a->tm_hour != b->tm_hour)
		return a->tm_hour < b->tm_hour ? -1 : 1;
	else if (a->tm_min != b->tm_min)
		return a->tm_min < b->tm_min ? -1 : 1;
	else if (a->tm_sec != b->tm_sec)
		return a->tm_sec < b->tm_sec ? -1 : 1;
	else
		return 0;
}

#if SIZEOF_TIME_T == SIZEOF_LONG
typedef unsigned long unsigned_time_t;
#elif SIZEOF_TIME_T == SIZEOF_INT
typedef unsigned int unsigned_time_t;
#elif SIZEOF_TIME_T == SIZEOF_LONG_LONG
typedef unsigned LONG_LONG unsigned_time_t;
#else
# error cannot find integer type which size is same as time_t.
#endif

static time_t
search_time_t(struct tm *tptr, int utc_p)
{
	time_t guess, guess_lo, guess_hi;
	struct tm *tm, tm_lo, tm_hi;
	int d, have_guess;
	int find_dst;
	IF_HAVE_GMTIME_R(struct tm result);
#define GUESS(p) (utc_p ? GMTIME(p, result) : LOCALTIME(p, result))

	find_dst = 0 < tptr->tm_isdst;

#ifdef NEGATIVE_TIME_T
	guess_lo = (time_t)~((unsigned_time_t)~(time_t)0 >> 1);
#else
	guess_lo = 0;
#endif
	guess_hi = ((time_t)-1) < ((time_t)0) ?
		(time_t)((unsigned_time_t)~(time_t)0 >> 1) :
		~(time_t)0;

	guess = timegm_noleapsecond(tptr);
	tm = GUESS(&guess);
	if (tm) {
		d = tmcmp(tptr, tm);
		if (d == 0) return guess;
		if (d < 0) {
			guess_hi = guess;
			guess -= 24 * 60 * 60;
		}
		else {
			guess_lo = guess;
			guess += 24 * 60 * 60;
		}
		if (guess_lo < guess && guess < guess_hi && (tm = GUESS(&guess)) != NULL) {
			d = tmcmp(tptr, tm);
			if (d == 0) return guess;
			if (d < 0)
				guess_hi = guess;
			else
				guess_lo = guess;
		}
	}

	tm = GUESS(&guess_lo);
	if (!tm) goto error;
	d = tmcmp(tptr, tm);
	if (d < 0) goto out_of_range;
	if (d == 0) return guess_lo;
	tm_lo = *tm;

	tm = GUESS(&guess_hi);
	if (!tm) goto error;
	d = tmcmp(tptr, tm);
	if (d > 0) goto out_of_range;
	if (d == 0) return guess_hi;
	tm_hi = *tm;

	have_guess = 0;

	while (guess_lo + 1 < guess_hi) {
		/* there is a gap between guess_lo and guess_hi. */
		unsigned long range = 0;
		if (!have_guess) {
			int a, b;
			/*
			  Try precious guess by a linear interpolation at first.
			  `a' and `b' is a coefficient of guess_lo and guess_hi as:

			  guess = (guess_lo * a + guess_hi * b) / (a + b)

			  However this causes overflow in most cases, following assignment
			  is used instead:

			  guess = guess_lo / d * a + (guess_lo % d) * a / d
			  + guess_hi / d * b + (guess_hi % d) * b / d
			  where d = a + b

			  To avoid overflow in this assignment, `d' is restricted to less than
			  sqrt(2**31).  By this restriction and other reasons, the guess is
			  not accurate and some error is expected.  `range' approximates
			  the maximum error.

			  When these parameters are not suitable, i.e. guess is not within
			  guess_lo and guess_hi, simple guess by binary search is used.
			*/
			range = 366 * 24 * 60 * 60;
			a = (tm_hi.tm_year - tptr->tm_year);
			b = (tptr->tm_year - tm_lo.tm_year);
			/* 46000 is selected as `some big number less than sqrt(2**31)'. */
			if (a + b <= 46000 / 12) {
				range = 31 * 24 * 60 * 60;
				a *= 12;
				b *= 12;
				a += tm_hi.tm_mon - tptr->tm_mon;
				b += tptr->tm_mon - tm_lo.tm_mon;
				if (a + b <= 46000 / 31) {
					range = 24 * 60 * 60;
					a *= 31;
					b *= 31;
					a += tm_hi.tm_mday - tptr->tm_mday;
					b += tptr->tm_mday - tm_lo.tm_mday;
					if (a + b <= 46000 / 24) {
						range = 60 * 60;
						a *= 24;
						b *= 24;
						a += tm_hi.tm_hour - tptr->tm_hour;
						b += tptr->tm_hour - tm_lo.tm_hour;
						if (a + b <= 46000 / 60) {
							range = 60;
							a *= 60;
							b *= 60;
							a += tm_hi.tm_min - tptr->tm_min;
							b += tptr->tm_min - tm_lo.tm_min;
							if (a + b <= 46000 / 60) {
								range = 1;
								a *= 60;
								b *= 60;
								a += tm_hi.tm_sec - tptr->tm_sec;
								b += tptr->tm_sec - tm_lo.tm_sec;
							}
						}
					}
				}
			}
			if (a <= 0) a = 1;
			if (b <= 0) b = 1;
			d = a + b;
			/*
			  Although `/' and `%' may produce unexpected result with negative
			  argument, it doesn't cause serious problem because there is a
			  fail safe.
			*/
			guess = guess_lo / d * a + (guess_lo % d) * a / d
				+ guess_hi / d * b + (guess_hi % d) * b / d;
			have_guess = 1;
		}

		if (guess <= guess_lo || guess_hi <= guess) {
			/* Precious guess is invalid. try binary search. */
			guess = guess_lo / 2 + guess_hi / 2;
			if (guess <= guess_lo)
				guess = guess_lo + 1;
			else if (guess >= guess_hi)
				guess = guess_hi - 1;
			range = 0;
		}

		tm = GUESS(&guess);
		if (!tm) goto error;
		have_guess = 0;

		d = tmcmp(tptr, tm);
		if (d < 0) {
			guess_hi = guess;
			tm_hi = *tm;
			if (range) {
				guess = guess - range;
				range = 0;
				if (guess_lo < guess && guess < guess_hi)
					have_guess = 1;
			}
		}
		else if (d > 0) {
			guess_lo = guess;
			tm_lo = *tm;
			if (range) {
				guess = guess + range;
				range = 0;
				if (guess_lo < guess && guess < guess_hi)
					have_guess = 1;
			}
		}
		else {
			if (!utc_p) {
				/* If localtime is nonmonotonic, another result may exist. */
				time_t guess2;
				if (find_dst) {
					guess2 = guess - 2 * 60 * 60;
					tm = LOCALTIME(&guess2, result);
					if (tm) {
						if (tptr->tm_hour != (tm->tm_hour + 2) % 24 ||
						    tptr->tm_min != tm->tm_min ||
						    tptr->tm_sec != tm->tm_sec
							) {
							guess2 -= (tm->tm_hour - tptr->tm_hour) * 60 * 60 +
								(tm->tm_min - tptr->tm_min) * 60 +
								(tm->tm_sec - tptr->tm_sec);
							if (tptr->tm_mday != tm->tm_mday)
								guess2 += 24 * 60 * 60;
							if (guess != guess2) {
								tm = LOCALTIME(&guess2, result);
								if (tmcmp(tptr, tm) == 0) {
									if (guess < guess2)
										return guess;
									else
										return guess2;
								}
							}
						}
					}
				}
				else {
					guess2 = guess + 2 * 60 * 60;
					tm = LOCALTIME(&guess2, result);
					if (tm) {
						if ((tptr->tm_hour + 2) % 24 != tm->tm_hour ||
						    tptr->tm_min != tm->tm_min ||
						    tptr->tm_sec != tm->tm_sec
							) {
							guess2 -= (tm->tm_hour - tptr->tm_hour) * 60 * 60 +
								(tm->tm_min - tptr->tm_min) * 60 +
								(tm->tm_sec - tptr->tm_sec);
							if (tptr->tm_mday != tm->tm_mday)
								guess2 -= 24 * 60 * 60;
							if (guess != guess2) {
								tm = LOCALTIME(&guess2, result);
								if (tmcmp(tptr, tm) == 0) {
									if (guess < guess2)
										return guess2;
									else
										return guess;
								}
							}
						}
					}
				}
			}
			return guess;
		}
	}
	/* Given argument has no corresponding time_t. Let's outerpolation. */
	if (tm_lo.tm_year == tptr->tm_year && tm_lo.tm_mon == tptr->tm_mon) {
		return guess_lo +
			(tptr->tm_mday - tm_lo.tm_mday) * 24 * 60 * 60 +
			(tptr->tm_hour - tm_lo.tm_hour) * 60 * 60 +
			(tptr->tm_min - tm_lo.tm_min) * 60 +
			(tptr->tm_sec - tm_lo.tm_sec);
	}
	else if (tm_hi.tm_year == tptr->tm_year && tm_hi.tm_mon == tptr->tm_mon) {
		return guess_hi +
			(tptr->tm_mday - tm_hi.tm_mday) * 24 * 60 * 60 +
			(tptr->tm_hour - tm_hi.tm_hour) * 60 * 60 +
			(tptr->tm_min - tm_hi.tm_min) * 60 +
			(tptr->tm_sec - tm_hi.tm_sec);
	}

out_of_range:
	rb_raise(rb_eArgError, "time out of range");

error:
	rb_raise(rb_eArgError, "gmtime/localtime error");
	return 0;      /* not reached */
}

static const char months[][4] = {
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec",
};

static long
obj2long(VALUE obj)
{
    if (TYPE(obj) == T_STRING) {
		obj = rb_str_to_inum(obj, 10, Qfalse);
    }

    return NUM2LONG(obj);
}

static long
obj2long1000(VALUE obj)
{
    if (TYPE(obj) == T_STRING) {
		obj = rb_str_to_inum(obj, 10, Qfalse);
        return NUM2LONG(obj) * 1000;
    }

    return NUM2LONG(rb_funcall(obj, id_mul, 1, INT2FIX(1000)));
}

static long
obj2nsec(VALUE obj, long *nsec)
{
    struct timespec ts;

    if (TYPE(obj) == T_STRING) {
		obj = rb_str_to_inum(obj, 10, Qfalse);
        *nsec = 0;
        return NUM2LONG(obj);
    }

    ts = rb_time_timespec(obj);
	if (ts.tv_sec < 0) rb_raise(rb_eArgError, "time interval must be positive");
    *nsec = ts.tv_nsec;
    return (long)ts.tv_sec;
}


static int
time2_arg_i(st_data_t key, st_data_t val, st_data_t store)
{
    VALUE *v = (VALUE*)store;
    VALUE symbol = (VALUE)key;
    VALUE value = (VALUE)val;
    ID id = SYM2ID(symbol);

    if (SYMBOL_P(symbol)) {
		if (id == id_year)
			v[0] = value;
		else if (id == id_mon)
			v[1] = value;
		else if (id == id_mday || id == id_day)
			v[2] = value;
		else if (id == id_wday)
			;
		else if (id == id_yday)
			;
		else if (id == id_hour)
			v[3] = value;
		else if (id == id_min)
			v[4] = value;
		else if (id == id_sec)
			v[5] = value;
		else if (id == id_dst || id == id_isdst)
			v[8] = value;
		else if (id == id_tz)
			v[7] = value;
		else
			rb_raise(rb_eArgError, "unknown key: %s", rb_id2name(id));
    }
    else
        rb_raise(rb_eArgError, "invalid argument type: %s", rb_obj_classname(symbol));

    return ST_CONTINUE;
}

static void
time2_arg(int argc, VALUE *argv, struct pg_tm *tm, long *nsec, struct pg_tz** tz)
{
    /* 0 -> year, 1 -> month, 2 -> day, 3 -> hour,
       4 -> min, 5 -> sec, 6 -> nsec,   7 -> zone
       8 -> dst */
    VALUE v[9] = { Qnil, Qnil, Qnil, Qnil, Qnil, Qnil, Qnil, Qnil, Qnil};
    int i;
    long year;

    MEMZERO(tm, struct pg_tm, 1);
    *nsec = 0;
    if (argc == 10) {
		v[0] = argv[5];
		v[1] = argv[4];
		v[2] = argv[3];
		v[3] = argv[2];
		v[4] = argv[1];
		v[5] = argv[0];
		v[6] = Qnil;
		v[7] = argv[9];
		v[8] = argv[8];
    }
    else {
		if (argc == 1 && TYPE(argv[0]) == T_HASH) {
			st_foreach(RHASH_TBL(argv[0]), time2_arg_i, (st_data_t)v);
		}
		else {
			rb_scan_args(argc, argv, "17", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7]);
			/* v[6] may be usec or zone (parsedate) */
			/* v[7] is wday (parsedate; ignored) */
			tm->tm_wday = -1;
			tm->tm_isdst = -1;
			*tz = NULL;
		}
    }

    year = obj2long(v[0]);

    if (0 <= year && year < 39) {
        rb_warning("2 digits year is used: %ld", year);
		year += 100;
    }
    else if (69 <= year && year < 139) {
        rb_warning("2 or 3 digits year is used: %ld", year);
    }
    else {
		year -= 1900;
    }

    tm->tm_year = year;

    if (NIL_P(v[1])) {
		tm->tm_mon = 0;
    }
    else {
		VALUE s = rb_check_string_type(v[1]);
		if (!NIL_P(s)) {
			tm->tm_mon = -1;
			for (i=0; i<12; i++) {
				if (RSTRING_LEN(s) == 3 &&
					STRCASECMP(months[i], RSTRING_PTR(s)) == 0) {
					tm->tm_mon = i;
					break;
				}
			}
			if (tm->tm_mon == -1) {
				char c = RSTRING_PTR(s)[0];

				if ('0' <= c && c <= '9') {
					tm->tm_mon = obj2long(s)-1;
				}
			}
		}
		else {
			tm->tm_mon = obj2long(v[1])-1;
		}
    }
    if (NIL_P(v[2])) {
		tm->tm_mday = 1;
    }
    else {
		tm->tm_mday = obj2long(v[2]);
    }
    tm->tm_hour = NIL_P(v[3])?0:obj2long(v[3]);
    tm->tm_min  = NIL_P(v[4])?0:obj2long(v[4]);

	// TODO: make it work at all ruby version
#ifndef HAVE_RB_TIME_TIMESPEC
	tm->tm_sec = NIL_P(v[5])?0:obj2long(v[5]);
    if (!NIL_P(v[6])) {
	  if (argc == 8) {
		/* v[6] is timezone, but ignored */
	  }
	  else if (argc == 7) {
		*nsec = obj2long1000(v[6]);;
	  }
    }
#else
    if (!NIL_P(v[6]) && argc == 7) {
	    tm->tm_sec  = NIL_P(v[5])?0:obj2long(v[5]);
        *nsec = obj2long1000(v[6]);
    }
    else {
		/* when argc == 8, v[6] is timezone, but ignored */
	    tm->tm_sec  = NIL_P(v[5])?0:obj2nsec(v[5], nsec);
    }
#endif

    if (!NIL_P(v[7]))
		GetTZ(v[7], *tz);
    else
		*tz = NULL;

    switch(v[8]) {
    case Qtrue:
		tm->tm_isdst = 1;
		break;
    case Qfalse:
		tm->tm_isdst = 0;
		break;
    case Qnil:
		tm->tm_isdst = -1;
		break;
    default:
		rb_raise(rb_eArgError, "dst must be true, false, or nil");
    }

    /* value validation */
    if (
		tm->tm_year != year ||
		tm->tm_mon  < 0 || tm->tm_mon  > 11
		|| tm->tm_mday < 1 || tm->tm_mday > 31
		|| tm->tm_hour < 0 || tm->tm_hour > 24
		|| (tm->tm_hour == 24 && (tm->tm_min > 0 || tm->tm_sec > 0))
		|| tm->tm_min  < 0 || tm->tm_min  > 59
		|| tm->tm_sec  < 0 || tm->tm_sec  > 60)
		rb_raise(rb_eArgError, "argument out of range");
}

static time_t
time2_make_time_t_orig(struct tm *tptr, int utc_p)
{
    time_t t;
#ifdef NEGATIVE_TIME_T
    struct tm *tmp;
#endif
    struct tm buf;
    IF_HAVE_GMTIME_R(struct tm result);

    buf = *tptr;
    if (utc_p) {
#ifdef HAVE_TIMEGM
		if ((t = timegm(&buf)) != -1)
			return t;
#  ifdef NEGATIVE_TIME_T
		if ((tmp = GMTIME(&t, result)) &&
			tptr->tm_year == tmp->tm_year &&
			tptr->tm_mon == tmp->tm_mon &&
			tptr->tm_mday == tmp->tm_mday &&
			tptr->tm_hour == tmp->tm_hour &&
			tptr->tm_min == tmp->tm_min &&
			tptr->tm_sec == tmp->tm_sec
			)
			return t;
#  endif
#endif
    }
    else {
#ifdef HAVE_MKTIME
		if ((t = mktime(&buf)) != -1)
			return t;
#  ifdef NEGATIVE_TIME_T
		if ((tmp = LOCALTIME(&t, result)) &&
			tptr->tm_year == tmp->tm_year &&
			tptr->tm_mon == tmp->tm_mon &&
			tptr->tm_mday == tmp->tm_mday &&
			tptr->tm_hour == tmp->tm_hour &&
			tptr->tm_min == tmp->tm_min &&
			tptr->tm_sec == tmp->tm_sec
			)
            return t;
#  endif
#endif

    }
    //rb_raise(rb_eArgError, "search_time_t");
    return search_time_t(&buf, utc_p);
}

static pg_time_t
time2_make_time_t(struct pg_tm *tptr, struct pg_tz const *tz, int utc_p)
{
    pg_time_t t;
    struct pg_tm *tmp;
    struct pg_tm buf;
    struct pg_tm result;

    buf = *tptr;

    if ((t = pg_mktime(&buf, tz)) != -1)
		return t;
    if ((tmp = pg_localtime_r(&t, tz, &result)) &&
		tptr->tm_year == tmp->tm_year &&
		tptr->tm_mon == tmp->tm_mon &&
		tptr->tm_mday == tmp->tm_mday &&
		tptr->tm_hour == tmp->tm_hour &&
		tptr->tm_min == tmp->tm_min &&
		tptr->tm_sec == tmp->tm_sec
		)
		return t;

    rb_raise(rb_eRuntimeError, "search_time_t");
    //return search_time_t(&buf, utc_p);
}


static VALUE
time2_utc_or_local(int argc, VALUE *argv, int utc_p, VALUE klass)
{
    struct pg_tm tm;
    struct pg_tz *tz;
    VALUE time;
    long nsec;
	pg_time_t timeval;

	time2_arg(argc, argv, &tm, &nsec, &tz);

	if (tz)
		timeval = time2_make_time_t(&tm, tz, utc_p);
	else {
		struct tm tm_orig;
		COPY_TM_TO_ORIG(tm, tm_orig);
		timeval = time2_make_time_t_orig(&tm_orig, utc_p);
	}

	time = rb_time_nano_new(timeval, nsec);
	if (utc_p) rb_funcall(time, id_utc, 0);

	return time;
/*     struct pg_tm tm; */
/*     struct pg_tz *tz; */
/*     VALUE time; */
/*     long nsec; */

/*     time_arg(argc, argv, &tm, &nsec, &tz); */
/*     if (!tz) */
/* 		tz = utc_p ? timezone_utc() : timezone_default(NULL); */
/*     time = time_new_internal(klass, make_time_t(&tm, tz, utc_p), nsec, tz); */
/*     if (utc_p) return time_gmtime(time); */
/*     return time_localtime_with_tz(time, timezone_default(NULL)); */
}

/*
 *  call-seq:
 *    Time.utc(year) => time
 *    Time.utc(year, month) => time
 *    Time.utc(year, month, day) => time
 *    Time.utc(year, month, day, hour) => time
 *    Time.utc(year, month, day, hour, min) => time
 *    Time.utc(year, month, day, hour, min, sec_with_frac) => time
 *    Time.utc(year, month, day, hour, min, sec, usec_with_frac) => time
 *    Time.utc(sec, min, hour, day, month, year, wday, yday, isdst, tz) => time
 *    Time.gm(year) => time
 *    Time.gm(year, month) => time
 *    Time.gm(year, month, day) => time
 *    Time.gm(year, month, day, hour) => time
 *    Time.gm(year, month, day, hour, min) => time
 *    Time.gm(year, month, day, hour, min, sec_with_frac) => time
 *    Time.gm(year, month, day, hour, min, sec, usec_with_frac) => time
 *    Time.gm(sec, min, hour, day, month, year, wday, yday, isdst, tz) => time
 *
 *  Creates a time based on given values, interpreted as UTC (GMT). The
 *  year must be specified. Other values default to the minimum value
 *  for that field (and may be <code>nil</code> or omitted). Months may
 *  be specified by numbers from 1 to 12, or by the three-letter English
 *  month names. Hours are specified on a 24-hour clock (0..23). Raises
 *  an <code>ArgumentError</code> if any values are out of range. Will
 *  also accept ten arguments in the order output by
 *  <code>Time#to_a</code>.
 *  <i>sec_with_frac</i> and <i>usec_with_frac</i> can have a fractional part.
 *
 *     Time.utc(2000,"jan",1,20,15,1)  #=> 2000-01-01 20:15:01 UTC
 *     Time.gm(2000,"jan",1,20,15,1)   #=> 2000-01-01 20:15:01 UTC
 */
static VALUE
time2_s_mkutc(int argc, VALUE *argv, VALUE klass)
{
    return time2_utc_or_local(argc, argv, Qtrue, klass);
}

/*
 *  call-seq:
 *   Time.local(year) => time
 *   Time.local(year, month) => time
 *   Time.local(year, month, day) => time
 *   Time.local(year, month, day, hour) => time
 *   Time.local(year, month, day, hour, min) => time
 *   Time.local(year, month, day, hour, min, sec_with_frac) => time
 *   Time.local(year, month, day, hour, min, sec, usec_with_frac) => time
 *   Time.local(sec, min, hour, day, month, year, wday, yday, isdst, tz) => time
 *   Time.mktime(year) => time
 *   Time.mktime(year, month) => time
 *   Time.mktime(year, month, day) => time
 *   Time.mktime(year, month, day, hour) => time
 *   Time.mktime(year, month, day, hour, min) => time
 *   Time.mktime(year, month, day, hour, min, sec_with_frac) => time
 *   Time.mktime(year, month, day, hour, min, sec, usec_with_frac) => time
 *   Time.mktime(sec, min, hour, day, month, year, wday, yday, isdst, tz) => time
 *
 *  Same as <code>Time::gm</code>, but interprets the values in the
 *  local time zone.
 *
 *     Time.local(2000,"jan",1,20,15,1)   #=> 2000-01-01 20:15:01 -0600
 */

static VALUE
time2_s_mktime(int argc, VALUE *argv, VALUE klass)
{
    return time2_utc_or_local(argc, argv, Qfalse, klass);
}


static void
time2_fill_gaps_tm(struct pg_tm *tm)
{
	struct pg_tm tm_now;
	int fill_now = 1;

	if (tm->tm_year == INT_MIN) {
		pg_time_t now = time(NULL);
		struct tm* resultp;
		IF_HAVE_GMTIME_R(struct tm result);

		resultp = LOCALTIME(&now, result);
		COPY_ORIG_TO_TM(*resultp, tm_now);
		tm->tm_year = tm_now.tm_year;
	}
	else
		fill_now = 0;

	if (tm->tm_yday != INT_MIN) {
		/* let mktime detect appropriate month and day */
		tm->tm_mday = tm->tm_yday + 1;
		tm->tm_mon = 0;
	}

	if (tm->tm_mon == INT_MIN)
		tm->tm_mon = fill_now ? tm_now.tm_mon : 0;
	else
		fill_now = 0;

	if (tm->tm_mday == INT_MIN)
		tm->tm_mday = fill_now ? tm_now.tm_mday : 1;
	else
		fill_now = 0;

	if (tm->tm_hour == INT_MIN)
		tm->tm_hour = fill_now ? tm_now.tm_hour : 0;
	else
		fill_now = 0;

	if (tm->tm_min == INT_MIN)
		tm->tm_min = fill_now ? tm_now.tm_min : 0;
	else
		fill_now = 0;

	if (tm->tm_sec == INT_MIN)
		tm->tm_sec = fill_now ? tm_now.tm_sec : 0;
	else
		fill_now = 0;
}

/*
 *  call-seq:
 *     Time.strptime( string, format ) => time
 *
 * Convert the <i>string</i> to time, using the format specified by <i>format</i>
 *
 * The format is composed of zero or more directives. Each directive is composed
 * of one of the following:
 * - one or more white-space characters;
 * - an ordinary character (neither '%' nor a white-space character);
 * - a conversion specification.
 *
 * Each conversion specification is composed of a '%' character followed by a
 * conversion character which specifies the replacement required. The following
 * conversion specifications are supported:
 * - <s>%a - The abbreviated weekday name (``Sun'')</s>
 * - <s>%A - The  full  weekday  name (``Sunday'')</s>
 * - %b - The abbreviated month name (``Jan'')
 * - %B - The  full  month  name (``January'')
 * - <s>%c - The preferred local date and time representation</s>
 * - %d - Day of the month (01..31)
 * - %F - Equivalent to %Y-%m-%d (the ISO 8601 date format)
 * - %H - Hour of the day, 24-hour clock (00..23)
 * - %I - Hour of the day, 12-hour clock (01..12)
 * - %j - Day of the year (001..366)
 * - %L - Millisecond of the second (000..999)
 * - %m - Month of the year (01..12)
 * - %M - Minute of the hour (00..59)
 * - %N - Fractional seconds digits, default is 9 digits (nanosecond)
 * - - <s>%3N  millisecond (3 digits)
 * - - %6N  microsecond (6 digits)
 * - - %9N  nanosecond (9 digits)</s>
 * - %p - Meridian indicator
 * - <s>%P - Meridian indicator (``am''  or  ``pm'')</s>
 * - %s - Number of seconds since 1970-01-01 00:00:00 UTC.
 * - %S - Second of the minute (00..60)
 * - <s>%U - Week  number  of the current year,
 * starting with the first Sunday as the first
 * day of the first week (00..53)
 * - %W - Week  number  of the current year,
 * starting with the first Monday as the first
 * day of the first week (00..53)
 * - %w - Day of the week (Sunday is 0, 0..6)</s>
 * - %x - Preferred representation for the date alone, no time (%m/%d/%y by default)
 * - %X - Preferred representation for the time alone, no date (%H:%M:%S by default)
 * - %y - Year without a century (00..99)
 * - %Y - Year with century
 * - <s>%Z - Time zone name</s>
 *
 * TODO:
 * - unknown modificators silently ignored
 * - %j replace %m and %d undepend on order of arguments
 * - document principe of gaps filling and replacement
 * - %N behaves different from old strptime
 */
static VALUE
time2_strptime(VALUE klass, VALUE str, VALUE format)
{
	struct pg_tm tm;
	struct tm tm_orig;
	long nsec = 0;
	long len_str, len_fmt;
	char *ptr_str, *ptr_fmt;
	char *tzptr = NULL;
	int tzlen = 0, utcp = 0;

	StringValue(str);
	StringValue(format);

	if (!rb_enc_str_asciicompat_p(format) || !rb_enc_str_asciicompat_p(str)) {
		rb_raise(rb_eArgError, "arguments should have ASCII compatible encoding");
	}

	tm.tm_year = INT_MIN;
	tm.tm_mon = INT_MIN;
	tm.tm_mday = INT_MIN;
	tm.tm_hour = INT_MIN;
	tm.tm_min = INT_MIN;
	tm.tm_sec = INT_MIN;
	tm.tm_wday = 0;
	tm.tm_yday = INT_MIN;
	tm.tm_isdst = -1;
	tm.tm_zone = NULL;

	ptr_fmt = RSTRING_PTR(format);
	len_fmt = RSTRING_LEN(format);

	if (memchr(ptr_fmt, '\0', len_fmt)) {
		long idx_str = 0, idx_fmt = 0;

		ptr_str = RSTRING_PTR(str);
		len_str = RSTRING_LEN(str);

		do {
			if(!pg_strptime(ptr_str+idx_str, ptr_fmt+idx_fmt, &tm, &nsec, &tzptr, &tzlen))
				rb_raise(rb_eArgError, "strptime error");
			while (idx_str < len_str && ptr_str[idx_str] != '\0') ++idx_str;
			idx_str++;
			while (idx_fmt < len_fmt && ptr_fmt[idx_fmt] != '\0') ++idx_fmt;
			idx_fmt++;
		} while(idx_fmt < len_fmt && idx_str < len_str);
		/* TODO: may have unprocessed symbols */
	}
	else {
		if(!pg_strptime(StringValueCStr(str), StringValueCStr(format), &tm, &nsec, &tzptr, &tzlen))
			rb_raise(rb_eArgError, "strptime error");
	}

	/* currently only very basic time zone support */
	if (tzlen > 0 && ( strncmp("Z", tzptr, tzlen) == 0 ||
					   strncmp("UTC", tzptr, tzlen) == 0 ||
					   strncmp("GMT", tzptr, tzlen) == 0)) {
		utcp = 1;
	}
	time2_fill_gaps_tm(&tm);
	COPY_TM_TO_ORIG(tm, tm_orig);

	return rb_time_nano_new(time2_make_time_t_orig(&tm_orig, utcp), nsec);
}

void
Init_time2(void)
{
    id_year = rb_intern("year");
    id_mon = rb_intern("mon");
    id_day = rb_intern("day");
    id_mday = rb_intern("mday");
    id_wday = rb_intern("wday");
    id_yday = rb_intern("yday");
    id_hour = rb_intern("hour");
    id_min = rb_intern("min");
    id_sec = rb_intern("sec");
    id_dst = rb_intern("dst");
    id_isdst = rb_intern("isdst");
    id_tz = rb_intern("tz");

    id_utc = rb_intern("utc");
    id_mul = rb_intern("*");

    /*{
		VALUE tz_dir;
		rb_require("tzdata"); /* should define $__tz_directory */
    /*	tz_dir = rb_gv_get("$__tz_directory");
		rb_tzdir = StringValueCStr(tz_dir);
	}*/


    rb_require("time"); /* defines strptime which we wil redefine later */
#ifndef RUBY_TIME_18_COMPAT
    rb_define_alias(rb_singleton_class(rb_cTime), "ruby_strptime", "strptime");
#endif

    /*rb_define_singleton_method(rb_cTime, "utc", time2_s_mkutc, -1);
    rb_define_singleton_method(rb_cTime, "gm", time2_s_mkutc, -1);
    rb_define_singleton_method(rb_cTime, "local", time2_s_mktime, -1);
    rb_define_singleton_method(rb_cTime, "mktime", time2_s_mktime, -1);
    */
    rb_define_singleton_method(rb_cTime, "strptime", time2_strptime, 2);
}

