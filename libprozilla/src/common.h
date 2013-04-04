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

/* Common #includes and #defines. */

/* $Id: common.h,v 1.7 2001/10/27 23:20:24 kalum Exp $ */


#ifndef COMMON_H
#define COMMON_H


#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if HAVE_STDIO_H
#  include <stdio.h>
#endif

#if STDC_HEADERS
#  if HAVE_STDLIB_H
#    include <stdlib.h>
#  endif
#  include <stdarg.h>
#endif

#if HAVE_STRING_H
#  if !STDC_HEADERS && HAVE_MEMORY_H
#    include <memory.h>
#  endif
#  include <string.h>
#else
#  if HAVE_STRINGS_H
#    include <strings.h>
#  endif
#endif

#if HAVE_UNISTD_H
#  include <unistd.h>
#endif

#if HAVE_CTYPE_H
#  include <ctype.h>
#endif

#if HAVE_ERRNO_H
#  include <errno.h>
#endif

#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#if HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#include <netinet/in_systm.h>

 
#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/mount.h>
#else
 #include <sys/vfs.h>
#endif


#if HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#if HAVE_NETDB_H
#  include <netdb.h>
#endif

#if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#if HAVE_ASSERT_H
#  include <assert.h>
#endif

#if HAVE_PWD_H
#  include <pwd.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_PTHREAD_H
#  include <pthread.h>
#endif

/* If we don't have vsnprintf() try to use __vsnprintf(). */
#if !defined(HAVE_VSNPRINTF) && defined(HAVE___VSNPRINTF)
#  undef vsnprintf
#  define vsnprintf __vsnprintf
#  define HAVE_VSNPRINTF
#endif

/* If we don't have snprintf() try to use __snprintf(). */
#if !defined(HAVE_SNPRINTF) && defined(HAVE___SNPRINTF)
#  undef snprintf
#  define snprintf __snprintf
#  define HAVE_SNPRINTF
#endif


typedef int boolean;


#ifndef FALSE
#  define FALSE (0)
#endif

#ifndef TRUE
#  define TRUE (!FALSE)
#endif

#ifndef NO
#  define NO FALSE
#endif

#ifndef YES
#  define YES TRUE
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

/* Gettext */
#include <libintl.h>
#define _(String) dgettext (PACKAGE, String)
#define gettext_noop(String) (String)
#ifndef HAVE_GNOME
#define N_(String) gettext_noop (String)
#endif
/* Gettext */


#endif				/* COMMON_H */
