/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPACCESSIBILITYADAPTER_H
#define WINDOWSOPACCESSIBILITYADAPTER_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#ifndef STATE_SYSTEM_HASPOPUP
#define STATE_SYSTEM_HASPOPUP 0x40000000
#endif

//#include <Oleacc.h>
#if _MSC_VER < 1500
#include <Winable.h>
#endif

class WindowsOpWindow;

#include "modules/pi/OpAccessibilityAdapter.h"
#include "modules/accessibility/opaccessibleitem.h"

#include "platforms/windows/IDL/Accessible2.h"
#include "platforms/windows/IDL/AccessibleText.h"

#ifndef STATE_SYSTEM_PROTECTED
#define STATE_SYSTEM_PROTECTED 0x20000000
#endif

void FreeAccessibleItemID(OpAccessibleItem* item);
LONG GetAccessibleItemID(OpAccessibleItem* item);

class WindowsOpAccessibilityAdapter : 
	public OpAccessibilityAdapter, 
	public IAccessible2,
	public IAccessibleText,
	public IServiceProvider
{

public:

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	static int s_last_id;
#endif

	//Ref counter starts at 1, because opera holds one ref on this.
	WindowsOpAccessibilityAdapter(OpAccessibleItem *accessible_item) : m_accessible_item(accessible_item), m_ref_counter(1) {}
	~WindowsOpAccessibilityAdapter() {};

#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
	void GetDebugInfo(OpString &info);
#endif

	//Table conversions table between MSAA and our API (see WindowsOpAccessibilityAdapter.cpp)
	static DWORD IAccRole[];
	static DWORD IAcc2Role[];
	static DWORD IAccStateMask[];

	static WindowsOpWindow* GetWindowForAccessibleItem(OpAccessibleItem* item);

	/*********** OpAccessibilityAdapter interface ***********/

	OpAccessibleItem* GetAccessibleItem() { return m_accessible_item; }
	void AccessibleItemRemoved() { FreeAccessibleItemID(m_accessible_item); m_accessible_item = NULL; this->Release(); }

	/**********************************************************/

	/**************** IAccessible2 interface ******************/
	//	For documentation, see
	//	http://www.linuxfoundation.org/en/Accessibility/IAccessible2

	virtual HRESULT STDMETHODCALLTYPE get_nRelations(long *nRelations);
	virtual HRESULT STDMETHODCALLTYPE get_relation(long relationIndex, IAccessibleRelation **relation);
	virtual HRESULT STDMETHODCALLTYPE get_relations(long maxRelations, IAccessibleRelation **relations, long *nRelations);
	virtual HRESULT STDMETHODCALLTYPE role(long *role);
	virtual HRESULT STDMETHODCALLTYPE scrollTo(enum IA2ScrollType scrollType);
	virtual HRESULT STDMETHODCALLTYPE scrollToPoint(enum IA2CoordinateType coordinateType, long x, long y);
	virtual HRESULT STDMETHODCALLTYPE get_groupPosition(long *groupLevel, long *similarItemsInGroup, long *positionInGroup);
	virtual HRESULT STDMETHODCALLTYPE get_states(AccessibleStates *states);
	virtual HRESULT STDMETHODCALLTYPE get_extendedRole(BSTR *extendedRole);
	virtual HRESULT STDMETHODCALLTYPE get_localizedExtendedRole(BSTR *localizedExtendedRole);
	virtual HRESULT STDMETHODCALLTYPE get_nExtendedStates(long *nExtendedStates);
	virtual HRESULT STDMETHODCALLTYPE get_extendedStates(long maxExtendedStates, BSTR **extendedStates, long *nExtendedStates);
	virtual HRESULT STDMETHODCALLTYPE get_localizedExtendedStates(long maxLocalizedExtendedStates, BSTR **localizedExtendedStates, long *nLocalizedExtendedStates);
	virtual HRESULT STDMETHODCALLTYPE get_uniqueID(long *uniqueID);
	virtual HRESULT STDMETHODCALLTYPE get_windowHandle(HWND *windowHandle);
	virtual HRESULT STDMETHODCALLTYPE get_indexInParent(long *indexInParent);
	virtual HRESULT STDMETHODCALLTYPE get_locale(IA2Locale *locale);
	virtual HRESULT STDMETHODCALLTYPE get_attributes(BSTR *attributes);

	/**********************************************************/

	/************** IAccessibleText interface *****************/
	virtual HRESULT STDMETHODCALLTYPE addSelection(long startOffset, long endOffset);
	virtual HRESULT STDMETHODCALLTYPE get_attributes(long offset, long *startOffset, long *endOffset, BSTR *textAttributes);
	virtual HRESULT STDMETHODCALLTYPE get_caretOffset(long *offset);
	virtual HRESULT STDMETHODCALLTYPE get_characterExtents(long offset, enum IA2CoordinateType coordType, long *x, long *y, long *width, long *height);
	virtual HRESULT STDMETHODCALLTYPE get_nSelections(long *nSelections);
	virtual HRESULT STDMETHODCALLTYPE get_offsetAtPoint(long x, long y, enum IA2CoordinateType coordType, long *offset);
	virtual HRESULT STDMETHODCALLTYPE get_selection(long selectionIndex, long *startOffset, long *endOffset);
	virtual HRESULT STDMETHODCALLTYPE get_text(long startOffset, long endOffset, BSTR *text);
	virtual HRESULT STDMETHODCALLTYPE get_textBeforeOffset(long offset, enum IA2TextBoundaryType boundaryType, long *startOffset, long *endOffset, BSTR *text);
	virtual HRESULT STDMETHODCALLTYPE get_textAfterOffset(long offset, enum IA2TextBoundaryType boundaryType, long *startOffset, long *endOffset,BSTR *text);
	virtual HRESULT STDMETHODCALLTYPE get_textAtOffset(long offset, enum IA2TextBoundaryType boundaryType, long *startOffset, long *endOffset, BSTR *text);
	virtual HRESULT STDMETHODCALLTYPE removeSelection(long selectionIndex);
	virtual HRESULT STDMETHODCALLTYPE setCaretOffset(long offset);
	virtual HRESULT STDMETHODCALLTYPE setSelection(long selectionIndex, long startOffset, long endOffset);
	virtual HRESULT STDMETHODCALLTYPE get_nCharacters(long *nCharacters);
	virtual HRESULT STDMETHODCALLTYPE scrollSubstringTo(long startIndex, long endIndex, enum IA2ScrollType scrollType);
	virtual HRESULT STDMETHODCALLTYPE scrollSubstringToPoint(long startIndex, long endIndex, enum IA2CoordinateType coordinateType, long x, long y);
	virtual HRESULT STDMETHODCALLTYPE get_newText(IA2TextSegment *newText);
	virtual HRESULT STDMETHODCALLTYPE get_oldText(IA2TextSegment *oldText);
        
	/**********************************************************/

	/**************** IAccessible interface *******************/
	//	For documentation, see
	//	http://msdn.microsoft.com/library/default.asp?url=/library/en-us/msaa/msaaccrf_87ja.asp

	HRESULT STDMETHODCALLTYPE get_accName(VARIANT varID, BSTR* pszName);
	HRESULT STDMETHODCALLTYPE get_accRole(VARIANT varID, VARIANT* pvarRole);
	HRESULT STDMETHODCALLTYPE get_accState(VARIANT varID, VARIANT* pvarState);
	HRESULT STDMETHODCALLTYPE accLocation(long* pxLeft, long* pyTop, 
											long* pcxWidth, long* pcyHeight, VARIANT varID);
	HRESULT STDMETHODCALLTYPE accHitTest(long xLeft, long yTop, VARIANT* pvarID);
	HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** ppdispParent);
	HRESULT STDMETHODCALLTYPE accNavigate(long navDir, VARIANT varStart, VARIANT* pvarEnd);
	HRESULT STDMETHODCALLTYPE get_accChild(VARIANT varChildID, IDispatch** ppdispChild);
	HRESULT STDMETHODCALLTYPE get_accChildCount(long* pcountChildren);
	HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* pvarID);
	HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* pvarChildren);
	HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT varID, BSTR* pszDescription);
	HRESULT STDMETHODCALLTYPE get_accValue(VARIANT varID, BSTR* pszValue);
	HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT varID, BSTR* pszHelp);
	HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild, long* pidTopic);
	HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT varID, BSTR* pszKeyboardShortcut);
	HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT varID, BSTR* pszDefaultAction);
	HRESULT STDMETHODCALLTYPE accSelect(long flagsSelect, VARIANT varID);
	HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT varID);
	HRESULT STDMETHODCALLTYPE put_accValue(VARIANT varID, BSTR pszValue);
	HRESULT STDMETHODCALLTYPE put_accName(VARIANT varID, BSTR pszName);

	/**********************************************************/

	/************** IServiceProvider interface ****************/
	//	For documentation, see
	//	http://msdn.microsoft.com/en-us/library/cc678965(VS.85).aspx

	HRESULT STDMETHODCALLTYPE QueryService(REFGUID guidService, REFIID riid, void **ppv);

	/******************* IUnknown interface *******************/
	//	For documentation, see
	//	http://msdn2.microsoft.com/en-us/library/ms680509.aspx

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** pvvObject);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	/**********************************************************/

	/******************* IDispatch interface ******************/
	//Not implemented since they're not required (return E_NOTIMPL)

	HRESULT STDMETHODCALLTYPE GetTypeInfoCount(unsigned int FAR* pctinfo);
	HRESULT STDMETHODCALLTYPE GetTypeInfo(unsigned int iTInfo, LCID  lcid, ITypeInfo FAR* FAR* ppTInfo);
	HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, OLECHAR FAR* FAR* rgszNames, unsigned int cNames, 
							LCID lcid, DISPID FAR*  rgDispId);
	HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, 
					DISPPARAMS FAR* pDispParams, VARIANT FAR* pVarResult, 
					EXCEPINFO FAR* pExcepInfo, unsigned int FAR* puArgErr);

	/**********************************************************/


private:

	/***********************************************************
	*               Implementation of IEnumVARIANT,            *
	*        needed to return a collection of VARIANT          *
	***********************************************************/
	class VariantEnum : public IEnumVARIANT
	{
	public:
		VariantEnum();
		~VariantEnum();

		//Adds a variant to the collection
		OP_STATUS Add(VARIANT* item);


		/*IEnumVARIANT interface*/
		//	For documentation, see
		//	http://msdn2.microsoft.com/en-us/library/ms221053.aspx
		HRESULT STDMETHODCALLTYPE Next(unsigned long celt, VARIANT FAR* rgvar,
			unsigned long FAR* pceltFetched);
		HRESULT STDMETHODCALLTYPE Skip(unsigned long celt);
		HRESULT STDMETHODCALLTYPE Reset();
		HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT FAR* FAR* ppenum);

		/*IUnknown interface*/
		//	For documentation, see
		//	http://msdn2.microsoft.com/en-us/library/ms680509.aspx
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** pvvObject);
		ULONG STDMETHODCALLTYPE AddRef();
		ULONG STDMETHODCALLTYPE Release();

	private:

		unsigned long m_position;
		OpVector<VARIANT> *m_contents;
		long m_ref_counter;
		long* m_num_clones;

		VariantEnum(long* num_clones);
	};

	long m_ref_counter;
	OpAccessibleItem* m_accessible_item;

	OpAccessibleItem* ReadyOrNull(OpAccessibleItem* accessible_item) {return (accessible_item && accessible_item->AccessibilityIsReady())?accessible_item:NULL; }
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // WINDOWSOPACCESSIBILITYADAPTER_H
