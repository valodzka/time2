/**********************************************************************

  time.c -

  $Author$
  created at: Tue Dec 28 14:31:59 JST 1993

  Copyright (C) 1993-2007 Yukihiro Matsumoto


 **********************************************************************/
#include "c.h"
#include <time.h>
#include "pgtime.h"
#include "private.h"
#include "pgtz.h"
#include "tzfile.h"

#include "ruby/ruby.h"
#include "ruby/st.h"
#include <sys/types.h>
#include <errno.h>

#include "ruby/encoding.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>

#ifndef TYPEOF_TIMEVAL_TV_SEC
# define TYPEOF_TIMEVAL_TV_SEC time_t
#endif

VALUE rb_cTime;
VALUE rb_cTimeZone;

static VALUE time_utc_offset _((VALUE));
static struct pg_tz* timezone_default(struct pg_tz *dflt);
static struct pg_tz* timezone_utc();
static struct pg_tz* timezone_cached_get(const char *name);

static ID id_divmod, id_mul, id_submicro;

struct time_object {
    struct timespec ts;
    struct pg_tm tm;
    int gmt;
    int tm_got;
    struct pg_tz const * tz;
};

#ifdef OLD_TIME_COMPAT

struct old_time_object {
    struct timespec ts;
    struct tm tm;
    int gmt;
    int tm_got;
};

static void (*time_free)(void *) = NULL;

#  define IS_GMT(tobj) \
    (((tobj)->gmt > INT_MIN+1)?(rb_bug("time2: old style gmt value"),0):(((tobj)->gmt-INT_MIN)))
#  define GMT_TRUE(tobj)  ((tobj)->gmt = INT_MIN+1)
#  define GMT_FALSE(tobj)  ((tobj)->gmt = INT_MIN)
#  define GetTimeval(obj, tobj) do {								\
		if(TYPE(obj) == T_DATA && RDATA(obj)->dmark != time_mark) {	\
			struct old_time_object *old_tobj =						\
				(struct old_time_object *)DATA_PTR(obj);			\
			tobj = time_from_old_format(&(obj),(old_tobj));			\
		}															\
		else {														\
			Data_Get_Struct(obj, struct time_object, tobj);			\
		}															\
    } while(0)

#  define IS_BIN_TIME(t) ((TYPE(t) == T_DATA) && ((RDATA(t)->dfree == time_free)))
#else
#  define IS_GMT(obj) ((tobj)->gmt)
#  define GMT_TRUE(tobj)  ((tobj)->gmt = 1)
#  define GMT_FALSE(tobj)  ((tobj)->gmt = 0)
#  define GetTimeval(obj, tobj) \
     Data_Get_Struct(obj, struct time_object, tobj)
#  define IS_BIN_TIME(t) TYPE(t) == T_DATA && RDATA(t)->dfree == time_free
#endif

#define GetTZval(obj, tobj) \
    Data_Get_Struct(obj, struct pg_tz, tobj)
#define GetOrInitTZ(obj, tobj) do {                        \
    if (RTEST(rb_obj_is_kind_of(obj, rb_cTimeZone)))       \
        GetTZval(obj, tobj);                               \
     else                                                  \
        tobj = timezone_cached_get(StringValueCStr(obj));  \
   } while(0)

#ifndef OLD_TIME_COMPAT
static void
time_free(void *tobj)
{
    if (tobj) xfree(tobj);
}
#else

static void
time_mark(void *p)
{
}

#endif

static VALUE
time_s_alloc(VALUE klass)
{
    VALUE obj;
    struct time_object *tobj;
#ifdef OLD_TIME_COMPAT
    obj = Data_Make_Struct(klass, struct time_object, time_mark, time_free, tobj);
#else
    obj = Data_Make_Struct(klass, struct time_object, 0, time_free, tobj);
#endif
    tobj->tm_got=0;
    tobj->gmt = INT_MIN;
    tobj->ts.tv_sec = 0;
    tobj->ts.tv_nsec = 0;
    return obj;
}

#ifdef OLD_TIME_COMPAT
static struct time_object *
time_from_old_format(VALUE *obj, struct old_time_object *old_tobj)
{
	struct time_object *tobj;

	tobj = ALLOC(struct time_object);
	MEMZERO(tobj, struct time_object, 1);

	tobj->ts = old_tobj->ts;
	tobj->tm_got = 0;
	tobj->gmt = old_tobj->gmt + INT_MIN;
	tobj->tz = timezone_default(NULL);

	xfree(old_tobj);
	DATA_PTR(*obj) = tobj;
	RDATA(*obj)->dfree = time_free;
	RBASIC(RDATA(*obj))->klass = rb_cTime;

	return tobj;
}
#endif

static void
time_modify(VALUE time)
{
    rb_check_frozen(time);
    if (!OBJ_UNTRUSTED(time) && rb_safe_level() >= 4)
	rb_raise(rb_eSecurityError, "Insecure: can't modify Time");
}

/*
 *  Document-method: now
 *
 *  Synonym for <code>Time.new</code>. Returns a +Time+ object
 *  initialized to the current system time.
 */

/*
 *  call-seq:
 *     Time.new -> time
 *
 *  Returns a <code>Time</code> object initialized to the current system
 *  time. <b>Note:</b> The object created will be created using the
 *  resolution available on your system clock, and so may include
 *  fractional seconds.
 *
 *     a = Time.new      #=> 2007-11-19 07:50:02 -0600
 *     b = Time.new      #=> 2007-11-19 07:50:02 -0600
 *     a == b            #=> false
 *     "%.6f" % a.to_f   #=> "1195480202.282373"
 *     "%.6f" % b.to_f   #=> "1195480202.283415"
 *
 */

static VALUE
time_init(VALUE time)
{
    struct time_object *tobj;

    time_modify(time);
    GetTimeval(time, tobj);
    tobj->tm_got=0;
    tobj->ts.tv_sec = 0;
    tobj->ts.tv_nsec = 0;
    tobj->tz = timezone_default(NULL);
#ifdef HAVE_CLOCK_GETTIME
    if (clock_gettime(CLOCK_REALTIME, &tobj->ts) == -1) {
		rb_sys_fail("clock_gettime");
    }
#else
    {
        struct timeval tv;
        if (gettimeofday(&tv, 0) < 0) {
            rb_sys_fail("gettimeofday");
        }
        tobj->ts.tv_sec = tv.tv_sec;
        tobj->ts.tv_nsec = tv.tv_usec * 1000;
    }
#endif

    return time;
}

#define NDIV(x,y) (-(-((x)+1)/(y))-1)
#define NMOD(x,y) ((y)-(-((x)+1)%(y))-1)

static void
time_overflow_p(pg_time_t *secp, long *nsecp)
{
    pg_time_t tmp, sec = *secp;
    long nsec = *nsecp;

    if (nsec >= 1000000000) {	/* nsec positive overflow */
	tmp = sec + nsec / 1000000000;
	nsec %= 1000000000;
	if (sec > 0 && tmp < 0) {
	    rb_raise(rb_eRangeError, "out of Time range");
	}
	sec = tmp;
    }
    if (nsec < 0) {		/* nsec negative overflow */
	tmp = sec + NDIV(nsec,1000000000); /* negative div */
	nsec = NMOD(nsec,1000000000);      /* negative mod */
	if (sec < 0 && tmp > 0) {
	    rb_raise(rb_eRangeError, "out of Time range");
	}
	sec = tmp;
    }

    *secp = sec;
    *nsecp = nsec;
}

static VALUE
time_new_internal(VALUE klass, pg_time_t sec, long nsec, struct pg_tz const * tz)
{
    VALUE time = time_s_alloc(klass);
    struct time_object *tobj;

    GetTimeval(time, tobj);
    time_overflow_p(&sec, &nsec);
    tobj->ts.tv_sec = sec;
    tobj->ts.tv_nsec = nsec;
    tobj->tz = tz;

    return time;
}

VALUE
rb_time_with_tz_new(pg_time_t sec, long usec, struct pg_tz const * tz)
{
    return time_new_internal(rb_cTime, sec, usec * 1000, tz);
}

VALUE
rb_time_new(pg_time_t sec, long usec)
{
    return time_new_internal(rb_cTime, sec, usec * 1000, timezone_default(NULL));
}

VALUE
rb_time_with_tz_nano_new(pg_time_t sec, long nsec, struct pg_tz const * tz)
{
    return time_new_internal(rb_cTime, sec, nsec, tz);
}

VALUE
rb_time_nano_new(pg_time_t sec, long nsec)
{
    return time_new_internal(rb_cTime, sec, nsec, timezone_default(NULL));
}

static struct timespec
time_timespec(VALUE num, int interval)
{
    struct timespec t;
    const char *tstr = interval ? "time interval" : "time";
    VALUE i, f, ary;

    switch (TYPE(num)) {
	case T_FIXNUM:
		t.tv_sec = FIX2LONG(num);
		if (interval && t.tv_sec < 0)
			rb_raise(rb_eArgError, "%s must be positive", tstr);
		t.tv_nsec = 0;
		break;

	case T_FLOAT:
		if (interval && RFLOAT_VALUE(num) < 0.0)
			rb_raise(rb_eArgError, "%s must be positive", tstr);
		else {
			double f, d;

			d = modf(RFLOAT_VALUE(num), &f);
            if (d < 0) {
                d += 1;
                f -= 1;
            }
			t.tv_sec = (pg_time_t)f;
			if (f != t.tv_sec) {
				rb_raise(rb_eRangeError, "%f out of Time range", RFLOAT_VALUE(num));
			}
			t.tv_nsec = (long)(d*1e9+0.5);
		}
		break;

	case T_BIGNUM:
		t.tv_sec = NUM2LONG(num);
		if (interval && t.tv_sec < 0)
			rb_raise(rb_eArgError, "%s must be positive", tstr);
		t.tv_nsec = 0;
		break;

	default:
        if (rb_respond_to(num, id_divmod)) {
            ary = rb_check_array_type(rb_funcall(num, id_divmod, 1, INT2FIX(1)));
            if (NIL_P(ary)) {
                goto typeerror;
            }
            i = rb_ary_entry(ary, 0);
            f = rb_ary_entry(ary, 1);
            t.tv_sec = NUM2LONG(i);
            if (interval && t.tv_sec < 0)
                rb_raise(rb_eArgError, "%s must be positive", tstr);
            f = rb_funcall(f, id_mul, 1, INT2FIX(1000000000));
            t.tv_nsec = NUM2LONG(f);
        }
        else {
typeerror:
            rb_raise(rb_eTypeError, "can't convert %s into %s",
                     rb_obj_classname(num), tstr);
        }
		break;
    }
    return t;
}

static struct timeval
time_timeval(VALUE num, int interval)
{
    struct timespec ts;
    struct timeval tv;

    ts = time_timespec(num, interval);
    tv.tv_sec = (TYPEOF_TIMEVAL_TV_SEC)ts.tv_sec;
    tv.tv_usec = ts.tv_nsec / 1000;

    return tv;
}

struct timeval
rb_time_interval(VALUE num)
{
    return time_timeval(num, Qtrue);
}

struct timeval
rb_time_timeval(VALUE time)
{
    struct time_object *tobj;
    struct timeval t;

    if (IS_BIN_TIME(time)) {
	  GetTimeval(time, tobj);
	  t.tv_sec = (TYPEOF_TIMEVAL_TV_SEC)tobj->ts.tv_sec;
	  t.tv_usec = tobj->ts.tv_nsec / 1000;
	  return t;
    }
    return time_timeval(time, Qfalse);
}

static struct timespec
rb_time_timespec_static(VALUE time)
{
    struct time_object *tobj;
    struct timespec t;

    if (IS_BIN_TIME(time)) {
	  GetTimeval(time, tobj);
	  t = tobj->ts;
	  return t;
    }

    return time_timespec(time, Qfalse);
}

//TODO:HACK
struct timespec rb_time_timespec(VALUE time) { return rb_time_timespec_static(time); }

/*
 *  call-seq:
 *     Time.at(time) => time
 *     Time.at(seconds_with_frac) => time
 *     Time.at(seconds, microseconds_with_frac) => time
 *
 *  Creates a new time object with the value given by <i>time</i>,
 *  the given number of <i>seconds_with_frac</i>, or
 *  <i>seconds</i> and <i>microseconds_with_frac</i> from the Epoch.
 *  <i>seconds_with_frac</i> and <i>microseconds_with_frac</i>
 *  can be Integer, Float, Rational, or other Numeric.
 *  The offset to be negative.
 *
 *     Time.at(0)            #=> 1969-12-31 18:00:00 -0600
 *     Time.at(Time.at(0))   #=> 1969-12-31 18:00:00 -0600
 *     Time.at(946702800)    #=> 1999-12-31 23:00:00 -0600
 *     Time.at(-284061600)   #=> 1960-12-31 00:00:00 -0600
 *     Time.at(946684800.2).usec #=> 200000
 *     Time.at(946684800, 123456.789).nsec #=> 123456789
 */

static VALUE
time_s_at(int argc, VALUE *argv, VALUE klass)
{
    struct timespec ts;
    VALUE time, t;

    if (rb_scan_args(argc, argv, "11", &time, &t) == 2) {
	  ts.tv_sec = NUM2LONG(time);
	  ts.tv_nsec = NUM2LONG(rb_funcall(t, id_mul, 1, INT2FIX(1000)));
    }
    else {
	  //HACK: to sure that function will be caled from this module, not from ruby library
	  ts = rb_time_timespec_static(time);
    }
    t = time_new_internal(klass, ts.tv_sec, ts.tv_nsec, timezone_default(NULL));
    if (IS_BIN_TIME(time)) {
		struct time_object *tobj, *tobj2;

		GetTimeval(time, tobj);
		GetTimeval(t, tobj2);
		if(IS_GMT(tobj)) GMT_TRUE(tobj2);
		else GMT_FALSE(tobj2);

    }
    return t;
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
obj2nsec(VALUE obj, long *nsec)
{
    struct timespec ts;

    if (TYPE(obj) == T_STRING) {
	obj = rb_str_to_inum(obj, 10, Qfalse);
        *nsec = 0;
        return NUM2LONG(obj);
    }

    ts = time_timespec(obj, 1);
    *nsec = ts.tv_nsec;
    return (long)ts.tv_sec;
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

static void
time_arg(int argc, VALUE *argv, struct pg_tm *tm, long *nsec, struct pg_tz** tz)
{
    VALUE v[8];
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
	tm->tm_isdst = RTEST(argv[8]) ? 1 : 0;
        GetOrInitTZ(argv[9], *tz);
    }
    else {
	rb_scan_args(argc, argv, "17", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7]);
	/* v[6] may be usec or zone (parsedate) */
	/* v[7] is wday (parsedate; ignored) */
	tm->tm_wday = -1;
	tm->tm_isdst = -1;
        *tz = NULL;
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
    if (!NIL_P(v[6]) && argc == 7) {
        tm->tm_sec  = NIL_P(v[5])?0:obj2long(v[5]);
        *nsec = obj2long1000(v[6]);
    }
    else {
	/* when argc == 8, v[6] is timezone, but ignored */
        tm->tm_sec  = NIL_P(v[5])?0:obj2nsec(v[5], nsec);
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

static VALUE time_gmtime(VALUE);
static VALUE time_localtime(int argc, VALUE *argv, VALUE time);
static VALUE time_localtime_with_tz(VALUE time, struct pg_tz const * tz);
static VALUE time_get_tm(VALUE, int);

#define GMTIME(tm, result) pg_gmtime_r(tm, &result)
#define LOCALTIME(tm, result) pg_localtime_r(tm, timezone_default(NULL), &result)

static int
leap_year_p(long y)
{
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

#define DIV(n,d) ((n)<0 ? NDIV((n),(d)) : (n)/(d))

static pg_time_t
timegm_noleapsecond(struct pg_tm *tm)
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
	   (pg_time_t)(tm_yday +
		    (tm_year-70)*365 +
		    DIV(tm_year-69,4) -
		    DIV(tm_year-1,100) +
		    DIV(tm_year+299,400))*86400;
}

static int
tmcmp(struct pg_tm *a, struct pg_tm *b)
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
search_time_t(struct pg_tm *tptr, int utc_p)
{
    time_t guess, guess_lo, guess_hi;
    struct pg_tm *tm, tm_lo, tm_hi;
    int d, have_guess;
    int find_dst;
    struct pg_tm result;
#define GUESS(p) (utc_p ? GMTIME(p, result) : LOCALTIME(p, result))

    find_dst = 0 < tptr->tm_isdst;

    guess_lo = (time_t)~((unsigned_time_t)~(time_t)0 >> 1);
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
    return 0;			/* not reached */
}

static pg_time_t
make_time_t(struct pg_tm *tptr, struct pg_tz const *tz, int utc_p)
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
    
    return search_time_t(&buf, utc_p);
}

static VALUE
time_utc_or_local(int argc, VALUE *argv, int utc_p, VALUE klass)
{
    struct pg_tm tm;
    struct pg_tz *tz;
    VALUE time;
    long nsec;

    time_arg(argc, argv, &tm, &nsec, &tz);
    if (!tz) 
      tz = utc_p ? timezone_utc() : timezone_default(NULL);
    time = time_new_internal(klass, make_time_t(&tm, tz, utc_p), nsec, tz);
    if (utc_p) return time_gmtime(time);
    return time_localtime_with_tz(time, timezone_default(NULL));
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
time_s_mkutc(int argc, VALUE *argv, VALUE klass)
{
    return time_utc_or_local(argc, argv, Qtrue, klass);
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
time_s_mktime(int argc, VALUE *argv, VALUE klass)
{
    return time_utc_or_local(argc, argv, Qfalse, klass);
}

/*
 *  call-seq:
 *     time.to_i   => int
 *     time.tv_sec => int
 *
 *  Returns the value of <i>time</i> as an integer number of seconds
 *  since the Epoch.
 *
 *     t = Time.now
 *     "%10.5f" % t.to_f   #=> "1049896564.17839"
 *     t.to_i              #=> 1049896564
 */

static VALUE
time_to_i(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
#ifdef TIMET2NUM /* COMPAT */
    return TIMET2NUM(tobj->ts.tv_sec);
#else
	return LONG2NUM(tobj->ts.tv_sec);
#endif
}

/*
 *  call-seq:
 *     time.to_f => float
 *
 *  Returns the value of <i>time</i> as a floating point number of
 *  seconds since the Epoch.
 *
 *     t = Time.now
 *     "%10.5f" % t.to_f   #=> "1049896564.13654"
 *     t.to_i              #=> 1049896564
 *
 *  Note that IEEE 754 double is not accurate enough to represent
 *  nanoseconds from the Epoch.
 */

static VALUE
time_to_f(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    return DBL2NUM((double)tobj->ts.tv_sec+(double)tobj->ts.tv_nsec/1e9);
}

/*
 *  call-seq:
 *     time.usec    => int
 *     time.tv_usec => int
 *
 *  Returns just the number of microseconds for <i>time</i>.
 *
 *     t = Time.now        #=> 2007-11-19 08:03:26 -0600
 *     "%10.6f" % t.to_f   #=> "1195481006.775195"
 *     t.usec              #=> 775195
 */

static VALUE
time_usec(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    return LONG2NUM(tobj->ts.tv_nsec/1000);
}

/*
 *  call-seq:
 *     time.nsec    => int
 *     time.tv_nsec => int
 *
 *  Returns just the number of nanoseconds for <i>time</i>.
 *
 *     t = Time.now        #=> 2007-11-17 15:18:03 +0900
 *     "%10.9f" % t.to_f   #=> "1195280283.536151409"
 *     t.nsec              #=> 536151406
 *
 *  The lowest digit of to_f and nsec is different because
 *  IEEE 754 double is not accurate enough to represent
 *  nanoseconds from the Epoch.
 *  The accurate value is returned by nsec.
 */

static VALUE
time_nsec(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    return LONG2NUM(tobj->ts.tv_nsec);
}

/*
 *  call-seq:
 *     time <=> other_time => -1, 0, +1
 *
 *  Comparison---Compares <i>time</i> with <i>other_time</i>.
 *
 *     t = Time.now       #=> 2007-11-19 08:12:12 -0600
 *     t2 = t + 2592000   #=> 2007-12-19 08:12:12 -0600
 *     t <=> t2           #=> -1
 *     t2 <=> t           #=> 1
 *
 *     t = Time.now       #=> 2007-11-19 08:13:38 -0600
 *     t2 = t + 0.1       #=> 2007-11-19 08:13:38 -0600
 *     t.nsec             #=> 98222999
 *     t2.nsec            #=> 198222999
 *     t <=> t2           #=> -1
 *     t2 <=> t           #=> 1
 *     t <=> t            #=> 0
 */

static VALUE
time_cmp(VALUE time1, VALUE time2)
{
    struct time_object *tobj1, *tobj2;

    GetTimeval(time1, tobj1);
    if (IS_BIN_TIME(time2)) {
		GetTimeval(time2, tobj2);
		if (tobj1->ts.tv_sec == tobj2->ts.tv_sec) {
			if (tobj1->ts.tv_nsec == tobj2->ts.tv_nsec) return INT2FIX(0);
			if (tobj1->ts.tv_nsec > tobj2->ts.tv_nsec) return INT2FIX(1);
			return INT2FIX(-1);
		}
		if (tobj1->ts.tv_sec > tobj2->ts.tv_sec) return INT2FIX(1);
		return INT2FIX(-1);
    }
    else {
		VALUE cmp;
		int n;

		cmp = rb_funcall(time2, rb_intern("<=>"), 1, time1);
		if (NIL_P(cmp)) return Qnil;

		n = rb_cmpint(cmp, time1, time2);
		if (n == 0) return INT2FIX(0);
		if (n > 0) return INT2FIX(1);
		return INT2FIX(-1);
    }
}

/*
 * call-seq:
 *  time.eql?(other_time)
 *
 * Return <code>true</code> if <i>time</i> and <i>other_time</i> are
 * both <code>Time</code> objects with the same seconds and fractional
 * seconds.
 */

static VALUE
time_eql(VALUE time1, VALUE time2)
{
    struct time_object *tobj1, *tobj2;

    GetTimeval(time1, tobj1);
    if (IS_BIN_TIME(time2)) {
		GetTimeval(time2, tobj2);
	if (tobj1->ts.tv_sec == tobj2->ts.tv_sec) {
	    if (tobj1->ts.tv_nsec == tobj2->ts.tv_nsec) return Qtrue;
	}
    }
    return Qfalse;
}

/*
 *  call-seq:
 *     time.utc? => true or false
 *     time.gmt? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents a time in UTC
 *  (GMT).
 *
 *     t = Time.now                        #=> 2007-11-19 08:15:23 -0600
 *     t.utc?                              #=> false
 *     t = Time.gm(2000,"jan",1,20,15,1)   #=> 2000-01-01 20:15:01 UTC
 *     t.utc?                              #=> true
 *
 *     t = Time.now                        #=> 2007-11-19 08:16:03 -0600
 *     t.gmt?                              #=> false
 *     t = Time.gm(2000,1,1,20,15,1)       #=> 2000-01-01 20:15:01 UTC
 *     t.gmt?                              #=> true
 */

static VALUE
time_utc_p(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    return IS_GMT(tobj) ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *   time.hash   => fixnum
 *
 * Return a hash code for this time object.
 */

static VALUE
time_hash(VALUE time)
{
	/* COMPAT
	TODO: find way to check if new api avaliable
    struct time_object *tobj;
    long hash = 0;

    GetTimeval(time, tobj);
#if SIZEOF_TIME_T > SIZEOF_INT
    hash = rb_hash_start((unsigned int)(tobj->ts.tv_sec >> (SIZEOF_INT * CHAR_BIT)));
    hash = rb_hash_uint(hash, (unsigned int)tobj->ts.tv_sec);
#else
    hash = rb_hash_start((unsigned int)tobj->ts.tv_sec);
#endif
    hash = rb_hash_end(rb_hash_uint(hash, tobj->ts.tv_nsec));
    return LONG2FIX(hash);
	*/
	struct time_object *tobj;
	long hash;

	GetTimeval(time, tobj);
	hash = tobj->ts.tv_sec ^ tobj->ts.tv_nsec;
	return LONG2FIX(hash);
}

/* :nodoc: */
static VALUE
time_init_copy(VALUE copy, VALUE time)
{
    struct time_object *tobj, *tcopy;

    if (copy == time) return copy;
    time_modify(copy);
    if (!IS_BIN_TIME(time)) {
		rb_raise(rb_eTypeError, "wrong argument type");
    }

    GetTimeval(time, tobj);
    GetTimeval(copy, tcopy);
    MEMCPY(tcopy, tobj, struct time_object, 1);

    return copy;
}

static VALUE
time_dup(VALUE time)
{
    VALUE dup = time_s_alloc(CLASS_OF(time));
    time_init_copy(dup, time);
    return dup;
}

static VALUE
time_localtime_with_tz(VALUE time, struct pg_tz const * tz)
{
    struct time_object *tobj;
    struct pg_tm *tm_tmp;
    pg_time_t t;
    struct pg_tm result;

    GetTimeval(time, tobj);

    if (!IS_GMT(tobj)) {
	if (tobj->tm_got && tobj->tz == tz)
	    return time;
    }
    else {
		time_modify(time);
    }
    t = tobj->ts.tv_sec;
    tm_tmp = pg_localtime_r(&t, tz, &result);
    if (!tm_tmp) rb_raise(rb_eArgError, "localtime error");
    tobj->tm = *tm_tmp;
    tobj->tz = tz;
    tobj->tm_got = 1;
    GMT_FALSE(tobj);
    return time;
}

/*
 *  call-seq:
 *     time.localtime => time
 *
 *  Converts <i>time</i> to local time (using the local time zone in
 *  effect for this process) modifying the receiver.
 *
 *     t = Time.gm(2000, "jan", 1, 20, 15, 1)  #=> 2000-01-01 20:15:01 UTC
 *     t.gmt?                                  #=> true
 *     t.localtime                             #=> 2000-01-01 14:15:01 -0600
 *     t.gmt?                                  #=> false
 */

static VALUE
time_localtime(int argc, VALUE *argv, VALUE time)
{
    VALUE tzobj;
    struct pg_tz* tz;

    if(rb_scan_args(argc, argv, "01", &tzobj)==0)
        tz = timezone_default(NULL);
    else {
        GetOrInitTZ(tzobj, tz);
    }

    return time_localtime_with_tz(time, tz);
}

/*
 *  call-seq:
 *     time.gmtime    => time
 *     time.utc       => time
 *
 *  Converts <i>time</i> to UTC (GMT), modifying the receiver.
 *
 *     t = Time.now   #=> 2007-11-19 08:18:31 -0600
 *     t.gmt?         #=> false
 *     t.gmtime       #=> 2007-11-19 14:18:31 UTC
 *     t.gmt?         #=> true
 *
 *     t = Time.now   #=> 2007-11-19 08:18:51 -0600
 *     t.utc?         #=> false
 *     t.utc          #=> 2007-11-19 14:18:51 UTC
 *     t.utc?         #=> true
 */

static VALUE
time_gmtime(VALUE time)
{
    struct time_object *tobj;
    struct pg_tm *tm_tmp;
    pg_time_t t;
    struct pg_tm result;

    GetTimeval(time, tobj);
    if (IS_GMT(tobj)) {
		if (tobj->tm_got)
			return time;
    }
    else {
		time_modify(time);
    }
    t = tobj->ts.tv_sec;
    tm_tmp = GMTIME(&t, result);
    if (!tm_tmp)
	rb_raise(rb_eArgError, "gmtime error");
    tobj->tm = *tm_tmp;
    tobj->tm_got = 1;
    GMT_TRUE(tobj);
    return time;
}

/*
 *  call-seq:
 *     time.getlocal => new_time
 *
 *  Returns a new <code>new_time</code> object representing <i>time</i> in
 *  local time (using the local time zone in effect for this process).
 *
 *     t = Time.gm(2000,1,1,20,15,1)   #=> 2000-01-01 20:15:01 UTC
 *     t.gmt?                          #=> true
 *     l = t.getlocal                  #=> 2000-01-01 14:15:01 -0600
 *     l.gmt?                          #=> false
 *     t == l                          #=> true
 */

static VALUE
time_getlocaltime(int argc, VALUE *argv, VALUE time)
{
    return time_localtime(argc, argv, time_dup(time));
}

/*
 *  call-seq:
 *     time.getgm  => new_time
 *     time.getutc => new_time
 *
 *  Returns a new <code>new_time</code> object representing <i>time</i> in
 *  UTC.
 *
 *     t = Time.local(2000,1,1,20,15,1)   #=> 2000-01-01 20:15:01 -0600
 *     t.gmt?                             #=> false
 *     y = t.getgm                        #=> 2000-01-02 02:15:01 UTC
 *     y.gmt?                             #=> true
 *     t == y                             #=> true
 */

static VALUE
time_getgmtime(VALUE time)
{
    return time_gmtime(time_dup(time));
}

static VALUE
time_get_tm(VALUE time, int gmt)
{
    if (gmt) return time_gmtime(time);
    else {
        struct time_object* tobj;
        GetTimeval(time, tobj);
        return time_localtime_with_tz(time, tobj->tz);
    }
}

/*
 *  call-seq:
 *     time.asctime => string
 *     time.ctime   => string
 *
 *  Returns a canonical string representation of <i>time</i>.
 *
 *     Time.now.asctime   #=> "Wed Apr  9 08:56:03 2003"
 */

static VALUE
time_asctime(VALUE time)
{
    struct time_object *tobj;
    char *s;
    char buf[STD_ASCTIME_BUF_SIZE];

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    s = pg_asctime_r(&tobj->tm, buf);
    if(!s) rb_sys_fail("asctime");

    return rb_str_new2(s);
}

#ifdef HAVE_TM_ZONE
#  define COPY_TZ(expr) expr
#else
#  define COPY_TZ(expr) (0)
#endif

#ifdef HAVE_STRUCT_TM_TM_GMTOFF
#  define COPY_GMTOFF(expr) expr
#else
#  define COPY_GMTOFF(expr) (0)
#endif


#define RB_STRFTIME(s, maxsize, format, timeptr, ts, gmt, ret) do {	\
	struct tm old_system_tm;					\
    	old_system_tm.tm_sec = (timeptr)->tm_sec;			\
	old_system_tm.tm_min = (timeptr)->tm_min;			\
	old_system_tm.tm_hour = (timeptr)->tm_hour;			\
	old_system_tm.tm_mday = (timeptr)->tm_mday;			\
	old_system_tm.tm_mon = (timeptr)->tm_mon;			\
	old_system_tm.tm_year = (timeptr)->tm_year;			\
	old_system_tm.tm_wday = (timeptr)->tm_wday;			\
	old_system_tm.tm_yday = (timeptr)->tm_yday;			\
	old_system_tm.tm_isdst = (timeptr)->tm_isdst;			\
	COPY_GMTOFF(old_system_tm.tm_gmtoff = (timeptr)->tm_gmtoff);	\
	COPY_TZ(old_system_tm.tm_zone = (timeptr)->tm_zone);		\
        len = rb_strftime(s, maxsize, format,				\
			  &old_system_tm, ts, gmt);			\
    } while(0)

size_t
rb_strftime(char *s, size_t maxsize, const char *format,
	    const struct tm *timeptr, const struct timespec *ts, int gmt);

/*
 *  call-seq:
 *     time.inspect => string
 *     time.to_s    => string
 *
 *  Returns a string representing <i>time</i>. Equivalent to calling
 *  <code>Time#strftime</code> with a format string of
 *  ``<code>%Y-%m-%d</code> <code>%H:%M:%S</code> <code>%z</code>''
 *  for a local time and
 *  ``<code>%Y-%m-%d</code> <code>%H:%M:%S</code> <code>UTC</code>''
 *  for a UTC time.
 *
 *     Time.now.to_s       #=> "2007-10-05 16:09:51 +0900"
 *     Time.now.utc.to_s   #=> "2007-10-05 07:09:51 UTC"
 */

static VALUE
time_to_s(VALUE time)
{
    struct time_object *tobj;
    char buf[128];
    int len;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
	time_get_tm(time, IS_GMT(tobj));
    }

    RB_STRFTIME(buf, sizeof(buf),
		IS_GMT(tobj) ? "%Y-%m-%d %H:%M:%S UTC" : "%Y-%m-%d %H:%M:%S %z",
		&tobj->tm, &tobj->ts, IS_GMT(tobj), len);

    return rb_str_new(buf, len);
}

static VALUE
time_add(struct time_object *tobj, VALUE offset, int sign)
{
    double v = NUM2DBL(offset);
    double f, d;
    unsigned_time_t sec_off;
    pg_time_t sec;
    long nsec_off, nsec;
    VALUE result;

    if (v < 0) {
	v = -v;
	sign = -sign;
    }
    d = modf(v, &f);
    sec_off = (unsigned_time_t)f;
    if (f != (double)sec_off)
	rb_raise(rb_eRangeError, "time %s %f out of Time range",
		 sign < 0 ? "-" : "+", v);
    nsec_off = (long)(d*1e9+0.5);

    if (sign < 0) {
	sec = tobj->ts.tv_sec - sec_off;
	nsec = tobj->ts.tv_nsec - nsec_off;
	if (sec > tobj->ts.tv_sec)
	    rb_raise(rb_eRangeError, "time - %f out of Time range", v);
    }
    else {
	sec = tobj->ts.tv_sec + sec_off;
	nsec = tobj->ts.tv_nsec + nsec_off;
	if (sec < tobj->ts.tv_sec)
	    rb_raise(rb_eRangeError, "time + %f out of Time range", v);
    }
    result = rb_time_with_tz_nano_new(sec, nsec, tobj->tz);
    if (IS_GMT(tobj)) {
		GetTimeval(result, tobj);
		GMT_TRUE(tobj);
    }
    return result;
}

/*
 *  call-seq:
 *     time + numeric => time
 *
 *  Addition---Adds some number of seconds (possibly fractional) to
 *  <i>time</i> and returns that value as a new time.
 *
 *     t = Time.now         #=> 2007-11-19 08:22:21 -0600
 *     t + (60 * 60 * 24)   #=> 2007-11-20 08:22:21 -0600
 */

static VALUE
time_plus(VALUE time1, VALUE time2)
{
    struct time_object *tobj;
    GetTimeval(time1, tobj);

    if (IS_BIN_TIME(time2)) {
		rb_raise(rb_eTypeError, "time + time?");
    }
    return time_add(tobj, time2, 1);
}

/*
 *  call-seq:
 *     time - other_time => float
 *     time - numeric    => time
 *
 *  Difference---Returns a new time that represents the difference
 *  between two times, or subtracts the given number of seconds in
 *  <i>numeric</i> from <i>time</i>.
 *
 *     t = Time.now       #=> 2007-11-19 08:23:10 -0600
 *     t2 = t + 2592000   #=> 2007-12-19 08:23:10 -0600
 *     t2 - t             #=> 2592000.0
 *     t2 - 2592000       #=> 2007-11-19 08:23:10 -0600
 */

static VALUE
time_minus(VALUE time1, VALUE time2)
{
    struct time_object *tobj;

    GetTimeval(time1, tobj);
    if (IS_BIN_TIME(time2)) {
		struct time_object *tobj2;
		double f;

		GetTimeval(time2, tobj2);
        if (tobj->ts.tv_sec < tobj2->ts.tv_sec)
            f = -(double)(unsigned_time_t)(tobj2->ts.tv_sec - tobj->ts.tv_sec);
        else
            f = (double)(unsigned_time_t)(tobj->ts.tv_sec - tobj2->ts.tv_sec);
		f += ((double)tobj->ts.tv_nsec - (double)tobj2->ts.tv_nsec)*1e-9;

		return DBL2NUM(f);
    }
    return time_add(tobj, time2, -1);
}

/*
 * call-seq:
 *   time.succ   => new_time
 *
 * Return a new time object, one second later than <code>time</code>.
 *
 *     t = Time.now       #=> 2007-11-19 08:23:57 -0600
 *     t.succ             #=> 2007-11-19 08:23:58 -0600
 */

static VALUE
time_succ(VALUE time)
{
    struct time_object *tobj;
    int gmt;

    GetTimeval(time, tobj);
    gmt = IS_GMT(tobj);
    time = rb_time_with_tz_nano_new(tobj->ts.tv_sec + 1, tobj->ts.tv_nsec, tobj->tz);
    GetTimeval(time, tobj);
	if (gmt)
		GMT_TRUE(tobj);
	else
		GMT_FALSE(tobj); return time;
}

VALUE
rb_time_succ(VALUE time)
{
  return time_succ(time);
}

/*
 *  call-seq:
 *     time.sec => fixnum
 *
 *  Returns the second of the minute (0..60)<em>[Yes, seconds really can
 *  range from zero to 60. This allows the system to inject leap seconds
 *  every now and then to correct for the fact that years are not really
 *  a convenient number of hours long.]</em> for <i>time</i>.
 *
 *     t = Time.now   #=> 2007-11-19 08:25:02 -0600
 *     t.sec          #=> 2
 */

static VALUE
time_sec(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_sec);
}

/*
 *  call-seq:
 *     time.min => fixnum
 *
 *  Returns the minute of the hour (0..59) for <i>time</i>.
 *
 *     t = Time.now   #=> 2007-11-19 08:25:51 -0600
 *     t.min          #=> 25
 */

static VALUE
time_min(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_min);
}

/*
 *  call-seq:
 *     time.hour => fixnum
 *
 *  Returns the hour of the day (0..23) for <i>time</i>.
 *
 *     t = Time.now   #=> 2007-11-19 08:26:20 -0600
 *     t.hour         #=> 8
 */

static VALUE
time_hour(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_hour);
}

/*
 *  call-seq:
 *     time.day  => fixnum
 *     time.mday => fixnum
 *
 *  Returns the day of the month (1..n) for <i>time</i>.
 *
 *     t = Time.now   #=> 2007-11-19 08:27:03 -0600
 *     t.day          #=> 19
 *     t.mday         #=> 19
 */

static VALUE
time_mday(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_mday);
}

/*
 *  call-seq:
 *     time.mon   => fixnum
 *     time.month => fixnum
 *
 *  Returns the month of the year (1..12) for <i>time</i>.
 *
 *     t = Time.now   #=> 2007-11-19 08:27:30 -0600
 *     t.mon          #=> 11
 *     t.month        #=> 11
 */

static VALUE
time_mon(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_mon+1);
}

/*
 *  call-seq:
 *     time.year => fixnum
 *
 *  Returns the year for <i>time</i> (including the century).
 *
 *     t = Time.now   #=> 2007-11-19 08:27:51 -0600
 *     t.year         #=> 2007
 */

static VALUE
time_year(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return LONG2NUM((long)tobj->tm.tm_year+1900);
}

/*
 *  call-seq:
 *     time.wday => fixnum
 *
 *  Returns an integer representing the day of the week, 0..6, with
 *  Sunday == 0.
 *
 *     t = Time.now   #=> 2007-11-20 02:35:35 -0600
 *     t.wday         #=> 2
 *     t.sunday?      #=> false
 *     t.monday?      #=> false
 *     t.tuesday?     #=> true
 *     t.wednesday?   #=> false
 *     t.thursday?    #=> false
 *     t.friday?      #=> false
 *     t.saturday?    #=> false
 */

static VALUE
time_wday(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_wday);
}

#define wday_p(n) {\
    struct time_object *tobj;\
    GetTimeval(time, tobj);\
    if (tobj->tm_got == 0) {\
		time_get_tm(time, IS_GMT(tobj));	\
    }\
    return (tobj->tm.tm_wday == (n)) ? Qtrue : Qfalse;\
}

/*
 *  call-seq:
 *     time.sunday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Sunday.
 *
 *     t = Time.local(1990, 4, 1)       #=> 1990-04-01 00:00:00 -0600
 *     t.sunday?                        #=> true
 */

static VALUE
time_sunday(VALUE time)
{
    wday_p(0);
}

/*
 *  call-seq:
 *     time.monday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Monday.
 *
 *     t = Time.local(2003, 8, 4)       #=> 2003-08-04 00:00:00 -0500
 *     p t.monday?                      #=> true
 */

static VALUE
time_monday(VALUE time)
{
    wday_p(1);
}

/*
 *  call-seq:
 *     time.tuesday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Tuesday.
 *
 *     t = Time.local(1991, 2, 19)      #=> 1991-02-19 00:00:00 -0600
 *     p t.tuesday?                     #=> true
 */

static VALUE
time_tuesday(VALUE time)
{
    wday_p(2);
}

/*
 *  call-seq:
 *     time.wednesday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Wednesday.
 *
 *     t = Time.local(1993, 2, 24)      #=> 1993-02-24 00:00:00 -0600
 *     p t.wednesday?                   #=> true
 */

static VALUE
time_wednesday(VALUE time)
{
    wday_p(3);
}

/*
 *  call-seq:
 *     time.thursday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Thursday.
 *
 *     t = Time.local(1995, 12, 21)     #=> 1995-12-21 00:00:00 -0600
 *     p t.thursday?                    #=> true
 */

static VALUE
time_thursday(VALUE time)
{
    wday_p(4);
}

/*
 *  call-seq:
 *     time.friday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Friday.
 *
 *     t = Time.local(1987, 12, 18)     #=> 1987-12-18 00:00:00 -0600
 *     t.friday?                        #=> true
 */

static VALUE
time_friday(VALUE time)
{
    wday_p(5);
}

/*
 *  call-seq:
 *     time.saturday? => true or false
 *
 *  Returns <code>true</code> if <i>time</i> represents Saturday.
 *
 *     t = Time.local(2006, 6, 10)      #=> 2006-06-10 00:00:00 -0500
 *     t.saturday?                      #=> true
 */

static VALUE
time_saturday(VALUE time)
{
    wday_p(6);
}

/*
 *  call-seq:
 *     time.yday => fixnum
 *
 *  Returns an integer representing the day of the year, 1..366.
 *
 *     t = Time.now   #=> 2007-11-19 08:32:31 -0600
 *     t.yday         #=> 323
 */

static VALUE
time_yday(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return INT2FIX(tobj->tm.tm_yday+1);
}

/*
 *  call-seq:
 *     time.isdst => true or false
 *     time.dst?  => true or false
 *
 *  Returns <code>true</code> if <i>time</i> occurs during Daylight
 *  Saving Time in its time zone.
 *
 *   # CST6CDT:
 *     Time.local(2000, 1, 1).zone    #=> "CST"
 *     Time.local(2000, 1, 1).isdst   #=> false
 *     Time.local(2000, 1, 1).dst?    #=> false
 *     Time.local(2000, 7, 1).zone    #=> "CDT"
 *     Time.local(2000, 7, 1).isdst   #=> true
 *     Time.local(2000, 7, 1).dst?    #=> true
 *
 *   # Asia/Tokyo:
 *     Time.local(2000, 1, 1).zone    #=> "JST"
 *     Time.local(2000, 1, 1).isdst   #=> false
 *     Time.local(2000, 1, 1).dst?    #=> false
 *     Time.local(2000, 7, 1).zone    #=> "JST"
 *     Time.local(2000, 7, 1).isdst   #=> false
 *     Time.local(2000, 7, 1).dst?    #=> false
 */

static VALUE
time_isdst(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return tobj->tm.tm_isdst?Qtrue:Qfalse;
}

/*
 *  call-seq:
 *     time.zone => string
 *
 *  Returns the name of the time zone used for <i>time</i>. As of Ruby
 *  1.8, returns ``UTC'' rather than ``GMT'' for UTC times.
 *
 *     t = Time.gm(2000, "jan", 1, 20, 15, 1)
 *     t.zone   #=> "UTC"
 *     t = Time.local(2000, "jan", 1, 20, 15, 1)
 *     t.zone   #=> "CST"
 */

static VALUE
time_zone(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }

    if (IS_GMT(tobj)) {
		return rb_str_new2("UTC");
    }

    return rb_str_new2(tobj->tm.tm_zone);
}

/*
 *  call-seq:
 *     time.gmt_offset => fixnum
 *     time.gmtoff     => fixnum
 *     time.utc_offset => fixnum
 *
 *  Returns the offset in seconds between the timezone of <i>time</i>
 *  and UTC.
 *
 *     t = Time.gm(2000,1,1,20,15,1)   #=> 2000-01-01 20:15:01 UTC
 *     t.gmt_offset                    #=> 0
 *     l = t.getlocal                  #=> 2000-01-01 14:15:01 -0600
 *     l.gmt_offset                    #=> -21600
 */

static VALUE
time_utc_offset(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }

    if (IS_GMT(tobj)) {
		return INT2FIX(0);
    }
    else {
		return INT2NUM(tobj->tm.tm_gmtoff);
    }
}

/*
 *  call-seq:
 *     time.to_a => array
 *
 *  Returns a ten-element <i>array</i> of values for <i>time</i>:
 *  {<code>[ sec, min, hour, day, month, year, wday, yday, isdst, zone
 *  ]</code>}. See the individual methods for an explanation of the
 *  valid ranges of each value. The ten elements can be passed directly
 *  to <code>Time::utc</code> or <code>Time::local</code> to create a
 *  new <code>Time</code>.
 *
 *     t = Time.now     #=> 2007-11-19 08:36:01 -0600
 *     now = t.to_a     #=> [1, 36, 8, 19, 11, 2007, 1, 323, false, "CST"]
 */

static VALUE
time_to_a(VALUE time)
{
    struct time_object *tobj;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    return rb_ary_new3(10,
		    INT2FIX(tobj->tm.tm_sec),
		    INT2FIX(tobj->tm.tm_min),
		    INT2FIX(tobj->tm.tm_hour),
		    INT2FIX(tobj->tm.tm_mday),
		    INT2FIX(tobj->tm.tm_mon+1),
		    LONG2NUM((long)tobj->tm.tm_year+1900),
		    INT2FIX(tobj->tm.tm_wday),
		    INT2FIX(tobj->tm.tm_yday+1),
		    tobj->tm.tm_isdst?Qtrue:Qfalse,
		    time_zone(time));
}

#define SMALLBUF 100
static int
rb_strftime_alloc(char **buf, const char *format,
		  struct pg_tm *time, struct timespec *ts, int gmt)
{
    int size, len, flen;

    (*buf)[0] = '\0';
    flen = strlen(format);
    if (flen == 0) {
	return 0;
    }
    errno = 0;
    RB_STRFTIME(*buf, SMALLBUF, format, time, ts, gmt, len);
    if (len != 0 || (**buf == '\0' && errno != ERANGE)) return len;
    for (size=1024; ; size*=2) {
	*buf = xmalloc(size);
	(*buf)[0] = '\0';
	RB_STRFTIME(*buf, size, format, time, ts, gmt, len);
	/*
	 * buflen can be zero EITHER because there's not enough
	 * room in the string, or because the control command
	 * goes to the empty string. Make a reasonable guess that
	 * if the buffer is 1024 times bigger than the length of the
	 * format string, it's not failing for lack of room.
	 */
	if (len > 0 || size >= 1024 * flen) return len;
	xfree(*buf);
    }
    /* not reached */
}

/*
 *  call-seq:
 *     time.strftime( string ) => string
 *
 *  Formats <i>time</i> according to the directives in the given format
 *  string. Any text not listed as a directive will be passed through
 *  to the output string.
 *
 *  Format meaning:
 *    %a - The abbreviated weekday name (``Sun'')
 *    %A - The  full  weekday  name (``Sunday'')
 *    %b - The abbreviated month name (``Jan'')
 *    %B - The  full  month  name (``January'')
 *    %c - The preferred local date and time representation
 *    %d - Day of the month (01..31)
 *    %F - Equivalent to %Y-%m-%d (the ISO 8601 date format)
 *    %H - Hour of the day, 24-hour clock (00..23)
 *    %I - Hour of the day, 12-hour clock (01..12)
 *    %j - Day of the year (001..366)
 *    %L - Millisecond of the second (000..999)
 *    %m - Month of the year (01..12)
 *    %M - Minute of the hour (00..59)
 *    %N - Fractional seconds digits, default is 9 digits (nanosecond)
 *            %3N  millisecond (3 digits)
 *            %6N  microsecond (6 digits)
 *            %9N  nanosecond (9 digits)
 *    %p - Meridian indicator (``AM''  or  ``PM'')
 *    %P - Meridian indicator (``am''  or  ``pm'')
 *    %s - Number of seconds since 1970-01-01 00:00:00 UTC.
 *    %S - Second of the minute (00..60)
 *    %U - Week  number  of the current year,
 *            starting with the first Sunday as the first
 *            day of the first week (00..53)
 *    %W - Week  number  of the current year,
 *            starting with the first Monday as the first
 *            day of the first week (00..53)
 *    %w - Day of the week (Sunday is 0, 0..6)
 *    %x - Preferred representation for the date alone, no time
 *    %X - Preferred representation for the time alone, no date
 *    %y - Year without a century (00..99)
 *    %Y - Year with century
 *    %Z - Time zone name
 *    %% - Literal ``%'' character
 *
 *     t = Time.now                        #=> 2007-11-19 08:37:48 -0600
 *     t.strftime("Printed on %m/%d/%Y")   #=> "Printed on 11/19/2007"
 *     t.strftime("at %I:%M%p")            #=> "at 08:37AM"
 */

static VALUE
time_strftime(VALUE time, VALUE format)
{
    void rb_enc_copy(VALUE, VALUE);
    struct time_object *tobj;
    char buffer[SMALLBUF], *buf = buffer;
    const char *fmt;
    long len;
    VALUE str;

    GetTimeval(time, tobj);
    if (tobj->tm_got == 0) {
		time_get_tm(time, IS_GMT(tobj));
    }
    StringValue(format);
    if (!rb_enc_str_asciicompat_p(format)) {
	rb_raise(rb_eArgError, "format should have ASCII compatible encoding");
    }
    format = rb_str_new4(format);
    fmt = RSTRING_PTR(format);
    len = RSTRING_LEN(format);
    if (len == 0) {
	rb_warning("strftime called with empty format string");
    }
    else if (memchr(fmt, '\0', len)) {
	/* Ruby string may contain \0's. */
	const char *p = fmt, *pe = fmt + len;

	str = rb_str_new(0, 0);
	while (p < pe) {
	    len = rb_strftime_alloc(&buf, p, &tobj->tm, &tobj->ts, IS_GMT(tobj));
	    rb_str_cat(str, buf, len);
	    p += strlen(p);
	    if (buf != buffer) {
		xfree(buf);
		buf = buffer;
	    }
	    for (fmt = p; p < pe && !*p; ++p);
	    if (p > fmt) rb_str_cat(str, fmt, p - fmt);
	}
	return str;
    }
    else {
	len = rb_strftime_alloc(&buf, RSTRING_PTR(format),
							&tobj->tm, &tobj->ts, IS_GMT(tobj));
    }
    str = rb_str_new(buf, len);
    if (buf != buffer) xfree(buf);
    rb_enc_copy(str, format);
    return str;
}

static void
time_fill_gaps_tm(struct pg_tm *tm, struct pg_tz const * tz)
{
	struct pg_tm tm_now;
	int fill_now = 1;

	//if (tm->tm_yday != INT_MIN) {
	  //}

	if (tm->tm_year == INT_MIN) {
		pg_time_t now = time(NULL);
		pg_localtime_r(&now, tz, &tm_now);
		tm->tm_year = tm_now.tm_year;
	}
	else
		fill_now = 0;

	if (tm->tm_yday != INT_MIN) {
		// let mktime will detect appropriate month and day
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
 * The format is composed of zero or more directives.Each directive is composed
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
 * - <s>%L - Millisecond of the second (000..999)</s>
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
 * - '\0' in string will cause error
 * - string with encoding
 */
static VALUE
time_strptime(VALUE klass, VALUE str, VALUE format) // quick unsafe implementation
{
    struct pg_tm tm;
    struct pg_tz *tz;
    VALUE time_obj;
    int utc_p;
    long nsec = 0;
    const char *p;

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

    p = pg_strptime(StringValueCStr(str), StringValueCStr(format), &tm, &nsec, timezone_default(NULL));

    if (!p) rb_raise(rb_eArgError, "strptime error");

    if(tm.tm_zone && strcmp("GMT", tm.tm_zone) == 0) {
        tz = timezone_utc();
        utc_p = 1;
    }
    else {//currently no timezone support
        tz = timezone_default(NULL);
        utc_p = 0;
    }
    time_fill_gaps_tm(&tm, tz);
    time_obj = time_new_internal(klass, make_time_t(&tm, tz, utc_p), nsec, tz);

    return utc_p ? time_gmtime(time_obj) : time_localtime_with_tz(time_obj, tz);
}

/*
 * undocumented
 */

static VALUE
time_mdump(VALUE time)
{
    struct time_object *tobj;//TODO:serialize timezone
    struct pg_tm *tm;
    unsigned long p, s;
    char buf[8];
    pg_time_t t;
    int nsec;
    int i;
    VALUE str;
    struct pg_tm result;

    GetTimeval(time, tobj);

    t = tobj->ts.tv_sec;
    tm = GMTIME(&t, result);

    if ((tm->tm_year & 0xffff) != tm->tm_year)
        rb_raise(rb_eArgError, "year too big to marshal: %ld", (long)tm->tm_year);

    p = 0x1UL        << 31 | /*  1 */
	IS_GMT(tobj) << 30 | /*  1 */
	tm->tm_year  << 14 | /* 16 */
	tm->tm_mon   << 10 | /*  4 */
	tm->tm_mday  <<  5 | /*  5 */
	tm->tm_hour;         /*  5 */
    s = tm->tm_min   << 26 | /*  6 */
	tm->tm_sec   << 20 | /*  6 */
	tobj->ts.tv_nsec / 1000;    /* 20 */
    nsec = tobj->ts.tv_nsec % 1000;

    for (i=0; i<4; i++) {
	buf[i] = (unsigned char)p;
	p = RSHIFT(p, 8);
    }
    for (i=4; i<8; i++) {
	buf[i] = (unsigned char)s;
	s = RSHIFT(s, 8);
    }

    str = rb_str_new(buf, 8);
    rb_copy_generic_ivar(str, time);
    if (nsec) {
        /*
         * submicro is formatted in fixed-point packed BCD (without sign).
         * It represent digits under microsecond.
         * For nanosecond resolution, 3 digits (2 bytes) are used.
         * However it can be longer.
         * Extra digits are ignored for loading.
         */
        unsigned char buf[2];
        int len = sizeof(buf);
        buf[1] = (nsec % 10) << 4;
        nsec /= 10;
        buf[0] = nsec % 10;
        nsec /= 10;
        buf[0] |= (nsec % 10) << 4;
        if (buf[1] == 0)
            len = 1;
        rb_ivar_set(str, id_submicro, rb_str_new((char *)buf, len));
    }
    return str;
}

/*
 * call-seq:
 *   time._dump   => string
 *
 * Dump _time_ for marshaling.
 */

static VALUE
time_dump(int argc, VALUE *argv, VALUE time)
{
    VALUE str;

    rb_scan_args(argc, argv, "01", 0);
    str = time_mdump(time);

    return str;
}

/*
 * undocumented
 */

static VALUE
time_mload(VALUE time, VALUE str)
{
    struct time_object *tobj;
    unsigned long p, s;
    time_t sec;
    long usec;
    unsigned char *buf;
    struct pg_tm tm;
    int i, gmt;
    long nsec;
    VALUE submicro;

    time_modify(time);

    submicro = rb_attr_get(str, id_submicro);
    if (submicro != Qnil) {
        st_delete(rb_generic_ivar_table(str), (st_data_t*)&id_submicro, 0);
    }
    rb_copy_generic_ivar(time, str);

    StringValue(str);
    buf = (unsigned char *)RSTRING_PTR(str);
    if (RSTRING_LEN(str) != 8) {
	rb_raise(rb_eTypeError, "marshaled time format differ");
    }

    p = s = 0;
    for (i=0; i<4; i++) {
	p |= buf[i]<<(8*i);
    }
    for (i=4; i<8; i++) {
	s |= buf[i]<<(8*(i-4));
    }

    if ((p & (1UL<<31)) == 0) {
        gmt = 0;
	sec = p;
	usec = s;
        nsec = usec * 1000;
    }
    else {
	p &= ~(1UL<<31);
	gmt        = (p >> 30) & 0x1;
	tm.tm_year = (p >> 14) & 0xffff;
	tm.tm_mon  = (p >> 10) & 0xf;
	tm.tm_mday = (p >>  5) & 0x1f;
	tm.tm_hour =  p        & 0x1f;
	tm.tm_min  = (s >> 26) & 0x3f;
	tm.tm_sec  = (s >> 20) & 0x3f;
	tm.tm_yday = tm.tm_wday = 0;
	tm.tm_isdst = 0;

	// BUG? utc_p always true
	sec = make_time_t(&tm, timezone_utc(), Qtrue);//TODO:should be stored?
	usec = (long)(s & 0xfffff);
        nsec = usec * 1000;

        if (submicro != Qnil) {
            unsigned char *ptr;
            long len;
            int digit;
            ptr = (unsigned char*)StringValuePtr(submicro);
            len = RSTRING_LEN(submicro);
            if (0 < len) {
                if (10 <= (digit = ptr[0] >> 4)) goto end_submicro;
                nsec += digit * 100;
                if (10 <= (digit = ptr[0] & 0xf)) goto end_submicro;
                nsec += digit * 10;
            }
            if (1 < len) {
                if (10 <= (digit = ptr[1] >> 4)) goto end_submicro;
                nsec += digit;
            }
end_submicro: ;
        }
    }
    time_overflow_p(&sec, &nsec);

    GetTimeval(time, tobj);
    tobj->tm_got = 0;
	if (gmt) { //TODO: normal deserialisation
		tobj->tz = timezone_utc();
		GMT_TRUE(tobj);
	}
	else {
		tobj->tz = timezone_default(NULL);
		GMT_FALSE(tobj);
	}
    tobj->ts.tv_sec = sec;
    tobj->ts.tv_nsec = nsec;

    return time;
}

/*
 * call-seq:
 *   Time._load(string)   => time
 *
 * Unmarshal a dumped +Time+ object.
 */

static VALUE
time_load(VALUE klass, VALUE str)
{
    VALUE time = time_s_alloc(klass);

    time_mload(time, str);//TODO:set tz
    return time;
}



static st_table *tz_cache = NULL;

static struct pg_tz*
timezone_cached_get(const char *name)
{
    st_data_t key = (st_data_t)name, value = 0;

    if(!tz_cache)
        tz_cache = st_init_strcasetable();

    st_lookup(tz_cache, key, &value);

    if (!value) {
        struct pg_tz* tz = pg_tzset(name);
        if (!tz)
            rb_raise(rb_eRuntimeError, "time zone \"%s\" not found", name);
        st_insert(tz_cache, (st_data_t)name, (st_data_t)tz);
        value = (st_data_t)tz;
    }

    return (struct pg_tz*)value;
}

static struct pg_tz*
timezone_utc()
{
    static struct pg_tz *utc_tz = NULL;

    if (!utc_tz)
        utc_tz = timezone_cached_get("UTC");

    return utc_tz;
}

static struct pg_tz*
timezone_default(struct pg_tz *dflt)
{
    //TODO:add to cache
    static struct pg_tz* tz = NULL;

    if (!tz && !dflt) {
        tz = select_default_timezone();
    }
    else if (dflt) {
        tz = dflt;
    }
    return tz;
}

static VALUE
timezone_get(VALUE klass, VALUE name)
{
   struct pg_tz* tz = timezone_cached_get(StringValueCStr(name));

   return Data_Wrap_Struct(klass, 0, 0, tz);
}

/*
 *  call-seq:
 *     Time::Zone.default => timezone 
 *     Time::Zone.local   => timezone 
 *
 *  Returns timezone which used as local timezone
 *
 *     Time::Zone.default   # => #<Time::Zone: Europe/Athens>
 */
static VALUE
timezone_default_get(VALUE klass)
{
    struct pg_tz * default_timezone = timezone_default(NULL);

    return Data_Wrap_Struct(klass, 0, 0, default_timezone);
}

/*
 *  call-seq:
 *     Time::Zone.default = timezone or it name
 *     Time::Zone.local = timezone or it name
 *
 *  Sets timezone which will be used as local timezone
 *
 *     Time::Zone.default = "Japan"            # => "Japan"
 *     Time.now                                # => 2009-03-18 21:07:51 JST
 *     Time::Zone.default = "US/Pacific"       # => "US/Pacific"
 *     Time.now                                # => 2009-03-18 05:08:06 PDT
 *     Time::Zone.default = Time::Zone["UTC"]  # => "UTC"
 *     Time.now                                # => 2009-03-18 12:08:17 UTC
 */
static VALUE
timezone_default_set(VALUE klass, VALUE timezone)
{
    struct pg_tz* tz;

    GetOrInitTZ(timezone, tz);
    timezone_default(tz);

    return timezone;
}

/*
 *  call-seq:
 *     timezone.name => string
 *     timezone.to_s => string
 *
 *  Return a string with name of the timezone
 *
 *     tz.name     # => "Brazil/West"
 */
static VALUE
timezone_name(VALUE timezone)
{
    struct pg_tz* tz;
    const char *tz_name;

    GetTZval(timezone, tz);
    tz_name = pg_get_timezone_name(tz);

    return tz_name ? rb_str_new_cstr(tz_name) : Qnil;
}

/*
 *  call-seq:
 *     timezone.inspect => string
 *
 *  Return a string describing this timezone object.
 *
 *     tz.inspect     # => #<Time::Zone: Europe/Berlin>
 */
static VALUE
timezone_inspect(VALUE timezone)
{
    VALUE str = rb_str_buf_new2("#<");
    struct pg_tz* tz;
    const char *tz_name;

    GetTZval(timezone, tz);
    tz_name = pg_get_timezone_name(tz);

    rb_str_buf_cat2(str, rb_obj_classname(timezone));
    rb_str_buf_cat2(str, ": ");
    rb_str_buf_cat2(str, tz_name ? tz_name : "nil");
    rb_str_buf_cat2(str, ">");

    return str;
}

/*
 *  <code>Time</code> is an abstraction of dates and times. Time is
 *  stored internally as the number of seconds and nanoseconds since
 *  the <em>Epoch</em>, January 1, 1970 00:00 UTC. On some operating
 *  systems, this offset is allowed to be negative. Also see the
 *  library modules <code>Date</code>. The
 *  <code>Time</code> class treats GMT (Greenwich Mean Time) and UTC
 *  (Coordinated Universal Time)<em>[Yes, UTC really does stand for
 *  Coordinated Universal Time. There was a committee involved.]</em>
 *  as equivalent.  GMT is the older way of referring to these
 *  baseline times but persists in the names of calls on POSIX
 *  systems.
 *
 *  All times are stored with some number of nanoseconds. Be aware of
 *  this fact when comparing times with each other---times that are
 *  apparently equal when displayed may be different when compared.
 */
const char * rb_tzdir = NULL;

void
Init_time2(void)
{
#undef rb_intern
#define rb_intern(str) rb_intern_const(str)
    VALUE tz_dir = rb_gv_get("$__tz_directory");
#ifdef OLD_TIME_COMPAT
	//Ponter to function time_free used to detect is object is Time
	//We get this pointer and use it to behave like old Time
    VALUE old_time = rb_funcall(rb_const_get(rb_cObject, rb_intern("Time")), rb_intern("now"), 0);
    time_free = RDATA(old_time)->dfree;
#endif

    rb_tzdir = StringValueCStr(tz_dir);

    id_divmod = rb_intern("divmod");
    id_mul = rb_intern("*");
    id_submicro = rb_intern("submicro");

    rb_cTime = rb_define_class("Time", rb_cObject);
    rb_include_module(rb_cTime, rb_mComparable);
	rb_require("time"); // defines strptime which we wil redefine later
	rb_define_alias(rb_singleton_class(rb_cTime), "old_strptime", "strptime");

    rb_define_alloc_func(rb_cTime, time_s_alloc);
    rb_define_singleton_method(rb_cTime, "now", rb_class_new_instance, -1);
    rb_define_singleton_method(rb_cTime, "at", time_s_at, -1);
    rb_define_singleton_method(rb_cTime, "utc", time_s_mkutc, -1);
    rb_define_singleton_method(rb_cTime, "gm", time_s_mkutc, -1);
    rb_define_singleton_method(rb_cTime, "local", time_s_mktime, -1);
    rb_define_singleton_method(rb_cTime, "mktime", time_s_mktime, -1);
    rb_define_singleton_method(rb_cTime, "strptime", time_strptime, 2);

    rb_define_method(rb_cTime, "to_i", time_to_i, 0);
    rb_define_method(rb_cTime, "to_f", time_to_f, 0);
    rb_define_method(rb_cTime, "<=>", time_cmp, 1);
    rb_define_method(rb_cTime, "eql?", time_eql, 1);
    rb_define_method(rb_cTime, "hash", time_hash, 0);
    rb_define_method(rb_cTime, "initialize", time_init, 0);
    rb_define_method(rb_cTime, "initialize_copy", time_init_copy, 1);

    rb_define_method(rb_cTime, "localtime", time_localtime, -1);
    rb_define_method(rb_cTime, "gmtime", time_gmtime, 0);
    rb_define_method(rb_cTime, "utc", time_gmtime, 0);
    rb_define_method(rb_cTime, "getlocal", time_getlocaltime, -1);
    rb_define_method(rb_cTime, "getgm", time_getgmtime, 0);
    rb_define_method(rb_cTime, "getutc", time_getgmtime, 0);

    rb_define_method(rb_cTime, "ctime", time_asctime, 0);
    rb_define_method(rb_cTime, "asctime", time_asctime, 0);
    rb_define_method(rb_cTime, "to_s", time_to_s, 0);
    rb_define_method(rb_cTime, "inspect", time_to_s, 0);
    rb_define_method(rb_cTime, "to_a", time_to_a, 0);

    rb_define_method(rb_cTime, "+", time_plus, 1);
    rb_define_method(rb_cTime, "-", time_minus, 1);

    rb_define_method(rb_cTime, "succ", time_succ, 0);
    rb_define_method(rb_cTime, "sec", time_sec, 0);
    rb_define_method(rb_cTime, "min", time_min, 0);
    rb_define_method(rb_cTime, "hour", time_hour, 0);
    rb_define_method(rb_cTime, "mday", time_mday, 0);
    rb_define_method(rb_cTime, "day", time_mday, 0);
    rb_define_method(rb_cTime, "mon", time_mon, 0);
    rb_define_method(rb_cTime, "month", time_mon, 0);
    rb_define_method(rb_cTime, "year", time_year, 0);
    rb_define_method(rb_cTime, "wday", time_wday, 0);
    rb_define_method(rb_cTime, "yday", time_yday, 0);
    rb_define_method(rb_cTime, "isdst", time_isdst, 0);
    rb_define_method(rb_cTime, "dst?", time_isdst, 0);
    rb_define_method(rb_cTime, "zone", time_zone, 0);
    rb_define_method(rb_cTime, "gmtoff", time_utc_offset, 0);
    rb_define_method(rb_cTime, "gmt_offset", time_utc_offset, 0);
    rb_define_method(rb_cTime, "utc_offset", time_utc_offset, 0);

    rb_define_method(rb_cTime, "utc?", time_utc_p, 0);
    rb_define_method(rb_cTime, "gmt?", time_utc_p, 0);

    rb_define_method(rb_cTime, "sunday?", time_sunday, 0);
    rb_define_method(rb_cTime, "monday?", time_monday, 0);
    rb_define_method(rb_cTime, "tuesday?", time_tuesday, 0);
    rb_define_method(rb_cTime, "wednesday?", time_wednesday, 0);
    rb_define_method(rb_cTime, "thursday?", time_thursday, 0);
    rb_define_method(rb_cTime, "friday?", time_friday, 0);
    rb_define_method(rb_cTime, "saturday?", time_saturday, 0);

    rb_define_method(rb_cTime, "tv_sec", time_to_i, 0);
    rb_define_method(rb_cTime, "tv_usec", time_usec, 0);
    rb_define_method(rb_cTime, "usec", time_usec, 0);
    rb_define_method(rb_cTime, "tv_nsec", time_nsec, 0);
    rb_define_method(rb_cTime, "nsec", time_nsec, 0);

    rb_define_method(rb_cTime, "strftime", time_strftime, 1);

    /* methods for marshaling */
    rb_define_method(rb_cTime, "_dump", time_dump, -1);
    rb_define_singleton_method(rb_cTime, "_load", time_load, 1);

    rb_cTimeZone = rb_define_class_under(rb_cTime, "Zone", rb_cObject);

    //rb_define_const(rb_cTimeZone, "UTC", timezone_get(rb_cTimeZone, rb_str_new));

    rb_define_singleton_method(rb_cTimeZone, "[]", timezone_get, 1);
    rb_define_singleton_method(rb_cTimeZone, "default", timezone_default_get, 0);
    rb_define_singleton_method(rb_cTimeZone, "default=", timezone_default_set, 1);
    rb_define_singleton_method(rb_cTimeZone, "local", timezone_default_get, 0);
    rb_define_singleton_method(rb_cTimeZone, "local=", timezone_default_set, 1);

    rb_define_method(rb_cTimeZone, "name", timezone_name, 0);
    rb_define_method(rb_cTimeZone, "to_s", timezone_name, 0);
    rb_define_method(rb_cTimeZone, "inspect", timezone_inspect, 0);

#if 0
    /* Time will support marshal_dump and marshal_load in the future (1.9 maybe) */
    rb_define_method(rb_cTime, "marshal_dump", time_mdump, 0);
    rb_define_method(rb_cTime, "marshal_load", time_mload, 1);
#endif
}
