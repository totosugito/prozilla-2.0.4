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

/* $Id: http-retr.h,v 1.5 2001/06/25 12:30:56 kalum Exp $ */


#ifndef HTTP_RETR_H
#define HTTP_RETR_H

#include "common.h"
#include "connection.h"

#ifdef __cplusplus
extern "C" {
#endif

  uerr_t proz_http_get_file(connection_t * connection);
  uerr_t http_loop(connection_t * connection);
  boolean http_loop_handle_error(uerr_t err);
#ifdef __cplusplus
}
#endif
#endif				/* FTP_RETR_H */
