/*
 *	This program is to filter warning message from arm compiler.
 *	Stupid ARM compiler generate too many warnings and no way
 *	to disable each messages.
 *
 *	So, we should do it manually.
 *
 *	Use it following way.
 *		armcc [options] file.c 2>&1 | sift-warn
 *
 *
 */

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>

#define	MAX_SIFT_ITEMS	128
#define	OUTPUT_FILE		stdout

typedef struct
{
	int		sw_type;
	int		sw_len[3];
	char	sw_str[3][40];
} SIFT_WARN_T;

SIFT_WARN_T warn_fmts[MAX_SIFT_ITEMS] =
{
	{ -1, { 0, 0, 0 }, { "Fatal error: ",	"Too many errors",	"" } },
	{ -2, { 0, 0, 0 }, { "Serious error: ",	"Serious",			"" } },
	{ -2, { 0, 0, 0 }, { "Error: ",			"",					"" } },
	{ -2, { 0, 0, 0 }, { "error: ",			"",					"" } },
	{  0, { 0, 0, 0 }, { "",				"",					"" } },
};

static char	warn_buf[512];		/* Input warning string from compilier		*/
static char	in_str1[128];		/* Read string buffer temporary used		*/
static char	in_str2[128];		/* Read string buffer temporary used		*/
static char	in_str3[128];		/* Read string buffer temporary used		*/
static char	in_str4[128];		/* Read string buffer temporary used		*/
static char	in_str5[128];		/* Read string buffer temporary used		*/
static char	in_str6[128];		/* Read string buffer temporary used		*/
static int	nSupItems = 0;		/* Number of items in supress list			*/
static int	nTotWarns = 0;		/* Number of Total     warnings				*/
static int	nSupWarns = 0;		/* Number of Supressed warnings				*/
static int	nTotError = 0;		/* Number of Total   errors					*/
static int	nSerError = 0;		/* Number of Serious errors					*/
static int	nHiddenEr = 0;		/* Number of Hidden  errors 				*/
static int	bHasError = 0;		/* Error message 가 있었는지 여부			*/

       int	bPathConv = 0;		/* Dos style path를 Unix style Path 로 변경	*/
       int	bSilent   = 0;		/* Quiet   mode or not						*/
       int	nVerbose  = 0;		/* Verbose mode or not						*/
       int	nMaxWarns = 9999;	/* Maximum number of warns allowed			*/


/*
 *	Dos path에 사용되는 back-slash 를 Unix 용 slash로 변경한다.
 */
char *
bslash2slash(char *instr)
{
	int		i, bQuote = 0;

	for (i = 0; i < strlen(instr); i++)
	{
		if (instr[i] == '"')
		{
			if (bQuote) break;
			else        bQuote = 1;
		}
		if (instr[i] == '\\')
			instr[i] = '/';
	}
	return(instr);
}

/*
 *	Warning message 가 Warning format list에 등록되어 있는지 검색하여,
 *	등록된 내용이면, skip하거나 강조하여 출력한다.
 */
int search_warn(char *warnStr)
{
	static int	nSupressed = 0;
	char		*w_str1, *w_str2, *w_str3;
	char		*f_str1, *f_str2, *f_str3;
	int			hlColor = 0, type;
	int			i, n, rc = 0;

	if (bPathConv)
	{
		(void)bslash2slash(warnStr);
	}

	for (i = 0; i < nSupItems; i++)
	{
		w_str1 = warn_fmts[i].sw_str[0];
		w_str2 = warn_fmts[i].sw_str[1];
		w_str3 = warn_fmts[i].sw_str[2];

		f_str1 = strstr(warnStr, w_str1);
		f_str2 = strstr(warnStr, w_str2);

		if ( (f_str1 != NULL) && (f_str2 != NULL) )
		{
			if (warn_fmts[i].sw_len[2] > 0)
			{
				f_str3 = strstr(f_str2 + warn_fmts[i].sw_len[1], w_str3);
				if (f_str3 == NULL)
					continue;
			}

			type = warn_fmts[i].sw_type;

			if (type == 0)
			{
				nSupressed++;
				return(i+1);
			}
			else if (type == -1)
			{
				fprintf(OUTPUT_FILE, warn_buf);
				fprintf(OUTPUT_FILE, "Compilation aborted\n");
				fflush(OUTPUT_FILE);
				exit(1);
			}
			else if (type < -1 )
			{
				bHasError = 1;
				nHiddenEr+= ( (type < -2) ? 1 : 0);
				hlColor = 1;
				rc = -1;
			}
			else if ((type >= 1) && (type <= 7))
			{
				hlColor = warn_fmts[i].sw_type;
				rc = 1;
			}
			break;
		}
	}

	if (rc == 0)
	{
		n = sscanf(warnStr,	"%s %d %s %s %d %s %d %s %d serious %s",
							in_str1,					// cm/cm_scan.c:
							&nTotWarns, in_str2, in_str3,	// 1 warnings (+
							&nSupWarns, in_str4,			// 1 suppressed),
							&nTotError, in_str5,			// 1 errors,
							&nSerError, in_str6				// 0 serious errors
						);

		if (n == 10)
		{
			nTotWarns -= nSupressed;
			nSupWarns += nSupressed;
		}
		else
		{
			n = sscanf(warnStr,	"%s %d %s %d %s %d serious %s",
								in_str1,
								&nTotWarns, in_str2,
								&nTotError, in_str5,
								&nSerError, in_str6
						);
			if (n == 7)
			{
				nTotWarns -= nSupressed;
				nSupWarns  = nSupressed;
			}
		}

		if ( (n == 7) || (n == 10) )
		{
			nTotError += nHiddenEr;

			if (nTotWarns || nTotError || nSerError)
			{
			  if (nSupWarns > 0)
			  {
				fprintf(OUTPUT_FILE, "%s %d warning%s (+ %d supressed), %d error%s, %d serious error%s\n",
								in_str1,
								nTotWarns, (nTotWarns > 1 ? "s" : ""),
								nSupWarns,
								nTotError, (nTotError > 1 ? "s" : ""),
								nSerError, (nSerError > 1 ? "s" : "")
							);
			  }
			  else
			  {
				fprintf(OUTPUT_FILE, "%s %d warning%s, %d error%s, %d serious error%s\n",
								in_str1,
								nTotWarns, (nTotWarns > 1 ? "s" : ""),
								nTotError, (nTotError > 1 ? "s" : ""),
								nSerError, (nSerError > 1 ? "s" : "")
							);
			  }
			  fflush(OUTPUT_FILE);
			}
			return(0);
		}
	}

	if (bSilent == 0)
	{
		if (hlColor)
		{
			int		pos1, pos2, len1, len2;

			pos1 = strstr (warnStr, w_str1) - &warnStr[0];
			pos2 = strrchr(warnStr, '\n'  ) - &warnStr[0];

			len1 = pos1;
			len2 = pos2 - pos1;

			fprintf(OUTPUT_FILE, "%*.*s\x1b[1;%dm%*.*s\x1b[0m\n",
								len1, len1, &warnStr[   0], hlColor+30,
								len2, len2, &warnStr[pos1]);
		}
		else
		{
			fprintf(OUTPUT_FILE, "%s", warnStr);
		}
		fflush(OUTPUT_FILE);
	}

	return -1;
}

/*
 *	Warning list 를 화일로 부터 읽어들인다.
 */
int import_list(char *filename)
{
	FILE	*fp;
	char	*cp0, *cp1, *cp2, *cpp[3];
	int		i, n, sw_type;
	int		len1, len2;
	int		nQuotes, nItems;

	/*	Build filter configuration strings		*/
	nItems = 0;
	while (warn_fmts[nItems].sw_type < 0)
	{
		nItems++;
		for (i = 0; i < 3; i++)
		{
	    	warn_fmts[nItems].sw_len[i] = strlen(warn_fmts[nItems].sw_str[i]);
		}
	}

	if (nVerbose > 1)
	{
		fprintf(OUTPUT_FILE, "Data File  = %s\n", filename);
		fprintf(OUTPUT_FILE, "nSupItems1 = %d\n", nItems);
	}

	if ( (fp = fopen(filename, "r")) != NULL)
	{
		(void) fgets(warn_buf, 512, fp);

		while (!feof(fp))
		{
			nQuotes = 0;
			sw_type = 1;
			n = sscanf(warn_buf, "%d", &sw_type);

			if (n == 1)
			{
				cp0 = strchr(warn_buf, '"');
			}
			else
			{
				sw_type = 0;
				cp0     = &warn_buf[0];
				while (isspace(*cp0))
				{
					cp0++;
				}
			}

			if ((cp0 != NULL) && (*cp0 == '"'))
			{
				cpp[0] = cpp[1] = cpp[2] = (char *)"";

				for (nQuotes = 0; nQuotes < 3; nQuotes++)
				{
					cp1 = (cp0 ? strchr(cp0  , '"') : NULL);
					if (cp1 == NULL) break;
					cp2 = (cp1 ? strchr(cp1+1, '"') : NULL);
					if (cp2 == NULL) break;

					cpp[nQuotes] = cp1+1;
					*cp2 = '\0';
					cp0 = cp2 + 1;
				}

				if ( (cp2 != NULL) && (nQuotes >= 2) )
				{
					if (nVerbose > 1)
					{
						fprintf(OUTPUT_FILE, "%d:[%s][%s][%s]\n", sw_type, cpp[0], cpp[1], cpp[2]);
					}

		        	warn_fmts[nItems].sw_type = sw_type;
					for (i = 0; i < 3; i++)
					{
		        		snprintf(warn_fmts[nItems].sw_str[i], 40, "%s", cpp[i]);
	    				warn_fmts[nItems].sw_len[i] = strlen(warn_fmts[nItems].sw_str[i]);
					}
					nItems++;
				}
			}

			(void) fgets(warn_buf, 512, fp);
		}

		fclose(fp);
	}

	if (nVerbose > 1)
	{
		fprintf(OUTPUT_FILE, "nSupItems2 = %d\n", nItems);
	}

	return(nItems);
}


/*
 *	Parse command line arguements, and do ....
 */
int
siftWarnMain(char *cmdName, char *buf)
{
	char *cp;
	int	len;

	/*	Make data filename from command name		*/
	sprintf(in_str1, "%s.pattern", cmdName);

	/*	Call warning list builder					*/
	nSupItems = import_list(in_str1);

	/*	Now read sift waring messages by filter DB	*/
	while ((cp = strchr(buf, '\n')) != NULL)
	{
		len = cp - buf + 1;
		snprintf(warn_buf, sizeof(warn_buf), "%*.*s", len, len, buf);
		buf += len;
		(void) search_warn(&warn_buf[0]);
	}

	if ( (bHasError == 0) && (nTotError == 0) && (nSerError == 0) && (nTotWarns <= nMaxWarns) )
		return(0);
	else
		return(1);
}
