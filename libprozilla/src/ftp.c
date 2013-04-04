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

/* FTP support. */

/* $Id: ftp.c,v 1.55 2005/09/04 00:06:50 kalum Exp $ */


#include "common.h"
#include "prozilla.h"
#include "connect.h"
#include "misc.h"
#include "url.h"
#include "netrc.h"
#include "debug.h"
#include "ftpparse.h"
#include "ftp.h"

/* #define UNIMPLEMENTED_CMD(a)    ((a == 500) || (a == 502) || (a == 504)) */

#define BUFFER_SIZE 2048


/******************************************************************************
 Return the numeric response of the FTP server by reading the first three
 characters in the buffer.
******************************************************************************/
static int ftp_get_return(const char *ftp_buffer)
{
  char code[4];
  strncpy(code, ftp_buffer, 3);
  code[3] = '\0';
  return atoi(code);
}

/******************************************************************************
 ...
******************************************************************************/
static uerr_t ftp_get_reply(connection_t * connection)
{
  int cont = 0;
  int code;
  response_line *srl;

  /* FIXME Make the line variable dynamically allocated. */
  /* Allocate the space in the buffer for the request. */

  char szBuffer[FTP_BUFFER_SIZE];
  char *strtok_saveptr;// = (char *) alloca(FTP_BUFFER_SIZE);

  memset(szBuffer, 0, FTP_BUFFER_SIZE);

  if (ftp_get_line(connection, szBuffer) != FTPOK)
    return FTPERR;

  if (!isdigit(*szBuffer))
    return FTPERR;

  if (*szBuffer == '\0')
    return FTPERR;

  code = ftp_get_return(szBuffer);

  if (szBuffer[3] == '-')
    cont = 1;
  else
    cont = 0;

  (void) strtok_r(szBuffer, "\r\n", &strtok_saveptr);

  srl = connection->serv_ret_lines = kmalloc(sizeof(response_line));
  srl->line = kstrdup(szBuffer);
  srl->next = 0;

  /* Add the first line to the struct. */

  while (cont)
  {
    if (ftp_get_line(connection, szBuffer) != FTPOK)
      return FTPERR;

    /* Server closed the connection. */
    if (*szBuffer == '\0')
      return FTPERR;

    //    proz_debug("Code %d",code);
    if ((ftp_get_return(szBuffer) == code) && (szBuffer[3] == ' '))
      cont = 0;

    (void) strtok_r(szBuffer, "\r\n", &strtok_saveptr);
    //    proz_debug(_("Message = %s"), szBuffer);
    srl->next = kmalloc(sizeof(response_line));
    srl = srl->next;
    srl->line = kstrdup(szBuffer);
    srl->next = 0;
  }

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
int ftp_check_msg(connection_t * connection, int len)
{
  int ret;

  if ((ret = krecv(connection->ctrl_sock, connection->szBuffer, len,
		   MSG_PEEK, &connection->ctrl_timeout)) == -1)
  {
    proz_debug(_("Error checking for FTP data: %s"), strerror(errno));
    return ret;
  }

  return ret;
}

/******************************************************************************
 ...
******************************************************************************/
int ftp_read_msg(connection_t * connection, int len)
{
  int ret;

  if ((ret = krecv(connection->ctrl_sock, connection->szBuffer, len, 0,
		   &connection->ctrl_timeout)) == -1)
  {
    proz_debug(_("Error receiving FTP data: %s"), strerror(errno));
    return ret;
  }

  return ret;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_send_msg(connection_t * connection, const char *format, ...)
{
  longstring command;
  va_list args;

  va_start(args, format);
#ifdef HAVE_VSNPRINTF
  vsnprintf(command, sizeof(command) - 1, format, args);
  command[sizeof(command) - 1] = '\0';
#else
  vsprintf(command, format, args);
#endif
  va_end(args);

  proz_debug(_("Sending:  %s"), command);

  if ((ksend(connection->ctrl_sock, command, strlen(command), 0,
	     &connection->ctrl_timeout)) == -1)
  {
    proz_debug(_("Error sending FTP data: %s"), strerror(errno));
    return WRITEERR;
  }

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_get_line(connection_t * connection, char *line)
{
  int iLen, iBuffLen = 0, ret = 0;
  char *szptr = line, ch;

  connection->szBuffer = &ch;

  while ((iBuffLen < BUFFER_SIZE)
	 && ((ret = ftp_check_msg(connection, 1)) > 0))
  {
    /* Now get the full string. */
    iLen = ftp_read_msg(connection, 1);

    if (iLen != 1)
      return FTPERR;

    iBuffLen += iLen;
    *szptr = ch;
    szptr += iLen;

    if (ch == '\n')
      break;			/* We have a line -> return. */
  }

  /* Check for error returned in ftp_check_msg(). */
  if (ret == -1)
    return FTPERR;

  /* if zero bytes were found that means the server has closed the connection*/
  if(ret==0)
    *(szptr) = '\0';
  else
    *(szptr + 1) = '\0';

  proz_debug(_("Received: %s"), line);

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_ascii(connection_t * connection)
{
  uerr_t err;

  err = ftp_send_msg(connection, "TYPE A\r\n");
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPUNKNOWNTYPE;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_binary(connection_t * connection)
{
  uerr_t err;

  err = ftp_send_msg(connection, "TYPE I\r\n");
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPUNKNOWNTYPE;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_port(connection_t * connection, const char *command)
{
  uerr_t err;

  err = ftp_send_msg(connection, command);
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPPORTERR;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_list(connection_t * connection, const char *file)
{
  uerr_t err;

  err = ftp_send_msg(connection, "LIST %s\r\n", file);
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if(ftp_get_return(connection->serv_ret_lines->line)==550)
    {
      return FTPNSFOD;
    }

  /*TODO Fix this up, any other return code with  5xx is a fatal error
   */
    if (connection->serv_ret_lines->line[0] == '5')
    return FTPERR;

  if (connection->serv_ret_lines->line[0] != '1')
    return FTPERR;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_retr(connection_t * connection, const char *file)
{
  uerr_t err;

  err = ftp_send_msg(connection, "RETR %s\r\n", file);
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] == '5')
    return FTPNSFOD;

  if (connection->serv_ret_lines->line[0] != '1')
    return FTPERR;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_pasv(connection_t * connection, unsigned char *addr)
{
  uerr_t err;
  unsigned char *p;
  int i;

  err = ftp_send_msg(connection, "PASV\r\n");
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);

  proz_debug(_("FTP PASV Header = %s"), connection->serv_ret_lines->line);

  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPNOPASV;

  /* Parse it. */
  p = (unsigned char *) connection->serv_ret_lines->line;
  for (p += 4; *p && !isdigit(*p); p++);

  if (!*p)
    return FTPINVPASV;

  for (i = 0; i < 6; i++)
  {
    addr[i] = 0;
    for (; isdigit(*p); p++)
      addr[i] = (*p - '0') + 10 * addr[i];

    if (*p == ',')
      p++;
    else if (i < 5)
    {
      return FTPINVPASV;
    }
  }

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_rest(connection_t * connection, off_t bytes)
{
  uerr_t err;

  err = ftp_send_msg(connection, "REST %lld\r\n", bytes);
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] != '3')
    return FTPRESTFAIL;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_cwd(connection_t * connection, const char *dir)
{
  uerr_t err;

  err = ftp_send_msg(connection, "CWD %s\r\n", dir);
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] == '5')
  {
    /* Is it due to the file being not found? */
    if (strstr(connection->serv_ret_lines->line, "o such file")
	|| strstr(connection->serv_ret_lines->line, "o Such File")
	|| strstr(connection->serv_ret_lines->line, "ot found")
	|| strstr(connection->serv_ret_lines->line, "ot Found"))
      return FTPNSFOD;
  }

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPCWDFAIL;

  return FTPOK;
}

/******************************************************************************
 Returns the current working directory in dir.
******************************************************************************/
uerr_t ftp_pwd(connection_t * connection, char *dir)
{
  uerr_t err;
  char *r, *l;
  char szBuffer[FTP_BUFFER_SIZE];

  err = ftp_send_msg(connection, "PWD\r\n");
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] == '5')
    return FTPPWDERR;

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPPWDFAIL;

  if ((r = strrchr(connection->serv_ret_lines->line, '"')) != NULL)
  {
    l = strchr(connection->serv_ret_lines->line, '"');
    if ((l != NULL) && (l != r))
    {
      *r = '\0';
      ++l;
      strcpy(dir, l);
      *r = '"';
    }
  } else
  {
    if ((r = strchr(connection->serv_ret_lines->line, ' ')) != NULL)
    {
      *r = '\0';
      strcpy(dir, szBuffer);
      *r = ' ';
    }
  }

  return FTPOK;
}

/******************************************************************************
 Returns the size of the file in size, on error size will be -1.
******************************************************************************/
uerr_t ftp_size(connection_t * connection, const char *file, off_t *size)
{
  uerr_t err;

  *size = -1;

  err = ftp_send_msg(connection, "SIZE %s\r\n", file);
  if (err != FTPOK)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  /* Now lets figure out what happened. */
  if (connection->serv_ret_lines->line[0] == '2')
  {
    sscanf(connection->serv_ret_lines->line + 3, "%lld", size);
    return FTPOK;
  } else if (connection->serv_ret_lines->line[0] == '5')	/* An error occured. */
  {

  if(ftp_get_return(connection->serv_ret_lines->line)==550)
    {
      return FTPNSFOD;
    }
    /* Is it due to the file being not found? */
    if (strstr(connection->serv_ret_lines->line, "o such file")
	|| strstr(connection->serv_ret_lines->line, "o Such File")
	|| strstr(connection->serv_ret_lines->line, "ot found")
	|| strstr(connection->serv_ret_lines->line, "ot Found"))
      return FTPNSFOD;
  }

  return FTPSIZEFAIL;
}

/******************************************************************************
 Connect to the given FTP server.
******************************************************************************/
uerr_t ftp_connect_to_server(connection_t * connection, const char *name,
			     int port)
{
  uerr_t err;

  err = connect_to_server(&(connection->ctrl_sock), name, port,
			  &connection->conn_timeout);
  if (err != NOCONERROR)
    return err;

  err = ftp_get_reply(connection);
  if (err != FTPOK)
    return err;

  if (connection->serv_ret_lines->line[0] != '2')
    return FTPCONREFUSED;

  return FTPOK;
}

/******************************************************************************
 This function will call bind() to return a bound socket then the FTP server 
 will be connected with a port request and asked to connect.
******************************************************************************/
uerr_t ftp_get_listen_socket(connection_t * connection, int *listen_sock)
{
  /* Get a fixed value. */
  char command[MAX_MSG_SIZE];
  int sockfd;
  socklen_t len;
  struct sockaddr_in TempAddr;
  char *port, *ipaddr;
  struct sockaddr_in serv_addr;
  uerr_t err;


  if (bind_socket(&sockfd) != BINDOK)
    return LISTENERR;

  len = sizeof(serv_addr);
  if (getsockname(sockfd, (struct sockaddr *) &serv_addr, &len) < 0)
  {
    perror("getsockname");
    close(sockfd);
    return CONPORTERR;
  }


  /* Get hosts info. */
  len = sizeof(TempAddr);

  if (getsockname(connection->ctrl_sock, (struct sockaddr *) &TempAddr,
		  &len) < 0)
  {
    perror("getsockname");
    close(sockfd);
    return CONPORTERR;
  }

  ipaddr = (char *) &TempAddr.sin_addr;

  port = (char *) &serv_addr.sin_port;

#define UC(b) (((int)b)&0xff)

  sprintf(command, "PORT %d,%d,%d,%d,%d,%d\r\n", UC(ipaddr[0]),
	  UC(ipaddr[1]), UC(ipaddr[2]), UC(ipaddr[3]), UC(port[0]),
	  UC(port[1]));

  err = ftp_port(connection, command);
  if (err != FTPOK)
    return err;

  *listen_sock = sockfd;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t ftp_login(connection_t * connection, const char *username,
		 const char *passwd)
{
  uerr_t err = FTPERR;
  int ret_code = 220;
  boolean logged_in = FALSE;

  while (1)
  {
    switch (ret_code)
    {
    case 220:
      /* IDEA: Lets add the proxy support here. */

      if (!ftp_use_proxy(connection))
      {
	/* No proxy just direct connection. */
	err = ftp_send_msg(connection, "USER %s\r\n", username);
      } else
      {
	switch (connection->ftp_proxy->type)
	{
	case USERatSITE:
	  err = ftp_send_msg(connection, "USER %s@%s:%d\r\n", username,
			     connection->u.host, connection->u.port);
	  break;
	case USERatPROXYUSERatSITE:
	  err = ftp_send_msg(connection, "USER %s@%s@%s:%d\r\n", username,
			     connection->ftp_proxy->username,
			     connection->u.host, connection->u.port);
	  break;
	case USERatSITE_PROXYUSER:
	  err = ftp_send_msg(connection, "USER %s:%d@%s %s\r\n", username,
			     connection->u.host, connection->u.port,
			     connection->ftp_proxy->username);
	  break;
	case PROXYUSERatSITE:
	  err = ftp_send_msg(connection, "USER %s@%s:%d\r\n",
			     connection->ftp_proxy->username,
			     connection->u.host, connection->u.port);
	  break;
	default:
	  /* Something else, just send PROXY USER. */
	  err = ftp_send_msg(connection, "USER %s\r\n",
			     connection->ftp_proxy->username);
	  break;
	}
      }

      if (err != FTPOK)
	return err;

      err = ftp_get_reply(connection);
      if (err != FTPOK)
	return err;

      break;


    case 230:			/* Fallthrough. */
    case 231:			/* Fallthrough. */
    case 202:

      logged_in = TRUE;

      if (!ftp_use_proxy(connection))
	return FTPOK;		/* Logged in succesfully. */

      switch (connection->ftp_proxy->type)
      {
      case LOGINthenUSERatSITE:
	err = ftp_send_msg(connection, "USER %s@%s:%d\r\n", username,
			   connection->u.host, connection->u.port);
	break;
      case OPENSITE:
	err =
	    ftp_send_msg(connection, "OPEN %s:%d\r\n", connection->u.host,
			 connection->u.port);
	break;
      case SITESITE:
	err =
	    ftp_send_msg(connection, "SITE %s:%d\r\n", connection->u.host,
			 connection->u.port);
	break;
      case PROXYUSERatSITE:
	err = ftp_send_msg(connection, "USER %s\r\n", username);
	break;
      default:
	/* TODO What is the default here? */
	return FTPOK;
	break;
      }

      if (err != FTPOK)
	return err;

      err = ftp_get_reply(connection);
      if (err != FTPOK)
	return err;

      break;


      /* Handle 421 services not available. */
    case 421:
      return FTPSERVCLOSEDATLOGIN;
      break;


      /* User name is all right, need password. */
    case 331:
      if (!ftp_use_proxy(connection))
      {
	/* No proxy just direct connection. */
	err = ftp_send_msg(connection, "PASS %s\r\n", passwd);
      } else
      {
	switch (connection->ftp_proxy->type)
	{
	case USERatSITE:
	  err = ftp_send_msg(connection, "PASS %s\r\n", passwd);
	  break;
	case USERatPROXYUSERatSITE:
	  err = ftp_send_msg(connection, "PASS %s@%s\r\n", passwd,
			     connection->ftp_proxy->passwd);
	  break;
	case USERatSITE_PROXYUSER:
	  err = ftp_send_msg(connection, "PASS %s\r\n", passwd);
	  break;
	case PROXYUSERatSITE:
	  err = ftp_send_msg(connection, "PASS %s\r\n",
			     connection->ftp_proxy->passwd);
	  break;
	default:
	  /* Something else we dont know about. */
	  err = ftp_send_msg(connection, "PASS %s\r\n",
			     connection->ftp_proxy->passwd);
	  break;
	}
      }

      if (err != FTPOK)
	return err;

      err = ftp_get_reply(connection);
      if (err != FTPOK)
	return err;

      break;


      /* 5xx series of commands indicate error. */
    case 530:
      return FTPLOGREFUSED;
      break;


    case 501:			/* Fallthrough. */
    case 503:			/* Fallthrough. */
    case 550:
      return FTPERR;
      break;


    default:
      /* Unknown error code. */
      proz_debug(_("Unknown code %d retuned during FTP login"), ret_code);
      return FTPERR;
      break;
    }

    ret_code = ftp_get_return(connection->serv_ret_lines->line);
    done_with_response(connection);
  }

  if (err != FTPOK)
    return err;

  return FTPOK;
}

/******************************************************************************
 ...
******************************************************************************/
boolean ftp_use_proxy(connection_t * connection)
{
  return (connection->ftp_proxy && connection->ftp_proxy->use_proxy &&
	  connection->ftp_proxy->proxy_url.url) ? TRUE : FALSE;
}

/******************************************************************************
 Gets info about the url (connection->u) from the FTP server, and fills in
 info like whether the server supports resume, the file size etc.
******************************************************************************/
uerr_t proz_ftp_get_url_info(connection_t * connection)
{
  uerr_t err;
  char *user, *passwd, *tmp;
  netrc_entry *netrc_ent;
  boolean passive_mode;
  longstring buffer;
  boolean size_ok;
  struct ftpparse fp;
  /* if we have to use a HTTP proxy call the routine which is defined in http.c
     and just return.
   */
  if (ftp_use_proxy(connection)
      && connection->ftp_proxy->type == HTTPPROXY)
  {
    err = ftp_get_url_info_from_http_proxy(connection);
    return err;
  }


  init_response(connection);

  if (ftp_use_proxy(connection))
  {
    connection_show_message(connection, _("Connecting to %s"),
			    connection->ftp_proxy->proxy_url.host);

    /* Connect to the proxy server here. */
    err = ftp_connect_to_server(connection,
				connection->ftp_proxy->proxy_url.host,
				connection->ftp_proxy->proxy_url.port);

    if (err != FTPOK)
    {
      connection_show_message(connection,
			      _("Error while connecting to %s"),
			      connection->ftp_proxy->proxy_url.host);
      return err;
    }

    connection_show_message(connection, _("Connected to %s"),
			    connection->ftp_proxy->proxy_url.host);
  } else
  {
    connection_show_message(connection, _("Connecting to %s"),
			    connection->u.host);

    err = ftp_connect_to_server(connection, connection->u.host,
				connection->u.port);

    if (err != FTPOK)
    {
      connection_show_message(connection,
			      _("Error while connecting to %s"),
			      connection->u.host);;
      return err;
    }
    connection_show_message(connection, _("Connected to %s"),
			    connection->u.host);
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

  if (strcmp(user, "anonymous") == 0)
    connection_show_message(connection,
			    _("Logging in as user %s with password %s"),
			    user, passwd);
  else
  {
    int pwd_len = strlen(passwd);
    char *tmp_pwd = (char *) kmalloc(pwd_len + 1);
    memset(tmp_pwd, 'x', pwd_len);
    tmp_pwd[pwd_len] = 0;
    connection_show_message(connection,
			    _("Logging in as user %s with password %s"),
			    user, tmp_pwd);
    kfree(tmp_pwd);
  }

  init_response(connection);
  err = ftp_login(connection, user, passwd);
  if (err != FTPOK)
  {
    close_sock(&connection->ctrl_sock);
    return err;
  }
  done_with_response(connection);

  connection_show_message(connection, _("Logged in successfully"));

  init_response(connection);
  err = ftp_binary(connection);
  if (err != FTPOK)
  {
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
      connection_show_message(connection,
			      _("CWD failed to change to directory '%s'"),
			      connection->u.dir);
      close_sock(&connection->ctrl_sock);
      return err;
    } else
    {
      done_with_response(connection);
    }
  } else
    connection_show_message(connection, _("CWD not needed"));

  init_response(connection);
  err = ftp_rest(connection, 0);
  if (err != FTPOK)
  {
    connection->resume_support = FALSE;
    connection_show_message(connection, _("REST failed"));
    /* NOTE: removed   return err; */
  } else
  {
    connection->resume_support = TRUE;
    connection_show_message(connection, _("REST ok"));
  }
  done_with_response(connection);

  /* Lets see whether the URL really is a file. */
  init_response(connection);
  err = ftp_cwd(connection, connection->u.file);
  if (err == FTPOK)
  {
    /* So connection->u.file is a directory and not a file. */
    connection->file_type = DIRECTORY;
    return FTPOK;
  } else
  {

    /* FIXME: The statement below is strictly not true, it could be a symlink 
       but for the moment lets leave this as it is, later we will perform a
       LIST command and detect whether it is a symlink. */
    connection->file_type = REGULAR_FILE;
  }

  done_with_response(connection);

  init_response(connection);
  err =
      ftp_size(connection, connection->u.file,
	       &connection->main_file_size);

 
/*   if ((err == FTPOK) || (err == FTPNSFOD) || (err != FTPSIZEFAIL)) */
/*   { */
/*     close_sock(&connection->ctrl_sock); */
/*     return err; */
/*   } */

 switch (err)
    {
    case FTPNSFOD:
      {
	close_sock(&connection->ctrl_sock);
	return err;
      }
 
    case FTPOK:
      size_ok=TRUE;
      break;
    case FTPSIZEFAIL:
      size_ok=FALSE;
      break;
    default:
      size_ok=FALSE;
   }

 
 done_with_response(connection);

/* Now we additionaly will get the server to display info with the
   list command, initially we only called the LIST command only if the
   SIZE failed
*/

  err = ftp_setup_data_sock_1(connection, &passive_mode);
  if (err != FTPOK)
  {
    close_sock(&connection->ctrl_sock);
    return err;
  }


  init_response(connection);
  err = ftp_ascii(connection);
  if (err != FTPOK)
  {
    close_sock(&connection->ctrl_sock);
    return err;
  }
  done_with_response(connection);



  init_response(connection);
  err = ftp_list(connection, connection->u.file);
  if (err != FTPOK)
    {
      if(err==FTPNSFOD)
	{
	  //If the remote server returns ftpnsfod which could be due
	  //to the fact that the server doesnt permit the directory
	  //contents to be listed we will print a warning and return
	  //FTPOK as it is not a fatal error.
	  connection_show_message(connection,
				  _("FTP LIST failed: File not found or access not permitted."));
	  close_sock(&connection->ctrl_sock);
	  return FTPOK;
	} 
      else
	{
	  connection_show_message(connection,
				  "FTP LIST failed: Server returned %s",connection->serv_ret_lines->line );
	  close_sock(&connection->ctrl_sock);
	  return FTPOK;
	}
    }
  done_with_response(connection);

  err = ftp_setup_data_sock_2(connection, &passive_mode);
  if (err != FTPOK)

  {
    close_sock(&connection->ctrl_sock);
    return err;
  }


  /* Now read the data to the buffer. */
  /* TODO Create a buffer which dynamically resizes itself as we add data. */

  if (krecv(connection->data_sock, buffer, sizeof(buffer), 0,
	    &connection->xfer_timeout) == -1)
  {
    connection_show_message(connection,
			    _("Error receiving FTP transfer data: %s"),
			    strerror(errno));
    return FTPERR;
  }

  proz_debug(_("String received after the LIST command = %s"), buffer);

  while ((tmp = strrchr(buffer, '\n')) || (tmp = strrchr(buffer, '\r')))
  {
    *tmp = 0;
  };

  close_sock(&connection->data_sock);
  close_sock(&connection->ctrl_sock);

  //  size_rt = size_returner(buffer, strlen(buffer));
  err =ftp_parse(&fp, buffer, strlen(buffer));
  if (err != FTPPARSEOK)
  {
    connection_show_message(connection,
			    _
			    ("Unable to parse the line the FTP server returned:please report URL to prozilla@genesys.ro "));
  }

  if(err==FTPPARSEOK)
    {
      proz_debug("size returned from LIST %ld",fp.filesize);
      //SEC size_rt off_t?
      if(size_ok==FALSE)
	{
	  proz_debug("SIZE failed, setting file size based on LIST");
	  connection->main_file_size = fp.filesize;
	}
    }
  return FTPOK;
}

/******************************************************************************
 This will be the first step in setting up a data sock, it will try
 PASV or PORT.
******************************************************************************/
uerr_t ftp_setup_data_sock_1(connection_t * connection,
			     boolean * passive_mode)
{
  uerr_t err;

  /* If enabled lets try PASV. */
  if (connection->ftp_use_pasv == TRUE)
  {
    init_response(connection);
    err = ftp_pasv(connection, connection->pasv_addr);

    /* If the error is due to the server not supporting PASV then set the
       flag and lets try PORT. */
    if ((err == FTPNOPASV) || (err == FTPINVPASV))
    {
      proz_debug(_("Server doesn't seem to support PASV"));
      *passive_mode = FALSE;
    } else if (err == FTPOK)	/* Server supports PASV. */
    {
      char dhost[256];
      unsigned short dport;

      sprintf(dhost, "%d.%d.%d.%d", connection->pasv_addr[0],
	      connection->pasv_addr[1], connection->pasv_addr[2],
	      connection->pasv_addr[3]);

      dport = (connection->pasv_addr[4] << 8) + connection->pasv_addr[5];

      err = connect_to_server(&connection->data_sock, dhost, dport,
			      &connection->xfer_timeout);
      if (err != NOCONERROR)
	return err;

      /* Everything seems to be ok. */
      *passive_mode = TRUE;
    } else
      return err;

    done_with_response(connection);
  } else
    *passive_mode = FALSE;	/* Ok... Since PASV is not to be used. */

  if (*passive_mode == FALSE)
  {
    /* Obtain a listen socket. */
    err = ftp_get_listen_socket(connection, &connection->listen_sock);
    if (err != FTPOK)
      return err;
  }

  return FTPOK;
}

/******************************************************************************
 This will be the second step in setting up a data sock, if passive mode is
 FALSE, it will call accept_connection().
******************************************************************************/
uerr_t ftp_setup_data_sock_2(connection_t * connection,
			     boolean * passive_mode)
{
  uerr_t err;

  if (*passive_mode == FALSE)	/* We have to accept the connection. */
  {
    err =
	accept_connection(connection->listen_sock, &connection->data_sock);
    if (err != ACCEPTOK)
      return err;
  }

  return FTPOK;
}



/*Loops for connection->attempts */
uerr_t ftp_get_url_info_loop(connection_t * connection)
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
			      _("Retrying attempt %d in %d seconds"),
			      connection->attempts,
			      connection->retry_delay.tv_sec);
      delay_ms(connection->retry_delay.tv_sec * 1000);

    }

    /*Push the handler which will cleanup any sockets that are left open */
    pthread_cleanup_push(cleanup_socks, (void *) connection);

    connection->err = proz_ftp_get_url_info(connection);
    /*pop the handler */
    pthread_cleanup_pop(0);

    connection->attempts++;

    switch (connection->err)
    {
    case FTPOK:
      connection_show_message(connection, _("Successfully got info"));
      pthread_mutex_lock(&connection->access_mutex);
      connection->running = FALSE;
      pthread_mutex_unlock(&connection->access_mutex);
      return connection->err;
      break;

    case FTPNSFOD:
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
