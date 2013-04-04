/******************************************************************************
 prozilla - a front end for prozilla, a download accelerator library
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
#if HAVE_CONFIG_H
#  include <config.h>
#endif

//#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#include "prozilla.h"
#include "misc.h"
#include "download_win.h"
#include "main.h"
#include "init.h"
#include "prefs.h"
#include "interface.h"

struct runtime rt;

// structure for options parsing,

struct option long_opts[] = {
	/*
	 * { name  has_arg  *flag  val } 
	 */
	{"resume", no_argument, NULL, 'r'},
/*    {"connections", required_argument, NULL, 'c'},*/
	{"license", no_argument, NULL, 'L'},
	{"help", no_argument, NULL, 'h'},
	{"gtk", no_argument, NULL, 'g'},
	{"no-netrc", no_argument, NULL, 'n'},
	{"tries", required_argument, NULL, 't'},
	{"force", no_argument, NULL, 'f'},
	{"version", no_argument, NULL, 'v'},
	{"directory-prefix", required_argument, NULL, 'P'},
	{"use-port", no_argument, NULL, 129},
	{"retry-delay", required_argument, NULL, 130},
	{"timeout", required_argument, NULL, 131},
	{"no-getch", no_argument, NULL, 132},
	{"debug", no_argument, NULL, 133},
	{"ftpsearch", no_argument, NULL, 's'},
	{"no-search", no_argument, NULL, 135},
	{"pt", required_argument, NULL, 136},
	{"pao", required_argument, NULL, 137},
	{"max-ftps-servers", required_argument, NULL, 138},
	{"max-bps", required_argument, NULL, 139},
	{"verbose", no_argument, NULL, 'v'},
	{"no-curses", no_argument, NULL, 140},
	{"min-size",required_argument,NULL,141},
	{"ftpsid", required_argument, NULL,142},
	{0, 0, 0, 0}
};

int open_new_dl_win (urlinfo * url_data, boolean ftpsearch);

DL_Window *dl_win = NULL;

extern int optind;
extern int opterr;
extern int nextchar;

/* displays the software license */
void
license (void)
{
	fprintf (stderr,
		 "   Copyright (C) 2000 Kalum Somaratna\n"
		 "\n"
		 "   This program is free software; you can redistribute it and/or modify\n"
		 "   it under the terms of the GNU General Public License as published by\n"
		 "   the Free Software Foundation; either version 2, or (at your option)\n"
		 "   any later version.\n"
		 "\n"
		 "   This program is distributed in the hope that it will be useful,\n"
		 "   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		 "   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		 "   GNU General Public License for more details.\n"
		 "\n"
		 "   You should have received a copy of the GNU General Public License\n"
		 "   along with this program; if not, write to the Free Software\n"
		 "   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n");
}


/* displays the help message */

void
help (void)
{
	fprintf (stderr,
		 "Usage: proz [OPTIONS] file_url\n"
		 "\n"
		 "Ex: proz http://gnu.org/gnu.jpg\n"
		 "\n"
		 "Options:\n"
		 "      -h, --help        Give this help\n"
		 "      -r, --resume      Resume an interrupted download\n"
		 "      -f, --force       Never prompt the user when overwriting files\n"
		 "      -1                Force a single connection only\n"
		 "      -n, --no-netrc    Don't use .netrc, get the user/password\n"
		 "                        from the command line,otherwise use the\n"
		 "                        anonymous login for FTP sessions\n"
		 "      --no-getch        Instead of waiting for the user pressing a key,\n"
		 "                        print the error to stdout and quit\n"
		 "      --debug           Log debugging info to a file (default is debug.log)\n"
		 "      -v,--verbose      Increase the amount of information sent to stdout\n"
		 "      --no-curses       Don't use Curses, plain text to stdout\n"
		 "\n"
		 "Directories:\n"
		 "      -P,  --directory-prefix=DIR  save the generated file to DIR/\n"
		 "\n"
		 "FTP Options:\n"
		 "      --use-port        Force usage of PORT insted of PASV (default)\n"
		 "                        for ftp transactions\n"
		 "\n"
		 "Download Options:\n"
		 "      -s,  --ftpsearch  Do a ftpsearch for faster mirrors\n"
		 "      --no-search       Do a direct download (no ftpsearch)\n"
		 "      -k=n              Use n connections instead of the default(4)\n"
		 "      --timeout=n       Set the timeout for connections to n seconds\n"
		 "                        (default 180)\n"
		 "      -t, --tries=n     Set number of attempts to n (default(200), 0=unlimited)\n"
		 "      --retry-delay=n   Set the time between retrys to n seconds\n"
		 "                        (default 15 seconds)\n"
		 "      --max-bps=n       Limit bandwith consumed to n bps (0=unlimited)\n"
		 "\n"
		 "FTP Search Options:\n"
		 "      --pt=n            Wait 2*n seconds for a server response (default 2*4)\n"
		 "      --pao=n           Ping n servers at once(default 5 servers at once)\n"
		 "      --max-ftps-servers=n  Request a max of n servers from ftpsearch (default 40)\n"
		 "      --min-size=n      If a file is smaller than 'n'Kb, don't search, just download it\n"
		 "      --ftpsid=n        The ftpsearch server to use\n" 
		 "                        (0=filesearching.com\n"
		 "                        1=ftpsearch.elmundo.es\n"
		 "\n"
		 "Information Options:\n"
		 "      -L, --license     Display software license\n"
		 "      -V, --version     Display version number\n"
		 "\n"
		 "ProZilla homepage: http://prozilla.genesys.ro\n"
		 "Please report bugs to <prozilla@genesys.ro>\n");
}



/* Displays the version */

void
version (void)
{
	fprintf (stderr, _("%s. Version: %s\n"), PACKAGE_NAME, VERSION);
}



void
ms (const char *msg, void *cb_data)
{
      PrintMessage("%s\n",msg);
}


int
open_new_dl_win (urlinfo * url_data, boolean ftpsearch)
{

	dl_win = new DL_Window (url_data);

	dl_win->dl_start (rt.num_connections, ftpsearch);

	proz_debug ("calling the callback function...");

	//need a timer here...
	while (dl_win->status != DL_DONE && dl_win->status != DL_IDLING && dl_win->status != DL_ABORTED)
	{
		delay_ms (700);	//wait before checking the status again...
		dl_win->my_cb ();
	}

	

return((dl_win->status==DL_DONE) ? 1:-1);
	//	delete (dl_win);
}


int
main (int argc, char **argv)
{
	int c;
	int ret;
	proz_init (argc, argv);	//init libprozilla
	set_defaults ();	//set some reasonable defaults
	load_prefs ();		//load values from the config file

	while ((c =
		getopt_long (argc, argv, "?hvrfk:1Lt:VgsP:", long_opts,
			     NULL)) != EOF)
	{
		switch (c)
		{
		case 'L':
			license ();
			exit (0);
		case 'h':
			help ();
			exit (0);
		case 'V':
			version ();
			exit (0);
		case 'r':
			rt.resume_mode = RESUME;
			break;
		case 'f':
			rt.force_mode = TRUE;
			break;
		case 'k':
			if (setargval (optarg, &rt.num_connections) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the -k option\n" "Please type proz --help for help\n"));
				exit (0);
			}

			if (rt.num_connections == 0)
			{
				printf (_("Hey! How can I download anything with 0 (Zero)" " connections!?\n" "Please type proz --help for help\n"));
				exit (0);
			}

			break;
		case 't':
			if (setargval (optarg, &rt.max_attempts) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the -t or --tries option(s)\n" "Please type proz --help for help\n"));
				exit (0);
			}
			break;
		case 'n':
			/*
			 * Don't use ~/.netrc" 
			 */
			rt.use_netrc = FALSE;
			break;

		case 'P':
			/*
			 * Save the downloaded file to DIR 
			 */
			rt.output_dir = kstrdup (optarg);
			break;
		case '?':
			help ();
			exit (0);
			break;
		case '1':
			rt.num_connections = 1;
			break;

		case 'g':
			/*
			 * TODO solve this soon 
			 */
			printf ("Error: GTK interface is not supported in "
				"the development version currently\n");
			exit (0);
			break;

		case 129:
			/*
			 * lets use PORT as the default then 
			 */
			rt.ftp_use_pasv = FALSE;
			break;
		case 130:
			/*
			 * retry-delay option 
			 */
			if (setargval (optarg, &rt.retry_delay) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --retry-delay option\n" "Please type proz --help for help\n"));
				exit (0);
			}
			break;
		case 131:
	    /*--timout option */
			if (setargval (optarg, &rt.itimeout) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --timeout option\n" "Please type proz --help for help\n"));
				exit (0);
			}
			break;
		case 132:
			/* --no-getch option */
			rt.dont_prompt = TRUE;
			break;

		case 133:
			/* --debug option */
			rt.debug_mode = TRUE;
			rt.libdebug_mode=TRUE;
			break;

		case 'v':
			/* --verbose option */
			rt.quiet_mode = FALSE;
			break;

		case 's':
			/* --ftpsearch option */
			rt.ftp_search = TRUE;
			break;

		case 135:
			/* --no-search option */
			rt.ftp_search = FALSE;
			break;

		case 136:
			/* --pt option */
			if (setargval (optarg, &rt.max_ping_wait) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --pt option\n" "Please type proz --help for help\n"));
				exit (0);
			}

			if (rt.max_ping_wait == 0)
			{
				printf (_("Hey! Does waiting for a server response for Zero(0)" " seconds make and sense to you!?\n" "Please type proz --help for help\n"));
				exit (0);
			}

			break;
		case 137:
			/* --pao option */
			if (setargval (optarg, &rt.max_simul_pings) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --pao option\n" "Please type proz --help for help\n"));
				exit (0);
			}

			if (rt.max_simul_pings == 0)
			{
				printf (_("Hey you! Will pinging Zero(0) servers at once" " achive anything for me!?\n" "Please type proz --help for help\n"));
				exit (0);
			}

			break;

		case 138:
			/* --max-ftp-servers option */
			if (setargval (optarg, &rt.ftps_mirror_req_n) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --pao option\n" "Please type proz --help for help\n"));
				exit (0);
			}

			if (rt.ftps_mirror_req_n == 0)
			{
				printf (_("Hey! Will requesting Zero(0) servers at once" "from the ftpearch achive anything!?\n" "Please type proz --help for help\n"));
				exit (0);
			}

			break;
		case 139:
			/* --max-bps */
			if (setlongargval (optarg, &rt.max_bps_per_dl) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --max-bps option\n" "Please type proz --help for help\n"));
				exit (0);
			}
			break;
		case 140:
      rt.display_mode = DISP_STDOUT;
      break;
		case 141:
			/* --min-size */
			if (setlongargval (optarg, &rt.min_search_size) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --min-size option\n" "Please type proz --help for help\n"));
				exit (0);
			}
			break;

		case 142:
			/* --ftpsid */
		
			if (setargval (optarg, &rt.ftpsearch_server_id) != 1)
			{
				/*
				 * The call failed  due to a invalid arg
				 */
				printf (_("Error: Invalid arguments for the --ftpsid option\n" "Please type proz --help for help\n"));
				exit (0);
			}

			if (rt.ftpsearch_server_id < 0 || rt.ftpsearch_server_id >1)
			{
				printf (_("The available servers are (0) filesearching.com and (1) ftpsearch.elmundo.es\n" "Please type proz --help for help\n"));
				exit (0);
			}

			break;



		default:
			printf (_("Error: Invalid  option\n"));
			exit (0);
		}
	}

	set_runtime_values ();	//tell libprozilla about any changed settings

	if (optind == argc)
	{
		help ();
	}
	else
	{
		/* Gettext stuff */
		setlocale (LC_ALL, "");
		bindtextdomain (PACKAGE, LOCALEDIR);
		textdomain (PACKAGE);

		/*delete the ~/.prozilla/debug.log file if present at the start of each run */
		proz_debug_delete_log ();

    if (rt.display_mode == DISP_CURSES)
      initCurses();
    
		/* we will now see whether the user has specfied any urls in the command line  and add them */
		for (int i = optind; i < argc; i++)
		{
			uerr_t err;
			urlinfo *url_data;
			url_data = (urlinfo *) malloc (sizeof (urlinfo));
			memset (url_data, 0, sizeof (urlinfo));

			//parses and validates the command-line parm
			err = proz_parse_url (argv[i], url_data, 0);
			if (err != URLOK)
			{
				PrintMessage (_
					("%s does not seem to be a valid URL"),
					argv[optind]);
				proz_debug
					("%s does not seem to be a valid URL",
					 argv[optind]);
				exit (0);
			}

			PrintMessage("Starting.....");
	//In to %s\n",url_data->host);
			// start the download
			ret=open_new_dl_win (url_data, rt.ftp_search);
			/*If the download failed the return -1 */
			if(ret==-1)
			  {
			    free(url_data);
			    delete(dl_win);
			    shutdown();
			    return -1;
			  }
			delete(dl_win);
			free (url_data);
		}
	}

  shutdown();

}

void shutdown(void)
{

   cleanuprt ();

  if (rt.display_mode == DISP_CURSES)
    shutdownCurses();
    
   proz_shutdown ();

}
