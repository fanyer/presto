/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef SELECT_FONT_DIALOG_H
#define SELECT_FONT_DIALOG_H

#include "adjunct/desktop_pi/DesktopColorChooser.h"

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class DesktopWindow;
class FontAtt;
class OpButton;
class OpDropDown;
class OpFontChooserListener;
class OpEdit;
class OpListBox;


class SelectFontDialog : public Dialog, public OpColorChooserListener
{
public:
	SelectFontDialog(FontAtt &initial_font, COLORREF initial_color,
					 BOOL fontname_only, INT32 font_id, OpFontChooserListener *listener);
	~SelectFontDialog();

	virtual BOOL GetIsBlocking() { return TRUE; }

	Type GetType() { return DIALOG_TYPE_SELECT_FONT; }
	const char *GetWindowName() { return "Select Font Dialog"; }
	const char *GetHelpAnchor() { return "fonts.html"; }

	void OnInit();
	UINT32 OnOk();

	void OnClick(OpWidget *widget, UINT32 id=0);
	void OnChange(OpWidget *widget, BOOL changed_by_mouse=FALSE);

	BOOL OnInputAction(OpInputAction *action);

	void OnColorSelected(COLORREF color);

	virtual void UpdateSizes();

	void UpdateSample();

private:
	int FindNearestSizeIndex(int pixelsize);
	int FindFontIndex(const uni_char *fontname);
	const uni_char *GetCurrentFont();
	BOOL CanChangeColor();
	BOOL HasDefaultFont();
	BOOL IsDefaultFontSelected();
	BOOL GetDefaultFont(FontAtt &font);

protected:
	
	virtual int GetCurrentPixelSize();

	OpListBox *mSizeListBox;

	struct SizeUserData
	{
		int pixelsize;
	};

private:
	struct FontUserData
	{
		OpString name;
		bool default_entry;
	};

	FontAtt &mInitialFont;
	OpFontChooserListener *mFcListener;
	OpListBox *mFontListBox;
	OpDropDown *mWeightDropDown;
	OpButton *mColorButton;
	OpCheckBox *mItalicCheckBox;
	OpCheckBox *mUnderlineCheckBox;
	OpCheckBox *mOverlineCheckBox;
	OpCheckBox *mStrikeoutCheckBox;
	OpEdit *mSampleEdit;
	COLORREF mInitialColor;
	BOOL mFontNameOnly;
	int mCurFontIx;
	int mCurWeightIx;
	int mCurSizeIx;
	int mLastPixelSizeSelected;
	int mWeight;
	int mFontId;
};


class SelectCSSFontDialog : public SelectFontDialog
{
public:
	SelectCSSFontDialog(FontAtt &initial_font, OpFontChooserListener *listener)
		:SelectFontDialog(initial_font,0,TRUE,-1,listener) {}
	
	Type GetType() { return DIALOG_TYPE_SELECT_FONT; }
	const char *GetWindowName() { return "Select CSS Font Dialog"; }
};

/** Tweaked Font Dialog to show font sizes from 1-7 (Smallest - Biggest), since that is what DocumentEditable wants
  */

class SelectMailComposeFontDialog : public SelectFontDialog
{
public:
	
	SelectMailComposeFontDialog(FontAtt &initial_font, OpFontChooserListener *listener)
		:SelectFontDialog(initial_font,0,FALSE,-1,listener) {}

	virtual void	UpdateSizes();
	virtual int		GetCurrentPixelSize();
	virtual BOOL	GetIsBlocking() { return FALSE; }
};

#endif // SELECT_FONT_DIALOG_H
