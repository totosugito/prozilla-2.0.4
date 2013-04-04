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

/* $Id: ftp-retr.h,v 1.6 2001/06/25 12:30:55 kalum Exp $ */

#ifndef FTP_RETR_H
#define FTP_RETR_H

#include "common.h"
#include "connection.h"

#ifdef __cplusplus
extern "C" {
#endif

  uerr_t proz_ftp_get_file(connection_t * connection);
  uerr_t ftp_loop(connection_t * connection);
  boolean ftp_loop_handle_error(uerr_t err);
  uerr_t ftp_get_file_from_http_proxy(connection_t * connection);

#ifdef __cplusplus
}
#endif
#endif				/* FTP_RETR_H */
