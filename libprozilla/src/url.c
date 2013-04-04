/* URL handling.
   Copyright (C) 1995, 1996, 1997 Free Software Foundation, Inc.
   
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

/* $Id: url.c,v 1.23 2001/10/27 11:24:40 kalum Exp $ */


#include "common.h"
#include "prozilla.h"
#include "url.h"
#include "misc.h"


/* NULL-terminated list of strings to be recognized as prototypes (URL
   schemes). Note that recognized doesn't mean supported -- only HTTP
   and FTP are supported for now.

   However, a string that does not match anything in the list will be
   considered a relative URL.  Thus it's important that this list has
   anything anyone could think of being legal.
   
   There are wild things here. :-) Take a look at
   <URL:http://www.w3.org/pub/WWW/Addressing/schemes.html> to see more
   fun.  */


/* Is X "."?  */
#define DOTP(x) ((*(x) == '.') && (!*(x + 1)))
/* Is X ".."?  */
#define DDOTP(x) ((*(x) == '.') && (*(x + 1) == '.') && (!*(x + 2)))


char *protostrings[] = {
  "cid:",
  "clsid:",
  "file:",
  "finger:",
  "ftp:",
  "gopher:",
  "hdl:",
  "http:",
  "https:",
  "ilu:",
  "ior:",
  "irc:",
  "java:",
  "javascript:",
  "lifn:",
  "mailto:",
  "mid:",
  "news:",
  "nntp:",
  "path:",
  "prospero:",
  "rlogin:",
  "service:",
  "shttp:",
  "snews:",
  "stanf:",
  "telnet:",
  "tn3270:",
  "wais:",
  "whois++:",
  NULL
};

/* TODO remove this stupid things... */


/* Similar to former, but for supported protocols: */
proto_t sup_protos[] = {
  {"http://", URLHTTP, DEFAULT_HTTP_PORT},
  {"ftp://", URLFTP, DEFAULT_FTP_PORT}
  /* { "file://", URLFILE, DEFAULT_FTP_PORT } */
};

/* Support for encoding and decoding of URL strings.  We determine
   whether a character is unsafe through   table lookup.  This
   code assumes ASCII character set and 8-bit chars.  */

enum {
  urlchr_reserved = 1,
  urlchr_unsafe = 2
};

#define R  urlchr_reserved
#define U  urlchr_unsafe
#define RU R|U

#define urlchr_test(c, mask) (urlchr_table[(unsigned char)(c)] & (mask))

/* rfc1738 reserved chars.  We don't use this yet; preservation of
   reserved chars will be implemented when I integrate the new
   `reencode_string' function.  */

#define RESERVED_CHAR(c) urlchr_test(c, urlchr_reserved)

/* Unsafe chars:
   - anything <= 32;
   - stuff from rfc1738 ("<>\"#%{}|\\^~[]`");
   - '@' and ':'; needed for encoding URL username and password.
   - anything >= 127. */

#define UNSAFE_CHAR(c) urlchr_test(c, urlchr_unsafe)

/* Convert the ASCII character X to a hex-digit.  X should be between
   '0' and '9', or between 'A' and 'F', or between 'a' and 'f'.  The
   result is a number between 0 and 15.  If X is not a hexadecimal
   digit character, the result is undefined.  */
#define XCHAR_TO_XDIGIT(x)			\
  (((x) >= '0' && (x) <= '9') ?			\
   ((x) - '0') : (toupper(x) - 'A' + 10))

/* The reverse of the above: convert a HEX digit in the [0, 15] range
   to an ASCII character representing it.  The A-F characters are
   always in upper case.  */
#define XDIGIT_TO_XCHAR(x) (((x) < 10) ? ((x) + '0') : ((x) - 10 + 'A'))

#define ARRAY_SIZE(array) (sizeof (array) / sizeof (*(array)))

const static unsigned char urlchr_table[256] = {
  U, U, U, U, U, U, U, U,	/* NUL SOH STX ETX  EOT ENQ ACK BEL */
  U, U, U, U, U, U, U, U,	/* BS  HT  LF  VT   FF  CR  SO  SI  */
  U, U, U, U, U, U, U, U,	/* DLE DC1 DC2 DC3  DC4 NAK SYN ETB */
  U, U, U, U, U, U, U, U,	/* CAN EM  SUB ESC  FS  GS  RS  US  */
  U, 0, U, U, 0, U, R, 0,	/* SP  !   "   #    $   %   &   '   */
  0, 0, 0, R, 0, 0, 0, R,	/* (   )   *   +    ,   -   .   /   */
  0, 0, 0, 0, 0, 0, 0, 0,	/* 0   1   2   3    4   5   6   7   */
  0, 0, U, R, U, R, U, R,	/* 8   9   :   ;    <   =   >   ?   */
  RU, 0, 0, 0, 0, 0, 0, 0,	/* @   A   B   C    D   E   F   G   */
  0, 0, 0, 0, 0, 0, 0, 0,	/* H   I   J   K    L   M   N   O   */
  0, 0, 0, 0, 0, 0, 0, 0,	/* P   Q   R   S    T   U   V   W   */
  0, 0, 0, U, U, U, U, 0,	/* X   Y   Z   [    \   ]   ^   _   */
  U, 0, 0, 0, 0, 0, 0, 0,	/* `   a   b   c    d   e   f   g   */
  0, 0, 0, 0, 0, 0, 0, 0,	/* h   i   j   k    l   m   n   o   */
  0, 0, 0, 0, 0, 0, 0, 0,	/* p   q   r   s    t   u   v   w   */
  0, 0, 0, U, U, U, U, U,	/* x   y   z   {    |   }   ~   DEL */

  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,

  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
  U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
};

/* Returns 1 if the URL begins with a protocol (supported or
   unsupported), 0 otherwise.  */
int has_proto(const char *url)
{
  char **s;

  for (s = protostrings; *s; s++)
    if (strncasecmp(url, *s, strlen(*s)) == 0)
      return 1;
  return 0;
}

/* Skip the username and password, if present here.  The function
   should be called *not* with the complete URL, but with the part
   right after the protocol.

   If no username and password are found, return 0.  */
int skip_uname(const char *url)
{
  const char *p;
  const char *q = NULL;
  for (p = url; *p && *p != '/'; p++)
    if (*p == '@')
      q = p;
  /* If a `@' was found before the first occurrence of `/', skip
     it.  */
  if (q != NULL)
    return q - url + 1;
  else
    return 0;
}

/* Decodes the forms %xy in a URL to the character the hexadecimal
   code of which is xy.  xy are hexadecimal digits from
   [0123456789ABCDEF] (case-insensitive).  If x or y are not
   hex-digits or `%' precedes `\0', the sequence is inserted
   literally.  */

void decode_string(char *s)
{
  char *t = s;			/* t - tortoise */
  char *h = s;			/* h - hare     */

  for (; *h; h++, t++)
  {
  if (*h != '%')
//    if(1)
    {
    copychar:
      *t = *h;
    } else
    {
      /* Do nothing if '%' is not followed by two hex digits. */
      if (!*(h + 1) || !*(h + 2)
	  || !(isxdigit(*(h + 1)) && isxdigit(*(h + 2))))
	goto copychar;
      *t = (XCHAR_TO_XDIGIT(*(h + 1)) << 4) + XCHAR_TO_XDIGIT(*(h + 2));
      h += 2;
    }
  }
  *t = '\0';
}

/* Like encode_string, but return S if there are no unsafe chars.  */

char *encode_string_maybe(const char *s)
{
  const char *p1;
  char *p2, *newstr;
  int newlen;
  int addition = 0;
  /*Changes Grendel: (*p1!='%') added */

  
  for (p1 = s; *p1; p1++)
    if ((*p1!='%') && UNSAFE_CHAR(*p1))
      addition += 2;	/* Two more characters (hex digits) */

  if (!addition)
    return (char *) s;

  newlen = (p1 - s) + addition;
  newstr = (char *) kmalloc(newlen + 1);

  p1 = s;
  p2 = newstr;
  while (*p1)
  {
    //    if (UNSAFE_CHAR(*p1))
if ((*p1!='%') && UNSAFE_CHAR(*p1))
/*	  if(0)*/
    {
      const unsigned char c = *p1++;
      *p2++ = '%';
      *p2++ = XDIGIT_TO_XCHAR(c >> 4);
      *p2++ = XDIGIT_TO_XCHAR(c & 0xf);
    } else
      *p2++ = *p1++;
  }
  *p2 = '\0';
  assert(p2 - newstr == newlen);

  return newstr;
}

/* Encode the unsafe characters (as determined by UNSAFE_CHAR) in a
   given string, returning a malloc-ed %XX encoded string.  */

char *encode_string(const char *s)
{
  char *encoded = encode_string_maybe(s);

  if (encoded != s)
    return encoded;
  else
    return kstrdup(s);
}

/* Encode unsafe characters in PTR to %xx.  If such encoding is done,
   the old value of PTR is freed and PTR is made to point to the newly
   allocated storage.  */

#define ENCODE(ptr) do {			\
  char *e_new = encode_string_maybe (ptr);	\
  if (e_new != ptr)				\
    {						\
      kfree (ptr);				\
      ptr = e_new;				\
    }						\
} while (0)

/* Returns the protocol type if URL's protocol is supported, or
   URLUNKNOWN if not.  */
uerr_t urlproto(const char *url)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(sup_protos); i++)
    if (!strncasecmp(url, sup_protos[i].name, strlen(sup_protos[i].name)))
      return sup_protos[i].ind;
  for (i = 0; url[i] && url[i] != ':' && url[i] != '/'; i++);
  if (url[i] == ':')
  {
    for (++i; url[i] && url[i] != '/'; i++)
      if (!isdigit(url[i]))
	return URLBADPORT;
    if (url[i - 1] == ':')
      return URLFTP;
    else
      return URLHTTP;
  } else
    return URLHTTP;
}


/* If PATH ends with `;type=X', return the character X.  */
char process_ftp_type(char *path)
{
  int len = strlen(path);

  if (len >= 7 && !memcmp(path + len - 7, ";type=", 6))
  {
    path[len - 7] = '\0';
    return path[len - 1];
  } else
    return '\0';
}

/* Canonicalize PATH, and return a new path.  The new path differs from PATH
   in that:
	Multple `/'s are collapsed to a single `/'.
	Leading `./'s and trailing `/.'s are removed.
	Trailing `/'s are removed.
	Non-leading `../'s and trailing `..'s are handled by removing
	portions of the path.

   E.g. "a/b/c/./../d/.." will yield "a/b".  This function originates
   from GNU Bash.

   Changes for Wget:
	Always use '/' as stub_char.
	Don't check for local things using canon_stat.
	Change the original string instead of strdup-ing.
	React correctly when beginning with `./' and `../'.  */
void path_simplify(char *path)
{
  register int i, start, ddot;
  char stub_char;

  if (!*path)
    return;

  /*stub_char = (*path == '/') ? '/' : '.'; */
  stub_char = '/';

  /* Addition: Remove all `./'-s preceding the string.  If `../'-s
     precede, put `/' in front and remove them too.  */
  i = 0;
  ddot = 0;
  while (1)
  {
    if (path[i] == '.' && path[i + 1] == '/')
      i += 2;
    else if (path[i] == '.' && path[i + 1] == '.' && path[i + 2] == '/')
    {
      i += 3;
      ddot = 1;
    } else
      break;
  }
  if (i)
    strcpy(path, path + i - ddot);

  /* Replace single `.' or `..' with `/'.  */
  if ((path[0] == '.' && path[1] == '\0')
      || (path[0] == '.' && path[1] == '.' && path[2] == '\0'))
  {
    path[0] = stub_char;
    path[1] = '\0';
    return;
  }
  /* Walk along PATH looking for things to compact.  */
  i = 0;
  while (1)
  {
    if (!path[i])
      break;

    while (path[i] && path[i] != '/')
      i++;

    start = i++;

    /* If we didn't find any slashes, then there is nothing left to do.  */
    if (!path[start])
      break;

    /* Handle multiple `/'s in a row.  */
    while (path[i] == '/')
      i++;

    if ((start + 1) != i)
    {
      strcpy(path + start + 1, path + i);
      i = start + 1;
    }

    /* Check for trailing `/'.  */
    if (start && !path[i])
    {
    zero_last:
      path[--i] = '\0';
      break;
    }

    /* Check for `../', `./' or trailing `.' by itself.  */
    if (path[i] == '.')
    {
      /* Handle trailing `.' by itself.  */
      if (!path[i + 1])
	goto zero_last;

      /* Handle `./'.  */
      if (path[i + 1] == '/')
      {
	strcpy(path + i, path + i + 1);
	i = (start < 0) ? 0 : start;
	continue;
      }

      /* Handle `../' or trailing `..' by itself.  */
      if (path[i + 1] == '.' && (path[i + 2] == '/' || !path[i + 2]))
      {
	while (--start > -1 && path[start] != '/');
	strcpy(path + start + 1, path + i + 2);
	i = (start < 0) ? 0 : start;
	continue;
      }
    }				/* path == '.' */
  }				/* while */

  if (!*path)
  {
    *path = stub_char;
    path[1] = '\0';
  }
}

/* Special versions of DOTP and DDOTP for parse_dir().  They work like
   DOTP and DDOTP, but they also recognize `?' as end-of-string
   delimiter.  This is needed for correct handling of query
   strings.  */

#define PD_DOTP(x)  ((*(x) == '.') && (!*((x) + 1) || *((x) + 1) == '?'))
#define PD_DDOTP(x) ((*(x) == '.') && (*(x) == '.')		\
		     && (!*((x) + 2) || *((x) + 2) == '?'))

/* Like strlen(), but allow the URL to be ended with '?'.  */
int urlpath_length(const char *url)
{
  const char *q = strchr(url, '?');
  if (q)
    return q - url;
  return strlen(url);
}

/* Build the directory and filename components of the path.  Both
   components are *separately* malloc-ed strings!  It does not change
   the contents of path.

   If the path ends with "." or "..", they are (correctly) counted as
   directories.  */
void parse_dir(const char *path, char **dir, char **file)
{
  int i, l;

  l = urlpath_length(path);
  for (i = l; i && path[i] != '/'; i--);

  if (!i && *path != '/')	/* Just filename */
  {
    if (PD_DOTP(path) || PD_DDOTP(path))
    {
      *dir = strdupdelim(path, path + l);
      *file = kstrdup(path + l);	/* normally empty, but could
					   contain ?... */
    } else
    {
      *dir = kstrdup("");	/* This is required because of FTP */
      *file = kstrdup(path);
    }
  } else if (!i)		/* /filename */
  {
    if (PD_DOTP(path + 1) || PD_DDOTP(path + 1))
    {
      *dir = strdupdelim(path, path + l);
      *file = kstrdup(path + l);	/* normally empty, but could
					   contain ?... */
    } else
    {
      *dir = kstrdup("/");
      *file = kstrdup(path + 1);
    }
  } else			/* Nonempty directory with or without a filename */
  {
    if (PD_DOTP(path + i + 1) || PD_DDOTP(path + i + 1))
    {
      *dir = strdupdelim(path, path + l);
      *file = kstrdup(path + l);	/* normally empty, but could
					   contain ?... */
    } else
    {
      *dir = strdupdelim(path, path + i);
      *file = kstrdup(path + i + 1);
    }
  }
}

/* Skip the protocol part of the URL, e.g. `http://'.  If no protocol
   part is found, returns 0.  */
int skip_proto(const char *url)
{
  char **s;
  int l;

  for (s = protostrings; *s; s++)
    if (!strncasecmp(*s, url, strlen(*s)))
      break;
  if (!*s)
    return 0;
  l = strlen(*s);
  /* HTTP and FTP protocols are expected to yield exact host names
     (i.e. the `//' part must be skipped, too).  */
  if (!strcmp(*s, "http:") || !strcmp(*s, "ftp:"))
    l += 2;
  return l;
}

/* Find the optional username and password within the URL, as per
   RFC1738.  The returned user and passwd char pointers are
   malloc-ed.  */
static uerr_t parse_uname(const char *url, char **user, char **passwd)
{
  int l;
  const char *p, *q, *col;
  char **where;

  *user = NULL;
  *passwd = NULL;

  /* Look for the end of the protocol string.  */
  l = skip_proto(url);
  if (!l)
    return URLUNKNOWN;
  /* Add protocol offset.  */
  url += l;
  /* Is there an `@' character?  */
  for (p = url; *p && *p != '/'; p++)
    if (*p == '@')
      break;
  /* If not, return.  */
  if (*p != '@')
    return URLOK;
  /* Else find the username and password.  */
  for (p = q = col = url; *p && *p != '/'; p++)
  {
    if (*p == ':' && !*user)
    {
      *user = (char *) kmalloc(p - url + 1);
      memcpy(*user, url, p - url);
      (*user)[p - url] = '\0';
      col = p + 1;
    }
    if (*p == '@')
      q = p;
  }
  /* Decide whether you have only the username or both.  */
  where = *user ? passwd : user;
  *where = (char *) kmalloc(q - col + 1);
  memcpy(*where, col, q - col);
  (*where)[q - col] = '\0';
  return URLOK;
}

/* Return the URL as fine-formed string, with a proper protocol, optional port
   number, directory and optional user/password.  If `hide' is non-zero (as it
   is when we're calling this on a URL we plan to print, but not when calling it
   to canonicalize a URL for use within the program), password will be hidden.
   The forbidden characters in the URL will be cleansed.  */
char *str_url(const urlinfo * u, int hide)
{
  char *res, *host, *user, *passwd, *proto_name, *dir, *file;
  int i, l, ln, lu, lh, lp, lf, ld;
  unsigned short proto_default_port;

  /* Look for the protocol name.  */
  for (i = 0; i < ARRAY_SIZE(sup_protos); i++)
    if (sup_protos[i].ind == u->proto)
      break;
  if (i == ARRAY_SIZE(sup_protos))
    return NULL;
  proto_name = sup_protos[i].name;
  proto_default_port = sup_protos[i].port;
  host = encode_string(u->host);
  dir = encode_string(u->dir);
  file = encode_string(u->file);
  user = passwd = NULL;
  if (u->user)
    user = encode_string(u->user);
  if (u->passwd)
  {
    if (hide)
      /* Don't output the password, or someone might see it over the user's
         shoulder (or in saved wget output).  Don't give away the number of
         characters in the password, either, as we did in past versions of
         this code, when we replaced the password characters with 'x's. */
      passwd = kstrdup("<password>");
    else
      passwd = encode_string(u->passwd);
  }
  if (u->proto == URLFTP && *dir == '/')
  {
    char *tmp = (char *) kmalloc(strlen(dir) + 3);
    /*sprintf (tmp, "%%2F%s", dir + 1); */
    tmp[0] = '%';
    tmp[1] = '2';
    tmp[2] = 'F';
    strcpy(tmp + 3, dir + 1);
    kfree(dir);
    dir = tmp;
  }

  ln = strlen(proto_name);
  lu = user ? strlen(user) : 0;
  lp = passwd ? strlen(passwd) : 0;
  lh = strlen(host);
  ld = strlen(dir);
  lf = strlen(file);
  res = (char *) kmalloc(ln + lu + lp + lh + ld + lf + 20);	/* safe sex */
  /* sprintf (res, "%s%s%s%s%s%s:%d/%s%s%s", proto_name,
     (user ? user : ""), (passwd ? ":" : ""),
     (passwd ? passwd : ""), (user ? "@" : ""),
     host, u->port, dir, *dir ? "/" : "", file); */
  l = 0;
  memcpy(res, proto_name, ln);
  l += ln;
  if (user)
  {
    memcpy(res + l, user, lu);
    l += lu;
    if (passwd)
    {
      res[l++] = ':';
      memcpy(res + l, passwd, lp);
      l += lp;
    }
    res[l++] = '@';
  }
  memcpy(res + l, host, lh);
  l += lh;
  if (u->port != proto_default_port)
  {
    res[l++] = ':';

    sprintf(res + l, "%ld", (long) u->port);

    l += numdigit(u->port);
  }
  res[l++] = '/';
  memcpy(res + l, dir, ld);
  l += ld;
  if (*dir)
    res[l++] = '/';
  strcpy(res + l, file);
  kfree(host);
  kfree(dir);
  kfree(file);
  kfree(user);
  kfree(passwd);
  return res;
}


/* Extract the given URL of the form
   (http:|ftp:)// (user (:password)?@)?hostname (:port)? (/path)?
   1. hostname (terminated with `/' or `:')
   2. port number (terminated with `/'), or chosen for the protocol
   3. dirname (everything after hostname)
   Most errors are handled.  No allocation is done, you must supply
   pointers to allocated memory.
   ...and a host of other stuff :-)

   - Recognizes hostname:dir/file for FTP and
     hostname (:portnum)?/dir/file for HTTP.
   - Parses the path to yield directory and file
   - Parses the URL to yield the username and passwd (if present)
   - Decodes the strings, in case they contain "forbidden" characters
   - Writes the result to struct urlinfo

   If the argument STRICT is set, it recognizes only the canonical
   form.  */
uerr_t proz_parse_url(const char *url, urlinfo * u, int strict)
{
  int i, l, abs_ftp;
  int recognizable;		/* Recognizable URL is the one where
				   the protocol name was explicitly
				   named, i.e. it wasn't deduced from
				   the URL format.  */
  uerr_t type;

  memset(u, 0, sizeof(urlinfo));
  recognizable = has_proto(url);
  if (strict && !recognizable)
    return URLUNKNOWN;
  for (i = 0, l = 0; i < ARRAY_SIZE(sup_protos); i++)
  {
    l = strlen(sup_protos[i].name);
    if (!strncasecmp(sup_protos[i].name, url, l))
      break;
  }
  /* If protocol is recognizable, but unsupported, bail out, else
     suppose unknown.  */
  if (recognizable && i == ARRAY_SIZE(sup_protos))
    return URLUNKNOWN;
  else if (i == ARRAY_SIZE(sup_protos))
    type = URLUNKNOWN;
  else
    u->proto = type = sup_protos[i].ind;

  if (type == URLUNKNOWN)
    l = 0;
  /* Allow a username and password to be specified (i.e. just skip
     them for now).  */
  if (recognizable)
    l += skip_uname(url + l);
  for (i = l; url[i] && url[i] != ':' && url[i] != '/'; i++);
  if (i == l)
    return URLBADHOST;
  /* Get the hostname.  */
  u->host = strdupdelim(url + l, url + i);

  /* Assume no port has been given.  */
  u->port = 0;
  if (url[i] == ':')
  {
    /* We have a colon delimiting the hostname.  It could mean that
       a port number is following it, or a directory.  */
    if (isdigit(url[++i]))	/* A port number */
    {
      if (type == URLUNKNOWN)
	u->proto = type = URLHTTP;
      for (; url[i] && url[i] != '/'; i++)
	if (isdigit(url[i]))
	  u->port = 10 * u->port + (url[i] - '0');
	else
	  return URLBADPORT;
      if (!u->port)
	return URLBADPORT;
    } else if (type == URLUNKNOWN)	/* or a directory */
      u->proto = type = URLFTP;
    else			/* or just a misformed port number */
      return URLBADPORT;
  } else if (type == URLUNKNOWN)
    u->proto = type = URLHTTP;
  if (!u->port)
  {
    int ind;
    for (ind = 0; ind < ARRAY_SIZE(sup_protos); ind++)
      if (sup_protos[ind].ind == type)
	break;
    if (ind == ARRAY_SIZE(sup_protos))
      return URLUNKNOWN;
    u->port = sup_protos[ind].port;
  }
  /* Some delimiter troubles...  */
  if (url[i] == '/' && url[i - 1] != ':')
    ++i;
  if (type == URLHTTP)
    while (url[i] && url[i] == '/')
      ++i;
  u->path = (char *) kmalloc(strlen(url + i) + 8);
  strcpy(u->path, url + i);
  if (type == URLFTP)
  {
    u->ftp_type = process_ftp_type(u->path);
    /* #### We don't handle type `d' correctly yet.  */
    if (!u->ftp_type || toupper(u->ftp_type) == 'D')
      u->ftp_type = 'I';
  }


  /* Parse the username and password (if existing).  */
  parse_uname(url, &u->user, &u->passwd);
  /* Decode the strings, as per RFC 1738.  */
  decode_string(u->host);
  // decode_string(u->path);
  if (u->user)
    decode_string(u->user);
  if (u->passwd)
    decode_string(u->passwd);
  /* Parse the directory.  */
  parse_dir(u->path, &u->dir, &u->file);
  /* Simplify the directory.  */
  path_simplify(u->dir);
  /* Remove the leading `/' in HTTP.  */
  if (type == URLHTTP && *u->dir == '/')
    strcpy(u->dir, u->dir + 1);
  /* Strip trailing `/'.  */
  l = strlen(u->dir);
  if (l > 1 && u->dir[l - 1] == '/')
    u->dir[l - 1] = '\0';
  /* Re-create the path: */
  abs_ftp = (u->proto == URLFTP && *u->dir == '/');
  /*  sprintf (u->path, "%s%s%s%s", abs_ftp ? "%2F": "/",
     abs_ftp ? (u->dir + 1) : u->dir, *u->dir ? "/" : "", u->file); */
  strcpy(u->path, abs_ftp ? "%2F" : "/");
  strcat(u->path, abs_ftp ? (u->dir + 1) : u->dir);
  strcat(u->path, *u->dir ? "/" : "");
  strcat(u->path, u->file);
  ENCODE(u->path);
  /* Create the clean URL.  */
  u->url = str_url(u, 0);
  return URLOK;
}











/******************************************************************************
 This function constructs and returns a malloced copy of the relative link
 from two pieces of information: local name of the referring file (s1) and
 local name of the referred file (s2).

 So, if s1 is "jagor.srce.hr/index.html" and s2 is
 "jagor.srce.hr/images/news.gif", new name should be "images/news.gif".

 Alternately, if the s1 is "fly.cc.fer.hr/ioccc/index.html", and s2 is
 "fly.cc.fer.hr/images/fly.gif", new name should be "../images/fly.gif".

 Caveats: s1 should not begin with '/', unless s2 begins with '/' too.
 s1 should not contain things like ".." and such --
 construct_relative("fly/ioccc/../index.html", "fly/images/fly.gif")
 will fail. (workaround is to call path_simplify on s1).
******************************************************************************/
char *construct_relative(const char *s1, const char *s2)
{
  int i, cnt, sepdirs1;
  char *res;

  if (*s2 == '/')
    return kstrdup(s2);

  /* s1 should *not* be absolute, if s2 wasn't. */
  assert(*s1 != '/');
  i = cnt = 0;

  /* Skip the directories common to both strings. */
  while (1)
  {
    for (;
	 s1[i] && s2[i] && s1[i] == s2[i] && s1[i] != '/'
	 && s2[i] != '/'; i++);
    if (s1[i] == '/' && s2[i] == '/')
      cnt = ++i;
    else
      break;
  }
  for (sepdirs1 = 0; s1[i]; i++)
    if (s1[i] == '/')
      ++sepdirs1;

  /* Now, construct the file as of:
     - ../ repeated sepdirs1 time
     - all the non-mutual directories of s2. */

  res = kmalloc(3 * sepdirs1 + strlen(s2 + cnt) + 1);
  for (i = 0; i < sepdirs1; i++)
    memcpy(res + 3 * i, "../", 3);
  strcpy(res + 3 * i, s2 + cnt);
  return res;
}

/******************************************************************************
 Add a URL to the list.
******************************************************************************/
urlpos *add_url(urlpos * l, const char *url, const char *file)
{
  urlpos *t, *b;

  t = kmalloc(sizeof(urlpos));
  memset(t, 0, sizeof(*t));
  t->url = kstrdup(url);
  t->local_name = kstrdup(file);
  if (!l)
    return t;
  b = l;
  while (l->next)
    l = l->next;
  l->next = t;
  return b;
}


/*This will copy a url structure to another */
void url_cpy(urlinfo * src, urlinfo * dest)
{


}


/* Find the last occurrence of character C in the range [b, e), or
   NULL, if none are present.  This is almost completely equivalent to
   { *e = '\0'; return strrchr(b); }, except that it doesn't change
   the contents of the string.  */
const char *find_last_char(const char *b, const char *e, char c)
{
  for (; e > b; e--)
    if (*e == c)
      return e;
  return NULL;
}

/* Resolve the result of "linking" a base URI (BASE) to a
   link-specified URI (LINK).

   Either of the URIs may be absolute or relative, complete with the
   host name, or path only.  This tries to behave "reasonably" in all
   foreseeable cases.  It employs little specific knowledge about
   protocols or URL-specific stuff -- it just works on strings.

   The parameters LINKLENGTH is useful if LINK is not zero-terminated.
   See uri_merge for a gentler interface to this functionality.

   #### This function should handle `./' and `../' so that the evil
   path_simplify can go.  */
char *uri_merge_1(const char *base, const char *link, int linklength,
		  int no_proto)
{
  char *constr;

  if (no_proto)
  {
    const char *end = base + urlpath_length(base);

    if (*link != '/')
    {
      /* LINK is a relative URL: we need to replace everything
         after last slash (possibly empty) with LINK.

         So, if BASE is "whatever/foo/bar", and LINK is "qux/xyzzy",
         our result should be "whatever/foo/qux/xyzzy".  */
      int need_explicit_slash = 0;
      int span;
      const char *start_insert;
      const char *last_slash = find_last_char(base, end, '/');
      if (!last_slash)
      {
	/* No slash found at all.  Append LINK to what we have,
	   but we'll need a slash as a separator.

	   Example: if base == "foo" and link == "qux/xyzzy", then
	   we cannot just append link to base, because we'd get
	   "fooqux/xyzzy", whereas what we want is
	   "foo/qux/xyzzy".

	   To make sure the / gets inserted, we set
	   need_explicit_slash to 1.  We also set start_insert
	   to end + 1, so that the length calculations work out
	   correctly for one more (slash) character.  Accessing
	   that character is fine, since it will be the
	   delimiter, '\0' or '?'.  */
	/* example: "foo?..." */
	/*               ^    ('?' gets changed to '/') */
	start_insert = end + 1;
	need_explicit_slash = 1;
      } else if (last_slash && last_slash != base
		 && *(last_slash - 1) == '/')
      {
	/* example: http://host"  */
	/*                      ^ */
	start_insert = end + 1;
	need_explicit_slash = 1;
      } else
      {
	/* example: "whatever/foo/bar" */
	/*                        ^    */
	start_insert = last_slash + 1;
      }

      span = start_insert - base;
      constr = (char *) kmalloc(span + linklength + 1);
      if (span)
	memcpy(constr, base, span);
      if (need_explicit_slash)
	constr[span - 1] = '/';
      if (linklength)
	memcpy(constr + span, link, linklength);
      constr[span + linklength] = '\0';
    } else			/* *link == `/' */
    {
      /* LINK is an absolute path: we need to replace everything
         after (and including) the FIRST slash with LINK.

         So, if BASE is "http://host/whatever/foo/bar", and LINK is
         "/qux/xyzzy", our result should be
         "http://host/qux/xyzzy".  */
      int span;
      const char *slash;
      const char *start_insert = NULL;	/* for gcc to shut up. */
      const char *pos = base;
      int seen_slash_slash = 0;
      /* We're looking for the first slash, but want to ignore
         double slash. */
    again:
      slash = memchr(pos, '/', end - pos);
      if (slash && !seen_slash_slash)
	if (*(slash + 1) == '/')
	{
	  pos = slash + 2;
	  seen_slash_slash = 1;
	  goto again;
	}

      /* At this point, SLASH is the location of the first / after
         "//", or the first slash altogether.  START_INSERT is the
         pointer to the location where LINK will be inserted.  When
         examining the last two examples, keep in mind that LINK
         begins with '/'. */

      if (!slash && !seen_slash_slash)
	/* example: "foo" */
	/*           ^    */
	start_insert = base;
      else if (!slash && seen_slash_slash)
	/* example: "http://foo" */
	/*                     ^ */
	start_insert = end;
      else if (slash && !seen_slash_slash)
	/* example: "foo/bar" */
	/*           ^        */
	start_insert = base;
      else if (slash && seen_slash_slash)
	/* example: "http://something/" */
	/*                           ^  */
	start_insert = slash;

      span = start_insert - base;
      constr = (char *) kmalloc(span + linklength + 1);
      if (span)
	memcpy(constr, base, span);
      if (linklength)
	memcpy(constr + span, link, linklength);
      constr[span + linklength] = '\0';
    }
  } else			/* !no_proto */
  {
    constr = strdupdelim(link, link + linklength);
  }
  return constr;
}

/* Merge BASE with LINK and return the resulting URI.  This is an
   interface to uri_merge_1 that assumes that LINK is a
   zero-terminated string.  */
char *uri_merge(const char *base, const char *link)
{
  return uri_merge_1(base, link, strlen(link), !has_proto(link));
}

/******************************************************************************
 Perform a "deep" free of the urlinfo structure. The structure should have
 been created with newurl, but need not have been used. If free_pointer
 is non-0, free the pointer itself.
******************************************************************************/
void proz_free_url(urlinfo * u, boolean complete)
{
  assert(u != NULL);

  if (u->url)
    kfree(u->url);
  if (u->host)
    kfree(u->host);
  if (u->path)
    kfree(u->path);
  if (u->file)
    kfree(u->file);
  if (u->dir)
    kfree(u->dir);
  if (u->user)
    kfree(u->user);
  if (u->passwd)
    kfree(u->passwd);
  if (u->referer)
    kfree(u->referer);
  if (complete)
    kfree(u);
}



urlinfo *proz_copy_url(urlinfo * u)
{
  urlinfo *dest_url;

  dest_url = (urlinfo *) kmalloc(sizeof(urlinfo));
  memset(dest_url, 0, sizeof(urlinfo));

  if (u->url)
    dest_url->url = kstrdup(u->url);
  dest_url->proto = u->proto;
  dest_url->port = u->port;
  if (u->host)
    dest_url->host = kstrdup(u->host);
  if (u->path)
    dest_url->path = kstrdup(u->path);
  if (u->dir)
    dest_url->dir = kstrdup(u->dir);
  if (u->file)
    dest_url->file = kstrdup(u->file);
  if (u->user)
    dest_url->user = kstrdup(u->user);
  if (u->passwd)
    dest_url->passwd = kstrdup(u->passwd);
  if (u->referer)
    dest_url->referer = kstrdup(u->referer);

  return dest_url;
}
