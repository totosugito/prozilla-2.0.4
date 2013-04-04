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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "prozilla.h"
#include "main.h"
#include "prefs.h"


typedef void (*prefproc) (int i, const char *const, FILE * const fp);

typedef struct prefopt_t {
  const char *varname;
  prefproc proc;
  int visible;
} prefopt_t;

void set_num_threads(int, const char *const, FILE * const);
void set_max_attempts(int, const char *const, FILE * const);

void set_max_bps_per_dl(int, const char *const, FILE * const);

void set_use_pasv(int, const char *const, FILE * const);
void set_retry_delay(int, const char *const, FILE * const);
void set_conn_timeout(int, const char *const, FILE * const);
void set_debug_mode(int, const char *const, FILE * const);
void set_libdebug_mode(int, const char *const, FILE * const);
void set_http_no_cache(int, const char *const, FILE * const);
void set_output_dir(int i, const char *const val, FILE * const fp);

void set_http_proxy(int i, const char *const val, FILE * const fp);
void set_http_proxy_username(int i, const char *const val,
			     FILE * const fp);
void set_http_proxy_passwd(int i, const char *const val, FILE * const fp);
void set_http_use_proxy(int i, const char *const val, FILE * const fp);
void set_http_proxy_type(int i, const char *const val, FILE * const fp);

void set_ftp_proxy(int i, const char *const val, FILE * const fp);
void set_ftp_proxy_username(int i, const char *const val, FILE * const fp);
void set_ftp_proxy_passwd(int i, const char *const val, FILE * const fp);
void set_ftp_use_proxy(int i, const char *const val, FILE * const fp);
void set_ftp_proxy_type(int i, const char *const val, FILE * const fp);
void set_mirrors_req(int i, const char *const val, FILE * const fp);
void set_max_simul_pings(int i, const char *const val, FILE * const fp);
void set_max_ping_wait(int i, const char *const val, FILE * const fp);
void set_use_ftpsearch(int, const char *const, FILE * const);
void set_ftpsearch_server_id(int i, const char *const val, FILE * const fp);
void set_display_mode(int i, const char *const val, FILE * const fp);
void set_search_size(int i, const char *const val, FILE * const fp);
/*TODO  add saving the proxy locations too*/


prefopt_t pref_opts[] = {
  {"threads", set_num_threads, 1},
  {"tries", set_max_attempts, 1},
  {"pasv", set_use_pasv, 1},
  {"retrydelay", set_retry_delay, 1},
  {"conntimeout", set_conn_timeout, 1},
  {"maxbpsperdl", set_max_bps_per_dl, 1},
  {"debug", set_debug_mode, 1},
  {"libdebug", set_libdebug_mode, 1},
  {"pragmanocache", set_http_no_cache, 1},
  //  {"outputdir", set_output_dir, 1},
  {"httpproxy", set_http_proxy, 1},
  {"httpproxyuser", set_http_proxy_username, 1},
  {"httpproxypassword", set_http_proxy_passwd, 1},
  {"httpproxytype", set_http_proxy_type, 1},
  {"usehttpproxy", set_http_use_proxy, 1},
  {"ftpproxy", set_ftp_proxy, 1},
  {"ftpproxyuser", set_ftp_proxy_username, 1},
  {"ftpproxypassword", set_ftp_proxy_passwd, 1},
  {"ftpproxytype", set_ftp_proxy_type, 1},
  {"useftpproxy", set_ftp_use_proxy, 1},
  {"mirrorsreq", set_mirrors_req, 1},
 {"maxsimulpings", set_max_simul_pings, 1},
 {"maxpingwait", set_max_ping_wait, 1},
 {"defaultftpsearch", set_use_ftpsearch, 1},
 {"ftpsearchserverid", set_ftpsearch_server_id, 1},
 {"displaymode",set_display_mode, 1},
 {"minsearchsize",set_search_size, 1},
  {NULL, 0, 0}
};

void set_num_threads(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.num_connections);
  } else
  {
    rt.num_connections = atoi(val);
    if (rt.num_connections <= 0 || rt.num_connections > 30)
      rt.num_connections = 4;
  }
}


void set_max_attempts(int i, const char *const val, FILE * const fp)
{
  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.max_attempts);
  } else
  {
    rt.max_attempts = atoi(val);
    if (rt.max_attempts < 0)
      rt.max_attempts = 0;
  }
}

void set_retry_delay(int i, const char *const val, FILE * const fp)
{
  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.retry_delay);
  } else
  {
    rt.retry_delay = atoi(val);
    if (rt.retry_delay < 0)
      rt.retry_delay = 15;
  }
}


void set_conn_timeout(int i, const char *const val, FILE * const fp)
{
  if (fp != NULL)
  {
    fprintf(fp, "%d", (int) rt.timeout.tv_sec);
  } else
  {
    rt.timeout.tv_sec = atoi(val);
    rt.timeout.tv_usec = 0;

    if (rt.timeout.tv_sec < 0)
      rt.timeout.tv_sec = 90;
  }
}


void set_max_bps_per_dl(int i, const char *const val, FILE * const fp)
{
  if (fp != NULL)
  {
    fprintf(fp, "%d", (int) rt.max_bps_per_dl);
  } else
  {
    rt.max_bps_per_dl = atoi(val);

    if (rt.max_bps_per_dl < 0)
      rt.max_bps_per_dl = 0;
  }

}

void set_use_pasv(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.ftp_use_pasv);
  } else
  {
    rt.ftp_use_pasv = atoi(val);
  }

}


void set_debug_mode(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.debug_mode);
  } else
  {
    rt.debug_mode = atoi(val);
  }
}

void set_libdebug_mode(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.libdebug_mode);
  } else
  {
    rt.libdebug_mode = atoi(val);
  }
}

void set_http_no_cache(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.http_no_cache);
  } else
  {
    rt.http_no_cache = atoi(val);
  }
}

void set_output_dir(int i, const char *const val, FILE * const fp)
{
  if (fp != NULL)
  {
    fprintf(fp, "%s", rt.output_dir);
  } else
  {
    free(rt.output_dir);
    rt.output_dir = strdup(val);
  }

}


void set_http_proxy(int i, const char *const val, FILE * const fp)
{

  uerr_t err;
  urlinfo url_data;

  if (fp != NULL)
  {
    fprintf(fp, "%s:%d", rt.http_proxy->proxy_url.host,
	    rt.http_proxy->proxy_url.port);
  } else
  {
    err = proz_parse_url(val, &url_data, 0);
    if (err != URLOK)
    {
      proz_debug("%s does not seem to be a valid  proxy value", val);
      return;
    }
    proz_free_url(&rt.http_proxy->proxy_url, 0);
    memcpy(&rt.http_proxy->proxy_url, &url_data, sizeof(url_data));
  }
}

void set_http_proxy_username(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%s", rt.http_proxy->username);
  } else
  {
    free(rt.http_proxy->username);
    rt.http_proxy->username = strdup(val);
  }
}



void set_http_proxy_passwd(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%s", rt.http_proxy->passwd);
  } else
  {
    free(rt.http_proxy->passwd);
    rt.http_proxy->passwd = strdup(val);
  }
}


void set_http_proxy_type(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", (int) rt.http_proxy->type);
  } else
  {
    rt.http_proxy->type = (proxy_type) atoi(val);
  }
}



void set_http_use_proxy(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.use_http_proxy);
  } else
  {
    rt.use_http_proxy = atoi(val);
  }
}




void set_ftp_proxy(int i, const char *const val, FILE * const fp)
{

  uerr_t err;
  urlinfo url_data;

  if (fp != NULL)
  {
    fprintf(fp, "%s:%d", rt.ftp_proxy->proxy_url.host,
	    rt.ftp_proxy->proxy_url.port);
  } else
  {
    err = proz_parse_url(val, &url_data, 0);
    if (err != URLOK)
    {
      proz_debug("%s does not seem to be a valid  proxy value", val);
      return;
    }
    proz_free_url(&rt.ftp_proxy->proxy_url, 0);
    memcpy(&rt.ftp_proxy->proxy_url, &url_data, sizeof(url_data));
  }
}

void set_ftp_proxy_username(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%s", rt.ftp_proxy->username);
  } else
  {
    free(rt.ftp_proxy->username);
    rt.ftp_proxy->username = strdup(val);
  }
}


void set_ftp_proxy_passwd(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%s", rt.ftp_proxy->passwd);
  } else
  {
    free(rt.ftp_proxy->passwd);
    rt.ftp_proxy->passwd = strdup(val);
  }
}

void set_ftp_proxy_type(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", (int) rt.ftp_proxy->type);
  } else
  {
    rt.ftp_proxy->type = (proxy_type) atoi(val);
  }
}

void set_ftp_use_proxy(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.use_ftp_proxy);
  } else
  {
    rt.use_ftp_proxy = atoi(val);
  }
}


void set_mirrors_req(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.ftps_mirror_req_n);
  } else
  {
    rt.ftps_mirror_req_n = atoi(val);
    if (rt.ftps_mirror_req_n <= 0 || rt.ftps_mirror_req_n> 1000)
      rt.ftps_mirror_req_n = 40;
  }
}

void set_max_simul_pings(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.max_simul_pings);
  } else
  {
    rt.max_simul_pings= atoi(val);
    if (rt.max_simul_pings <= 0 || rt.max_simul_pings> 30)
      rt.max_simul_pings=5;
  }
}



void set_max_ping_wait(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.max_ping_wait);
  } else
  {
    rt.max_ping_wait= atoi(val);
    if (rt.max_ping_wait <= 0 || rt.max_ping_wait> 30)
      rt.max_ping_wait=5;
  }
}


void set_use_ftpsearch(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.ftp_search);
  } else
  {
    rt.ftp_search = atoi(val);
  }

}

void set_ftpsearch_server_id(int i, const char *const val, FILE * const fp)
{
  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.ftpsearch_server_id);
  } else
  {
    rt.ftpsearch_server_id = atoi(val);
    if (rt.ftpsearch_server_id < 0)
      rt.ftpsearch_server_id = 0;
    else if (rt.ftpsearch_server_id > 1)
      rt.ftpsearch_server_id = 1;
    
  }
}

void set_display_mode(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%d", rt.display_mode);
  } else
  {
    rt.display_mode = atoi(val);
  }
}

void set_search_size(int i, const char *const val, FILE * const fp)
{

  if (fp != NULL)
  {
    fprintf(fp, "%ld", rt.min_search_size);
  } else
  {
    rt.min_search_size = atoi(val);
  }
}

void save_prefs()
{
  char config_fname[PATH_MAX];
  FILE *fp;
  int i;

  snprintf(config_fname, PATH_MAX, "%s/.prozilla/%s", rt.home_dir,
	   "prozconfig");

  if ((fp = fopen(config_fname, "wt")) == NULL)
  {
    perror("could not save preferences file");
    proz_debug("could not save preferences file :- %s", strerror(errno));
    return;
  }

  fprintf(fp, "%s",
	  "# ProZilla preferences file\n# This file is loaded and OVERWRITTEN each time ProZilla is run.\n# Please try to avoid writing to this file.\n#\n");

  for (i = 0; pref_opts[i].varname != NULL; i++)
  {
    fprintf(fp, "%s=", pref_opts[i].varname);
    (*pref_opts[i].proc) (i, NULL, fp);
    fprintf(fp, "\n");
  }
  fclose(fp);
}

void load_prefs()
{
  char config_fname[PATH_MAX];
  FILE *fp;
  int i;
  char line[256];
  char *tok1, *tok2;

  snprintf(config_fname, PATH_MAX, "%s/.prozilla/%s", rt.home_dir,
	   "prozconfig");

  if ((fp = fopen(config_fname, "rt")) == NULL)
  {

    if (errno == ENOENT)	/*Create the file then if it doesnt exist */
    {
      save_prefs();
      return;
    }

    else
    {
      perror(_("could not open preferences file for reading"));
      proz_debug("could not open preferences file :- %s", strerror(errno));
      return;
    }
  }

  line[sizeof(line) - 1] = '\0';
  while (fgets(line, sizeof(line) - 1, fp) != NULL)
  {
    tok1 = strtok(line, " =\t\r\n");
    if ((tok1 == NULL) || (tok1[0] == '#'))
      continue;
    tok2 = strtok(NULL, "\r\n");
    if (tok2 == NULL)
      continue;

    for (i = 0; pref_opts[i].varname != NULL; i++)
    {
      if (strcmp(tok1, pref_opts[i].varname) == 0)
      {
	if (pref_opts[i].proc != NULL)
	  (*pref_opts[i].proc) (i, tok2, NULL);
      }
    }
  }

  fclose(fp);
}
