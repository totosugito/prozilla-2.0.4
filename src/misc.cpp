#include "interface.h"
#include "misc.h"


int is_number(char *str)
{
    int i = 0;

    while (str[i])
    {
	if (isdigit(str[i]) == 0)
	{
	    return 0;
	}
	i++;
    }
    return 1;
}


/* TODO port these functions */
char *kstrdup(const char *s)
{
    char *s1;

    s1 = strdup(s);
    if (!s1)
    {
//	die("Not enough memory to continue: strdup failed");
    }
    return s1;

}
/* Extracts a numurical argument from a option,
   when it has been specified for example as -l=3 or, -l3 
   returns 1 on success or 0 on a error (non nemrical argument etc 
*/

int setargval(char *optstr, int *num)
{
    if (*optstr == '=')
    {
	if (is_number(optstr + 1) == 1)
	{
	    *num = atoi(optstr + 1);
	    return 1;
	} else
	{
	    return 0;
	}
    } else
    {
	if (is_number(optstr) == 1)
	{
	    *num = atoi(optstr);
	    return 1;

	} else
	{
	    return 0;
	}
    }

}

int setlongargval(char *optstr, long *num)
{
    if (*optstr == '=')
    {
	if (is_number(optstr + 1) == 1)
	{
	    *num = atol(optstr + 1);
	    return 1;
	} else
	{
	    return 0;
	}
    } else
    {
	if (is_number(optstr) == 1)
	{
	    *num = atol(optstr);
	    return 1;

	} else
	{
	    return 0;
	}
    }

}

void delay_ms(int ms)
{
    struct timeval tv_delay;

    memset(&tv_delay, 0, sizeof(tv_delay));

    tv_delay.tv_sec = ms / 1000;
    tv_delay.tv_usec = (ms * 1000) % 1000000;

    if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv_delay) < 0)
    {
	proz_debug("Warning Unable to delay\n");
    }
}
