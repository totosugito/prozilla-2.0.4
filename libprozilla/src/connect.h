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

/* Connection routines. */

/* $Id: connect.h,v 1.17 2001/05/09 22:58:00 kalum Exp $ */


#ifndef CONNECT_H
#define CONNECT_H


#include "common.h"
#include "prozilla.h"


#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

  uerr_t connect_to_server(int *sock, const char *name, int port,
			   struct timeval *timeout);

  uerr_t bind_socket(int *sockfd);

  int select_fd(int fd, struct timeval *timeout, int writep);

  int krecv(int sock, char *buffer, int size, int flags,
	    struct timeval *timeout);
  int ksend(int sock, char *buffer, int size, int flags,
	    struct timeval *timeout);
  uerr_t accept_connection(int listen_sock, int *data_sock);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* CONNECT_H */
