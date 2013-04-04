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

/* Connection routines. */

/* $Id: connect.c,v 1.24 2005/07/29 16:28:47 kalum Exp $ */


#include "common.h"
#include "misc.h"
#include "debug.h"
#include "connect.h"



/******************************************************************************
 Connect to the specified server.
******************************************************************************/
uerr_t connect_to_server(int *sock, const char *name, int port,
			 struct timeval *tout)
{
  int status, noblock, flags;
  char szPort[10];
  extern int h_errno;
   int opt; 
   struct timeval timeout;
    struct addrinfo hints, *res=NULL;
    int error;


  assert(name != NULL);
  memcpy(&timeout, tout, sizeof(timeout));

  memset(&hints, 0, sizeof(hints));
  memset(szPort, 0, sizeof(szPort));
  snprintf(szPort, sizeof(szPort), "%d", port);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

    error = getaddrinfo(name, szPort, &hints, &res);
    if (error) {
           freeaddrinfo(res);
            return HOSTERR;
        }


  /* Create a socket. */
  if ((*sock = socket(res->ai_family, res->ai_socktype, IPPROTO_TCP)) < 1)
  {
    free(res);
    return CONSOCKERR;
  }

  /* Experimental. */
  flags = fcntl(*sock, F_GETFL, 0);
  if (flags != -1)
    noblock = fcntl(*sock, F_SETFL, flags | O_NONBLOCK);
  else
    noblock = -1;

  status = connect(*sock, res->ai_addr, res->ai_addrlen);


  if ((status == -1) && (noblock != -1) && (errno == EINPROGRESS))
  {
    fd_set writefd;

    FD_ZERO(&writefd);
    FD_SET(*sock, &writefd);

    status = select((*sock + 1), NULL, &writefd, NULL, &timeout);

    /* Do we need to retry if the err is EINTR? */

    if (status > 0)
    {
      socklen_t arglen = sizeof(int);

      if (getsockopt(*sock, SOL_SOCKET, SO_ERROR, &status, &arglen) < 0)
	status = errno;

      if (status != 0)
	errno = status, status = -1;

      if (errno == EINPROGRESS)
	errno = ETIMEDOUT;
    } else if (status == 0)
      errno = ETIMEDOUT, status = -1;
  }

  if (status < 0)
  {
    close(*sock);

    if (errno == ECONNREFUSED)
    {
      free(res);
      return CONREFUSED;
    } else
    {
      free(res);
      return CONERROR;
    }
  } else
  {
    flags = fcntl(*sock, F_GETFL, 0);

    if (flags != -1)
      fcntl(*sock, F_SETFL, flags & ~O_NONBLOCK);
  }

  
    /* Enable KEEPALIVE, so dead connections could be closed
     * earlier. Useful in conjuction with TCP kernel tuning
     * in /proc/sys/net/ipv4/tcp_* files. */
    opt = 1;
    setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE,
               (char *) &opt, (int) sizeof(opt));  
 
  free(res);

  return NOCONERROR;
}

/******************************************************************************
 ...
******************************************************************************/
uerr_t bind_socket(int *sockfd)
{
  struct sockaddr_in serv_addr;

  /* Open a TCP socket (an Internet stream socket). */
  if ((*sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    return CONSOCKERR;

  /* Fill in the structure fields for binding. */
  memset((void *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(0);	/* Let the system choose. */

  /* Bind the address to the socket. */
  if (bind(*sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("bind");
    close(*sockfd);
    return BINDERR;
  }

  /* Allow only one server. */
  if (listen(*sockfd, 1) < 0)
  {
    perror("listen");
    close(*sockfd);
    return LISTENERR;
  }

  return BINDOK;
}

/******************************************************************************
 ...
******************************************************************************/
int select_fd(int fd, struct timeval *timeout, int writep)
{
  fd_set fds, exceptfds;
  struct timeval to;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  FD_ZERO(&exceptfds);
  FD_SET(fd, &exceptfds);
  memcpy(&to, timeout, sizeof(struct timeval));

  return (select(fd + 1, writep ? NULL : &fds, writep ? &fds : NULL,
		 &exceptfds, &to));
}

/******************************************************************************
 Receive size bytes from sock with a time delay.
******************************************************************************/
int krecv(int sock, char *buffer, int size, int flags,
	  struct timeval *timeout)
{
  int ret, arglen;

  arglen = sizeof(int);

  assert(size >= 0);

  do
  {
    if (timeout)
    {
      do
      {
	ret = select_fd(sock, timeout, 0);
      }
      while ((ret == -1) && (errno == EINTR));


      if (ret <= 0)
      {
	/* proz_debug("Error after select res=%d errno=%d.", ret, errno); */

	/* Set errno to ETIMEDOUT on timeout. */
	if (ret == 0)
	  errno = ETIMEDOUT;

	return -1;
      }
    }

    ret = recv(sock, buffer, size, flags);
  }
  while ((ret == -1) && (errno == EINTR));

  return ret;
}

/******************************************************************************
 Send size bytes to sock with a time delay.
******************************************************************************/
int ksend(int sock, char *buffer, int size, int flags,
	  struct timeval *timeout)
{
  int ret = 0;

  /* write() may write less than size bytes, thus the outward loop
     keeps trying it until all was written, or an error occurred. The
     inner loop is reserved for the usual EINTR f*kage, and the
     innermost loop deals with the same during select(). */

  while (size != 0)
  {
    do
    {
      if (timeout)
      {
	do
	{
	  ret = select_fd(sock, timeout, 1);
	}
	while ((ret == -1) && (errno == EINTR));

	if (ret <= 0)
	{
	  /* Set errno to ETIMEDOUT on timeout. */
	  if (ret == 0)
	    errno = ETIMEDOUT;
	  return -1;
	}
      }
      ret = send(sock, buffer, size, flags);
    }
    while ((ret == -1) && (errno == EINTR));

    if (ret <= 0)
      break;

    buffer += ret;
    size -= ret;
  }
  return ret;
}

/******************************************************************************
 Accept a connection.
******************************************************************************/
uerr_t accept_connection(int listen_sock, int *data_sock)
{
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  int sockfd;

  sockfd = accept(listen_sock, (struct sockaddr *) &cli_addr, &clilen);
  if (sockfd < 0)
  {
    perror("accept");
    return ACCEPTERR;
  }

  *data_sock = sockfd;

  /* Now we can free the listen socket since it is not needed...
     accept() returned the new socket... */
  close(listen_sock);

  return ACCEPTOK;
}
