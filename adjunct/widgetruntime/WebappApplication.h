/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBAPP_APPLICATION_H
#define WEBAPP_APPLICATION_H

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef WEBAPP_SUPPORT

#include "TemplateApplication.h"

class WebappWindow;


/** 
 * Class inteded for starting browselet (browser specialized for use with single web page).
 */
class WebappApplication : public TemplateApplication
{
		WebappApplication();

	public:
		static WebappApplication* Create();
	
	protected:
		virtual ~WebappApplication() {}

	private:
		WebappWindow *m_webapp;

		OP_STATUS	FinalizeStart();
		BOOL		AnchorSpecial(OpWindowCommander * commander,
								  const OpDocumentListener::AnchorSpecialInfo & info);

};

#endif // WEBAPP_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
#endif // WEBAPP_APPLICATION_H
