/* Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "headerdisplay.h"

HeaderDisplayItem::HeaderDisplayItem()
: Link(),
  m_type(Header::UNKNOWN),
  m_default_display(FALSE),
  m_display(FALSE),
  m_default_display_headername(FALSE),
  m_display_headername(FALSE),
  m_default_display_parsed(FALSE),
  m_display_parsed(FALSE),
  m_default_new_line(FALSE),
  m_new_line(FALSE),
  m_default_right_adjusted(FALSE),
  m_right_adjusted(FALSE),
  m_default_font_color(0),
  m_font_color(0)
{
}

OP_STATUS HeaderDisplayItem::Init()
{
	m_default_font_att.SetWeight(4);
	m_default_font_att.SetUnderline(FALSE);
	m_default_font_att.SetOverline(FALSE);
	m_default_font_att.SetHeight(10);
	m_default_font_att.SetStrikeOut(FALSE);
	m_default_font_att.SetSmallCaps(FALSE);
	m_default_font_att.SetItalic(FALSE);
	m_default_font_att.SetHasGetOutline(FALSE);
	m_default_font_att.SetFontNumber(0);
//	m_default_font_att.SetFaceName(const uni_char* faceName);
	m_font_att=m_default_font_att;

	m_default_font_color=m_font_color=OP_RGB(0,0,0);

	switch(m_type)
	{
	case Header::ORGANIZATION:
		{
			m_default_display=m_display=TRUE;
			m_default_display_headername=m_display_headername=FALSE;
			m_default_display_parsed=m_display_parsed=TRUE;
			m_default_new_line=m_new_line=FALSE;
			m_default_right_adjusted=m_right_adjusted=FALSE;
		}
		break;

	case Header::BCC:
	case Header::CC:
		{
			m_default_display=m_display=TRUE;
			m_default_display_headername=m_display_headername=TRUE;
			m_default_display_parsed=m_display_parsed=TRUE;
			m_default_new_line=m_new_line=TRUE;
			m_default_right_adjusted=m_right_adjusted=FALSE;
		}
		break;

	case Header::SUBJECT:
		{
			m_default_display=m_display=TRUE;
			m_default_display_headername=m_display_headername=FALSE;
			m_default_display_parsed=m_display_parsed=FALSE;
			m_default_new_line=m_new_line=TRUE;
			m_default_right_adjusted=m_right_adjusted=FALSE;
			m_default_font_att.SetHeight(16);
			m_font_att=m_default_font_att;
		}
		break;

    case Header::APPROVED:
	case Header::COMPLAINTSTO:
	case Header::MAILCOPIESTO:
	case Header::RESENTSENDER:
	case Header::SENDER:
	case Header::ARTICLENAMES:
	case Header::ARTICLEUPDATES:
	case Header::INREPLYTO:
	case Header::MESSAGEID:
	case Header::RESENTMESSAGEID:
	case Header::REFERENCES:
	case Header::SEEALSO:
	case Header::SUPERSEDES:
	case Header::EXPIRES:
	case Header::XFACE:
	case Header::XREF:
		{
			m_default_display=m_display=FALSE;
			m_default_display_headername=m_display_headername=TRUE;
			m_default_display_parsed=m_display_parsed=TRUE;
			m_default_new_line=m_new_line=TRUE;
			m_default_right_adjusted=m_right_adjusted=FALSE;
		}
		break;

	case Header::FROM:
	case Header::REPLYTO:
	case Header::RESENTBCC:
	case Header::RESENTCC:
	case Header::RESENTFROM:
	case Header::RESENTREPLYTO:
	case Header::RESENTTO:
	case Header::TO:
	case Header::FOLLOWUPTO:
	case Header::NEWSGROUPS:
	case Header::RESENTDATE:
		{
			m_default_display=m_display=TRUE;
			m_default_display_headername=m_display_headername=TRUE;
			m_default_display_parsed=m_display_parsed=TRUE;
			m_default_new_line=m_new_line=TRUE;
			m_default_right_adjusted=m_right_adjusted=FALSE;
		}
		break;

	case Header::DATE:
		{
			m_default_display=m_display=TRUE;
			m_default_display_headername=m_display_headername=FALSE;
			m_default_display_parsed=m_display_parsed=TRUE;
			m_default_new_line=m_new_line=FALSE;
			m_default_right_adjusted=m_right_adjusted=TRUE;
		}
		break;

	default:
		{
			m_default_display=m_display=FALSE;
			m_default_display_headername=m_display_headername=TRUE;
			m_default_display_parsed=m_display_parsed=FALSE;
			m_default_new_line=m_new_line=TRUE;
			m_default_right_adjusted=m_right_adjusted=FALSE;
		}
		break;
	};

	m_default_display_type = m_display_type = HeaderDisplay::GetDefaultDisplayType(m_type);

	return OpStatus::OK;
}

OP_STATUS HeaderDisplayItem::ReadSettings(PrefsFile* prefs_file)
{
	OP_STATUS ret = OpStatus::OK;
	OpString section;
	if ((ret=section.Set(UNI_L("Header.")))!=OpStatus::OK ||
		(ret=section.Append(m_headername.CStr()))!= OpStatus::OK)
	{
		return ret;
	}

	TRAP(ret, m_display = prefs_file->ReadIntL(section, UNI_L("Display"), m_default_display)!=FALSE; 
			m_display_headername = prefs_file->ReadIntL(section, UNI_L("Display Name"), m_default_display_headername)!=FALSE; 
			m_display_parsed = prefs_file->ReadIntL(section, UNI_L("Parsed"), m_default_display_parsed)!=FALSE; 
			m_new_line = prefs_file->ReadIntL(section, UNI_L("New Line"), m_default_new_line)!=FALSE; 
			m_right_adjusted = prefs_file->ReadIntL(section, UNI_L("Right Adjusted"), m_default_right_adjusted)!=FALSE; 
			m_display_type = (HeaderDisplay::DisplayType)prefs_file->ReadIntL(section, UNI_L("Display Type"), (HeaderDisplay::DisplayType)m_default_display_type));

	//Font Att
	if (ret!=OpStatus::OK)
		return ret;

	OpString default_serializedfont, serializedfont;
	ret = m_default_font_att.Serialize(&default_serializedfont);
	if (ret!=OpStatus::OK)
		return ret;

	TRAP(ret, prefs_file->ReadStringL(section, UNI_L("Font"), serializedfont, default_serializedfont));
	if (ret!=OpStatus::OK)
		return ret;

	m_font_att.Unserialize(serializedfont.CStr());

	//Font colour
	OpString default_color_string, color_string;

	BYTE default_r = GetRValue(m_default_font_color);
	BYTE default_g = GetGValue(m_default_font_color);
	BYTE default_b = GetBValue(m_default_font_color);
	ret = default_color_string.AppendFormat(UNI_L("#%02x%02x%02x"), default_r, default_g, default_b);
	if (ret!=OpStatus::OK)
		return ret;

	TRAP(ret, prefs_file->ReadStringL(section, UNI_L("Color"), color_string, default_color_string));
	if (ret!=OpStatus::OK)
		return ret;

	int r=0, g=0, b=0;
	if (7 != color_string.Length() ||
		 3 != uni_sscanf(color_string.CStr(), UNI_L("#%2x%2x%2x"), &r, &g, &b) )
	{
		uni_sscanf(default_color_string.CStr(), UNI_L("#%2x%2x%2x"), &r, &g, &b);
	}

	m_font_color = OP_RGB(r, g, b);

	return ret;
}

OP_STATUS HeaderDisplayItem::WriteSettings(PrefsFile* prefs_file)
{
	OP_STATUS ret = OpStatus::OK;
	OpString section;
	if ((ret=section.Set(UNI_L("Header.")))!=OpStatus::OK ||
		(ret=section.Append(m_headername.CStr()))!= OpStatus::OK)
	{

		return ret;
	}

	TRAP(ret, prefs_file->DeleteSectionL(section.CStr()); 
		    if (m_default_display!=m_display) prefs_file->WriteIntL(section, UNI_L("Display"), m_display); 
			if (m_default_display_headername!=m_display_headername) prefs_file->WriteIntL(section, UNI_L("Display Name"), m_display_headername); 
			if (m_default_display_parsed!=m_display_parsed) prefs_file->WriteIntL(section, UNI_L("Parsed"), m_display_parsed); 
			if (m_default_new_line!=m_new_line) prefs_file->WriteIntL(section, UNI_L("New Line"), m_new_line); 
			if (m_default_right_adjusted!=m_right_adjusted) prefs_file->WriteIntL(section, UNI_L("Right Adjusted"), m_right_adjusted); 
			if (m_default_display_type!=m_display_type) prefs_file->WriteIntL(section, UNI_L("Display Type"), m_display_type));

	if (ret!=OpStatus::OK)
		return ret;


	if (!(m_default_font_att==m_font_att))
	{
		OpString serializedfont;
		ret = m_font_att.Serialize(&serializedfont);
		if (ret!=OpStatus::OK)
			return ret;

		TRAP(ret, prefs_file->WriteStringL(section, UNI_L("Font"), serializedfont));
		if (ret!=OpStatus::OK)
			return ret;
	}

	if (m_default_font_color!=m_font_color)
	{
		OpString color_string;

		BYTE default_r = GetRValue(m_font_color);
		BYTE default_g = GetGValue(m_font_color);
		BYTE default_b = GetBValue(m_font_color);
		ret = color_string.AppendFormat(UNI_L("#%02x%02x%02x"), default_r, default_g, default_b);
		if (ret!=OpStatus::OK)
			return ret;

		TRAP(ret, prefs_file->WriteStringL(section, UNI_L("Color"), color_string));
	}

	return ret;
}

/*****************************************************/
HeaderDisplay::HeaderDisplay()
: m_prefs_file(NULL),
  m_headerdisplaylist(NULL),
  m_last_accessed_item(NULL)
{
}

HeaderDisplay::~HeaderDisplay()
{
    if (m_headerdisplaylist)
    {
        m_headerdisplaylist->Clear();
        OP_DELETE(m_headerdisplaylist);
    }
}

OP_STATUS HeaderDisplay::Init(PrefsFile* prefs_file)
{
	OP_ASSERT(m_prefs_file==NULL && m_headerdisplaylist==NULL);

	if (!prefs_file)
		return OpStatus::ERR_NULL_POINTER;

	m_prefs_file = prefs_file;

	m_headerdisplaylist = OP_NEW(Head, ());
	if (!m_headerdisplaylist)
		return OpStatus::ERR_NO_MEMORY;

	int standard_header_count = Header::UNKNOWN-Header::ALSOCONTROL;
	BOOL* header_array = OP_NEWA(BOOL, standard_header_count);
	if (!header_array)
		return OpStatus::ERR_NO_MEMORY;

	op_memset(header_array, FALSE, standard_header_count*sizeof(BOOL));

	OpString header_string;
    TRAPD(ret, prefs_file->ReadStringL(UNI_L("Accounts"), UNI_L("Headers"), header_string,
				UNI_L("Subject,Date,From,Organization,To,Cc,Bcc,Reply-To,Newsgroups,Followup-To,Resent-Date,Resent-From,Resent-To,Resent-Cc,Resent-Bcc,Resent-Reply-To,User-Agent,X-Mailer")));

	OpString8 headername;
	const uni_char* header_start = header_string.CStr();
	const uni_char* header_end;
	while (header_start && *header_start)
	{
		//Find start
		while (*header_start==' ' || *header_start==',') header_start++;
		if (!*header_start)
			break;

		//Find end
		header_end = uni_strchr(header_start, ',');
		if (!header_end)
			header_end = header_start+uni_strlen(header_start);

		while (header_end>header_start && (*header_end==0 || *header_end==' ' || *header_end==',')) header_end--;

		//Process header
		headername.Set(header_start, header_end-header_start+1);
		if (headername.HasContent())
		{
			HeaderDisplayItem* header_item = FindHeaderItem(headername);
			if (!header_item)
			{
				header_item = CreateItem(headername);
				if (header_item)
					header_item->ReadSettings(m_prefs_file);
			}

			if (header_item && header_item->m_type < Header::UNKNOWN)
			{
				header_array[header_item->m_type] |= TRUE;
			}
		}

		header_start = header_end+1;
	}

	//Append all other standard headers
	int i;
	for (i=0; i<standard_header_count; i++)
	{
		if (!header_array[i] && !Header::IsInternalHeader((Header::HeaderType)i))
		{
			HeaderDisplayItem* header_item = FindHeaderItem(static_cast<Header::HeaderType>(i));
			if (!header_item)
			{
				header_item = CreateItem(static_cast<Header::HeaderType>(i));
			}
		}
	}
	OP_DELETEA(header_array);

	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SaveSettings()
{
	if (!m_prefs_file)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS ret;
	//Generate header string
	OpString header_string;
	HeaderDisplayItem* header_item = static_cast<HeaderDisplayItem*>(m_headerdisplaylist->First());
	while (header_item)
	{
		if (header_string.HasContent())
		{
			if ((ret=header_string.Append(",")) != OpStatus::OK)
				return ret;
		}

		if ((ret=header_string.Append(header_item->m_headername.CStr())) != OpStatus::OK)
			return ret;

		header_item = static_cast<HeaderDisplayItem*>(header_item->Suc());
	}
	//Write header string
    TRAP(ret, m_prefs_file->WriteStringL(UNI_L("Accounts"), UNI_L("Headers"), header_string));

	//Write header settings
	header_item = static_cast<HeaderDisplayItem*>(m_headerdisplaylist->First());
	while (header_item)
	{
		if ((ret=header_item->WriteSettings(m_prefs_file)) != OpStatus::OK)
			return ret;

		header_item = static_cast<HeaderDisplayItem*>(header_item->Suc());
	}

	TRAP(ret, m_prefs_file->CommitL());

	return OpStatus::OK;
}

const HeaderDisplayItem* HeaderDisplay::GetItem(UINT index) const
{
	UINT i = index;
	const HeaderDisplayItem* item = static_cast<const HeaderDisplayItem*>(m_headerdisplaylist->First());

	while (item && i>0)
	{
		item = static_cast<const HeaderDisplayItem*>(item->Suc());
		i--;
	}

	return item;
}

const HeaderDisplayItem* HeaderDisplay::FindItem(Header::HeaderType headertype)
{
	return FindHeaderItem(headertype);
}

const HeaderDisplayItem* HeaderDisplay::FindItem(const OpStringC8& headername)
{
	return FindHeaderItem(headername);
}

void HeaderDisplay::MoveUp(UINT index)
{
	HeaderDisplayItem* item = const_cast<HeaderDisplayItem*>(GetItem(index));
	if (!item)
		return;

	HeaderDisplayItem* movedown_item = static_cast<HeaderDisplayItem*>(item->Pred());
	if (movedown_item)
	{
		movedown_item->Out();
		movedown_item->Follow(item);
	}
}

void HeaderDisplay::MoveDown(UINT index)
{
	HeaderDisplayItem* item = const_cast<HeaderDisplayItem*>(GetItem(index));
	if (!item)
		return;

	HeaderDisplayItem* moveup_item = static_cast<HeaderDisplayItem*>(item->Suc());
	if (moveup_item)
	{
		moveup_item->Out();
		moveup_item->Precede(item);
	}
}

HeaderDisplay::DisplayType HeaderDisplay::GetDefaultDisplayType(Header::HeaderType headertype)
{
	switch(headertype)
	{
	case Header::FROM:	return LONG_ADDRESS;

	case Header::BCC:
	case Header::CC:
	case Header::APPROVED:
	case Header::COMPLAINTSTO:
	case Header::MAILCOPIESTO:
	case Header::REPLYTO:
	case Header::RESENTBCC:
	case Header::RESENTCC:
	case Header::RESENTFROM:
	case Header::RESENTREPLYTO:
	case Header::RESENTSENDER:
	case Header::RESENTTO:
	case Header::SENDER:
	case Header::TO:	return SHORT_ADDRESS;

	case Header::ARTICLENAMES:
	case Header::FOLLOWUPTO:
	case Header::NEWSGROUPS:	return NEWSGROUP;

	case Header::ARTICLEUPDATES:
	case Header::INREPLYTO:
	case Header::MESSAGEID:
	case Header::RESENTMESSAGEID:
	case Header::REFERENCES:
	case Header::SEEALSO:
	case Header::SUPERSEDES:	return MESSAGE_ID;

	case Header::DATE:
	case Header::EXPIRES:
	case Header::RESENTDATE:	return DATE;

	case Header::XFACE:	return XFACE;

	case Header::XREF:	return XREF;

	default:	return RAW;
	};
}

HeaderDisplay::DisplayType HeaderDisplay::GetDefaultDisplayType(const OpStringC8& headername)
{
	return GetDefaultDisplayType(Header::GetType(headername));
}

/*****************************************************/
BOOL HeaderDisplay::GetDisplay(Header::HeaderType headertype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	return (item && item->m_display);
}

BOOL HeaderDisplay::GetDisplay(const OpStringC8& headername)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	return (item && item->m_display);
}

BOOL HeaderDisplay::GetDisplayHeadername(Header::HeaderType headertype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	return (item && item->m_display_headername);
}

BOOL HeaderDisplay::GetDisplayHeadername(const OpStringC8& headername)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	return (item && item->m_display_headername);
}

BOOL HeaderDisplay::GetDisplayParsed(Header::HeaderType headertype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	return (item && item->m_display_parsed);
}

BOOL HeaderDisplay::GetDisplayParsed(const OpStringC8& headername)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	return (item && item->m_display_parsed);
}

BOOL HeaderDisplay::GetNewLine(Header::HeaderType headertype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	return (item && item->m_new_line);
}

BOOL HeaderDisplay::GetNewLine(const OpStringC8& headername)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	return (item && item->m_new_line);
}

BOOL HeaderDisplay::GetRightAdjusted(Header::HeaderType headertype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	return (item && item->m_right_adjusted);
}

BOOL HeaderDisplay::GetRightAdjusted(const OpStringC8& headername)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	return (item && item->m_right_adjusted);
}

OP_STATUS HeaderDisplay::GetFontAtt(Header::HeaderType headertype, FontAtt& font_att)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	font_att = item->m_font_att;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::GetFontAtt(const OpStringC8& headername, FontAtt& font_att)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	font_att = item->m_font_att;
	return OpStatus::OK;
}

COLORREF HeaderDisplay::GetFontColor(Header::HeaderType headertype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	return (item && item->m_font_color);
}

COLORREF HeaderDisplay::GetFontColor(const OpStringC8& headername)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	return (item && item->m_font_color);
}

OP_STATUS HeaderDisplay::GetDisplayType(const OpStringC8& headername, DisplayType &displaytype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	displaytype = item->m_display_type;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::GetDisplayType(Header::HeaderType headertype, DisplayType &displaytype)
{
	const HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	displaytype = item->m_display_type;
	return OpStatus::OK;
}

/*****************************************************/
OP_STATUS HeaderDisplay::SetDisplay(Header::HeaderType headertype, BOOL display)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display = display!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplay(const OpStringC8& headername, BOOL display)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display = display!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplayHeadername(Header::HeaderType headertype, BOOL display)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display_headername = display!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplayHeadername(const OpStringC8& headername, BOOL display)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display_headername = display!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplayParsed(Header::HeaderType headertype, BOOL parsed)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display_parsed = parsed!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplayParsed(const OpStringC8& headername, BOOL parsed)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display_parsed = parsed!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetNewLine(Header::HeaderType headertype, BOOL new_line)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_new_line = new_line!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetNewLine(const OpStringC8& headername, BOOL new_line)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_new_line = new_line!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetRightAdjusted(Header::HeaderType headertype, BOOL right_adjusted)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_right_adjusted = right_adjusted!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetRightAdjusted(const OpStringC8& headername, BOOL right_adjusted)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_right_adjusted = right_adjusted!=FALSE;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetFontAtt(Header::HeaderType headertype, FontAtt& font_att)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_font_att = font_att;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetFontAtt(const OpStringC8& headername, FontAtt& font_att)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_font_att = font_att;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetFontColor(Header::HeaderType headertype, COLORREF font_color)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_font_color = font_color;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetFontColor(const OpStringC8& headername, COLORREF font_color)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_font_color = font_color;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplayType(Header::HeaderType headertype, DisplayType displaytype)
{
	HeaderDisplayItem* item = FindHeaderItem(headertype);
	if (!item)
	{
		item = CreateItem(headertype);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display_type = displaytype;
	return OpStatus::OK;
}

OP_STATUS HeaderDisplay::SetDisplayType(const OpStringC8& headername, DisplayType displaytype)
{
	HeaderDisplayItem* item = FindHeaderItem(headername);
	if (!item)
	{
		item = CreateItem(headername);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
	}

	item->m_display_type = displaytype;
	return OpStatus::OK;
}

/*****************************************************/
HeaderDisplayItem* HeaderDisplay::FindLastDisplayOrUnknownHeaderItem()
{
	HeaderDisplayItem* last_displayed_item = NULL;

	HeaderDisplayItem* item = static_cast<HeaderDisplayItem*>(m_headerdisplaylist->First());
	while (item)
	{
		if (item->m_display!=FALSE || item->m_type==Header::UNKNOWN)
			last_displayed_item = item;

		item = static_cast<HeaderDisplayItem*>(item->Suc());
	}

	return last_displayed_item;
}

HeaderDisplayItem* HeaderDisplay::FindHeaderItem(Header::HeaderType headertype)
{
	if (m_last_accessed_item && m_last_accessed_item->m_type==headertype)
		return m_last_accessed_item;

	HeaderDisplayItem* item = static_cast<HeaderDisplayItem*>(m_headerdisplaylist->First());
	while (item && item->m_type!=headertype)
	{
		item = static_cast<HeaderDisplayItem*>(item->Suc());
	}

	if (item)
		m_last_accessed_item = item;

	return item;
}

HeaderDisplayItem* HeaderDisplay::FindHeaderItem(const OpStringC8& headername)
{
	if (m_last_accessed_item && m_last_accessed_item->m_headername.CompareI(headername)==0)
		return m_last_accessed_item;

	HeaderDisplayItem* item = static_cast<HeaderDisplayItem*>(m_headerdisplaylist->First());
	while (item && item->m_headername.CompareI(headername)!=0)
	{
		item = static_cast<HeaderDisplayItem*>(item->Suc());
	}

	if (item)
		m_last_accessed_item = item;

	return item;
}

HeaderDisplayItem* HeaderDisplay::CreateItem(Header::HeaderType headertype)
{
	OP_ASSERT(FindHeaderItem(headertype)==NULL);
	HeaderDisplayItem* item = OP_NEW(HeaderDisplayItem, ());
	if (!item)
		return NULL;

	item->Into(m_headerdisplaylist);
	m_last_accessed_item = item;

	item->m_type = headertype;
	OpStatus::Ignore(Header::GetName(headertype, item->m_headername));

	item->Init();
	return item;
}

HeaderDisplayItem* HeaderDisplay::CreateItem(const OpStringC8& headername)
{
	OP_ASSERT(FindHeaderItem(headername)==NULL);
	HeaderDisplayItem* item = OP_NEW(HeaderDisplayItem, ());
	if (!item)
		return NULL;

	item->Into(m_headerdisplaylist);
	m_last_accessed_item = item;

	if (item->m_headername.Set(headername) != OpStatus::OK)
	{
		item->Out();
		OP_DELETE(item);
		return NULL;
	}
	item->m_type = Header::GetType(headername);

	item->Init();
	return item;
}

#endif //M2_SUPPORT
