/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPWIDGETBITMAP_H
#define OPWIDGETBITMAP_H

#include "modules/img/image.h"

/***********************************************************************************
**
**	OpWidgetBitmap
**
***********************************************************************************/

class OpWidgetBitmap
{
	public:

		OpWidgetBitmap(OpBitmap* bitmap);
		OpWidgetBitmap(Image& image) : m_image(image), m_refcounter(0) {}
		~OpWidgetBitmap();

		void AddRef() { m_refcounter++; }
		void Release();

		Image& GetImage() { return m_image; }

	private:

		Image		m_image;
		INT32		m_refcounter;
};

#endif // OPWIDGETBITMAP_H
