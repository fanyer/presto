/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UI_DIRECTION_H
#define UI_DIRECTION_H

#include "modules/locale/oplanguagemanager.h"

/**
 * The source of information on Quick UI direction.
 *
 * Use this to obtain the Quick UI direction at any given time.  In all
 * real-life situations, the value returned from UiDirection::Get() will
 * correspond to the direction of the UI language.  Selftests, however, may use
 * UiDirection::Override to temporarily override the direction with any value
 * that is independent of the language setting.  For example:
 * @code
 *	test("Test a widget in RTL mode")
 *	{
 *		UiDirection::Override override(UiDirection::RTL);
 *		QuickWidget* widget = //...
 *		// verify stuff
 *		// ...
 *		// The UI direction override will be removed automatically as 'override'
 *		// goes out of scope.
 *	}
 * @endcode
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class UiDirection
{
public:
	enum Value
	{
		LTR, ///< left to right
		RTL  ///< right to left
	};

	inline static Value Get();

#ifdef SELFTEST
	class Override
	{
	public:
		explicit Override(Value value) { UiDirection::Set(value); }
		~Override() { UiDirection::Set(-1); }
	};

private:
	static void Set(int value) { s_value = value; }

	static int s_value;
#endif // SELFTEST
};


#ifdef SELFTEST
UiDirection::Value UiDirection::Get()
{
	return s_value != -1
			? Value(s_value)
			: g_languageManager->GetWritingDirection() == OpLanguageManager::LTR ? LTR : RTL;
}
#else // SELFTEST
UiDirection::Value UiDirection::Get()
{
	return g_languageManager->GetWritingDirection() == OpLanguageManager::LTR ? LTR : RTL;
}
#endif // SELFTEST

#endif // UI_DIRECTION_H
