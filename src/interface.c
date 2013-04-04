/* The  file contins routines for managing curses 
   Copyright (C) 2000 Kalum Somaratna 
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif				/*
				 * HAVE_CONFIG_H 
				 */

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif				/*
				 * HAVE_CURSES 
				 */

#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include "interface.h"
#include "misc.h"
#include "main.h"
//#include "ftpsearch_win.h"


/* This is used to convert the connection.status enums to a user
 * friendly text message */
/* the enums are defined in connection.h and are as belor
 *typedef enum {
 *    IDLE = 0,
 *    CONNECTING,
 *    LOGGININ,
 *    DOWNLOADING,
 *    COMPLETED,
 *    LOGINFAIL,
 *    CONREJECTED,
 *    REMOTEFATAL,
 *    LOCALFATAL
 *} dl_status;
 *
 * And here are the texts for the above enums.
*/
const char *dl_status_txt[] = {
    "Idle",
    "Connecting",
    "Logging in",
    "Downloading",
    "Completed",
/* I have decided to change the login failed message 
 * to something else like "login rejected" because people
 * might get alarmed and abort the download rather than 
 * letting prozilla handle it */
    "Login Denied",
    "Connect Refused",
    "Remote Fatal",
    "Local Fatal",
    "Timed Out",
    "Max attempts reached",
};

#define CTRL(x) ((x) & 0x1F)


//static int top_con = 0;	// the connection that is on the top of the display 
				 
#define MAX_CON_VIEWS 4

/*
 * needed for the message(...) function 
 */
pthread_mutex_t curses_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
char message_buffer[MAX_MSG_SIZE];

/* Added for current_dl_speed */
time_t time_stamp;
off_t bytes_got_last_time;
int start = 1;
float current_dl_speed = 0.0;
off_t total_bytes_got;

int maxRow = 0;
int maxCol = 0;
int messageRow = 0;
int messageCol = 0;


/* because of different args for different ncurses, I had to write these 
 * my self 
 */

#define kwattr_get(win,a,p,opts)	  ((void)((a) != 0 && (*(a) = (win)->_attrs)), (void)((p) != 0 && (*(p) = PAIR_NUMBER((win)->_attrs))),OK)

#define kwattr_set(win,a,p,opts) ((win)->_attrs = (((a) & ~A_COLOR) | COLOR_PAIR(p)), OK)


/* Message: prints a message to the screen */
void curses_message(char *message)
{
    short i;
    int x, y;
    attr_t attrs;
    /*
     * Lock the mutex 
     */
    pthread_mutex_lock(&curses_msg_mutex);


	/*
	 * get the cursor position 
	 */
	getyx(stdscr, y, x);
	kwattr_get(stdscr, &attrs, &i, NULL);
	move(messageRow-1, 0);
	clrtoeol();
	move(messageRow, 0);
	clrtoeol();
	move(messageRow+1, 0);
	clrtoeol();
	attrset(COLOR_PAIR(MSG_PAIR) | A_BOLD);
	mvprintw(messageRow, 0, "%s", message);
	attrset(COLOR_PAIR(NULL_PAIR));
	/*
	 * Unlock the mutex 
	 */
	refresh();
	kwattr_set(stdscr, attrs, i, NULL);
	/*
	 * set the cursor to whhere it was */

	move(y, x);
	pthread_mutex_unlock(&curses_msg_mutex);
  
}


/* Displays the mesasge and gets the users input for overwriting files*/
int curses_query_user_input(const char *args, ...)
{
	char p[MAX_MSG_SIZE+1];
    va_list vp;
    attr_t attrs;
    int x, y;
    int ch;
    int i;

    va_start(vp, args);
    vsnprintf(p,sizeof(p) , args, vp);
    va_end(vp);
    getyx(stdscr, y, x);
    kwattr_get(stdscr, &attrs, &i, NULL);

    attrset(COLOR_PAIR(PROMPT_PAIR) | A_BOLD);
    move(19, 0);
    clrtoeol();
    move(20, 0);
    clrtoeol();
    move(21, 0);
    clrtoeol();
    mvprintw(19, 0, "%s", p);
    echo();
    refresh();
    do
    {
	napms(20);
	ch = getch();
    }
    while (ch == ERR);

    refresh();
    noecho();
    kwattr_set(stdscr, attrs, i, NULL);
    /*
     * set the cursor to where it was 
     */
    move(y, x);
    /*
     * the following strange line  is for compatibility 
     */
    return islower(ch) ? toupper(ch) : ch;
}


void initCurses()
{
    initscr();
    keypad(stdscr, TRUE);
    start_color();
    noecho();
    nodelay(stdscr, TRUE);
    init_pair(HIGHLIGHT_PAIR, COLOR_GREEN, COLOR_BLACK);
    init_pair(MSG_PAIR, COLOR_YELLOW, COLOR_BLUE);
    init_pair(WARN_PAIR, COLOR_RED, COLOR_BLACK);
    init_pair(PROMPT_PAIR, COLOR_RED, COLOR_BLACK);
    init_pair(SELECTED_PAIR, COLOR_WHITE, COLOR_BLUE);
    getmaxyx(stdscr,maxRow,maxCol);
    messageRow = maxRow - 2;
    messageCol = 1;
}

void shutdownCurses()
{
  
  endwin();
  
}


void PrintMessage(const char *format, ...)
{
  va_list args;
  char message[MAX_MSG_SIZE + 1 + 1];

    va_start(args, format);
    vsnprintf((char *) &message, MAX_MSG_SIZE, format, args);
    va_end(args);

  if (rt.display_mode == DISP_CURSES)
  {
    curses_message(message);
    }
  else
  {
    if (fwrite(message, 1, strlen(message), stdout) != strlen(message))
      {
        return;
      }
  }
    delay_ms(500);
}

void DisplayInfo(int row, int col, const char *format, ...)
{
  va_list args;
  char message[MAX_MSG_SIZE + 1 + 1];

    va_start(args, format);
    vsnprintf((char *) &message, MAX_MSG_SIZE, format, args);
    va_end(args);

  if (rt.display_mode == DISP_CURSES)
  {
    attrset(COLOR_PAIR(NULL_PAIR) | A_BOLD);
    mvaddstr(row,col, message);
    attrset(COLOR_PAIR(NULL_PAIR));

    refresh();
    }
  else
  {
    if (fwrite(message, 1, strlen(message), stdout) != strlen(message))
      {
        return;
      }
  }
  //  delay_ms(300);
}

void DisplayCursesInfo(download_t * download )
{
  char buf[1000];
  int i = 0;
  int line = 1;
  int secs_left;  
  //  erase();
    refresh();
    attrset(COLOR_PAIR(HIGHLIGHT_PAIR) | A_BOLD);
      snprintf(buf, sizeof(buf), _("Connection  Server                    Status              Received"));
    mvprintw(line++,1, buf);

  attrset(COLOR_PAIR(NULL_PAIR));

    total_bytes_got = proz_download_get_total_bytes_got(download) / 1024;
  
    if (start == 1)
    {
      time_stamp = time(NULL);
      bytes_got_last_time = total_bytes_got;
      start = 0;
    }

    if (time_stamp + 1 == time(NULL))
    {
      current_dl_speed = total_bytes_got - bytes_got_last_time;
      time_stamp = time(NULL);
      bytes_got_last_time = total_bytes_got;
    }
    
  for (i = 0; i < download->num_connections; i++)
    {
      snprintf(buf,sizeof(buf), _("  %2d        %-25.25s %-15.15s %10.1fK of %.1fK"), i + 1,
	      download->pconnections[i]->u.host,
	      proz_connection_get_status_string(download->pconnections[i]),
	      (float)proz_connection_get_total_bytes_got(download->
						  pconnections[i])/1024, 
          ((float)download->pconnections[i]->main_file_size / 1024) / download->num_connections);
      mvprintw(line++,1, buf);
    }

    line = line + 2;
    attrset(COLOR_PAIR(HIGHLIGHT_PAIR) | A_BOLD);
    mvprintw(line++, 1, "%s", download->u.url);
    line++;
    
    attrset(COLOR_PAIR(HIGHLIGHT_PAIR));
    if (download->main_file_size > 0)
      mvprintw(line++,1,_("File Size = %lldK"),download->main_file_size / 1024);
    else
      mvprintw(line++,1,_("File Size = UNKNOWN"));
      
    snprintf(buf,sizeof(buf), _("Total Bytes received %lld Kb (%.2f%%)"),
	    total_bytes_got,
      (((float)total_bytes_got * 100) / ((float)download->main_file_size / 1024)));
    line++;
    mvprintw(line++,1, buf);

    snprintf(buf,sizeof(buf),_("Current speed = %1.2fKb/s, Average D/L speed = %1.2fKb/s"),
	   current_dl_speed , proz_download_get_average_speed(download) / 1024);
    mvprintw(line++,1, buf);
    clrtoeol();

    if ((secs_left = proz_download_get_est_time_left(download)) != -1)
    {
      if (secs_left < 60)
	      snprintf(buf,sizeof(buf), _("Time Remaining %d Seconds"), secs_left);
      else if (secs_left < 3600)
	      snprintf(buf,sizeof(buf), _("Time Remaining %d Minutes %d Seconds"), secs_left / 60,
		    secs_left % 60);
      else
	      snprintf(buf,sizeof(buf), _("Time Remaining %d Hours %d minutes"), secs_left / 3600,
		    (secs_left % 3600) / 60);

      mvprintw(line++,1, buf);
      clrtoeol();
      line++;
      
      attrset(COLOR_PAIR(HIGHLIGHT_PAIR) | A_BOLD);
      if (download->resume_support)
        mvprintw(line++,1,_("Resume Supported"));
      else
        mvprintw(line++,1,_("Resume NOT Supported"));
    }
  attrset(COLOR_PAIR(NULL_PAIR));
  refresh();
}
