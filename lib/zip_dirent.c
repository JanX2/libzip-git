/*
  $NiH: zip_dirent.c,v 1.1.2.1 2004/03/22 14:17:33 dillo Exp $

  zip_dirent.c -- read directory entry (local or central), clean dirent
  Copyright (C) 1999, 2003, 2004 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <nih@giga.or.at>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.
 
  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "zip.h"
#include "zipint.h"

static time_t _zip_d2u_time(int, int);
static unsigned short _zip_read2(unsigned char **);
static unsigned int _zip_read4(unsigned char **);
static char *_zip_readfpstr(FILE *, int, int, struct zip_error *);
static char *_zip_readstr(unsigned char **, int, int, struct zip_error *);



void
_zip_dirent_finalize(struct zip_dirent *zde)
{
    free(zde->filename);
    zde->filename = NULL;
    free(zde->extrafield);
    zde->extrafield = NULL;
    free(zde->comment);
    zde->comment = NULL;
}



/* _zip_dirent_read(zde, fp, bufp, left, localp, error):
   Fills the zip directory entry zde.

   If bufp is non-NULL, data is taken from there and bufp is advanced
   by the amount of data used; no more than left bytes are used.
   Otherwise data is read from fp as needed.

   If localp != 0, it reads a local header instead of a central
   directory entry.

   Returns 0 if successful. On error, error is filled in and -1 is
   returned.
*/

int
_zip_dirent_read(struct zip_dirent *zde, FILE *fp,
		 unsigned char **bufp, int left, int localp,
		 struct zip_error *error)
{
    unsigned char buf[CDENTRYSIZE];
    unsigned char *cur;
    unsigned short dostime, dosdate;
    int size;

    if (localp)
	size = LENTRYSIZE;
    else
	size = CDENTRYSIZE;
    
    if (bufp) {
	/* use data from buffer */
	cur = *bufp;
	if (left < size) {
	    _zip_error_set(error, ZERR_NOZIP, 0);
	    return -1;
	}
    }
    else {
	/* read entry from disk */
	if ((fread(buf, 1, size, fp)<size)) {
	    _zip_error_set(error, ZERR_READ, errno);
	    return -1;
	}
	left = size;
	cur = buf;
    }

    if (memcmp(cur, (localp ? LOCAL_MAGIC : CENTRAL_MAGIC), 4) != 0) {
	/* XXX: zip_err = ZERR_NOZIP; */
	return -1;
    }
    cur += 4;

    
    /* convert buffercontents to zip_dirent */
    
    if (!localp)
	zde->version_madeby = _zip_read2(&cur);
    else
	zde->version_madeby = 0;
    zde->version_needed = _zip_read2(&cur);
    zde->bitflags = _zip_read2(&cur);
    zde->comp_method = _zip_read2(&cur);
    
    /* convert to time_t */
    dostime = _zip_read2(&cur);
    dosdate = _zip_read2(&cur);
    zde->last_mod = _zip_d2u_time(dostime, dosdate);
    
    zde->crc = _zip_read4(&cur);
    zde->comp_size = _zip_read4(&cur);
    zde->uncomp_size = _zip_read4(&cur);
    
    zde->filename_len = _zip_read2(&cur);
    zde->extrafield_len = _zip_read2(&cur);
    
    if (localp) {
	zde->comment_len = 0;
	zde->disk_number = 0;
	zde->int_attrib = 0;
	zde->ext_attrib = 0;
	zde->offset = 0;
    } else {
	zde->comment_len = _zip_read2(&cur);
	zde->disk_number = _zip_read2(&cur);
	zde->int_attrib = _zip_read2(&cur);
	zde->ext_attrib = _zip_read4(&cur);
	zde->offset = _zip_read4(&cur);
    }

    zde->filename = NULL;
    zde->extrafield = NULL;
    zde->comment = NULL;

    if (bufp) {
	if (left < CDENTRYSIZE + (zde->filename_len+zde->extrafield_len
				  +zde->comment_len)) {
	    _zip_error_set(error, ZERR_NOZIP, 0);
	    return -1;
	}

	if (zde->filename_len) {
	    zde->filename = _zip_readstr(&cur, zde->filename_len, 1, error);
	    if (!zde->filename)
		    return -1;
	}

	if (zde->extrafield_len) {
	    zde->extrafield = _zip_readstr(&cur, zde->extrafield_len, 0,
					   error);
	    if (!zde->extrafield)
		return -1;
	}

	if (zde->comment_len) {
	    zde->comment = _zip_readstr(&cur, zde->comment_len, 0, error);
	    if (!zde->comment)
		return -1;
	}
    }
    else {
	if (zde->filename_len) {
	    zde->filename = _zip_readfpstr(fp, zde->filename_len, 1, error);
	    if (!zde->filename)
		    return -1;
	}

	if (zde->extrafield_len) {
	    zde->extrafield = _zip_readfpstr(fp, zde->extrafield_len, 0,
					     error);
	    if (!zde->extrafield)
		return -1;
	}

	if (zde->comment_len) {
	    zde->comment = _zip_readfpstr(fp, zde->comment_len, 0, error);
	    if (!zde->comment)
		return -1;
	}
    }

    if (bufp)
      *bufp = cur;

    return 0;
}



static time_t
_zip_d2u_time(int dtime, int ddate)
{
    struct tm *tm;
    time_t now;

    now = time(NULL);
    tm = localtime(&now);
    
    tm->tm_year = ((ddate>>9)&127) + 1980 - 1900;
    tm->tm_mon = ((ddate>>5)&15) - 1;
    tm->tm_mday = ddate&31;

    tm->tm_hour = (dtime>>11)&31;
    tm->tm_min = (dtime>>5)&63;
    tm->tm_sec = (dtime<<1)&62;

    return mktime(tm);
}



static unsigned short
_zip_read2(unsigned char **a)
{
    unsigned short ret;

    ret = (*a)[0]+((*a)[1]<<8);
    *a += 2;

    return ret;
}



static unsigned int
_zip_read4(unsigned char **a)
{
    unsigned int ret;

    ret = ((((((*a)[3]<<8)+(*a)[2])<<8)+(*a)[1])<<8)+(*a)[0];
    *a += 4;

    return ret;
}



static char *
_zip_readfpstr(FILE *fp, int len, int nulp, struct zip_error *error)
{
    char *r, *o;

    r = (char *)malloc(nulp?len+1:len);
    if (!r) {
	_zip_error_set(error, ZERR_MEMORY, 0);
	return NULL;
    }

    if (fread(r, 1, len, fp)<len) {
	free(r);
	_zip_error_set(error, ZERR_READ, errno);
	return NULL;
    }

    if (nulp) {
	/* elephant */
	/* XXX: what does this do? */
	r[len] = 0;
	o = r-1;
	while (((o=memchr(o+1, 0, r+len-(o+1))) < r+len) && o)
	       *o = ' ';
    }
    
    return r;
}



static char *
_zip_readstr(unsigned char **buf, int len, int nulp, struct zip_error *error)
{
    char *r, *o;

    r = (char *)malloc(nulp?len+1:len);
    if (!r) {
	_zip_error_set(error, ZERR_MEMORY, 0);
	return NULL;
    }
    
    memcpy(r, *buf, len);
    *buf += len;

    if (nulp) {
	/* elephant */
	/* XXX: what does this do? */
	r[len] = 0;
	o = r-1;
	while (((o=memchr(o+1, 0, r+len-(o+1))) < r+len) && o)
	       *o = ' ';
    }

    return r;
}
