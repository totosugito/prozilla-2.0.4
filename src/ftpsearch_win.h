/******************************************************************************
 fltk prozilla - a front end for prozilla, a download accelerator library
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

#ifndef FTPSEARCH_WIN_H
#define FTPSEARCH_WIN_H

#include <config.h>
#include "prozilla.h"
//#include "ftps_win.h"

//void ftps_win_message_proc (const char *msg, void *cb_data);


class FTPS_Window {
public:
  FTPS_Window();
  void fetch_mirror_info(urlinfo *u, off_t file_size, char *ftps_loc,
			 ftpsearch_server_type_t server_type,
			 int num_req_mirrors);

  void cleanup();

  uerr_t callback();
  void ping_list();
  void print_status(ftps_request_t * request, int quiet_mode);

  ftps_request_t *request;

  boolean request_running;
  boolean ping_running;
  boolean got_mirror_info;
  boolean ping_done;
  boolean exit_ftpsearch_button_pressed;

private:

};

#endif
