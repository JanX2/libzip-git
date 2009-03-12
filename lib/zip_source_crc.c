/*
  zip_source_crc.c -- pass-through source that calculates CRC32
  Copyright (C) 2009 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <libzip@nih.at>

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

#include "zipint.h"

struct crc {
    struct zip_source *src;
    int eof;
    zip_uint64_t size;
    zip_uint32_t crc;
};

static zip_int64_t crc_read(void *, void *, zip_uint64_t, enum zip_source_cmd);



ZIP_EXTERN struct zip_source *
zip_source_crc(struct zip *za, struct zip_source *src)
{
    struct crc *ctx;

    if (za == NULL || src == NULL) {
	_zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((ctx=(struct crc *)malloc(sizeof(*ctx))) == NULL) {
	_zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    ctx->src = src;


    return zip_source_function(za, crc_read, ctx);
}



static zip_int64_t
crc_read(void *_ctx, void *data, zip_uint64_t len, enum zip_source_cmd cmd)
{
    struct crc *ctx;
    zip_int64_t n;

    ctx = (struct crc *)_ctx;

    switch (cmd) {
    case ZIP_SOURCE_OPEN:
	if (zip_source_call(ctx->src, data, len, cmd) < 0)
	    return -1;

	ctx->eof = 0;
	ctx->crc = crc32(0, NULL, 0);
	ctx->size = 0;

	return 0;

    case ZIP_SOURCE_READ:
	if (ctx->eof || len == 0)
	    return 0;

	if ((n=zip_source_call(ctx->src, data, len, cmd)) < 0)
	    return -1;

	if (n == 0)
	    ctx->eof = 1;
	else {
	    ctx->size += n;
	    ctx->crc = crc32(ctx->crc, data, n);
	}
	return n;

    case ZIP_SOURCE_CLOSE:
	if (zip_source_call(ctx->src, data, len, cmd) < 0)
	    return -1;
	return 0;

    case ZIP_SOURCE_STAT:
	if (zip_source_call(ctx->src, data, len, cmd) < 0)
	    return -1;
	else {
	    struct zip_stat *st;

	    st = (struct zip_stat *)data;

	    if (ctx->eof) {
		/* XXX: error if mismatch with what src provided? */
		st->size = ctx->size;
		st->crc = ctx->crc;
		st->valid |= ZIP_STAT_SIZE|ZIP_STAT_CRC;
	    }
	}
	return 0;
	
    case ZIP_SOURCE_ERROR:
	return zip_source_call(ctx->src, data, len, cmd);

    case ZIP_SOURCE_FREE:
	zip_source_call(ctx->src, data, len, cmd);
	free(ctx);
	return 0;

    default:
	return zip_source_call(ctx->src, data, len, cmd);
    }
    
}
