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

/* HTTP support. */

/* $Id: http.h,v 1.12 2005/03/31 20:10:57 sean Exp $ */


#ifndef HTTP_H
#define HTTP_H


#include "common.h"
#include "url.h"
#include "connection.h"

#ifdef __cplusplus
extern "C" {
#endif

  int buf_readchar(int fd, char *ret, struct timeval *timeout);
  int buf_peek(int fd, char *ret, struct timeval *timeout);
  uerr_t fetch_next_header(int fd, char **hdr, struct timeval *timeout);

  int hparsestatline(const char *hdr, const char **rp);
  int hskip_lws(const char *hdr);
  off_t hgetlen(const char *hdr);
  off_t hgetrange(const char *hdr);
  char *hgetlocation(const char *hdr);
  char *hgetmodified(const char *hdr);
  int hgetaccept_ranges(const char *hdr);

  uerr_t http_fetch_headers(connection_t * connection, http_stat_t * hs,
			    char *command);

  char *get_basic_auth_str(char *user, char *passwd, char *auth_header);

  boolean http_use_proxy(connection_t * connection);
  uerr_t proz_http_get_url_info(connection_t * connection);
  uerr_t http_get_url_info_loop(connection_t * connection);
#ifdef __cplusplus
}
#endif
#endif				/* HTTP_H */
