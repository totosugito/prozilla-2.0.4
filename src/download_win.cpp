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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif	

#include "main.h"
#include "download_win.h"
#include "interface.h"

void
DL_Window::cleanup (boolean erase_dlparts)
{

	/*handle cleanup */
	if (status == DL_DOWNLOADING)
	{
		proz_download_stop_downloads (download);
		if (erase_dlparts == TRUE)
		{
			proz_download_delete_target (download);
			proz_log_delete_logfile (download);
		}
	}
	else if (status == DL_GETTING_INFO)
	{
		/*terminate info thread */
		pthread_cancel (info_thread);
		pthread_join (info_thread, NULL);
	}
	else if (status == DL_JOINING)
	{
		/*terminate joining thread */
		proz_download_cancel_joining_thread (download);
		pthread_join (download->join_thread, NULL);
	}
	else if (status == DL_FTPSEARCHING)
	{
		ftpsearch_win->cleanup ();
	}

	status = DL_ABORTED;
}

DL_Window::~DL_Window ()
{
	proz_connection_free_connection (connection, true);
	delete (ftpsearch_win);

}

DL_Window::DL_Window (urlinfo * url_data)
{
//  key = 0;
	got_info = FALSE;
	got_dl = FALSE;
	status = DL_IDLING;

	memcpy (&u, url_data, sizeof (u));
	memset (&update_time, 0, sizeof (struct timeval));
	num_connections = rt.num_connections;

	ftpsearch_win = new FTPS_Window ();
	do_ftpsearch = FALSE;
	using_ftpsearch = FALSE;

}

void
DL_Window::my_cb ()
{
	if (status == DL_GETTING_INFO)
	{
		handle_info_thread ();
		return;
	}


	// if ((got_info == TRUE && status == DL_IDLING && got_dl == FALSE)
	if ((status == DL_RESTARTING && got_info == TRUE)
	    || status == DL_DLPRESTART)
	{
		do_download ();
	}

	if (status == DL_DOWNLOADING)
	{
		handle_download_thread ();

		return;
	}

	if (status == DL_JOINING)
	{
		handle_joining_thread ();
		return;
	}

	if (status == DL_FATALERR)
	{
		handle_dl_fatal_error ();
		return;
	}

	if (status == DL_PAUSED)
	{
		//TODO what to do when paused 
		return;
	}
	if (status == DL_FTPSEARCHING)
	{
		handle_ftpsearch ();
		return;
	}

}



void
DL_Window::dl_start (int num_connections, boolean ftpsearch)
{

	do_ftpsearch = ftpsearch;

	connection = proz_connection_init (&u, &getinfo_mutex);
	proz_connection_set_msg_proc (connection, ms, this);

	PrintMessage("Creating the thread that gets info about file..\n");
	proz_get_url_info_loop (connection, &info_thread);

	status = DL_GETTING_INFO;
}


void
DL_Window::do_download ()
{
  logfile lf;
  status = DL_DLPRESTART;
  //setup the download 
  download = proz_download_init (&connection->u);
  proz_debug("proz_download_init complete");
  handle_prev_download ();
  if(status==DL_FATALERR)
    {
      return;
    }

  if(download->resume_mode ==TRUE)
    {
      if (proz_log_read_logfile(&lf, download, FALSE) != 1)
	{
	  PrintMessage
	    ("A error occured while processing the logfile! Assuming default number of connections");
	}
      // validate the info returned
      //TODO check whether files size is the same and if not prompt the user what to do.
      if (lf.num_connections != num_connections)
	{
	  PrintMessage
	    ("The previous download used a different number of connections than the default! so I will use the previous number of threads");
	  num_connections = lf.num_connections;
	  assert(num_connections > 0);
	  // cleanup(FALSE);
	}
    }
 if (rt.display_mode == DISP_CURSES)
   erase();

  start_download ();
}

void
DL_Window::handle_prev_download ()
{
  int ret = 0;
  /* we will check and see if 1. previous partial download exists and
     2. if the file to be downloaded already exists in the local
     directory
  */

  //Check and see if the file is already downloaded..
//check the --no-getch flag
  if (rt.dont_prompt != TRUE)
    {
      int target_exists=proz_download_target_exist(download);
      if(target_exists)
	{
	  ret=askUserOverwrite(connection);
	  if(ret=='O')
	    {
	      //just continue, it will anyway be overwritten by the download routines.
	    }
	  if(ret=='A')
	    {
	      status = DL_FATALERR;
	      return;
	    }

	}
    }
  /*Check for a prior download */
  int previous_dl = proz_download_prev_download_exists (download);
  if (previous_dl == 1)
    {

      download->resume_mode = TRUE;
      //connection supports resume
      if (connection->resume_support)
	{
	  //check the --no-getch flag
	  if (rt.dont_prompt == TRUE)
	    {
	      //see if the user had a preference, resume or overwrite
	      if (rt.resume_mode == TRUE)
		download->resume_mode = TRUE;
	      else
		if (rt.force_mode == TRUE)
		  download->resume_mode = FALSE;
	    }
	  else
	    {
	      //ask the user (curses or terminal)
	      ret = askUserResume(connection, true);
	      if (ret == 'R')
		download->resume_mode = TRUE;
	      else
		download->resume_mode = FALSE;
	    }
	}
      else
	{//resume NOT supported
	  // --no-getch and no force-mode means fatal error!!!
	  if (rt.dont_prompt == TRUE && rt.force_mode == FALSE)
	    {
	      status = DL_FATALERR;
	      handle_dl_fatal_error ();
	      return;
	    }
      
	  //force overwrite
	  if (rt.dont_prompt == TRUE && rt.force_mode == TRUE)
	    download->resume_mode = FALSE;
	  else
	    {
	      //Ask the user
	      ret = askUserResume(connection, false);
	      if (ret == 'O')
		download->resume_mode = FALSE;
	      else
		{ //Abort
		  status = DL_FATALERR;
		  handle_dl_fatal_error ();
		  return;
		}
	    }
	}
    }
 }



void
DL_Window::start_download ()
{
	int ret = 0;

	proz_debug("start_download");
	if (using_ftpsearch != TRUE)
		ret = num_connections =
			proz_download_setup_connections_no_ftpsearch
			(download, connection, num_connections);
	else
		ret = proz_download_setup_connections_ftpsearch (download,
								 connection,
								 ftpsearch_win->
								 request,
								 num_connections);

	if (ret == -1)
	{
    PrintMessage("Write Error: There may not be enough free space or a disk write failed when attempting to create output file\n");
		status = DL_ABORTED;
		return;
	}

	/*Display resume status */
	if (download->resume_support)
	{
		PrintMessage("RESUME supported\n\n");
	}
	else
	{
	  PrintMessage("RESUME NOT supported\n");
	}

	gettimeofday (&update_time, NULL);

	proz_download_start_downloads (download, download->resume_mode);
	status = DL_DOWNLOADING;
	return;

}

void
DL_Window::handle_info_thread ()
{
  bool getting_info = proz_connection_running (connection);


  if (getting_info == FALSE)
    {
      pthread_join (info_thread, NULL);

      if (connection->err == HOK || connection->err == FTPOK)
	{
	  got_info = TRUE;

	  if (connection->main_file_size != -1)
	    {
	      PrintMessage("File Size = %lld Kb\n\n",
			   connection->main_file_size / 1024);
	    }
	  else
	    PrintMessage("File Size is UNKOWN\n\n");

	  //Added ftpsearch only is size > min size
	  if ((connection->main_file_size != -1
	       && do_ftpsearch == TRUE) && (connection->main_file_size / 1024 >= rt.min_search_size))
	    {
	      status = DL_FTPSEARCHING;

	      if (rt.ftpsearch_server_id == 0)
		{
		  ftpsearch_win->
		    fetch_mirror_info
		    (&connection->u,
		     connection->main_file_size,
		     "http://www.filesearching.com/cgi-bin/s",
		     FILESEARCH_RU,
		     rt.ftps_mirror_req_n);
		}
	      else if (rt.ftpsearch_server_id == 1)
		{
		  ftpsearch_win->
		    fetch_mirror_info
		    (&connection->u,
		     connection->main_file_size,
		     "http://ftpsearch.elmundo.es:8000/ftpsearch",
		     LYCOS, rt.ftps_mirror_req_n);
		}

	    }
	  else
	    {
	      if (connection->main_file_size / 1024 >= rt.min_search_size  && (do_ftpsearch == TRUE))
		PrintMessage("File size is less than the minimum, skipping ftpsearch");

	      do_download ();
	    }

	}
      else
	{

	  if (connection->err == FTPNSFOD
	      || connection->err == HTTPNSFOD)
	    {
	      PrintMessage("The URL %s doesnt exist!\n",
			   connection->u.url);
	      got_dl = FALSE;
	      got_info = FALSE;
	      status = DL_FATALERR;
	    }
	  else
	    {
	      PrintMessage("An error occurred: %s \n",
			   proz_strerror(connection->err));
	      got_dl = FALSE;
	      got_info = FALSE;
	      status = DL_FATALERR;
	    }

	}
    }
}


void
DL_Window::handle_ftpsearch ()
{

	uerr_t err;
	err = ftpsearch_win->callback ();
	if (err == MASSPINGDONE)
	{
		if (ftpsearch_win->request->num_mirrors == 0)
		{
			using_ftpsearch = FALSE;
			PrintMessage("No suitable mirrors were found, downloading from original server\n");
			do_download ();
			return;
		}
		using_ftpsearch = TRUE;
		do_download ();
		return;
	}
	if (err == FTPSFAIL)
	{
		using_ftpsearch = FALSE;
		do_download ();
		return;
	}

//  if(ftpsearch_win->exit_ftpsearch_button_pressed==TRUE)
	if (using_ftpsearch == TRUE)
	{
		if (ftpsearch_win->got_mirror_info == TRUE)
		{
			using_ftpsearch = TRUE;
		}
		else
		{
			using_ftpsearch = FALSE;
		}
		do_download ();
	}
}

void
DL_Window::handle_download_thread ()
{

  uerr_t err;
  struct timeval cur_time;
  struct timeval diff_time;

  err = proz_download_handle_threads (download);


  gettimeofday (&cur_time, NULL);

  proz_timeval_subtract (&diff_time, &cur_time, &update_time);

  if ((((diff_time.tv_sec * 1000) + (diff_time.tv_usec / 1000)) > 200)
      || err == DLDONE)
    {
 
      print_status (download, rt.quiet_mode);
    
      if (download->main_file_size != -1)
	{
	}

      /*The time of the current screen */
      gettimeofday (&update_time, NULL);
    }

  if (err == DLDONE)
    {
      PrintMessage("Got DL succesfully, now renaming file\n");
      got_dl = TRUE;

      PrintMessage ("Renaming file %s .....\n",
		    download->u.file);
      status = DL_JOINING;
      proz_download_join_downloads (download);
      joining_thread_running = TRUE;
    }

  if (err == CANTRESUME)
    {
      /*We can only use one connections and we cant resume */
      num_connections = 1;
      connection->resume_support = FALSE;
      got_dl = FALSE;
      status = DL_RESTARTING;
    }

  if (err == DLLOCALFATAL)
    {

      PrintMessage ("One connection of the download %s encountered a unrecoverable local error, usually lack of free space, or a write to bad medium, or a problem with permissions,so please fix this and retry\n",
		    connection->u.url);
      got_dl = FALSE;
      status = DL_FATALERR;
    }


  if (err == DLREMOTEFATAL)
    {

      PrintMessage (_
		    ("A connection(s) of the download %s encountered a unrecoverable remote error, usually the file not being present in the remote server, therefore the download had to be aborted!\n"),
		    connection->u.url);
      got_dl = FALSE;
      status = DL_FATALERR;
    }

}


void
DL_Window::handle_joining_thread ()
{

	boolean bDone = false;

	uerr_t building_status = proz_download_get_join_status (download);

	if (building_status == JOINERR)
	{
		if (joining_thread_running == TRUE)
		{
			proz_download_wait_till_end_joining_thread (download);
			joining_thread_running = FALSE;
		}

	}

	if (building_status == JOINDONE)
	{

		if (joining_thread_running == TRUE)
		{
			proz_download_wait_till_end_joining_thread (download);
			joining_thread_running = FALSE;
			bDone = true;
		}

		/*has the user pressed OK at the end of the download */
		if (bDone == true)
		{
      PrintMessage("All Done.\n");
      //curses_query_user_input("Press any key to exit.");
			proz_download_delete_dl_file (download);
			proz_download_free_download (download, 0);
			status = DL_IDLING;
		}
		status = DL_DONE;
	}
}


void
DL_Window::handle_dl_fatal_error ()
{

	status = DL_FATALERR;
	cleanup (FALSE);
	return;
}

void
DL_Window::print_status (download_t * download, int quiet_mode)
{

  if (rt.display_mode == DISP_CURSES)
    DisplayCursesInfo(download);
  else
    {

      if (quiet_mode == FALSE)
	{
	  for (int i = 0; i < download->num_connections; i++)
	    {

	      fprintf (stdout,
		       "%2.2d  %-30.30s  %15.15s  %10Ld\n",
		       i + 1, download->pconnections[i]->u.host,
		       proz_connection_get_status_string (download->
							  pconnections
							  [i]),
		       proz_connection_get_total_bytes_got
		       (download->pconnections[i]));
	    }

	  fprintf (stdout, "Total Bytes received %Ld Kb\n",
		   proz_download_get_total_bytes_got (download) / 1024);


	  fprintf (stdout, "Average Speed = %.3f Kb/sec\n",
		   proz_download_get_average_speed (download) / 1024);
	}
  

      int secs_left;
      char timeLeft[30];

      if ((secs_left =
	   proz_download_get_est_time_left (download)) != -1)
	{
	  if (secs_left < 60)
	    snprintf (timeLeft, sizeof(timeLeft), "00:%.2d", secs_left);
	  else if (secs_left < 3600)
	    snprintf (timeLeft, sizeof(timeLeft), "00:%.2d:%.2d",
		      secs_left / 60, secs_left % 60);
	  else
	    snprintf (timeLeft, sizeof(timeLeft), "%.2d:%.2d:00",
		      secs_left / 3600,
		      (secs_left % 3600) / 60);
	}
      else
	sprintf (timeLeft, "??:??:??");

      off_t totalDownloaded = 0;
      off_t totalFile = 0;
      long aveSpeed = 0;
      totalFile = download->main_file_size / 1024;
      totalDownloaded =
	proz_download_get_total_bytes_got (download) /
	1024;
      aveSpeed = (long)(proz_download_get_average_speed (download) / 1024);

      //WGET looks like this:
      //xx% [=======>    ] nnn,nnn,nnn XXXX.XXK/s ETA hh:mm:ss

      fprintf (stdout, "  %.2lf%% %lldKb/%lldkb %0.3fKb/s ETA %s           \r", 
	       ((float)totalDownloaded) / ((float)totalFile / 100), 
	       totalDownloaded, totalFile, (float)aveSpeed, timeLeft);
      fflush (stdout);
    }
}

int DL_Window::askUserResume(connection_t *connection, boolean resumeSupported)
{
    int ret = 0;
    const char msg[] = "Previous download of %s exists, would you like to (R)esume it or (O)verwrite it?";
    const char msg2[] = "Previous download of %s exists, would you like to (A)bort it or (O)verwrite it?";

    
  do {
      if (rt.display_mode == DISP_CURSES)
         ret = curses_query_user_input((resumeSupported == TRUE)?msg:msg2,connection->u.file);
      else
      {
        fprintf(stdout,"\n");
        fprintf(stdout,(resumeSupported == TRUE)?msg:msg2,connection->u.file);
        ret = getc(stdin);
        ret = islower(ret) ? toupper(ret) : ret;
      } 
      
      switch(ret)
      {
        case 'O':
          break;
        case 'A':
          if (resumeSupported == TRUE)
            ret=0;
          break;
        case 'R':
          if (resumeSupported == FALSE)
            ret=0;
          break;
        default:
          ret=0;
        break;
      }

    } while (ret == 0);
    
    return ret;
  
}


int DL_Window::askUserOverwrite(connection_t *connectionb)
{
    int ret = 0;
    const char msg[] = "File %s already exists, would you like to (A)bort it or (O)verwrite it?";

    
  do {
      if (rt.display_mode == DISP_CURSES)
         ret = curses_query_user_input(msg,connection->u.file);
      else
      {
        fprintf(stdout,"\n");
        fprintf(stdout,msg,connection->u.file);
        ret = getc(stdin);
        ret = islower(ret) ? toupper(ret) : ret;
      } 
      
      switch(ret)
      {
        case 'O':
          break;
        case 'A':
          break;
        default:
          ret=0;
        break;
      }

    } while (ret == 0);
    
    return ret;
}
