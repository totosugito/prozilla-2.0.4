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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

//#include <malloc.h>
//#include <alloca.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "main.h"
#include "ftpsearch_win.h"
#include "interface.h"

FTPS_Window::FTPS_Window ()
{
	memset (&request, 0, sizeof (request));

	request_running = FALSE;
	ping_running = FALSE;
	got_mirror_info = FALSE;
}


void
FTPS_Window::fetch_mirror_info (urlinfo * u, off_t file_size,
				char *ftps_loc,
				ftpsearch_server_type_t server_type,
				int num_req_mirrors)
{
	assert (u->file);

	request = proz_ftps_request_init (u, file_size, ftps_loc,
					  server_type, num_req_mirrors);
  PrintMessage("Attempting to get %d mirrors from %s\n\n",num_req_mirrors, ftps_loc);
	//proz_connection_set_msg_proc (request->connection,
	//			      ftps_win_message_proc, this);
	proz_get_complete_mirror_list (request);

	request_running = TRUE;
}


//void
//ftps_win_message_proc (const char *msg, void *cb_data)
//{
//
//}

uerr_t
FTPS_Window::callback ()
{

	if (request_running == TRUE)
	{
		if (proz_request_info_running (request) == FALSE)
		{
			pthread_join (request->info_thread, NULL);
			if (request->err != MIRINFOK)
			{
				request_running = FALSE;
				got_mirror_info = FALSE;
				return FTPSFAIL;
			}
			else
			{
				print_status(request, rt.quiet_mode);
			}

			request_running = FALSE;
			got_mirror_info = TRUE;

			request->max_simul_pings = rt.max_simul_pings;
			request->ping_timeout.tv_sec = rt.max_ping_wait;
			request->ping_timeout.tv_usec = 0;

			//Launch the pinging thread
			PrintMessage("Got mirror info, %d server(s) found\n", request->num_mirrors);
			proz_mass_ping (request);
			ping_running = TRUE;
			return MASSPINGINPROGRESS;
		}
		return FTPSINPROGRESS;
	}

	if (ping_running == TRUE)
	{

		print_status(request, rt.quiet_mode);
    
		if (proz_request_mass_ping_running (request) == FALSE)
		{
			ping_done = TRUE;
			ping_running = FALSE;
			proz_sort_mirror_list (request->mirrors,
					       request->num_mirrors);

			// We have a seprate func to display this
			print_status(request, FALSE);
      
			return MASSPINGDONE;
		}
		return MASSPINGINPROGRESS;
	}
	return MASSPINGINPROGRESS;
}



void
FTPS_Window::cleanup ()
{
	if (request_running == TRUE)
	{
		proz_cancel_mirror_list_request (request);
		return;
	}

	if (ping_running == TRUE)
	{
		proz_cancel_mass_ping (request);
		return;
	}
}


void
cb_exit_ftpsearch (void *data)
{

	FTPS_Window *window = (FTPS_Window *) data;

	window->exit_ftpsearch_button_pressed = TRUE;

	if (window->request_running == TRUE)
	{
		proz_cancel_mirror_list_request (window->request);
	}

	if (window->ping_running == TRUE)
	{
		proz_cancel_mass_ping (window->request);
	}
}

void FTPS_Window::print_status(ftps_request_t *request, int quiet_mode)
{
    if (quiet_mode == FALSE || rt.display_mode == DISP_CURSES)
    {
		for (int i = 0; i < request->num_mirrors; i++)
		{
			pthread_mutex_lock (&request->access_mutex);
			ftp_mirror_stat_t status = request->mirrors[i].status;
			pthread_mutex_unlock (&request->access_mutex);
			switch (status)
			{
			case UNTESTED:
				DisplayInfo(i+1,1, "%-30.30s  %s\n",
					 request->mirrors[i].server_name,
					 "NOT TESTED");
				break;
			case RESPONSEOK:
				DisplayInfo(i+1,1, "%-30.30s  %dms\n",
					 request->mirrors[i].server_name,
					 request->mirrors[i].milli_secs);
				break;
			case NORESPONSE:
			case ERROR:
        DisplayInfo(i+1,1, "%-30.30s  %s\n",
					 request->mirrors[i].server_name,
					 "NO REPONSE");
				break;
			default:
				DisplayInfo(i+1,1, "%-30.30s  %s\n",
					 request->mirrors[i].server_name,
					 "Unkown condition!!");
				break;
			}
		}
    if (rt.display_mode == DISP_STDOUT)
      fprintf(stdout,"\n");
  
  }
  
}
