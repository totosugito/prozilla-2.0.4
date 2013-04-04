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

/* A test program. */

/* $Id: test.c,v 1.30 2001/09/23 01:39:16 kalum Exp $ */


#include "common.h"
#include "prozilla.h"
#include "connection.h"
#include "misc.h"
#include "connect.h"
#include "ftp.h"
#include "http.h"
#include "download.h"
#include "ftpsearch.h"
#include "ping.h"


/******************************************************************************
 The main function.
******************************************************************************/
int main(int argc, char **argv)
{
  uerr_t err;
  connection_t *connection;
  ftps_request_t request;
  struct timeval timeout, ping_time;
  int i, j;

  proz_init(argc, argv);
  libprozrtinfo.conn_timeout.tv_sec = 100;

  proz_prepare_ftps_request(&request, "ddd-3.3.tar.bz2", 4811576,
			    "http://ftpsearch.uniovi.es:8000/ftpsearch",
			    LYCOS, 40);

  proz_get_complete_mirror_list(&request);

  for (i = 0; i < request.num_mirrors; i++)
  {
    printf("%s\n", request.mirrors[i].server_name);
    printf("\t%d\n", request.mirrors[i].num_paths);
    for (j = 0; j < request.mirrors[i].num_paths; j++)
      printf("\t\t%s\n", request.mirrors[i].paths[j].path);
  }
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;

  proz_shutdown();

  exit(EXIT_SUCCESS);
}
