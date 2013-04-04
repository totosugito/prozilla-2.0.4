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
#include "connect.h"
#include "misc.h"
#include "url.h"
#include "netrc.h"
#include "debug.h"
#include "ping.h"



#define TCP_PING_PACKSIZE 3

uerr_t tcp_ping(ping_t * ping_data)
{
  int status, noblock, flags;
  extern int h_errno;
  struct timeval start_time;
  struct timeval end_time;
  char ping_buf[TCP_PING_PACKSIZE];
  int bytes_read;
    struct addrinfo hints, *res=NULL;
    char szPort[10];
    int error;

  assert(ping_data->host);
  memset(&hints, 0, sizeof(hints));
  memset(szPort, 0, sizeof(szPort));
  snprintf(szPort, sizeof(szPort), "%d", ping_data->port);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

    error = getaddrinfo(ping_data->host, szPort, &hints, &res);
    if (error) {
      return ping_data->err = HOSTERR;
        }
 
 if ((ping_data->sock = socket(res->ai_family, res->ai_socktype, IPPROTO_TCP)) < 1)
  {
    free(res);
    return ping_data->err = CONSOCKERR;
  }

  /* Experimental. */
  flags = fcntl(ping_data->sock, F_GETFL, 0);
  if (flags != -1)
    noblock = fcntl(ping_data->sock, F_SETFL, flags | O_NONBLOCK);
  else
    noblock = -1;

  /* get start time */
  gettimeofday(&start_time, 0);

  status =
      connect(ping_data->sock, res->ai_addr, res->ai_addrlen);

  if ((status == -1) && (noblock != -1) && (errno == EINPROGRESS))
  {
    fd_set writefd;

    FD_ZERO(&writefd);
    FD_SET(ping_data->sock, &writefd);

    status =
	select((ping_data->sock + 1), NULL, &writefd, NULL,
	       &ping_data->timeout);

    /* Do we need to retry if the err is EINTR? */

    if (status > 0)
    {
      socklen_t arglen = sizeof(int);

      if (getsockopt
	  (ping_data->sock, SOL_SOCKET, SO_ERROR, &status, &arglen) < 0)
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
    close_sock(&ping_data->sock);

    if (errno == ECONNREFUSED)
    {
      free(res);
      return ping_data->err = CONREFUSED;
    } else if (errno == ETIMEDOUT)
    {
      free(res);
      return ping_data->err = PINGTIMEOUT;
    } else
    {
      free(res);
      return ping_data->err = CONERROR;
    }
  } else
  {
    flags = fcntl(ping_data->sock, F_GETFL, 0);

    if (flags != -1)
      fcntl(ping_data->sock, F_SETFL, flags & ~O_NONBLOCK);
  }

  /* setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt,
     (int) sizeof(opt)); */

  free(res);


  /*So far so good connection established */

  bytes_read =
      krecv(ping_data->sock, ping_buf, TCP_PING_PACKSIZE, 0,
	    &ping_data->timeout);
  close_sock(&ping_data->sock);

  proz_debug("bytes read = %d", bytes_read);
  if (bytes_read == -1)
  {
    if (errno == ETIMEDOUT)
      return ping_data->err = PINGTIMEOUT;
    else
      return ping_data->err = READERR;
  }

  if (bytes_read == 0 || bytes_read < TCP_PING_PACKSIZE)
    return ping_data->err = READERR;

  /* the end time */
  gettimeofday(&end_time, 0);
  proz_timeval_subtract(&ping_data->ping_time, &end_time, &start_time);


  /*  standard_ping_milli_secs =(int)((((float)ping_data->ping_time.tv_usec/1000)+(((float)ping_data->ping_time.tv_sec)*1000))*3/(float)bytes_read);

     ping_data->ping_time.tv_sec=standard_ping_milli_secs/1000;
     ping_data->ping_time.tv_usec=standard_ping_milli_secs%1000;
   */

  return ping_data->err = PINGOK;
}


void proz_mass_ping(ftps_request_t * request)
{

  request->mass_ping_running = TRUE;
  if (pthread_create(&request->mass_ping_thread, NULL,
		     (void *) &mass_ping, (void *) request) != 0)
    proz_die(_("Error: Not enough system resources"));

}

void proz_cancel_mass_ping(ftps_request_t * request)
{
  /*TODO Rewrite so that this will terminate the pingin threads as well */
  request->mass_ping_running = FALSE;
  pthread_cancel(request->mass_ping_thread);
  pthread_join(request->mass_ping_thread,0);
}

void mass_ping(ftps_request_t * request)
{
  int i, j, k = 0, num_iter, num_left, simul_pings;
  pthread_t *ping_threads;
  ping_t *ping_requests;


  simul_pings = request->max_simul_pings;

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  ping_threads = (pthread_t *) kmalloc(sizeof(pthread_t) * simul_pings);
  ping_requests = kmalloc(sizeof(ping_t) * request->num_mirrors);

  num_iter = request->num_mirrors / simul_pings;
  num_left = request->num_mirrors % simul_pings;
  proz_debug("Max simul pings=%d", simul_pings);
  proz_debug("request->num_mirrors=%d", request->num_mirrors);

  pthread_mutex_lock(&request->access_mutex);
  request->mass_ping_running = TRUE;
  pthread_mutex_unlock(&request->access_mutex);


  k = 0;

  for (i = 0; i < num_iter; i++)
  {
    for (j = 0; j < simul_pings; j++)
    {
      ping_t ping_request;

      memset(ping_requests + k, 0, sizeof(ping_request));
      /*FIXME */
      ping_requests[k].timeout.tv_sec = request->ping_timeout.tv_sec;
      ping_requests[k].timeout.tv_usec = request->ping_timeout.tv_usec;
      ping_requests[k].host = strdup(request->mirrors[k].server_name);
      ping_requests[k].port = 21;

      if (pthread_create(&ping_threads[j], NULL,
			 (void *) &tcp_ping,
			 (void *) (ping_requests + k)) != 0)
	proz_die("Error: Not enough system resources"
		 "to create thread!\n");
      k++;
    }

    k -= simul_pings;

    for (j = 0; j < simul_pings; j++)
    {
      /*Wait till the end of each thread. */
      pthread_join(ping_threads[j], NULL);
      if (ping_requests[k].err == PINGOK)
      {
	pthread_mutex_lock(&request->access_mutex);
	request->mirrors[k].milli_secs =
	    (ping_requests[k].ping_time.tv_sec * 1000) +
	    (ping_requests[k].ping_time.tv_usec / 1000);
	request->mirrors[k].status = RESPONSEOK;
	pthread_mutex_unlock(&request->access_mutex);
      } else
      {
	pthread_mutex_lock(&request->access_mutex);
	request->mirrors[k].status = NORESPONSE;
	pthread_mutex_unlock(&request->access_mutex);
      }
      k++;
    }
  }


  for (j = 0; j < num_left; j++)
  {
    ping_t ping_request;

    memset(ping_requests + k, 0, sizeof(ping_request));
    /*FIXME */
    ping_requests[k].timeout.tv_sec = request->ping_timeout.tv_sec;
    ping_requests[k].timeout.tv_usec = 0;
    ping_requests[k].host = strdup(request->mirrors[k].server_name);
    ping_requests[k].port = 21;

    if (pthread_create(&ping_threads[j], NULL,
		       (void *) &tcp_ping,
		       (void *) (&ping_requests[k])) != 0)
      proz_die("Error: Not enough system resources" "to create thread!\n");

    k++;
  }

  k -= num_left;

  for (j = 0; j < num_left; j++)
  {
    /*Wait till the end of each thread. */
    pthread_join(ping_threads[j], NULL);

    /*Wait till the end of each thread. */
    pthread_join(ping_threads[j], NULL);
    if (ping_requests[k].err == PINGOK)
    {
      pthread_mutex_lock(&request->access_mutex);
      request->mirrors[k].milli_secs =
	  (ping_requests[k].ping_time.tv_sec * 1000) +
	  (ping_requests[k].ping_time.tv_usec / 1000);
      request->mirrors[k].status = RESPONSEOK;
      pthread_mutex_unlock(&request->access_mutex);
    } else
    {
      pthread_mutex_lock(&request->access_mutex);
      request->mirrors[k].status = NORESPONSE;
      pthread_mutex_unlock(&request->access_mutex);
    }
    k++;
  }

  proz_debug("mass_ping complete.");
  pthread_mutex_lock(&request->access_mutex);
  request->mass_ping_running = FALSE;
  pthread_mutex_unlock(&request->access_mutex);
}
