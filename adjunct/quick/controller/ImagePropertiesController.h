/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */

#ifndef IMG_PROPERTIES_CONTROLLER_H
#define IMG_PROPERTIES_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/CloseDialogContext.h"
#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"
#include "modules/about/opgenerateddocument.h"
#include "modules/url/url2.h"


class ImagePropertiesController
		: public CloseDialogContext
		, public BrowserViewListener
{
public:
	ImagePropertiesController(const URL& url, const OpStringC& alt, const OpStringC& longdesc);

	virtual void OnBrowserViewReady(BrowserView& browser_view);

protected:
	virtual void InitL();

private: 
	class ImagePropertiesHTMLView : public OpGeneratedDocument
	{
	public:
		ImagePropertiesHTMLView(URL url, Image *img)
			: OpGeneratedDocument(url, HTML5)
			, m_image(img)
		{}

		OP_STATUS GenerateData();
	private:
		Image *m_image;
	};

    OP_STATUS InitProperties();
	OP_STATUS MakeNiceExposureTime(OpString8& exposure_time);

	URL m_url;
	Image m_image;
	const OpStringC& m_alt;
	const OpStringC& m_longdesc;
};

#endif //IMG_PROPERTIES_CONTROLLER_H
