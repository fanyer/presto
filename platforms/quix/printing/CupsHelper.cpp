/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/printing/CupsHelper.h"
#include "platforms/quix/printing/CupsHelperPrivate.h"

#include "platforms/quix/printing/CupsFunctions.h"
#include "modules/pi/OpDLL.h"

struct CupsHelper::CupsOptions
{
	enum OptionType
	{
		COPIES,
		NUM_OPTIONS
	};

	OpString8 names[NUM_OPTIONS];
	OpString8 values[NUM_OPTIONS];
	cups_option_t cups_options[NUM_OPTIONS];
};

CupsHelper::CupsHelper()
	: m_private(0)
	, m_functions(0)
	, m_copies(1)
	, m_cups(0)
	, m_init_status(OpStatus::ERR)
{
}

CupsHelper::~CupsHelper()
{
	OP_DELETE(m_private);
	OP_DELETE(m_functions);
	OP_DELETE(m_cups);
}


const uni_char* CupsHelper::GetLibraryName()
{
	return UNI_L("libcups.so.2");
}


OP_STATUS CupsHelper::Init()
{
	RETURN_IF_ERROR(OpDLL::Create(&m_cups));
	RETURN_IF_ERROR(m_cups->Load(GetLibraryName()));

	m_functions = OP_NEW(CupsFunctions, ());
	if (!m_functions)
		return OpStatus::ERR_NO_MEMORY;

	m_functions->cupsPrintFile = (CupsFunctions::cupsPrintFileFunction)m_cups->GetSymbolAddress("cupsPrintFile");
	if (!m_functions->cupsPrintFile)
		return OpStatus::ERR;

	m_functions->cupsGetDests = (CupsFunctions::cupsGetDestsFunction)m_cups->GetSymbolAddress("cupsGetDests");
	if (!m_functions->cupsGetDests)
		return OpStatus::ERR;

	m_functions->cupsFreeDests = (CupsFunctions::cupsFreeDestsFunction)m_cups->GetSymbolAddress("cupsFreeDests");
	if (!m_functions->cupsFreeDests)
		return OpStatus::ERR;

	m_functions->cupsGetPPD = (CupsFunctions::cupsGetPPDFunction)m_cups->GetSymbolAddress("cupsGetPPD");
	if (!m_functions->cupsGetPPD)
		return OpStatus::ERR;

	m_functions->ppdOpenFile = (CupsFunctions::ppdOpenFileFunction)m_cups->GetSymbolAddress("ppdOpenFile");
	if (!m_functions->ppdOpenFile)
		return OpStatus::ERR;

	m_functions->ppdClose = (CupsFunctions::ppdCloseFunction)m_cups->GetSymbolAddress("ppdClose");
	if (!m_functions->ppdClose)
		return OpStatus::ERR;

	m_functions->ppdMarkDefaults = (CupsFunctions::ppdMarkDefaultsFunction)m_cups->GetSymbolAddress("ppdMarkDefaults");
	if (!m_functions->ppdMarkDefaults)
		return OpStatus::ERR;

	m_init_status = OpStatus::OK;

	return OpStatus::OK;
}


bool CupsHelper::HasConnection()
{
	return m_init_status == OpStatus::OK;
}


bool CupsHelper::PrintFile(const char* filename, const char* jobname)
{
#ifdef DEBUG
	fprintf(stderr, "Printing %s / %s to %s\n", filename, jobname, m_printername.CStr());
#endif

	if (!HasConnection())
		return false;

	CupsOptions options;
	RETURN_VALUE_IF_ERROR(CreateOptions(options), false);

	int job_id = m_functions->cupsPrintFile(m_printername.CStr(), filename, jobname, CupsOptions::NUM_OPTIONS, options.cups_options);

	return job_id != 0;
}

bool CupsHelper::GetDestinations(DestinationInfo**& info)
{
	if (!HasConnection())
		return false;

	cups_dest_t* dests = 0;
	int num_entry = m_functions->cupsGetDests(&dests);
	if (num_entry <= 0)
		return false;

	info = new DestinationInfo* [num_entry+1];
	if (!info)
	{
		m_functions->cupsFreeDests(num_entry,dests);
		return false;
	}

	for (int i=0; i<=num_entry; i++)
		info[i] = 0;

	for (int i=0; i<num_entry; i++)
	{
		info[i] = new DestinationInfo;
		if (!info[i])
		{
			ReleaseDestinations(info);
			return false;
		}

		info[i]->name = strdup(dests[i].name);
		info[i]->is_default = dests[i].is_default;

		for (int j=0; j<dests[i].num_options; j++)
		{
			if (!info[i]->media && strcasecmp(dests[i].options[j].name, "media") == 0)
			{
				info[i]->media = strdup(dests[i].options[j].value);
				if (!info[i]->media)
				{
					ReleaseDestinations(info);
					return false;
				}
			}
			else if (!info[i]->info && strcasecmp(dests[i].options[j].name, "printer-info") == 0)
			{
				info[i]->info = strdup(dests[i].options[j].value);
				if (!info[i]->info)
				{
					ReleaseDestinations(info);
					return false;
				}
			}
			else if (!info[i]->location && strcasecmp(dests[i].options[j].name, "printer-location") == 0)
			{
				info[i]->location = strdup(dests[i].options[j].value);
				if (!info[i]->location)
				{
					ReleaseDestinations(info);
					return false;
				}
			}
			else if (strcasecmp(dests[i].options[j].name, "copies") == 0)
				info[i]->copies = atoi(dests[i].options[j].value);
		}
	}

	m_functions->cupsFreeDests(num_entry,dests);
	return true;
}


void CupsHelper::ReleaseDestinations(DestinationInfo**& info)
{
	if (info)
	{
		for (int i=0; info[i]; i++)
		{
			delete info[i];
		}
		delete [] info;
	}
}


bool CupsHelper::GetPaperSizes(DestinationInfo& info)
{
	if(!m_private)
		m_private = OP_NEW(CupsHelperPrivate,());
	return m_private ? m_private->GetPaperSizes(info, m_functions) : false;
}


OP_STATUS CupsHelper::CreateOptions(CupsOptions& options)
{
	for (int i = 0; i < CupsOptions::NUM_OPTIONS; i++)
		RETURN_IF_ERROR(SetOption(options, i));

	return OpStatus::OK;
}

OP_STATUS CupsHelper::SetOption(CupsOptions& options, int option_type)
{
	switch(option_type)
	{
		case CupsOptions::COPIES:
			RETURN_IF_ERROR(options.names[option_type].Set("copies"));
			RETURN_IF_ERROR(options.values[option_type].AppendFormat("%d", m_copies));
			break;
	}

	options.cups_options[option_type].name = options.names[option_type].CStr();
	options.cups_options[option_type].value = options.values[option_type].CStr();

	return OpStatus::OK;
}
