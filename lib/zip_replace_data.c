/*
  $NiH: zip_replace_data.c,v 1.12 2003/10/02 14:13:32 dillo Exp $

  zip_replace_data.c -- replace file from buffer
  Copyright (C) 1999, 2003 Dieter Baron and Thomas Klausner

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



#include <stdlib.h>
#include <string.h>

#include "zip.h"
#include "zipint.h"

struct read_data {
    const char *buf, *data;
    off_t len;
    int freep;
};

static ssize_t read_data(void *state, void *data, size_t len,
			 enum zip_cmd cmd);



int
zip_replace_data(struct zip *zf, int idx,
		 const void *data, off_t len, int freep)
{
    if (idx < 0 || idx >= zf->nentry) {
	_zip_error_set(&zf->error, ZERR_INVAL, 0);
	return -1;
    }
    
    return _zip_replace_data(zf, idx, NULL, data, len, freep);
}



int
_zip_replace_data(struct zip *zf, int idx, const char *name,
		  const void *data, off_t len, int freep)
{
    struct read_data *f;

    if ((f=malloc(sizeof(*f))) == NULL) {
	_zip_error_set(&zf->error, ZERR_MEMORY, 0);
	return -1;
    }

    f->data = data;
    f->len = len;
    f->freep = freep;
    
    return _zip_replace(zf, idx, name, read_data, f, 0);
}



static int
read_data(void *state, void *data, size_t len, enum zip_cmd cmd)
{
    struct read_data *z;
    char *buf;
    int n;

    z = (struct read_data *)state;
    buf = (char *)data;

    switch (cmd) {
    case ZIP_CMD_INIT:
	z->buf = z->data;
	return 0;
	
    case ZIP_CMD_READ:
	n = len > z->len ? z->len : len;
	if (n < 0)
	    n = 0;

	if (n) {
	    memcpy(buf, z->buf, n);
	    z->buf += n;
	    z->len -= n;
	}

	return n;
	
    case ZIP_CMD_CLOSE:
	if (z->freep) {
	    free((void *)z->data);
	    z->data = NULL;
	}
	free(z);
	return 0;

    default:
	;
    }

    return -1;
}
