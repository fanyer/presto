/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"

//#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "SelectFontDialog.h"

#include "modules/pi/OpFont.h"
#include "modules/pi/OpScreenInfo.h"

#include "adjunct/desktop_pi/DesktopColorChooser.h"
#include "adjunct/desktop_pi/DesktopFontChooser.h"

#include "modules/display/styl_man.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpListBox.h"

#include <limits.h>
#include <stdlib.h>


SelectFontDialog::SelectFontDialog(FontAtt &initial_font, COLORREF initial_color, BOOL fontname_only,INT32 font_id, OpFontChooserListener *listener )
	: mSizeListBox(0),
	  mInitialFont(initial_font),
	  mFcListener(listener),
	  mFontListBox(0),
	  mWeightDropDown(0),
	  mColorButton(0),
	  mItalicCheckBox(0),
	  mUnderlineCheckBox(0),
	  mOverlineCheckBox(0),
	  mStrikeoutCheckBox(0),
	  mSampleEdit(0),
	  mInitialColor(initial_color),
	  mFontNameOnly(fontname_only),
	  mCurFontIx(-1),
	  mCurWeightIx(-1),
	  mCurSizeIx(-1),
	  mLastPixelSizeSelected(-1),
	  mWeight(-1),
	  mFontId(font_id)
{
}

SelectFontDialog::~SelectFontDialog()
{
	if (mFontListBox)
	{
		int num_items = mFontListBox->CountItems();
		for (int i=0; i<num_items; i++)
		{
			FontUserData *d = (FontUserData *)mFontListBox->GetItemUserData(i);
			OP_DELETE(d);
		}
	}
	if (mSizeListBox)
	{
		int num_items = mSizeListBox->CountItems();
		for (int i = 0; i < num_items; i++)
		{
			SizeUserData *d = (SizeUserData *)mSizeListBox->GetItemUserData(i);
			OP_DELETE(d);
		}
	}
}

void SelectFontDialog::OnInit()
{
	if( mFontNameOnly )
	{
		mFontListBox = (OpListBox *)GetWidgetByName("Font_listbox");
		mSampleEdit  = (OpEdit *)GetWidgetByName("Sample_edit");

		if( !mFontListBox || !mSampleEdit )
		{
			return;
		}

		mLastPixelSizeSelected = 17;
	}
	else
	{
		mFontListBox = (OpListBox *)GetWidgetByName("Font_listbox");
		mSizeListBox = (OpListBox *)GetWidgetByName("Size_listbox");
		mWeightDropDown = (OpDropDown *)GetWidgetByName("Weight_dropdown");
		mColorButton = (OpButton *)GetWidgetByName("Color_button");
		mItalicCheckBox = (OpCheckBox *)GetWidgetByName("Italic_checkbox");
		mUnderlineCheckBox = (OpCheckBox *)GetWidgetByName("Underline_checkbox");
		mOverlineCheckBox = (OpCheckBox *)GetWidgetByName("Overline_checkbox");
		mStrikeoutCheckBox = (OpCheckBox *)GetWidgetByName("Strikeout_checkbox");
		mSampleEdit  = (OpEdit *)GetWidgetByName("Sample_edit");

		if( !mFontListBox || !mSizeListBox || !mWeightDropDown || !mColorButton ||
			!mItalicCheckBox || !mUnderlineCheckBox || !mOverlineCheckBox ||
			!mStrikeoutCheckBox || !mSampleEdit )
		{
			return;
		}

		mLastPixelSizeSelected = mInitialFont.GetHeight();
		mColorButton->SetBackgroundColor(mInitialColor);

		OpString loc_str;
		g_languageManager->GetString(Str::S_FONT_LIGHT, loc_str);
		mWeightDropDown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::S_FONT_NORMAL, loc_str);
		mWeightDropDown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::S_FONT_BOLD, loc_str);
		mWeightDropDown->AddItem(loc_str.CStr());

		mWeight = mInitialFont.GetWeight();
		int index = 1;
		if (mWeight < 4)
			index = 0;
		else if (mWeight > 5)
			index = 2;
		mWeightDropDown->SelectItem(index, TRUE);

		mItalicCheckBox->SetValue(mInitialFont.GetItalic());
		mUnderlineCheckBox->SetValue(mInitialFont.GetUnderline());
		mOverlineCheckBox->SetValue(mInitialFont.GetOverline());
		mStrikeoutCheckBox->SetValue(mInitialFont.GetStrikeOut());
	}

	if( mFontListBox )
		mFontListBox->SetListener(this);
	if( mWeightDropDown)
		mWeightDropDown->SetListener(this);
	if( mSizeListBox )
		mSizeListBox->SetListener(this);
	if( mItalicCheckBox )
		mItalicCheckBox->SetListener(this);
	if( mUnderlineCheckBox )
		mUnderlineCheckBox->SetListener(this);
	if( mOverlineCheckBox )
		mOverlineCheckBox->SetListener(this);
	if( mStrikeoutCheckBox)
		mStrikeoutCheckBox->SetListener(this);

	mSampleEdit->SetText(UNI_L("AaBbCc\xC5\xE5"));


	int numFonts = styleManager->GetFontManager()->CountFonts();
	for (int i=0; i<numFonts; i++)
	{
		const OpFontInfo *item = styleManager->GetFontInfo(i);
		if (item)
		{
			const uni_char *name = item->GetFace();
			FontUserData *userdata = OP_NEW(FontUserData, ());
			if (userdata)
			{
				userdata->name.Set(name);
				userdata->default_entry = false;
				int num_items = mFontListBox->CountItems();
				int index;
				for (index=0; index<num_items; index++)
				{
					FontUserData *d = (FontUserData *)mFontListBox->GetItemUserData(index);
					if (d && d->name.Compare(name) > 0)
						break;
				}

				if (index == num_items)
					index = -1; // At end

				mFontListBox->AddItem(name, index, 0, reinterpret_cast<INTPTR>(userdata));
			}
		}
	}


	const uni_char *name = mInitialFont.GetFaceName();

	int index = -1;

	if( HasDefaultFont() )
	{
		FontAtt default_font;
		if( GetDefaultFont(default_font) )
		{
			OpString name;
			g_languageManager->GetString(Str::S_DEFAULT_FONT_TEXT,name);
			if( name.CStr() && default_font.GetFaceName() )
			{
				name.AppendFormat(UNI_L(" - %s"), default_font.GetFaceName());
				FontUserData *userdata = OP_NEW(FontUserData, ());
				if (userdata)
				{
					userdata->name.Set(default_font.GetFaceName());
					userdata->default_entry = true;
					mFontListBox->AddItem(name.CStr(), 0, 0, reinterpret_cast<INTPTR>(userdata));
				}
			}
			if (mInitialFont == default_font)
				index = 0;
		}
	}

	if (index == -1 && name)
	{
		index = FindFontIndex(name);
		if (index == -1)
		{
			OpFontManager *fontman = styleManager->GetFontManager();
			name = fontman->GetGenericFontName(GENERIC_FONT_SANSSERIF);
			if (name)
			{
				index = FindFontIndex(name);
			}
		}
	}

	if (index == -1)
		index = 0;

	mFontListBox->SelectItem(index, TRUE);
	OnChange(mFontListBox);
}


UINT32 SelectFontDialog::OnOk()
{
	if (!mFcListener)
		return 0;

	if( IsDefaultFontSelected() )
	{
		FontAtt fontatt;
		GetDefaultFont(fontatt);
		mFcListener->OnFontSelected(fontatt, mInitialColor);
		return 0;
	}


	FontAtt fontatt;
	fontatt.SetFaceName(mInitialFont.GetFaceName());
	fontatt.SetHeight(mInitialFont.GetHeight());
	fontatt.SetWeight(mInitialFont.GetWeight());
	fontatt.SetItalic(mInitialFont.GetItalic());
	fontatt.SetUnderline(mInitialFont.GetUnderline());
	fontatt.SetOverline(mInitialFont.GetOverline());
	fontatt.SetStrikeOut(mInitialFont.GetStrikeOut());
	COLORREF color = mInitialColor;

	const uni_char *face = GetCurrentFont();
	if (face)
	{
		fontatt.SetFaceName(face);
	}
	if (!mFontNameOnly)
	{
		int pixelsize = GetCurrentPixelSize();
		if (pixelsize != -1)
		{
			fontatt.SetHeight(pixelsize);
		}
		fontatt.SetWeight(mWeight);

		if (mItalicCheckBox)
			fontatt.SetItalic(mItalicCheckBox->GetValue());
		if (mUnderlineCheckBox)
			fontatt.SetUnderline(mUnderlineCheckBox->GetValue());
		if (mOverlineCheckBox)
			fontatt.SetOverline(mOverlineCheckBox->GetValue());
		if (mStrikeoutCheckBox)
			fontatt.SetStrikeOut(mStrikeoutCheckBox->GetValue());

		color = mColorButton->GetBackgroundColor(mInitialColor);
	}

	mFcListener->OnFontSelected(fontatt, color);

	return 0;
}

void SelectFontDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget == mItalicCheckBox || widget == mUnderlineCheckBox ||
		widget == mOverlineCheckBox || widget == mStrikeoutCheckBox)
	{
		UpdateSample();
	}
	else if (widget != mFontListBox && widget != mSizeListBox &&
			 widget != mWeightDropDown && widget != mSampleEdit)
	{
		Dialog::OnClick(widget, id);
	}
}

void SelectFontDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == mFontListBox)
	{
		int selected = mFontListBox->GetSelectedItem();
		if (selected >= 0 && selected != mCurFontIx)
		{
			BOOL enable = !IsDefaultFontSelected();

			if( mWeightDropDown)
				mWeightDropDown->SetEnabled(enable);
			SetWidgetEnabled( "label_for_Weight_dropdown", enable );
			if( mSizeListBox )
				mSizeListBox->SetEnabled(enable);
			SetWidgetEnabled( "label_for_Size_listbox", enable );
			if( mItalicCheckBox )
				mItalicCheckBox->SetEnabled(enable);
			if( mUnderlineCheckBox )
				mUnderlineCheckBox->SetEnabled(enable);
			if( mOverlineCheckBox )
				mOverlineCheckBox->SetEnabled(enable);
			if( mStrikeoutCheckBox)
				mStrikeoutCheckBox->SetEnabled(enable);
			if( mColorButton)
				mColorButton->SetEnabled(enable && CanChangeColor() );
			SetWidgetEnabled( "label_for_Color_button", enable && CanChangeColor() );
			mCurFontIx = selected;
			UpdateSizes();
		}
	}
	else if (widget == mSizeListBox)
	{
		int selected = mSizeListBox->GetSelectedItem();
		if (selected >= 0 && selected != mCurSizeIx)
		{
			mCurSizeIx = selected;
			SizeUserData *size_ud = (SizeUserData *)mSizeListBox->GetItemUserData(mCurSizeIx);
			OP_ASSERT(size_ud);
			if (size_ud)
			{
				mLastPixelSizeSelected = size_ud->pixelsize;
			}
		}
	}
	else if (widget == mWeightDropDown)
	{
		int selected = mWeightDropDown->GetSelectedItem();
		switch (selected)
		{
		case 0:
			mWeight = 2;
			break;
		case 1:
			mWeight = 4;
			break;
		case 2:
			mWeight = 7;
			break;
		default:
			OP_ASSERT(FALSE);
		}
	}
	else if (widget != mItalicCheckBox && widget != mUnderlineCheckBox &&
			 widget != mOverlineCheckBox && widget != mStrikeoutCheckBox &&
			 widget != mSampleEdit)
	{
		Dialog::OnChange(widget, changed_by_mouse);
	}

	if (widget != mSampleEdit)
		UpdateSample();
}

BOOL SelectFontDialog::OnInputAction(OpInputAction *action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_CHOOSE_FONT_COLOR:
				{
					BOOL enable = !IsDefaultFontSelected() && CanChangeColor();
					SetWidgetEnabled( "label_for_Color_button", enable );
					child_action->SetEnabled( enable );
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_CHOOSE_FONT_COLOR:
		{
			OpColorChooser *chooser = 0;
			COLORREF color = mColorButton->GetBackgroundColor(mInitialColor);

			if (OpStatus::IsSuccess(OpColorChooser::Create(&chooser)))
			{
				chooser->Show(color, this, this);
				OP_DELETE(chooser);
			}

			break;
		}
	}

	return Dialog::OnInputAction(action);
}

void SelectFontDialog::OnColorSelected(COLORREF color)
{
	UINT32 new_color = OP_RGB(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color));
	mColorButton->SetBackgroundColor(new_color);
}

void SelectFontDialog::UpdateSizes()
{
	if (!mFontListBox || !mSizeListBox || mCurFontIx == -1)
		return;

	mCurSizeIx = -1;

	FontUserData *userdata = (FontUserData *)mFontListBox->GetItemUserData(mCurFontIx);
	OP_ASSERT(userdata);
	if (!userdata)
		return;

	int num_items = mSizeListBox->CountItems();
	for (INT32 i = 0; i < num_items; i++)
	{
		SizeUserData *d = (SizeUserData *)mSizeListBox->GetItemUserData(i);
		OP_DELETE(d);
	}
	mSizeListBox->RemoveAll();

#ifdef X11FONT
	int *sizes = ((X11OpFontManager *)styleManager->GetFontManager())->GetPixelSizes(userdata->name.CStr());

	if (!sizes)
		return;

	for (int i=0; sizes[i]; i++)
	{
		OpString sizestr;
		sizestr.AppendFormat(UNI_L("%d"), sizes[i]);
		SizeUserData *d = OP_NEW(SizeUserData, ());
		if (d)
		{
			d->pixelsize = sizes[i];
			mSizeListBox->AddItem(sizestr.CStr(), -1, 0, d);
		}
	}
	OP_DELETEA(sizes);
#endif // X11FONT

	if (mSizeListBox->CountItems() == 0)
	{
		/* True-type fonts (and others) don't specify which sizes they support,
		   since they support any size. Therefore, let's build a list of some
		   common sizes for the user to choose from. */
#ifdef X11FONT
		static const int pixelsizes[] = {
			5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
			19, 20, 21, 22, 24, 26, 28, 32, 36, 48, 60, 72, 84, 96 };
		int count = sizeof(pixelsizes) / sizeof(int);
		for (int i=0; i<count; i++)
		{
			OpString sizestr;
			sizestr.AppendFormat(UNI_L("%d"), pixelsizes[i]);
			SizeUserData *d = OP_NEW(SizeUserData, ());
			if (d)
			{
				d->pixelsize = pixelsizes[i];
				mSizeListBox->AddItem(sizestr.CStr(), -1, 0, d);
			}
		}
#else
		static const int pointsizes[] = { 8, 9, 10, 11, 12, 14, 16, 18,
										  20, 22, 24, 26, 28, 36, 48, 72 };
		int count = sizeof(pointsizes) / sizeof(int);
		int lastsize = -1;
		UINT32 dummy, dpi;
		g_op_screen_info->GetDPI(&dummy, &dpi, GetOpWindow());
		OpAutoVector<OpString> names;
		OpVector<void> user_datas;

		for (int i=0; i<count; i++)
		{
			int pixelsize = pointsizes[i] * dpi / 72;
			if (pixelsize != lastsize)
			{
				OpString *sizestr = OP_NEW(OpString, ());
				if (sizestr)
				{
					sizestr->AppendFormat(UNI_L("%d"), pixelsize);
					SizeUserData *d = OP_NEW(SizeUserData, ());
					if (d)
					{
						d->pixelsize = pixelsize;
						names.Add(sizestr);
						user_datas.Add(d);
					}
				}
				lastsize = pixelsize;
			}
		}

		mSizeListBox->AddItems(&names, NULL, &user_datas);
#endif // X11FONT
	}

	if( IsDefaultFontSelected() )
	{
		FontAtt fontAtt;
		GetDefaultFont(fontAtt);
		mLastPixelSizeSelected = fontAtt.GetHeight();
	}

	if (mLastPixelSizeSelected != -1)
	{
		mCurSizeIx = FindNearestSizeIndex(mLastPixelSizeSelected);
	}
	else
	{
		mCurSizeIx = 0;
	}
	mSizeListBox->SelectItem(mCurSizeIx, TRUE);
	UpdateSample();
}

void SelectFontDialog::UpdateSample()
{
	if( !mSampleEdit || !mFontListBox || mCurFontIx < 0 )
	{
		return;
	}

	FontUserData *font_ud = (FontUserData *)mFontListBox->GetItemUserData(mCurFontIx);
	OP_ASSERT(font_ud);
	if (!font_ud)
		return;
	int fontnum = styleManager->GetFontNumber(font_ud->name.CStr());
	OP_ASSERT(fontnum != -1);
	if (fontnum == -1)
		return;
	const OpFontInfo *fontinfo = styleManager->GetFontInfo(fontnum);
	OP_ASSERT(fontinfo);
	if (!fontinfo)
		return;

	int pixel_size = 10;
	if( mFontNameOnly )
	{
		pixel_size = mLastPixelSizeSelected;
	}
	else
	{
		if( !mSizeListBox || mCurSizeIx < 0 )
		{
			return;
		}

		SizeUserData *size_ud = (SizeUserData *)mSizeListBox->GetItemUserData(mCurSizeIx);
		OP_ASSERT(size_ud);
		if (!size_ud)
			return;

		pixel_size = size_ud->pixelsize;
	}


	BOOL italic = FALSE;
	if( mItalicCheckBox )
	{
		italic = mItalicCheckBox->GetValue();
	}

	mSampleEdit->SetFontInfo(fontinfo, pixel_size, italic, mWeight, JUSTIFY_LEFT);
}

int SelectFontDialog::FindNearestSizeIndex(int pixelsize)
{
	int index = 0;

	if (pixelsize != -1)
	{
		int closest_diff = INT_MAX;
		int count = mSizeListBox->CountItems();
		for (int i=0; i<count; i++)
		{
			SizeUserData *size_ud = (SizeUserData *)mSizeListBox->GetItemUserData(i);
			OP_ASSERT(size_ud);
			if (size_ud)
			{
				int this_diff = abs(size_ud->pixelsize - pixelsize);
				if (this_diff < closest_diff || (this_diff == closest_diff &&
					size_ud->pixelsize > pixelsize))
				{
					index = i;
					closest_diff = this_diff;
				}
			}
		}
	}

	return index;
}

int SelectFontDialog::FindFontIndex(const uni_char *fontname)
{
	int num_items = mFontListBox->CountItems();
	for (int index=0; index<num_items; index++)
	{
		FontUserData *d = (FontUserData *)mFontListBox->GetItemUserData(index);
		if (d && !d->default_entry && uni_strcmp(d->name.CStr(),
												 fontname) == 0)
			return index;
	}

	return -1;
}

const uni_char *SelectFontDialog::GetCurrentFont()
{
	FontUserData *userdata = (FontUserData *)mFontListBox->GetItemUserData(mCurFontIx);
	OP_ASSERT(userdata);
	if (!userdata)
		return 0;

	return userdata->name.CStr();
}

int SelectFontDialog::GetCurrentPixelSize()
{
	SizeUserData *userdata = (SizeUserData *)mSizeListBox->GetItemUserData(mCurSizeIx);
	OP_ASSERT(userdata);
	if (!userdata)
		return 0;

	return userdata->pixelsize;
}


BOOL SelectFontDialog::CanChangeColor()
{
	return !HasDefaultFont();
}


BOOL SelectFontDialog::HasDefaultFont()
{
	if( mFontId == OP_SYSTEM_FONT_EMAIL_COMPOSE ||
		mFontId == OP_SYSTEM_FONT_EMAIL_DISPLAY ||
		mFontId == OP_SYSTEM_FONT_UI_MENU ||
		mFontId == OP_SYSTEM_FONT_UI_TOOLBAR ||
		mFontId == OP_SYSTEM_FONT_UI_DIALOG ||
		mFontId == OP_SYSTEM_FONT_UI_PANEL ||
		mFontId == OP_SYSTEM_FONT_UI_TOOLTIP ||
		mFontId == OP_SYSTEM_FONT_EMAIL_COMPOSE)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


BOOL SelectFontDialog::IsDefaultFontSelected()
{
	if( mFontListBox )
	{
		int selected = mFontListBox->GetSelectedItem();
		return selected == 0 && HasDefaultFont();
	}
	else
	{
		return FALSE;
	}
}


BOOL SelectFontDialog::GetDefaultFont(FontAtt &font)
{
	if( HasDefaultFont() )
	{
		if( mFontId == OP_SYSTEM_FONT_EMAIL_COMPOSE ||
			mFontId == OP_SYSTEM_FONT_EMAIL_DISPLAY ||
			mFontId == OP_SYSTEM_FONT_UI_MENU ||
			mFontId == OP_SYSTEM_FONT_UI_TOOLBAR ||
			mFontId == OP_SYSTEM_FONT_UI_DIALOG ||
			mFontId == OP_SYSTEM_FONT_UI_PANEL ||
			mFontId == OP_SYSTEM_FONT_UI_TOOLTIP ||
			mFontId == OP_SYSTEM_FONT_EMAIL_COMPOSE)
		{
			g_op_ui_info->GetFont((OP_SYSTEM_FONT)mFontId, font, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

void SelectMailComposeFontDialog::UpdateSizes()
{
	if (!mSizeListBox)
		return;

	if (mSizeListBox->CountItems() > 0)
		return UpdateSample();

		static const int pixelsizes[] = { 5, 9, 11, 13, 20, 48, 72};


	int count = sizeof(pixelsizes) / sizeof(int);
	for (int i=0; i<count; i++)
	{
		SizeUserData *d = OP_NEW(SizeUserData, ());
		if (d)
		{
			OpString sizestr;
			switch (i)
			{
			case 0:
				g_languageManager->GetString(Str::S_SMALLEST_FONT_SIZE,sizestr);
				break;
			case 1:
				g_languageManager->GetString(Str::S_SMALLER_FONT_SIZE,sizestr);
				break;
			case 2:
				g_languageManager->GetString(Str::S_SMALL_FONT_SIZE,sizestr);
				break;
			case 3:
				g_languageManager->GetString(Str::S_MEDIUM_FONT_SIZE,sizestr);
				break;
			case 4:
				g_languageManager->GetString(Str::S_BIG_FONT_SIZE,sizestr);
				break;
			case 5:
				g_languageManager->GetString(Str::S_BIGGER_FONT_SIZE,sizestr);
				break;
			case 6:
				g_languageManager->GetString(Str::S_BIGGEST_FONT_SIZE,sizestr);
				break;
			}

			d->pixelsize = pixelsizes[i];
			mSizeListBox->AddItem(sizestr.CStr(), -1, 0, reinterpret_cast<INTPTR>(d));
		}
	}
}

int  SelectMailComposeFontDialog::GetCurrentPixelSize()
{

	if (mSizeListBox->GetSelectedItem()>=0)
	{
		SizeUserData *userdata = (SizeUserData *)mSizeListBox->GetItemUserData(mSizeListBox->GetSelectedItem());

		if (userdata)
		{
			switch (userdata->pixelsize)
			{
				case 5: return 1;
				case 9: return 2;
				case 11: return 3;
				case 13: return 4;
				case 20: return 5;
				case 48: return 6;
				case 72: return 7;
				default: break;

			}
		}
	}
	return 3;
}
