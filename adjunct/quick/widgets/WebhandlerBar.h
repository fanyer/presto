/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef WEBHANDLER_BAR_H
#define WEBHANDLER_BAR_H

#include "adjunct/quick/widgets/OpInfobar.h"

class WebhandlerBar : public OpInfobar
{
public:
	static OP_STATUS Construct(WebhandlerBar** obj);

protected:
	WebhandlerBar(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref, PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);
	virtual ~WebhandlerBar() {}

	virtual const char* GetStatusFieldName() { return "tbb_WebHandlerStatusText"; }

	virtual const char* GetQuestionFieldName() { return "tbb_WebHandlerQuestionText"; }

	virtual void OnReadContent(PrefsSection* section);
};

#endif // WEBHANDLER_BAR
