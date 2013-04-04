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

/* Several connection-related routines. */

/* $Id: connection.h,v 1.34 2005/01/11 01:49:11 sean Exp $ */


#ifndef CONNECTION_H
#define CONNECTION_H


#include "common.h"
#include "prozilla.h"
#include "url.h"


#ifdef __cplusplus
extern "C" {
#endif

  void init_response(connection_t * connection);
  void done_with_response(connection_t * connection);
  void connection_change_status(connection_t * connection,
				dl_status status);


  uerr_t connection_retr_fsize_not_known(connection_t * connection,
					 char *read_buffer,
					 int read_buffer_size);

  uerr_t connection_retr_fsize_known(connection_t * connection,
				     char *read_buffer,
				     int read_buffer_size);
  int connection_load_resume_info(connection_t * connection);

  void connection_show_message(connection_t * connection,
			       const char *format, ...);
  void connection_calc_ratebps(connection_t * connection);
  void connection_throttle_bps(connection_t * connection);
  void get_url_info_loop(connection_t * connection);
  size_t write_data_with_lock(connection_t *connection, const void *ptr, size_t size, size_t nmemb);

#ifdef __cplusplus
}
#endif
#endif				/* CONNECTION_H */
