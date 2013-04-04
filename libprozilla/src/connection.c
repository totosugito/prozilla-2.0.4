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

/* $Id: connection.c,v 1.48 2005/09/19 16:18:02 kalum Exp $ */

#include "common.h"

#include "prozilla.h"
#include "connection.h"
#include "misc.h"
#include "connect.h"
#include "ftp.h"
#include "http.h"
#include "debug.h"


/******************************************************************************
 ...
******************************************************************************/
void init_response(connection_t * connection)
{
  connection->serv_ret_lines = 0;
}

/******************************************************************************
 This will free up the serv_ret_lines array if necessary, in order to prepare
 it for the storage of the servers response.
******************************************************************************/
void done_with_response(connection_t * connection)
{
  response_line *p, *p1;

  /* Return if the array is not allocated. */
  if (connection->serv_ret_lines == 0)
    return;

  p1 = connection->serv_ret_lines;

  do
  {
    p = p1;
    p1 = p->next;
    kfree(p);
  }
  while (p1 != 0);

  connection->serv_ret_lines = 0;
}

/******************************************************************************
 Initialises the connection and sets it with values from the runtime struct.
******************************************************************************/

connection_t * proz_connection_init(urlinfo *url,pthread_mutex_t * mutex)
{

  connection_t * connection=kmalloc(sizeof(connection_t)); 
  memset(connection, 0, sizeof(connection_t));

  /*  memcpy(&connection->u, url, sizeof(urlinfo));*/
  if(url)
    memcpy(&connection->u,
		     proz_copy_url(url),
		     sizeof(urlinfo));

  /* Copy the proxy structs. */
  if (libprozrtinfo.ftp_proxy)
  {
    connection->ftp_proxy = kmalloc(sizeof(proxy_info));
    memcpy(connection->ftp_proxy, libprozrtinfo.ftp_proxy,
	   sizeof(proxy_info));
    /*
       connection->use_ftp_proxy = libprozrtinfo.use_ftp_proxy;
     */
  }

  if (libprozrtinfo.http_proxy)
  {
    connection->http_proxy = kmalloc(sizeof(proxy_info));
    memcpy(connection->http_proxy, libprozrtinfo.http_proxy,
	   sizeof(proxy_info));
    /*
       connection->use_http_proxy = libprozrtinfo.use_http_proxy;
     */
  }

  connection->use_netrc = libprozrtinfo.use_netrc;

  connection->retry = TRUE;
  connection->ftp_use_pasv = libprozrtinfo.ftp_use_pasv;
  connection->http_no_cache = libprozrtinfo.http_no_cache;

  connection->user_agent = strdup(DEFAULT_USER_AGENT);
  connection->file_mode = strdup("wb");
  /*NOTE: default of unlimited attempts */
  connection->max_attempts = 0;
  connection->attempts = 0;
  /* Initialise all with default timeouts */
  memcpy(&connection->xfer_timeout, &libprozrtinfo.conn_timeout,
	 sizeof(connection->xfer_timeout));
  memcpy(&connection->ctrl_timeout, &libprozrtinfo.conn_timeout,
	 sizeof(connection->ctrl_timeout));
  memcpy(&connection->conn_timeout, &libprozrtinfo.conn_timeout,
	 sizeof(connection->conn_timeout));
  memcpy(&connection->retry_delay, &libprozrtinfo.conn_retry_delay,
	 sizeof(&connection->retry_delay));
  connection->max_attempts = libprozrtinfo.max_attempts;

  connection->rate_bps = 0;
  /* Unlimited bandwith (0) */
  connection->max_allowed_bps = 0;
  pthread_cond_init(&connection->connecting_cond, NULL);
  connection->status_change_mutex = mutex;
  if (connection->status_change_mutex != 0)
  {
    pthread_mutex_init(connection->status_change_mutex, NULL);
  }
  pthread_mutex_init(&connection->access_mutex, NULL);

  return connection;
}

/* Locks the connections mutex befoer chaging state. */
void connection_change_status(connection_t * connection, dl_status status)
{
  if (connection->status_change_mutex != 0)
  {
    pthread_mutex_lock(connection->status_change_mutex);
    connection->status = status;
    pthread_mutex_unlock(connection->status_change_mutex);
  }
}


/* this will open connection->localfile and read from connection->data_sock 
   (which should be already setup) till a EOF is reached or 
   the server closes the connection, in which case there is no way to know 
   whether we got the complete file.  
*/

uerr_t connection_retr_fsize_not_known(connection_t * connection,
				       char *read_buffer,
				       int read_buffer_size)
{
  off_t bytes_read;

  connection_change_status(connection, DOWNLOADING);
  gettimeofday(&connection->time_begin, NULL);

  do
  {
    bytes_read =
	krecv(connection->data_sock, read_buffer, read_buffer_size, 0,
	      &connection->xfer_timeout);
    if (bytes_read > 0)
    {
      if (write_data_with_lock(connection, read_buffer, sizeof(char), bytes_read) < bytes_read)
      {
	proz_debug(_("write failed"));
	connection_show_message(connection,
				_
				("Unable to write to file %s: %s!"),
				connection->localfile, strerror(errno));
	connection_change_status(connection, LOCALFATAL);
	return FWRITEERR;
      }
      pthread_mutex_lock(&connection->access_mutex);
      connection->remote_bytes_received += bytes_read;
      pthread_mutex_unlock(&connection->access_mutex);

      /*TODO: caclculate and throttle connections speed here */
      /*DONE: */
      connection_calc_ratebps(connection);
      connection_throttle_bps(connection);
    }
  }
  while (bytes_read > 0);

  if (bytes_read == -1)
  {
    if (errno == ETIMEDOUT)
    {
      proz_debug(_("connection timed out"));
      connection_change_status(connection, TIMEDOUT);
      return READERR;
    }
    connection_change_status(connection, REMOTEFATAL);
    return READERR;
  }

  connection_change_status(connection, COMPLETED);

  connection_show_message(connection,
			  _("download for this connection completed"
			    "%s : %ld received"), connection->localfile,
			  connection->remote_bytes_received);
  return FILEGETOK;
}


/* This will open connection->localfile and read from connection->data_sock 
   (which should be already setup) till the requested number of bytes are read.
   Now since we explicitly know how much bytes to get we can do so, and is the server 
   closes the connection prematurely we know that has hapenned (because it hasn't supplied 
   the required number of bytes) and return a READERR.
*/
uerr_t connection_retr_fsize_known(connection_t * connection,
				   char *read_buffer, int read_buffer_size)
{
  off_t bytes_read;
  off_t bytes_to_get;

  pthread_mutex_lock(&connection->access_mutex);
  bytes_to_get = connection->remote_endpos - connection->remote_startpos;
  pthread_mutex_unlock(&connection->access_mutex);

  connection_change_status(connection, DOWNLOADING);
  gettimeofday(&connection->time_begin, NULL);

  while (bytes_to_get > 0)
  {
    bytes_read =
	krecv(connection->data_sock, read_buffer,
	      bytes_to_get >
	      read_buffer_size ? read_buffer_size : bytes_to_get, 0,
	      &connection->xfer_timeout);

    if (bytes_read == 0 && bytes_to_get > 0)
    {
      connection_show_message(connection,
			      _("Server Closed Connection Prematurely!"));
      connection_change_status(connection, REMOTEFATAL);
      return READERR;
    }

    if (bytes_read == -1)
    {
      if (errno == ETIMEDOUT)
      {
	proz_debug(_("connection timed out"));
	connection_change_status(connection, TIMEDOUT);
	return READERR;
      }
      connection_change_status(connection, REMOTEFATAL);
      return READERR;
    }

    bytes_to_get -= bytes_read;

    if (bytes_read > 0)
    {
     
      if (write_data_with_lock(connection, read_buffer, sizeof(char), bytes_read) < bytes_read)
      {

	proz_debug(_("write failed"));
	connection_show_message(connection,
				_
				("Unable to write to file %s: %s!"),
				connection->localfile, strerror(errno));
	connection_change_status(connection, LOCALFATAL);
	return FWRITEERR;
      }
      pthread_mutex_lock(&connection->access_mutex);
      connection->remote_bytes_received += bytes_read;
      pthread_mutex_unlock(&connection->access_mutex);

      /*TODO: caclculate and throttle connections speed here */
      /*DONE: */
      connection_calc_ratebps(connection);
      connection_throttle_bps(connection);
    }
  }

  connection_change_status(connection, COMPLETED);

  connection_show_message(connection,
			  _("download for this connection completed"
			    "%s : %ld received"), connection->localfile,
			  connection->remote_bytes_received);
  return FILEGETOK;
}


/* This function modifies a single connections download start and 
   end info it returns 1 on sucess and -1 on error. 
*/


int connection_load_resume_info(connection_t * connection)
{



      if(connection->remote_startpos-connection->orig_remote_startpos!=connection->remote_bytes_received)
	{
	  proz_debug("connection->remote start pos before loading %ld", connection->remote_startpos);
	  //connection->remote_startpos +=connection->remote_bytes_received;
	  connection->remote_startpos +=(connection->remote_bytes_received-(connection->remote_startpos-connection->orig_remote_startpos));	  

	  proz_debug("connection->remote start pos after loading %ld", connection->remote_startpos);
	}
  return 1;
}

dl_status proz_connection_get_status(connection_t * connection)
{
  dl_status status;
  pthread_mutex_lock(connection->status_change_mutex);
  status = connection->status;
  pthread_mutex_unlock(connection->status_change_mutex);
  return status;
}


/*This will return a textual representation of the status of a conenction. */
char *proz_connection_get_status_string(connection_t * connection)
{
  dl_status status;
  pthread_mutex_lock(connection->status_change_mutex);
  status = connection->status;
  pthread_mutex_unlock(connection->status_change_mutex);

  switch (connection->status)
  {

  case IDLE:
    return (_("Idle"));

  case CONNECTING:
    return (_("Connecting"));

  case LOGGININ:
    return (_("Logging in"));

  case DOWNLOADING:
    return (_("Downloading"));
    break;
  case COMPLETED:
    return (_("Completed"));

  case LOGINFAIL:
    return (_("Login Denied"));

  case CONREJECT:
    return (_("Connect Refused"));

  case REMOTEFATAL:
    return (_("Remote Fatal"));

  case LOCALFATAL:
    return (_("Local Fatal"));

  case TIMEDOUT:
    return (_("Timed Out"));
  case MAXTRYS:
    return (_("Max attempts reached"));

  default:
    return (_("Unkown Status!"));
  }
}


pthread_mutex_t connection_msg_mutex = PTHREAD_MUTEX_INITIALIZER;

/*calls the msg_proc function if not null */
void connection_show_message(connection_t * connection, const char *format,
			     ...)
{
  va_list args;
  char message[MAX_MSG_SIZE + 1];

  pthread_mutex_lock(&connection_msg_mutex);
  va_start(args, format);
  vsnprintf(message, MAX_MSG_SIZE, format, args);
  va_end(args);
  if (connection->msg_proc)
    connection->msg_proc(message, connection->cb_data);

  /*FIXME: Remove this later */
//  printf("%s\n", message);
  pthread_mutex_unlock(&connection_msg_mutex);
}


/* Returns the total number of bytes that have been saved to the file*/
off_t proz_connection_get_total_bytes_got(connection_t * connection)
{
  off_t ret;

  pthread_mutex_lock(&connection->access_mutex);
  ret = connection->remote_bytes_received;
  pthread_mutex_unlock(&connection->access_mutex);

  return ret;

}

/***************************************************************************** 
Returns the total number of bytes that has being got from the server 
by this connection.
******************************************************************************/
off_t proz_connection_get_total_remote_bytes_got(connection_t * connection)
{
  off_t ret;
  pthread_mutex_lock(&connection->access_mutex);
  ret = (connection->remote_bytes_received
	 -   (connection->remote_startpos-connection->orig_remote_startpos));
  pthread_mutex_unlock(&connection->access_mutex);
  //proz_debug("CONNECTION TOTAL REMOTE BYTES GOT =%lld", ret); 
  return ret;
}

void proz_get_url_info_loop(connection_t * connection, pthread_t *thread)
{
  assert(connection);
  assert(thread);
  connection->running = TRUE;
  pthread_create(thread, NULL,
		 (void *(*)(void *)) get_url_info_loop,
		 (void *) connection);
}

/************************************************************************
This Fucntion will retreive info about the given url in the connection, 
handling conditions like redirection from http to ftp etc 

*************************************************************************/
void get_url_info_loop(connection_t * connection)
{

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  /*TODO Should we try to make it broadcast a condition to the other threads? */
  pthread_mutex_lock(&connection->access_mutex);
  connection->running = TRUE;
  pthread_mutex_unlock(&connection->access_mutex);

  do
  {
    switch (connection->u.proto)
    {
    case URLHTTP:
      connection->err = http_get_url_info_loop(connection);
      break;
    case URLFTP:
      connection->err = ftp_get_url_info_loop(connection);
      break;
    default:
      proz_die(_("Error: unsupported protocol"));
    }

    if (connection->err == NEWLOCATION)
    {
      char *constructed_newloc;
      char *referer;
      referer=kstrdup(connection->u.url);
      /*DONE : handle relative urls too */
      constructed_newloc =
	  uri_merge(connection->u.url, connection->hs.newloc);

      proz_debug("Redirected to %s, merged URL = %s",
		 connection->hs.newloc, constructed_newloc);

      proz_free_url(&connection->u, 0);
      connection->err =
	  proz_parse_url(constructed_newloc, &connection->u, 0);


      if (connection->err != URLOK)
      {
	connection_show_message(connection,
				_
				("The server returned location is wrong: %s!"),
				constructed_newloc);
	pthread_mutex_lock(&connection->access_mutex);
	connection->running = FALSE;
	pthread_mutex_unlock(&connection->access_mutex);
	kfree(constructed_newloc);
	connection->err = HERR;
	return;
      } else
	connection_show_message(connection, _("Redirected to => %s"),
				constructed_newloc);
      connection->u.referer=referer;
      kfree(constructed_newloc);
      connection->err = NEWLOCATION;
    }

  }
  while (connection->err == NEWLOCATION);

  return;
}


void proz_connection_set_msg_proc(connection_t * connection,
				  message_proc msg_proc, void *cb_data)
{
  assert(connection != NULL);


  connection->msg_proc = msg_proc;
  connection->cb_data = cb_data;
}



void connection_calc_ratebps(connection_t * connection)
{
  struct timeval tv_cur;
  struct timeval tv_diff;
  float diff_us;

  pthread_mutex_lock(&connection->access_mutex);

  if (connection->time_begin.tv_sec == 0
      && connection->time_begin.tv_usec == 0)
  {

    connection->rate_bps = 0;
    pthread_mutex_unlock(&connection->access_mutex);
    return;
  } else
  {
    gettimeofday(&tv_cur, NULL);
    proz_timeval_subtract(&tv_diff, &tv_cur, &(connection->time_begin));
    diff_us = ((float) tv_diff.tv_sec * 10e5) + tv_diff.tv_usec;

/*     if (diff_us == 0) */
/*     { */
/*       pthread_mutex_unlock(&connection->access_mutex); */
/*       return; */
/*     } */
/*     connection->rate_bps = */
/* 	((float) (connection->remote_bytes_received */
/* 	 -   (connection->remote_startpos-connection->orig_remote_startpos)) * 10e5 / diff_us); */
/*   } */

    if (diff_us <100000)
    {
      connection->rate_bps = 0;
      pthread_mutex_unlock(&connection->access_mutex);
      return;
    }
    else
    connection->rate_bps =
	((float) (connection->remote_bytes_received
	 -   (connection->remote_startpos-connection->orig_remote_startpos)) * 10e5 / diff_us);
  }

  pthread_mutex_unlock(&connection->access_mutex);
  return;
}


void connection_throttle_bps(connection_t * connection)
{

  struct timeval tv_cur;
  struct timeval tv_diff;
  float diff_us;
  float wtime;
  struct timeval tv_delay;
  float con_timeout_usecs;

  pthread_mutex_lock(&connection->access_mutex);

  con_timeout_usecs =
      (connection->conn_timeout.tv_sec * 10e5) +
      connection->conn_timeout.tv_usec;

  if (connection->rate_bps == 0 || connection->max_allowed_bps == 0)
  {
    pthread_mutex_unlock(&connection->access_mutex);
    return;
  }

  if (connection->time_begin.tv_sec == 0
      && connection->time_begin.tv_usec == 0)
  {
    pthread_mutex_unlock(&connection->access_mutex);
    return;
  }


  gettimeofday(&tv_cur, NULL);
  proz_timeval_subtract(&tv_diff, &tv_cur, &(connection->time_begin));
  diff_us = ((float) tv_diff.tv_sec * 10e5) + tv_diff.tv_usec;

  if (diff_us == 0)
  {
    pthread_mutex_unlock(&connection->access_mutex);
    return;
  }


  wtime =
      10e5 * (connection->remote_bytes_received
	 -   (connection->remote_startpos-connection->orig_remote_startpos)) /
      connection->max_allowed_bps;

  pthread_mutex_unlock(&connection->access_mutex);

  memset(&tv_delay, 0, sizeof(tv_delay));

  if (wtime > diff_us)
  {
    /*too fast have to delay */
    //    proz_debug("wtime %f, diff_us %f", wtime, diff_us);
    if ((wtime - diff_us) > con_timeout_usecs)	/* problem here */
    {
      /*If we were to delay for wtime-diff_us we would cause a connection 
         timeout, so rather than doing that shall we delay for a bit lesser
         than the time for the timeout, like say 1 second less
       */
      const int limit_time_us = 2 * 10e5;

      /*  Will the connection timeout - limit_time_us  be less or equal to  0?
         If so no point in delaing beacuse the connection wold timeout
       */

      if ((con_timeout_usecs - limit_time_us) <= 0)
      {
	proz_debug
	    ("Cant throttle: Connection would timeout if done so, please try increasing the timeout value");
	return;
      }

      tv_delay.tv_usec = con_timeout_usecs - limit_time_us;
      /*        message
         ("Cant throttle fully : Connection would timeout if done so, please try increasing the timeout value"); */

      proz_debug("delaymaxlimit %ld sec\n", tv_delay.tv_usec);
    } else
    {
      tv_delay.tv_usec = (wtime - diff_us);
//#warning "comment out the following line before releasing the code base"
      proz_debug("sleeping %f secs\n", (wtime - diff_us) / 10e5);
    }

    tv_delay.tv_sec = tv_delay.tv_usec / 1000000;
    tv_delay.tv_usec = tv_delay.tv_usec % 1000000;

    if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv_delay) < 0)
    {
      proz_debug("Unable to throttle Bandwith\n");
    }

  }
}


boolean proz_connection_running(connection_t * connection)
{
  boolean running;
  pthread_mutex_lock(&connection->access_mutex);
  running = connection->running;
  pthread_mutex_unlock(&connection->access_mutex);
  return running;
}

void proz_connection_set_url(connection_t * connection, urlinfo *url)
{
  assert(url);
    memcpy(&connection->u,
		     proz_copy_url(url),
		     sizeof(urlinfo));
}

void proz_connection_free_connection(connection_t * connection,
				     boolean complete)
{
  assert(connection);
  /*TODO what about szBuffer..also have to free the URL u */

  if (connection->localfile)
    kfree(connection->localfile);
  if (connection->file_mode)
    kfree(connection->file_mode);
  if (connection->http_proxy)
    kfree(connection->http_proxy);
  if (connection->ftp_proxy)
    kfree(connection->ftp_proxy);

  if (connection->user_agent)
    kfree(connection->user_agent);

  /* free the serv_ret_lines array */
  if (connection->serv_ret_lines != 0)
  {
    done_with_response(connection);
  }

  if (complete == TRUE)
    kfree(connection);
}


size_t write_data_with_lock(connection_t * connection, const void *ptr, size_t size, size_t nmemb)
{
  size_t ret;
  flockfile(connection->fp);
  /*Seek appropriately......*/

  ret=fseeko(connection->fp, connection->local_startpos+connection->remote_bytes_received, SEEK_SET);
  ret=fwrite( ptr, size, nmemb, connection->fp);

  funlockfile(connection->fp);
  return ret;
}
