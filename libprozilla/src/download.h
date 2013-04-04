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

/* Download routines. */

/* $Id: download.h,v 1.25 2001/09/30 23:13:50 kalum Exp $ */


#ifndef DOWNLOAD_H
#define DOWNLOAD_H


#include "common.h"
#include "prozilla.h"
#include "connection.h"


#ifdef __cplusplus
extern "C" {
#endif

  void download_show_message(download_t * download, const char *format,
			     ...);
  int download_query_conns_status_count(download_t * download,
					dl_status status, char *server);

  void download_join_downloads(download_t * download);
  void join_downloads(download_t * download);
  void download_calc_throttle_factor(download_t * download);
  uerr_t download_handle_threads_ftpsearch(download_t * download);
  uerr_t download_handle_threads_no_ftpsearch(download_t * download);
#ifdef __cplusplus
}
#endif
#endif				/* DOWNLOAD_H */
