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

/* Debugging routines. */

/* $Id: debug.c,v 1.20 2001/08/17 21:53:39 kalum Exp $ */


#include "common.h"
#include "misc.h"
#include "prozilla.h"
#include "debug.h"


static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;


/******************************************************************************
Initialises the debug system, deletes any prior debug.log file if present 
******************************************************************************/
void debug_init()
{
  proz_debug_delete_log();
  return;
}


void proz_debug_delete_log()
{

  char logfile_name[PATH_MAX];
  snprintf(logfile_name, PATH_MAX, "%s/debug.log", libprozrtinfo.log_dir);


  if (unlink(logfile_name) == -1)
  {
    /*
     * if the file is not present the continue silently 
     */
    if (errno == ENOENT)
      return;
    else
    {
      proz_debug(_("unable to delete the file %s. Reason-: %s"),
		 strerror(errno));
    }
  }
}

/******************************************************************************
 Write a message to the debug-file.
******************************************************************************/
void proz_debug(const char *format, ...)
{
  FILE *fp;
  va_list args;
  char message[MAX_MSG_SIZE + 1 + 1];

  char logfile_name[PATH_MAX];
  snprintf(logfile_name, PATH_MAX, "%s/debug.log", libprozrtinfo.log_dir);


  pthread_mutex_lock(&debug_mutex);

  if (libprozrtinfo.debug_mode == TRUE)
  {
    va_start(args, format);
    vsnprintf((char *) &message, MAX_MSG_SIZE, format, args);
    va_end(args);

    /* Remove all newlines from the end of the string. */
    while ((message[strlen(message) - 1] == '\r')
	   || (message[strlen(message) - 1] == '\n'))
      message[strlen(message) - 1] = '\0';

    /* Append a newline. */
    message[strlen(message) + 1] = '\0';
    message[strlen(message)] = '\n';

    if (!(fp = fopen(logfile_name, "at")))
    {
      pthread_mutex_unlock(&debug_mutex);
      return;
    }

    if (fwrite(message, 1, strlen(message), fp) != strlen(message))
    {
      pthread_mutex_unlock(&debug_mutex);
      fclose(fp);
      return;
    }

    fclose(fp);
  }

  pthread_mutex_unlock(&debug_mutex);
}
