/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include <stdint.h>
#include <gtk/gtk.h>

/** @brief Utility functions when using GTK
  */
namespace GtkUtils
{
	/** Runs the main loop until nothing is left on the loop
	  */
	void ProcessEvents();

	/** A very simple string class
	  */
	class String
	{
	public:
		explicit String(const char* data = 0) : m_data(CopyString(data)) {}
		explicit String(const String& data) : m_data(CopyString(data.Get())) {}
		~String() { delete[] m_data; }
		String& operator=(const String& data) { return *this = data.Get(); }
		String& operator=(const char* data);

		const char* Get() const { return m_data; }

	private:
		static char* CopyString(const char* data);
		char* m_data;
	};

	/** Converts COLORREF to GdkColor
	  * e.g. 0xA0B0C0 is converted to #A0A0B0B0C0C0
	  */
	GdkColor ColorrefToGdkColor(const uint32_t color);

	/** Converts GdkColor to COLORREF
	  */
	uint32_t GdkColorToColorref(const GdkColor *gcolor);

	/** Set widget resource name as reported by WM_CLASS property
	 *
	 * @param widget Should be a toplevel widget
	 * @param name Use lower case and no spacing (example "colorselectordialog")
	 */
	void SetResourceName(GtkWidget* widget, const char* name);
};
