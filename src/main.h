
#ifndef MAIN_H
#define MAIN_H

#include <sys/time.h>
#include "prozilla.h"

/* Gettext */
#include <libintl.h>
//#define _(String) dgettext (PACKAGE,String)
#define gettext_noop(String) (String)
#ifndef HAVE_GNOME
#define N_(String) gettext_noop (String)
#endif
/* Gettext */


/*We will have a runtime structure for this program */

#define PRZCONFDIR ".prozilla"

  typedef enum {
	  RESUME
  } rto;
  
  typedef enum {
    DISP_CURSES,
    DISP_STDOUT
  } DISPLAYMODE;
  
struct runtime {
  int num_connections;
  int max_redirections;
  /*
   * whether to use the netrc file 
   */
  int use_netrc;
  int ftp_use_pasv;
  int max_attempts;
  int retry_delay;		/*delay in seconds */
  /*
   * The timeout period for the connections 
   */
  struct timeval timeout;
  int itimeout;
  int debug_mode;
  int quiet_mode;
  int libdebug_mode;
  int ftp_search;
  int force_mode;
  /* The maximum number of servers to ping at once */
  int max_simul_pings;
  /* The max number of seconds to wait for a server response to ping */
  int max_ping_wait;
  /* The maximum number of servers/mirrors to request */
  int ftps_mirror_req_n;
  long max_bps_per_dl;
  /* The dir to save the generated file in */
  char *output_dir;
  /*The directory where the Dl'ed portions are stored */
  char *dl_dir;
  char *logfile_dir;
  char *home_dir;
  /*The dir where the config files are stored */
  char *config_dir;
  proxy_info *ftp_proxy;
  proxy_info *http_proxy;
  int use_http_proxy;
  int use_ftp_proxy;
  int http_no_cache;
  //int use_ftpsearch;
  int ftpsearch_server_id;
  //new options
  int resume_mode;			//
  int dont_prompt;	//don't prompt user, display message and die
  int display_mode; //curses, bare terminal, others...
  long min_search_size;  //size in K
};

extern struct runtime rt;

void shutdown(void);

#endif
