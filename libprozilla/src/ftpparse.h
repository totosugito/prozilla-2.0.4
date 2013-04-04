/******************************************************************************
 libprozilla - a download accelerator library
 Copyright (C) 2001 Kalum Somaratna

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
******************************************************************************/

/* FTP LIST command parsing code. */

/* $Id: ftpparse.h,v 1.15 2005/08/06 17:07:35 kalum Exp $ */


#ifndef FTPPARSE_H
#define FTPPARSE_H


#include "common.h"
#include "prozilla.h"
#ifdef __cplusplus
extern "C" {
#endif


typedef struct ftpparse {
  char *filename; 
  int namelen;
  off_t filesize; /* number of octets */
  //  int mtimetype;
  time_t mtime; /* modification time */
  file_type_t filetype;
  char *id; /* not necessarily 0-terminated */
char *date_str;
}ftpparse ;


uerr_t ftp_parse(ftpparse *fp,char *buf,int len);

#ifdef __cplusplus
}
#endif
#endif				/* FTPPARSE_H */
