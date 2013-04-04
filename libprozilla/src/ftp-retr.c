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

/* $Id: ftp-retr.c,v 1.18 2005/01/11 01:49:11 sean Exp $ */


#include "common.h"
#include "prozilla.h"
#include "connect.h"
#include "misc.h"
#include "url.h"
#include "netrc.h"
#include "debug.h"
#include "ftp.h"
#include "ftp-retr.h"


/* Will download a portion of/or the full file from the connection->url.
 */

uerr_t proz_ftp_get_file(connection_t * connection)
{
  uerr_t err;
  char *user, *passwd;
  netrc_entry *netrc_ent;
  boolean passive_mode;
  char buffer[HTTP_BUFFER_SIZE];


  /* we want it to terminate immediately */

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  assert(connection->localfile != NULL);
  assert(connection->file_mode != NULL);

  /*clear the socks */
  connection->ctrl_sock = 0;
  connection->data_sock = 0;
  connection->listen_sock = 0;

  /* if there is nothing to download then return  */
  if (connection->status == COMPLETED)
  {
    gettimeofday(&connection->time_begin, NULL);
    return FTPOK;
  }

  init_response(connection);

  pthread_mutex_lock(connection->status_change_mutex);
  connection->status = CONNECTING;
  /*  connection_change_status(connection, CONNECTING); */
  pthread_cond_broadcast(&connection->connecting_cond);
  pthread_mutex_unlock(connection->status_change_mutex);

  /* if we have to use a HTTP proxy call the routine which is defined in http-retr.c  and just return.
   */
  if (ftp_use_proxy(connection)
      && connection->ftp_proxy->type == HTTPPROXY)
  {
    err = ftp_get_file_from_http_proxy(connection);
    return err;
  }

  if (ftp_use_proxy(connection))
  {
    /* Connect to the proxy server here. */
    err = ftp_connect_to_server(connection,
				connection->ftp_proxy->proxy_url.host,
				connection->ftp_proxy->proxy_url.port);
  } else
  {
    err = ftp_connect_to_server(connection, connection->u.host,
				connection->u.port);
  }

  if (err == FTPCONREFUSED)
  {
    connection_change_status(connection, CONREJECT);
    close_sock(&connection->ctrl_sock);
    return err;
  }
  if (err != FTPOK)
  {
    connection_change_status(connection, REMOTEFATAL);
    return err;
  }

  done_with_response(connection);

  user = connection->u.user;
  passwd = connection->u.passwd;

  /* Use .netrc if asked to do so. */
  if (connection->use_netrc == TRUE)
  {
    netrc_ent = search_netrc(libprozrtinfo.netrc_list, connection->u.host);

    if (netrc_ent != NULL)
    {
      user = netrc_ent->account;
      passwd = netrc_ent->password;
    }
  }

  user = user ? user : libprozrtinfo.ftp_default_user;
  passwd = passwd ? passwd : libprozrtinfo.ftp_default_passwd;
  proz_debug(_("Logging in as user %s with password %s."), user, passwd);

  connection_change_status(connection, LOGGININ);

  init_response(connection);
  err = ftp_login(connection, user, passwd);
  if (err != FTPOK)
  {
    if (err == FTPLOGREFUSED)
    {
      connection_change_status(connection, LOGINFAIL);
    } else
    {
      connection_change_status(connection, REMOTEFATAL);
    }
    close_sock(&connection->ctrl_sock);
    return err;
  }
  done_with_response(connection);


  init_response(connection);
  err = ftp_binary(connection);
  if (err != FTPOK)
  {
    connection_change_status(connection, REMOTEFATAL);
    close_sock(&connection->ctrl_sock);
    return err;
  }
  done_with_response(connection);

  /* Do we need to CWD? */
  if (*connection->u.dir)
  {
    init_response(connection);

    err = ftp_cwd(connection, connection->u.dir);
    if (err != FTPOK)
    {
      connection_change_status(connection, REMOTEFATAL);
      proz_debug(_("CWD failed to change to directory '%s'."),
		 connection->u.dir);
      close_sock(&connection->ctrl_sock);
      return err;
    } else
    {
      proz_debug(_("CWD ok."));
      done_with_response(connection);
    }
  } else
    proz_debug(_("CWD not needed."));

  err = ftp_setup_data_sock_1(connection, &passive_mode);
  if (err != FTPOK)
  {
    connection_change_status(connection, REMOTEFATAL);
    close_sock(&connection->ctrl_sock);
    return err;
  }

  /* do we need to REST */
  if (connection->remote_startpos > 0
      && connection->resume_support == TRUE)
  {
    err = ftp_rest(connection, connection->remote_startpos);
  }
  if (err != FTPOK)
  {
    if (err == FTPRESTFAIL)
      proz_debug
	  (_
	   ("I have a bug in my  code!!, check remote_starpos and resume_support values"));

    connection_change_status(connection, REMOTEFATAL);
    close_sock(&connection->ctrl_sock);
    return err;
  }

  err = ftp_retr(connection, connection->u.file);
  if (err != FTPOK)
  {
    proz_debug(_("RETR failed"));
    close_sock(&connection->ctrl_sock);
    connection_change_status(connection, REMOTEFATAL);
    return err;
  }

  err = ftp_setup_data_sock_2(connection, &passive_mode);
  if (err != FTPOK)
  {
    return err;
  }

  /* which routine to call */
  if (connection->main_file_size == -1)
    err =
	connection_retr_fsize_not_known(connection, buffer,
					sizeof(buffer));
  else
    err = connection_retr_fsize_known(connection, buffer, sizeof(buffer));

  close_sock(&connection->data_sock);
  close_sock(&connection->ctrl_sock);
  if (err == FILEGETOK)
    return FTPOK;

  else
    return err;
}


/* A genuine loop ;) It willed be called by the main thread, and 
this will handle all possible errors itself, retrying until the number 
of maximum tries for the  connection is realised or a error occurs which 
needs to be passed upwards for handling, such as LOGINFAIL
*/

uerr_t ftp_loop(connection_t * connection)
{
  boolean retrying_from_loop = FALSE;

  assert(connection->max_attempts >= 0);
  assert(connection->attempts >= 0);

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  do
  {
    if (connection->attempts > 0)
    {

      if (retrying_from_loop == TRUE)
      {
	connection_show_message(connection,
				_("Retrying..Attempt %d in %d seconds"),
				connection->attempts,
				connection->retry_delay.tv_sec);
	delay_ms(connection->retry_delay.tv_sec * 1000);
      }

      if (connection->resume_support == TRUE)
      {
	if (connection_load_resume_info(connection) == -1)
	{
	  connection_show_message(connection,
				  _
				  ("Error while attemting to process download file "));
	}
      } else
      {
	/*If we cant resume then reset the connections bytesreceived to 0 */
	connection->remote_bytes_received = 0;
      }
    }


    /*Push the handler which will cleanup any  sockets that are left open */
    pthread_cleanup_push(cleanup_socks, (void *) connection);


    connection->err = proz_ftp_get_file(connection);
    /*pop the handler */
    pthread_cleanup_pop(0);

    connection->attempts++;

    /*Should the error be handled at this level ? */
    if (!ftp_loop_handle_error(connection->err))
    {
      return connection->err;	/*If not return and the main thread will handle it */
    }



    switch (connection->err)
    {
    case FTPOK:
      connection_show_message(connection, _("Successfully got download"));
      return connection->err;
      break;

    default:
      connection_show_message(connection,
			      _("Error occured in connection..."));
      break;
    }
    retrying_from_loop = TRUE;
  }
  while ((connection->attempts < connection->max_attempts)
	 || connection->max_attempts == 0);


  connection_show_message(connection,
			  _
			  ("I have tried %d attempt(s) and have failed, aborting"),
			  connection->attempts);
  return connection->err;
}

/*Return true if it is a error which can be handled within the ftp_loop,
or false if it should be passed upwards so that the main download thread can 
restart it when necessary after processing other threads status too */
boolean ftp_loop_handle_error(uerr_t err)
{

  proz_debug("Error encountered in ftp_loop is %d", err);
  if (err == FTPNSFOD || err == FTPLOGREFUSED || err == FTPCONREFUSED
      || err == FWRITEERR || err == FOPENERR || err == FTPCWDFAIL
      || err == FTPRESTFAIL)
    return FALSE;
  else
    return TRUE;

}
