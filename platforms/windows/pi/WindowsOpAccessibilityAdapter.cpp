/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "WindowsOpAccessibilityAdapter.h"

#include "modules/unicode/unicode_segmenter.h"

#include "platforms/windows/pi/WindowsOpWindow.h"

#include "platforms/windows/IDL/Accessible2_i.c"
#include "platforms/windows/IDL/AccessibleText_i.c"
#include "platforms/windows/IDL/AccessibleRole.h"

//Table indicating the MSAA equivalent role of each of our accessibility roles
DWORD WindowsOpAccessibilityAdapter::IAccRole[] = {
		ROLE_SYSTEM_APPLICATION,		//kElementKindApplication
		ROLE_SYSTEM_CLIENT,				//kElementKindWindow
		ROLE_SYSTEM_ALERT,				//kElementKindAlert
		ROLE_SYSTEM_DIALOG,				//kElementKindAlertDialog
		ROLE_SYSTEM_DIALOG,				//kElementKindDialog
		ROLE_SYSTEM_GROUPING,			//kElementKindView
		ROLE_SYSTEM_DOCUMENT,			//kElementKindWebView
		ROLE_SYSTEM_TOOLBAR,			//kElementKindToolbar
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindButton
		ROLE_SYSTEM_CHECKBUTTON,		//kElementKindCheckbox
		ROLE_SYSTEM_COMBOBOX,			//kElementKindDropdown
		ROLE_SYSTEM_GROUPING,			//kElementKindRadioTabGroup
		ROLE_SYSTEM_RADIOBUTTON,		//kElementKindRadioButton
		ROLE_SYSTEM_PAGETAB,			//kElementKindTab
		ROLE_SYSTEM_STATICTEXT,			//kElementKindStaticText
		ROLE_SYSTEM_TEXT,				//kElementKindSinglelineTextedit
		ROLE_SYSTEM_TEXT,				//kElementKindMultilineTextedit
		ROLE_SYSTEM_SCROLLBAR,			//kElementKindHorizontalScrollbar
		ROLE_SYSTEM_SCROLLBAR,			//kElementKindVerticalScrollbar
		ROLE_SYSTEM_CLIENT,				//kElementKindScrollview
		ROLE_SYSTEM_SLIDER,				//kElementKindSlider
		ROLE_SYSTEM_PROGRESSBAR,		//kElementKindProgress
		ROLE_SYSTEM_STATICTEXT,			//kElementKindLabel
		ROLE_SYSTEM_STATICTEXT,			//kElementKindDescriptor
		ROLE_SYSTEM_LINK,				//kElementKindLink
		ROLE_SYSTEM_GRAPHIC,			//kElementKindImage
		ROLE_SYSTEM_MENUBAR,			//kElementKindMenuBar
		ROLE_SYSTEM_MENUPOPUP,			//kElementKindMenu
		ROLE_SYSTEM_MENUITEM,			//kElementKindMenuItem
		ROLE_SYSTEM_LIST,				//kElementKindList
		ROLE_SYSTEM_TABLE,				//kElementKindGrid
		ROLE_SYSTEM_OUTLINE,			//kElementKindOutlineList
		ROLE_SYSTEM_TABLE,				//kElementKindTable
		ROLE_SYSTEM_LIST,				//kElementKindHeaderList
		ROLE_SYSTEM_LISTITEM,			//kElementKindListRow
		ROLE_SYSTEM_COLUMNHEADER,		//kElementKindListHeader
		ROLE_SYSTEM_COLUMN,				//kElementKindListColumn
		ROLE_SYSTEM_CELL,				//kElementKindListCell
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartArrowUp
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartArrowDown
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartPageUp
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartPageDown
		ROLE_SYSTEM_INDICATOR,			//kElementKindScrollbarPartKnob
		ROLE_SYSTEM_BUTTONDROPDOWN,		//kElementKindDropdownButtonPart
		ROLE_SYSTEM_LIST,				//kElementKindDirectory
		ROLE_SYSTEM_DOCUMENT,			//kElementKindDocument
		ROLE_SYSTEM_DOCUMENT,			//kElementKindLog
		ROLE_SYSTEM_PANE,				//kElementKindPanel
		ROLE_SYSTEM_CLIENT,				//kElementKindWorkspace
		ROLE_SYSTEM_SEPARATOR,			//kElementKindSplitter
		ROLE_SYSTEM_GROUPING,			//kElementKindSection
		ROLE_SYSTEM_GROUPING,			//kElementKindParagraph
		ROLE_SYSTEM_GROUPING,			//kElementKindForm
		0,			//kElementKindUnknown
	};

//Table indicating the IAccessible2 equivalent role of each of our accessibility roles
DWORD WindowsOpAccessibilityAdapter::IAcc2Role[] = {
		ROLE_SYSTEM_APPLICATION,		//kElementKindApplication
		ROLE_SYSTEM_CLIENT,				//kElementKindWindow
		ROLE_SYSTEM_ALERT,				//kElementKindAlert
		ROLE_SYSTEM_ALERT,				//kElementKindAlertDialog
		ROLE_SYSTEM_DIALOG,				//kElementKindDialog
		ROLE_SYSTEM_GROUPING,			//kElementKindView
		ROLE_SYSTEM_DOCUMENT,			//kElementKindWebView
		ROLE_SYSTEM_TOOLBAR,			//kElementKindToolbar
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindButton
		ROLE_SYSTEM_CHECKBUTTON,		//kElementKindCheckbox
		ROLE_SYSTEM_COMBOBOX,			//kElementKindDropdown
		ROLE_SYSTEM_GROUPING,			//kElementKindRadioTabGroup
		ROLE_SYSTEM_RADIOBUTTON,		//kElementKindRadioButton
		ROLE_SYSTEM_PAGETAB,			//kElementKindTab
		ROLE_SYSTEM_STATICTEXT,			//kElementKindStaticText
		ROLE_SYSTEM_TEXT,				//kElementKindSinglelineTextedit
		ROLE_SYSTEM_TEXT,				//kElementKindMultilineTextedit
		ROLE_SYSTEM_SCROLLBAR,			//kElementKindHorizontalScrollbar
		ROLE_SYSTEM_SCROLLBAR,			//kElementKindVerticalScrollbar
		ROLE_SYSTEM_CLIENT,				//kElementKindScrollview
		ROLE_SYSTEM_SLIDER,				//kElementKindSlider
		ROLE_SYSTEM_PROGRESSBAR,		//kElementKindProgress
		IA2_ROLE_LABEL,					//kElementKindLabel
		ROLE_SYSTEM_STATICTEXT,			//kElementKindDescriptor
		ROLE_SYSTEM_LINK,				//kElementKindLink
		ROLE_SYSTEM_GRAPHIC,			//kElementKindImage
		ROLE_SYSTEM_MENUBAR,			//kElementKindMenuBar
		ROLE_SYSTEM_MENUPOPUP,			//kElementKindMenu
		ROLE_SYSTEM_MENUITEM,			//kElementKindMenuItem
		ROLE_SYSTEM_LIST,				//kElementKindList
		ROLE_SYSTEM_TABLE,				//kElementKindGrid
		ROLE_SYSTEM_OUTLINE,			//kElementKindOutlineList
		ROLE_SYSTEM_TABLE,				//kElementKindTable
		ROLE_SYSTEM_LIST,				//kElementKindHeaderList
		ROLE_SYSTEM_LISTITEM,			//kElementKindListRow
		ROLE_SYSTEM_COLUMNHEADER,		//kElementKindListHeader
		ROLE_SYSTEM_COLUMN,				//kElementKindListColumn
		ROLE_SYSTEM_CELL,				//kElementKindListCell
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartArrowUp
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartArrowDown
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartPageUp
		ROLE_SYSTEM_PUSHBUTTON,			//kElementKindScrollbarPartPageDown
		ROLE_SYSTEM_INDICATOR,			//kElementKindScrollbarPartKnob
		ROLE_SYSTEM_BUTTONDROPDOWN,		//kElementKindDropdownButtonPart
		ROLE_SYSTEM_LIST,				//kElementKindDirectory
		ROLE_SYSTEM_DOCUMENT,			//kElementKindDocument
		ROLE_SYSTEM_DOCUMENT,			//kElementKindLog
		ROLE_SYSTEM_PANE,				//kElementKindPanel
		IA2_ROLE_DESKTOP_PANE,			//kElementKindWorkspace
		ROLE_SYSTEM_SEPARATOR,			//kElementKindSplitter
		IA2_ROLE_SECTION,				//kElementKindSection
		IA2_ROLE_PARAGRAPH,				//kElementKindParagraph
		IA2_ROLE_FORM,					//kElementKindForm
		IA2_ROLE_UNKNOWN,				//kElementKindUnknown
	};


//Table indicating which MSAA state should be always set for each of our accessibility roles
DWORD WindowsOpAccessibilityAdapter::IAccStateMask[] = {
		0,							//kElementKindApplication
		STATE_SYSTEM_FOCUSABLE,		//kElementKindWindow
		0,							//kElementKindAlert
		0,							//kElementKindAlertDialog
		0,							//kElementKindDialog
		0,							//kElementKindView
		0,							//kElementKindWebView
		0,							//kElementKindToolbar
		STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_HOTTRACKED,		//kElementKindButton
		STATE_SYSTEM_SELECTABLE | STATE_SYSTEM_HOTTRACKED,		//kElementKindCheckbox
		STATE_SYSTEM_HOTTRACKED,	//kElementKindDropdown
		0,							//kElementKindRadioTabGroup
		STATE_SYSTEM_FOCUSABLE,		//kElementKindRadioButton
		STATE_SYSTEM_SELECTABLE | STATE_SYSTEM_HOTTRACKED,		//kElementKindTab
		0,							//kElementKindStaticText
		STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_HOTTRACKED,		//kElementKindSinglelineTextedit
		0,							//kElementKindMultilineTextedit
		0,							//kElementKindHorizontalScrollbar
		0,							//kElementKindVerticalScrollbar
		0,							//kElementKindScrollview
		STATE_SYSTEM_HOTTRACKED,	//kElementKindSlider
		0,							//kElementKindProgress
		0,							//kElementKindLabel
		0,							//kElementKindDescriptor
		0,							//kElementKindLink
		0,							//kElementKindImage
		0,							//kElementKindMenuBar
		0,							//kElementKindMenu
		STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_HOTTRACKED |STATE_SYSTEM_HASPOPUP,	//kElementKindMenuItem
		0,							//kElementKindList
		0,							//kElementKindGrid
		0,							//kElementKindOutlineList
		0,							//kElementKindTable
		0,							//kElementKindHeaderList
		STATE_SYSTEM_SELECTABLE,	//kElementKindListRow
		STATE_SYSTEM_READONLY,		//kElementKindListHeader
		STATE_SYSTEM_SELECTABLE,	//kElementKindListColumn
		STATE_SYSTEM_SELECTABLE,	//kElementKindListCell
		0,							//kElementKindScrollbarPartArrowUp
		0,							//kElementKindScrollbarPartArrowDown
		0,							//kElementKindScrollbarPartPageUp
		0,							//kElementKindScrollbarPartPageDown
		0,							//kElementKindScrollbarPartKnob
		STATE_SYSTEM_HOTTRACKED,	//kElementKindDropdownButtonPart
		0,							//kElementKindDirectory
		0,							//kElementKindDocument
		0,							//kElementKindLog
		0,							//kElementKindPanel
		0,							//kElementKindWorkspace
		0,							//kElementKindSplitter
		0,							//kElementKindSection
		0,							//kElementKindParagraph
		0							//kElementKindUnknown
	};

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
int WindowsOpAccessibilityExtension::s_last_id = 0;
#endif

OP_STATUS OpAccessibilityAdapter::Create(OpAccessibilityAdapter** adapter, OpAccessibleItem* accessible_item)
{
	*adapter = (OpAccessibilityAdapter*)(new WindowsOpAccessibilityAdapter(accessible_item));
	if(*adapter == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

WindowsOpWindow* WindowsOpAccessibilityAdapter::GetWindowForAccessibleItem(OpAccessibleItem* item)
{
	WindowsOpWindow* window = NULL;
	while ((!item->AccessibilityIsReady() || (window = (WindowsOpWindow*)(item->AccessibilityGetWindow())) == NULL) && (item = item->GetAccessibleParent()) != NULL);

	return window;
}

// maps OpAccessibleItem instances to a unique id
//OpPointerHashTable<OpAccessibleItem, LONG> g_accessibleitem_id_hashtable;

LONG GetAccessibleItemID(OpAccessibleItem* item)
{
	return (LONG)((LONG_PTR)item) >> 2;
/*
	LONG* id = 0;

	if(OpStatus::IsSuccess(g_accessibleitem_id_hashtable.GetData(item, &id)))
	{
		return (LONG)(LONG_PTR)id;
	}
	id = (LONG *)++g_unique_id_counter;

	g_accessibleitem_id_hashtable.Add(item, id);

	return (LONG)(LONG_PTR)id;
*/
}

void FreeAccessibleItemID(OpAccessibleItem* item)
{
/*
	LONG* id;

	if(g_accessibleitem_id_hashtable.Contains(item))
		g_accessibleitem_id_hashtable.Remove(item, &id);
*/
}

OP_STATUS OpAccessibilityAdapter::SendEvent(OpAccessibleItem* sender, Accessibility::Event evt)
{
	if (!g_op_system_info->IsScreenReaderRunning() && evt.is_state_event)
		return OpStatus::OK;

	WindowsOpWindow* window = WindowsOpAccessibilityAdapter::GetWindowForAccessibleItem(sender);

	if (window == NULL)
		return OpStatus::ERR;

	if (evt.is_state_event)
	{
		LONG id = GetAccessibleItemID(sender);

		Accessibility::State state = sender->AccessibilityGetState();
		if (evt.event_info.state_event & Accessibility::kAccessibilityStateInvisible && !(state & Accessibility::kAccessibilityStateInvisible))
			NotifyWinEvent(EVENT_OBJECT_SHOW, window->m_hwnd, id, CHILDID_SELF);
		if (evt.event_info.state_event & Accessibility::kAccessibilityStateInvisible && state & Accessibility::kAccessibilityStateInvisible)
			NotifyWinEvent(EVENT_OBJECT_HIDE, window->m_hwnd, id, CHILDID_SELF);
		if (evt.event_info.state_event & Accessibility::kAccessibilityStateFocused && state & Accessibility::kAccessibilityStateFocused)
			NotifyWinEvent(EVENT_OBJECT_FOCUS, window->m_hwnd, id, CHILDID_SELF);

		NotifyWinEvent(EVENT_OBJECT_STATECHANGE, window->m_hwnd, id, CHILDID_SELF);
		return OpStatus::OK;

	}

	switch (evt.event_info.event_type)
	{
		case Accessibility::kAccessibilityEventMoved:
		case Accessibility::kAccessibilityEventResized:
			NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventTextChanged:
		{
			Accessibility::ElementKind role = sender->AccessibilityGetRole();
			if (role == Accessibility::kElementKindSinglelineTextedit ||
				role == Accessibility::kElementKindMultilineTextedit ||
				role == Accessibility::kElementKindDropdown)
				NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			else
				NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;
		}

		case Accessibility::kAccessibilityEventTextChangedBykeyboard:
			NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventDescriptionChanged:
			NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventURLChanged:
			break;

		case Accessibility::kAccessibilityEventSelectedTextRangeChanged:
		case Accessibility::kAccessibilityEventSelectionChanged:
			NotifyWinEvent(EVENT_OBJECT_SELECTIONWITHIN, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventSetLive:
			break;

		case Accessibility::kAccessibilityEventStartDragAndDrop:
			NotifyWinEvent(EVENT_SYSTEM_DRAGDROPSTART, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventEndDragAndDrop:
			NotifyWinEvent(EVENT_SYSTEM_DRAGDROPEND, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventStartScrolling:
			NotifyWinEvent(EVENT_SYSTEM_SCROLLINGSTART, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventEndScrolling:
			NotifyWinEvent(EVENT_SYSTEM_SCROLLINGEND, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

		case Accessibility::kAccessibilityEventReorder:
			NotifyWinEvent(EVENT_OBJECT_REORDER, window->m_hwnd, GetAccessibleItemID(sender), CHILDID_SELF);
			break;

	}

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_info;
	GetDebugInfo(dbg_info);
	OutputDebugString(dbg_info);
	dbg_info.Empty();
	dbg_info.AppendFormat(UNI_L(": sent event %d\n"),event_constant);
	OutputDebugString(dbg_info);
#endif

	return OpStatus::OK;
}

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
void WindowsOpAccessibilityExtension::GetDebugInfo(OpString &info)
{
	if (!m_listener)
		return;
	OpString name;
	m_listener->AccessibilityGetText(name);
	if (name.Length() > 15)
		*(name.CStr() + 16) = 0;

	OpAccessibilityExtension::AccessibilityState state = m_listener->AccessibilityGetState();

	OpString status;
	status.Set(UNI_L(""));
	if (state & OpAccessibilityExtension::kAccessibilityStateInvisible)
		status.Append(UNI_L("H"));
	if (state & OpAccessibilityExtension::kAccessibilityStateDisabled)
		status.Append(UNI_L("D"));
	if (state & OpAccessibilityExtension::kAccessibilityStateCheckedOrSelected)
		status.Append(UNI_L("S"));
	if (state & OpAccessibilityExtension::kAccessibilityStateReadOnly)
		status.Append(UNI_L("R"));

	info.Empty();
	if (m_IAccessible)
		info.AppendFormat(UNI_L("(Acc #%d {%d} %s - \"%s\")"), m_IAccessible->m_id, m_listener->AccessibilityGetRole(), status.CStr(), name.CStr());
	else
		info.AppendFormat(UNI_L("(Acc #N/A {%d} %s - \"%s\")"), m_listener->AccessibilityGetRole(), status.CStr(), name.CStr());
}
#endif

HRESULT WindowsOpAccessibilityAdapter::QueryService(REFGUID guidService, REFIID riid, void **ppv)
{
	if (guidService != IID_IAccessible && guidService != IID_IAccessible2 && guidService != IID_IAccessibleText)
		return E_INVALIDARG;

	return QueryInterface(riid, ppv);
}

HRESULT WindowsOpAccessibilityAdapter::QueryInterface(REFIID iid, void ** pvvObject)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	if (iid == IID_IUnknown || iid == IID_IDispatch || iid == IID_IAccessible || iid == IID_IAccessible2)
	{
		*pvvObject=(IAccessible2*)this;
		AddRef();
		return S_OK;
	}
	else if (iid == IID_IAccessibleText)
	{
		OpString text;
		if (OpStatus::IsSuccess(m_accessible_item->AccessibilityGetText(text)) && (text.HasContent() || m_accessible_item->AccessibilityGetRole() == Accessibility::kElementKindSinglelineTextedit || m_accessible_item->AccessibilityGetRole() == Accessibility::kElementKindMultilineTextedit ) )
		{
			*pvvObject=(IAccessibleText*)this;
			AddRef();
			return S_OK;
		}
	}
	else if (iid == IID_IServiceProvider)
	{
		*pvvObject=(IServiceProvider*)this;
		AddRef();
		return S_OK;
	}

	*pvvObject=NULL;
	return E_NOINTERFACE;
}

ULONG WindowsOpAccessibilityAdapter::AddRef()
{
/*#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_msg;
	if (!m_accessibility_extension)
		dbg_msg.AppendFormat(UNI_L("(Acc #%d )"), m_id);
	else
		m_accessibility_extension->GetDebugInfo(dbg_msg);
	dbg_msg.AppendFormat(UNI_L(": AddRef - %d -> %d\n"), m_ref_counter, m_ref_counter+1);

	OutputDebugString(dbg_msg.CStr());
#endif*/

	return ++m_ref_counter;
}

ULONG WindowsOpAccessibilityAdapter::Release()
{
/*#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_msg;
	if (!m_accessibility_extension)
		dbg_msg.AppendFormat(UNI_L("(Acc #%d )"), m_id);
	else
		m_accessibility_extension->GetDebugInfo(dbg_msg);
	dbg_msg.AppendFormat(UNI_L(": Release - %d -> %d\n"), m_ref_counter, m_ref_counter-1);

	OutputDebugString(dbg_msg.CStr());
#endif*/

	if (!(--m_ref_counter))
	{
		OP_ASSERT(!m_accessible_item);
		if (!m_accessible_item)
			delete this;
		return 0;
	}
	return m_ref_counter;
}

HRESULT WindowsOpAccessibilityAdapter::GetTypeInfoCount(unsigned int FAR* pctinfo)
{
	*pctinfo=0;
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::GetTypeInfo(unsigned int iTInfo, LCID  lcid, ITypeInfo FAR* FAR* ppTInfo)
{
	*ppTInfo=0;
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::GetIDsOfNames(REFIID riid, OLECHAR FAR* FAR* rgszNames, unsigned int cNames, 
						LCID lcid, DISPID FAR*  rgDispId)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, 
				DISPPARAMS FAR* pDispParams, VARIANT FAR* pVarResult, 
				EXCEPINFO FAR* pExcepInfo, unsigned int FAR* puArgErr)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_accName(VARIANT varID, BSTR* pszName)
{
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_msg;
	if (!m_accessibility_extension)
		dbg_msg.AppendFormat(UNI_L("(Acc #%d )"), m_id);
	else
		m_accessibility_extension->GetDebugInfo(dbg_msg);
	dbg_msg.AppendFormat(UNI_L(": get_accName - "), m_ref_counter, m_ref_counter-1);

#endif

	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* acc_dest;

	if (varID.lVal==CHILDID_SELF)
		acc_dest=m_accessible_item;
	else
	{
		acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
	}

	OpAccessibleItem* acc_label= m_accessible_item->GetAccessibleLabelForControl();

	OpString string;
	*pszName = NULL;

	if (acc_label)
	{
		if (!acc_label->AccessibilityIsReady())
			return E_FAIL;
		if (OpStatus::IsError(acc_label->AccessibilityGetText(string)))
			return S_FALSE;
	}
	else
	{
		Accessibility::ElementKind role = acc_dest->AccessibilityGetRole();

		OpAccessibleItem* acc_parent = ReadyOrNull(acc_dest->GetAccessibleParent());

		if (role == Accessibility::kElementKindListRow && acc_parent && acc_parent->AccessibilityGetRole() != Accessibility::kElementKindTable)
		{
			acc_dest->AccessibilityGetText(string);
			int n = acc_dest->GetAccessibleChildrenCount();
			for (int i = 0; i < n; i++)
			{
				OpAccessibleItem* child = ReadyOrNull(acc_dest->GetAccessibleChild(i));
				OpString text;
				if (child)
				{
					child->AccessibilityGetText(text);
					if (text.HasContent())
					{
						if (string.HasContent())
							string.Append(UNI_L("  "));
						string.Append(text);
					}
				}
			}
		}

		else if (role == Accessibility::kElementKindSinglelineTextedit ||
			role == Accessibility::kElementKindMultilineTextedit ||
			role == Accessibility::kElementKindSection ||
			role == Accessibility::kElementKindParagraph
			)
		{
			return S_FALSE;
		}

		else if (OpStatus::IsError(acc_dest->AccessibilityGetText(string)))
			return S_FALSE;
	}

	*pszName = SysAllocString(string.CStr());
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	dbg_msg.AppendFormat(UNI_L("%s\n"), string.CStr());

	OutputDebugString(dbg_msg.CStr());
#endif

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accRole(VARIANT varID, VARIANT* pvarRole)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* acc_dest;

	if (varID.lVal==CHILDID_SELF)
		acc_dest=m_accessible_item;
	else
	{
		acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
	}

	VariantInit(pvarRole);
	pvarRole->vt=VT_I4;
	Accessibility::ElementKind role = acc_dest->AccessibilityGetRole();

	OpAccessibleItem* acc_parent = ReadyOrNull(acc_dest->GetAccessibleParent());
	OpAccessibleItem* first_child = NULL;
	if (m_accessible_item->GetAccessibleChildrenCount() >= 1)
		first_child = ReadyOrNull(m_accessible_item->GetAccessibleChild(0));

	//Groups of tabs have a specific role
	if (role == Accessibility::kElementKindWindow && acc_dest->AccessibilityGetWindow() && ((WindowsOpWindow*)acc_dest->AccessibilityGetWindow())->IsDialog())
		pvarRole->lVal = ROLE_SYSTEM_DIALOG;
	else if (role == Accessibility::kElementKindWindow && acc_dest->AccessibilityGetWindow() && ((WindowsOpWindow*)acc_dest->AccessibilityGetWindow())->m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
		pvarRole->lVal = ROLE_SYSTEM_APPLICATION;
	else if (role == Accessibility::kElementKindView && acc_parent && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindTab)
		pvarRole->lVal = ROLE_SYSTEM_PROPERTYPAGE;
	else if (role == Accessibility::kElementKindRadioTabGroup && first_child && first_child->AccessibilityGetRole() == Accessibility::kElementKindTab)
		pvarRole->lVal = ROLE_SYSTEM_PAGETABLIST;
	else if (role == Accessibility::kElementKindListRow && acc_parent && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindOutlineList)
		pvarRole->lVal = ROLE_SYSTEM_OUTLINEITEM;
	else if (role == Accessibility::kElementKindListRow && acc_parent && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindTable)
		pvarRole->lVal = ROLE_SYSTEM_ROW;
	else 
		pvarRole->lVal = WindowsOpAccessibilityAdapter::IAccRole[role];

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accState(VARIANT varID, VARIANT* pvarState)
{
	Accessibility::State state;
	Accessibility::ElementKind role;

	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
	{
		pvarState->lVal= STATE_SYSTEM_UNAVAILABLE;
		return S_OK;
	}

	VariantInit(pvarState);
	pvarState->vt=VT_I4;
	if (varID.lVal==CHILDID_SELF)
	{
		state = m_accessible_item->AccessibilityGetState();
		role = m_accessible_item->AccessibilityGetRole();
	}
	else
	{
		OpAccessibleItem* acc_child = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_child)
			return E_INVALIDARG;
		if (!acc_child->AccessibilityIsReady())
		{
			pvarState->lVal= STATE_SYSTEM_UNAVAILABLE;
			return S_OK;
		}
		state = acc_child->AccessibilityGetState();
		role = acc_child->AccessibilityGetRole();
	}

	pvarState->lVal = WindowsOpAccessibilityAdapter::IAccStateMask[role];

	if (state & Accessibility::kAccessibilityStateFocused)
		pvarState->lVal |= STATE_SYSTEM_FOCUSED;
	if (state & Accessibility::kAccessibilityStateDisabled)
		pvarState->lVal |= STATE_SYSTEM_UNAVAILABLE;
	if (state & Accessibility::kAccessibilityStateInvisible)
		pvarState->lVal |= STATE_SYSTEM_INVISIBLE;
	if (state & Accessibility::kAccessibilityStateCheckedOrSelected)
		if (role == Accessibility::kElementKindCheckbox ||
			role == Accessibility::kElementKindRadioButton)
			pvarState->lVal |= STATE_SYSTEM_CHECKED;
		else if(role == Accessibility::kElementKindButton)
			pvarState->lVal |= STATE_SYSTEM_PRESSED;
		else
			pvarState->lVal |= STATE_SYSTEM_SELECTED;

	if (state & Accessibility::kAccessibilityStateGrayed)
		pvarState->lVal |= STATE_SYSTEM_MIXED;
	if (state & Accessibility::kAccessibilityStateAnimated)
		pvarState->lVal |= STATE_SYSTEM_ANIMATED;
	if (state & Accessibility::kAccessibilityStateExpanded)
		pvarState->lVal |= STATE_SYSTEM_EXPANDED;
	else if (state & Accessibility::kAccessibilityStateExpandable)
		pvarState->lVal |= STATE_SYSTEM_COLLAPSED;
	if (state & Accessibility::kAccessibilityStatePassword)
		pvarState->lVal |= STATE_SYSTEM_PROTECTED;
	if (state & Accessibility::kAccessibilityStateReadOnly)
		pvarState->lVal |= STATE_SYSTEM_READONLY;
	if (state & Accessibility::kAccessibilityStateTraversedLink)
		pvarState->lVal |= STATE_SYSTEM_TRAVERSED;
	if (state & Accessibility::kAccessibilityStateDefaultButton)
		pvarState->lVal |= STATE_SYSTEM_DEFAULT;
	if (state & Accessibility::kAccessibilityStateMultiple)
		pvarState->lVal |= STATE_SYSTEM_MIXED;
	if (state & Accessibility::kAccessibilityStateOffScreen)
		pvarState->lVal |= STATE_SYSTEM_OFFSCREEN;
	if (state & Accessibility::kAccessibilityStateBusy)
		pvarState->lVal |= STATE_SYSTEM_BUSY;

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::accLocation(long* pxLeft, long* pyTop, 
										long* pcxWidth, long* pcyHeight, VARIANT varID)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpRect position;
	if (varID.lVal==CHILDID_SELF)
	{
		if (OpStatus::IsError(m_accessible_item->AccessibilityGetAbsolutePosition(position)))
			return DISP_E_MEMBERNOTFOUND;
		*pxLeft = position.x;
		*pyTop = position.y;
		*pcxWidth = position.width;
		*pcyHeight = position.height;
	}
	else
	{
		OpAccessibleItem* acc_child = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_child)
			return E_INVALIDARG;
		if (!acc_child->AccessibilityIsReady())
			return E_FAIL;

		if (OpStatus::IsError(acc_child->AccessibilityGetAbsolutePosition(position)))
			return DISP_E_MEMBERNOTFOUND;

		*pxLeft = position.x;
		*pyTop = position.y;
		*pcxWidth = position.width;
		*pcyHeight = position.height;
	}

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::accHitTest(long xLeft, long yTop, VARIANT* pvarID)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	VariantInit(pvarID);
	OpAccessibleItem* acc_child = m_accessible_item->GetAccessibleChildOrSelfAt(xLeft, yTop);
	if (!acc_child)
	{
		pvarID->vt=VT_EMPTY;
		return S_FALSE;
	}
	else if (acc_child==m_accessible_item)
	{
		pvarID->vt=VT_I4;
		pvarID->lVal=CHILDID_SELF;
		AddRef();
	}
	else
	{
		pvarID->vt=VT_DISPATCH;
		pvarID->pdispVal=(WindowsOpAccessibilityAdapter*)(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(acc_child));
		pvarID->pdispVal->AddRef();
	}

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accParent(IDispatch** ppdispParent)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;

	OpAccessibleItem* acc_parent = m_accessible_item->GetAccessibleParent();
	if (acc_parent == NULL && m_accessible_item->AccessibilityGetWindow() != NULL)
	{
		HWND hwnd = ((WindowsOpWindow*)(m_accessible_item->AccessibilityGetWindow()))->m_hwnd;
		IAccessible* native_iaccessible;
		AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible, (void**)(&native_iaccessible));
		*ppdispParent= native_iaccessible;
		(*ppdispParent)->AddRef();
	}
	else if (acc_parent == NULL)
	{
		*ppdispParent = NULL;
		return S_FALSE;
	}
	else
	{
		*ppdispParent= (WindowsOpAccessibilityAdapter*)(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(acc_parent));
		(*ppdispParent)->AddRef();
	}

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::accNavigate(long navDir, VARIANT varStart, VARIANT* pvarEnd)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	VariantInit(pvarEnd);
	OpAccessibleItem* start;
	OpAccessibleItem* end= NULL;

	if (varStart.lVal==CHILDID_SELF)
		start = m_accessible_item;
	else
		start = m_accessible_item->GetAccessibleChild(varStart.lVal-1);

	if (!start)
		return E_INVALIDARG;

	switch (navDir)
	{
		case NAVDIR_FIRSTCHILD:
			end= start->GetAccessibleChild(0);
			break;
		case NAVDIR_LASTCHILD:
			end= start->GetAccessibleChild(start->GetAccessibleChildrenCount()-1);
			break;
		case NAVDIR_PREVIOUS:
			end= start->GetPreviousAccessibleSibling();
			break;
		case NAVDIR_NEXT:
			end= start->GetNextAccessibleSibling();
			break;

		case NAVDIR_LEFT:
			end= start->GetLeftAccessibleObject();
			break;
		case NAVDIR_RIGHT:
			end= start->GetRightAccessibleObject();
			break;
		case NAVDIR_UP:
			end= start->GetUpAccessibleObject();
			break;
		case NAVDIR_DOWN:
			end= start->GetDownAccessibleObject();
			break;
	}

	if (!end)
	{
		pvarEnd->vt= VT_EMPTY;
		return S_FALSE;
	}
	else
	{
		pvarEnd->vt= VT_DISPATCH;
		pvarEnd->pdispVal=(WindowsOpAccessibilityAdapter*)AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(end);
		pvarEnd->pdispVal->AddRef();
		return S_OK;
	}
}

HRESULT WindowsOpAccessibilityAdapter::get_accChild(VARIANT varChildID, IDispatch** ppdispChild)
{
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_msg;
	if (!m_accessibility_extension)
		dbg_msg.AppendFormat(UNI_L("(Acc #%d )"), m_id);
	else
		m_accessibility_extension->GetDebugInfo(dbg_msg);
	dbg_msg.AppendFormat(UNI_L(": get_accChild - "), m_ref_counter, m_ref_counter-1);

#endif

	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	*ppdispChild=NULL;
	if (varChildID.vt != VT_I4)
		return E_INVALIDARG;

	if (varChildID.lVal==CHILDID_SELF)
	{
		*ppdispChild=this;
		AddRef();
		return S_OK;
	}

	if (varChildID.lVal>(m_accessible_item->GetAccessibleChildrenCount()) || varChildID.lVal<1)
		return E_INVALIDARG;

	OpAccessibleItem* child = m_accessible_item->GetAccessibleChild(varChildID.lVal-1);
	*ppdispChild=(WindowsOpAccessibilityAdapter*)AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(child);
	(*ppdispChild)->AddRef();

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_info;
	child->GetDebugInfo(dbg_info);
	dbg_msg.AppendFormat(UNI_L("%s\n"), dbg_info.CStr());

	OutputDebugString(dbg_msg.CStr());
#endif
	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accChildCount(long* pcountChildren)
{
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_msg;
	if (!m_accessibility_extension)
		dbg_msg.AppendFormat(UNI_L("(Acc #%d )"), m_id);
	else
		m_accessibility_extension->GetDebugInfo(dbg_msg);
	dbg_msg.AppendFormat(UNI_L(": get_accChildCount - "), m_ref_counter, m_ref_counter-1);

#endif

	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	*pcountChildren = m_accessible_item->GetAccessibleChildrenCount();

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	dbg_msg.AppendFormat(UNI_L("%d\n"), *pcountChildren);

	OutputDebugString(dbg_msg.CStr());
#endif

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accFocus(VARIANT* pvarID)
{
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_msg;
	if (!m_accessibility_extension)
		dbg_msg.AppendFormat(UNI_L("(Acc #%d )"), m_id);
	else
		m_accessibility_extension->GetDebugInfo(dbg_msg);
	dbg_msg.AppendFormat(UNI_L(": get_accFocus - "), m_ref_counter, m_ref_counter-1);

#endif

	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* focused= m_accessible_item->GetAccessibleFocusedChildOrSelf();
	VariantInit(pvarID);

	if (!focused)
	{
		pvarID->vt= VT_EMPTY;
		return S_FALSE;
	}
	else if (focused == m_accessible_item)
	{
		AddRef();
		pvarID->vt= VT_I4;
		pvarID->lVal= CHILDID_SELF;
	}
	else
	{
		pvarID->vt= VT_DISPATCH;
		pvarID->pdispVal= (WindowsOpAccessibilityAdapter*)AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(focused);
		pvarID->pdispVal->AddRef();
	}

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	OpString dbg_info;
	focused->GetDebugInfo(dbg_info);
	dbg_msg.AppendFormat(UNI_L("%s\n"), dbg_info.CStr());

	OutputDebugString(dbg_msg.CStr());
#endif

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accSelection(VARIANT* pvarChildren)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	int children = m_accessible_item->GetAccessibleChildrenCount();
	int selected_count = 0;

	VariantEnum* selected= new VariantEnum();

	for (int i=0;i<children; i++)
	{
		OpAccessibleItem* child = ReadyOrNull(m_accessible_item->GetAccessibleChild(i));
		if (child && child->AccessibilityGetState() & Accessibility::kAccessibilityStateCheckedOrSelected)
		{
			VARIANT* v= new VARIANT;
			VariantInit(v);
			v->vt=VT_DISPATCH;
			((WindowsOpAccessibilityAdapter*)AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(child))->QueryInterface(IID_IAccessible,(void**)&(v->pdispVal));
			v->pdispVal->AddRef();

			selected->Add(v);
			selected_count++;
		}
	}

	VariantInit(pvarChildren);

	if (!selected_count)
	{
		delete selected;
		pvarChildren->vt= VT_EMPTY;
		return VT_EMPTY;
	}
	
	pvarChildren->vt=VT_UNKNOWN;
	pvarChildren->punkVal=selected;

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accDescription(VARIANT varID, BSTR* pszDescription)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* acc_dest;

	if (varID.lVal==CHILDID_SELF)
		acc_dest = m_accessible_item;
	else
	{
		acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);

		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
	}

	OpString string;

//changes the description of radio buttons to "x of n" (similar to what Firefox seems to be doing
/*	if (acc_dest->AccessibilityGetRole() == Accessibility::kElementKindRadioButton)
	{
		string.Set("");
		OpAccessibleItem *acc_dest_parent= acc_dest->GetAccessibleParent();
		if (!acc_dest_parent)
		{
			*pszDescription = NULL;
			return S_FALSE;
		}

		int n=acc_dest_parent->GetAccessibleChildrenCount();
		for (int i=0; i<n ;i++)
			if (acc_dest_parent->GetAccessibleChild(i) == acc_dest)
			{
				string.AppendFormat(UNI_L("%d of %d"), i+1, n);
				break;
			}
	}

	else*/ if(OpStatus::IsError(acc_dest->AccessibilityGetDescription(string)))
	{
		*pszDescription = NULL;
		return S_FALSE;
	}

	*pszDescription = SysAllocString(string.CStr());
	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accValue(VARIANT varID, BSTR* pszValue)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* acc_dest;

	if (varID.lVal==CHILDID_SELF)
		acc_dest = m_accessible_item;
	else
	{
		acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
	}

	Accessibility::ElementKind role = acc_dest->AccessibilityGetRole();

	if (role == Accessibility::kElementKindProgress ||
		role == Accessibility::kElementKindHorizontalScrollbar ||
		role == Accessibility::kElementKindVerticalScrollbar)
	{
		int value;
		if (OpStatus::IsError(acc_dest->AccessibilityGetValue(value)))
			return S_FALSE;

		uni_char str[63];

		uni_itoa(value, str, 10);

		*pszValue = SysAllocString(str);
		return S_OK;

	}

	else if (role == Accessibility::kElementKindSinglelineTextedit ||
		role == Accessibility::kElementKindMultilineTextedit ||
		role == Accessibility::kElementKindDropdown)
	{
		OpString string;
		if (OpStatus::IsError(acc_dest->AccessibilityGetText(string)))
		{
			*pszValue = NULL;
			return S_FALSE;
		}
		*pszValue = SysAllocString(string.CStr());
		return S_OK;
	}
	else
		return S_FALSE;

}

HRESULT WindowsOpAccessibilityAdapter::get_accHelp(VARIANT varID, BSTR* pszHelp)
{
	return E_NOTIMPL;
}
HRESULT WindowsOpAccessibilityAdapter::get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild, long* pidTopic)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_accKeyboardShortcut(VARIANT varID, BSTR* pszKeyboardShortcut)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* acc_dest;

	if (varID.lVal==CHILDID_SELF)
		acc_dest = m_accessible_item;
	else
	{
		acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
	}

	uni_char key;
	ShiftKeyState shifts;

	if(OpStatus::IsError(acc_dest->AccessibilityGetKeyboardShortcut(&shifts, &key)))
	{
		*pszKeyboardShortcut = NULL;
		return S_FALSE;
	}

	OpString shortcut;
	BOOL append_plus = FALSE;

	if (shifts & SHIFTKEY_CTRL)
	{
		shortcut.Append(UNI_L("Ctrl"));
		append_plus= TRUE;
	}
	if (shifts & SHIFTKEY_ALT)
	{
		if (append_plus)
			shortcut.Append(UNI_L("+"));
		shortcut.Append(UNI_L("Alt"));
		append_plus= TRUE;
	}
	if (shifts & SHIFTKEY_SHIFT)
	{
		if (append_plus)
			shortcut.Append(UNI_L("+"));
		shortcut.Append(UNI_L("Shift"));
		append_plus= TRUE;
	}
	if (key)
	{
		if (append_plus)
			shortcut.Append(UNI_L("+"));
		shortcut.Append(&key,1);
	}
	
	*pszKeyboardShortcut = SysAllocString(shortcut.CStr());

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_accDefaultAction(VARIANT varID, BSTR* pszDefaultAction)
{
	return E_NOTIMPL;
}


HRESULT WindowsOpAccessibilityAdapter::accSelect(long flagsSelect, VARIANT varID)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpAccessibleItem* acc_dest;

	if (varID.lVal==CHILDID_SELF)
		acc_dest = m_accessible_item;
	else
	{
		acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
	}

	Accessibility::SelectionSet flags = 0;

	if (flagsSelect & SELFLAG_TAKEFOCUS)
		acc_dest->AccessibilitySetFocus();
		
	if (flagsSelect & SELFLAG_TAKESELECTION)
		flags |= Accessibility::kSelectionSetSelect;
	if (flagsSelect & SELFLAG_EXTENDSELECTION)
		flags |= Accessibility::kSelectionSetExtend;
	if (flagsSelect & SELFLAG_ADDSELECTION)
		flags |= Accessibility::kSelectionSetAdd;
	if (flagsSelect & SELFLAG_REMOVESELECTION)
		flags |= Accessibility::kSelectionSetRemove;

	if (flags)
	{
		OpAccessibleItem* acc_parent = ReadyOrNull(acc_dest->GetAccessibleParent());
		if (acc_parent && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindListRow)
		{
			acc_dest = acc_parent;
			acc_parent = ReadyOrNull(acc_dest->GetAccessibleParent());
		}

		if (acc_parent && acc_dest->AccessibilityChangeSelection(flags, acc_dest))
			return S_OK;
		else
			return S_FALSE;
	}

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::accDoDefaultAction(VARIANT varID)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	if (varID.lVal==CHILDID_SELF)
		m_accessible_item->AccessibilityClicked();
	else
	{
		OpAccessibleItem* acc_dest = m_accessible_item->GetAccessibleChild(varID.lVal-1);
		if (!acc_dest)
			return E_INVALIDARG;
		if (!acc_dest->AccessibilityIsReady())
			return E_FAIL;
		acc_dest->AccessibilityClicked();
	}

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::put_accValue(VARIANT varID, BSTR pszValue)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::put_accName(VARIANT varID, BSTR pszName)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_nRelations(long *nRelations)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_relation(long relationIndex, IAccessibleRelation **relation)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_relations(long maxRelations, IAccessibleRelation **relations, long *nRelations)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::role(long *role)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	Accessibility::ElementKind int_role = m_accessible_item->AccessibilityGetRole();

	OpAccessibleItem* acc_parent = ReadyOrNull(m_accessible_item->GetAccessibleParent());
	OpAccessibleItem* first_child = NULL;
	if (m_accessible_item->GetAccessibleChildrenCount() >= 1)
		first_child = ReadyOrNull(m_accessible_item->GetAccessibleChild(0));

	//Groups of tabs have a specific role
	if (int_role == Accessibility::kElementKindWindow && m_accessible_item->AccessibilityGetWindow() && ((WindowsOpWindow*)m_accessible_item->AccessibilityGetWindow())->IsDialog())
		(*role) = ROLE_SYSTEM_DIALOG;
	else if (int_role == Accessibility::kElementKindWindow && m_accessible_item->AccessibilityGetWindow() && ((WindowsOpWindow*)m_accessible_item->AccessibilityGetWindow())->m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
		(*role) = IA2_ROLE_FRAME;
	else if (int_role == Accessibility::kElementKindWindow && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindWorkspace)
		(*role) = IA2_ROLE_INTERNAL_FRAME;
	else if (int_role == Accessibility::kElementKindView && m_accessible_item && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindTab)
		(*role) = ROLE_SYSTEM_PROPERTYPAGE;
	else if (int_role == Accessibility::kElementKindRadioTabGroup && first_child && first_child->AccessibilityGetRole() == Accessibility::kElementKindTab)
		(*role) = ROLE_SYSTEM_PAGETABLIST;
	else if (int_role == Accessibility::kElementKindListRow && acc_parent && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindOutlineList)
		(*role) = ROLE_SYSTEM_OUTLINEITEM;
	else if (int_role == Accessibility::kElementKindListRow && acc_parent && acc_parent->AccessibilityGetRole() == Accessibility::kElementKindTable)
		(*role) = ROLE_SYSTEM_ROW;
	else 
		(*role) = WindowsOpAccessibilityAdapter::IAcc2Role[int_role];

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::scrollTo(enum IA2ScrollType scrollType)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	switch (scrollType)
	{
		case IA2_SCROLL_TYPE_TOP_LEFT:
		case IA2_SCROLL_TYPE_TOP_EDGE:
		case IA2_SCROLL_TYPE_LEFT_EDGE:	
			m_accessible_item->AccessibilityScrollTo(Accessibility::kScrollToTop);
			break;
		case IA2_SCROLL_TYPE_BOTTOM_RIGHT:
		case IA2_SCROLL_TYPE_BOTTOM_EDGE:
		case IA2_SCROLL_TYPE_RIGHT_EDGE:
			m_accessible_item->AccessibilityScrollTo(Accessibility::kScrollToBottom);
			break;
		case IA2_SCROLL_TYPE_ANYWHERE:
			m_accessible_item->AccessibilityScrollTo(Accessibility::kScrollToAnywhere);
			break;
		default:
			return E_INVALIDARG;
	}

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::scrollToPoint(enum IA2CoordinateType coordinateType, long x, long y)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_groupPosition(long *groupLevel, long *similarItemsInGroup, long *positionInGroup)
{
	return S_FALSE;
}

HRESULT WindowsOpAccessibilityAdapter::get_states(AccessibleStates *states)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_extendedRole(BSTR *extendedRole)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_localizedExtendedRole(BSTR *localizedExtendedRole)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_nExtendedStates(long *nExtendedStates)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_extendedStates(long maxExtendedStates, BSTR **extendedStates, long *nExtendedStates)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_localizedExtendedStates(long maxLocalizedExtendedStates, BSTR **localizedExtendedStates, long *nLocalizedExtendedStates)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_uniqueID(long *uniqueID)
{
	OP_ASSERT(m_accessible_item);

	(*uniqueID) = (long)GetAccessibleItemID(m_accessible_item);
	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_windowHandle(HWND *windowHandle)
{
	WindowsOpWindow* window = WindowsOpAccessibilityAdapter::GetWindowForAccessibleItem(m_accessible_item);

	if (window == NULL)
		return E_FAIL;

	*windowHandle = window->m_hwnd;

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_indexInParent(long *indexInParent)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;

	OpAccessibleItem* acc_parent = m_accessible_item->GetAccessibleParent();

	if (acc_parent == NULL)
	{
		*indexInParent = -1;
		return S_FALSE;
	}

	if (!acc_parent->AccessibilityIsReady())
		return E_FAIL;

	*indexInParent = acc_parent->GetAccessibleChildIndex(m_accessible_item);
	if (*indexInParent == Accessibility::NoSuchChild)
		return E_FAIL;

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_locale(IA2Locale *locale)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_attributes(BSTR *attributes)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpString attrs;
	m_accessible_item->AccessibilityGetAttributes(attrs);

	*attributes = SysAllocString(attrs.CStr());

	return S_OK;
	
}

HRESULT WindowsOpAccessibilityAdapter::addSelection(long startOffset, long endOffset)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	if (OpStatus::IsError(m_accessible_item->AccessibilitySetSelectedTextRange(startOffset, endOffset)))
		return E_FAIL;

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_attributes(long offset, long *startOffset, long *endOffset, BSTR *textAttributes)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_caretOffset(long *offset)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_characterExtents(long offset, enum IA2CoordinateType coordType, long *x, long *y, long *width, long *height)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_nSelections(long *nSelections)
{
	*nSelections = 1;
	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_offsetAtPoint(long x, long y, enum IA2CoordinateType coordType, long *offset)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_selection(long selectionIndex, long *startOffset, long *endOffset)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_text(long startOffset, long endOffset, BSTR *text)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpString acc_text;
	if (OpStatus::IsError(m_accessible_item->AccessibilityGetText(acc_text)))
		return E_FAIL;

	if (startOffset < 0 || endOffset < startOffset || endOffset > acc_text.Length())
		return E_INVALIDARG;

	*text=SysAllocString(acc_text.SubString(startOffset, endOffset-startOffset).CStr());

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::get_textBeforeOffset(long offset, enum IA2TextBoundaryType boundaryType, long *startOffset, long *endOffset, BSTR *text)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_textAfterOffset(long offset, enum IA2TextBoundaryType boundaryType, long *startOffset, long *endOffset,BSTR *text)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_textAtOffset(long offset, enum IA2TextBoundaryType boundaryType, long *startOffset, long *endOffset, BSTR *text)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpString acc_text;
	if (OpStatus::IsError(m_accessible_item->AccessibilityGetText(acc_text)))
		return E_FAIL;

	if (offset < 0 || offset > acc_text.Length())
		return E_INVALIDARG;

	if (boundaryType == IA2_TEXT_BOUNDARY_ALL)
	{
		*text = SysAllocString(acc_text.CStr());
		return S_OK;
	}

	if (boundaryType == IA2_TEXT_BOUNDARY_CHAR || boundaryType == IA2_TEXT_BOUNDARY_WORD || boundaryType == IA2_TEXT_BOUNDARY_SENTENCE)
	{
		UnicodeSegmenter::Type type;

		switch (boundaryType)
		{
			case IA2_TEXT_BOUNDARY_CHAR:
				type = UnicodeSegmenter::Grapheme;
				break;
			case IA2_TEXT_BOUNDARY_WORD:
				type = UnicodeSegmenter::Word;
				break;
			case IA2_TEXT_BOUNDARY_SENTENCE:
				type = UnicodeSegmenter::Sentence;
				break;
			default:
				return E_FAIL;
				break;
		}

		int new_offset = 0;
		int old_offset = 0;
		int length = acc_text.Length() + 1; //We want the length including null char
		UnicodeSegmenter seg = UnicodeSegmenter(type);

		while (new_offset <= offset)
		{
			old_offset = new_offset;
			new_offset += seg.FindBoundary(acc_text.CStr() + new_offset, length - new_offset);
		}
		*startOffset=old_offset;
		*endOffset=new_offset;
		*text = SysAllocString(acc_text.SubString(old_offset,new_offset - old_offset).CStr());

		return S_OK;
	}

	return E_FAIL;
}

HRESULT WindowsOpAccessibilityAdapter::removeSelection(long selectionIndex)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::setCaretOffset(long offset)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::setSelection(long selectionIndex, long startOffset, long endOffset)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_nCharacters(long *nCharacters)
{
	if (!m_accessible_item)
		return CO_E_OBJNOTCONNECTED;
	if (!m_accessible_item->AccessibilityIsReady())
		return E_FAIL;

	OpString acc_text;
	if (OpStatus::IsError(m_accessible_item->AccessibilityGetText(acc_text)))
		return E_FAIL;

	*nCharacters = acc_text.Length();

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::scrollSubstringTo(long startIndex, long endIndex, enum IA2ScrollType scrollType)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::scrollSubstringToPoint(long startIndex, long endIndex, enum IA2CoordinateType coordinateType, long x, long y)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_newText(IA2TextSegment *newText)
{
	return E_NOTIMPL;
}

HRESULT WindowsOpAccessibilityAdapter::get_oldText(IA2TextSegment *oldText)
{
	return E_NOTIMPL;
}

WindowsOpAccessibilityAdapter::VariantEnum::VariantEnum(long* num_clones)
	: m_position(0), m_ref_counter(0), m_num_clones(num_clones) {}

WindowsOpAccessibilityAdapter::VariantEnum::VariantEnum()
	: m_position(0), m_ref_counter(1)
{
	m_num_clones= new long;
	*m_num_clones=1;
	m_contents= new OpVector<VARIANT>;
}

WindowsOpAccessibilityAdapter::VariantEnum::~VariantEnum()
{
	if(!--(*m_num_clones))
	{
		delete m_num_clones;

		for (unsigned int i = 0; i < m_contents->GetCount(); i++)
		{
			VARIANT *v=m_contents->Get(i);
			VariantClear(v);
			delete (v);
		}

		m_contents->Clear();
		delete m_contents;
	}
}

HRESULT WindowsOpAccessibilityAdapter::VariantEnum::QueryInterface(REFIID iid, void ** pvvObject)
{
	if (iid == IID_IUnknown || iid == IID_IEnumVARIANT)
	{
		*pvvObject=(IEnumVARIANT*)this;
		AddRef();
		return S_OK;
	}
	else
	{
		*pvvObject=NULL;
		return E_NOINTERFACE;
	}
}

ULONG WindowsOpAccessibilityAdapter::VariantEnum::AddRef()
{
	return ++m_ref_counter;
}

ULONG WindowsOpAccessibilityAdapter::VariantEnum::Release()
{
	if (!(--m_ref_counter))
	{
		delete this;
		return 0;
	}

	return m_ref_counter;
}

OP_STATUS WindowsOpAccessibilityAdapter::VariantEnum::Add(VARIANT *item)
{
	return m_contents->Add(item);
}

HRESULT WindowsOpAccessibilityAdapter::VariantEnum::Next(unsigned long celt, VARIANT *rgvar, unsigned long *pceltFetched)
{
	unsigned long i;

	if (pceltFetched)
		*pceltFetched = 0;

	if (rgvar == NULL)
		return E_INVALIDARG;

	for (i = m_position; i < m_contents->GetCount() && (celt--) > 0; i++)
		rgvar[i-m_position]=*(m_contents->Get(i));

	
	if (pceltFetched)
		*pceltFetched = i-m_position;

	m_position=i;

	if (celt)
		return S_FALSE;
	else
		return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::VariantEnum::Reset()
{
	m_position=0;

	return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::VariantEnum::Skip(unsigned long celt)
{
	m_position=m_position+celt;

	if (m_position >= m_contents->GetCount())
	{
		m_position = m_contents->GetCount();
		return S_FALSE;
	}
	else
		return S_OK;
}

HRESULT WindowsOpAccessibilityAdapter::VariantEnum::Clone(IEnumVARIANT** ppenum)
{
	VariantEnum *clone = new VariantEnum(m_num_clones);

	if (!clone)
		return E_OUTOFMEMORY;

	clone->m_contents=m_contents;
	clone->m_position=m_position;
	(*m_num_clones)++;

	clone->QueryInterface(IID_IEnumVARIANT, (void**)ppenum);

	return S_OK;
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT
