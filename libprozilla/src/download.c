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

/* $Id: download.c,v 1.42 2005/09/19 16:18:02 kalum Exp $ */


#include "common.h"
#include "download.h"
#include "ftp-retr.h"
#include "http-retr.h"
#include "logfile.h"
#include "debug.h"
#include "ftpsearch.h"


/******************************************************************************
 Initialize the download.
******************************************************************************/
download_t *proz_download_init(urlinfo * u)
{

/*  pthread_mutexattr_t attr; */
  download_t *download = kmalloc(sizeof(download_t));

  /*  attr.__mutexkind = PTHREAD_MUTEX_RECURSIVE_NP;  */

  memset(download, 0, sizeof(download_t));

  /*FIXME   pthread_mutex_init(&download->status_change_mutex, &attr); */

  pthread_mutex_init(&download->status_change_mutex, NULL);
  pthread_mutex_init(&download->access_mutex, NULL);

  memcpy(&download->u, u, sizeof(urlinfo));

  download->dl_dir = kmalloc(PATH_MAX);
  download->output_dir = kmalloc(PATH_MAX);
  download->log_dir = kmalloc(PATH_MAX);
  strcpy(download->dl_dir, libprozrtinfo.dl_dir);
  strcpy(download->output_dir, libprozrtinfo.output_dir);
  strcpy(download->log_dir, libprozrtinfo.log_dir);
  download->resume_mode = FALSE;
  download->max_simul_connections = 0;

  download->max_allowed_bps = libprozrtinfo.max_bps_per_dl;
  download->file_build_msg = (char *) kmalloc(MAX_MSG_SIZE + 1);
  download->using_ftpsearch = FALSE;
  return download;
}

/******************************************************************************
 This will setup a download based on the connection info, and will attempt 
 to setup req_connections, but it might not be possible  if we dont know the 
 file size, returns the number of connections allocated. 
******************************************************************************/
int proz_download_setup_connections_no_ftpsearch(download_t * download,
						 connection_t * connection,
						 int req_connections)
{
  int num_connections, i;
  off_t bytes_per_connection;
  off_t bytes_left;
  FILE *fp;
  char *out_file;
  struct stat stat_buf;

  download->main_file_size = connection->main_file_size;
  download->resume_support = connection->resume_support;

  if (download->main_file_size == -1)
    {
      num_connections = 1;
      bytes_per_connection = -1;
      bytes_left = -1;

    } else
      {
	if (connection->resume_support == FALSE)
	  num_connections = 1;
	else
	  num_connections = req_connections;

	bytes_per_connection = connection->main_file_size / num_connections;
	bytes_left = connection->main_file_size % num_connections;
      }

  download->pconnections = kmalloc(sizeof(connection_t *) * num_connections);
  download->num_connections = num_connections;

  out_file=kmalloc(PATH_MAX);
  snprintf(out_file, PATH_MAX, "%s/%s.prozilla",
	   download->dl_dir, connection->u.file);

  proz_debug("out file %s",out_file);
	  
  //First see if the file exists then we dont create a new one else we do

  if (stat(out_file, &stat_buf) == -1)
    {
		proz_debug("stat failed");
      /* the call failed */
      /* if the error is due to the file not been present then there is no 
	 need to do anything..just continue, otherwise return error (-1)
      */
      if (errno == ENOENT)
	{
		proz_debug("file doesnt exist");
	  //File not exists so create it
	  if (!
	      (fp =
	       fopen(out_file, "w+")))
	    {
	      download_show_message(download,
				    _
				    ("Unable to open file %s: %s!"),
				    out_file, strerror(errno));
			proz_debug("Unable to open file %s: %s!",
				    out_file, strerror(errno));
			return -1;
	    }
		proz_debug("created file");

	}
      else
	  {
		  proz_debug("something else happened %d", errno);
		return -1;
	  }
    }
  else
    {
		proz_debug("stat success");
      //TODO: File exists :  if it doesnt match file size warna boput it...
      if (!
	  (fp =
	   fopen(out_file, "r+")))
	{
	  download_show_message(download,
				_
				("Unable to open file %s: %s!"),
				out_file, strerror(errno));
	  proz_debug("Unable to open file %s: %s!",
				out_file, strerror(errno));
	  return -1;
	}

    }


  //TRY setting the offset;
  if (download->main_file_size != -1)
    {

      if(fseeko(fp, download->main_file_size, SEEK_SET)!=0)
	  {
		  proz_debug("fseek failed");
		return -1;
	  }
    }

  /*Make sure all writes go directly to the file */
   setvbuf(fp, NULL, _IONBF, 0);

  for (i = 0; i < num_connections; i++)
    {
      download->pconnections[i]=proz_connection_init(&download->u,
						     &download->status_change_mutex);

      /*Copy somethings we need from the original connection */
      download->resume_support = download->pconnections[i]->resume_support =
	connection->resume_support;
      memcpy(&download->pconnections[i]->hs, &connection->hs,
	     sizeof(http_stat_t));


      download->pconnections[i]->localfile = kmalloc(PATH_MAX);
      strcpy(out_file, download->pconnections[i]->localfile);

    
      download->pconnections[i]->fp=fp;

      download->pconnections[i]->retry = TRUE;

      if (connection->main_file_size == -1)
	{
	  download->pconnections[i]->main_file_size = -1;
	  download->pconnections[i]->remote_startpos = 0;
	  download->pconnections[i]->orig_remote_startpos = 0;
	  download->pconnections[i]->remote_endpos = -1;


	  download->pconnections[i]->local_startpos = 0;
	} else
	  {
	    download->pconnections[i]->main_file_size = connection->main_file_size;
	    download->pconnections[i]->orig_remote_startpos = download->pconnections[i]->remote_startpos = i * bytes_per_connection;
	    download->pconnections[i]->remote_endpos =
	      i * bytes_per_connection + bytes_per_connection;

	    //Changing things here.....
	    download->pconnections[i]->local_startpos =    download->pconnections[i]->remote_startpos;
	  }



      /*Set the connections message to be download->msg_proc calback */
      proz_connection_set_msg_proc(download->pconnections[i],
				   download->msg_proc, download->cb_data);
    }

  /* Add the remaining bytes to the last connection   */

  download->pconnections[--i]->remote_endpos += bytes_left;
  download->using_ftpsearch = FALSE;


  /*NOTE:  Should we check for previously started downloads here and adjust 
    the local_startpos, accordingly or check for resumes later in another function 
    which is called after this?
  */

	proz_debug("return num_connections %d",num_connections);
  return num_connections;
}


/* This will for each connection setup the local_startpos and file write mode if any prior download exists returns 1 on success, and -1 on a error.*/
int proz_download_load_resume_info(download_t * download)
{
  int i;
  int ret = 1;
  logfile lf;
  if(proz_log_read_logfile(&lf, download, TRUE)==1)
     proz_debug("sucessfully loaded resume info");


  for (i = 0; i < download->num_connections; i++)
  {

    if(download->pconnections[i]->remote_endpos - download->pconnections[i]->remote_startpos == download->pconnections[i]->remote_bytes_received)
      {

	connection_change_status(download->pconnections[i], COMPLETED);
	//This should fix the error we received when resuming when the
	//average rate was too high.
	download->pconnections[i]->remote_startpos +=download->pconnections[i]->remote_bytes_received;
	continue;
      }

            download->pconnections[i]->remote_startpos +=download->pconnections[i]->remote_bytes_received;


  }

  download->resume_mode = TRUE;
  return ret;
}

/* This will create the threads and start the downloads, 
   if resume is true it will load the resume info too if the download supports it
*/

void proz_download_start_downloads(download_t * download,
				   boolean resume_mode)
{

  int i;
 
  if (resume_mode)
  {
    /*Does this download suport resume? */
    if (download->resume_support == TRUE)
      proz_download_load_resume_info(download);
  }
else
  {
 /*Create the log file */
  if (log_create_logfile
      (download->num_connections, download->main_file_size,
       download->u.url, download) != 1)
  {
    download_show_message(download,
			  _("Warning! Unable to create logfile!"));
  }
}

  /* Allocate number of threads */
  download->threads =
      (pthread_t *) kmalloc(sizeof(pthread_t) * download->num_connections);

  /*Create them */
  for (i = 0; i < download->num_connections; i++)
  {
    switch (download->pconnections[i]->u.proto)
    {
    case URLHTTP:
      /*   http_loop(&download->connections[i]); */
      if (pthread_create(&download->threads[i], NULL,
			 (void *) &http_loop,
			 (void *) (download->pconnections[i])) != 0)

	proz_die(_("Error: Not enough system resources"));

      break;
    case URLFTP:

      /*  ftp_loop(&download->connections[i]);  */


      if (pthread_create(&download->threads[i], NULL,
			 (void *) &ftp_loop,
			 (void *) (download->pconnections[i])) != 0)
	proz_die(_("Error: Not enough system resources"));

      break;
    default:
      proz_die(_("Error: Unsupported Protocol was specified"));
    }
  }

  download_show_message(download, _("All threads created"));
}

void proz_download_stop_downloads(download_t * download)
{
  int i;
  /*Stop the threads */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_cancel(download->threads[i]);
    pthread_join(download->threads[i], NULL);
  }
}

/* returns  one of  DLINPROGRESS, DLERR, DLDONE, DLREMOTEFATAL, DLLOCALFATAL*/
uerr_t proz_download_handle_threads(download_t * download)
{


  //Create logfile everytime this is callaed
  log_create_logfile(download->num_connections, download->main_file_size,download->u.url, download);

  if (download->using_ftpsearch == TRUE)
    return download_handle_threads_ftpsearch(download);
  else
    return download_handle_threads_no_ftpsearch(download);
}

/* returns  one of  DLINPROGRESS, DLERR, DLDONE, DLREMOTEFATAL, DLLOCALFATAL*/
uerr_t download_handle_threads_no_ftpsearch(download_t * download)
{
  int i;

  for (i = 0; i < download->num_connections; i++)
    {
      /*Set the DL start time if it is not done so */
      pthread_mutex_lock(download->pconnections[i]->status_change_mutex);
      if (download->pconnections[i]->status == DOWNLOADING
	  && download->start_time.tv_sec == 0
	  && download->start_time.tv_usec == 0)
	{
	  gettimeofday(&download->start_time, NULL);
	}
      pthread_mutex_unlock(download->pconnections[i]->status_change_mutex);
    }


  /*If all the connections are completed then end them, and return complete */
  if ((proz_download_all_dls_status(download, COMPLETED)) == TRUE)
    {
      char * out_filename;
      char * orig_filename;
      download_show_message(download,
			    "All the conenctions have retreived the file"
			    "..waiting for them to end");
      proz_download_wait_till_all_end(download);
      download_show_message(download, "All the threads have being ended.");

      /*Close and rename file to original */
      //      flockfile(download->pconnections[0]->fp);
      fclose(download->pconnections[0]->fp);
      //  funlockfile(download->pconnections[0]->fp);

      out_filename=kmalloc(PATH_MAX);
      orig_filename=kmalloc(PATH_MAX);

      snprintf(orig_filename, PATH_MAX, "%s/%s",
	       download->dl_dir, download->pconnections[0]->u.file);
      snprintf(out_filename, PATH_MAX, "%s/%s.prozilla",
	       download->dl_dir, download->pconnections[0]->u.file);
      if(rename(out_filename, orig_filename)==-1)
	{
	  download_show_message(download, "Error While attempting to rename the file: %s", strerror(errno));
	}

      download_show_message(download, "Successfully renamed file");
      /*Delete the logfile as we dont need it now */
      if(proz_log_delete_logfile(download)!=1)
	download_show_message(download, "Error: Unable to delete the logfile: %s", strerror(errno));

      return DLDONE;
    }

  /*TODO handle restartable connections */
  for (i = 0; i < download->num_connections; i++)
    {
      dl_status status;
      uerr_t connection_err;

      pthread_mutex_lock(download->pconnections[i]->status_change_mutex);
      status = download->pconnections[i]->status;
      pthread_mutex_unlock(download->pconnections[i]->status_change_mutex);

      pthread_mutex_lock(&download->pconnections[i]->access_mutex);
      connection_err = download->pconnections[i]->err;
      pthread_mutex_unlock(&download->pconnections[i]->access_mutex);


      switch (status)
	{
	case MAXTRYS:
	  break;

	case REMOTEFATAL:
	  /* handle the CANTRESUME err code */

	  if (connection_err == CANTRESUME)
	    {
	      /*Terminate the connections */
	      proz_download_stop_downloads(download);
	      /*FIXME Do we delete any downloaded portions here ? */
	      return CANTRESUME;
	    } else /*Handle the file not being found on the server */
	      if (connection_err == FTPNSFOD || connection_err == HTTPNSFOD)
		{
		  if (proz_download_all_dls_filensfod(download) == TRUE)
		    {
		      download_show_message(download,
					    _
					    ("The file was not found in all the connections!"));
		      /*Terminate the connections */
		      proz_download_stop_downloads(download);
		      return DLREMOTEFATAL;
		    } else
		      {
			download_show_message(download, _("Relaunching download"));
			/* Make sure this thread has terminated */
			pthread_join(download->threads[i], NULL);

			pthread_mutex_lock(&download->status_change_mutex);
			if (pthread_create
			    (&download->threads[i], NULL, (void *) &ftp_loop,
			     (void *) (download->pconnections[i])) != 0)
			  proz_die(_("Error: Not enough system resources"));
			pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
			pthread_mutex_unlock(&download->status_change_mutex);
		      }
		} else /*Handle the file not being found on the server */
		  if (connection_err == FTPCWDFAIL)
		    {
		      if (proz_download_all_dls_ftpcwdfail(download) == TRUE)
			{
			  download_show_message(download,
						_
						("Failed to change to the working directory on all the connections!"));
			  /*Terminate the connections */
			  proz_download_stop_downloads(download);
			  return DLREMOTEFATAL;
			} else
			  {
			    download_show_message(download, _("Relaunching download"));
			    /* Make sure this thread has terminated */
			    pthread_join(download->threads[i], NULL);

			    pthread_mutex_lock(&download->status_change_mutex);
			    if (pthread_create
				(&download->threads[i], NULL, (void *) &ftp_loop,
				 (void *) (download->pconnections[i])) != 0)
			      proz_die(_("Error: Not enough system resources"));
			    pthread_cond_wait(&download->pconnections[i]->connecting_cond, &download->status_change_mutex);
			    pthread_mutex_unlock(&download->status_change_mutex);
			  }
		    }

	  break;

	case LOCALFATAL:
	  proz_download_stop_downloads(download);
	  download_show_message(download,
				_
				("Connection %d, had a local fatal error: %s .Aborting download. "),
				i,
				proz_strerror(download->pconnections[i]->err));
	  return DLLOCALFATAL;
	  break;

	case LOGINFAIL:
	  /*
	   * First check if the ftp server did not allow any thread 
	   * to login at all, then  retry the curent thread 
	   */
	  if (proz_download_all_dls_status(download, LOGINFAIL) == TRUE)
	    {
	      download_show_message(download,
				    _
				    ("All logins rejected! Retrying connection"));
	      /* Make sure this thread has terminated */
	      pthread_join(download->threads[i], NULL);

	      pthread_mutex_lock(&download->status_change_mutex);
	      if (pthread_create
		  (&download->threads[i], NULL, (void *) &ftp_loop,
		   (void *) (download->pconnections[i])) != 0)
		proz_die(_("Error: Not enough system resources"));
	      pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
	      pthread_mutex_unlock(&download->status_change_mutex);
	      break;
	    } else
	      {

		/*
		 * Ok so at least there is one download whos login has not been rejected, 
		 * so lets see if it has completed, if so we can relaunch this connection, 
		 * as the commonest reason for a ftp login being rejected is because, the 
		 * ftp server has a limit on the number of logins permitted from the same 
		 * IP address. 
		 */

		/*
		 * Query the number of threads that are downloading 
		 * if it is zero then relaunch this connection
		 */
		int dling_conns_count =
		  download_query_conns_status_count(download, DOWNLOADING, NULL);

		if (dling_conns_count > download->max_simul_connections)
		  {
		    download->max_simul_connections = dling_conns_count;
		    break;
		  }


		if (dling_conns_count == 0
		    &&
		    (download_query_conns_status_count(download, CONNECTING, NULL)
		     == 0)
		    && (download_query_conns_status_count(download, LOGGININ, NULL)
			== 0))
		  {

		    /* Make sure this thread has terminated */
		    pthread_join(download->threads[i], NULL);
		    pthread_mutex_lock(&download->status_change_mutex);
		    download_show_message(download, _("Relaunching download"));
		    if (pthread_create(&download->threads[i], NULL,
				       (void *) &ftp_loop,
				       (void *) (download->pconnections[i])) != 0)
		      proz_die(_("Error: Not enough system resources"));
		    pthread_cond_wait(&download->pconnections[i]->connecting_cond,
				      &download->status_change_mutex);
		    pthread_mutex_unlock(&download->status_change_mutex);
		  } else
		    if (dling_conns_count < download->max_simul_connections
			&&
			(download_query_conns_status_count
			 (download, CONNECTING, NULL) == 0)
			&&
			(download_query_conns_status_count
			 (download, LOGGININ, NULL) == 0))
		      {

			/* Make sure this thread has terminated */
			pthread_join(download->threads[i], NULL);
			pthread_mutex_lock(&download->status_change_mutex);
			download_show_message(download, _("Relaunching download"));
			if (pthread_create(&download->threads[i], NULL,
					   (void *) &ftp_loop,
					   (void *) (download->pconnections[i])) != 0)
			  proz_die(_("Error: Not enough system resources"));
			pthread_cond_wait(&download->pconnections[i]->connecting_cond,
					  &download->status_change_mutex);
			pthread_mutex_unlock(&download->status_change_mutex);
		      }

	      }
	  break;

	case CONREJECT:
	  /*
	   * First check if the ftp server did not allow any thread 
	   * to login at all, then  retry the curent thread 
	   */
	  if (proz_download_all_dls_status(download, CONREJECT) == TRUE)
	    {
	      download_show_message(download,
				    _
				    ("All connections attempts have been  rejected! Retrying connection"));
	      /* Make sure this thread has terminated */
	      pthread_join(download->threads[i], NULL);

	      pthread_mutex_lock(&download->status_change_mutex);
	      if (pthread_create
		  (&download->threads[i], NULL, (void *) &ftp_loop,
		   (void *) (download->pconnections[i])) != 0)
		proz_die(_("Error: Not enough system resources"));

	      pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
	      pthread_mutex_unlock(&download->status_change_mutex);
	      break;
	    } else
	      {
		/*
		 * Ok so at least there is one download whos connections attempt has not been rejected, 
		 * so lets see if it has completed, if so we can relaunch this connection, 
		 * as the commonest reason for a ftp login being rejected is because, the 
		 * ftp server has a limit on the number of logins permitted from the same 
		 * IP address. 
		 */

		/*
		 * Query the number of threads that are downloading 
		 * if it is zero then relaunch this connection
		 */
		int dling_conns_count =
		  download_query_conns_status_count(download, DOWNLOADING, NULL);

		if (dling_conns_count > download->max_simul_connections)
		  {
		    download->max_simul_connections = dling_conns_count;
		    break;
		  }


		if (dling_conns_count == 0
		    &&
		    (download_query_conns_status_count(download, CONNECTING, NULL)
		     == 0)
		    && (download_query_conns_status_count(download, LOGGININ, NULL)
			== 0))
		  {

		    /* Make sure this thread has terminated */
		    pthread_join(download->threads[i], NULL);
		    pthread_mutex_lock(&download->status_change_mutex);
		    download_show_message(download, _("Relaunching download"));
		    if (pthread_create(&download->threads[i], NULL,
				       (void *) &ftp_loop,
				       (void *) (download->pconnections[i])) != 0)
		      proz_die(_("Error: Not enough system resources"));
		    pthread_cond_wait(&download->pconnections[i]->connecting_cond,
				      &download->status_change_mutex);
		    pthread_mutex_unlock(&download->status_change_mutex);
		  } else
		    if (dling_conns_count < download->max_simul_connections
			&&
			(download_query_conns_status_count
			 (download, CONNECTING, NULL) == 0)
			&&
			(download_query_conns_status_count
			 (download, LOGGININ, NULL) == 0))
		      {

			/* Make sure this thread has terminated */
			pthread_join(download->threads[i], NULL);
			pthread_mutex_lock(&download->status_change_mutex);
			download_show_message(download, _("Relaunching download"));
			if (pthread_create(&download->threads[i], NULL,
					   (void *) &ftp_loop,
					   (void *) (download->pconnections[i])) != 0)
			  proz_die(_("Error: Not enough system resources"));
			pthread_cond_wait(&download->pconnections[i]->connecting_cond,
					  &download->status_change_mutex);
			pthread_mutex_unlock(&download->status_change_mutex);
		      }

	      }
	  break;

	default:
	  break;
	}
    }

  /*bandwith throttling */
  download_calc_throttle_factor(download);
  return DLINPROGRESS;
}

pthread_mutex_t download_msg_mutex = PTHREAD_MUTEX_INITIALIZER;

/*calls the msg_proc function if not null */
void download_show_message(download_t * download, const char *format, ...)
{
  va_list args;
  char message[MAX_MSG_SIZE + 1];

  pthread_mutex_lock(&download_msg_mutex);
  va_start(args, format);
  vsnprintf(message, MAX_MSG_SIZE, format, args);
  va_end(args);
  if (download->msg_proc)
    download->msg_proc(message, download->cb_data);

  /*FIXME: Remove this later */
//  printf("%s\n", message);
  pthread_mutex_unlock(&download_msg_mutex);
}



/*This will return a pointer to the connection requested. */

connection_t *proz_download_get_connection(download_t * download,
					   int number)
{
  assert(number >= 0 && number < download->num_connections);
  return (download->pconnections[number]);

}



/*Returns the total number of bytes got.*/
off_t proz_download_get_total_bytes_got(download_t * download)
{
  off_t total_bytes_got = 0;
  int i;


  for (i = 0; i < download->num_connections; i++)
  {
    total_bytes_got +=
	proz_connection_get_total_bytes_got(download->pconnections[i]);
  }
  return total_bytes_got;
}


/*Returns 1 if a previous download exits, 0 if not, and -1 on error */
int proz_download_prev_download_exists(download_t * download)
{
  /* Currently if a logfile exists it assumes that a previous uncompleted 
     download exists
   */

  return proz_log_logfile_exists(download);
}


/*Returns the download speed in bytes per second */
float proz_download_get_average_speed(download_t * download)
{
  float speed;
  struct timeval cur_time;
  struct timeval diff_time;
  off_t total_remote_bytes_got =
      proz_download_get_total_remote_bytes_got(download);

  /*Has the download has been started.... */
  if (download->start_time.tv_sec > 0 || download->start_time.tv_usec > 0)
  {
    gettimeofday(&cur_time, NULL);
    /*Get the diff. */
    proz_timeval_subtract(&diff_time, &cur_time, &download->start_time);

    if (diff_time.tv_sec >= 1)// || diff_time.tv_usec > 0)
    {
      speed = (float) total_remote_bytes_got / ((float) diff_time.tv_sec +
						((float) diff_time.
						 tv_usec / 10e5));
    } else
      speed = 0;
  } else
    speed = 0;			/*The DL hasnt started yet */

  return speed;

}


/* This can be called to erase the portions of the main file 
   that have been got.
*/
int proz_download_delete_dl_file(download_t * download)
{
  char *out_file;
  out_file=kmalloc(PATH_MAX);
  snprintf(out_file, PATH_MAX, "%s/%s.prozilla",
	   download->dl_dir, download->u.file);

  if (unlink(out_file) == -1)
    {
      /*
       * if the file is not present no need for a error message ;-) 
       */
      if (errno == ENOENT)
	{
	  return 1;
	}
      else
	{
	  download_show_message(download,
				_
				("unable to delete the file %s. Reason-: %s"),
				out_file, strerror(errno));
	  return -1;
	}
    }

 
  return 1;
}


/*This will wait till all the downloaded threads are not running */
void proz_download_wait_till_all_end(download_t * download)
{
  int i;
  /*Wait till the end of all  the threads */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_join(download->threads[i], NULL);
  }
}


/*Creates the joining thread */
void proz_download_join_downloads(download_t * download)
{
  download->building = TRUE;
     pthread_create(&download->join_thread, NULL,
		   (void *(*)(void *)) download_join_downloads,
		   (void *) download);
}


/*This function will call download_join_downloads with a handler to join the downloaded files*/
void download_join_downloads(download_t * download)
{
  //  pthread_cleanup_push(cleanup_joining_thread, (void *) download);
  join_downloads(download);
  //  pthread_cleanup_pop(0);
}

/*This function will join the downloaded files*/
void join_downloads(download_t * download)
{

 pthread_mutex_lock(&download->access_mutex);
  download->building = 0;
  pthread_mutex_unlock(&download->access_mutex);
  return;
}



/***************************************************************************** 
Returns the total number of bytes that has being got from the server 
by the all the connections managed by this download
******************************************************************************/
off_t proz_download_get_total_remote_bytes_got(download_t * download)
{

  off_t total_bytes_recv = 0;
  int i;

  for (i = 0; i < download->num_connections; i++)
  {
    proz_debug("DOWNLOAD_TOTAL_BYTES_RECV=%lld for connection %d", total_bytes_recv, i); 
    total_bytes_recv +=
	proz_connection_get_total_remote_bytes_got(download->
						   pconnections[i]);
  }
  return total_bytes_recv;
}



/*If all the downlaods status is equal to status  ,returns TRUE */
int proz_download_all_dls_status(download_t * download, dl_status status)
{
  int i;
  pthread_mutex_lock(&download->status_change_mutex);
  for (i = 0; i < download->num_connections; i++)
  {
    if (download->pconnections[i]->status != status)
    {
      pthread_mutex_unlock(&download->status_change_mutex);
      return FALSE;
    }
  }

  pthread_mutex_unlock(&download->status_change_mutex);
  return TRUE;
}




boolean proz_download_all_dls_filensfod(download_t * download)
{
  int i;
  uerr_t err;
  /*Lock mutex */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_mutex_lock(&download->pconnections[i]->access_mutex);
    err = download->pconnections[i]->err;
    pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
    if (err != FTPNSFOD && err != HTTPNSFOD)
    {
      return FALSE;
    }
  }


  return TRUE;
}


boolean proz_download_all_dls_ftpcwdfail(download_t * download)
{
  int i;
  uerr_t err;
  /*Lock mutex */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_mutex_lock(&download->pconnections[i]->access_mutex);
    err = download->pconnections[i]->err;
    pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
    if (err != FTPNSFOD && err != FTPCWDFAIL)
    {
      return FALSE;
    }
  }


  return TRUE;
}


/*If all the downlaods conections err status is equal to in_err (ie all encountered the same error) ,returns TRUE */

boolean proz_download_all_dls_err(download_t * download, uerr_t in_err)
{
 int i;
  uerr_t err;
  /*Lock mutex */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_mutex_lock(&download->pconnections[i]->access_mutex);
    err = download->pconnections[i]->err;
    pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
    if (err !=in_err)
    {
      return FALSE;
    }
  }

  return TRUE;
}

/* Returns the number of connections whose status is status ie (connecting to the server specified),
 if server is NULL then it returns the total number of connections that are having the status which is equal the to the status specified */
int download_query_conns_status_count(download_t * download,
				      dl_status status, char *server)
{
  int i;
  int count = 0;
  pthread_mutex_lock(&download->status_change_mutex);

  for (i = 0; i < download->num_connections; i++)
  {
    if (download->pconnections[i]->status == status)
    {
      if (server == NULL
	  || (strcasecmp(server, download->pconnections[i]->u.host) == 0))
	count++;
    }
  }

  pthread_mutex_unlock(&download->status_change_mutex);
  return count;
}




void proz_download_set_msg_proc(download_t * download,
				message_proc msg_proc, void *cb_data)
{
  assert(download != NULL);


  download->msg_proc = msg_proc;
  download->cb_data = cb_data;
}

/*Returns the number of seconds left remaining in the download,
if it cannot be calculated say if the file size of not known, it returns -1
*/
off_t proz_download_get_est_time_left(download_t * download)
{

  long secs_left;
  float average_speed;
  off_t total_bytes_got;

  if (download->main_file_size == -1)
    return -1;

  total_bytes_got = proz_download_get_total_bytes_got(download);

  average_speed = proz_download_get_average_speed(download);
  if (average_speed == 0)
    return -1;

  return secs_left =
      (off_t) ((download->main_file_size -
	       total_bytes_got) / average_speed);

}


void proz_download_free_download(download_t * download, boolean complete)
{

  assert(download);
  /*TODO free the URL */

  if (download->dl_dir)
    kfree(download->dl_dir);
  if (download->output_dir)
    kfree(download->output_dir);

  if (download->log_dir)
    kfree(download->log_dir);

  if (download->file_build_msg)
    kfree(download->file_build_msg);
  if (download->threads)
    kfree(download->threads);

  /*Now handle the freeing of the connections */

  if (download->num_connections > 0 && download->pconnections)
  {
    int i;
    for (i = 0; i < download->num_connections; i++)
    {
      proz_connection_free_connection(download->pconnections[i], 0);
    }
    kfree(download->pconnections);
  }

  if (complete == TRUE)
    kfree(download);
}

void download_calc_throttle_factor(download_t * download)
{
  int i;
  int num_slow_cons = 0;
  long t_slow_rates = 0;
  long limit_high_cons_rate;
  long avg_rate;

  int num_dl_cons =
      download_query_conns_status_count(download, DOWNLOADING, NULL);

  if (num_dl_cons == 0)
    return;

  avg_rate = download->max_allowed_bps / num_dl_cons;

  if (download->max_allowed_bps == 0)
  {
    for (i = 0; i < download->num_connections; i++)
    {
      pthread_mutex_lock(&(download->pconnections[i]->access_mutex));
      download->pconnections[i]->max_allowed_bps = 0;
      pthread_mutex_unlock(&(download->pconnections[i]->access_mutex));
    }
    return;
  }

  /*MAKE IR USE THE NUMBER OF ACTIVE DOWNLOAdING CONENCTIONS: Done */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_mutex_lock(&(download->pconnections[i]->access_mutex));
    if ((proz_connection_get_status(download->pconnections[i]) ==
	 DOWNLOADING) && download->pconnections[i]->rate_bps < avg_rate)
    {
      t_slow_rates += download->pconnections[i]->rate_bps;
      num_slow_cons++;
    }
    pthread_mutex_unlock(&(download->pconnections[i]->access_mutex));
  }


  /*fixme mutex to preven this conenctions */
  if (num_slow_cons > num_dl_cons)
    num_dl_cons = num_slow_cons;


  /*If all the connections are slower then no need to do anything */

  if (num_slow_cons == num_dl_cons)
  {
    for (i = 0; i < download->num_connections; i++)
    {
      pthread_mutex_lock(&(download->pconnections[i]->access_mutex));
      download->pconnections[i]->max_allowed_bps = 0;
      pthread_mutex_unlock(&(download->pconnections[i]->access_mutex));
    }
    return;
  }


  limit_high_cons_rate =
      (download->max_allowed_bps - t_slow_rates) / (num_dl_cons -
						    num_slow_cons);

  /*
     proz_debug("total slow connections = %ld", num_slow_cons);
     proz_debug("total slow rates = %ld", t_slow_rates);
     proz_debug("limit_high_cons_rate = %ld", limit_high_cons_rate);
   */

  for (i = 0; i < download->num_connections; i++)
  {
    pthread_mutex_lock(&(download->pconnections[i]->access_mutex));
    if ((proz_connection_get_status(download->pconnections[i]) ==
	 DOWNLOADING) && download->pconnections[i]->rate_bps >= avg_rate)
    {
      download->pconnections[i]->max_allowed_bps = limit_high_cons_rate;
    }
    pthread_mutex_unlock(&(download->pconnections[i]->access_mutex));
  }
}


/*This function will check if the target output file, 
ie: the file that we are going to rebuild to exists. 
Returns,
 1= file exists
 0 = file does not exists
 -1 = error, ie cant have permissions to stat the file etc etc
*/

int proz_download_target_exist(download_t * download)
{

  char out_file_name[PATH_MAX];
  struct stat st_buf;
  int ret;

  snprintf(out_file_name, PATH_MAX, "%s/%s", download->output_dir,
	   download->u.file);

  ret = stat(out_file_name, &st_buf);
  if (ret == -1)
  {
    if (errno == ENOENT)
      return 0;
    else
      return -1;
  }

  /*File was statable so it exists */
  return 1;

}


/*This function will delete the target output file, 
Returns,
 1= sucessfully delted the file
 0 = file does not exist
 -1 = error, ie cant have permissions to delete the file etc etc
*/

int proz_download_delete_target(download_t * download)
{

  char out_file_name[PATH_MAX];
  int ret;

  snprintf(out_file_name, PATH_MAX, "%s/%s", download->output_dir,
	   download->u.file);

  ret = remove(out_file_name);
  if (ret == -1)
  {
    if (errno == ENOENT)
      return 0;
    else
      return -1;
  }

  /*File was statable so it exists */
  return 1;

}

/*Tries to switch to another server which is downloading or has completed, returns 1 on sucesss, or -1 on failure
 */
int download_switch_server_ftpsearch(download_t *download, int bad_connection)
{
	    int j, usable_server;

	    usable_server = -1;
	    /*Search for a server which is downloading or completed and try to switch to it
*/

	    pthread_mutex_lock(&download->status_change_mutex);
	    for (j = 0; j < download->num_connections; j++)
	    {
	      if (download->pconnections[j]->status == DOWNLOADING ||
		  download->pconnections[j]->status == COMPLETED)
	      {
		usable_server = j;
	      }
	    }
	    pthread_mutex_unlock(&download->status_change_mutex);

	    if (usable_server != -1)
	    {
	      /*We have a server which is DLING */

	      /* Make sure this thread has terminated */
	      pthread_join(download->threads[bad_connection], NULL);

	      /*copy url and relaunch */
	      proz_free_url(&download->pconnections[bad_connection]->u, 0);
	      memcpy(&download->pconnections[bad_connection]->u,
		     proz_copy_url(&download->
				   pconnections[usable_server]->u),
		     sizeof(urlinfo));
	      proz_debug
		  ("Found server %s which is downloading will relaunch based on it",
		   download->pconnections[usable_server]->u.host);

	      pthread_mutex_lock(&download->status_change_mutex);
	      /*Relaunch thread */
	      if (pthread_create
		  (&download->threads[bad_connection], NULL, (void *) &ftp_loop,
		   (void *) (download->pconnections[bad_connection])) != 0)
		proz_die(_("Error: Not enough system resources"));
			pthread_cond_wait(&download->pconnections[bad_connection]->connecting_cond, &download->status_change_mutex);
	 			pthread_mutex_unlock(&download->status_change_mutex);
	      return 1;

	    } else
	    {
	      /*Shit! no servers are downloading or completed so what shall we do? we shall do nothing and wait for one connectection at least to start downloading */
	      return -1;
	    }
 
}  

/* returns  one of  DLINPROGRESS, DLERR, DLDONE, DLREMOTEFATAL, DLLOCALFATAL*/
uerr_t download_handle_threads_ftpsearch(download_t * download)
{
  int i;

  for (i = 0; i < download->num_connections; i++)
  {
    /*Set the DL start time if it is not done so */
    pthread_mutex_lock(download->pconnections[i]->status_change_mutex);
    if (download->pconnections[i]->status == DOWNLOADING
	&& download->start_time.tv_sec == 0
	&& download->start_time.tv_usec == 0)
    {
      gettimeofday(&download->start_time, NULL);
    }
    pthread_mutex_unlock(download->pconnections[i]->status_change_mutex);
  }


  /*If all the connections are completed then end them, and return complete */
  if ((proz_download_all_dls_status(download, COMPLETED)) == TRUE)
  {
      char * out_filename;
      char * orig_filename;

    download_show_message(download,
			  "All the conenctions have retreived the file"
			  "..waiting for them to end");
    proz_download_wait_till_all_end(download);
    download_show_message(download, "All the threads have being ended.");
      /*Close and rename file to original */
      flockfile(download->pconnections[0]->fp);
      fclose(download->pconnections[0]->fp);
      funlockfile(download->pconnections[0]->fp);

      out_filename=kmalloc(PATH_MAX);
      orig_filename=kmalloc(PATH_MAX);

      snprintf(orig_filename, PATH_MAX, "%s/%s",
	       download->dl_dir, download->pconnections[0]->u.file);
      snprintf(out_filename, PATH_MAX, "%s/%s.prozilla",
	       download->dl_dir, download->pconnections[0]->u.file);
      if(rename(out_filename, orig_filename)==-1)
	{
	  download_show_message(download, "Error While attempting to rename the file: %s", strerror(errno));
	}

      download_show_message(download, "Successfully renamed file");
      /*Delete the logfile as we dont need it now */
      if(proz_log_delete_logfile(download)!=1)
	download_show_message(download, "Error: Unable to delete the logfile: %s", strerror(errno));
    return DLDONE;
  }

  /*TODO handle restartable connections */
  for (i = 0; i < download->num_connections; i++)
  {
    dl_status status;
    uerr_t connection_err;
    pthread_mutex_lock(download->pconnections[i]->status_change_mutex);
    status = download->pconnections[i]->status;
    pthread_mutex_unlock(download->pconnections[i]->status_change_mutex);

    pthread_mutex_lock(&download->pconnections[i]->access_mutex);
    connection_err = download->pconnections[i]->err;
    pthread_mutex_unlock(&download->pconnections[i]->access_mutex);


    switch (status)
    {
    case MAXTRYS:
      break;

    case REMOTEFATAL:
      /* handle the CANTRESUME err code */

      if (connection_err == CANTRESUME)
      {
	/*Terminate the connections */
	proz_download_stop_downloads(download);
	/*FIXME Do we delete any downloaded portions here ? */
	return CANTRESUME;
      } else /*Handle the file not being found on the server */
	if (connection_err == FTPNSFOD || connection_err == HTTPNSFOD)
      {
	if (proz_download_all_dls_filensfod(download) == TRUE)
	{
	  download_show_message(download,
				_
				("The file was not found in all the connections!"));
	  /*Terminate the connections */
	  proz_download_stop_downloads(download);
	  return DLREMOTEFATAL;
	} else
	{

	  /*Now we have to be careful */
	  int server_pos, cur_path_pos;

	  server_pos =
	      ftpsearch_get_server_position(download->ftps_info,
					    download->pconnections[i]->u.
					    host);
	  cur_path_pos =
	      ftpsearch_get_path_position(download->ftps_info,
					  download->pconnections[i]->u.host,
					  download->pconnections[i]->u.dir);
	  assert(cur_path_pos != -1);

	  proz_debug("Server pos = %d, cur_path_pos=%d", server_pos,
		     cur_path_pos);
	  /*mark path as not valid */
	  download->ftps_info->mirrors[server_pos].paths[cur_path_pos].
	      valid = FALSE;

	  /*See if more paths are avail */ ;
	  if (cur_path_pos <
	      (download->ftps_info->mirrors[server_pos].num_paths - 1))
	  {
	    char *url_buf;

	    /* Make sure this thread has terminated */
	    pthread_join(download->threads[i], NULL);
	    /*additional paths avail to try */

	    download_show_message(download,
				  _
				  ("Trying additional paths available on this server"));
	    proz_debug("Trying path %s",
		       download->ftps_info->mirrors[server_pos].
		       paths[cur_path_pos + 1]);

	    url_buf =
		malloc(strlen
		       (download->ftps_info->mirrors[server_pos].
			server_name) +
		       strlen(download->ftps_info->mirrors[server_pos].
			      paths[cur_path_pos + 1].path) +
		       strlen(download->pconnections[i]->u.file) + 11 + 1);

	    sprintf(url_buf, "ftp://%s/%s/%s",
		    download->ftps_info->mirrors[server_pos].server_name,
		    download->ftps_info->mirrors[server_pos].
		    paths[cur_path_pos + 1].path,
		    download->pconnections[i]->u.file);

	    proz_debug("Target url for relaunching is %s", url_buf);
	    /*FIXME */
	    proz_parse_url(url_buf, &download->pconnections[i]->u, 0);
	    free(url_buf);

	    /* Make sure this thread has terminated */
	    pthread_join(download->threads[i], NULL);
	    pthread_mutex_lock(&download->status_change_mutex);
			download_show_message(download, _("Relaunching download"));
	    /*Relaunch thread */
	    if (pthread_create
		(&download->threads[i], NULL, (void *) &ftp_loop,
		 (void *) (download->pconnections[i])) != 0)
	      proz_die(_("Error: Not enough system resources"));
	    pthread_cond_wait(&download->pconnections[i]->connecting_cond,
					  &download->status_change_mutex);
	    pthread_mutex_unlock(&download->status_change_mutex);
	  } else
	  {
 
	    /*download_show_message(download,
	       _
	       ("No additional paths on this server available, so will try to switch to another server")); */
	    /*Find any server that is downloading or completed and use it */
download_switch_server_ftpsearch(download, i);
	  }
	}
      } else /*Handle the file not being found on the server */
	if (connection_err == FTPCWDFAIL)
      {
	if (proz_download_all_dls_ftpcwdfail(download) == TRUE)
	{
	  download_show_message(download,
				_
				("Failed to change to the working directory on all the connections!"));
	  /*Terminate the connections */
	  proz_download_stop_downloads(download);
	  return DLREMOTEFATAL;
	} else
	{

	  /*Now we have to be careful */
	  int server_pos, cur_path_pos;

	  server_pos =
	      ftpsearch_get_server_position(download->ftps_info,
					    download->pconnections[i]->u.
					    host);
	  assert(server_pos != -1);
	  cur_path_pos =
	      ftpsearch_get_path_position(download->ftps_info,
					  download->pconnections[i]->u.host,
					  download->pconnections[i]->u.dir);
	  assert(cur_path_pos != -1);

	  proz_debug("Server pos = %d, cur_path_pos=%d", server_pos,
		     cur_path_pos);

	  /*mark path as not valid */
	  download->ftps_info->mirrors[server_pos].paths[cur_path_pos].
	      valid = FALSE;
	  /*See if more paths are avail */ ;
	  if (cur_path_pos <
	      (download->ftps_info->mirrors[server_pos].num_paths - 1))
	  {
	    char *url_buf;

	    /* Make sure this thread has terminated */
	    pthread_join(download->threads[i], NULL);
	    /*additional paths avail to try */

	    download_show_message(download,
				  _
				  ("Trying additional paths available on this server"));
	    proz_debug("Trying path %s",
		       download->ftps_info->mirrors[server_pos].
		       paths[cur_path_pos + 1]);

	    url_buf =
		malloc(strlen
		       (download->ftps_info->mirrors[server_pos].
			server_name) +
		       strlen(download->ftps_info->mirrors[server_pos].
			      paths[cur_path_pos + 1].path) +
		       strlen(download->pconnections[i]->u.file) + 11 + 1);

	    sprintf(url_buf, "ftp://%s/%s/%s",
		    download->ftps_info->mirrors[server_pos].server_name,
		    download->ftps_info->mirrors[server_pos].
		    paths[cur_path_pos + 1].path,
		    download->pconnections[i]->u.file);

	    proz_debug("Target url for relaunching is %s", url_buf);
	    /*FIXME */
	    proz_parse_url(url_buf, &download->pconnections[i]->u, 0);
	    free(url_buf);

	    /*Relaunch thread */
	    if (pthread_create
		(&download->threads[i], NULL, (void *) &ftp_loop,
		 (void *) (download->pconnections[i])) != 0)
	      proz_die(_("Error: Not enough system resources"));
	  } else
	  {

	    /*      download_show_message(download, _("No additional paths on this server available, so will try to switch to another server")); */


	    proz_debug
		("No additional paths on this server available, so will try to switch to another server");
	    /*Find any server that is downloading or completed and use it */
	    download_switch_server_ftpsearch(download, i);
	  }
	}
      } else if (connection_err == FTPRESTFAIL)
      {
	/*Handle the server not supporting REST */
	if (proz_download_all_dls_err(download, FTPRESTFAIL) == TRUE)
	{
	  download_show_message(download,
				_
				("The server(s) do not support  REST on all the connections!"));
	  /*Terminate the connections */
	  proz_download_stop_downloads(download);
	  return DLREMOTEFATAL;
	} else
	{

	  /*Now we have to be careful */
	  int server_pos;

	  server_pos =
	      ftpsearch_get_server_position(download->ftps_info,
					    download->pconnections[i]->u.
					    host);

	  proz_debug("Server pos = %d", server_pos);
	  /*mark server as not supporting resume */
	  download->ftps_info->mirrors[server_pos].resume_supported =
	      FALSE;

	  {
   download_show_message(download,
				    _
				    ("This server does not support resuming downloads, so will switch to another server"));
	    download_switch_server_ftpsearch(download, i);

	  }
	}
      }

      break;

    case LOCALFATAL:
      proz_download_stop_downloads(download);
      download_show_message(download,
			    _
			    ("Connection %d, had a local fatal error: %s .Aborting download. "),
			    i,
			    proz_strerror(download->pconnections[i]->err));
      return DLLOCALFATAL;
      break;

    case LOGINFAIL:
      /*
       * First check if the ftp server did not allow any thread 
       * to login at all, then  retry the curent thread 
       */
      if (proz_download_all_dls_status(download, LOGINFAIL) == TRUE)
      {
	download_show_message(download,
			      _
			      ("All logins rejected! Retrying connection"));
	/* Make sure this thread has terminated */
	pthread_join(download->threads[i], NULL);
	pthread_mutex_lock(&download->status_change_mutex);
	if (pthread_create
	    (&download->threads[i], NULL, (void *) &ftp_loop,
	     (void *) (download->pconnections[i])) != 0)
	  proz_die(_("Error: Not enough system resources"));

	pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			  &download->status_change_mutex);
	pthread_mutex_unlock(&download->status_change_mutex);
	break;
      } else
      {

	    int j, usable_server;

	    /*Find any server that is downloading or completed and use it instead */

	    usable_server = -1;

	    pthread_mutex_lock(&download->status_change_mutex);
	    for (j = 0; j < download->num_connections; j++)
	    {
	      if ((download->pconnections[j]->status == DOWNLOADING ||
		  download->pconnections[j]->status == COMPLETED) && j!=i)
	      {
		usable_server = j;
	      }
	    }
	    pthread_mutex_unlock(&download->status_change_mutex);

	    if (usable_server != -1)
	    {
	      /*We have a server which is DLING */
	      download_show_message(download,
				    _
				    ("This server has rejected the login attempt, so will switch to another server"));
	      /* Make sure this thread has terminated */
	      pthread_join(download->threads[i], NULL);

	      /*copy url and relaunch */
	      proz_free_url(&download->pconnections[i]->u, 0);
	      memcpy(&download->pconnections[i]->u,
		     proz_copy_url(&download->
				   pconnections[usable_server]->u),
		     sizeof(urlinfo));
	      proz_debug
		  ("Found server %s which is downloading will relaunch based on it",
		   download->pconnections[usable_server]->u.host);


	      /*Relaunch thread */
	      pthread_join(download->threads[i], NULL);
	      pthread_mutex_lock(&download->status_change_mutex);
	      download_show_message(download, _("Relaunching download"));
	      if (pthread_create
		  (&download->threads[i], NULL, (void *) &ftp_loop,
		   (void *) (download->pconnections[i])) != 0)
		proz_die(_("Error: Not enough system resources"));

	      pthread_cond_wait(&download->pconnections[i]->connecting_cond,
					  &download->status_change_mutex);
	      pthread_mutex_unlock(&download->status_change_mutex);
	    } else
	    {
	     
	/*
	 * Ok so at least there is one download whos login has not been rejected, 
	 * so lets see if it has completed, if so we can relaunch this connection, 
	 * as the commonest reason for a ftp login being rejected is because, the 
	 * ftp server has a limit on the number of logins permitted from the same 
	 * IP address. 
	 */

	/*
	 * Query the number of threads that are downloading 
	 * if it is zero then relaunch this connection
	 */
	int server_pos;
	int dling_conns_count =
	    download_query_conns_status_count(download, DOWNLOADING,
					      download->
					      pconnections[i]->u.host);

	server_pos =
	    ftpsearch_get_server_position(download->ftps_info,
					  download->pconnections[i]->u.host);

	if (dling_conns_count >
	    download->ftps_info->mirrors[server_pos].max_simul_connections)
	{
	  download->ftps_info->mirrors[server_pos].max_simul_connections =
	      dling_conns_count;
	  break;
	}


	if (dling_conns_count == 0
	    &&
	    (download_query_conns_status_count
	     (download, CONNECTING, download->pconnections[i]->u.host) == 0)
	    &&
	    (download_query_conns_status_count
	     (download, LOGGININ, download->pconnections[i]->u.host) == 0))
	{

	  /* Make sure this thread has terminated */
	  pthread_join(download->threads[i], NULL);
	  pthread_mutex_lock(&download->status_change_mutex);
	  download_show_message(download, _("Relaunching download"));
	  if (pthread_create(&download->threads[i], NULL,
			     (void *) &ftp_loop,
			     (void *) (download->pconnections[i])) != 0)
	    proz_die(_("Error: Not enough system resources"));
	  pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
	  pthread_mutex_unlock(&download->status_change_mutex);
	} else
	    if (dling_conns_count <
		download->ftps_info->mirrors[server_pos].
		max_simul_connections
		&&
		(download_query_conns_status_count
		 (download, CONNECTING,
		  download->pconnections[i]->u.host) == 0)
		&&
		(download_query_conns_status_count
		 (download, LOGGININ,
		  download->pconnections[i]->u.host) == 0))
	{

	  /* Make sure this thread has terminated */
	  pthread_join(download->threads[i], NULL);
	  pthread_mutex_lock(&download->status_change_mutex);
	  download_show_message(download, _("Relaunching download"));
	  if (pthread_create(&download->threads[i], NULL,
			     (void *) &ftp_loop,
			     (void *) (download->pconnections[i])) != 0)
	    proz_die(_("Error: Not enough system resources"));
	  pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
	  pthread_mutex_unlock(&download->status_change_mutex);
	}
	    }
      }
      break;

    case CONREJECT:
      /*
       * First check if the ftp server did not allow any thread 
       * to login at all, then  retry the curent thread 
       */
      if (proz_download_all_dls_status(download, CONREJECT) == TRUE)
      {
	download_show_message(download,
			      _
			      ("All connections attempts have been  rejected! Retrying connection"));
	/* Make sure this thread has terminated */
	pthread_join(download->threads[i], NULL);
	pthread_mutex_lock(&download->status_change_mutex);

	if (pthread_create
	    (&download->threads[i], NULL, (void *) &ftp_loop,
	     (void *) (download->pconnections[i])) != 0)
	  proz_die(_("Error: Not enough system resources"));
	pthread_cond_wait(&download->pconnections[i]->connecting_cond,
					  &download->status_change_mutex);
	pthread_mutex_unlock(&download->status_change_mutex);
	break;
      } else
      {

	    int j, usable_server;

	    /*Find any server that is downloading or completed and use it instead */

	    usable_server = -1;

	    pthread_mutex_lock(&download->status_change_mutex);
	    for (j = 0; j < download->num_connections; j++)
	    {
	      if ((download->pconnections[j]->status == DOWNLOADING ||
		  download->pconnections[j]->status == COMPLETED) && j!=i)
	      {
		usable_server = j;
	      }
	    }
	    pthread_mutex_unlock(&download->status_change_mutex);

	    if (usable_server != -1)
	    {
	      /*We have a server which is DLING */
	      download_show_message(download,
				    _
				    ("This server has rejected the connection attempt, so will switch to another server"));
	      /* Make sure this thread has terminated */
	      pthread_join(download->threads[i], NULL);

	      /*copy url and relaunch */
	      proz_free_url(&download->pconnections[i]->u, 0);
	      memcpy(&download->pconnections[i]->u,
		     proz_copy_url(&download->
				   pconnections[usable_server]->u),
		     sizeof(urlinfo));
	      proz_debug
		  ("Found server %s which is downloading will relaunch based on it",
		   download->pconnections[usable_server]->u.host);

	      /*Relaunch thread */
	      pthread_mutex_lock(&download->status_change_mutex);
	      
	      if (pthread_create
		  (&download->threads[i], NULL, (void *) &ftp_loop,
		   (void *) (download->pconnections[i])) != 0)
		proz_die(_("Error: Not enough system resources"));

	      pthread_cond_wait(&download->pconnections[i]->connecting_cond,
					  &download->status_change_mutex);
	      pthread_mutex_unlock(&download->status_change_mutex);

	    } else
	    {
	/*
	 * Ok so at least there is one download whos connections attempt has not been rejected, 
	 * so lets see if it has completed, if so we can relaunch this connection, 
	 * as the commonest reason for a ftp login being rejected is because, the 
	 * ftp server has a limit on the number of logins permitted from the same 
	 * IP address. 
	 */

	/*
	 * Query the number of threads that are downloading 
	 * if it is zero then relaunch this connection
	 */
	int server_pos;
	int dling_conns_count =
	    download_query_conns_status_count(download, DOWNLOADING,
					      download->
					      pconnections[i]->u.host);
	server_pos =
	    ftpsearch_get_server_position(download->ftps_info,
					  download->pconnections[i]->u.host);


	if (dling_conns_count >
	    download->ftps_info->mirrors[server_pos].max_simul_connections)
	{
	  download->ftps_info->mirrors[server_pos].max_simul_connections =
	      dling_conns_count;
	  break;
	}


	if (dling_conns_count == 0
	    &&
	    (download_query_conns_status_count
	     (download, CONNECTING, download->pconnections[i]->u.host) == 0)
	    &&
	    (download_query_conns_status_count
	     (download, LOGGININ, download->pconnections[i]->u.host) == 0))
	{

	  /* Make sure this thread has terminated */
	  pthread_join(download->threads[i], NULL);
	  pthread_mutex_lock(&download->status_change_mutex);
	  download_show_message(download, _("Relaunching download"));
	  if (pthread_create(&download->threads[i], NULL,
			     (void *) &ftp_loop,
			     (void *) (download->pconnections[i])) != 0)
	    proz_die(_("Error: Not enough system resources"));
	  pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
	  pthread_mutex_unlock(&download->status_change_mutex);
	} else
	    if (dling_conns_count <
		download->ftps_info->mirrors[server_pos].
		max_simul_connections
		&&
		(download_query_conns_status_count
		 (download, CONNECTING,
		  download->pconnections[i]->u.host) == 0)
		&&
		(download_query_conns_status_count
		 (download, LOGGININ,
		  download->pconnections[i]->u.host) == 0))
	{

	  /* Make sure this thread has terminated */
	  pthread_join(download->threads[i], NULL);
	  pthread_mutex_lock(&download->status_change_mutex);
	  download_show_message(download, _("Relaunching download"));
	  if (pthread_create(&download->threads[i], NULL,
			     (void *) &ftp_loop,
			     (void *) (download->pconnections[i])) != 0)
	    proz_die(_("Error: Not enough system resources"));
	  pthread_cond_wait(&download->pconnections[i]->connecting_cond,
			    &download->status_change_mutex);
	  pthread_mutex_unlock(&download->status_change_mutex);
	}
	    }
      }
      break;

    default:
      break;
    }
  }

  /*bandwith throttling */
  download_calc_throttle_factor(download);
  return DLINPROGRESS;
}



/******************************************************************************
 This will setup a download based on the connection info and the ftpsearch results.
******************************************************************************/
int proz_download_setup_connections_ftpsearch(download_t * download,
					      connection_t * connection,
					      ftps_request_t * request,
					      int req_connections)
{
  int num_connections, i, num_usable_mirrors = 0;
  off_t bytes_per_connection;
  off_t bytes_left;
  FILE *fp;
  char *out_file;
  struct stat stat_buf;
  /*TODO Check for log file and use same number of threads */

  proz_debug("proz_download_setup_connections_ftpsearch");
  download->main_file_size = connection->main_file_size;

  download->ftps_info = request;
  num_connections = req_connections;
  /*Are there any mirrors that are working? */
  /**/
  if (request->num_mirrors > 0
      && request->mirrors[0].status == RESPONSEOK)
    {
	  proz_debug("got mirrors");
      /*We should get the info and allocate here */
      download->resume_support = TRUE;


      bytes_per_connection = connection->main_file_size / num_connections;
      bytes_left = connection->main_file_size % num_connections;

      /* Get the number of mirros that are within 200% ping speed of the fastest */
      for (i = 0; i < request->num_mirrors; i++)
	{
	  if ((request->mirrors[i].status == RESPONSEOK) &&
	      (request->mirrors[i].milli_secs <=
	       (request->mirrors[0].milli_secs +
		((request->mirrors[0].milli_secs * 100) / 100))))
	    num_usable_mirrors++;
	}

      proz_debug("usable mirrors = %d", num_usable_mirrors);
      proz_debug("num_connections = %d", num_connections);
      download->pconnections =
	kmalloc(sizeof(connection_t*) * num_connections);
      download->num_connections = num_connections;

      for (i = 0; i < num_connections; i++)
	{
	  urlinfo *url = malloc(sizeof(urlinfo));
	  char *url_buf;

	  if (i < num_usable_mirrors)
	    {
	      url_buf =
		malloc(strlen(request->mirrors[i].server_name) +
		       strlen(request->mirrors[i].paths[0].path) +
		       strlen(connection->u.file) + 11 + 1);

	      sprintf(url_buf, "ftp://%s/%s/%s", request->mirrors[i].server_name,
		      request->mirrors[i].paths[0].path, connection->u.file);

	      proz_debug("Target url is %s", url_buf);
	      /*FIXME */
	      proz_parse_url(url_buf, url, 0);
	      free(url_buf);

	      download->pconnections[i]=proz_connection_init(url,
							     &download->status_change_mutex);

	    } else
	      {
		int extra_mirror = 0;
		/*FIXME improve allocation algorithm in this part */

		url_buf =
		  malloc(strlen(request->mirrors[extra_mirror].server_name) +
			 strlen(request->mirrors[extra_mirror].paths[0].path) +
			 strlen(connection->u.file) + 11 + 1);

		sprintf(url_buf, "ftp://%s/%s/%s",
			request->mirrors[extra_mirror].server_name,
			request->mirrors[extra_mirror].paths[0].path,
			connection->u.file);

		proz_debug("Target url is %s", url_buf);
		/*FIXME */
		proz_parse_url(url_buf, url, 0);
		free(url_buf);

		download->pconnections[i]=proz_connection_init(url,
							       &download->status_change_mutex);
	      }


	  download->resume_support = download->pconnections[i]->resume_support =
	    TRUE;
	  memcpy(&download->pconnections[i]->hs, &connection->hs,
		 sizeof(http_stat_t));


	  out_file=kmalloc(PATH_MAX);
	  snprintf(out_file, PATH_MAX, "%s/%s.prozilla",
		   download->dl_dir, connection->u.file);


	  //First see if the file exists then we dont create a new one else we do

	  if (stat(out_file, &stat_buf) == -1)
	    {
	      /* the call failed */
	      /* if the error is due to the file not been present then there is no 
		 need to do anything..just continue, otherwise return error (-1)
	      */
	      if (errno == ENOENT)
		{
		  //File not exists so create it
		  if (!
		      (fp =
		       fopen(out_file, "w+")))
		    {
		      download_show_message(download,
					    _
					    ("Unable to open file %s: %s!"),
					    out_file, strerror(errno));
		      return -1;
		    }

		}
	      else
		return -1;
	    }
	  else
	    {
	      //TODO: File exists : so stat it and if it doesnt match file size warna boput it...
	      if (!
		  (fp =
		   fopen(out_file, "r+")))
		{
		  download_show_message(download,
					_
					("Unable to open file %s: %s!"),
					out_file, strerror(errno));
		  return -1;
		}

	    }


	  //TRY setting the offset;
	  if (download->main_file_size != -1)
	    {

	      if(fseeko(fp, download->main_file_size, SEEK_SET)!=0)
		return -1;
	    }

	  /*Make sure all writes go directly to the file */
	  setvbuf(fp, NULL, _IONBF, 0);



	  download->pconnections[i]->localfile = kmalloc(PATH_MAX);
	  strcpy(out_file, download->pconnections[i]->localfile);
	  download->pconnections[i]->fp=fp;


	  download->pconnections[i]->retry = TRUE;

	  download->pconnections[i]->main_file_size = connection->main_file_size;
	  download->pconnections[i]->orig_remote_startpos = download->pconnections[i]->remote_startpos = i * bytes_per_connection;
	  download->pconnections[i]->remote_endpos =
	    i * bytes_per_connection + bytes_per_connection;

	  //Changing things here.....
	  download->pconnections[i]->local_startpos =    download->pconnections[i]->remote_startpos;


	  /*Set the connections message to be download->msg_proc calback */
	  proz_connection_set_msg_proc(download->pconnections[i],
				       download->msg_proc, download->cb_data);
	}

      /* Add the remaining bytes to the last connection   */
      download->pconnections[--i]->remote_endpos += bytes_left;
    } else
      {
	proz_debug("No mirrors, which were up are found");
	download->using_ftpsearch = FALSE;
	return proz_download_setup_connections_no_ftpsearch(download,
							    connection,
							    req_connections);
      }

  download->using_ftpsearch = TRUE;
  return num_connections;
}


uerr_t proz_download_get_join_status(download_t *download)
{

  int building_status;
 pthread_mutex_lock(&download->access_mutex);
 building_status = download->building;
 pthread_mutex_unlock(&download->access_mutex);
 
 switch(building_status)
   {
   case 1:
     return JOININPROGRESS;
   case 0:
     return JOINDONE;
   case -1:
     return JOINERR;
   default:
     proz_die("Bad building falg in download structure");
   }

   return -1;
}


float proz_download_get_file_build_percentage(download_t *download)
{

  float percent_done;
  pthread_mutex_lock(&download->access_mutex);
  percent_done =100;
  pthread_mutex_unlock(&download->access_mutex);
  return percent_done;
}

void proz_download_cancel_joining_thread(download_t * download)
{
      pthread_cancel(download->join_thread);
      pthread_join(download->join_thread, NULL);
}

void proz_download_wait_till_end_joining_thread(download_t * download)
{
 pthread_join(download->join_thread, NULL);
}
