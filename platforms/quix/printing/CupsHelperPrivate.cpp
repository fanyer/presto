/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

// This file can not include pch.h as there are defines that will make the cups include
// fail. We have to include cups.h because the structure we use in this file requires
// almost all from cups.h to be defined

#include "CupsHelperPrivate.h"

#include <cups/cups.h>
#include <cups/ppd.h>
#define CUPS_INCLUDED

#include "platforms/quix/printing/CupsFunctions.h"
#include "platforms/quix/toolkits/ToolkitPrinterHelper.h" // Defines DestinationInfo

static const ppd_option_t* GetPPDOption(ppd_file_t* ppd, const char *key)
{
	if (ppd)
	{
		for (int gr = 0; gr < ppd->num_groups; ++gr)
		{
			for (int opt = 0; opt < ppd->groups[gr].num_options; ++opt)
			{
				if (strcmp(ppd->groups[gr].options[opt].keyword, key) == 0)
					return &ppd->groups[gr].options[opt];
			}
		}
	}
	return 0;
}


bool CupsHelperPrivate::GetPaperSizes(DestinationInfo& info, CupsFunctions* functions)
{
	if (info.paperlist)
		return true;

	if (!info.name)
		return false;

	const char* ppd_file = functions->cupsGetPPD(info.name);
	if (!ppd_file)
		return false;

	ppd_file_t* ppd = (ppd_file_t*)functions->ppdOpenFile(ppd_file);
	unlink(ppd_file);
	if (!ppd)
		return false;

	functions->ppdMarkDefaults(ppd);

	const ppd_option_t* page_sizes = GetPPDOption(ppd, "PageSize");
	if (!page_sizes || page_sizes->num_choices <= 0)
	{
		functions->ppdClose(ppd);
		return false;
	}

	info.paperlist = (DestinationInfo::PaperItem**)calloc(page_sizes->num_choices, sizeof(DestinationInfo::PaperItem*));
	if (!info.paperlist)
	{
		functions->ppdClose(ppd);
		return false;
	}

	info.paperlist_size = page_sizes->num_choices;

	for (int i=0; i<page_sizes->num_choices; i++)
	{
		int j;
		for (j=0; j<ppd->num_sizes; j++)
		{
			if (!strcmp(page_sizes->choices[i].choice, ppd->sizes[j].name))
				break;
		}

		info.paperlist[i] = (DestinationInfo::PaperItem*)malloc(sizeof(DestinationInfo::PaperItem));
		if (!info.paperlist[i] || !(info.paperlist[i]->name = strdup(page_sizes->choices[i].text)))
		{
			for( ; i>=0; i--)
				free(info.paperlist[i]);
			free(info.paperlist);
			info.paperlist = 0;
			info.paperlist_size = 0;
			return false;
		}

		if (page_sizes->choices[i].marked)
			info.default_paper = i;

		if (j < ppd->num_sizes)
		{
			info.paperlist[i]->width = ppd->sizes[j].width;
			info.paperlist[i]->length = ppd->sizes[j].length;
		}
	}

	return true;
}
