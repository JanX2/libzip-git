/*
  $NiH: zip_stat_index.c,v 1.1.4.1 2004/03/22 14:17:34 dillo Exp $

  zip_stat_index.c -- get information about file by index
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



#include "zip.h"
#include "zipint.h"



int
zip_stat_index(struct zip *za, int index, struct zip_stat *st)
{
    if (index < 0 || index >= za->nentry) {
	_zip_error_set(&za->error, ZERR_INVAL, 0);
	return -1;
    }

    if (za->entry[index].state != ZIP_ST_UNCHANGED
	&& za->entry[index].state != ZIP_ST_RENAMED) {
	_zip_error_set(&za->error, ZERR_CHANGED, 0);
	return NULL;
    }
    
    st->name = zip_get_name(za, index);
    st->index = index;
    st->crc = za->cdir->entry[index].crc;
    st->size = za->cdir->entry[index].uncomp_size;
    st->mtime = za->cdir->entry[index].last_mod;
    st->comp_size = za->cdir->entry[index].comp_size;
    st->comp_method = za->cdir->entry[index].comp_method;
    /* st->bitflags = za->cdir->entry[index].bitflags; */

    return 0;
}
