/*
 * Powerdog Industries kindly requests feedback from anyone modifying
 * this function:
 *
 * Date: Thu, 05 Jun 1997 23:17:17 -0400  
 * From: Kevin Ruddy <kevin.ruddy@powerdog.com>
 * To: James FitzGibbon <james@nexis.net>
 * Subject: Re: Use of your strptime(3) code (fwd)
 * 
 * The reason for the "no mod" clause was so that modifications would
 * come back and we could integrate them and reissue so that a wider 
 * audience could use it (thereby spreading the wealth).  This has   
 * made it possible to get strptime to work on many operating systems.
 * I'm not sure why that's "plain unacceptable" to the FreeBSD team.
 * 
 * Anyway, you can change it to "with or without modification" as
 * you see fit.  Enjoy.                                          
 * 
 * Kevin Ruddy
 * Powerdog Industries, Inc.
 */
/*
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Powerdog Industries.
 * 4. The name of Powerdog Industries may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* SOURCE: 
     http://www.freebsd.org.ua/cgi/cvsweb.cgi/src/lib/libc/stdtime/?cvsroot=freebsd
	 Revision 1.35, Mon Nov 17 04:19:15 2003 UTC by nectar 
 */
#include "c.h"
#include "private.h"
#include "pgtz.h"
#include "tzfile.h"

//#define TZ_STRLEN_MAX 255
static const struct pg_time_locale {
  const char *wday[7];
  const char *weekday[7];
  const char *mon[12];
  const char *month[12];
  const char *date_fmt;
  const char *X_fmt; /* The time, using the locale's format. */
  const char *x_fmt; /* The date, using the locale's date format. */
  const char *c_fmt; /* Date and time, using the locale's format. */
  const char *ampm_fmt; /* 12-hour clock time using the AM/PM notation; 
						   in the default locale, this shall be equivalent to %I:%M:%S %p */
  /* TODO: add PM AM localisation */
} pgDefaultTimeLocale = {
  {
	"Sun","Mon","Tue","Wed","Thu","Fri","Sat",
  },
  {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
	"Friday", "Saturday"
  },
  {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  },
  {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
  },
  "%a %b %e %H:%M:%S %Z %Y",
  "%H:%M:%S",
  "%m/%d/%y",
  "%a %b %e %H:%M:%S %Y",
  "%I:%M:%S %p"
};


#define asizeof(a)	(sizeof (a) / sizeof ((a)[0]))
#define strncasecmp(s1, s2, n) (st_strncasecmp(s1, s2, n))

static const char *
_pg_strptime(const char *buf, const char *fmt, struct pg_tm *tm, long *nsec, struct pg_tz const *tz)
{
	char	c;
	const char *ptr;
	int	i, len;
	int Ealternative, Oalternative;
	struct pg_time_locale const *loc = &pgDefaultTimeLocale;
	int has_am_pm = 0; // 1 - AM, 2 - PM

	ptr = fmt;
	while (*ptr != 0) {
		if (*buf == 0)
			break;

		c = *ptr++;

		if (c != '%') {
			if (isspace((unsigned char)c))
				while (*buf != 0 && isspace((unsigned char)*buf))
					buf++;
			else if (c != *buf++)
				return 0;
			continue;
		}

		Ealternative = 0;
		Oalternative = 0;
label:
		c = *ptr++;
		switch (c) {
		case 0:
		case '%':
			if (*buf++ != '%')
				return 0;
			break;

		case '+':
		  buf = _pg_strptime(buf, loc->date_fmt, tm, nsec,tz);
			if (buf == 0)
				return 0;
			break;

		case 'C':
			if (!isdigit((unsigned char)*buf))
				return 0;

			/* XXX This will break for 3-digit centuries. */
			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 19)
				return 0;

			tm->tm_year = i * 100 - 1900;
			break;

		case 'c': /* Date and time, using the locale's format. */
 		    buf = _pg_strptime(buf, loc->c_fmt, tm, nsec, tz);			
			if (buf == 0)
				return 0;
			break;

		case 'D':
			buf = _pg_strptime(buf, "%m/%d/%y", tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'E':
			if (Ealternative || Oalternative)
				break;
			Ealternative++;
			goto label;

		case 'O':
			if (Ealternative || Oalternative)
				break;
			Oalternative++;
			goto label;

		case 'F':
			buf = _pg_strptime(buf, "%Y-%m-%d", tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'R':
			buf = _pg_strptime(buf, "%H:%M", tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'r': /* 12-hour clock time using the AM/PM notation; 
					 in the default locale, this shall be equivalent to %I:%M:%S %p */
  		    buf = _pg_strptime(buf, loc->ampm_fmt, tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'T':
			buf = _pg_strptime(buf, "%H:%M:%S", tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'X': /* The time, using the locale's format */
			buf = _pg_strptime(buf, loc->X_fmt, tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'x': /* The date, using the locale's date format */
			buf = _pg_strptime(buf, loc->x_fmt, tm, nsec, tz);
			if (buf == 0)
				return 0;
			break;

		case 'j':
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 3;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 366)
				return 0;

			tm->tm_yday = i - 1;
			break;

		case 'M':
		case 'S':
			if (*buf == 0 || isspace((unsigned char)*buf))
				break;

			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}

			if (c == 'M') {
				if (i > 59)
					return 0;
				tm->tm_min = i;
			} else {
				if (i > 60)
					return 0;
				tm->tm_sec = i;
			}

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'H':
		case 'I':
		case 'k':
		case 'l':
			/*
			 * Of these, %l is the only specifier explicitly
			 * documented as not being zero-padded.  However,
			 * there is no harm in allowing zero-padding.
			 *
			 * XXX The %l specifier may gobble one too many
			 * digits if used incorrectly.
			 */
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'H' || c == 'k') {
				if (i > 23)
					return 0;
			} else if (i > 12)
				return 0;

			tm->tm_hour = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'p':
			if (strncasecmp(buf, "am", (len=2)) == 0 || strncasecmp(buf, "a.m.", (len=4)) == 0) {
			  has_am_pm = 1;
			  if (tm->tm_hour > 12)
				return 0;
			  buf += len;
			  break;
			}

			if (strncasecmp(buf, "pm", (len=2)) == 0 || strncasecmp(buf, "p.m.", (len=4)) == 0) {
			  has_am_pm = 2;
			  if (tm->tm_hour > 12)
				return 0;
			  buf += 4;
			  break;
			}
			return 0;

		case 'A':
		case 'a':
			for (i = 0; i < asizeof(loc->weekday); i++) {
				len = strlen(loc->weekday[i]);
				if (strncasecmp(buf, loc->weekday[i],
						len) == 0)
					break;
				len = strlen(loc->wday[i]);
				if (strncasecmp(buf, loc->wday[i],
						len) == 0)
					break;
			}
			if (i == asizeof(loc->weekday))
				return 0;

			tm->tm_wday = i;
			buf += len;
			break;

		case 'U':
		case 'W':
			/*
			 * XXX This is bogus, as we can not assume any valid
			 * information present in the tm structure at this
			 * point to calculate a real value, so just check the
			 * range for now.
			 */
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 53)
				return 0;
			else {
			  //if (c == 'U')
			  //else

			}

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'w':
			if (!isdigit((unsigned char)*buf))
				return 0;

			i = *buf - '0';
			if (i > 6)
				return 0;

			tm->tm_wday = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'd':
		case 'e':
			/*
			 * The %e specifier is explicitly documented as not
			 * being zero-padded but there is no harm in allowing
			 * such padding.
			 *
			 * XXX The %e specifier may gobble one too many
			 * digits if used incorrectly.
			 */
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 31)
				return 0;

			tm->tm_mday = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'B':
		case 'b':
		case 'h':
			for (i = 0; i < asizeof(loc->month); i++) {
				if (Oalternative) {
					if (c == 'B') {
						len = strlen(loc->month[i]);
						if (strncasecmp(buf,
								loc->month[i],
								len) == 0)
							break;
					}
				} else {
					len = strlen(loc->month[i]);
					if (strncasecmp(buf, loc->month[i],
							len) == 0)
						break;
					len = strlen(loc->mon[i]);
					if (strncasecmp(buf, loc->mon[i],
							len) == 0)
						break;
				}
			}
			if (i == asizeof(loc->month))
				return 0;

			tm->tm_mon = i;
			buf += len;
			break;

		case 'm':
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 12)
				return 0;

			tm->tm_mon = i - 1;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'Q': // number of miliseconds since the epoch (1970-01-01 00:00:00 UTC)
		case 's': // number of seconds since the epoch (1970-01-01 00:00:00 UTC)
			{
			char *cp;
			int sverrno;
			long n, r, ms = 0;
			pg_time_t t;

			sverrno = errno;
			errno = 0;
			n = strtol(buf, &cp, 10);

			if (c == 's') {
				t = n;
				r = (long)t;
			}
			else {
				t = n / 1000;
				ms = n % 1000;
				r = ((long)t) * 1000 + ms;
			}

			if (errno == ERANGE || r != n) {
				errno = sverrno;
				return 0;
			}
			errno = sverrno;
			buf = cp;
			*nsec = ms * 1000000;
			pg_localtime_r(&t, tz, tm);
			}
			break;
		case 'N':
		    {
			char *cp;
			int sverrno;

			sverrno = errno;
			errno = 0;
			*nsec = strtol(buf, &cp, 10);
			if (errno != 0) {
				errno = sverrno;
				return 0;
			}
			buf = cp;
		    }
			break;
		case 'Y':
		case 'y':
			if (*buf == 0 || isspace((unsigned char)*buf))
				break;

			if (!isdigit((unsigned char)*buf))
				return 0;

			len = (c == 'Y') ? 4 : 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'Y')
				i -= 1900;
			if (c == 'y' && i < 69)
				i += 100;
			if (i < 0)
				return 0;

			tm->tm_year = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'Z':
			{
			const char *cp;
			char zonestr[TZ_STRLEN_MAX+1];

			for (cp = buf; *cp && isupper((unsigned char)*cp); ++cp) {/*empty*/}
			if (cp - buf) {
				strncpy(zonestr, buf, TZ_STRLEN_MAX);
				zonestr[TZ_STRLEN_MAX] = '\0';
				// TODO: normal implementation
				if (0 == strcmp(zonestr, "GMT")) {
				  tm->tm_zone = "GMT";
				}
				else {
				  rb_notimplement();
				}				
				//} else if (0 == strcmp(zonestr, tzname[0])) {
				    //tm->tm_isdst = 0;
				//} else if (0 == strcmp(zonestr, tzname[1])) {
				  //  tm->tm_isdst = 1;
				buf += cp - buf;
			}
			}
			break;
		}
	}
	
	switch(has_am_pm) {
	case 1: /* AM */
		if (tm->tm_hour == 12)
			tm->tm_hour = 0;
		break;
	case 2: /* PM */
		if (tm->tm_hour != 12)
			tm->tm_hour += 12;
		break;
	}

	return (char *)buf;
}


const char *
pg_strptime(const char * buf, 
			const char * fmt,  
			struct pg_tm * tm, 
			long *nsec,
			struct pg_tz const *tz)
{
	return  _pg_strptime(buf, fmt, tm, nsec, tz);
}
