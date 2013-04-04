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

/* FTP support. */

/* $Id: ftp.h,v 1.35 2005/09/04 00:06:50 kalum Exp $ */


#ifndef FTP_H
#define FTP_H


#include "common.h"
#include "url.h"
#include "connection.h"

#ifdef __cplusplus
extern "C" {
#endif

  int ftp_check_msg(connection_t * connection, int len);
  int ftp_read_msg(connection_t * connection, int len);
  uerr_t ftp_send_msg(connection_t * connection, const char *format, ...);

  uerr_t ftp_get_line(connection_t * connection, char *line);

  uerr_t ftp_ascii(connection_t * connection);
  uerr_t ftp_binary(connection_t * connection);
  uerr_t ftp_port(connection_t * connection, const char *command);
  uerr_t ftp_list(connection_t * connection, const char *file);
  uerr_t ftp_retr(connection_t * connection, const char *file);
  uerr_t ftp_pasv(connection_t * connection, unsigned char *addr);
  uerr_t ftp_rest(connection_t * connection, off_t bytes);
  uerr_t ftp_cwd(connection_t * connection, const char *dir);
  uerr_t ftp_pwd(connection_t * connection, char *dir);
  uerr_t ftp_size(connection_t * connection, const char *file, off_t *size);

  uerr_t ftp_connect_to_server(connection_t * connection, const char *name,
			       int port);

  uerr_t ftp_get_listen_socket(connection_t * connection,
			       int *listen_sock);

  uerr_t ftp_login(connection_t * connection, const char *username,
		   const char *passwd);

  boolean ftp_use_proxy(connection_t * connection);

  uerr_t proz_ftp_get_url_info(connection_t * connection);

  uerr_t ftp_setup_data_sock_1(connection_t * connection,
			       boolean * passive_mode);
  uerr_t ftp_setup_data_sock_2(connection_t * connection,
			       boolean * passive_mode);
  uerr_t ftp_get_url_info_loop(connection_t * connection);

  uerr_t ftp_get_url_info_from_http_proxy(connection_t * connection);
#ifdef __cplusplus
}
#endif
#endif				/* FTP_H */
