// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.	It may not be distributed
// under any circumstances.

#ifndef HEADERDISPLAY_H
#define HEADERDISPLAY_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/header.h"

#include "modules/display/FontAtt.h"
#include "modules/prefsfile/prefsfile.h"	
#include "modules/util/simset.h"

class HeaderDisplayItem;

class HeaderDisplay
{
public:
	enum DisplayType
	{
		LONG_ADDRESS,
		SHORT_ADDRESS,
		DATE,
		MESSAGE_ID,
		NEWSGROUP,
		XFACE,
		XREF,
		RAW
	};

public:
    HeaderDisplay();
    ~HeaderDisplay();

	OP_STATUS	Init(PrefsFile* prefs_file);
	OP_STATUS	SaveSettings();

	const HeaderDisplayItem* GetItem(UINT index) const;
	const HeaderDisplayItem* FindItem(Header::HeaderType headertype);
	const HeaderDisplayItem* FindItem(const OpStringC8& headername);
	HeaderDisplayItem* GetFirstItem() const { return (HeaderDisplayItem*)(m_headerdisplaylist->First()); }
	
	void	MoveUp(UINT index);
	void	MoveDown(UINT index);

	static	DisplayType GetDefaultDisplayType(Header::HeaderType headertype);
	static	DisplayType GetDefaultDisplayType(const OpStringC8& headername);

	BOOL	GetDisplay(Header::HeaderType headertype);
	BOOL	GetDisplay(const OpStringC8& headername);
	BOOL	GetDisplayHeadername(Header::HeaderType headertype);
	BOOL	GetDisplayHeadername(const OpStringC8& headername);
	BOOL	GetDisplayParsed(Header::HeaderType headertype);
	BOOL	GetDisplayParsed(const OpStringC8& headername);
	BOOL	GetNewLine(Header::HeaderType headertype);
	BOOL	GetNewLine(const OpStringC8& headername);
	BOOL	GetRightAdjusted(Header::HeaderType headertype);
	BOOL	GetRightAdjusted(const OpStringC8& headername);
	OP_STATUS	GetFontAtt(Header::HeaderType headertype, FontAtt& font_att);
	OP_STATUS	GetFontAtt(const OpStringC8& headername, FontAtt& font_att);
	COLORREF	GetFontColor(Header::HeaderType headertype);
	COLORREF	GetFontColor(const OpStringC8& headername);
	OP_STATUS	GetDisplayType(Header::HeaderType headertype, DisplayType &displaytype);
	OP_STATUS	GetDisplayType(const OpStringC8& headername, DisplayType &displaytype);
	
	OP_STATUS	SetDisplay(Header::HeaderType headertype, BOOL display);
	OP_STATUS	SetDisplay(const OpStringC8& headername, BOOL display);
	OP_STATUS	SetDisplayHeadername(Header::HeaderType headertype, BOOL display);
	OP_STATUS	SetDisplayHeadername(const OpStringC8& headername, BOOL display);
	OP_STATUS	SetDisplayParsed(Header::HeaderType headertype, BOOL parsed);
	OP_STATUS	SetDisplayParsed(const OpStringC8& headername, BOOL parsed);
	OP_STATUS	SetNewLine(Header::HeaderType headertype, BOOL new_line);
	OP_STATUS	SetNewLine(const OpStringC8& headername, BOOL new_line);
	OP_STATUS	SetRightAdjusted(Header::HeaderType headertype, BOOL right_adjusted);
	OP_STATUS	SetRightAdjusted(const OpStringC8& headername, BOOL right_adjusted);
	OP_STATUS	SetFontAtt(Header::HeaderType headertype, FontAtt& font_att);
	OP_STATUS	SetFontAtt(const OpStringC8& headername, FontAtt& font_att);
	OP_STATUS	SetFontColor(Header::HeaderType headertype, COLORREF font_color);
	OP_STATUS	SetFontColor(const OpStringC8& headername, COLORREF font_color);
	OP_STATUS	SetDisplayType(Header::HeaderType headertype, DisplayType displaytype);
	OP_STATUS	SetDisplayType(const OpStringC8& headername, DisplayType displaytype);

	BOOL		AddHeader(const OpStringC8& headername) { return CreateItem(headername) ? TRUE : FALSE; }
	
private:
	HeaderDisplayItem* FindLastDisplayOrUnknownHeaderItem();
	HeaderDisplayItem* FindHeaderItem(Header::HeaderType headertype);
	HeaderDisplayItem* FindHeaderItem(const OpStringC8& headername);
	HeaderDisplayItem* CreateItem(Header::HeaderType headertype);
	HeaderDisplayItem* CreateItem(const OpStringC8& headername);

private:
    PrefsFile*	m_prefs_file;
    Head* m_headerdisplaylist;
	HeaderDisplayItem* m_last_accessed_item;
};

class HeaderDisplayItem : public Link
{
public:
	HeaderDisplayItem();
	OP_STATUS Init();
	OP_STATUS ReadSettings(PrefsFile* prefs_file);
	OP_STATUS WriteSettings(PrefsFile* prefs_file);

public:
	Header::HeaderType	m_type;
	OpString8	m_headername;
	UINT		m_default_display:1;
	UINT		m_display:1;
	UINT		m_default_display_headername:1;
	UINT		m_display_headername:1;
	UINT		m_default_display_parsed:1;
	UINT		m_display_parsed:1;
	UINT		m_default_new_line:1;
	UINT		m_new_line:1;
	UINT		m_default_right_adjusted:1;
	UINT		m_right_adjusted:1;
	FontAtt		m_default_font_att;
	FontAtt		m_font_att;
	COLORREF	m_default_font_color;
	COLORREF	m_font_color;
	HeaderDisplay::DisplayType	m_default_display_type;
	HeaderDisplay::DisplayType	m_display_type;
};


#endif //M2_SUPPORT

#endif // HEADERDISPLAY_H
