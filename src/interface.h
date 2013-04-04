/* Declarations for the interface
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef INTERFACE_H

#include "prozilla.h"
//#include "connection.h"
//#include "ftpsearch_win.h"

#ifdef __cplusplus
extern "C" {
#endif

/*definnitions about the ncurse colors that the app will use*/
enum {
    NULL_PAIR = 0,
    HIGHLIGHT_PAIR,
    MSG_PAIR,
    WARN_PAIR,
    PROMPT_PAIR,
    SELECTED_PAIR
};

void initCurses();
void shutdownCurses();
void DisplayCursesInfo(download_t * download);

void curses_message(char *msg);
/* Displays the mesasge and gets the users input for overwriting files*/

int curses_query_user_input(const char *args, ...);

void PrintMessage(const char *format, ...);
void DisplayInfo(int row, int col, const char *format, ...);

#ifdef __cplusplus
}
#endif

#define INTERFACE_H
#endif
