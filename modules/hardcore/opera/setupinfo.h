/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_OPERA_SETUPINFO_H
#define MODULES_HARDCORE_OPERA_SETUPINFO_H

#ifdef SETUP_INFO

class OpSetupInfo
{
public:
	struct Feature
	{
		const uni_char* name;
		const uni_char* owner;
		const uni_char* description;
		BOOL enabled;
	};

	static int GetFeatureCount();
	static Feature GetFeature(int index);

	struct Tweak
	{
		const uni_char* name;
		const uni_char* owner;
		const uni_char* module;
		const uni_char* description;
		BOOL tweaked;
		const uni_char* value;
		const uni_char* default_value;
	};

	static int GetTweakCount();
	static Tweak GetTweak(int index);

	struct Api
	{
		const uni_char* name;
		const uni_char* owner;
		const uni_char* module;
		const uni_char* description;
		BOOL imported;
	};

	static int GetApiCount();
	static Api GetApi(int index);
};

#endif // SETUP_INFO

#endif // !MODULES_HARDCORE_OPERA_SETUPINFO_H
