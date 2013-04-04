/******************************************************************************
 fltk prozilla - a front end for prozilla, a download accelerator library
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif				/*
				 * HAVE_CONFIG_H 
				 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "prozilla.h"
#include "main.h"
#include "init.h"



extern struct runtime rt;

/* Sets the default config */
void set_defaults()
{

  struct stat st;
  char cwd[PATH_MAX];
  /*
   * Zero the structure which holds the config data 
   */
  memset(&rt, 0, sizeof(rt));

  if (getcwd(cwd, sizeof(cwd)) == NULL)
  {
    proz_debug("Failed to get the current working directory");
    strcpy(cwd, ".");
  }

  if (rt.home_dir != 0)
    free(rt.home_dir);
  rt.home_dir = strdup(libprozrtinfo.home_dir);

  /*TODO  what to do if the homedir is NULL */
  if (rt.config_dir != 0)
    free(rt.config_dir);
  rt.config_dir = (char *) malloc(PATH_MAX);
  snprintf(rt.config_dir, PATH_MAX, "%s/%s", rt.home_dir, PRZCONFDIR);
  /* Make the ~/.prozilla directory if necessary */

  if (stat(rt.config_dir, &st) == -1)
  {
    /*error has hapenned */
    if (errno == ENOENT)
    {
      /*Create the dir then */
      if (mkdir(rt.config_dir, S_IRWXU) != 0)
      {
	perror
	    (_("unable to create the directory to store the config info in"));
	exit(0);
      }
    } else
      perror(_("Error while stating the config info directory"));
  }

  /*Output the file to the directory , cwd by default */

  if (rt.output_dir != 0)
    free(rt.output_dir);
  rt.output_dir = strdup(cwd);

  if (rt.dl_dir != 0)
    free(rt.dl_dir);
  rt.dl_dir = strdup(cwd);

  if (rt.logfile_dir != 0)
    free(rt.logfile_dir);
  rt.logfile_dir = strdup(rt.config_dir);


  /*
   * The default no of connections and maximum redirections allowed 
   */
  rt.num_connections = 4;
  rt.max_redirections = 10;
  /* Uses PASV by default 
   */
  rt.ftp_use_pasv = libprozrtinfo.ftp_use_pasv;
  /*
   * The force option, off by default when enabled 
   * cause Prozilla not to prompt the user about overwriting existent
   * files etc..
   */
  rt.force_mode = FALSE;
  /*
   * .netrc options 
   */
  rt.use_netrc = TRUE;


  /*
   * The max number of trys and the delay between each 
   */
  rt.max_attempts = 0;		/*TODO it is currently UNLIMITED */
  rt.retry_delay = 15;		/*
				 * delay in seconds 
				 */

  /*Default is to not log any debug info */
  rt.debug_mode = FALSE;
  rt.quiet_mode = TRUE;
  rt.libdebug_mode = TRUE;
  rt.ftp_search = FALSE;
  rt.min_search_size=128;
  rt.max_simul_pings = 5;
  rt.max_ping_wait = 8;
  rt.ftps_mirror_req_n = 40;

  rt.max_bps_per_dl = 0;	/* 0= No throttling */
 

  if (rt.http_proxy != 0)
    free(rt.http_proxy);
  rt.http_proxy = (proxy_info *) malloc(sizeof(proxy_info));
  rt.http_proxy->username = strdup("");
  rt.http_proxy->passwd = strdup("");
  rt.http_proxy->type = HTTPPROXY;
  proz_parse_url("localhost:3128", &rt.http_proxy->proxy_url, 0);
  rt.use_http_proxy = FALSE;

  if (rt.ftp_proxy != 0)
    free(rt.ftp_proxy);
  rt.ftp_proxy = (proxy_info *) malloc(sizeof(proxy_info));
  rt.ftp_proxy->username = strdup("");
  rt.ftp_proxy->passwd = strdup("");
  rt.ftp_proxy->type = HTTPPROXY;
  proz_parse_url("localhost:3128", &rt.ftp_proxy->proxy_url, 0);
  rt.use_ftp_proxy = FALSE;
  rt.http_no_cache = FALSE;
  rt.timeout.tv_sec = 90;
  rt.timeout.tv_usec = 0;
  //rt.use_ftpsearch=FALSE;
  rt.ftpsearch_server_id = 1;
  /*Set the values necessary for libprozilla */
  rt.display_mode = DISP_CURSES;
  set_runtime_values();
}



/*This sets the runtime values to libprozilla from the runtime structure */
void set_runtime_values()
{
  struct timeval tv;

  proz_set_connection_timeout(&rt.timeout);
  tv.tv_sec = rt.retry_delay;
  tv.tv_usec = 0;
  proz_set_connection_retry_delay(&tv);
  libprozrtinfo.ftp_use_pasv = rt.ftp_use_pasv;
  libprozrtinfo.http_no_cache = rt.http_no_cache;
  proz_set_output_dir(rt.output_dir);
  /*FIXME */
  proz_set_download_dir(rt.output_dir);
  proz_set_logfile_dir(rt.logfile_dir);

  proz_set_http_proxy(rt.http_proxy);
  proz_use_http_proxy(rt.use_http_proxy);
  proz_set_ftp_proxy(rt.ftp_proxy);
  proz_use_ftp_proxy(rt.use_ftp_proxy);

  libprozrtinfo.max_bps_per_dl = rt.max_bps_per_dl;
  libprozrtinfo.debug_mode = rt.libdebug_mode;
  libprozrtinfo.max_attempts = rt.max_attempts;
}


void cleanuprt()
{

  if (rt.config_dir != 0)
    free(rt.config_dir);

  if (rt.http_proxy != 0)
    free(rt.http_proxy);

  if (rt.ftp_proxy != 0)
    free(rt.ftp_proxy);

  if (rt.home_dir != 0)
    free(rt.home_dir);

  if (rt.output_dir != 0)
    free(rt.output_dir);

  if (rt.dl_dir != 0)
    free(rt.dl_dir);

  if (rt.logfile_dir != 0)
    free(rt.logfile_dir);

  }
