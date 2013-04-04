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

/* $Id: http-retr.c,v 1.20 2005/03/31 20:10:57 sean Exp $ */

#include "common.h"
#include "prozilla.h"
#include "connect.h"
#include "misc.h"
#include "url.h"
#include "netrc.h"
#include "debug.h"
#include "http.h"
#include "http-retr.h"

/* Will download a portion of/or the full file from the connection->url.
 */
uerr_t proz_http_get_file(connection_t * connection)
{
  uerr_t err;
  int remote_port_len;
  char *user, *passwd, *www_auth = NULL, *proxy_auth = NULL, *range =
      NULL, *location = NULL, *referer = NULL, *pragma_no_cache = NULL;
  char *request, *remote_port;
  netrc_entry *netrc_ent;
  char buffer[HTTP_BUFFER_SIZE];
  /*The http stats that were returned after the call with GET */
  http_stat_t hs_after_get;

  /* we want it to terminate immediately */
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  assert(connection->localfile != NULL);
  assert(connection->file_mode != NULL);

  /*clear the socks */
  connection->data_sock = 0;
  memset(&hs_after_get, 0, sizeof(hs_after_get));

  /* if there is nothing to download then return  */
  if (connection->status == COMPLETED)
  {
    pthread_mutex_lock(&connection->access_mutex);
    gettimeofday(&connection->time_begin, NULL);
    pthread_mutex_unlock(&connection->access_mutex);
    return HOK;
  }

  connection_change_status(connection, CONNECTING);

  if (http_use_proxy(connection))
  {
    connection_show_message(connection, _("Connecting to %s"),
			    connection->http_proxy->proxy_url.host);
    err = connect_to_server(&connection->data_sock,
			    connection->http_proxy->proxy_url.host,
			    connection->http_proxy->proxy_url.port,
			    &connection->xfer_timeout);
    if (err != NOCONERROR)
    {
      proz_debug(_("Error connecting to %s"),
		 connection->http_proxy->proxy_url.host);
      connection_change_status(connection, REMOTEFATAL);
      return err;
    }
  } else
  {
    connection_show_message(connection, _("Connecting to %s"),
			    connection->u.host);

    err = connect_to_server(&connection->data_sock, connection->u.host,
			    connection->u.port, &connection->xfer_timeout);
    if (err != NOCONERROR)
    {
      proz_debug(_("Error connecting to %s"), connection->u.host);
      connection_change_status(connection, REMOTEFATAL);
      return err;
    }
  }

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

  user = user ? user : "";
  passwd = passwd ? passwd : "";

  if (strlen(user) || strlen(passwd))
  {
    /* Construct the necessary header. */
    www_auth = get_basic_auth_str(user, passwd, "Authorization");
    proz_debug(_("Authenticating as user %s password %s"), user, passwd);
    proz_debug(_("Authentification string=%s"), www_auth);
  } else
    www_auth = 0;

  if (http_use_proxy(connection))
  {
    if (strlen(connection->http_proxy->username)
	|| strlen(connection->http_proxy->passwd))
      proxy_auth =
	  get_basic_auth_str(connection->http_proxy->username,
			     connection->http_proxy->passwd,
			     "Proxy-Authorization");
  }

  if (connection->u.port == 80)
  {
    remote_port = NULL;
    remote_port_len = 0;
  } else
  {
    remote_port = (char *) alloca(64);
    remote_port_len = sprintf(remote_port, ":%d", connection->u.port);
  }

  if (connection->hs.accept_ranges == 1)
  {
    range = (char *) alloca(18 + 64);
    sprintf(range, "Range: bytes=%lld-\r\n", connection->remote_startpos);
	proz_debug("Range = %lld Range = %s",connection->remote_startpos, range);
  }

  if (connection->u.referer)
  {
    referer = (char *) alloca(13 + strlen(connection->u.referer));
    sprintf(referer, "Referer: %s\r\n", connection->u.referer);
  }

  /* If we go through a proxy the request for the URL is different */
  if (http_use_proxy(connection))
  {
    location = (char *) alloca(strlen(connection->u.url) + 1);
    strcpy(location, connection->u.url);
  } else
  {
    location = (char *) alloca(strlen(connection->u.path) + 1);
    strcpy(location, connection->u.path);
  }

  /*Use no-cache directive for proxy servers? */
  if (http_use_proxy(connection)
      && (connection->http_no_cache || connection->attempts > 0))
  {
    pragma_no_cache = (char *) alloca(21);
    sprintf(pragma_no_cache, "Pragma: no-cache\r\n");
  }

  request = (char *) alloca(strlen(location)
			    + strlen(connection->user_agent)
			    + strlen(connection->u.host) + remote_port_len
			    + (range ? strlen(range) : 0)
			    + (referer ? strlen(referer) : 0)
			    + (www_auth ? strlen(www_auth) : 0)
			    + (proxy_auth ? strlen(proxy_auth) : 0)
			    + 64
			    +
			    (pragma_no_cache ? strlen(pragma_no_cache) :
			     0));

  /* TODO Add referrer tag. */
  sprintf(request,
	  "GET %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s%s\r\nAccept: */*\r\n%s%s%s%s%s\r\n",
	  location, connection->user_agent, connection->u.host,
	  remote_port ? remote_port : "", range ? range : "",
	  referer ? referer : "",
	  www_auth ? www_auth : "", proxy_auth ? proxy_auth : "",
	  pragma_no_cache ? pragma_no_cache : "");

  proz_debug("2 HTTP request = %s", request);

  connection_show_message(connection, _("Sending HTTP request"));
  err = http_fetch_headers(connection, &hs_after_get, request);

/* What hapenned ? */
  if (err != HOK)
  {
    proz_debug("2 http_fetch_headers err != HOK %d", err);
    /*Check if we authenticated using any user or password and if we 
       were kicked out, if so return HAUTHFAIL */
    if (err == HAUTHREQ && (strlen(user) || strlen(passwd)))
      err = HAUTHFAIL;
    /*
     * a error occured druing the process 
     */
    close_sock(&connection->data_sock);
    connection_change_status(connection, REMOTEFATAL);
    return err;
  }

  /*Check for the server lying about it being able to handle ranges */
  if (hs_after_get.contlen != -1)
  {
    if (connection->resume_support == TRUE)
    {
      if (hs_after_get.contlen !=
	  connection->main_file_size - connection->remote_startpos)
      {
	proz_debug("Error contlen does not match the requested range!");
	close_sock(&connection->data_sock);
	connection_change_status(connection, REMOTEFATAL);
	return CANTRESUME;
      }
    }
  }

/* which routine to call */
  if (connection->main_file_size == -1)
    err =
	connection_retr_fsize_not_known(connection, buffer,
					sizeof(buffer));
  else
    err = connection_retr_fsize_known(connection, buffer, sizeof(buffer));

  close_sock(&connection->data_sock);

  if (err == FILEGETOK)
    return HOK;
  else
  {
	  proz_debug("err != FILEGETOK %d", err);
	  return err;
  }
}



/* A genuine loop ;) It willed be called by the main thread, and 
this will handle all possible errors itself, retrying until the number 
of maximum tries for the  connection is realised
*/

uerr_t http_loop(connection_t * connection)
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
				_("Retrying...Attempt %d in %d seconds"),
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

    /*Push the handler which will cleanup any sockets that are left open */
    pthread_cleanup_push(cleanup_socks, (void *) connection);
    connection->err = proz_http_get_file(connection);
    /*pop the handler */
    pthread_cleanup_pop(0);

    connection->attempts++;

    /*Should the error be handled at this level ? */
    if (!http_loop_handle_error(connection->err))
    {
      connection_show_message(connection, _("Will be handled in main "));
      return connection->err;	/*If not return and the main thread will handle it */
    }

    switch (connection->err)
    {
    case HOK:
      connection_show_message(connection, _("Successfully got download"));
      return connection->err;
      break;

      /*TODO : What should we do if the file is not found, well pass it up to the main routine */

    default:
      connection_show_message(connection, proz_strerror(connection->err));
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


/*Return true if it is a error which can be handled within the htp_loop,
or false if it should be passed upwards so that the main download thread can 
restart it when necessary after processing other threads status too */
boolean http_loop_handle_error(uerr_t err)
{
  proz_debug("Error encountered in http_loop is %d", err);
  if (err == HTTPNSFOD || err == FWRITEERR || err == FOPENERR
      || err == CANTRESUME)
    return FALSE;
  else
    return TRUE;
}


/*
  I am writing a seperate function to handle FTP proxying through HTTP, I
  MHO whoever thought of using HTTP to proxy FTP is a shithead, 
  its such a PITA ;)
 */
uerr_t ftp_get_file_from_http_proxy(connection_t * connection)
{

  uerr_t err;
  int remote_port_len;
  char *user, *passwd, *www_auth = NULL, *proxy_auth = NULL, *range =
      NULL, *pragma_no_cache = NULL;

  char *request, *remote_port;
  netrc_entry *netrc_ent;
  char buffer[HTTP_BUFFER_SIZE];
  /*The http stats that were returned after the call with GET */
  http_stat_t hs_after_get;

  /* we want it to terminate immediately */
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  memset(&connection->hs, 0, sizeof(connection->hs));
  memset(&hs_after_get, 0, sizeof(hs_after_get));

  /* if there is nothing to download then return  */
  if (connection->status == COMPLETED)
  {
    pthread_mutex_lock(&connection->access_mutex);
    gettimeofday(&connection->time_begin, NULL);
    pthread_mutex_unlock(&connection->access_mutex);
    return FTPOK;
  }

  err = connect_to_server(&connection->data_sock,
			  connection->ftp_proxy->proxy_url.host,
			  connection->ftp_proxy->proxy_url.port,
			  &connection->xfer_timeout);

  if (err != NOCONERROR)
  {
    connection_show_message(connection, _("Error connecting to %s"),
			    connection->ftp_proxy->proxy_url.host);
    return err;
  }

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

  user = user ? user : "";
  passwd = passwd ? passwd : "";

  if (strlen(user) || strlen(passwd))
  {
    /* Construct the necessary header. */
    www_auth = get_basic_auth_str(user, passwd, "Authorization");
    proz_debug(_("Authenticating as user %s password %s"), user, passwd);
    proz_debug(_("Authentification string=%s"), www_auth);
  } else
    www_auth = 0;

  if (strlen(connection->ftp_proxy->username)
      || strlen(connection->ftp_proxy->passwd))
    proxy_auth =
	get_basic_auth_str(connection->ftp_proxy->username,
			   connection->ftp_proxy->passwd,
			   "Proxy-Authorization");

  remote_port = (char *) alloca(64);
  remote_port_len = sprintf(remote_port, ":%d", connection->u.port);

  if (connection->hs.accept_ranges == 1)
  {
    range = (char *) alloca(18 + 64);
    sprintf(range, "Range: bytes=%lld-\r\n", connection->remote_startpos);
  }


  /*Use no-cache directive for proxy servers? */
  if (http_use_proxy(connection)
      && (connection->http_no_cache || connection->attempts > 0))
  {
    pragma_no_cache = (char *) alloca(21);
    sprintf(pragma_no_cache, "Pragma: no-cache\r\n");
  }


  /*Referrer TAG should not be needed in FTP through HTTP proxy..right */


  request = (char *) alloca(strlen(connection->u.url)
			    + strlen(connection->user_agent)
			    + strlen(connection->u.host) + remote_port_len
			    + (range ? strlen(range) : 0)
			    + (www_auth ? strlen(www_auth) : 0)
			    + (proxy_auth ? strlen(proxy_auth) : 0)
			    + 64
			    +
			    (pragma_no_cache ? strlen(pragma_no_cache) :
			     0));


  /* TODO Add referrer tag. */
  sprintf(request,
	  "GET %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s%s\r\nAccept: */*\r\n%s%s%s%s\r\n",
	  connection->u.url, connection->user_agent, connection->u.host,
	  remote_port ? remote_port : "", range ? range : "",
	  www_auth ? www_auth : "", proxy_auth ? proxy_auth : "",
	  pragma_no_cache ? pragma_no_cache : "");

  proz_debug("HTTP request = %s", request);

  connection_show_message(connection, _("Sending HTTP request"));
  err = http_fetch_headers(connection, &hs_after_get, request);



  if (err == HAUTHREQ)
  {
    connection_change_status(connection, LOGINFAIL);
    return FTPLOGREFUSED;
  } else if (err == HTTPNSFOD)
  {
    connection_change_status(connection, REMOTEFATAL);
    return FTPNSFOD;
  } else if (err != HOK)
  {
    connection_change_status(connection, REMOTEFATAL);
    return FTPERR;
  }

  /*Check for the server lying about it being able to handle ranges */
  if (hs_after_get.contlen != -1)
  {
    if (connection->resume_support == TRUE)
    {
      if (hs_after_get.contlen !=
	  connection->main_file_size - connection->remote_startpos)
      {
	proz_debug("Error contlen does not match the requested range!");
	connection_change_status(connection, REMOTEFATAL);
	return CANTRESUME;
      }
    }
  }

/* which routine to call */
  if (connection->main_file_size == -1)
    err =
	connection_retr_fsize_not_known(connection, buffer,
					sizeof(buffer));
  else
    err = connection_retr_fsize_known(connection, buffer, sizeof(buffer));

  close_sock(&connection->data_sock);

  if (err == FILEGETOK)
    return FTPOK;
  else
    return err;

}
