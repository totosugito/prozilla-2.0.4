#ifndef DOWNLOAD_WIN_H
#define DOWNLOAD_WIN_H

#include <config.h>
#include <ctype.h>
#include "prozilla.h"
#include "ftpsearch_win.h"

void ms(const char *msg, void *cb_data);

typedef enum {
  DL_IDLING,
  DL_GETTING_INFO,
  DL_FTPSEARCHING,
  DL_DLPRESTART,
  DL_DOWNLOADING,
  DL_JOINING,
  DL_RESTARTING,
  DL_PAUSED,
  DL_ABORTED,
  DL_FATALERR,
  DL_DONE
} dlwin_status_t;


//The running dialog type 
typedef enum {
  DLG_GENERIC,
  DLG_URLNSFOD,
  DLG_TARGETERASE,
  DLG_JOININING,
  DLG_PREVRESUME,
  DLG_ABORT,
  DLG_UNKNOWNERR,
} dlg_class;

class DL_Window {

public:
   DL_Window(urlinfo * url_data);
   ~DL_Window();

  void dl_start(int num_connections, boolean ftpsearch);
  void my_cb();

  void handle_info_thread();
  void handle_ftpsearch();
  void handle_download_thread();

  void start_download();

  void handle_joining_thread();
  void handle_dl_fatal_error();
  void cleanup(boolean erase_dlparts);
  //void DL_Window::print_status(download_t * download, int quiet_mode);
  void print_status(download_t * download, int quiet_mode);

  connection_t *connection;
  download_t *download;
  urlinfo u;
  boolean got_info;
  boolean got_dl;

  dlwin_status_t status;

  pthread_t info_thread;
  pthread_mutex_t getinfo_mutex;
  int num_connections;
  /*The time  elapsed since the last update */
  struct timeval update_time;

private:
  void do_download();
  void handle_prev_download();
  boolean joining_thread_running;
  boolean do_ftpsearch;
  boolean using_ftpsearch;
  FTPS_Window *ftpsearch_win;
  int askUserResume(connection_t *connection, boolean resumeSupported);
  int askUserOverwrite(connection_t *connection);
};

#endif
