/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MOCK_CUPS_FUNCTIONS_H
#define MOCK_CUPS_FUNCTIONS_H

#ifdef SELFTEST

#include "platforms/quix/printing/CupsFunctions.h"

class MockCupsFunctions : public CupsFunctions
{
public:
	MockCupsFunctions()
	  : printfile_dest(0)
	  , printfile_file(0)
	  , printfile_job(0)
	{
		s_functions = this;
		cupsPrintFile = &PrintFile;
	}

	static int PrintFile(const char* destname, const char* filename, const char* title, int num_options, cups_option_t* options)
	{
		s_functions->printfile_dest = destname;
		s_functions->printfile_file = filename;
		s_functions->printfile_job = title;

		for (int i = 0; i < num_options; i++)
		{
			Option* option = OP_NEW(Option, ());
			option->name.Set(options[i].name);
			option->value.Set(options[i].value);
			s_functions->printfile_options.Add(option);
		}

		return 1;
	}

	const char* GetOptionValue(const char* name)
	{
		for (unsigned i = 0; i < printfile_options.GetCount(); i++)
		{
			if (printfile_options.Get(i)->name.Compare(name) == 0)
				return printfile_options.Get(i)->value.CStr();
		}

		return 0;
	}

	const char* printfile_dest;
	const char* printfile_file;
	const char* printfile_job;

private:
	struct Option
	{
		OpString8 name;
		OpString8 value;
	};
	OpAutoVector<Option> printfile_options;

	static MockCupsFunctions* s_functions;
};

MockCupsFunctions* MockCupsFunctions::s_functions = 0;

#endif // SELFTEST

#endif // MOCK_CUPS_FUNCTIONS_H
