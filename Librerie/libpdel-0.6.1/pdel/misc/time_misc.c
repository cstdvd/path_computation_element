/*
 * time_misc.h
 *
 * PD time releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include <wtypes.h>
#include <windows.h>
#include <winbase.h>
#else

#define HAVE_TIMEGM 1
#define HAVE_GETTIMEOFDAY 1

#endif

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif

#include "pdel/pd_time.h"

time_t
pd_timegm(struct tm *t)
{
#ifdef HAVE_TIMEGM
	return(timegm(t));
#else
	time_t tl, tb;  
	struct tm *tg;  

	tl = mktime(t);  
	if(tl == -1) {
		t->tm_hour--;
		tl = mktime (t);
		if (tl == -1) {
			return -1;
		}
		tl += 3600;	
	}  
	tg = gmtime(&tl);  
	tg->tm_isdst = 0;  
	tb = mktime(tg);  
	if (tb == -1) {
		tg->tm_hour--;
		tb = mktime (tg);
		if (tb == -1) {
			return -1;
		}
		tb += 3600;	
	}  
	return (tl - (tb - tl)); 
#endif
}


#ifdef WIN32
/*
 * Number of micro-seconds between the beginning of the Windows epoch
 * (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
 *
 * This assumes all Win32 compilers have 64-bit support.
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
  #define DELTA_EPOCH_IN_USEC  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_USEC  11644473600000000ULL
#endif

static u_int64_t filetime_to_unix_epoch (const FILETIME *ft)
{
	u_int64_t res = (u_int64_t) ft->dwHighDateTime << 32;

	res |= ft->dwLowDateTime;
	res /= 10;				   /* from 100 nano-sec periods to usec */
	res -= DELTA_EPOCH_IN_USEC;  /* from Win epoch to Unix epoch */
	return (res);
}
#endif

int
pd_gettimeofday (struct timeval *tv, void *tz_unused)
{
#ifdef HAVE_GETTIMEOFDAY
	return(gettimeofday(tv, tz_unused));
#else
	/* For now this assumes WIN32 */
	FILETIME  ft;
	u_int64_t tim;

	if (!tv) {
		errno = EINVAL;
		return (-1);
	}
	GetSystemTimeAsFileTime(&ft);
	tim = filetime_to_unix_epoch(&ft);
	tv->tv_sec  = (long) (tim / 1000000L);
	tv->tv_usec = (long) (tim % 1000000L);
	return (0);
#endif
}

/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

static const char *abb_weekdays[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    NULL
};

static const char *full_weekdays[] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    NULL
};

static const char *abb_month[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
    NULL
};

static const char *full_month[] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
    NULL,
};

static const char *ampm[] = {
    "am",
    "pm",
    NULL
};

/*
 * tm_year is relative this year 
 */
static const int tm_year_base = 1900;

/*
 * Return TRUE iff `year' was a leap year.
 * Needed for strptime.
 */
static int
is_leap_year (int year)
{
    return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
}

/* Needed for strptime. */
static int
match_string (const char **buf, const char **strs)
{
    int i = 0;

    for (i = 0; strs[i] != NULL; ++i) {
	int len = strlen (strs[i]);

	if (strncasecmp (*buf, strs[i], len) == 0) {
	    *buf += len;
	    return i;
	}
    }
    return -1;
}

/* Needed for strptime. */
static int
first_day (int year)
{
    int ret = 4;

    for (; year > 1970; --year)
	ret = (ret + 365 + is_leap_year (year) ? 1 : 0) % 7;
    return ret;
}

/*
 * Set `timeptr' given `wnum' (week number [0, 53])
 * Needed for strptime
 */

static void
set_week_number_sun (struct tm *timeptr, int wnum)
{
    int fday = first_day (timeptr->tm_year + tm_year_base);

    timeptr->tm_yday = wnum * 7 + timeptr->tm_wday - fday;
    if (timeptr->tm_yday < 0) {
	timeptr->tm_wday = fday;
	timeptr->tm_yday = 0;
    }
}

/*
 * Set `timeptr' given `wnum' (week number [0, 53])
 * Needed for strptime
 */

static void
set_week_number_mon (struct tm *timeptr, int wnum)
{
    int fday = (first_day (timeptr->tm_year + tm_year_base) + 6) % 7;

    timeptr->tm_yday = wnum * 7 + (timeptr->tm_wday + 6) % 7 - fday;
    if (timeptr->tm_yday < 0) {
	timeptr->tm_wday = (fday + 1) % 7;
	timeptr->tm_yday = 0;
    }
}

/*
 * Set `timeptr' given `wnum' (week number [0, 53])
 * Needed for strptime
 */
static void
set_week_number_mon4 (struct tm *timeptr, int wnum)
{
    int fday = (first_day (timeptr->tm_year + tm_year_base) + 6) % 7;
    int offset = 0;

    if (fday < 4)
	offset += 7;

    timeptr->tm_yday = offset + (wnum - 1) * 7 + timeptr->tm_wday - fday;
    if (timeptr->tm_yday < 0) {
	timeptr->tm_wday = fday;
	timeptr->tm_yday = 0;
    }
}

/* strptime: roken */
//extern "C"
//strptime (const char *buf, const char *format, struct tm *timeptr)
char *
pd_strptime(const char *buf,
	    const char *format,
	    struct tm *timeptr)
{
    char c;

    for (; (c = *format) != '\0'; ++format) {
	char *s;
	int ret;

	if (isspace (c)) {
	    while (isspace (*buf))
		++buf;
	} else if (c == '%' && format[1] != '\0') {
	    c = *++format;
	    if (c == 'E' || c == 'O')
		c = *++format;
	    switch (c) {
	    case 'A' :
		ret = match_string (&buf, full_weekdays);
		if (ret < 0)
		    return NULL;
		timeptr->tm_wday = ret;
		break;
	    case 'a' :
		ret = match_string (&buf, abb_weekdays);
		if (ret < 0)
		    return NULL;
		timeptr->tm_wday = ret;
		break;
	    case 'B' :
		ret = match_string (&buf, full_month);
		if (ret < 0)
		    return NULL;
		timeptr->tm_mon = ret;
		break;
	    case 'b' :
	    case 'h' :
		ret = match_string (&buf, abb_month);
		if (ret < 0)
		    return NULL;
		timeptr->tm_mon = ret;
		break;
	    case 'C' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_year = (ret * 100) - tm_year_base;
		buf = s;
		break;
	    case 'c' :		/* %a %b %e %H:%M:%S %Y */
		s = strptime (buf, "%a %b %e %H:%M:%S %Y", timeptr);
		if (s == NULL)
		    return NULL;
		buf = s;
		break;
	    case 'D' :		/* %m/%d/%y */
		s = strptime (buf, "%m/%d/%y", timeptr);
		if (s == NULL)
		    return NULL;
		buf = s;
		break;
	    case 'd' :
	    case 'e' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_mday = ret;
		buf = s;
		break;
	    case 'H' :
	    case 'k' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_hour = ret;
		buf = s;
		break;
	    case 'I' :
	    case 'l' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		if (ret == 12)
		    timeptr->tm_hour = 0;
		else
		    timeptr->tm_hour = ret;
		buf = s;
		break;
	    case 'j' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_yday = ret - 1;
		buf = s;
		break;
	    case 'm' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_mon = ret - 1;
		buf = s;
		break;
	    case 'M' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_min = ret;
		buf = s;
		break;
	    case 'n' :
		if (*buf == '\n')
		    ++buf;
		else
		    return NULL;
		break;
	    case 'p' :
		ret = match_string (&buf, ampm);
		if (ret < 0)
		    return NULL;
		if (timeptr->tm_hour == 0) {
		    if (ret == 1)
			timeptr->tm_hour = 12;
		} else
		    timeptr->tm_hour += 12;
		break;
	    case 'r' :		/* %I:%M:%S %p */
		s = strptime (buf, "%I:%M:%S %p", timeptr);
		if (s == NULL)
		    return NULL;
		buf = s;
		break;
	    case 'R' :		/* %H:%M */
		s = strptime (buf, "%H:%M", timeptr);
		if (s == NULL)
		    return NULL;
		buf = s;
		break;
	    case 'S' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_sec = ret;
		buf = s;
		break;
	    case 't' :
		if (*buf == '\t')
		    ++buf;
		else
		    return NULL;
		break;
	    case 'T' :		/* %H:%M:%S */
	    case 'X' :
		s = strptime (buf, "%H:%M:%S", timeptr);
		if (s == NULL)
		    return NULL;
		buf = s;
		break;
	    case 'u' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_wday = ret - 1;
		buf = s;
		break;
	    case 'w' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_wday = ret;
		buf = s;
		break;
	    case 'U' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		set_week_number_sun (timeptr, ret);
		buf = s;
		break;
	    case 'V' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		set_week_number_mon4 (timeptr, ret);
		buf = s;
		break;
	    case 'W' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		set_week_number_mon (timeptr, ret);
		buf = s;
		break;
	    case 'x' :
		s = strptime (buf, "%Y:%m:%d", timeptr);
		if (s == NULL)
		    return NULL;
		buf = s;
		break;
	    case 'y' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		if (ret < 70)
		    timeptr->tm_year = 100 + ret;
		else
		    timeptr->tm_year = ret;
		buf = s;
		break;
	    case 'Y' :
		ret = strtol (buf, &s, 10);
		if (s == buf)
		    return NULL;
		timeptr->tm_year = ret - tm_year_base;
		buf = s;
		break;
	    case 'Z' :
		/* Unsupported. Just ignore.  */
		break;
	    case '\0' :
		--format;
		/* FALLTHROUGH */
	    case '%' :
		if (*buf == '%')
		    ++buf;
		else
		    return NULL;
		break;
	    default :
		if (*buf == '%' || *++buf == c)
		    ++buf;
		else
		    return NULL;
		break;
	    }
	} else {
	    if (*buf == c)
		++buf;
	    else
		return NULL;
	}
    }
    return (char *)buf;
}
