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

/* Miscellaneous routines. */

/* $Id: misc.h,v 1.25 2001/09/02 23:29:16 kalum Exp $ */


#ifndef MISC_H
#define MISC_H


#include "common.h"
#include "prozilla.h"

void *kmalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);
char *kstrdup(const char *str);

boolean is_number(const char *str);
int numdigit(long a);
char *strdupdelim(const char *beg, const char *end);
void prnum(char *where, long num);

int setargval(char *optstr, int *num);
void base64_encode(const char *s, char *store, int length);
char *home_dir(void);

void delay_ms(int ms);
int close_sock(int *sock);
void cleanup_socks(void *cdata);
#endif				/* MISC_H */
