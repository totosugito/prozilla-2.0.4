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

//Rewrite of the ftp parsing code as we needed to extract the date of
//the file to save it when the downloading is complete.


#include "common.h"
#include "ftpparse.h"
#include "prozilla.h"


    
 
  /* MultiNet (some spaces removed from examples) */
  /* "00README.TXT;1      2 30-DEC-1996 17:44 [SYSTEM] (RWED,RWED,RE,RE)" */
  /* "CORE.DIR;1          1  8-SEP-1996 16:09 [SYSTEM] (RWE,RWE,RE,RE)" */
  /* and non-MutliNet VMS: */
  /* "CII-MANUAL.TEX;1  213/216  29-JAN-1996 03:33:12  [ANONYMOU,ANONYMOUS]   (RWED,RWED,,)" */

  /* MSDOS format */
  /* 04-27-00  09:09PM       <DIR>          licensed */
  /* 07-18-00  10:16AM       <DIR>          pub */
  /* 04-14-00  03:47PM                  589 readme.htm */



long getlong(char *buf,int len)
{
  long u = 0;
  while (len-- > 0)
    u = u * 10 + (*buf++ - '0');
  return u;
}


/*	Find next Field
**	---------------
**	Finds the next RFC822 token in a string
**	On entry,
**	*pstr	points to a string containing a word separated
**		by white white space "," ";" or "=". The word
**		can optionally be quoted using <"> or "<" ">"
**		Comments surrrounded by '(' ')' are filtered out
**
** 	On exit,
**	*pstr	has been moved to the first delimiter past the
**		field
**		THE STRING HAS BEEN MUTILATED by a 0 terminator
**
**	Returns	a pointer to the first word or NULL on error
*/
char * get_nextfield (char ** pstr)
{
    char * p = *pstr;

    char * start = NULL;
    if (!pstr || !*pstr) return NULL;
    while (1) {
	/* Strip white space and other delimiters */
	while (*p && (isspace((int) *p) || *p==',' || *p==';' || *p=='=')) p++;
	if (!*p) {
	    *pstr = p;
	    return NULL;				   	 /* No field */
	}

	if (*p == '"') {				     /* quoted field */
	    start = ++p;
	    for(;*p && *p!='"'; p++)
		if (*p == '\\' && *(p+1)) p++;	       /* Skip escaped chars */
	    break;			    /* kr95-10-9: needs to stop here */
	} else if (*p == '<') {				     /* quoted field */
	    start = ++p;
	    for(;*p && *p!='>'; p++)
		if (*p == '\\' && *(p+1)) p++;	       /* Skip escaped chars */
	    break;			    /* kr95-10-9: needs to stop here */
	} else if (*p == '(') {					  /* Comment */
	    for(;*p && *p!=')'; p++)
		if (*p == '\\' && *(p+1)) p++;	       /* Skip escaped chars */
	    p++;
	} else {					      /* Spool field */
	    start = p;
	    while(*p && !isspace((int) *p) && *p!=',' && *p!=';' && *p!='=')
		p++;
	    break;						   /* Got it */
	}
    }
    if (*p) *p++ = '\0';
    *pstr = p;
    return start;
}



uerr_t ftp_parse(ftpparse *fp,char *buf,int len)
{

  char *cp;
  char *token;
  char *ptr;
  char *date;
  int i;
  fp->filename = 0;
  fp->namelen = 0;
  //  fp->flagtrycwd = 0;
  // fp->flagtryretr = 0;
  //fp->sizetype = FTPPARSE_SIZE_UNKNOWN;
  fp->filesize = 0;
  //  fp->mtimetype = FTPPARSE_MTIME_UNKNOWN;
  fp->mtime = 0;
  // fp->idtype = FTPPARSE_ID_UNKNOWN;
  fp->id = 0;
  // fp->idlen = 0;

  proz_debug("FTP LIST to be parsed is %s", cp=strdup(buf));
  free(cp);

  if (len < 2) /* an empty name in EPLF, with no info, could be 2 chars */
    return  FTPPARSENOTEXIST;

  switch(*buf) {
    case 'b':
    case 'c':
    case 'd':
    case 'l':
    case 'p':
    case 's':
    case '-':
   /* UNIX-style listing, without inum and without blocks */
    /* "-rw-r--r--   1 root     other        531 Jan 29 03:26 README" */
    /* "dr-xr-xr-x   2 root     other        512 Apr  8  1994 etc" */
    /* "dr-xr-xr-x   2 root     512 Apr  8  1994 etc" */
    /* "lrwxrwxrwx   1 root     other          7 Jan 25 00:17 bin -> usr/bin" */
    /* Also produced by Microsoft's FTP servers for Windows: */
    /* "----------   1 owner    group         1803128 Jul 10 10:18 ls-lR.Z" */
    /* "d---------   1 owner    group               0 May  9 19:45 Softlib" */
    /* Also WFTPD for MSDOS: */
    /* "-rwxrwxrwx   1 noone    nogroup      322 Aug 19  1996 message.ftp" */
    /* Also NetWare: */
    /* "d [R----F--] supervisor            512       Jan 16 18:53    login" */
    /* "- [R----F--] rhesus             214059       Oct 20 15:27    cx.exe" */
    /* Also NetPresenz for the Mac: */
    /* "-------r--         326  1391972  1392298 Nov 22  1995 MegaPhone.sit" */
    /* "drwxrwxr-x               folder        2 May 10  1996 network"
       */

      if (*buf == 'd') fp->filetype = DIRECTORY;
      if (*buf == '-') fp->filetype = DIRECTORY;
      if (*buf == 'l') fp->filetype = SYMBOLIC_LINK;
      ptr=cp=strdup(buf);
   
   for (i=0;i<4;i++)
    {
      //add checking
      token= get_nextfield(&cp);
      if(token == NULL) //failed to parse
	return FTPPARSEFAIL;
    }
    /*
    ** This field can either be group or size. We find out by looking at the
    ** next field. If this is a non-digit then this field is the size.
    */
    while (*cp && isspace((int) *cp)) cp++;
        if (isdigit((int) *cp)) {
	token = get_nextfield(&cp);
	while (*cp && isspace((int) *cp)) cp++;
    }
	//if it is a filename
	fp->filesize=strtol(token,NULL,10);
	proz_debug("FTP file size is %ld", fp->filesize);


	while (*cp && isspace((int) *cp)) cp++;
	assert(cp+12<ptr+len);
	date = cp;
	cp += 12;
	*cp++ = '\0';
	fp->date_str = strdup(date);

	proz_debug("LIST date is %s", fp->date_str);

	return FTPPARSEOK;
  default:
    return FTPPARSEFAIL;

  }

}




time_t parse_time(const char * str)
{

    char * p;
    struct tm tm;
    time_t t;

    if (!str) return 0;

   if ((p = strchr(str, ','))) 
     {

}

    return 0;

}
