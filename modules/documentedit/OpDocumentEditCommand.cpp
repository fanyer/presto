/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/dom/domutils.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/pi/OpClipboard.h"
#include "modules/util/opstrlst.h"
#include "modules/style/css.h"
#include "modules/style/css_dom.h"
#include "modules/layout/layout_workplace.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

#ifdef DOCUMENT_EDIT_SUPPORT


#define REPORT_AND_RETURN_FALSE_IF_ERROR(status)	if (OpStatus::IsError(status)) \
													{ \
														if (OpStatus::IsMemoryError(status)) \
															ReportOOM(); \
														return FALSE; \
													}

/* static */
OP_DOCUMENT_EDIT_COMMAND OpDocumentEdit::ConvertCommand(const uni_char* command)
{
	// IE also has Refresh, LiveResize, etc...

	if (command) switch (command[0])
	{
	case 'b': case 'B':
		if (uni_stricmp(command, UNI_L("BACKCOLOR")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_BACKCOLOR;

		else if (uni_stricmp(command, UNI_L("BOLD")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_BOLD;
		break;

	case 'c': case 'C':
		if (uni_stricmp(command, UNI_L("CONTENTREADONLY")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_CONTENTREADONLY;

		else if (uni_stricmp(command, UNI_L("COPY")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_COPY;

		else if (uni_stricmp(command, UNI_L("CREATELINK")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_CREATELINK;

		else if (uni_stricmp(command, UNI_L("CUT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_CUT;
		break;

	case 'd': case 'D':
		if (uni_stricmp(command, UNI_L("DECREASEFONTSIZE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE;

		else if (uni_stricmp(command, UNI_L("DELETE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_DELETE;
		break;

	case 'f': case 'F':
		if (uni_stricmp(command, UNI_L("FONTNAME")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_FONTNAME;

		else if (uni_stricmp(command, UNI_L("FONTSIZE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_FONTSIZE;

		else if (uni_stricmp(command, UNI_L("FORECOLOR")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_FORECOLOR;

		else if (uni_stricmp(command, UNI_L("FORMATBLOCK")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_FORMATBLOCK;
		break;

	case 'h': case 'H':
		if (uni_stricmp(command, UNI_L("HILITECOLOR")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_HILITECOLOR;
		break;

	case 'i': case 'I':
		if (uni_stricmp(command, UNI_L("INCREASEFONTSIZE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE;

		else if (uni_stricmp(command, UNI_L("INDENT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INDENT;

		// Maybe worth a strincmp pretest on "insert"
		else if (uni_stricmp(command, UNI_L("INSERTHORIZONTALRULE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INSERTHORIZONTALRULE;

		else if (uni_stricmp(command, UNI_L("INSERTHTML")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INSERTHTML;

		else if (uni_stricmp(command, UNI_L("INSERTIMAGE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INSERTIMAGE;

		else if (uni_stricmp(command, UNI_L("INSERTORDEREDLIST")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST;

		else if (uni_stricmp(command, UNI_L("INSERTPARAGRAPH")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INSERTPARAGRAPH;

		else if (uni_stricmp(command, UNI_L("INSERTUNORDEREDLIST")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST;

		else if (uni_stricmp(command, UNI_L("ITALIC")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_ITALIC;
		break;

	case 'j': case 'J':
		// Maybe worth a strincmp pretest on "justify"
		if (uni_stricmp(command, UNI_L("JUSTIFYCENTER")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER;

		else if (uni_stricmp(command, UNI_L("JUSTIFYFULL")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL;

		else if (uni_stricmp(command, UNI_L("JUSTIFYLEFT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT;

		else if (uni_stricmp(command, UNI_L("JUSTIFYRIGHT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT;
		break;

	case 'o': case 'O':
		if (uni_stricmp(command, UNI_L("OUTDENT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_OUTDENT;
		else if (uni_stricmp(command, UNI_L("OPERA-DEFAULTBLOCK")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_DEFAULT_BLOCK;
		break;

	case 'p': case 'P':
		if (uni_stricmp(command, UNI_L("PASTE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_PASTE;
		break;

	case 'r': case 'R':
		if (uni_stricmp(command, UNI_L("READONLY")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_READONLY;

		else if (uni_stricmp(command, UNI_L("REDO")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_REDO;

		else if (uni_stricmp(command, UNI_L("REMOVEFORMAT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_REMOVEFORMAT;
		break;

	case 's': case 'S':
		if (uni_stricmp(command, UNI_L("SELECTALL")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SELECTALL;

#ifdef SPELLCHECK_COMMAND_SUPPORT
		else if (uni_stricmp(command, UNI_L("SPELLCHECK")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK;
		else if (uni_stricmp(command, UNI_L("SPELLCHECK_BLOCKING")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_BLOCKING;
		else if (uni_stricmp(command, UNI_L("SPELLCHECK_DEBUG")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_DEBUG;
		else if (uni_stricmp(command, UNI_L("SPELLCHECK_HAS_MISSPELLING")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_HAS_MISSPELLING;
#endif

		else if (uni_stricmp(command, UNI_L("STRIKETHROUGH")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH;

		else if(uni_stricmp(command, UNI_L("STYLEWITHCSS")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_STYLEWITHCSS;

		else if (uni_stricmp(command, UNI_L("SUBSCRIPT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT;

		else if (uni_stricmp(command, UNI_L("SUPERSCRIPT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT;
		break;

	case 'u': case 'U':
		if (uni_stricmp(command, UNI_L("UNDERLINE")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_UNDERLINE;

		else if (uni_stricmp(command, UNI_L("UNDO")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_UNDO;

		else if (uni_stricmp(command, UNI_L("UNLINK")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_UNLINK;

		else if (uni_stricmp(command, UNI_L("UNSELECT")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_UNSELECT;

		else if (uni_stricmp(command, UNI_L("USECSS")) == 0)
			return OP_DOCUMENT_EDIT_COMMAND_USECSS;
		break;
	}

	return OP_DOCUMENT_EDIT_COMMAND_UNKNOWN;
}

static HTML_ElementType ConvertFormatBlockValueToType(const uni_char* value)
{
	DEBUG_CHECKER_STATIC();
	// Fix: escape the string.
	if (uni_stristr(value, UNI_L("PRE")))
		return HE_PRE;
	else if (uni_stristr(value, UNI_L("P")))
		return HE_P;
	else if (uni_stristr(value, UNI_L("DIV")))
		return HE_DIV;
	else if (uni_stristr(value, UNI_L("H1")))
		return HE_H1;
	else if (uni_stristr(value, UNI_L("H2")))
		return HE_H2;
	else if (uni_stristr(value, UNI_L("H3")))
		return HE_H3;
	else if (uni_stristr(value, UNI_L("H4")))
		return HE_H4;
	else if (uni_stristr(value, UNI_L("H5")))
		return HE_H5;
	else if (uni_stristr(value, UNI_L("H6")))
		return HE_H6;
	else if (uni_stristr(value, UNI_L("ADDRESS")))
		return HE_ADDRESS;
	else if (uni_stristr(value, UNI_L("BLOCKQUOTE")))
		return HE_BLOCKQUOTE;
	return HE_UNKNOWN;
}

static const uni_char* ConvertTypeToFormatBlockValue(HTML_ElementType type)
{
	DEBUG_CHECKER_STATIC();
	switch(type)
	{
		 // ADS 1.2 can't cope with returning strings directly !
	case HE_P: { const uni_char *tmp = UNI_L("P"); return tmp; }
	case HE_DIV: { const uni_char *tmp = UNI_L("DIV"); return tmp; }
	case HE_H1: { const uni_char *tmp = UNI_L("H1"); return tmp; }
	case HE_H2: { const uni_char *tmp = UNI_L("H2"); return tmp; }
	case HE_H3: { const uni_char *tmp = UNI_L("H3"); return tmp; }
	case HE_H4: { const uni_char *tmp = UNI_L("H4"); return tmp; }
	case HE_H5: { const uni_char *tmp = UNI_L("H5"); return tmp; }
	case HE_H6: { const uni_char *tmp = UNI_L("H6"); return tmp; }
	case HE_ADDRESS: { const uni_char *tmp = UNI_L("ADDRESS"); return tmp; }
	case HE_PRE: { const uni_char *tmp = UNI_L("PRE"); return tmp; }
	case HE_BLOCKQUOTE: { const uni_char *tmp = UNI_L("BLOCKQUOTE"); return tmp; }
	}
	const uni_char * tmp = UNI_L("");
	return tmp;
}

static HTML_ElementType ConvertCommandToType(OP_DOCUMENT_EDIT_COMMAND command)
{
	DEBUG_CHECKER_STATIC();
	switch(command)
	{
		case OP_DOCUMENT_EDIT_COMMAND_BOLD:				return HE_STRONG;
		case OP_DOCUMENT_EDIT_COMMAND_ITALIC:			return HE_EM;
		case OP_DOCUMENT_EDIT_COMMAND_UNDERLINE:		return HE_U;
		case OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH:	return HE_STRIKE;
		case OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT:		return HE_SUB;
		case OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT:		return HE_SUP;
		case OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE:	return HE_SMALL;
		case OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE:	return HE_BIG;
		case OP_DOCUMENT_EDIT_COMMAND_CREATELINK:		return HE_A;
		case OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST:	return HE_OL;
		case OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST:	return HE_UL;
		case OP_DOCUMENT_EDIT_COMMAND_INSERTPARAGRAPH:	return HE_P;
	}
	return HE_UNKNOWN;
}

static int ConvertCommandToAlign(OP_DOCUMENT_EDIT_COMMAND command)
{
	DEBUG_CHECKER_STATIC();
	int align = CSS_VALUE_left;
	switch(command)
	{
		case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER: align = CSS_VALUE_center; break;
		case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL: align = CSS_VALUE_justify; break;
		case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT: align = CSS_VALUE_right; break;
	}
	return align;
}

static BOOL GetAllowCommandInReadonly(OP_DOCUMENT_EDIT_COMMAND command)
{
	DEBUG_CHECKER_STATIC();
	switch(command)
	{
	case OP_DOCUMENT_EDIT_COMMAND_USECSS:
	case OP_DOCUMENT_EDIT_COMMAND_STYLEWITHCSS:
	case OP_DOCUMENT_EDIT_COMMAND_READONLY:
	case OP_DOCUMENT_EDIT_COMMAND_CONTENTREADONLY:
	case OP_DOCUMENT_EDIT_COMMAND_COPY:
	case OP_DOCUMENT_EDIT_COMMAND_SELECTALL:
		return TRUE;
	}
	return FALSE;
}

/* static */
OP_STATUS OpDocumentEdit::execCommand(const uni_char* command, OpDocumentEdit* document_edit, BOOL showui, const uni_char* value, URL& origin_url, FramesDocument* frm_doc)
{
	OP_ASSERT((document_edit || frm_doc) && origin_url.IsValid());
	OP_DOCUMENT_EDIT_COMMAND cmd = ConvertCommand(command);

	if (document_edit)
	{
		RETURN_IF_ERROR(document_edit->queryCommandSupported(cmd, origin_url));
		return document_edit->execCommand(cmd, showui, value);
	}
	else
		/* Spec allows some commands to be called without existing
		   "editing host". We are handling a subset as it's not
		   always clear how specific commands should be handled. */
		switch (cmd)
		{
#ifdef USE_OP_CLIPBOARD
		case OP_DOCUMENT_EDIT_COMMAND_COPY:
		case OP_DOCUMENT_EDIT_COMMAND_CUT:
		case OP_DOCUMENT_EDIT_COMMAND_PASTE:
		{
			if (!DOM_Utils::IsClipboardAccessAllowed(origin_url, frm_doc->GetWindow()))
				return OpStatus::ERR_NO_ACCESS;

			OP_STATUS status;
			ClipboardListener *listener = frm_doc->GetClipboardListener();
			if (cmd == OP_DOCUMENT_EDIT_COMMAND_COPY)
				status = g_clipboard_manager->Copy(listener, frm_doc->GetWindow()->GetUrlContextId(), frm_doc);
			else if (cmd == OP_DOCUMENT_EDIT_COMMAND_CUT)
				status = g_clipboard_manager->Cut(listener, frm_doc->GetWindow()->GetUrlContextId(), frm_doc);
			else
				status = g_clipboard_manager->Paste(listener, frm_doc);

			return status;
		}
#endif // USE_OP_CLIPBOARD

		default:
			return OpStatus::ERR_NOT_SUPPORTED;
		}
}

/* static */
BOOL OpDocumentEdit::queryCommandEnabled(const uni_char* command, OpDocumentEdit* document_edit, URL& origin_url, FramesDocument* frm_doc)
{
	OP_ASSERT(document_edit || (origin_url.IsValid() && frm_doc));

	if (document_edit)
	{
		document_edit->CheckLogTreeChanged();
		return document_edit->queryCommandEnabled(ConvertCommand(command));
	}
	else
		switch (ConvertCommand(command))
		{
#ifdef USE_OP_CLIPBOARD
		case OP_DOCUMENT_EDIT_COMMAND_COPY:
		case OP_DOCUMENT_EDIT_COMMAND_CUT:
			return DOM_Utils::IsClipboardAccessAllowed(origin_url, frm_doc->GetWindow()) && frm_doc->HasSelectedText();

		case OP_DOCUMENT_EDIT_COMMAND_PASTE:
			return DOM_Utils::IsClipboardAccessAllowed(origin_url, frm_doc->GetWindow()) && g_clipboard_manager->HasText();
#endif // USE_OP_CLIPBOARD

		default:
			return FALSE;
		}
}

BOOL OpDocumentEdit::queryCommandState(const uni_char* command)
{
	DEBUG_CHECKER(TRUE);
	CheckLogTreeChanged();
	return queryCommandState(ConvertCommand(command));
}

/* static */
BOOL OpDocumentEdit::queryCommandSupported(const uni_char* command, OpDocumentEdit* document_edit, URL& origin_url, FramesDocument* frm_doc)
{
	OP_ASSERT((document_edit || frm_doc) && origin_url.IsValid());
	if (document_edit)
	{
		document_edit->CheckLogTreeChanged();
		return OpStatus::IsSuccess(document_edit->queryCommandSupported(ConvertCommand(command), origin_url));
	}
	else
		switch (ConvertCommand(command))
		{
#ifdef USE_OP_CLIPBOARD
		case OP_DOCUMENT_EDIT_COMMAND_COPY:
		case OP_DOCUMENT_EDIT_COMMAND_CUT:
		case OP_DOCUMENT_EDIT_COMMAND_PASTE:
			return DOM_Utils::IsClipboardAccessAllowed(origin_url, frm_doc->GetWindow());
#endif // USE_OP_CLIPBOARD

		default:
			return FALSE;
		}

}

OP_STATUS OpDocumentEdit::queryCommandValue(const uni_char* command, TempBuffer& value)
{
	DEBUG_CHECKER(TRUE);
	CheckLogTreeChanged();
	return queryCommandValue(ConvertCommand(command), value);
}

OP_STATUS OpDocumentEdit::execCommand(OP_DOCUMENT_EDIT_COMMAND command, BOOL showui, const uni_char* value)
{
	DEBUG_CHECKER(TRUE);
	CheckLogTreeChanged();

	if (m_readonly && !GetAllowCommandInReadonly(command))
		return OpStatus::ERR_NOT_SUPPORTED;

	ReflowAndUpdate();

	switch(command)
	{
	case OP_DOCUMENT_EDIT_COMMAND_USECSS:
		if (value)
			// I think FALSE should be TRUE here. But mozilla and mozillas testsuite seem to have mixed it up??
			m_usecss = (uni_stricmp(value, UNI_L("FALSE")) == 0);
		break;
	case OP_DOCUMENT_EDIT_COMMAND_STYLEWITHCSS:
		if (value)
			// I think FALSE should be TRUE here. But mozilla and mozillas testsuite seem to have mixed it up??
			m_usecss = (uni_stricmp(value, UNI_L("TRUE")) == 0);
		break;
#ifdef SPELLCHECK_COMMAND_SUPPORT
	case OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_BLOCKING:
		m_blocking_spellcheck = TRUE;
	case OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK:
		return HandleSpellCheckCommand(showui, value);
#endif
	case OP_DOCUMENT_EDIT_COMMAND_READONLY:
		if (value) // Depricated command. Replaced by CONTENTREADONLY to fix mozilla bug.
			m_readonly = (uni_stricmp(value, UNI_L("FALSE")) == 0);
		break;
	case OP_DOCUMENT_EDIT_COMMAND_CONTENTREADONLY:
		if (value)
			m_readonly = (uni_stricmp(value, UNI_L("TRUE")) == 0);
		break;
	case OP_DOCUMENT_EDIT_COMMAND_HILITECOLOR:
		if (value)
		{
			TempBuffer text;
			REPORT_AND_RETURN_FALSE_IF_ERROR(text.Append("background-color: "));
			REPORT_AND_RETURN_FALSE_IF_ERROR(text.Append(value));
			REPORT_AND_RETURN_FALSE_IF_ERROR(text.Append(";"));

			HTML_Element* helm = NewElement(HE_SPAN);
			if (!helm)
			{
				ReportOOM();
				return FALSE;
			}

			REPORT_AND_RETURN_FALSE_IF_ERROR(helm->SetAttribute(GetDoc(), ATTR_STYLE, NULL, NS_IDX_DEFAULT, text.GetStorage(), text.Length(), NULL, FALSE));
			REPORT_AND_RETURN_FALSE_IF_ERROR(InsertStyleElement(helm));
		}
		break;
	case OP_DOCUMENT_EDIT_COMMAND_BACKCOLOR:
		if (value && m_caret.GetElement())
		{
			HTML_Element* root = m_doc->GetCaret()->GetContainingElementActual(m_caret.GetElement());
			BeginChange(root);

			CSS_DOMStyleDeclaration *styledeclaration;
			if (OpStatus::IsSuccess(root->DOMGetInlineStyle(styledeclaration, m_doc->GetDOMEnvironment())))
			{
				OnElementChange(root);
				CSS_DOMException exception = CSS_DOMEXCEPTION_NONE;
				styledeclaration->SetProperty(CSS_PROPERTY_background_color, value, exception);
				OP_DELETE(styledeclaration);
				OnElementChanged(root);
			}

			EndChange(root);
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_BOLD:
	case OP_DOCUMENT_EDIT_COMMAND_ITALIC:
	case OP_DOCUMENT_EDIT_COMMAND_UNDERLINE:
	case OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH:
	case OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT:
	case OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT:
		{
			HTML_ElementType type = ConvertCommandToType(command);
			BOOL removed = FALSE;

			m_undo_stack.BeginGroup();
			if (command == OP_DOCUMENT_EDIT_COMMAND_ITALIC)
			{
				// Special case since italic need to check 2 types
				if (GetHasStyle(HE_I,ATTR_NULL,TRUE))
				{
					RemoveStyle(HE_I);
					removed = TRUE;
				}
				else if(GetHasStyle(HE_I))
					RemoveStyle(HE_I);
				if (GetHasStyle(HE_EM,ATTR_NULL,TRUE))
				{
					RemoveStyle(HE_EM);
					removed = TRUE;
				}
				else if(GetHasStyle(HE_EM))
					RemoveStyle(HE_EM);
			}
			else if (command == OP_DOCUMENT_EDIT_COMMAND_BOLD)
			{
				// Special case since bold need to check 2 types
				if (GetHasStyle(HE_B,ATTR_NULL,TRUE))
				{
					RemoveStyle(HE_B);
					removed = TRUE;
				}
				else if(GetHasStyle(HE_B))
					RemoveStyle(HE_B);
				if (GetHasStyle(HE_STRONG,ATTR_NULL,TRUE))
				{
					RemoveStyle(HE_STRONG);
					removed = TRUE;
				}
				else if(GetHasStyle(HE_STRONG))
					RemoveStyle(HE_STRONG);
			}
			else if (GetHasStyle(type,ATTR_NULL,TRUE))
			{
				RemoveStyle(type);
				removed = TRUE;
			}
			else if(GetHasStyle(type))
				RemoveStyle(type);

			if (!removed)
				InsertStyle(type);
			m_undo_stack.EndGroup();
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE:
	case OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE:
		{
			HTML_ElementType this_type = command == OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE ? HE_SMALL : HE_BIG;
			HTML_ElementType other_type = this_type == HE_SMALL ? HE_BIG : HE_SMALL;
			m_undo_stack.BeginGroup();
			if(m_selection.HasContent())
			{
				InsertStyle(this_type,other_type,TRUE,HE_FONT);
				if(GetHasStyle(other_type))
					RemoveStyle(other_type,ATTR_NULL,TRUE);
			}
			else
			{
				if(GetHasStyle(other_type))
					RemoveStyle(other_type,ATTR_NULL,TRUE);
				else
					InsertStyle(this_type,(HTML_ElementType)0,TRUE,HE_FONT);
			}
			m_undo_stack.EndGroup();
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_REMOVEFORMAT:
		if (GetHasStyle(HE_ANY))
			RemoveStyle(HE_ANY);
		break;

	case OP_DOCUMENT_EDIT_COMMAND_CREATELINK:
		{
			SelectionState state = GetSelectionState(FALSE);
			if (value && (state.HasEditableContent() || m_layout_modifier.IsActive()))
			{
				HTML_Element *start = NULL, *stop = NULL;
				if(m_layout_modifier.IsActive())
					start = stop = m_layout_modifier.m_helm;
				else
				{
					start = state.editable_start_elm;
					stop = state.editable_stop_elm;
				}
				int true_count = 0;
				void *args[2];
				args[0] = this; args[1] = (void*)value;
				REPORT_AND_RETURN_FALSE_IF_ERROR(ApplyFunctionBetween(start,stop,&SetNewHref,args,NULL,&true_count));
				if(true_count)
					break; // we've changed href on at least ONE anchor in the selection, that's all, don't do any more
				HTML_Element *new_elm = NewElement(HE_A);
				if (!new_elm)
				{
					ReportOOM();
					return FALSE;
				}
				new_elm->SetAttribute(m_doc, ATTR_HREF, NULL, NS_IDX_DEFAULT, value, uni_strlen(value), NULL, FALSE);
				REPORT_AND_RETURN_FALSE_IF_ERROR(InsertStyleElement(new_elm));
			}
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_DELETE:
		{
			OpInputAction action(OpInputAction::ACTION_DELETE);
			OnInputAction(&action);
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_FONTNAME:
		if (value)
			InsertFontFace(value);
		break;

	case OP_DOCUMENT_EDIT_COMMAND_FONTSIZE:
		if (value)
		{
			if(GetHasStyle(HE_BIG))
				RemoveStyle(HE_BIG);
			if(GetHasStyle(HE_SMALL))
				RemoveStyle(HE_SMALL);
			InsertFontSize(uni_atoi(value));
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_FORECOLOR:
		if (value)
		{
			BOOL do_insert = FALSE;
			COLORREF container_color, caret_color;
			COLORREF new_color = GetColorVal(value, uni_strlen(value), TRUE);

			if(m_caret.GetElement())
			{
				Head prop_list;
				HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
				GetSelectionState(FALSE);

				HTML_Element* containing_elm = m_doc->GetCaret()->GetContainingElement(m_caret.GetElement());
				LayoutProperties* lprops = LayoutProperties::CreateCascade(
						containing_elm, prop_list, hld_profile);

				if(lprops)
				{
					container_color = lprops->GetProps()->font_color;

					LayoutProperties* child_props = lprops->GetChildProperties(hld_profile, m_caret.GetElement());
					if (child_props)
					{
						caret_color = child_props->GetProps()->font_color;
						if (new_color != caret_color)
						{
							/* If the new color is different from the current one and the one from
							 * the container element then insert a font tag */
							if (new_color != container_color)
								do_insert = TRUE;
							else if (GetHasStyle(HE_FONT, ATTR_COLOR))
							{
								/* Otherwise, if we were in a font tag with a different color than
								 * the containing element, but the new color matches the container
								 * element's once again, we should continue editing outside of the
								 * font tag. (a sort of "reset color"). */
								RemoveStyle(HE_FONT, ATTR_COLOR);
							}
						}
					}
					else
						do_insert = TRUE;

					prop_list.Clear();
				}
			}
			else
				do_insert = TRUE;

			if (do_insert)
				InsertFontColor(new_color);
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTPARAGRAPH:
		// IE's always inserts paragraph if this is called from script. (Even if inside pre)
		if(m_caret.GetElement())
			InsertBreak(TRUE, TRUE);
		break;

	case OP_DOCUMENT_EDIT_COMMAND_FORMATBLOCK:
		if (value)
		{
			HTML_ElementType newtype = ConvertFormatBlockValueToType(value);
			if (newtype == HE_UNKNOWN)
				return FALSE;

			// Add the new format to the block
			HTML_Element* helm = NewElement(newtype);
			if (helm)
			{
				m_undo_stack.BeginGroup();
				HTML_ElementType remove_types[] = { HE_P, HE_DIV, HE_H1, HE_H2, HE_H3, HE_H4, HE_H5, HE_H6, HE_ADDRESS, HE_PRE, HE_UNKNOWN };
				if(OpStatus::IsSuccess(InsertBlockElementMultiple(helm,FALSE,FALSE,newtype==HE_DIV,remove_types)))
				{
					// Remove any existing format from the block
					RemoveBlockTypes(remove_types,ATTR_NULL,FALSE,newtype);
				}
				m_undo_stack.EndGroup();
			}
			else
				ReportOOM();
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTHORIZONTALRULE:
		if(m_caret.GetElement())
		{
			const uni_char* tmp = UNI_L("<hr>");
			InsertTextHTML(tmp, uni_strlen(tmp),NULL,NULL,GetHrContainer(m_caret.GetElement()));
		}
		break;
	case OP_DOCUMENT_EDIT_COMMAND_INSERTHTML:
		if (value)
		{
			BOOL is_html = uni_strpbrk(value, UNI_L("<&")) != NULL;
			if (!is_html)
				InsertPlainText(value, uni_strlen(value));
			else
				InsertTextHTML(value, uni_strlen(value), NULL, NULL, NULL, TIDY_LEVEL_MINIMAL);
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTIMAGE:
		if (value)
		{
			TempBuffer str;
			str.Append(UNI_L("<img src=\""));
			str.Append(value);
			str.Append(UNI_L("\">"));
			InsertTextHTML(str.GetStorage(), str.Length());
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST:
	case OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST:
		ToggleInsertList(command == OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST);
		break;

	case OP_DOCUMENT_EDIT_COMMAND_INDENT:
		{
			OP_STATUS status = OpStatus::OK;
			HTML_ElementType list_types[] = {HE_OL,HE_UL,HE_UNKNOWN};
			BOOL includes_lists = GetHasBlockTypes(list_types);
			m_undo_stack.BeginGroup();
			if(m_usecss)
				status = IndentSelection(40);
			else
			{
				HTML_Element* helm = NewElement(HE_BLOCKQUOTE);
				if (helm)
					status = InsertBlockElementMultiple(helm,includes_lists);
				else
					status = OpStatus::ERR_NO_MEMORY;
			}
			if(OpStatus::IsSuccess(status) && includes_lists)
				status = IncreaseListNestling();
			if(OpStatus::IsMemoryError(status))
				ReportOOM();
			m_undo_stack.EndGroup();
		}
		break;

	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT:
		JustifySelection((CSSValue)ConvertCommandToAlign(command));
		break;

	case OP_DOCUMENT_EDIT_COMMAND_OUTDENT:
		{
			HTML_ElementType types[] = {HE_BLOCKQUOTE, HE_UNKNOWN};
			HTML_ElementType list_types[] = {HE_OL,HE_UL,HE_UNKNOWN};
			m_undo_stack.BeginGroup();
			if(m_usecss)
				IndentSelection(-40);
			else
				RemoveBlockTypes(types,ATTR_NULL,TRUE);
			if(GetHasBlockTypes(list_types))
				DecreaseListNestling();
			m_undo_stack.EndGroup();
		}
		break;

#ifdef USE_OP_CLIPBOARD
	case OP_DOCUMENT_EDIT_COMMAND_CUT:
		Cut();
		break;
	case OP_DOCUMENT_EDIT_COMMAND_COPY:
		Copy();
		break;
	case OP_DOCUMENT_EDIT_COMMAND_PASTE:
	{
		HTML_Element* target = m_selection.HasContent() ? m_selection.GetStartElement() : m_caret.GetElement();
		g_clipboard_manager->Paste(this, m_doc, target);
	}
	break;
#endif // USE_OP_CLIPBOARD

	case OP_DOCUMENT_EDIT_COMMAND_UNDO:
		Undo();
		break;
	case OP_DOCUMENT_EDIT_COMMAND_REDO:
		Redo();
		break;

	case OP_DOCUMENT_EDIT_COMMAND_SELECTALL:
		m_selection.SelectAll();
		break;

	case OP_DOCUMENT_EDIT_COMMAND_UNLINK:
		if (GetHasStyle(HE_A))
			RemoveStyle(HE_A);
		break;

	case OP_DOCUMENT_EDIT_COMMAND_UNSELECT:
		m_selection.SelectNothing();
		break;

	case OP_DOCUMENT_EDIT_COMMAND_DEFAULT_BLOCK:
		if (uni_stri_eq(value, UNI_L("p")) || uni_stri_eq(value, UNI_L("div")))
			m_paragraph_element_type = static_cast<HTML_ElementType>(HTM_Lex::GetTag(value, uni_strlen(value)));
		break;

	default:
		OP_ASSERT(0);
		break;
	}

	if (m_listener)
		m_listener->OnTextChanged();

	return OpStatus::OK;
}

BOOL OpDocumentEdit::queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND command)
{
	DEBUG_CHECKER(TRUE);
	switch(command)
	{
	case OP_DOCUMENT_EDIT_COMMAND_USECSS:
		return !m_usecss;
	case OP_DOCUMENT_EDIT_COMMAND_STYLEWITHCSS:
		return m_usecss;
	case OP_DOCUMENT_EDIT_COMMAND_READONLY:
	case OP_DOCUMENT_EDIT_COMMAND_CONTENTREADONLY:
		return TRUE;
	case OP_DOCUMENT_EDIT_COMMAND_HILITECOLOR:
	case OP_DOCUMENT_EDIT_COMMAND_BACKCOLOR:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_BOLD:
	case OP_DOCUMENT_EDIT_COMMAND_ITALIC:
	case OP_DOCUMENT_EDIT_COMMAND_UNDERLINE:
	case OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH:
	case OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT:
	case OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE:
	case OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_REMOVEFORMAT:
		return GetHasStyle(HE_ANY);
	case OP_DOCUMENT_EDIT_COMMAND_CREATELINK:
		return m_selection.HasContent();

	case OP_DOCUMENT_EDIT_COMMAND_DELETE:
	case OP_DOCUMENT_EDIT_COMMAND_FONTNAME:
	case OP_DOCUMENT_EDIT_COMMAND_FONTSIZE:
	case OP_DOCUMENT_EDIT_COMMAND_FORECOLOR:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_FORMATBLOCK:
	case OP_DOCUMENT_EDIT_COMMAND_INDENT:
	case OP_DOCUMENT_EDIT_COMMAND_INSERTPARAGRAPH:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTHORIZONTALRULE:
		return TRUE;
	case OP_DOCUMENT_EDIT_COMMAND_INSERTHTML:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTIMAGE:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST:
	case OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_OUTDENT:
		{
			HTML_ElementType types[] = {HE_BLOCKQUOTE,HE_OL,HE_UL,HE_UNKNOWN};
			return GetHasBlockTypes(types);
		}

#ifdef USE_OP_CLIPBOARD
	case OP_DOCUMENT_EDIT_COMMAND_COPY:
		return m_selection.HasContent() || m_layout_modifier.IsActive();
	case OP_DOCUMENT_EDIT_COMMAND_CUT:
		return m_selection.HasContent() || m_layout_modifier.IsActive();
	case OP_DOCUMENT_EDIT_COMMAND_PASTE:
		return g_clipboard_manager->HasText();
#endif // USE_OP_CLIPBOARD

	case OP_DOCUMENT_EDIT_COMMAND_SELECTALL:
		return TRUE;

	case OP_DOCUMENT_EDIT_COMMAND_UNDO:
		return m_undo_stack.CanUndo();
	case OP_DOCUMENT_EDIT_COMMAND_REDO:
		return m_undo_stack.CanRedo();

	case OP_DOCUMENT_EDIT_COMMAND_UNLINK:
		return GetHasStyle(HE_A);
	case OP_DOCUMENT_EDIT_COMMAND_UNSELECT:
		return TRUE;
	}
	return FALSE;
}

BOOL OpDocumentEdit::queryCommandState(OP_DOCUMENT_EDIT_COMMAND command)
{
	DEBUG_CHECKER(TRUE);
	HTML_ElementType indent_types[] = {HE_BLOCKQUOTE,HE_OL,HE_UL,HE_UNKNOWN};

	switch(command)
	{
	case OP_DOCUMENT_EDIT_COMMAND_USECSS:
		// I think FALSE should be TRUE here. But mozilla and mozillas testsuite seem to have mixed it up??
		return !m_usecss;
	case OP_DOCUMENT_EDIT_COMMAND_STYLEWITHCSS:
		return m_usecss;
	case OP_DOCUMENT_EDIT_COMMAND_READONLY:
		// Depricated command. Replaced by CONTENTREADONLY to fix mozilla bug.
		return !m_readonly;
	case OP_DOCUMENT_EDIT_COMMAND_CONTENTREADONLY:
		return m_readonly;
	case OP_DOCUMENT_EDIT_COMMAND_INDENT:
		return GetHasBlockTypes(indent_types);
	case OP_DOCUMENT_EDIT_COMMAND_OUTDENT:
		return !GetHasBlockTypes(indent_types);

	case OP_DOCUMENT_EDIT_COMMAND_CREATELINK:
	case OP_DOCUMENT_EDIT_COMMAND_UNLINK:
	case OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE:
	case OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE:
		return FALSE;

#ifdef SPELLCHECK_COMMAND_SUPPORT
	case OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_HAS_MISSPELLING:
		return m_has_spellchecked;
#endif // SPELLCHECK_COMMAND_SUPPORT


	case OP_DOCUMENT_EDIT_COMMAND_FONTNAME:
		return GetHasStyle(HE_FONT,ATTR_FACE);
	case OP_DOCUMENT_EDIT_COMMAND_FONTSIZE:
		return GetHasStyle(HE_FONT,ATTR_SIZE);
	case OP_DOCUMENT_EDIT_COMMAND_FORECOLOR:
		return GetHasStyle(HE_FONT,ATTR_COLOR);
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT:
	case OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT:
		{
			HTML_ElementType check_types[] = { HE_TEXT, HE_BR, HE_UNKNOWN };
			int align = ConvertCommandToAlign(command);
			if(align == CSS_VALUE_left)
				return !GetSelectionMatchesFunction(&IsNonLeftAligned,NULL,NULL,NULL,FALSE,FALSE);

			return GetSelectionMatchesFunction(&IsAlignedAs,reinterpret_cast<void*>(align),&StaticIsMatchingType,check_types,TRUE,FALSE);
		}
	case OP_DOCUMENT_EDIT_COMMAND_FORMATBLOCK:
		{
			HTML_ElementType types[] = { HE_P, HE_H1, HE_H2, HE_H3, HE_H4, HE_H5, HE_H6, HE_ADDRESS, HE_PRE, HE_UNKNOWN };
			return GetHasBlockTypes(types);
		}
	default:
		break;
	}

	HTML_ElementType type = ConvertCommandToType(command);
	if(!(type == HE_UNKNOWN))
	{
		if(type == HE_STRONG || type == HE_B)
			return GetHasStyle(HE_STRONG,ATTR_NULL,TRUE) || GetHasStyle(HE_B,ATTR_NULL,TRUE);
		return GetHasStyle(type,ATTR_NULL,TRUE);
	}

	return FALSE;
}

OP_STATUS OpDocumentEdit::queryCommandSupported(OP_DOCUMENT_EDIT_COMMAND command, URL& origin_url)
{
	DEBUG_CHECKER(TRUE);
	if (command == OP_DOCUMENT_EDIT_COMMAND_UNKNOWN)
		return OpStatus::ERR_NOT_SUPPORTED;

	if ((command == OP_DOCUMENT_EDIT_COMMAND_CUT ||
		command == OP_DOCUMENT_EDIT_COMMAND_COPY ||
		command == OP_DOCUMENT_EDIT_COMMAND_PASTE) && !DOM_Utils::IsClipboardAccessAllowed(origin_url, GetDoc()->GetWindow()))
	{
		return OpStatus::ERR_NO_ACCESS;
	}

	return OpStatus::OK;
}

OP_STATUS MakeColor(UINT32 col, TempBuffer& value)
{
	DEBUG_CHECKER_STATIC();
	col = HTM_Lex::GetColValByIndex(col);

	uni_char buf[8]; /* ARRAY OK 2009-05-06 stighal */
	RETURN_IF_ERROR(value.Append("rgb("));

	uni_itoa(GetRValue(col), buf, 10);
	RETURN_IF_ERROR(value.Append(buf));

	RETURN_IF_ERROR(value.Append(","));

	uni_itoa(GetGValue(col), buf, 10);
	RETURN_IF_ERROR(value.Append(buf));

	RETURN_IF_ERROR(value.Append(","));

	uni_itoa(GetBValue(col), buf, 10);
	RETURN_IF_ERROR(value.Append(buf));

	RETURN_IF_ERROR(value.Append(")"));

	return OpStatus::OK;
}

OP_STATUS OpDocumentEdit::queryCommandValue(OP_DOCUMENT_EDIT_COMMAND command, TempBuffer& value)
{
	DEBUG_CHECKER(TRUE);
	// FIX: mozilla only returns an empty string instead of the default value
	//      even though the selection coverse several different values

	switch(command)
	{
	case OP_DOCUMENT_EDIT_COMMAND_FORMATBLOCK:
		{
			HTML_Element* containing_elm;
			if (m_caret.GetElement() && (containing_elm = m_doc->GetCaret()->GetContainingElement(m_caret.GetElement())))
			{
				HTML_ElementType type = containing_elm->Type();
				return value.Append(ConvertTypeToFormatBlockValue(type));
			}
			else
				return value.Append(UNI_L(""));
		}
	case OP_DOCUMENT_EDIT_COMMAND_FONTNAME:
		{
		return GetFontFace(value);
		}
	case OP_DOCUMENT_EDIT_COMMAND_FONTSIZE:
		{
			return value.AppendLong(GetFontSize());
		}
	case OP_DOCUMENT_EDIT_COMMAND_FORECOLOR:
		{
			HTML_Element* root = m_caret.GetElement();
			UINT32 color = OP_RGB(0, 0, 0);
			if (root)
			{
				Head prop_list;
				HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
				LayoutProperties* lprops = LayoutProperties::CreateCascade(root, prop_list, hld_profile);
				if (lprops && lprops->GetProps()->font_color != USE_DEFAULT_COLOR)
					color = lprops->GetProps()->font_color;
				prop_list.Clear();
			}

			return MakeColor(color, value);
		}
		break;
#ifdef SPELLCHECK_COMMAND_SUPPORT
	case OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_DEBUG:
		{
			OpString_list list;
			RETURN_IF_ERROR(g_internal_spellcheck->GetInstalledLanguages(list));
			for(int i=0;i<(int)list.Count();i++)
			{
				RETURN_IF_ERROR(value.Append(list.Item(i)));
				if(i != (int)(list.Count()-1))
					RETURN_IF_ERROR(value.Append(UNI_L(",")));
			}
		}
		break;
#endif // SPELLCHECK_COMMAND_SUPPORT
	case OP_DOCUMENT_EDIT_COMMAND_HILITECOLOR:
	case OP_DOCUMENT_EDIT_COMMAND_BACKCOLOR:
		{
			if(!m_caret.GetElement())
				return value.Append(UNI_L(""));
			HTML_Element* containing_elm = m_doc->GetCaret()->GetContainingElement(m_caret.GetElement());
			HTML_Element* root = containing_elm;
			if (command == OP_DOCUMENT_EDIT_COMMAND_HILITECOLOR)
			{
				root = m_caret.GetElement();
				while(root && root != containing_elm && root->Type() != HE_SPAN /* || HasBackgroundColor(root) */)
					root = root->Parent();
			}

			UINT32 bg_color = OP_RGB(255,255,255);
			if (root)
			{
				Head prop_list;
				HLDocProfile *hld_profile = m_doc->GetHLDocProfile();
				LayoutProperties* lprops = LayoutProperties::CreateCascade(root, prop_list, hld_profile);
				if (lprops && lprops->GetProps()->bg_color != USE_DEFAULT_COLOR)
					bg_color = lprops->GetProps()->bg_color;
				prop_list.Clear();
			}

			return MakeColor(bg_color, value);
		}
		break;
	case OP_DOCUMENT_EDIT_COMMAND_DEFAULT_BLOCK:
		value.Append(HTM_Lex::GetTagString(m_paragraph_element_type));
		break;
	}
	return OpStatus::OK;
}

#endif // DOCUMENT_EDIT_SUPPORT
