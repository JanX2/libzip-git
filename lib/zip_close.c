/*
  $NiH: zip_close.c,v 1.37.4.4 2004/04/06 21:43:35 dillo Exp $

  zip_close.c -- close zip archive and update changes
  Copyright (C) 1999 Dieter Baron and Thomas Klausner

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

static int add_data(struct zip *, int, struct zip_dirent *, FILE *);
static int add_data_comp(zip_read_func, void *, struct zip_dirent *, FILE *,
			 struct zip_error *);
static int add_data_uncomp(zip_read_func, void *, struct zip_dirent *, FILE *,
			   struct zip_error *);
static int copy_data(FILE *, off_t, off_t, FILE *, struct zip_error *);




int
zip_close(struct zip *za)
{
    int changed, survivors;
    int i, count, tfd, ret, error;
    char *temp;
    FILE *tfp;
    mode_t mask;
    struct zip_cdir *cd;
    struct zip_dirent de;

    changed = survivors = 0;
    for (i=0; i<za->nentry; i++) {
	if (za->entry[i].state != ZIP_ST_UNCHANGED)
	    changed = 1;
	if (za->entry[i].state != ZIP_ST_DELETED)
	    survivors++;
    }

    if (!changed) {
	_zip_free(za);
	return 0;
    }

    /* don't create zip files with no entries */
    if (survivors == 0) {
	ret = 0;
	if (za->zn)
	    ret = remove(za->zn);
	_zip_free(za);
	/* XXX: inconsistent: za freed, returned -1 */
	return ret;
    }	       
	
    if ((cd=malloc(sizeof(*cd))) == NULL) {
	_zip_error_set(&za->error, ZERR_MEMORY, 0);
	return -1;
    }
    cd->nentry = 0;

    if ((cd->entry=malloc(sizeof(*(cd->entry))*survivors)) == NULL) {
	_zip_error_set(&za->error, ZERR_MEMORY, 0);
	free(cd);
	return -1;
    }

    if ((temp=malloc(strlen(za->zn)+8)) == NULL) {
	_zip_error_set(&za->error, ZERR_MEMORY, 0);
	_zip_cdir_free(cd);
	return -1;
    }

    sprintf(temp, "%s.XXXXXX", za->zn);

    if ((tfd=mkstemp(temp)) == -1) {
	_zip_error_set(&za->error, ZERR_TMPOPEN, errno);
	_zip_cdir_free(cd);
	free(temp);
	return -1;
    }
    
    if ((tfp=fdopen(tfd, "r+b")) == NULL) {
	_zip_error_set(&za->error, ZERR_TMPOPEN, errno);
	_zip_cdir_free(cd);
	close(tfd);
	remove(temp);
	free(temp);
	return -1;
    }

    error = 0;
    for (i=0; i<za->nentry; i++) {
	if (za->entry[i].state == ZIP_ST_DELETED)
	    continue;

	if (i < za->cdir->nentry) {
	    if (fseek(za->zp, za->cdir->entry[i].offset, SEEK_SET) != 0) {
		_zip_error_set(&za->error, ZERR_SEEK, errno);
		error = 1;
		break;
	    }
	    if (_zip_dirent_read(&de, za->zp, NULL, 0, 1, &za->error) != 0) {
		error = 1;
		break;
	    }
	}
	else {
	    de.filename = NULL;
	    de.filename_len = 0;
	    de.extrafield = NULL;
	    de.extrafield_len = 0;
	    de.comment = NULL;
	    de.comment_len = 0;
	}

	if (za->entry[i].ch_filename) {
	    free(de.filename);
	    de.filename = za->entry[i].ch_filename;
	    de.filename_len = strlen(de.filename);
	    za->entry[i].ch_filename = NULL;
	}

	cd->entry[cd->nentry].offset = ftell(tfp);

	if (za->entry[i].state == ZIP_ST_REPLACED
	    || za->entry[i].state == ZIP_ST_ADDED) {
	    de.last_mod = za->entry[i].ch_mtime;
	    if (add_data(za, i, &de, tfp) < 0) {
		error -1;
		break;
	    }
	}
	else {
	    if (_zip_dirent_write(&de, tfp, 1, &za->error) < 0) {
		error = 1;
		break;
	    }
	    if (copy_data(za->zp, cd->entry[i].offset, de.uncomp_size,
			  tfp, &za->error) < 0) {
		error = 1;
		break;
	    }
	}
	    
	/* XXX: merge za->cdir->entry[i] and de into cd->entry[cd->nentry] */
	cd->nentry++;

	_zip_dirent_finalize(&de);
    }

    if (!error) {
	if (_zip_cdir_write(cd, tfp, &za->error) < 0)
	    error = 1;
    }
    
    if (error) {
	_zip_dirent_finalize(&de);
	_zip_cdir_free(cd);
	fclose(tfp);
	remove(temp);
	free(temp);
	return -1;
    }

    _zip_cdir_free(cd);

    if (fclose(tfp) != 0) {
	/* XXX: handle fclose(tfp) error */
	remove(temp);
	free(temp);
	return -1;
    }
    
    if (rename(temp, za->zn) != 0) {
	_zip_error_set(&za->error, ZERR_RENAME, errno);
	remove(temp);
	free(temp);
	return -1;
    }
    if (za->zp) {
	fclose(za->zp);
	za->zp = NULL;
    }
    mask = umask(0);
    umask(mask);
    chmod(za->zn, 0666&~mask);

    _zip_free(za);
    
    return 0;
}



static int
add_data(struct zip *za, int idx, struct zip_dirent *de, FILE *ft)
{
    off_t offstart, offend;

    offstart = ftell(ft);

    if (_zip_dirent_write(de, ft, 1, &za->error) < 0)
	return -1;

    if (za->entry[idx].ch_flags & ZIP_CH_ISCOMP) {
	if (add_data_comp(za->entry[idx].ch_func, za->entry[idx].ch_data,
			  de, ft, &za->error) < 0)
	    return -1;
    }
    else {
	if (add_data_uncomp(za->entry[idx].ch_func, za->entry[idx].ch_data,
			    de, ft, &za->error) < 0)
	    return -1;
    }

    offend = ftell(ft);

    if (fseek(ft, offstart, SEEK_SET) < 0) {
	_zip_error_set(&za->error, ZERR_SEEK, errno);
	return -1;
    }
    
    if (_zip_dirent_write(de, ft, 1, &za->error) < 0)
	return -1;
    
    if (fseek(ft, offend, SEEK_SET) < 0) {
	_zip_error_set(&za->error, ZERR_SEEK, errno);
	return -1;
    }

    return 0;
}



static int
add_data_comp(zip_read_func rf, void *ud, struct zip_dirent *de, FILE *ft,
	      struct zip_error *error)
{
    char buf[8192];
    ssize_t n;
    struct zip_stat st;

    if (rf(ud, NULL, 0, ZIP_CMD_INIT) < 0) {
	/* XXX: set error */
	return -1;
    }

    if (rf(ud, &st, sizeof(st), ZIP_CMD_STAT) < 0) {
	/* XXX: set error */
	return -1;
    }

    de->comp_size = 0;
    while ((n=rf(ud, buf, sizeof(buf), ZIP_CMD_READ)) > 0) {
	if (fwrite(buf, 1, n, ft) != n) {
	    _zip_error_set(error, ZERR_WRITE, errno);
	    return -1;
	}
	
	de->comp_size += n;
    }
    if (n < 0) {
	/* XXX: set error */
	return -1;
    }	

    if (rf(ud, NULL, 0, ZIP_CMD_CLOSE) < 0) {
	/* XXX: set error */
	return -1;
    }

    de->comp_method = st.comp_method;
    /* de->last_mod = st.mtime; */
    de->crc = st.crc;
    de->uncomp_size = st.size;

    return 0;
}



static int
add_data_uncomp(zip_read_func rf, void *ud, struct zip_dirent *de, FILE *ft,
		struct zip_error *error)
{
    char b1[8192], b2[8192];
    int end, flush, ret;
    ssize_t n;
    z_stream zstr;

    if (rf(ud, NULL, 0, ZIP_CMD_INIT) < 0) {
	/* XXX: set error */
	return -1;
    }

    /* ZIP_CMD_STAT for mtime? */

    de->comp_method = ZIP_CM_DEFLATE;
    de->comp_size = de->uncomp_size = 0;
    de->crc = crc32(0, NULL, 0);

    zstr.zalloc = Z_NULL;
    zstr.zfree = Z_NULL;
    zstr.opaque = NULL;
    zstr.avail_in = 0;
    zstr.avail_out = 0;

    /* -15: undocumented feature of zlib to _not_ write a zlib header */
    deflateInit2(&zstr, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 9,
		 Z_DEFAULT_STRATEGY);

    zstr.next_out = b2;
    zstr.avail_out = sizeof(b2);
    zstr.avail_in = 0;

    flush = 0;
    end = 0;
    while (!end) {
	if (zstr.avail_in == 0 && !flush) {
	    if ((n=rf(ud, b1, sizeof(b1), ZIP_CMD_READ)) < 0) {
		/* XXX: set error */
		deflateEnd(&zstr);
		return -1;
	    }
	    if (n > 0) {
		zstr.avail_in = n;
		zstr.next_in = b1;
		de->uncomp_size += n;
		de->crc = crc32(de->crc, b1, n);
	    }
	    else
		flush = Z_FINISH;
	}

	ret = deflate(&zstr, flush);
	if (ret != Z_OK && ret != Z_STREAM_END) {
	    _zip_error_set(error, ZERR_ZLIB, 0 /* XXX */);
	    return -1;
	}
	
	if (zstr.avail_out != sizeof(b2)) {
	    n = sizeof(b2) - zstr.avail_out;
	    
	    if (fwrite(b2, 1, n, ft) != n) {
		_zip_error_set(error, ZERR_WRITE, errno);
		return -1;
	    }
	
	    zstr.next_out = b2;
	    zstr.avail_out = sizeof(b2);
	    de->comp_size += n;
	}

	if (ret == Z_STREAM_END) {
	    deflateEnd(&zstr);
	    end = 1;
	}
    }

    if (rf(ud, NULL, 0, ZIP_CMD_CLOSE) < 0) {
	/* XXX: set error */
	return -1;
    }

    return 0;
}



static int
copy_data(FILE *fs, off_t offset, off_t len, FILE *ft, struct zip_error *error)
{
    char buf[8192];
    int n, nn;

    if (len == 0)
	return 0;

    if (fseek(fs, offset, SEEK_SET) < 0) {
	_zip_error_set(error, ZERR_SEEK, errno);
	return -1;
    }

    while (len > 0) {
	nn = len > sizeof(buf) ? sizeof(buf) : len;
	if ((n=fread(buf, 1, nn, fs)) < 0) {
	    _zip_error_set(error, ZERR_READ, errno);
	    return -1;
	}

	if (fwrite(buf, 1, n, ft) != n) {
	    _zip_error_set(error, ZERR_WRITE, errno);
	    return -1;
	}
	
	len -= n;
    }

    return 0;
}
