/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef CUPS_FUNCTIONS_H
#define CUPS_FUNCTIONS_H

#ifndef CUPS_INCLUDED

typedef struct cups_option_s
{
	char		*name;
	char		*value;
} cups_option_t;


typedef struct cups_dest_s {
    char *name, *instance;
    int is_default;
    int num_options;
    cups_option_t *options;
} cups_dest_t;
#endif

struct CupsFunctions
{
	typedef int (*cupsPrintFileFunction) (const char* destname, const char* filename, const char* title, int num_options, cups_option_t* options);
	typedef int (*cupsGetDestsFunction) (cups_dest_t **dests);
	typedef int (*cupsFreeDestsFunction) (int num_dests, cups_dest_t *dests);
	typedef const char* (*cupsGetPPDFunction) (const char* name);
	typedef /*ppd_file_t**/ void* (*ppdOpenFileFunction) (const char *filename);
	typedef void (*ppdCloseFunction) (/*ppd_file_t**/ void* ppd);
	typedef void (*ppdMarkDefaultsFunction) (/*ppd_file_t**/void* ppd);

	cupsPrintFileFunction cupsPrintFile;
	cupsGetDestsFunction cupsGetDests;
	cupsFreeDestsFunction cupsFreeDests;
	cupsGetPPDFunction cupsGetPPD;
	ppdOpenFileFunction ppdOpenFile;
	ppdCloseFunction ppdClose;
	ppdMarkDefaultsFunction ppdMarkDefaults;
};

#endif // CUPS_FUNCTIONS_H
