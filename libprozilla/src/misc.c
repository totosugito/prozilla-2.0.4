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

/* Miscellaneous routines. */

/* $Id: misc.c,v 1.32 2005/01/11 01:49:11 sean Exp $ */


#include "common.h"
#include "prozilla.h"
#include "misc.h"
#include "debug.h"


void cleanup_httpsocks(connection_t * connection);
void cleanup_ftpsocks(connection_t * connection);

/******************************************************************************
 Allocates size bytes of memory. If size is 0 it returns NULL. If there is
 not enough memory the program quits with an error message.
******************************************************************************/
void *kmalloc(size_t size)
{
  void *ret;

  if (size == 0)
    return NULL;

  ret = malloc(size);

  if (ret == NULL)
    proz_die(_("Failed to malloc() %lu bytes."), size);

  return ret;
}

/******************************************************************************
 A wrapper for realloc() which aborts if not enough memory is present.
******************************************************************************/
void *krealloc(void *ptr, size_t new_size)
{
  void *ret;

  ret = realloc(ptr, new_size);

  if (!ret)
    proz_die(_("Failed to realloc() %lu bytes."), new_size);

  return ret;
}

/******************************************************************************
 A wrapper for free() which handles NULL pointers.
******************************************************************************/
void kfree(void *ptr)
{
  if (ptr != NULL)
    free(ptr);
}

/******************************************************************************
 A wrapper for strdup() which aborts if not enough memory is present.
******************************************************************************/
char *kstrdup(const char *str)
{
  char *ret = strdup(str);

  if (!ret)
    proz_die(_("Not enough memory to continue: strdup() failed."));

  return ret;
}

/******************************************************************************
 Checks whether the specified string is a number or digit.
******************************************************************************/
boolean is_number(const char *str)
{
  unsigned int i = 0;

  if (str[0] == '\0')
    return FALSE;

  while (str[i] != '\0')
  {
    if (!isdigit(str[i]))
      return FALSE;
    i++;
  }
  return TRUE;
}

/******************************************************************************
 How many digits are in a long integer?
******************************************************************************/
int numdigit(long a)
{
  int res;

  for (res = 1; (a /= 10) != 0; res++)
    ;

  return res;
}

/******************************************************************************
 Copy the string formed by two pointers (one on the beginning, other on the
 char after the last char) to a new, malloc()-ed location. 0-terminate it.
******************************************************************************/
char *strdupdelim(const char *beg, const char *end)
{
  char *res;

  res = kmalloc(end - beg + 1);
  memcpy(res, beg, end - beg);
  res[end - beg] = '\0';
  return res;
}

/******************************************************************************
 Print a long integer to the string buffer. The digits are first written in
 reverse order (the least significant digit first), and are then reversed.
******************************************************************************/
void prnum(char *where, long num)
{
  char *p;
  int i = 0, l;
  char c;

  if (num < 0)
  {
    *where++ = '-';
    num = -num;
  }

  p = where;

  /* Print the digits to the string. */
  do
  {
    *p++ = num % 10 + '0';
    num /= 10;
  }
  while (num);

  /* And reverse them. */
  l = p - where - 1;
  for (i = l / 2; i >= 0; i--)
  {
    c = where[i];
    where[i] = where[l - i];
    where[l - i] = c;
  }
  where[l + 1] = '\0';
}

/******************************************************************************
 Extracts a numurical argument from an option, when it has been specified for
 example as -l=3 or -l3. Returns 1 on success or 0 on error (non numerical
 argument etc).
******************************************************************************/
int setargval(char *optstr, int *num)
{
  if (*optstr == '=')
  {
    if (is_number(optstr + 1))
    {
      *num = atoi(optstr + 1);
      return 1;
    } else
      return 0;
  } else
  {
    if (is_number(optstr))
    {
      *num = atoi(optstr);
      return 1;
    } else
      return 0;
  }

}

/******************************************************************************
 Encode the given string to base64 format and place it into store. store will
 be 0-terminated, and must point to a writable buffer of at least
 1+BASE64_LENGTH(length) bytes. Note: Routine stolen from wget (grendel).
******************************************************************************/
void base64_encode(const char *s, char *store, int length)
{
  /* Conversion table. */
  char tbl[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
  };

  int i;
  unsigned char *p = (unsigned char *) store;

  /* Transform the 3x8 bits to 4x6 bits, as required by base64. */
  for (i = 0; i < length; i += 3)
  {
    *p++ = tbl[s[0] >> 2];
    *p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
    *p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
    *p++ = tbl[s[2] & 0x3f];
    s += 3;
  }

  /* Pad the result if necessary... */
  if (i == length + 1)
    *(p - 1) = '=';
  else if (i == length + 2)
    *(p - 1) = *(p - 2) = '=';
  /* ...and zero-terminate it. */
  *p = '\0';
}

/******************************************************************************
 Return the user's home directory (strdup-ed), or NULL if none is found.
******************************************************************************/
char *home_dir(void)
{
  char *home = getenv("HOME");

  if (home == NULL)
  {
    /* If $HOME is not defined, try getting it from the passwd file. */
    struct passwd *pwd = getpwuid(getuid());
    if (!pwd || !pwd->pw_dir)
      return NULL;
    home = pwd->pw_dir;
  }

  return home ? kstrdup(home) : NULL;
}

/******************************************************************************
 Subtract the `struct timeval' values X and Y, storing the result in RESULT.
 Return 1 if the difference is negative, otherwise 0. 
******************************************************************************/
int proz_timeval_subtract(struct timeval *result, struct timeval *x,
			  struct timeval *y)
{

  /* Perform the carry for the later subtraction by updating Y. */
  if (x->tv_usec < y->tv_usec)
  {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }

  if (x->tv_usec - y->tv_usec > 1000000)
  {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait. `tv_usec' is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

/******************************************************************************
 Wait for 'ms' milliseconds.
******************************************************************************/
void delay_ms(int ms)
{
  struct timeval tv_delay;

  memset(&tv_delay, 0, sizeof(tv_delay));

  tv_delay.tv_sec = ms / 1000;
  tv_delay.tv_usec = (ms * 1000) % 1000000;

  if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv_delay) < 0)
    proz_debug(_("Warning: Unable to delay"));
}


/*Closes a socket and zeroes the socket before returning */
int close_sock(int *sock)
{

  int retval = close(*sock);
  *sock = 0;
  return retval;
}

/*Returns a string representation of a prozilla errror */

char *proz_strerror(uerr_t error)
{

  switch (error)
  {

  case HOSTERR:
    return _("Unable to lookup hostname");
  case CONSOCKERR:
    return _("Unable to create socket");
  case CONERROR:
    return _("Error occured while connecting");
  case CONREFUSED:
    return _("The connection attempt was refused");
  case ACCEPTERR:
    return _("Error while accepting the connection");
  case BINDERR:
    return _("Error while Binding socket");
  case LISTENERR:
    return _("Error while listening");
  case SERVERCLOSECONERR:
    return _("The connection was reset/closed by the peer");
  case URLUNKNOWN:
    return _("The URL Protocol was unknown");
  case URLBADPORT:
    return _("The port specified in the URL is not valid!");
  case URLBADHOST:
    return _("The Hostname specified in the URL is not valid!");
  case URLBADPATTERN:
    return _("The Pattern specified in the URL does not look valid!");
  case HEOF:
    return _("End of file reached in HTTP connection");
  case HERR:
    return _("Error occured in HTTP data transfer");
  case HAUTHREQ:
    return _("Authentification is required to access this resource");
  case HAUTHFAIL:
    return _("Failed to Authenticate with host!");
  case HTTPNSFOD:
    return _("The URL was not found on the host!");
  case FTPLOGREFUSED:
    return _("The host disallowed the login attempt");
  case FTPPORTERR:
    return _("The PORT request was rejected by the server");
  case FTPNSFOD:
    return _("The object file/dir was not found on the host!");
  case FTPUNKNOWNTYPE:
    return _("The TYPE specified in not known by the FTP server!");
  case FTPUNKNOWNCMD:
    return _("The command is not known by the FTP server!");
  case FTPSIZEFAIL:
    return _("The SIZE command failed");
  case FTPERR:
    return _("Error occured in FTP data transfer");
  case FTPRESTFAIL:
    return _("The REST command failed");
  case FTPACCDENIED:
    return _("The peer did not allow access");
  case FTPPWDERR:
    return _("The host rejected the password");
  case FTPPWDFAIL:
    return _("The host rejected the password");
  case FTPINVPASV:
    return _("The PASV (passive mode) was not supported the host");
  case FTPNOPASV:
    return _("The host does not support PASV (passive mode) transfers");
  case FTPCONREFUSED:
    return _("The connection attempt was refused");
  case FTPCWDFAIL:
    return _("Failed to (CWD)change to the directory");
  case FTPSERVCLOSEDATLOGIN:
    return
	_
	("The host said the requested service was unavailable and closed the control connection");
  case CONPORTERR:
    return _("getsockname failed!");

  case GATEWAYTIMEOUT:
    return
	_
	("The server, while acting as a gateway or proxy, received an invalid response from the upstream server it accessed in attempting to fulfill the request");

  case SERVICEUNAVAIL:
    return
	_
	("The server is currently unable to handle the request due to a temporary overloading or maintenance of the server.");

  case BADGATEWAY:
    return
	_
	("The server, while acting as a gateway or proxy, received an invalid response from the upstream server it accessed in attempting to fulfill the request");

  case INTERNALSERVERR:
    return
	_
	("The server encountered an unexpected condition which prevented it from fulfilling the request.");

  case UNKNOWNREQ:
    return
	_
	("The server does not support the functionality required to fulfill the request.");

  case FOPENERR:
    return _("Error while opening file");
  case FWRITEERR:
    return _("Error while writing to file");

  case DLABORTED:
    return _("The Download was aborted");
  case DLLOCALFATAL:
    return _("The Download encountered a local fatal error");
  case CANTRESUME:
    return _("Error: Resuming this connection is not possible");
  case READERR:
    return _("Error while reading data from socket");
  case WRITEERR:
    return _("Error while writing data to socket");
  case PROXERR:
    return _("Error while Proxying");
  case FILEISDIR:
    return _("The location is a directory");

  default:
    return _("Unknown/Unsupported error code");
  }

}

/* Cleanup handler which will be popped and which will be called when the thread is cancelled, it make sure that there will be no sockets left open if the thread is cancelled prematurely
*/
#include "ftp.h"

void cleanup_socks(void *cdata)
{
  connection_t *connection = (connection_t *) cdata;

  switch (connection->u.proto)
  {
  case URLHTTP:
    cleanup_httpsocks(connection);
    break;
  case URLFTP:
    if (ftp_use_proxy(connection)
	&& connection->ftp_proxy->type == HTTPPROXY)
    {
      /* We have to cleanup the http socks instead 
         if we are going through a http proxy */
      cleanup_httpsocks(connection);
    } else
      cleanup_ftpsocks(connection);
    break;
  default:
    proz_die(_("Error: unsupported protocol"));
  }
}



void cleanup_ftpsocks(connection_t * connection)
{
  int flags;

  proz_debug("in clean ftp sock\n");


  if (connection->data_sock > 0)
  {
    flags = fcntl(connection->data_sock, F_GETFD, 0);
    if (flags == -1)
    {
      proz_debug("data sock invalid\n");
    } else
      close_sock(&connection->data_sock);
  }

  if (connection->ctrl_sock > 0)
  {
    flags = fcntl(connection->ctrl_sock, F_GETFD, 0);
    if (flags == -1)
    {
      proz_debug("control sock invalid\n");
    } else
      close_sock(&connection->ctrl_sock);
  }

}


void cleanup_httpsocks(connection_t * connection)
{
  int flags;

  proz_debug("in clean http sock\n");

  if (connection->data_sock > 0)
  {
    flags = fcntl(connection->data_sock, F_GETFD, 0);
    if (flags == -1)
    {
      proz_debug("sock invalid\n");
    } else
      close(connection->data_sock);
  }

}




