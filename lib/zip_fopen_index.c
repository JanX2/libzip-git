/*
  $NiH: zip_fopen_index.c,v 1.19 2004/04/19 11:49:13 dillo Exp $

  zip_fopen_index.c -- open file in zip archive for reading by index
  Copyright (C) 1999, 2004 Dieter Baron and Thomas Klausner

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



#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "zip.h"
#include "zipint.h"

static struct zip_file *_zip_file_new(struct zip *zf);



struct zip_file *
zip_fopen_index(struct zip *zf, int fileno, int flags)
{
    int len, ret;
    int zfflags;
    struct zip_file *zff;

    if ((fileno < 0) || (fileno >= zf->nentry)) {
	_zip_error_set(&zf->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((flags & ZIP_FL_UNCHANGED) == 0
	&& ZIP_ENTRY_DATA_CHANGED(zf->entry+fileno)) {
	_zip_error_set(&zf->error, ZIP_ER_CHANGED, 0);
	return NULL;
    }

    if (fileno >= zf->cdir->nentry) {
	_zip_error_set(&zf->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((flags & ZIP_FL_COMPRESSED)
	|| (zf->cdir->entry[fileno].comp_method == ZIP_CM_STORE))
	zfflags = ZIP_ZF_COMP;
    else {
	if (zf->cdir->entry[fileno].comp_method != ZIP_CM_DEFLATE) {
	    _zip_error_set(&zf->error, ZIP_ER_COMPNOTSUPP, 0);
	    return NULL;
	}
	zfflags = 0;
    }

    zff = _zip_file_new(zf);

    zff->flags = zfflags;
    /* zff->name = zf->cdir->entry[fileno].filename; */
    zff->method = zf->cdir->entry[fileno].comp_method;
    zff->bytes_left = zf->cdir->entry[fileno].uncomp_size;
    zff->cbytes_left = zf->cdir->entry[fileno].comp_size;
    zff->crc_orig = zf->cdir->entry[fileno].crc;

    if ((zff->fpos=_zip_file_get_offset(zf, fileno)) == 0) {
	zip_fclose(zff);
	return NULL;
    }
    
    if (zff->flags & ZIP_ZF_COMP)
	zff->bytes_left = zff->cbytes_left;
    else {
	if ((zff->buffer=(char *)malloc(BUFSIZE)) == NULL) {
	    _zip_error_set(&zf->error, ZIP_ER_MEMORY, 0);
	    zip_fclose(zff);
	    return NULL;
	}

	len = _zip_file_fillbuf(zff->buffer, BUFSIZE, zff);
	if (len <= 0) {
	    _zip_error_copy(&zf->error, &zff->error);
	    zip_fclose(zff);
	return NULL;
	}

	if ((zff->zstr = (z_stream *)malloc(sizeof(z_stream))) == NULL) {
	    _zip_error_set(&zf->error, ZIP_ER_MEMORY, 0);
	    zip_fclose(zff);
	    return NULL;
	}
	zff->zstr->zalloc = Z_NULL;
	zff->zstr->zfree = Z_NULL;
	zff->zstr->opaque = NULL;
	zff->zstr->next_in = zff->buffer;
	zff->zstr->avail_in = len;
	
	/* negative value to tell zlib that there is no header */
	if ((ret=inflateInit2(zff->zstr, -MAX_WBITS)) != Z_OK) {
	    _zip_error_set(&zf->error, ZIP_ER_ZLIB, ret);
	    zip_fclose(zff);
	    return NULL;
	}
    }
    
    return zff;
}



int
_zip_file_fillbuf(void *buf, size_t buflen, struct zip_file *zff)
{
    int i, j;

    if (zff->error.zip_err != ZIP_ER_OK)
	return -1;

    if ((zff->flags & ZIP_ZF_EOF) || zff->cbytes_left <= 0 || buflen <= 0)
	return 0;
    
    if (fseek(zff->zf->zp, zff->fpos, SEEK_SET) < 0) {
	_zip_error_set(&zff->error, ZIP_ER_SEEK, errno);
	return -1;
    }
    if (buflen < zff->cbytes_left)
	i = buflen;
    else
	i = zff->cbytes_left;

    j = fread(buf, 1, i, zff->zf->zp);
    if (j == 0) {
	_zip_error_set(&zff->error, ZIP_ER_EOF, 0);
	j = -1;
    }
    else if (j < 0)
	_zip_error_set(&zff->error, ZIP_ER_READ, errno);
    else {
	zff->fpos += j;
	zff->cbytes_left -= j;
    }

    return j;	
}



static struct zip_file *
_zip_file_new(struct zip *zf)
{
    struct zip_file *zff, **file;
    int n;

    if ((zff=(struct zip_file *)malloc(sizeof(struct zip_file))) == NULL) {
	_zip_error_set(&zf->error, ZIP_ER_MEMORY, 0);
	return NULL;
    }
    
    if (zf->nfile >= zf->nfile_alloc-1) {
	n = zf->nfile_alloc + 10;
	file = (struct zip_file **)realloc(zf->file,
					   n*sizeof(struct zip_file *));
	if (file == NULL) {
	    _zip_error_set(&zf->error, ZIP_ER_MEMORY, 0);
	    free(zff);
	    return NULL;
	}
	zf->nfile_alloc = n;
	zf->file = file;
    }

    zf->file[zf->nfile++] = zff;

    zff->zf = zf;
    _zip_error_init(&zff->error);
    zff->flags = 0;
    zff->crc = crc32(0L, Z_NULL, 0);
    zff->crc_orig = 0;
    zff->method = -1;
    zff->bytes_left = zff->cbytes_left = 0;
    zff->fpos = 0;
    zff->buffer = NULL;
    zff->zstr = NULL;

    return zff;
}
