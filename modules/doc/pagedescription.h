/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PAGEDESCRIPTION_H
#define PAGEDESCRIPTION_H

#ifdef PAGED_MEDIA_SUPPORT

//  -------- sheet --------
// |                       |
// |   --- page box ----   |
// |  |                 |  |
// |  |   -----------   |  |
// |  |  |           |  |  |
// |  |  |           |  |  |
// |  |  | page area |  |  |
// |  |  |           |  |  |
// |  |  |           |  |  |
// |  |   -----------   |  |
// |  |                 |  |
// |  |   margin area   |  |
// |  |                 |  |
// |   -----------------   |
// |                       |
//  -----------------------

class PageDescription
  : public Link
{
private:

	/** Position of the page. */

	long			y;

	/** Name of the page */

	uni_char*		name;

	/** Width of the sheet */

	short			sheet_width;

	/** Height of the sheet */

	long			sheet_height;

	/** Width of the page area */

	short			page_area_width;

	/** Height of the page area */

	long			page_area_height;

	/** Left position of the page box (relative to the sheet) */

	short			page_box_left;

	/** Top position of the page box (relative to the sheet) */

	short			page_box_top;

	/** Right position of the page box (relative to the sheet) */

	short			page_box_right;

	/** Bottom position of the page box (relative to the sheet) */

	short			page_box_bottom;

	/** Left margin */

	short			margin_left;

	/** Right margin */

	short			margin_right;

	/** Top margin */

	short			margin_top;

	/** Bottom margin */

	short			margin_bottom;

public:

					PageDescription(long y)
					  : y(y), name(NULL) {}

	long			GetPageTop() const { return y; }
	long			GetPageBottom() const { return y + page_area_height; }
	short			GetPageWidth() const { return page_area_width; }
	long			GetPageHeight() const { return page_area_height; }
	short			GetSheetWidth() const { return sheet_width; }
	long			GetSheetHeight() const { return sheet_height; }
	short			GetPageBoxTop() const { return page_box_top; }
	short			GetPageBoxLeft() const { return page_box_left; }
	short			GetPageBoxRight() const { return page_box_right; }
	short			GetPageBoxBottom() const { return page_box_bottom; }
	short			GetMarginTop() const { return margin_top; }
	short			GetMarginBottom() const { return margin_bottom; }
	short			GetMarginLeft() const { return margin_left; }
	short			GetMarginRight() const {return margin_right; }
	const uni_char*	GetName() const { return name; }

	int				GetNumber();

	void			SetMargins(short left, short right, short top, short bottom)
						{ margin_left = left; margin_right = right; margin_top = top; margin_bottom = bottom; }
	void			SetPageBoxOffset(short left, short top, short right, short bottom)
						{ page_box_left = left; page_box_top = top; page_box_right = right; page_box_bottom = bottom;}
	void			SetPageArea(short width, long height)
						{ page_area_width = width; page_area_height = height; }
	void			SetSheet(short width, long height)
						{ OP_ASSERT(height > 0 || !"We will hang since adding a paper doesn't add any height");
							sheet_width = width; sheet_height = height; }

#ifdef _PRINT_SUPPORT_
	OP_DOC_STATUS	PrintPage(VisualDevice* vd, HTML_Document* doc, const RECT& rect);
#endif
};

#endif // PAGED_MEDIA_SUPPORT
#endif // !PAGEDESCRIPTION_H
