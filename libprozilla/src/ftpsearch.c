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

#include "common.h"
#include "prozilla.h"
#include "misc.h"
#include "connect.h"
#include "debug.h"
#include "http.h"
#include "ftpsearch.h"


urlinfo *prepare_lycos_url(ftps_request_t * request, char *ftps_loc,
			   int num_req_mirrors);
urlinfo *prepare_filesearching_url(ftps_request_t * request, char *ftps_loc,
				   int num_req_mirrors);

uerr_t parse_lycos_html_mirror_list(ftps_request_t * request, char *p);
uerr_t parse_filesearching_html_mirror_list(ftps_request_t * request, char *p);

uerr_t get_mirror_info(connection_t * connection, char **ret_buf);

uerr_t parse_html_mirror_list(ftps_request_t * request, char *p);

char *find_ahref(char *buf);
char *find_end(char *buf);
char *find_closed_a(char *buf);
char *get_string_ahref(char *buf, char *out, size_t out_size);

char *grow_buffer(char *buf_start, char *cur_pos, int *buf_len,
		  int data_len);
uerr_t get_complete_mirror_list(ftps_request_t * request);
ftp_mirror_t *reprocess_mirror_list(ftp_mirror_t * mirrors,
				    int *num_servers);


ftps_request_t * proz_ftps_request_init(
			       urlinfo * requested_url, off_t file_size,
			       char *ftps_loc,
			       ftpsearch_server_type_t server_type,
			       int num_req_mirrors)
{
  urlinfo *url;
  ftps_request_t * request;

  assert(requested_url);
  assert(requested_url->file);

  request=kmalloc(sizeof(ftps_request_t));
  memset(request, 0, sizeof(ftps_request_t));
  request->file_name = strdup(requested_url->file);
  request->requested_url = proz_copy_url(requested_url);
  request->file_size = file_size;
  request->server_type = server_type;
  pthread_mutex_init(&request->access_mutex, 0);

  switch (server_type)
  {
  case LYCOS:
    url = prepare_lycos_url(request, ftps_loc, num_req_mirrors);
    if (url == 0)
      proz_die("Bad URl specification");
 
    /*NOTE  pasing zero as the status change mutes as we dont need it here */
      request->connection=proz_connection_init(url,0);


    break;
  case FILESEARCH_RU:
    url = prepare_filesearching_url(request, ftps_loc, num_req_mirrors);
    if (url == 0)
      proz_die("Bad URl specification");
 
    /*NOTE  pasing zero as the status change mutes as we dont need it here */
      request->connection=proz_connection_init(url,0);
    break;
  default:
    proz_debug("Unsupported FTP search server type");
    proz_die("Unsupported FTP search server type");
  }

  return request;
}


urlinfo *prepare_lycos_url(ftps_request_t * request, char *ftps_loc,
			   int num_req_mirrors)
{
  urlinfo *url;
  uerr_t err;
  char *lycos_url_buf;
  int lycos_url_len = strlen(ftps_loc) +
      strlen
      ("?form=advanced&query=%s&doit=Search&type=Exact+search&hits=%d&matches=&hitsprmatch=&limdom=&limpath=&limsize1=%d&limsize2=%d&limtime1=&limtime2=&f1=Host&f2=Path&f3=Size&f4=-&f5=-&f6=-&header=none&sort=none&trlen=20");


  assert(request->file_name);

  url = (urlinfo *) kmalloc(sizeof(urlinfo));

  /* Okay lets now construct the URL we want to do lycos */
  lycos_url_buf =
      (char *) kmalloc(lycos_url_len + strlen(request->file_name) + 300);

  sprintf(lycos_url_buf,
	  "%s?form=advanced&query=%s&doit=Search&type=Exact+search&hits=%d&matches=&hitsprmatch=&limdom=&limpath=&limsize1=%Ld&limsize2=%lld&f1=Host&f2=Path&f3=Size&f4=-&f5=-&f6=-&header=none&sort=none&trlen=20",
	  ftps_loc, request->file_name, num_req_mirrors,
	  request->file_size, request->file_size);

  /* Debugging purposes */
  /*sprintf(lycos_url_buf,"localhost/search.html");    */


  proz_debug("ftpsearch url= %s\n", lycos_url_buf);

  err = proz_parse_url(lycos_url_buf, url, 0);

  if (err != URLOK)
    return 0;
  else
    return url;
}



urlinfo *prepare_filesearching_url(ftps_request_t * request, char *ftps_loc,
			   int num_req_mirrors)
{
  urlinfo *url;
  uerr_t err;
  char *filesearching_url_buf;
  int filesearching_url_len = strlen(ftps_loc) +
      strlen
      ("?q=ddd-3.3.tar.bz2&l=en&t=f&e=on&m=20&o=n&s=on&s1=4811576&s2=4811576&d=&p=&p2=&x=10&y=14");


  assert(request->file_name);

  url = (urlinfo *) kmalloc(sizeof(urlinfo));

  /* Okay lets now construct the URL we want to do lycos */
  filesearching_url_buf =
      (char *) kmalloc(filesearching_url_len + strlen(request->file_name) + 300);

  sprintf(filesearching_url_buf,
	  "%s?q=%s&l=en&t=f&e=on&m=%d&o=n&s=on&s1=%Ld&s2=%Ld&d=&p=&p2=&x=10&y=14",
	  ftps_loc, request->file_name, num_req_mirrors,
	  request->file_size, request->file_size);

  /* Debugging purposes */
  /*  sprintf(filesearching_url_buf,"localhost/fs.html");   */


  proz_debug("ftpsearch url= %s\n", filesearching_url_buf);

  err = proz_parse_url(filesearching_url_buf, url, 0);

  if (err != URLOK)
    return 0;
  else
    return url;
}



uerr_t parse_html_mirror_list(ftps_request_t * request, char *p)
{

  switch (request->server_type)
  {
  case LYCOS:
    return parse_lycos_html_mirror_list(request, p);
    break;
  case FILESEARCH_RU:
    return parse_filesearching_html_mirror_list(request, p);
    break;
  default:
    proz_debug("Unsupported FTP search server type");
    proz_die("Unsupported FTP search server type");
  }
  return MIRPARSEFAIL;
}

uerr_t parse_lycos_html_mirror_list(ftps_request_t * request, char *p)
{

  struct ftp_mirror **pmirrors = &request->mirrors;
  int *num_servers = &request->num_mirrors;
  char *p1, *p2, *i = 0, *j;
  ftp_mirror_t *ftp_mirrors;
  int k, num_ah = 0, num_pre = 0;
  char buf[1024];


  if (strstr(p, "No hits") != 0)
  {
    *num_servers = 0;
    return MIRINFOK;
  }

/*Check the number of PRE tags */
  p1 = p;
  while (((p1 = strstr(p1, "<PRE>")) != NULL) && p1)
  {
    num_pre++;
    p1 += 5;
  }

  proz_debug("Number of PRE tags found = %d\n", num_pre);

  if (num_pre == 1)
  {

    if ((i = strstr(p, "<PRE>")) == NULL)
    {
      proz_debug("nomatches found");
      return MIRPARSEFAIL;
    }

    proz_debug("match at %d found", i - p);

    if ((j = strstr(p, "</PRE>")) == NULL)
    {
      proz_debug("nomatches found");
      return MIRPARSEFAIL;
    }
  } else
  {
    /*search for the reported hits text */
    char *rep_hits;
    int prior_pres = 0;

    if ((rep_hits = strstr(p, "reported hits")) == NULL)
    {
      proz_debug("no reported hits found");
      return MIRPARSEFAIL;
    }

    /* Okay so we got the position after the results, lets see how many PRE tags were there before it */

    p1 = p;
    while (((p1 = strstr(p1, "<PRE>")) < rep_hits) && p1)
    {
      prior_pres++;
      p1 += 5;
    }
    /* now get the location of the PRE before the output */

    p1 = p;
    i = 0;
    while (prior_pres--)
    {
      p1 = strstr(p1, "<PRE>");
      p1 += 5;
    }
    i = p1 - 5;

    /*now find the </PRE>  tag which is after the results */
    j = strstr(i, "</PRE>");

    if (j == NULL)
    {
      proz_debug("The expected </PRE> tag was not found!\n");
      return MIRPARSEFAIL;
    }
  }

  p1 = kmalloc((j - i - 5) + 100);
  strncpy(p1, i + 5, j - i - 5);

  p1[j - i - 5 + 1] = 0;
  proz_debug("\nstring len= %ld", strlen(p1));

  p2 = p1;

  while ((i = strstr(p1, "<A HREF=")) != NULL)
  {
    num_ah++;
    p1 = i + 8;
  }

  proz_debug("\n%d ahrefs found\n", num_ah);

  if (num_ah == 0)
  {
    *num_servers = 0;
    return MIRINFOK;
  }



  *num_servers = num_ah / 3;
  proz_debug("%d servers found\n", *num_servers);

  /* Allocate +1 because we need to add the user specified server as well
   */
  ftp_mirrors =
      (ftp_mirror_t *) kmalloc(sizeof(ftp_mirror_t) *
			       ((*num_servers) + 1));

  for (k = 0; k < *num_servers; k++)
  {
    memset(&(ftp_mirrors[k]), 0, sizeof(ftp_mirror_t));
    p2 = get_string_ahref(p2, buf, sizeof(buf)/sizeof(char));
    ftp_mirrors[k].server_name = kstrdup(buf);
    p2 = get_string_ahref(p2, buf, sizeof(buf)/sizeof(char));

    ftp_mirrors[k].paths = kmalloc(sizeof(mirror_path_t));

    //      ftp_mirrors[k].paths=kmalloc (sizeof (char *));
    ftp_mirrors[k].num_paths = 1;

    /*Strip any leading slash in the path name if preent */
    if (*buf == '/')
      ftp_mirrors[k].paths[0].path = kstrdup(buf + 1);
    else
      ftp_mirrors[k].paths[0].path = kstrdup(buf);

    p2 = get_string_ahref(p2, buf,sizeof(buf)/sizeof(char));
    ftp_mirrors[k].file_name = kstrdup(buf);
  }

  /* add the users server to the end if it is a ftp server */
  if (request->requested_url->proto == URLFTP)
  {
    memset(&(ftp_mirrors[k]), 0, sizeof(ftp_mirror_t));
    ftp_mirrors[k].server_name = kstrdup(request->requested_url->host);

    ftp_mirrors[k].paths = kmalloc(sizeof(mirror_path_t));
    ftp_mirrors[k].num_paths = 1;

    if (*(request->requested_url->dir))
      ftp_mirrors[k].paths[0].path = kstrdup(request->requested_url->dir);
    else
      ftp_mirrors[k].paths[0].path = kstrdup("");

    ftp_mirrors[k].file_name = kstrdup(request->requested_url->file);
    *num_servers += 1;
  }

  proz_debug("%d servers found\n", *num_servers);

  for (k = 0; k < *num_servers; k++)
  {

    ftp_mirrors[k].full_name =
	(char *) kmalloc(strlen(ftp_mirrors[k].server_name) +
			 strlen(ftp_mirrors[k].paths[0].path) +
			 strlen(ftp_mirrors[k].file_name) + 13);
    sprintf(ftp_mirrors[k].full_name, "%s%s:21/%s%s%s", "ftp://",
	    ftp_mirrors[k].server_name, ftp_mirrors[k].paths[0].path, "/",
	    ftp_mirrors[k].file_name);

    proz_debug("%s\n", ftp_mirrors[k].full_name);
  }


  *pmirrors = reprocess_mirror_list(ftp_mirrors, num_servers);
  /*    *pmirrors = ftp_mirrors; */
  return MIRINFOK;
}


uerr_t parse_filesearching_html_mirror_list(ftps_request_t * request, char *p)
{

  struct ftp_mirror **pmirrors = &request->mirrors;
  int *num_servers = &request->num_mirrors;
  char *p1, *p2, *i = 0, *j;
  ftp_mirror_t *ftp_mirrors;
  int k, num_ah = 0, num_pre = 0;
  char buf[1024];


  if (strstr(p, "not found") != 0)
  {
    *num_servers = 0;
    return MIRINFOK;
  }

/*Check the number of PRE tags */
  p1 = p;
  while (((p1 = strstr(p1, "<pre")) != NULL) && p1)
  {
    num_pre++;
    p1 += 4;
  }

  proz_debug("Number of PRE tags found = %d\n", num_pre);

  if (num_pre == 1)
  {

    if ((i = strstr(p, "<pre class=list>")) == NULL)
    {
      proz_debug("nomatches found");
      return MIRPARSEFAIL;
    }

    proz_debug("match at %d found", i - p);

    if ((j = strstr(p, "</pre>")) == NULL)
    {
      proz_debug("nomatches found");
      return MIRPARSEFAIL;
    }
  } else
  {
    /*search for the reported hits text */
    char *rep_hits;
    int prior_pres = 0;

    if ((rep_hits = strstr(p, "reported hits")) == NULL)
    {
      proz_debug("no reported hits found");
      return MIRPARSEFAIL;
    }

    /* Okay so we got the position after the results, lets see how many PRE tags were there before it */

    p1 = p;
    while (((p1 = strstr(p1, "<pre")) < rep_hits) && p1)
    {
      prior_pres++;
      p1 += 5;
    }
    /* now get the location of the PRE before the output */

    p1 = p;
    i = 0;
    while (prior_pres--)
    {
      p1 = strstr(p1, "<pre class=list>");
      p1 += 5;
    }
    i = p1 - 5;

    /*now find the </PRE>  tag which is after the results */
    j = strstr(i, "</pre>");

    if (j == NULL)
    {
      proz_debug("The expected </PRE> tag was not found!\n");
      return MIRPARSEFAIL;
    }
  }

  p1 = kmalloc((j - i - 16) + 100);
  strncpy(p1, i + 16, j - i - 16);

  proz_debug("\nstring len= %ld", strlen(p1));
  proz_debug("\nstring value= %s", p1);

  p1[j - i - 16 + 1] = 0;

  p2 = p1;

  while ((i = strstr(p1, "<a href=")) != NULL)
  {
    num_ah++;
    p1 = i + 8;
  }

  proz_debug("\n%d ahrefs found\n", num_ah);

  if (num_ah == 0)
  {
    *num_servers = 0;
    return MIRINFOK;
  }



  *num_servers = num_ah / 3;
  proz_debug("%d servers found\n", *num_servers);

  /* Allocate +1 because we need to add the user specified server as well
   */
  ftp_mirrors =
      (ftp_mirror_t *) kmalloc(sizeof(ftp_mirror_t) *
			       ((*num_servers) + 1));

  for (k = 0; k < *num_servers; k++)
  {
    memset(&(ftp_mirrors[k]), 0, sizeof(ftp_mirror_t));
    p2 = get_string_ahref(p2, buf,sizeof(buf)/sizeof(char));
    ftp_mirrors[k].server_name = kstrdup(buf);
    p2 = get_string_ahref(p2, buf,sizeof(buf)/sizeof(char));
    ftp_mirrors[k].paths = kmalloc(sizeof(mirror_path_t));

    //      ftp_mirrors[k].paths=kmalloc (sizeof (char *));
    ftp_mirrors[k].num_paths = 1;

    /*Strip any trailing slash */
    if(buf[strlen(buf)-1]=='/')
      buf[strlen(buf)-1]=0;

    /*Strip any leading slash in the path name if preent */
    if (*buf == '/')
      ftp_mirrors[k].paths[0].path = kstrdup(buf + 1);
    else
      ftp_mirrors[k].paths[0].path = kstrdup(buf);

    p2 = get_string_ahref(p2, buf,sizeof(buf)/sizeof(char));
    ftp_mirrors[k].file_name = kstrdup(buf);
  }

  /* add the users server to the end if it is a ftp server */
  if (request->requested_url->proto == URLFTP)
  {
    memset(&(ftp_mirrors[k]), 0, sizeof(ftp_mirror_t));
    ftp_mirrors[k].server_name = kstrdup(request->requested_url->host);

    ftp_mirrors[k].paths = kmalloc(sizeof(mirror_path_t));
    ftp_mirrors[k].num_paths = 1;

    if (*(request->requested_url->dir))
      ftp_mirrors[k].paths[0].path = kstrdup(request->requested_url->dir);
    else
      ftp_mirrors[k].paths[0].path = kstrdup("");

    ftp_mirrors[k].file_name = kstrdup(request->requested_url->file);
    *num_servers += 1;
  }

  proz_debug("%d servers found\n", *num_servers);

  for (k = 0; k < *num_servers; k++)
  {

    ftp_mirrors[k].full_name =
	(char *) kmalloc(strlen(ftp_mirrors[k].server_name) +
			 strlen(ftp_mirrors[k].paths[0].path) +
			 strlen(ftp_mirrors[k].file_name) + 13);
    sprintf(ftp_mirrors[k].full_name, "%s%s:21/%s%s%s", "ftp://",
	    ftp_mirrors[k].server_name, ftp_mirrors[k].paths[0].path, "/",
	    ftp_mirrors[k].file_name);

    proz_debug("%s\n", ftp_mirrors[k].full_name);
  }


  *pmirrors = reprocess_mirror_list(ftp_mirrors, num_servers);
  /*    *pmirrors = ftp_mirrors; */
  return MIRINFOK;
}


uerr_t get_mirror_info(connection_t * connection, char **ret_buf)
{
  uerr_t err;
  int remote_port_len;
  char *user, *passwd, *www_auth = NULL, *proxy_auth = NULL, *location =
      NULL, *referer = NULL, *pragma_no_cache = NULL;
  char *request, *remote_port;
  netrc_entry *netrc_ent;
  char buffer[HTTP_BUFFER_SIZE];
  /*The http stats that were returned after the call with GET */
  http_stat_t hs_after_get;

  char *p, *p1, *p2;
  int p_len, ret, total;

  /* we want it to terminate immediately */
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);


  /*clear the socks */
  connection->data_sock = 0;
  memset(&hs_after_get, 0, sizeof(hs_after_get));


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

  /*Use no-cache directive for proxy servers, yes by default here as we dont want ftpsearch rsults which can change soon to be cached */
  if (http_use_proxy(connection))
  {
    pragma_no_cache = (char *) alloca(21);
    sprintf(pragma_no_cache, "Pragma: no-cache\r\n");
  }

  request = (char *) alloca(strlen(location)
			    + strlen(connection->user_agent)
			    + strlen(connection->u.host) + remote_port_len
			    + (referer ? strlen(referer) : 0)
			    + (www_auth ? strlen(www_auth) : 0)
			    + (proxy_auth ? strlen(proxy_auth) : 0)
			    + 64
			    +
			    (pragma_no_cache ? strlen(pragma_no_cache) :
			     0));

  /* TODO Add referrer tag. */
  sprintf(request,
	  "GET %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s%s\r\nAccept: */*\r\n%s%s%s%s\r\n",
	  location, connection->user_agent, connection->u.host,
	  remote_port ? remote_port : "",
	  referer ? referer : "",
	  www_auth ? www_auth : "", proxy_auth ? proxy_auth : "",
	  pragma_no_cache ? pragma_no_cache : "");

  proz_debug("1 HTTP request = %s", request);

  connection_show_message(connection, _("Sending HTTP request"));
  err = http_fetch_headers(connection, &hs_after_get, request);

  /* What hapenned ? */
  if (err != HOK)
  {
	proz_debug("1 http_fetch_headers err != HOK %d",err);
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


  /* Ok start fetching the data */
  p1 = p = (char *) kmalloc(HTTP_BUFFER_SIZE + 1);
  p_len = HTTP_BUFFER_SIZE + 1;
  total = 0;


  do
  {

    ret =
	krecv(connection->data_sock, buffer, sizeof(buffer), 0,
	      &connection->xfer_timeout);
    if (ret > 0)
    {
      p2 = grow_buffer(p, p1, &p_len, ret);
      memcpy(p2 + (p1 - p), buffer, ret);
      p1 = (p1 - p) + ret + p2;
      p = p2;
    }
    total += ret;
  }
  while (ret > 0);

  if (ret == -1)
  {
    if (errno == ETIMEDOUT)
    {
      close(connection->data_sock);
      return READERR;
    }
    close(connection->data_sock);
    return READERR;
  }

  p[total] = 0;
  *ret_buf = p;


  close_sock(&connection->data_sock);
  return HOK;
}



char *find_ahref(char *buf)
{

  return (strcasestr(buf, "<A HREF="));
}

char *find_end(char *buf)
{
  return (strcasestr(buf, ">"));
}

char *find_closed_a(char *buf)
{
  return (strcasestr(buf, "</A"));
}

char *get_string_ahref(char *buf, char *out, size_t out_size)
{
  char *p1, *p2, *p3;
  size_t to_copy;

  p1 = find_ahref(buf);
  assert(p1 != NULL);

  p2 = find_end(p1);
  assert(p2 != NULL);

  p3 = find_closed_a(p2);
  assert(p3 != NULL);

  to_copy = p3 - p2 - 1;
  if (to_copy >= out_size)
    to_copy = out_size - 1;
  strncpy(out, p2 + 1, to_copy);
  out[to_copy] = 0;
 

 return p3;
}



char *grow_buffer(char *buf_start, char *cur_pos, int *buf_len,
		  int data_len)
{
  const int INIT_SIZE = 4048;
  int bytes_left;
  char *p;

  /* find how many bytes are left */
  bytes_left = *buf_len - (cur_pos - buf_start);
  assert(bytes_left >= 0);
  assert(data_len <= INIT_SIZE);

  if (bytes_left < data_len + 1)
  {
    /* time to realloc the buffer buffer */
    p = krealloc(buf_start, *buf_len + INIT_SIZE);
    *buf_len += INIT_SIZE;
    return p;
  } else
  {
    return buf_start;
  }
}



void proz_get_complete_mirror_list(ftps_request_t * request)
{
  request->info_running = TRUE;
  /*      get_complete_mirror_list(request); */

  if (pthread_create(&request->info_thread, NULL,
		     (void *) &get_complete_mirror_list,
		     (void *) request) != 0)
    proz_die(_("Error: Not enough system resources"));



}


void proz_cancel_mirror_list_request(ftps_request_t * request)
{
 request->info_running = FALSE;
 pthread_cancel(request->info_thread);
 pthread_join(request->info_thread,0);
}

uerr_t get_complete_mirror_list(ftps_request_t * request)
{
  char *data_buf;

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  do
  {
    pthread_mutex_lock(&request->access_mutex);
    request->info_running = TRUE;
    pthread_mutex_unlock(&request->access_mutex);

    pthread_cleanup_push(cleanup_socks, (void *) request->connection);
    request->err = get_mirror_info(request->connection, &data_buf);
    pthread_cleanup_pop(0);

    if (request->err == NEWLOCATION)
    {
      char *constructed_newloc;
      /*DONE : handle relative urls too */
      constructed_newloc =
	  uri_merge(request->connection->u.url,
		    request->connection->hs.newloc);

      proz_debug("Redirected to %s, merged URL = %s",
		 request->connection->hs.newloc, constructed_newloc);

      proz_free_url(&request->connection->u, 0);
      request->err =
	  proz_parse_url(constructed_newloc, &request->connection->u, 0);


      if (request->err != URLOK)
      {
	connection_show_message(request->connection,
				_
				("The server returned location is wrong: %s!"),
				constructed_newloc);
	pthread_mutex_lock(&request->connection->access_mutex);
	request->info_running = FALSE;
	pthread_mutex_unlock(&request->connection->access_mutex);
	kfree(constructed_newloc);
	pthread_mutex_lock(&request->access_mutex);
	request->info_running = FALSE;
	pthread_mutex_unlock(&request->access_mutex);
	return (request->err = HERR);
      } else
	connection_show_message(request->connection,
				_("Redirected to => %s"),
				constructed_newloc);

      kfree(constructed_newloc);
      request->err = NEWLOCATION;
    }
  }
  while (request->err == NEWLOCATION);

  /*TODO handle and process the redirection here */
  if (request->err != HOK)
  {
    pthread_mutex_lock(&request->access_mutex);
    request->info_running = FALSE;
    pthread_mutex_unlock(&request->access_mutex);
    return request->err;
  }

  request->err = parse_html_mirror_list(request, data_buf);
  /*TODO  see if can give further info */
  pthread_mutex_lock(&request->access_mutex);
  request->info_running = FALSE;
  pthread_mutex_unlock(&request->access_mutex);
  return request->err;

}


boolean proz_request_info_running(ftps_request_t * request)
{
  boolean ret;
  pthread_mutex_lock(&request->access_mutex);
  ret = request->info_running;
  pthread_mutex_unlock(&request->access_mutex);
  return ret;
}

boolean proz_request_mass_ping_running(ftps_request_t * request)
{
  boolean ret;
  pthread_mutex_lock(&request->access_mutex);
  ret = request->mass_ping_running;
  pthread_mutex_unlock(&request->access_mutex);
  return ret;
}



ftp_mirror_t *reprocess_mirror_list(ftp_mirror_t * mirrors,
				    int *num_servers)
{

  ftp_mirror_t *ftp_mirrors;
  int i, j;
  int num_new_servers = 0;

  ftp_mirrors =
      (ftp_mirror_t *) kmalloc(sizeof(ftp_mirror_t) * ((*num_servers)));

  for (i = 0; i < *num_servers; i++)
  {
    if (mirrors[i].copied != 1)
    {
      num_new_servers++;
      memset(ftp_mirrors + num_new_servers - 1, 0, sizeof(ftp_mirror_t));
      memcpy(ftp_mirrors + num_new_servers - 1, mirrors + i,
	     sizeof(ftp_mirror_t));

      /*For the moment assume that all the mirrors support resume */
      ftp_mirrors[num_new_servers - 1].resume_supported=TRUE;

      for (j = i + 1; j < *num_servers; j++)
      {
	if ((strcasecmp
	     (mirrors[i].server_name,
	      mirrors[j].server_name) == 0) && mirrors[j].copied != 1)
	{
	  /*found a  match */
	  ftp_mirrors[num_new_servers - 1].num_paths++;
	  ftp_mirrors[num_new_servers - 1].paths =
	      krealloc(ftp_mirrors[num_new_servers - 1].paths,
		       (sizeof(mirror_path_t) *
			ftp_mirrors[num_new_servers - 1].num_paths));

	  //              ftp_mirrors[num_new_servers-1].paths = krealloc(ftp_mirrors[num_new_servers-1].paths,ftp_mirrors[num_new_servers-1].num_paths );

	  ftp_mirrors[num_new_servers -
		      1].paths[ftp_mirrors[num_new_servers - 1].num_paths -
			       1].path = strdup(mirrors[j].paths[0].path);

	  ftp_mirrors[num_new_servers -
		      1].paths[ftp_mirrors[num_new_servers - 1].num_paths -
			       1].valid = TRUE;

	  mirrors[j].copied = 1;
	}
      }
    }
  }

  *num_servers = num_new_servers;


  proz_debug("Displaying the reparsed list \n");
  for (i = 0; i < num_new_servers; i++)
  {
    proz_debug("%s\n", ftp_mirrors[i].full_name);
    for (j = 0; j < ftp_mirrors[i].num_paths; j++)
      proz_debug("\t%s\n", ftp_mirrors[i].paths[j].path);
  }
  proz_debug("End display reparsed list\n");
  /*TODO free the mirros struct which we will not use now */
  return ftp_mirrors;
}



/*fixme do something about this, move to a better file rather than main.c  */

int compare_two_servers(const void *a, const void *b)
{
  const ftp_mirror_t *ma = (const ftp_mirror_t *) a;
  const ftp_mirror_t *mb = (const ftp_mirror_t *) b;

  int milli_sec_a;
  int milli_sec_b;

  if (ma->status != RESPONSEOK && (mb->status != RESPONSEOK))
    return 1000000;


  milli_sec_a = ma->milli_secs;

  if (ma->status != RESPONSEOK)
  {
    milli_sec_a = 1000000;
  }


  milli_sec_b = mb->milli_secs;

  if (mb->status != RESPONSEOK)
  {
    milli_sec_b = 1000000;
  }


  return (milli_sec_a - milli_sec_b);
}


void proz_sort_mirror_list(ftp_mirror_t * mirrors, int num_servers)
{
  int i;
  qsort(mirrors, num_servers, sizeof(ftp_mirror_t), compare_two_servers);
  for (i = 0; i < num_servers; i++)
    proz_debug("Mirror = %s, time =%d", mirrors[i].server_name,
	       mirrors[i].milli_secs);
}




int ftpsearch_get_server_position(ftps_request_t * request, char *server)
{
  int i;
  for (i = 0; i < request->num_mirrors; i++)
  {

    if (strcmp(request->mirrors[i].server_name, server) == 0)
      return i;
  }
  return -1;
}


int ftpsearch_get_path_position(ftps_request_t * request, char *server,
				char *path)
{
  int i, pos;

  pos = ftpsearch_get_server_position(request, server);
  assert(pos != -1);

  proz_debug("num avail paths %d", request->mirrors[pos].num_paths);

  for (i = 0; i < request->mirrors[pos].num_paths; i++)
  {
    proz_debug("avail path is %s", request->mirrors[pos].paths[i].path);
    proz_debug("path to check is %s", path);
    if (strcmp(request->mirrors[pos].paths[i].path, path) == 0)
      return i;

  }
  return -1;

}
