/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_IMAGE_WIDGET_H
#define OP_IMAGE_WIDGET_H

#include "OpBrowserView.h"
#include "modules/url/url_man.h"
#ifdef USE_ABOUT_FRAMEWORK
#include "modules/about/opgenerateddocument.h"
#endif

/***********************************************************************************
**
**	OpWebImageWidget
**
***********************************************************************************/

class OpWebImageWidget : public OpBrowserView
{
	public:
		static OP_STATUS Construct(OpWebImageWidget** obj);

		OpWebImageWidget(BOOL border = TRUE) : OpBrowserView(border) {}

		// (mortenha) Defaultvalues for the scale properties are based on how
		// the widget previously worked by default.
		virtual void SetImage(const uni_char* path, BOOL scale_width = TRUE, BOOL scale_height = FALSE);

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_WEBIMAGE;}

	protected:

		virtual OP_STATUS Init();

	private:
#ifdef USE_ABOUT_FRAMEWORK
		class OpWebImageWidgetHTMLView : public OpGeneratedDocument
		{
		public:
			OpWebImageWidgetHTMLView(URL &url, const uni_char *path, BOOL scale_width, BOOL scale_height)
				: OpGeneratedDocument(url, HTML5)
				, path(path)
				, scale_width(scale_width)
				, scale_height(scale_height)
			{}

			OP_STATUS GenerateData();

		private:
			const uni_char *path;
			BOOL scale_width, scale_height;
		};
#endif
};

#endif // OP_IMAGE_WIDGET_H
