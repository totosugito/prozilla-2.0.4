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

/* Main include-file. */

/* $Id: prozilla.h,v 1.63 2005/09/19 15:25:48 kalum Exp $ */


#ifndef PROZILLA_H
#define PROZILLA_H

#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include "netrc.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    /*Connect establishment related values */
    NOCONERROR, HOSTERR, CONSOCKERR, CONERROR,
    CONREFUSED, ACCEPTERR, ACCEPTOK, BINDERR, BINDOK, LISTENERR, LISTENOK,
    SERVERCLOSECONERR, CONPORTERR,

    /* URL handling related value */
    URLOK, URLHTTP, URLFTP, URLFILE, URLUNKNOWN, URLBADPORT,
    URLBADHOST, URLBADPATTERN,

    /* HTTP related values */
    NEWLOCATION, HOK, HEOF, HERR, HAUTHREQ, HAUTHFAIL,
    HTTPNSFOD,

    /*FTP related value */
    FTPOK, FTPLOGINC, FTPLOGREFUSED, FTPPORTERR,
    FTPNSFOD, FTPRETROK, FTPUNKNOWNTYPE, FTPUNKNOWNCMD, FTPSIZEFAIL,
    FTPERR, FTPRESTFAIL, FTPACCDENIED,
    FTPPWDERR,
    FTPINVPASV, FTPNOPASV, FTPCONREFUSED, FTPCWDFAIL, FTPPWDFAIL,
    FTPSERVCLOSEDATLOGIN,
    /*FTP parsing related values */
    FTPPARSEOK, FTPPARSENOTEXIST, FTPPARSEFAIL,

    /*Error values that can happen due to failed proxie request */
    GATEWAYTIMEOUT, SERVICEUNAVAIL, BADGATEWAY, INTERNALSERVERR,
    UNKNOWNREQ,

    /*File handling related return values. */
    FOPENERR, FWRITEERR,
    RETROK,

    /*Download related retrn values. */
    DLINPROGRESS, DLABORTED, DLDONE, CANTRESUME, DLLOCALFATAL,
	DLREMOTEFATAL,
    /*FTPSEARCH+ping  related return values. */
    FTPSINPROGRESS, MASSPINGINPROGRESS, FTPSFAIL, MASSPINGDONE,

    /*PING related values */
    PINGOK, PINGTIMEOUT,
    /*Misc Value */
    RETRFINISHED, READERR,

    PROXERR, WRITEERR,
    FILEISDIR,
    MIRINFOK, MIRPARSEOK, MIRPARSEFAIL, FILEGETOK,
    /*Related to file joining */
   JOININPROGRESS, JOINDONE, JOINERR 
  } uerr_t;


#define FTP_BUFFER_SIZE   2048
#define HTTP_BUFFER_SIZE  2048
#define MAX_MSG_SIZE      1024

#define DEFAULT_FTP_PORT  21
#define DEFAULT_HTTP_PORT 80

/* This is used when no password is found or specified. */
#define DEFAULT_FTP_USER   "anonymous"
#define DEFAULT_FTP_PASSWD "billg@hotmail.com"

/* The D/L ed fragments will be saved to files with this extension.
 * E.g.: gnu.jpg.prz0, gnu.jpg.prz1 etc... */
#define DEFAULT_FILE_EXT ".prz"

/* The extension for the log file created. */
#define DEFAULT_LOG_EXT ".log"

#define DEFAULT_USER_AGENT "Prozilla"

  typedef char longstring[1024];

/* Callback message function. */
  typedef void (*message_proc) (const char *msg, void *cb_data);


/* Structure containing info on a URL. */
  typedef struct _urlinfo {
    char *url;			/* Unchanged URL. */
    uerr_t proto;		/* URL protocol. */
    char *host;			/* Extracted hostname. */
    unsigned short port;
    char ftp_type;
    char *path, *dir, *file;	/* Path, dir and file (properly decoded). */
    char *user, *passwd;	/* For FTP. */

    char *referer;		/* The source from which the request
				   URI was obtained. */
  } urlinfo;

  typedef enum {
    USERatSITE,
    USERatPROXYUSERatSITE,
    USERatSITE_PROXYUSER,
    PROXYUSERatSITE,
    LOGINthenUSERatSITE,
    OPENSITE,
    SITESITE,
    HTTPPROXY,
    FTPGATE,
    WINGATE
  } proxy_type;


  typedef enum {
    IDLE = 0,
    CONNECTING,
    LOGGININ,
    DOWNLOADING,
    COMPLETED,
    LOGINFAIL,
    CONREJECT,
    REMOTEFATAL,
    LOCALFATAL,
    TIMEDOUT,
    MAXTRYS
  } dl_status;


  typedef enum {
    NOT_FOUND,
    REGULAR_FILE,
    DIRECTORY,
    SYMBOLIC_LINK,
    UNKNOWN
  } file_type_t;


  typedef struct {
    off_t len;			/* Received length. */
    off_t contlen;		/* Expected length. */
    int res;			/* The result of last read. */

    /* -1 = Accept ranges not found.
       0  = Accepts range is none.
       1  = Accepts ranges. */
    int accept_ranges;

    char *newloc;		/* New location (redirection). */
    char *remote_time;		/* Remote time-stamp string. */
    char *error;		/* Textual HTTP error. */
    int statcode;		/* Status code. */
  } http_stat_t;

  typedef struct {
    urlinfo proxy_url;
    char *username;
    char *passwd;
    proxy_type type;
    boolean use_proxy;
  } proxy_info;

  typedef struct libprozinfo {
    int argc;
    char **argv;
    boolean debug_mode;

    /* For netrc. */
    netrc_entry *netrc_list;
    boolean use_netrc;

    char *home_dir;
    char *ftp_default_user;
    char *ftp_default_passwd;
    char *dl_dir;
    char *output_dir;
    char *log_dir;
    boolean ftp_use_pasv;
    proxy_info *ftp_proxy;
    proxy_info *http_proxy;
    boolean http_no_cache;
    /* the default timeout for all the connection types (ctrl, data etc) */
    struct timeval conn_timeout;
    struct timeval conn_retry_delay;
    int max_attempts;
    long max_bps_per_dl;
  } libprozinfo;

  extern libprozinfo libprozrtinfo;


  typedef struct response_line {
    char *line;
    struct response_line *next;
  } response_line;


  typedef struct connection_t {

    /* struct which contains the parsed url info. It includes the remote file,
       path,protocol etc. */
    urlinfo u;

    /* The error status of the connection. */
    uerr_t err;

    /* Proxy specific info. */

    proxy_info *ftp_proxy;
    proxy_info *http_proxy;

    /* Netrc. */
    boolean use_netrc;

    /* FTP specific info. */
    boolean ftp_use_pasv;
    struct timeval xfer_timeout;
    struct timeval conn_timeout;
    struct timeval ctrl_timeout;
    unsigned char pasv_addr[6];
    int ctrl_sock;
    int data_sock;
    int listen_sock;

    /* Additional info about what this URL is. */
    /* FIXME Should this be in the url_info struct? */
    file_type_t file_type;

    /* The lines that the server returned. */
    response_line *serv_ret_lines;

    /* Does the server support resume? */
    boolean resume_support;

    /* The file name to save the data to locally. */
    char *localfile;

    /* Pointer to file that we will be saving the data to locally. */
    FILE *fp;

    /* Tells whether to open the file for appending or for writing etc. 
       Used for adding resume support. */
    char *file_mode;

    /* FIXME Add an enum here to say whether run mode is resume or normal etc.
       and remove the file mode. */

    off_t remote_startpos;
    off_t orig_remote_startpos;
    off_t remote_endpos;
    off_t remote_bytes_received;

    off_t main_file_size;

    /* The permanent base offset from the beginning of the file, put in
       anticipation of making the threads download to a single file. */
    off_t local_base_offset;

    /* Indicates the startpos of the localfile. It is always 0 in normal mode
       and can be any positive value in resume mode. */
    off_t local_startpos;

    /* The start position at the beginning of the download. */
    off_t orig_local_startpos;
    /*    long bytes_xferred; */
    dl_status status;
    char *szBuffer;

    /* Tells the download thread whether to abort the download or not. */
    boolean abort;

    /* Information about the connection's start and end time. */
    struct timeval time_begin;
    struct timeval time_end;

    /* Info about whether to retry the thread or not. */
    boolean retry;

    /* The number of attempts to try to complete a connection. 0 means unlimited
       connection attempts. */
    int max_attempts;
    /* The number of attempts that this connection has been tried */
    int attempts;

    /* The time when to try to restart the connection. */
    struct timeval retry_delay;

    /* Each connection will acquire this mutex before changing state. */
    pthread_mutex_t *status_change_mutex;
    /*This will be broadcast when the connection starts connecting */
    pthread_cond_t connecting_cond;

    /* User agent for HTTP. */
    char *user_agent;

    http_stat_t hs;
    message_proc msg_proc;
    /*additional callback data whcih is specified by the user 
       that is passed to msg_proc */
    void *cb_data;
    /* Indicates whether a conenction is running or not */
    int running;
    /*Mutex used to lock acesss to data in this struct
       that is accesed/written by other threads */
    pthread_mutex_t access_mutex;
    /*This indicates that we should use the pragma no-cache directive for http proxies */
    boolean http_no_cache;
    /*The rate of this connection whcih is calcualted */
    long rate_bps;
    /*We limit the connections speed to this */
    long max_allowed_bps;
  } connection_t;


  typedef enum {
    UNTESTED = 0, RESPONSEOK, NORESPONSE, ERROR
  } ftp_mirror_stat_t;

  typedef enum {
    LYCOS, FILESEARCH_RU
  } ftpsearch_server_type_t;


  typedef struct {
    char *path;
    boolean valid;
  } mirror_path_t;

  typedef struct ftp_mirror {
    char *server_name;
    mirror_path_t *paths;
    char *file_name;
    char *full_name;
    char *file_size;
    struct timeval tv;
    int milli_secs;
    int num_paths;
    ftp_mirror_stat_t status;
    int copied;
    boolean resume_supported;
    int max_simul_connections;
  } ftp_mirror_t;

  typedef struct {
    off_t file_size;
    char *file_name;
    connection_t *connection;
    ftpsearch_server_type_t server_type;
    ftp_mirror_t *mirrors;
    int num_mirrors;
    uerr_t err;
    boolean info_running;
    boolean mass_ping_running;
    pthread_mutex_t access_mutex;
    pthread_t info_thread;
    pthread_t mass_ping_thread;
    int max_simul_pings;
    struct timeval ping_timeout;
    urlinfo *requested_url;
  } ftps_request_t;


  typedef struct download_t {
    urlinfo u;
    char *dl_dir;
    char *log_dir;
    char *output_dir;
    connection_t **pconnections;
    pthread_t *threads;
    pthread_mutex_t status_change_mutex;
    int num_connections;
    /* Optional will be called back with info about download. */
    message_proc msg_proc;
    /*additional callback data which is specified by the user 
       that is passed to msg_proc */
    void *cb_data;
    off_t main_file_size;
    boolean resume_mode;
    struct timeval start_time;
    /*Does this DL support resume? */
    boolean resume_support;
    /*This contains the building status, 1 = building,0 build finished,  -1 error occured */
    int building;
    /*This is the percentage of the file that is been built currently */
    float file_build_percentage;
    int max_simul_connections;
    /*Mutex used to lock acesss to data in this struct
       that is accesed/written by other threads */
    pthread_mutex_t access_mutex;
    /* The message that is returned when the file build process is finished 
     */
    char *file_build_msg;
    /*Max mps for this download */
    long max_allowed_bps;
    ftps_request_t *ftps_info;
    boolean using_ftpsearch;
    pthread_t join_thread;
	} download_t;


  typedef struct {
    /* the number of connections that this download was started with */
    int num_connections;
    /*I have added these newly */
    int version;		/*The version of this logfile */
    /*Indicates whether we have got info  about the files size, final URL etc */
    boolean got_info;
    off_t file_size;
    int url_len;		/*The length in bytes of the url text stred in the log file */
    char *url;
    int reserved[30];
  } logfile;


  /*Structs for logfile handling */



  typedef struct {
    char *host;
    int port;
    struct timeval timeout;
    struct timeval ping_time;
    int sock;
    uerr_t err;
  } ping_t;


/*Functions for URL parsing */
  uerr_t proz_parse_url(const char *url, urlinfo * u, boolean strict);
  urlinfo *proz_copy_url(urlinfo * u);
  void proz_free_url(urlinfo * u, boolean complete);

/*Functions that set values which will apply for all conenctions. */
  int proz_init(int argc, char **argv);
  void proz_shutdown(void);
  void proz_die(const char *message, ...);

  void proz_set_http_proxy(proxy_info * proxy);
  void proz_set_ftp_proxy(proxy_info * proxy);
  void proz_use_ftp_proxy(boolean use);
  void proz_use_http_proxy(boolean use);
  void proz_set_connection_timeout(struct timeval *timeout);
  void proz_set_connection_retry_delay(struct timeval *delay);
  void proz_set_download_dir(char *dir);
  void proz_set_logfile_dir(char *dir);
  void proz_set_output_dir(char *dir);
  char *proz_get_libprozilla_version();

  /*Functions for loggind debug messages */
  void proz_debug(const char *format, ...);
  void proz_debug_delete_log();

/*Functions which are for handling a  single connection. */
  connection_t * proz_connection_init(urlinfo * url, pthread_mutex_t * mutex);
  void proz_connection_set_url(connection_t * connection, urlinfo *url);

  char *proz_connection_get_status_string(connection_t * connection);
  off_t proz_connection_get_total_bytes_got(connection_t * connection);
  void proz_get_url_info_loop(connection_t * connection, pthread_t *thread);
  off_t proz_connection_get_total_remote_bytes_got(connection_t *
						  connection);
  void proz_connection_set_msg_proc(connection_t * connection,
				    message_proc msg_proc, void *cb_data);
  void proz_connection_free_connection(connection_t * connection,
				       boolean complete);
  dl_status proz_connection_get_status(connection_t * connection);
  boolean proz_connection_running(connection_t * connection);

/*Functions which are for handling a download */
  download_t *proz_download_init(urlinfo * u);
  int proz_download_setup_connections_no_ftpsearch(download_t * download,
						   connection_t *
						   connection,
						   int req_connections);
  void proz_download_start_downloads(download_t * download,
				     boolean resume);
  int proz_download_load_resume_info(download_t * download);
  void proz_download_stop_downloads(download_t * download);

  boolean proz_download_all_dls_status(download_t * download,
				       dl_status status);
  boolean proz_download_all_dls_err(download_t * download, uerr_t err);

  boolean proz_download_all_dls_filensfod(download_t * download);
  boolean proz_download_all_dls_ftpcwdfail(download_t * download);


  off_t proz_download_get_total_bytes_got(download_t * download);
  uerr_t proz_download_handle_threads(download_t * download);
  int proz_download_prev_download_exists(download_t * download);
  float proz_download_get_average_speed(download_t * download);
  int proz_download_delete_dl_file(download_t * download);
  void proz_download_wait_till_all_end(download_t * download);
  off_t proz_download_get_total_remote_bytes_got(download_t * download);
  void proz_download_set_msg_proc(download_t * download,
				  message_proc msg_proc, void *cb_data);
  off_t proz_download_get_est_time_left(download_t * download);
  void proz_download_free_download(download_t * download,
				   boolean complete);
  int proz_download_target_exist(download_t * download);
  int proz_download_delete_target(download_t * download);

  /* Functions related to handling the logfile created for a download */
  int proz_log_read_logfile(logfile * lf, download_t * download,
			    boolean load_con_info);
  int proz_log_delete_logfile(download_t * download);
  int proz_log_logfile_exists(download_t * download);
  char *proz_strerror(uerr_t error);

  /*Ftpsearch releated */
ftps_request_t * proz_ftps_request_init(
			       urlinfo * requested_url, off_t file_size,
			       char *ftps_loc,
			       ftpsearch_server_type_t server_type,
			       int num_req_mirrors);
  void proz_get_complete_mirror_list(ftps_request_t * request);
  boolean proz_request_info_running(ftps_request_t * request);
  boolean proz_request_mass_ping_running(ftps_request_t * request);
  void proz_mass_ping(ftps_request_t * request);
  void proz_sort_mirror_list(ftp_mirror_t * mirrors, int num_servers);
  void proz_cancel_mirror_list_request(ftps_request_t * request);

  int proz_download_setup_connections_ftpsearch(download_t * download,
						connection_t * connection,
						ftps_request_t * request,
						int req_connections);
  void proz_cancel_mass_ping(ftps_request_t * request);

 /*Misc functions */
  int proz_timeval_subtract(struct timeval *result, struct timeval *x,
			    struct timeval *y);

  /*Funcs related to joining the downloaded file portions */
  void proz_download_join_downloads(download_t * download);
  uerr_t proz_download_get_join_status(download_t *download);
  float proz_download_get_file_build_percentage(download_t *download);
  void proz_download_cancel_joining_thread(download_t * download);
  void proz_download_wait_till_end_joining_thread(download_t * download);

#ifdef __cplusplus
}
#endif
#endif				/* PROZILLA_H */
