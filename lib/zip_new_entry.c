/*
  $NiH: zip_new_entry.c,v 1.7.4.2 2004/03/22 14:56:48 dillo Exp $

  zip_new_entry.c -- create and init struct zip_entry
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

#include "zip.h"
#include "zipint.h"



struct zip_entry *
_zip_new_entry(struct zip *zf)
{
    struct zip_entry *ze;
    if (!zf) {
	ze = (struct zip_entry *)malloc(sizeof(struct zip_entry));
	if (!ze) {
	    _zip_error_set(&zf->error, ZERR_MEMORY, 0);
	    return NULL;
	}
    }
    else {
	if (zf->nentry >= zf->nentry_alloc-1) {
	    zf->nentry_alloc += 16;
	    zf->entry = (struct zip_entry *)realloc(zf->entry,
						    sizeof(struct zip_entry)
						    * zf->nentry_alloc);
	    if (!zf->entry) {
		_zip_error_set(&zf->error, ZERR_MEMORY, 0);
		return NULL;
	    }
	}
	ze = zf->entry+zf->nentry;
    }

    ze->state = ZIP_ST_UNCHANGED;

    ze->ch_filename = NULL;
    ze->ch_func = NULL;
    ze->ch_data = NULL;
    ze->ch_flags = 0;
    ze->ch_comp_method = ZIP_CM_DEFAULT;
    ze->ch_mtime = -1;

    if (zf)
	zf->nentry++;

    return ze;
}
