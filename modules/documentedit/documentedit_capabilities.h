/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCUMENTEDIT_CAPABILITIES_H
#define DOCUMENTEDIT_CAPABILITIES_H

// Has OpDocumentEdit::OnElementDeleted
#define DOCUMENTEDIT_CAP_ONELEMENTDELETED

// OpDocumentEdit::execCommand returns BOOL
#define DOCUMENTEDIT_CAP_EXECCOMMAND_RET

// Has OpDocumentEditLayoutModifier
#define DOCUMENTEDIT_CAP_LAYOUTMODIFIER

// Has GetCursorType
#define DOCUMENTEDIT_CAP_GETCURSORTYPE

// Has GetPoint
#define DOCUMENTEDIT_CAP_GETTEXTSELPOINT

// Has GetSelection
#define DOCUMENTEDIT_CAP_GETSELECTION

// supports contentEditable
#define DOCUMENTEDIT_CAP_CONTENTEDITABLE

// OpDocumentEdit is a OpInputContext
#define DOCUMENTEDIT_CAP_ISINPUTCONTEXT

// Using LayoutProperties::GetProps to access HTMLayoutProperties. For cached props
#define DOCUMENTEDIT_CAP_USE_LAYOUTPROPERTIES_GETPROPS

// The OpDocumentEditListener has the OnTextChanged() method
#define DOCEDIT_CAP_HAS_ONTEXTCHANGED

// The documentedit code can handle that ClearSelection(TRUE) removes
// focus.
#define DOCEDIT_CAP_SURVIVES_CLEARSELECTION

// OpDocumentEdit::OnElementOut exists
#define DOCEDIT_CAP_HAS_ONBEFOREELEMENTOUT

// OpDocumentEdit::OnTextChanged exists
#define DOCEDIT_CAP_HAS_DOCEDIT_ONTEXTCHANGED

// Supports spellchecking
#define DOCEDIT_CAP_SPELLCHECKING

// OpDocumentEdit::OnTextElmGetsLayoutWords exists
#define DOCEDIT_CAP_ON_TEXT_ELM_GETS_LAYOUT_WORDS

// Supports ASCII key strings for actions/menus/dialogs/skin
#define DOCUMENTEDIT_CAP_INI_KEYS_ASCII

// The documentedit module understands that background properties has moved
#define DOCUMENTEDIT_CAP_CSS3_BACKGROUND

// passes script to GetFirstFontNumber (logdoc_util)
#define DOCEDIT_CAP_GETFIRSTFONTNUMBER_SCRIPT

// Has new GetFontFace(OpString) method in OpDocumentEdit
#define DOCEDIT_CAP_HASIMPROVEDGETFONTFACE

// GetText has new parameter block_quotes_as_text.
#define DOCEDIT_CAP_BLOCKQUOTES_AS_TEXT

// Has the OpDocumentEdit::CleanReferencesToElement() method
#define DOCEDIT_CAP_HAS_CLEANREFERENCES

// Has GetTextHTMLFromNamedElement() and DeleteNamedElement() in OpDocumentEdit
#define DOCEDIT_CAP_HAS_DELETE_NAMED_ELEMENT

#endif // !DOCUMENTEDIT_CAPABILITIES_H
