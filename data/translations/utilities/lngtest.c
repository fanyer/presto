/*
 * Simple program to be used by translators to transform an Opera PO file
 * into a simplistic LNG file for internal testing.
 *
 * Copyright © 2003-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

#define TRUE 1
#define FALSE 0

#define ID_LANGUAGENAME -1
#define ID_LANGUAGECODE -2

#define LINELEN 1024
#define STRLEN 4096

int linenum;
char *poname;
int parseid(char *data);
void warning(int line, char *format, ...);
int hash(const char *s, int n);

int main(int argc, char *argv[])
{
	FILE *po, *lng;
	char *lngname, *p, line[LINELEN], translation[STRLEN], name[LINELEN];
	int hasid, hasstr, hasname, hasline, headerdone, intranssection, usehash;
	int hasheader, haslngname, haslngcode, hasctxt, hasmsgctxt;
	int strnum, fuzzyline, cformatline, conflictline, namehash;
	time_t nowtime;
	struct tm *timestruct_p;

	if ((argc < 2 || argc > 3) || '/' == argv[1][0] || '-' == argv[1][0])
	{
		printf("Usage: %s PO-file [nohash]\n\n"
		       "Converts the Opera PO file specified on the command line "
			   "into a simple LNG\n"
		       "file suitable for internal testing.\n\n"
		       "Write \"nohash\" as the second parameter to not use hashed "
		       "identifiers\n"
		       "(before Opera 9).\n\n"
		       "Copyright (C) 2003-2011 Opera Software ASA. Not to be "
			   "distributed.\n"
#if defined __DATE__ && defined __TIME__
			   "Built " __DATE__ " " __TIME__ "\n"
#endif
			   , argv[0]);
		return 0;
	}

	/* Check for nohash flag */
	if (3 == argc)
	{
		if (0 == strcmp(argv[2], "nohash"))
		{
			usehash = 0;
		}
		else
		{
			warning(0, "Invalid argument: %s\n", argv[2]);
			return 1;
		}
	}
	else
	{
		usehash = 1;
	}

	/* Construct output file name */
	lngname = malloc(strlen(argv[1]) + 5);
	strcpy(lngname, argv[1]);
	p = strrchr(lngname, '.');
	if (!p)
	{
		warning(0, "Malformed PO file name %s: no full stop.\n",
		        argv[1]);
		return 1;
	}
	strcpy(p, ".lng");

	poname = argv[1];
	if (0 == strcmp(lngname, poname))
	{
		warning(0, "Malformed PO file name %s: ends in \".lng\".\n",
		        poname);
		return 1;
	}

	/* Open input */
	printf("Opening %s\n", poname);
	po = fopen(poname, "rt");
	if (!po)
	{
		warning(0, "Cannot open %s: %s.\n",
		        poname, strerror(errno));
		return 2;
	}

	/* Create output */
	printf("Creating %s\n", lngname);
	lng = fopen(lngname, "wt");
	if (!lng)
	{
		warning(0, "Cannot create %s: %s.\n",
		        lngname, strerror(errno));
		return 2;
	}

	nowtime = time(NULL);
	timestruct_p = localtime(&nowtime);

	fprintf(lng, "; Opera language file version 2.0\n"
	             "; For internal testing only, may not be distributed.\n"
	             "; Copyright (C) 1995-%u Opera Software ASA. All rights reserved.\n\n"
	             "[Info]\n",
	        timestruct_p->tm_year + 1900);

	hasid = FALSE;
	hasstr = FALSE;
	hasname = FALSE;
	headerdone = FALSE;
	intranssection = FALSE;
	hasheader = FALSE;
	haslngname = FALSE;
	haslngcode = FALSE;
	hasctxt = FALSE;
	hasmsgctxt = FALSE;
	fuzzyline = 0;
	cformatline = 0;
	conflictline = 0;

	/* Read PO file and transform */
	linenum = 0;
	do
	{
		hasline = (NULL != fgets(line, LINELEN, po));

		++ linenum;
		if (hasline && 0 == strncmp(line, "#: ", 3))
		{
			/* New entry starting */
			char *colon = strchr(line + 3, ':');
			if (colon)
			{
				hasname = TRUE;
				namehash = hash(line + 3, colon - line - 3);
				strncpy(name, line + 3, colon - line - 3);
				name[colon - line - 3] = 0;
			}
			else
			{
				warning(linenum, "Malformed header.\n");
			}
		}
		else if (hasline && !hasctxt && 0 == strncmp(line, "msgctxt ", 8))
		{
			hasmsgctxt = TRUE;
			if (!usehash)
			{
				warning(linenum, "Nohash mode not supported for msgctxt.\n");
				return 1;
			}


			/* Make sure context matches "#:" line */
			if (hasname)
			{
				char *firstquote, *lastquote;

				/* Get context name */
				firstquote = strchr(line, '"');
				lastquote = strrchr(line, '"');
				if (firstquote && lastquote > firstquote + 1)
				{
					if (hash(firstquote + 1, lastquote - firstquote - 1)
					    != namehash)
					{
						warning(linenum, "Malformed PO file: invalid msgctxt.\n");
						return 1;
					}
				}

				hasctxt = TRUE;
			}
			else
			{
				warning(linenum, "Malformed header.\n");
			}
		}
		else if (hasline && 0 == strncmp(line, "msgid ", 6))
		{
			/* New entry starting */
			hasid = TRUE;
			hasstr = FALSE;
			strnum = 0;

			/* Check if we are done with the header */
			if (!headerdone && strstr(line, "::") != NULL)
			{
				headerdone = TRUE;
			}
			if (!headerdone)
			{
				/* Meta data */
				if (strstr(line, "<LanguageName>") != NULL)
				{
					strnum = ID_LANGUAGENAME;
					haslngname = TRUE;
				}
				else if (strstr(line, "<LanguageCode>") != NULL)
				{
					strnum = ID_LANGUAGECODE;
					haslngcode = TRUE;
				}
				else if (0 == strcmp(line, "msgid \"\"\n") || 0 == strcmp(line, "msgid \"\"\r\n") )
				{
					/* Do nothing with the initial header */
					hasid = FALSE;
				}
				else
				{
					warning(linenum, "Cannot parse line:\n %s", line);
				}
			}
			else
			{
				/* Standard string, get the id number */
				if (hasmsgctxt)
				{
					strnum = -1;
				}
				else
				{
					hasmsgctxt = FALSE;
					strnum = parseid(line);
				}
				switch (strnum)
				{
				case -1: /* Not found */
					strnum = hasctxt ? namehash : 0;
					break;
				case -2: /* Syntax error */
					hasid = FALSE;
					break;
				}
			}
		}
		else if (hasline && hasid && 0 == strncmp(line, "msgstr ", 7))
		{
			char *firstquote, *lastquote;

			/* Begin a new translated string */
			hasstr = TRUE;
			translation[0] = 0;

			/* Get data */
			firstquote = strchr(line, '"');
			lastquote = strrchr(line, '"');
			if (lastquote > firstquote + 1)
			{
				if (lastquote - firstquote > STRLEN)
				{
					warning(linenum,
					        "Translated string too long, ignored:\n %s",
					        line);
				}
				else
				{
					memcpy(translation, firstquote + 1,
					       lastquote - firstquote - 1);
					translation[lastquote - firstquote - 1] = 0;
				}
			}
		}
		else if (hasline && (hasid || hasstr) && '"' == line[0])
		{
			if (0 == strnum && hasid && !hasstr)
			{
				/* Retrieve ID from first split line */
				strnum = parseid(line);
				if (strnum < 0 && (!usehash || strnum != namehash))
				{
					/* There should have been an ID here, cannot parse entry */
					warning(linenum,
					        "Cannot parse entry, invalid identifier.");
					hasid = FALSE;
				}
			}
			else if (hasstr)
			{
				/* Continue translated string */
				char *firstquote = strchr(line, '"');
				char *lastquote = strrchr(line, '"');
				if (lastquote > firstquote + 1)
				{
					if (lastquote - firstquote + strlen(translation) > STRLEN)
					{
						warning(linenum, "Translated string too long, "
						        "ignored:\n %s\n",
						        line);
					}
					else
					{
						size_t old_strlen = strlen(translation);
						memcpy(translation + old_strlen,
						       firstquote + 1,
						       lastquote - firstquote - 1);
						translation[old_strlen + lastquote - firstquote - 1] = 0;
					}
				}
			}
		}
		else if (hasid && hasstr)
		{
			/* Report any delayed errors */
			if (fuzzyline)
			{
				if (cformatline)
				{
					warning(fuzzyline,
					        "Fuzzy c-format string discarded for %d.\n",
					        strnum);
				}
				else
				{
					warning(fuzzyline, "Fuzzy translation for %d.\n", strnum);
				}
			}
			if (conflictline)
			{
				warning(conflictline, "Translation conflict for %d.\n", strnum);
			}

			/* We're done with the entry, print it to the file */
			if (!headerdone && strnum < 0)
			{
				/* Special header strings */
				switch (strnum)
				{
				case ID_LANGUAGENAME:
					if (!*translation)
					{
						warning(linenum - 1, "Language name empty.\n");
					}
					else if (*translation == '<')
					{
						warning(linenum - 1,
						        "Language name starts with \"<\".\n");
					}
					fprintf(lng, "LanguageName=\"%s (testing)\"\n", translation);
					break;

				case ID_LANGUAGECODE:
					if (!*translation)
					{
						warning(linenum - 1, "Language code empty.\n");
					}
					else if (*translation == '<')
					{
						warning(linenum - 1,
						        "Language code starts with \"<\".\n");
					}
					else if (!islower(*(unsigned char *) translation) ||
					         !islower(*(unsigned char *) &translation[1]) ||
					         !('_' == translation[2] || 0 == translation[2]))
					{
						warning(linenum - 1,
						        "Language code not in ISO format:\n %s\n",
						        translation);
					}
					fprintf(lng, "Language=\"%s\"\n", translation);
					break;
				}
				if (hasheader && haslngname && haslngcode)
				{
					headerdone = TRUE;
				}
			}
			else if (hasname && headerdone &&
			         (strnum > 0 || (strnum < 0 && strnum == namehash)))
			{
				/* Regular translated string */
				if (namehash == 1850176044 /* hash("S_HTML_DIRECTION") */)
				{
					/* Magic for S_HTML_DIRECTION */
					if (strcmp(translation, "ltr") == 0 ||
					    strcmp(translation, "rtl") == 0)
					{
						fprintf(lng, intranssection ? "\n[Info]\nDirection=%d\n"
						                            : "Direction=%d\n",
						        strcmp(translation, "rtl") == 0);
						intranssection = FALSE;
					}
					else
					{
						warning(linenum - 1, "Bad S_HTML_DIRECTION string: \"%s\".\n",
						        translation);
					}
				}
				if (!intranssection)
				{
					fputs("\n[Translation]\n\n", lng);
					intranssection = TRUE;
				}
				fprintf(lng, (usehash && !hasctxt) ? "; %s:%d\n" : "; %s\n", name, strnum);
				if (!usehash)
				{
					namehash = strnum;
				}
				if (fuzzyline && cformatline)
				{
					fprintf(lng, "%d=\"[FUZZY]\"\n", namehash);
				}
				else if (fuzzyline)
				{
					fprintf(lng, "%d=\"[FUZZY]%s\"\n", namehash, translation);
				}
				else if (0 == *translation)
				{
					warning(linenum - 1, "Missing translation for %d.\n", strnum);
					fprintf(lng, "; Untranslated %d=\"%s\"\n",
					        namehash, translation);
				}
				else
				{
					if (0 == strcmp(translation, "[]"))
					{
						/* Special convention for empty strings */
						translation[0] = 0;
					}
					fprintf(lng, "%d=\"%s\"\n", namehash, translation);
				}
			}
			else if (!hasname)
			{
				/* PO header line missing/broken */
				warning(linenum, "Missing header, entry for %d discarded.\n", strnum);
			}
			else
			{
				/* Sanity check */
				warning(linenum, "Internal program error: id number == %d.\n",
				        strnum);
			}

			/* Prepare for next entry */
			hasstr = FALSE;
			hasid = FALSE;
			hasname = FALSE;
			hasctxt = FALSE;
			fuzzyline = 0;
			cformatline = 0;
			conflictline = 0;
		}
		else if (hasline && !headerdone && 0 == strncmp(line, "\"Content-Type: ", 15))
		{
			/* Extract charset information from the PO file */
			char *charset = strstr(line, "charset=");
			if (charset)
			{
				char *backslash = strchr(charset + 8, '\\');
				if (backslash)
				{
					fprintf(lng, "Charset=\"%.*s\"\n",
					        (int) (backslash - charset - 8), charset + 8);
				}
				else
				{
					warning(linenum, "Cannot parse charset:\n %s\n",
					        charset);
				}
				hasheader = TRUE;
			}
		}
		else if (hasline && !headerdone && 0 == strncmp(line, "\"Project-Id-Version: ", 21))
		{
			/* Extract database information from the PO file */
			char *dbversion = strstr(line, "dbversion ");
			if (dbversion)
			{
				char *backslash = strchr(dbversion + 10, '\\');
				if (backslash)
				{
					fprintf(lng, "DB.version=\"%.*s\"\n",
					        (int) (backslash - dbversion - 10), dbversion + 10);
				}
				else
				{
					warning(linenum, "Cannot parse dbversion:\n %s\n",
					        dbversion);
				}
				hasheader = TRUE;
			}
		}
		else if (hasline && !hasid && '#' == line[0])
		{
			/* Check fuzzy flag */
			if (strstr(line, ", fuzzy") != NULL)
			{
				fuzzyline = linenum;
			}

			/* Check c-format flag */
			if (strstr(line, ", c-format") != NULL)
			{
				cformatline = TRUE;
			}

			/* Check translation conflict flag */
			if (0 == strncmp(line, "# Translation conflict:", 23))
			{
				conflictline = linenum;
			}
		}
	} while (hasline);

	/* Finish up */
	fclose(po);
	fclose(lng);
	puts("Done.");

	return 0;
}

int parseid(char *data)
{
	int strnum = -1;
	char *delim = strstr(data, "::");
	if (NULL != delim)
	{
		char *quote = strchr(data, '"');
		if (quote < delim)
		{
			char *idstr = malloc(delim - quote);
			int test;

			memcpy(idstr, quote + 1, delim - quote - 1);
			idstr[delim - quote - 1] = 0;
			test = sscanf(idstr, "%d", &strnum);
			if (test != 1)
			{
				warning(linenum, "Cannot parse ID \"%s\":\n %s",
				        idstr, data);
				strnum = -2;
			}
			free(idstr);
		}
	}
	return strnum;
}

void warning(int line, char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);

	/* Print error message with line number reference, if available.
	 * If the line number is zero, this is an error.
	 */
	if (line > 0)
	{
		fprintf(stderr, "%s(%d): Warning: ", poname, line);
	}
	else
	{
		fputs("Error: ", stderr);
	}
	vfprintf(stderr, format, arguments);
	va_end(arguments);
}

int hash(const char *s, int n)
{
	/* Hash: djb2 This algorithm was first reported by Dan
	 * Bernstein many years ago in comp.lang.c.  It is easily
	 * implemented and has relatively good statistical properties.
	 */
	unsigned long hashval = 5381;
	while (n --)
	{
		hashval += (hashval << 5) + *(s ++); // hash*33 + c
	}
	return (int) hashval;
}
