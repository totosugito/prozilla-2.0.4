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

/* HTTP support. */

/* $Id: http.c,v 1.22 2005/03/31 20:10:57 sean Exp $ */


#include "common.h"
#include "prozilla.h"
#include "misc.h"
#include "connect.h"
#include "debug.h"
#include "http.h"


/* Some status code validation macros: */
#define H_20X(x)        (((x) >= 200) && ((x) < 300))
#define H_PARTIAL(x)    ((x) == HTTP_PARTIAL_CONTENTS)
#define H_REDIRECTED(x) (((x) == HTTP_MOVED_PERMANENTLY) || ((x) == HTTP_MOVED_TEMPORARILY))


/* HTTP/1.0 status codes from RFC1945, given for reference. */

/* Successful 2xx. */
#define HTTP_OK                200
#define HTTP_CREATED           201
#define HTTP_ACCEPTED          202
#define HTTP_NO_CONTENT        204
#define HTTP_PARTIAL_CONTENTS  206

/* Redirection 3xx. */
#define HTTP_MULTIPLE_CHOICES  300
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_MOVED_TEMPORARILY 302
#define HTTP_NOT_MODIFIED      304

/* Client error 4xx. */
#define HTTP_BAD_REQUEST       400
#define HTTP_UNAUTHORIZED      401
#define HTTP_FORBIDDEN         403
#define HTTP_NOT_FOUND         404

/* Server errors 5xx. */
#define HTTP_INTERNAL          500
#define HTTP_NOT_IMPLEMENTED   501
#define HTTP_BAD_GATEWAY       502
#define HTTP_UNAVAILABLE       503
#define HTTP_GATEWAY_TIMEOUT   504

#define DYNAMIC_LINE_BUFFER 40


/******************************************************************************
 ...
******************************************************************************/
int buf_readchar(int fd, char *ret, struct timeval *timeout)
{
  int res;

  res = krecv(fd, ret, 1, 0, timeout);

  if (res <= 0)
    return res;

  return 1;
}

/******************************************************************************
 This is similar to buf_readchar(), only it doesn't move the buffer position.
******************************************************************************/
int buf_peek(int fd, char *ret, struct timeval *timeout)
{
  int res;

  res = krecv(fd, ret, 1, MSG_PEEK, timeout);

  if (res <= 0)
    return res;

  return 1;
}

/******************************************************************************
 Function to fetch a header from socket/file descriptor fd. The header may be
 of arbitrary length, since the function allocates as much memory as necessary
 for the header to fit. Most errors are handled.

 The header may be terminated by LF or CRLF.  If the character after LF is SP
 or HT (horizontal tab), the header spans to another line (continuation
 header), as per RFC2068.

 The trailing CRLF or LF are stripped from the header, and it is
 zero-terminated.
******************************************************************************/
uerr_t fetch_next_header(int fd, char **hdr, struct timeval * timeout)
{
  int i, bufsize, res;
  char next;

  bufsize = DYNAMIC_LINE_BUFFER;
  *hdr = kmalloc(bufsize);

  for (i = 0; 1; i++)
  {
    if (i > bufsize - 1)
      *hdr = krealloc(*hdr, (bufsize <<= 1));

    res = buf_readchar(fd, *hdr + i, timeout);

    if (res == 1)
    {
      if ((*hdr)[i] == '\n')
      {
	if (!(i == 0 || (i == 1 && (*hdr)[0] == '\r')))
	{
	  /* If the header is non-empty, we need to check if it continues on
	     to the other line. We do that by getting the next character
	     without actually downloading it (i.e. peeking it). */
	  res = buf_peek(fd, &next, timeout);

	  if (res == 0)
	    return HEOF;
	  else if (res == -1)
	    return HERR;

	  /* If the next character is SP or HT, just continue. */
	  if (next == '\t' || next == ' ')
	    continue;
	}

	/* The header ends. */
	(*hdr)[i] = '\0';

	/* Get rid of '\r'. */
	if (i > 0 && (*hdr)[i - 1] == '\r')
	  (*hdr)[i - 1] = '\0';

	break;
      }
    } else if (res == 0)
      return HEOF;
    else
      return HERR;
  }

  return HOK;
}

/******************************************************************************
 ...
******************************************************************************/
int hparsestatline(const char *hdr, const char **rp)
{
  int mjr, mnr;			/* HTTP major and minor version. */
  int statcode;			/* HTTP status code. */
  const char *p;

  *rp = NULL;
  /* The standard format of HTTP-Version is: HTTP/x.y, where x is major
     version, and y is minor version. */
  if (strncmp(hdr, "HTTP/", 5) != 0)
    return -1;

  hdr += 5;
  p = hdr;

  for (mjr = 0; isdigit(*hdr); hdr++)
    mjr = 10 * mjr + (*hdr - '0');

  if (*hdr != '.' || p == hdr)
    return -1;

  ++hdr;
  p = hdr;

  for (mnr = 0; isdigit(*hdr); hdr++)
    mnr = 10 * mnr + (*hdr - '0');

  if (*hdr != ' ' || p == hdr)
    return -1;

  /* Wget will accept only 1.0 and higher HTTP-versions. The value of minor
     version can be safely ignored. */
  if (mjr < 1)
    return -1;

  /* Skip the space. */
  ++hdr;
  if (!(isdigit(*hdr) && isdigit(hdr[1]) && isdigit(hdr[2])))
    return -1;

  statcode = 100 * (*hdr - '0') + 10 * (hdr[1] - '0') + (hdr[2] - '0');
  /* RFC2068 requires a SPC here, even if there is no reason-phrase. As some
     servers/CGI are (incorrectly) setup to drop the SPC, we'll be liberal
     and allow the status line to end here. */
  if (hdr[3] != ' ')
  {
    if (!hdr[3])
      *rp = hdr + 3;
    else
      return -1;
  } else
    *rp = hdr + 4;

  return statcode;
}

/******************************************************************************
 Skip LWS (linear white space), if present. Returns number of characters to
 skip.
******************************************************************************/
int hskip_lws(const char *hdr)
{
  int i;

  for (i = 0;
       *hdr == ' ' || *hdr == '\t' || *hdr == '\r' || *hdr == '\n'; ++hdr)
    ++i;

  return i;
}

/******************************************************************************
 Return the content length of the document body, if this is Content-length
 header, -1 otherwise.
******************************************************************************/
off_t hgetlen(const char *hdr)
{
  const int l = 15;		/* strlen("content-length:"). */
  off_t len;

  if (strncasecmp(hdr, "content-length:", l))
    return -1;

  hdr += (l + hskip_lws(hdr + l));
  if (!*hdr)
    return -1;

  if (!isdigit(*hdr))
    return -1;

  for (len = 0; isdigit(*hdr); hdr++)
    len = 10 * len + (*hdr - '0');

  proz_debug("contenlen %s  contentlen %lld",*hdr,len);
  return len;
}

/******************************************************************************
 Return the content-range in bytes, as returned by the server, if this is
 Content-range header, -1 otherwise.
******************************************************************************/
off_t hgetrange(const char *hdr)
{
  const int l = 14;		/* strlen("content-range:"). */
  off_t len;

  if (strncasecmp(hdr, "content-range:", l))
    return -1;

  hdr += (l + hskip_lws(hdr + l));
  if (!*hdr)
    return -1;

  /* Nutscape proxy server sends content-length without "bytes" specifier,
     which is a breach of HTTP/1.1 draft. But heck, I must support it... */
  if (!strncasecmp(hdr, "bytes", 5))
  {
    hdr += 5;
    hdr += hskip_lws(hdr);
    if (!*hdr)
      return -1;
  }

  if (!isdigit(*hdr))
    return -1;

  for (len = 0; isdigit(*hdr); hdr++)
    len = 10 * len + (*hdr - '0');

  proz_debug("range %s  range %lld",*hdr,len);
  return len;
}

/******************************************************************************
 Returns a malloc-ed copy of the location of the document, if the string hdr
 begins with LOCATION_H, or NULL.
******************************************************************************/
char *hgetlocation(const char *hdr)
{
  const int l = 9;		/* strlen("location:"). */

  if (strncasecmp(hdr, "location:", l))
    return NULL;

  hdr += (l + hskip_lws(hdr + l));

  return kstrdup(hdr);
}

/******************************************************************************
 Returns a malloc-ed copy of the last-modified date of the document, if the
 hdr begins with LASTMODIFIED_H.
******************************************************************************/
char *hgetmodified(const char *hdr)
{
  const int l = 14;		/* strlen("last-modified:"). */

  if (strncasecmp(hdr, "last-modified:", l))
    return NULL;

  hdr += (l + hskip_lws(hdr + l));

  return kstrdup(hdr);
}

/******************************************************************************
 Returns 0 if the header is accept-ranges, and it contains the word "none",
 -1 if there is no accept ranges, 1 is there is accept-ranges and it is not
 none.
******************************************************************************/
int hgetaccept_ranges(const char *hdr)
{
  const int l = 14;		/* strlen("accept-ranges:"). */

  if (strncasecmp(hdr, "accept-ranges:", l))
    return -1;

  hdr += (l + hskip_lws(hdr + l));

  if (strstr(hdr, "none"))
    return 0;
  else
    return 1;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t http_fetch_headers(connection_t * connection, http_stat_t * hs,
			  char *command)
{
  uerr_t err;
  int num_written, hcount, statcode, all_length;
  off_t contlen, contrange;
  char *hdr, *type, *all_headers;
  const char *error;

  hs->len = 0L;
  hs->contlen = -1;
  hs->accept_ranges = -1;
  hs->res = -1;
  hs->newloc = NULL;
  hs->remote_time = NULL;
  hs->error = NULL;

  num_written = ksend(connection->data_sock, command, strlen(command), 0,
		      &connection->xfer_timeout);
  if (num_written != strlen(command))
  {
    proz_debug(_("Failed writing HTTP request"));
    return WRITEERR;
  }

  all_headers = NULL;
  all_length = 0;
  contlen = contrange = -1;
  statcode = -1;
  type = NULL;

  /* Header-fetching loop. */
  hcount = 0;

  for (;;)
  {
    ++hcount;

    /* Get the header. */
    err = fetch_next_header(connection->data_sock, &hdr,
			    &connection->xfer_timeout);

    proz_debug(_("Header = %s"), hdr);

    if (err == HEOF)
    {
      proz_debug(_("End of file while parsing headers"));

      kfree(hdr);
      if (type)
	kfree(type);
      if (all_headers)
	kfree(all_headers);

      return HEOF;
    } else if (err == HERR)
    {
      proz_debug(_("Read error in headers"));

      kfree(hdr);
      if (type)
	kfree(type);
      if (all_headers)
	kfree(all_headers);

      return HERR;
    }

    /* Exit on empty header. */
    if (!*hdr)
    {
      kfree(hdr);
      break;
    }

    /* Check for errors documented in the first header. */
    if (hcount == 1)
    {
      statcode = hparsestatline(hdr, &error);
      hs->statcode = statcode;

      /* Store the descriptive response. */
      if (statcode == -1)	/* Malformed request. */
	hs->error = kstrdup(_("UNKNOWN"));
      else if (!*error)
	hs->error = kstrdup(_("(no description)"));
      else
	hs->error = kstrdup(error);
    }

    if (contlen == -1)
    {
      contlen = hgetlen(hdr);
      hs->contlen = contlen;
    }

    /* If the server specified a new location then lets store it. */

    if (!hs->newloc)
      hs->newloc = hgetlocation(hdr);

    if (!hs->remote_time)
      hs->remote_time = hgetmodified(hdr);

    if (hs->accept_ranges == -1)
      hs->accept_ranges = hgetaccept_ranges(hdr);

    if (!hs->newloc)
      hs->newloc = hgetlocation(hdr);

    kfree(hdr);
  }

  if (H_20X(statcode))
    return HOK;

  if (H_REDIRECTED(statcode) || statcode == HTTP_MULTIPLE_CHOICES)
  {
    /* RFC2068 says that in case of the 300 (multiple choices) response, the
       server can output a preferred URL through `Location' header; otherwise,
       the request should be treated like GET. So, if the location is set, it
       will be a redirection; otherwise, just proceed normally. */
    if (statcode == HTTP_MULTIPLE_CHOICES && !hs->newloc)
      return HOK;
    else
    {
      if (all_headers)
	kfree(all_headers);
      if (type)
	kfree(type);
      return NEWLOCATION;
    }
  }

  if (statcode == HTTP_UNAUTHORIZED)
    return HAUTHREQ;

  if (statcode == HTTP_NOT_FOUND)
    return HTTPNSFOD;

  if (statcode == HTTP_INTERNAL)
    return INTERNALSERVERR;

  if (statcode == HTTP_NOT_IMPLEMENTED)
    return UNKNOWNREQ;

  if (statcode == HTTP_BAD_GATEWAY)
    return BADGATEWAY;

  if (statcode == HTTP_UNAVAILABLE)
    return SERVICEUNAVAIL;

  if (statcode == HTTP_GATEWAY_TIMEOUT)
    return GATEWAYTIMEOUT;

  return HERR;
}

/******************************************************************************
 ...
******************************************************************************/
char *get_basic_auth_str(char *user, char *passwd, char *auth_header)
{
  char *p1, *p2, *ret;
  int len = strlen(user) + strlen(passwd) + 1;
  int b64len = 4 * ((len + 2) / 3);

  p1 = kmalloc(len + 1);
  sprintf(p1, "%s:%s", user, passwd);
  p2 = kmalloc(b64len + 1);

  /* Encode username:passwd to base64. */
  base64_encode(p1, p2, len);
  ret = kmalloc(strlen(auth_header) + b64len + 11);
  sprintf(ret, "%s: Basic %s\r\n", auth_header, p2);

  kfree(p1);
  kfree(p2);

  return ret;
}

/******************************************************************************
 ...
******************************************************************************/
boolean http_use_proxy(connection_t * connection)
{
  return (connection->http_proxy && connection->http_proxy->use_proxy
	  && connection->http_proxy->proxy_url.url) ? TRUE : FALSE;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t proz_http_get_url_info(connection_t * connection)
{
  uerr_t err;
  int remote_port_len;
  char *user, *passwd, *www_auth = NULL, *proxy_auth = NULL,
      *referer = NULL, *location = NULL, *pragma_no_cache = NULL;
  char *request, *remote_port;
  netrc_entry *netrc_ent;

  memset(&connection->hs, 0, sizeof(connection->hs));

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
      connection_show_message(connection, _("Error connecting to %s"),
			      connection->http_proxy->proxy_url.host);
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
      connection_show_message(connection, _("Error connecting to %s"),
			      connection->u.host);
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
			    + (referer ? strlen(referer) : 0)
			    + (www_auth ? strlen(www_auth) : 0)
			    + (proxy_auth ? strlen(proxy_auth) : 0) + 64
			    +
			    (pragma_no_cache ? strlen(pragma_no_cache) :
			     0));

  sprintf(request,
	  "GET %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s%s\r\nAccept: */*\r\n%s%s%s%s\r\n",
	  location, connection->user_agent, connection->u.host,
	  remote_port ? remote_port : "",
	  referer ? referer : "",
	  www_auth ? www_auth : "", proxy_auth ? proxy_auth : "",
	  pragma_no_cache ? pragma_no_cache : "");

  proz_debug("HTTP request = %s", request);

  connection_show_message(connection, _("Sending HTTP request"));
  err = http_fetch_headers(connection, &connection->hs, request);

  close_sock(&connection->data_sock);

  if (err == HOK)
  {
    connection->main_file_size = connection->hs.contlen;
    if (connection->hs.accept_ranges == 1)
      connection->resume_support = TRUE;
    else if (connection->hs.accept_ranges == -1)
      connection->resume_support = FALSE;
  }


  connection->file_type = REGULAR_FILE;
  return err;
}


/*Loops for connection->attempts */
uerr_t http_get_url_info_loop(connection_t * connection)
{

  pthread_mutex_lock(&connection->access_mutex);
  connection->running = TRUE;
  pthread_mutex_unlock(&connection->access_mutex);
  assert(connection->attempts >= 0);

  do
  {
    if (connection->attempts > 0 && connection->err != NEWLOCATION)
    {

      connection_show_message(connection,
			      _("Retrying...Attempt %d in %d seconds"),
			      connection->attempts,
			      connection->retry_delay.tv_sec);
      delay_ms(connection->retry_delay.tv_sec * 1000);
    }

    /*Push the handler which will cleanup any sockets that are left open */
    pthread_cleanup_push(cleanup_socks, (void *) connection);
    connection->err = proz_http_get_url_info(connection);
    /*pop the handler */
    pthread_cleanup_pop(0);

    connection->attempts++;

    switch (connection->err)
    {
    case HOK:
      connection_show_message(connection, _("Successfully got info"));
      pthread_mutex_lock(&connection->access_mutex);
      connection->running = FALSE;
      pthread_mutex_unlock(&connection->access_mutex);
      return connection->err;
      break;

    case NEWLOCATION:
      return connection->err;
      break;

    case HTTPNSFOD:
      connection_show_message(connection, _("File not found!"));
      pthread_mutex_lock(&connection->access_mutex);
      connection->running = FALSE;
      pthread_mutex_unlock(&connection->access_mutex);
      return connection->err;
      break;

    default:
      connection_show_message(connection, proz_strerror(connection->err));
      break;
    }

  }
  while ((connection->attempts < connection->max_attempts)
	 || connection->max_attempts == 0);


  connection_show_message(connection,
			  _
			  ("I have tried %d attempt(s) and have failed, aborting"),
			  connection->attempts);
  pthread_mutex_lock(&connection->access_mutex);
  connection->running = FALSE;
  pthread_mutex_unlock(&connection->access_mutex);
  return connection->err;
}



/*
  I am writing a seperate function to handle FTP proxying through HTTP, I
  MHO whoever thought of using HTTP to proxy FTP is a shithead, 
  its such a PITA ;)
 */

uerr_t ftp_get_url_info_from_http_proxy(connection_t * connection)
{

  uerr_t err;
  int remote_port_len;
  char *user, *passwd, *www_auth = NULL, *proxy_auth =
      NULL, *pragma_no_cache = NULL;

  char *request, *remote_port;
  netrc_entry *netrc_ent;

  memset(&connection->hs, 0, sizeof(connection->hs));

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
			    + (www_auth ? strlen(www_auth) : 0)
			    + (proxy_auth ? strlen(proxy_auth) : 0)
			    + 64
			    +
			    (pragma_no_cache ? strlen(pragma_no_cache) :
			     0));

  sprintf(request,
	  "GET %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s%s\r\nAccept: */*\r\n%s%s%s\r\n",
	  connection->u.url, connection->user_agent, connection->u.host,
	  remote_port,
	  www_auth ? www_auth : "", proxy_auth ? proxy_auth : "",
	  pragma_no_cache ? pragma_no_cache : "");

  proz_debug("HTTP request = %s", request);


  err = http_fetch_headers(connection, &connection->hs, request);

  close_sock(&connection->data_sock);

  /*Convert the error code to the equivalent FTP one if possible */

  if (err == HOK)
  {
    connection->main_file_size = connection->hs.contlen;
    if (connection->hs.accept_ranges == 1)
      connection->resume_support = TRUE;
    else if (connection->hs.accept_ranges == -1)
      connection->resume_support = FALSE;
    return FTPOK;
  }


  if (err == HAUTHREQ)
    return FTPLOGREFUSED;
  else if (err == HTTPNSFOD)
    return FTPNSFOD;


  /*  connection->file_type = REGULAR_FILE; */
  return FTPERR;

}
