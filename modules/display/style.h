/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _STYLE_
#define _STYLE_

#include "modules/display/FontAtt.h"
#include "modules/windowcommander/WritingSystem.h"

/** Layout attributes for Style objects. */
struct LayoutAttr {
    int LeftHandOffset;
    int RightHandOffset;
    int LeadingOffset;
    int TrailingOffset;
    int LeadingSeparation;
    int TrailingSeparation;
    LayoutAttr();
    LayoutAttr(int, int, int, int, int, int);
};

/** Presentation attributes for Style objects. */
struct PresentationAttr {
	struct PresentationFont {
		PresentationFont() : Font(0), FontNumber(-1) {}
		OP_STATUS Set(const FontAtt* fa);
		FontAtt*	Font;
		short		FontNumber;
	};
	PresentationFont font_attr[WritingSystem::NumScripts];

	BOOL		Italic;
	BOOL		Bold;
	BOOL		InheritFontSize;
	long		Color;
	BOOL		Underline;
	BOOL		StrikeOut;
	PresentationAttr();
	PresentationAttr(BOOL, BOOL, long, BOOL inherit_fs = FALSE, BOOL uline = FALSE, BOOL strike = FALSE);

	PresentationAttr& operator =(const PresentationAttr& p)
	{
		Italic = p.Italic;
		Bold = p.Bold;
		InheritFontSize = p.InheritFontSize;
		Color = p.Color;
		Underline = p.Underline;
		StrikeOut = p.StrikeOut;
		for (int i = 0; i < WritingSystem::NumScripts; ++i)
			font_attr[i] = p.font_attr[i];
		return *this;
	}

/**
 * Second phase constructor. You must call this method prior to using the 
 * PresentationAttr struct.
 *
 * @return OP_STATUS Status of the construction, always check this.
 */
	OP_STATUS Construct(const FontAtt*);	
	OP_STATUS   SetFontsFromDefaultScript();
	const PresentationAttr::PresentationFont& GetPresentationFont(const WritingSystem::Script s) const { return font_attr[s]; }
};

class Style {

  private:
    LayoutAttr			layout_attr;
    PresentationAttr	pres_attr;

  public:

    Style() {};
    Style(const LayoutAttr& lattr, const PresentationAttr& pattr);
    Style(int lho, int rho, int lo, int to, int ls, int ts, BOOL i, BOOL b, long col, BOOL inherit_fs = FALSE, BOOL uline = FALSE, BOOL strike = FALSE);
     ~Style();

/**
* Second phase constructor. You must call this method prior to using the 
* Style object, unless it was created using the Create() method.
*
* @return OP_STATUS Status of the construction, always check this.
*/

	OP_STATUS Construct(const FontAtt* lf);

/**
* OOM safe creation of a Style object.
*
* @return Style* The created object if successful and NULL otherwise.
*/

	static Style* Create(int lho, int rho, int lo, int to, int ls, int ts, BOOL i, BOOL b, const FontAtt* lf, long col, BOOL inherit_fs = FALSE, BOOL uline = FALSE, BOOL strike = FALSE);

    const LayoutAttr&
				GetLayoutAttr() const { return layout_attr; }
    const PresentationAttr&
				GetPresentationAttr() const { return pres_attr; }

    void        SetLayoutAttr(const LayoutAttr& lattr);
    OP_STATUS   SetPresentationAttr(const PresentationAttr& pattr);
	OP_STATUS   SetFont(const FontAtt* att, const WritingSystem::Script script);
};

#endif /* _STYLE_ */
