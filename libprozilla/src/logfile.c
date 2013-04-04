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

#include "common.h"
#include "prozilla.h"
#include "misc.h"
#include "logfile.h"

/*creates the log file and stores the info */
/*If download is not NULL will store info about the downloads connection allocations to it too.*/

int log_create_logfile(int num_connections, int file_size, char *url,
		       download_t * download)
{
  char buffer[PATH_MAX];
  FILE *fp = NULL;
  int i;
  logfile lf;

  memset(&lf, 0, sizeof(lf));
  /*
   * Compute the name of the logfile 
   */
  snprintf(buffer, PATH_MAX, "%s/%s%s.log", download->log_dir,
	   download->u.file, DEFAULT_FILE_EXT);
  if (!(fp = fopen(buffer, "wb")))
    {

      /*
       * fixme add the error displaing to the main function 
       */
      download_show_message(download,
			    _("Error opening file %s for writing: %s"),
			    buffer, strerror(errno));
      return -1;
    }

  lf.num_connections = num_connections;
  lf.version = 1000;
  lf.got_info = download == NULL ? 1 : 0;
  lf.file_size = file_size;
  lf.url_len = strlen(url);

  /*Write the logfile header */

  /* No of connections */
  if (fwrite(&lf, 1, sizeof(lf), fp) != sizeof(lf))
    {
      download_show_message(download, _("Error writing to file %s: %s"),
			    buffer, strerror(errno));
      fclose(fp);
      return -1;
    }

  /* Now we write the url to it */

  if (fwrite(url, 1, strlen(url), fp) != strlen(url))
    {
      download_show_message(download, _("Error writing to file %s: %s"),
			    buffer, strerror(errno));
      fclose(fp);
      return -1;
    }

  /*Now we write each of the connections start and end positions to the file if download is not null  */

  if (download != NULL)
    {
      for (i = 0; i < download->num_connections; i++)
	{

	  pthread_mutex_lock(&download->pconnections[i]->access_mutex);

	  if (fwrite
	      (&download->pconnections[i]->local_startpos, 1,
	       sizeof(download->pconnections[i]->local_startpos),
	       fp) != sizeof(download->pconnections[i]->local_startpos))
	    {

	      pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
	      download_show_message(download, _("Error writing to file %s: %s"),
				    buffer, strerror(errno));
	      fclose(fp);
	      return -1;
	    }

	  if (fwrite
	      (&download->pconnections[i]->orig_remote_startpos, 1,
	       sizeof(download->pconnections[i]->orig_remote_startpos),
	       fp) != sizeof(download->pconnections[i]->orig_remote_startpos))
            {

	      pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
	      download_show_message(download, _("Error writing to file %s: %s"),
				    buffer, strerror(errno));
	      fclose(fp);
	      return -1;
            }

	  if (fwrite
	      (&download->pconnections[i]->remote_endpos, 1,
	       sizeof(download->pconnections[i]->remote_endpos),
	       fp) != sizeof(download->pconnections[i]->remote_endpos))
	    {
	      pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
	      download_show_message(download, _("Error writing to file %s: %s"),
				    buffer, strerror(errno));
	      fclose(fp);
	      return -1;
	    }

	  if (fwrite
	      (&download->pconnections[i]->remote_bytes_received, 1,
	       sizeof(download->pconnections[i]->remote_bytes_received),
	       fp) != sizeof(download->pconnections[i]->remote_bytes_received))
	    {

	  pthread_mutex_unlock(&download->pconnections[i]->access_mutex);
	      download_show_message(download, _("Error writing to file %s: %s"),
				    buffer, strerror(errno));
	      fclose(fp);
	      return -1;
	    }
	  pthread_mutex_unlock(&download->pconnections[i]->access_mutex);

	}

    }

  fclose(fp);
  return 1;
}

/* returns 1 if the logfile exists, 0 if it doesn't and -1 on error*/
int proz_log_logfile_exists(download_t * download)
{
  char buffer[PATH_MAX];
  int ret;
  struct stat st_buf;

  /*
   * Compute the name of the logfile 
   */
  snprintf(buffer, PATH_MAX, "%s/%s%s.log", download->log_dir,
	   download->u.file, DEFAULT_FILE_EXT);

  ret = stat(buffer, &st_buf);
  if (ret == -1)
  {
    if (errno == ENOENT)
      return 0;
    else
      return -1;
  } else
    return 1;
}

/* delete the log file */
int proz_log_delete_logfile(download_t * download)
{
  char buffer[PATH_MAX];
  int ret;

  snprintf(buffer, PATH_MAX, "%s/%s%s.log", download->log_dir,
	   download->u.file, DEFAULT_FILE_EXT);

  ret = unlink(buffer);
  if (ret == -1)
    {
      if (errno == ENOENT)
	{
	  download_show_message(download, _("logfile doesn't exist"));
	  return 1;
	} else
	  {
	    download_show_message(download, "Error: Unable to delete the logfile: %s", strerror(errno));
	    return -1;
	  }
    }

  return 1;
}

/* Read the logfile into the logfile structure */

int proz_log_read_logfile(logfile * lf, download_t * download,
			  boolean load_con_info)
{
  char buffer[PATH_MAX];
  FILE *fp = NULL;
  int i;

  /*
   * Compute the name of the logfile 
   */
  snprintf(buffer, PATH_MAX, "%s/%s%s.log", download->log_dir,
	   download->u.file, DEFAULT_FILE_EXT);

  if (!(fp = fopen(buffer, "rb")))
    {
      /*
       * fixme add the error displaing to the main function 
       */
      download_show_message(download,
			    _("Error opening file %s for reading: %s"),
			    buffer, strerror(errno));
      return -1;
    }


  if (fread(lf, 1, sizeof(logfile), fp) != sizeof(logfile))
    {
      fclose(fp);
      return -1;
    }

  lf->url = kmalloc(lf->url_len + 1);

  if (fread(lf->url, 1, lf->url_len, fp) != lf->url_len)
    {
      fclose(fp);
      return -1;
    }

  lf->url[lf->url_len] = 0;


  if (load_con_info == TRUE)
    {
      for (i = 0; i < lf->num_connections; i++)
	{



	  proz_debug("value before= %d", download->pconnections[i]->local_startpos);

	  if (fread
	      (&download->pconnections[i]->local_startpos, 1,
	       sizeof(download->pconnections[i]->local_startpos),
	       fp) != sizeof(download->pconnections[i]->local_startpos))
            {
	      download_show_message(download,
				    _("Error reading from file %s: %s"), buffer,
				    strerror(errno));
	      fclose(fp);
	      return -1;
            }

	  proz_debug("value after= %d", download->pconnections[i]->local_startpos);

	  proz_debug("orig_remote_startpos before= %d", download->pconnections[i]->orig_remote_startpos);
	  	  if (fread
	  	      (&download->pconnections[i]->orig_remote_startpos, 1,
	       sizeof(download->pconnections[i]->orig_remote_startpos),
	       fp) != sizeof(download->pconnections[i]->orig_remote_startpos))
	    {
	      download_show_message(download,
				    _("Error reading from file %s: %s"), buffer,
				    strerror(errno));
	      fclose(fp);
	      return -1;
	    }

	  proz_debug("orig_remote_startpos after= %d", download->pconnections[i]->orig_remote_startpos);
     

	  proz_debug("remote_edndpos before= %d", download->pconnections[i]->remote_endpos);
	  if (fread
	      (&download->pconnections[i]->remote_endpos, 1,
	       sizeof(download->pconnections[i]->remote_endpos),
	       fp) != sizeof(download->pconnections[i]->remote_endpos))
	    {
	      download_show_message(download,
				    _("Error reading from file %s: %s"), buffer,
				    strerror(errno));
	      fclose(fp);
	      return -1;
	    }
     

	  proz_debug("remote_endpos after= %d", download->pconnections[i]->remote_endpos);
	  proz_debug("remote_bytes_received before= %d", download->pconnections[i]->remote_bytes_received);
	  	  if (fread
	  	      (&download->pconnections[i]->remote_bytes_received, 1,
	       sizeof(download->pconnections[i]->remote_bytes_received),
	       fp) != sizeof(download->pconnections[i]->remote_bytes_received))
	    {
	      download_show_message(download,
				    _("Error reading from file %s: %s"), buffer,
				    strerror(errno));
	      fclose(fp);
	      return -1;
	    }

	  proz_debug("remote_bytes_received after= %d", download->pconnections[i]->remote_bytes_received);

	}

    }


  fclose(fp);
  return 1;
}
