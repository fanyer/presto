/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_TABSAPIHELPER_H
#define DOM_EXTENSIONS_TABSAPIHELPER_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/hardcore/timer/optimer.h"
#include "modules/windowcommander/OpTabAPIListener.h"
#include "modules/windowcommander/TabsApiNotificationHelper.h"
#include "modules/dom/src/domrestartobject.h"
#include "modules/ecmascript_utils/esasyncif.h"

#define TAB_API_CALL_FAILED_IF_ERROR(expr) \
{ \
	OP_STATUS TAB_API_CALL_FAILED_IF_ERROR_TMP = expr; \
	if (OpStatus::IsError(TAB_API_CALL_FAILED_IF_ERROR_TMP)) \
		return DOM_TabsApiHelperBase::CallTabAPIException(this_object, return_value, TAB_API_CALL_FAILED_IF_ERROR_TMP); \
}

class HandleInitialContentElementConversionCallback;
/**
  * Helper class implementing base functionality for DOM_TabsApiHelper and DOM_TabsApiFocusHelper.
  */
class DOM_TabsApiHelperBase
	: public TabsApiNotificationHelper
	, public DOM_RestartObject
{

public:
	class Error : public OpStatus
	{
	public:
		enum
		{
			ERR_NOT_ENOUGH_INITIAL_CONTENT = OpStatus::USER_ERROR + 1,
			/**< Reported when we try to create a group with less than 2 tabs. */
			ERR_WRONG_MOVE_ARGUMENT,
			/**< Reported when we try to move a tab or a tab group and any of the arguments is of invalid type. */
			ERR_WRONG_MOVE_STATE
			/**< Reported when we try to move a tab or a tab group to an incompatible target. */
		};
	};
	DOM_TabsApiHelperBase();
	OP_STATUS GetStatus() { return m_status; }
	OP_STATUS IsFinished() { return m_is_finished; }

	// Helpers for translating EcmaScript props objects into C++ structs.
	struct TabProps
	{
		const uni_char* url;
		const uni_char* title;
		BOOL3 is_locked;
		BOOL3 is_private;
		BOOL3 is_selected;
	};
	void ExtractTabProps(TabProps& props, ES_Object* es_props, DOM_Runtime* runtime, BOOL clone_strings);

	struct WindowProps
	{
		BOOL3 is_focused;
		BOOL3 is_private;
		int height; // < UNSPECIFIED_WINDOW_DIMENSION - undefined
		int width;  // < UNSPECIFIED_WINDOW_DIMENSION - undefined
		int top;    // < UNSPECIFIED_WINDOW_DIMENSION - undefined
		int left;   // < UNSPECIFIED_WINDOW_DIMENSION - undefined
	};
	void ExtractWindowProps(WindowProps& props, ES_Object* es_props, DOM_Runtime* runtime);

	struct TabGroupProps
	{
		BOOL3 is_collapsed;
	};
	void ExtractTabGroupProps(TabGroupProps& props, ES_Object* es_props, DOM_Runtime* runtime);

	static int CallTabAPIException(DOM_Object* this_object, ES_Value* return_value, OP_STATUS error);
	/**< Translate TabApiError to appropriate DOM_Exception */
protected:
	void Finish(OP_STATUS status);
	OP_STATUS OpenNewTab(OpTabAPIListener::WindowTabInsertTarget& insert_target, ES_Object* es_props, BOOL3 focused, OpWindowCommander** wc = NULL);
private:
	OP_STATUS m_status;
	BOOL m_is_finished;
};

/**
  *  Helper class for performing possibly asynchronous calls to OpExtensionUiListener.
  *  It is meant to be used in conjunction wit DOM_RestartObject infrastructure -
  *  see its description for general info on suspending/restarting calls.
  *
  *  The calls which only have success/error "return values" can have their status
  *  checked by using GetStatus method after it completed.
  *  For methods which return more than just status the return value is stored in
  *  relevant field of m_query_result structure (accesible by GetResult method).
  *
  *  \see DOM_RestartObject
  */
class DOM_TabsApiHelper
	: public DOM_TabsApiHelperBase
{
public:
	DOM_TabsApiHelper();
	static OP_STATUS Make(DOM_TabsApiHelper*& new_obj, DOM_Runtime* runtime);

	void QueryAllWindows();
	/**< Queries all top-level browser Windows.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_ALL_WINDOWS and its value stored
	     in query_all_windows subfield of m_query_result. */


	void QueryAllTabGroups();
	/**< Queries all tab groups.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_ALL_TAB_GROUPS and its value stored
	     in query_all_tab_groups subfield of m_query_result. */

	void QueryAllTabs();
	/**< Queries all tabs.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_ALL_TABS and its value stored
	     in query_all_tabs subfield of m_query_result. */

	void QueryWindow(OpTabAPIListener::TabAPIItemId window_id, BOOL include_content);
	/**< Queries properties of a specified top-level browser window.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_WINDOW and its value stored
	     in query_window subfield of m_query_result.

	     @param window_id Id of a window of which info is requested.
	            If window_id == 0 then currently active window is queried.
	     @param include_content If this is set to TRUE then, in addition to
	            window properties, its contents(tabs&tab groups) are queried
	            => tabs and tab_gropus fields will be filled. */

	void QueryTabGroup(OpTabAPIListener::TabAPIItemId tab_group_id, BOOL include_content);
	/**< Queries properties of a specified tab group.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_TAB_GROUP and its value stored
	     in query_tab_group subfield of m_query_result.

	     @param tab_group_id Id of a tab group of which info is requested.
	     @param include_content If this is set to TRUE then, in addition to
	            tab group properties, its contents(tabs) are queried
	            => tabs field will be filled. */

	void QueryTab(OpWindowCommander* target_widow);
	/**< Queries properties of a tab corresponding to a specific core Window.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_TAB and its value stored
	     in query_tab subfield of m_query_result.

	     @param target_widow window commander of a core Window we request
	            tab info about. MUST NOT be NULL. */

	void QueryTab(OpTabAPIListener::TabAPIItemId target_tab);
	/**< Queries properties of a tab corresponding to a specific core Window.

	     After successfully finished query the result should
	     be of type RESULT_QUERY_TAB and its value stored
	     in query_tab subfield of m_query_result.

	     @param target_tab Id of a queried tab. */

	void RequestTabClose(OpTabAPIListener::TabAPIItemId tab_id);
	/**< Requests top-level browser window to be closed.

	     @param window_id Id of a top-level browser window requested to be closed. */

	void RequestWindowClose(OpTabAPIListener::TabAPIItemId window_id);
	/**< Requests top-level browser window to be closed.

	     @param window_id Id of a top-level browser window requested to be closed. */

	void RequestWindowMove(OpTabAPIListener::TabAPIItemId window_id, const OpRect& rect);
	/**< Requests top-level browser window to be moved/resized to a specified rectangle.

	     @param window_id Id of a top-level browser window requested to be moved.
	     @param rect target window rectangle. */

	void RequestTabGroupClose(OpTabAPIListener::TabAPIItemId target_tab_group_id);
	/**< Requests close tab group.

	     @param target_tab_group_id Id of a tab group in which the tab should be updated. */

	void RequestTabGroupUpdate(OpTabAPIListener::TabAPIItemId target_tab_group_id, ES_Object* es_props, DOM_Runtime* origining_runtime);
	/**< Requests updating tab group.

	     @param target_tab_group_id Id of a tab group in which the tab should be updated.
	     @param es_props Ecmascript BrowserTabProperties object describing properties of updated tab group.
	     @param origining_runtime DOM_Runtime requesting specified operation. */

	void RequestTabUpdate(OpTabAPIListener::TabAPIItemId target_tab_id, ES_Object* es_props, DOM_Runtime* origining_runtime);
	/**< Requests updating tab.

	     @param target_tab_id Id of a tab group in which the tab should be updated.
	     @param es_props Ecmascript BrowserTabProperties object describing properties of updated tab .
	     @param origining_runtime DOM_Runtime requesting specified operation. */

	void RequestTabFocus(OpTabAPIListener::TabAPIItemId target_tab_id);
	/**< Requests focusing a tab.

	     @param target_tab_id Id of a tab group in which the tab should be focused. */
protected:
	virtual void OnNotification(OP_STATUS status, QueryResult::ResultType type);
};

/**
  *  Helper class for performing possibly asynchronous calls to OpExtensionUiListener.
  *  Used for focusing tab groups and windows. Separated from DOM_TabsApiHelper as it
  *  needs to do 2 async calls to complete the operation.It is meant to be used in
  *  conjunction wit DOM_RestartObject infrastructure - description for general info
  *  on suspending/restarting calls.
  *
  *  \see DOM_RestartObject
  */
class DOM_TabsApiFocusHelper
	: public DOM_TabsApiHelperBase
{
public:
	DOM_TabsApiFocusHelper(){};

	static OP_STATUS Make(DOM_TabsApiFocusHelper*& new_obj, DOM_Runtime* runtime);

	void RequestTabGroupFocus(OpTabAPIListener::TabAPIItemId target_tab_group_id);
	/**< Requests focusing a tab group.

	     @param tab_group_id Id of a tab group in which the tab should be focused. */

	void RequestWindowFocus(OpTabAPIListener::TabAPIItemId target_window_id);
	/**< Requests focusing a window.

	     @param target_window_id Id of a window in which the tab should be focused. */
protected:
	virtual void OnNotification(OP_STATUS status, QueryResult::ResultType type);
};

/**
  *  Helper class for performing possibly asynchronous calls to OpExtensionUiListener.
  *  Used for focusing tab groups and windows. Separated from DOM_TabsApiHelper as it
  *  needs to do 2 async calls to complete the operation.It is meant to be used in
  *  conjunction wit DOM_RestartObject infrastructure - description for general info
  *  on suspending/restarting calls.
  *
  *  \see DOM_RestartObject
  */
class DOM_TabsApiInsertHelper
	: public DOM_TabsApiHelperBase
{
public:
	DOM_TabsApiInsertHelper();

	static OP_STATUS Make(DOM_TabsApiInsertHelper*& new_obj, DOM_Runtime* runtime);

	void RequestInsert(ES_Object* insert_what, ES_Object* into_what, ES_Object* before_what /*optional*/);

protected:
	virtual void OnNotification(OP_STATUS status, QueryResult::ResultType type);
	OpTabAPIListener::WindowTabInsertTarget m_insert_target;
	OpTabAPIListener::TabAPIItemId m_src_group_id;
	OpTabAPIListener::TabAPIItemId m_src_tab_id;

	OP_STATUS CheckBeforeWhat(ES_Object* before_what);
	OP_STATUS CheckSrc(ES_Object* insert_what);
	OP_STATUS FindInsertDestination(ES_Object* into_what);
	void PerformMove();
};

/**
  *  Helper class for performing possibly asynchronous calls to OpExtensionUiListener.
  *  Used for focusing tab groups and windows. Separated from DOM_TabsApiHelper as it
  *  needs to do 2 async calls to complete the operation.It is meant to be used in
  *  conjunction wit DOM_RestartObject infrastructure - description for general info
  *  on suspending/restarting calls.
  *
  *  \see DOM_RestartObject
  */
class DOM_TabsApiTabCreateHelper
	: public DOM_TabsApiHelperBase
{
public:
	DOM_TabsApiTabCreateHelper();

	static OP_STATUS Make(DOM_TabsApiTabCreateHelper*& new_obj, DOM_Runtime* runtime);

	void RequestCreateTab(ES_Object* tab_props, OpTabAPIListener::TabAPIItemId parent_win_id, OpTabAPIListener::TabAPIItemId parent_group_id, ES_Object* before_what);
	/**< Requests creation of a new tab.

	     Only one of target_window_id and target_tab_group_id may be specified (non-0).
	     If target_window_id == 0 and target_tab_group_id == 0 then it will be created in currently active browser window non grouped.

	     @param target_window_id Id of a top-level browser window in which the tab should be created.
	     @param target_tab_group_id Id of a tab group in which the tab should be created.
	     @param es_props Ecmascript BrowserTabProperties object describing properties of new tab.
	     @param result_win If successful this will be set to set to a core Window corresponding to the newly created tab.
	     @param origining_runtime DOM_Runtime requesting specified operation
	            (needed to determine the opener for newly created core Window).*/


protected:
	virtual void GCTrace();
	virtual void OnNotification(OP_STATUS status, QueryResult::ResultType type);
	OpTabAPIListener::WindowTabInsertTarget m_insert_target;
	ES_Object* m_properties;

	OP_STATUS CheckBeforeWhat(ES_Object* before_what);
	void FindInsertDestination(OpTabAPIListener::TabAPIItemId parent_win_id, OpTabAPIListener::TabAPIItemId parent_group_id);
	void PerformCreate();
};

/**
  *  Helper class for performing possibly asynchronous calls to OpExtensionUiListener.
  *  Used for focusing tab groups and windows. Separated from DOM_TabsApiHelper as it
  *  needs to do 2 async calls to complete the operation.It is meant to be used in
  *  conjunction wit DOM_RestartObject infrastructure - description for general info
  *  on suspending/restarting calls.
  *
  *  \see DOM_RestartObject
  */
class DOM_TabsApiContainerCreateHelper
	: public DOM_TabsApiHelperBase
	, public OpTimerListener
{
public:
	DOM_TabsApiContainerCreateHelper();

	static OP_STATUS Make(DOM_TabsApiContainerCreateHelper*& new_obj, DOM_Runtime* runtime);

	void RequestCreateTabGroup(ES_Object* initial_content, ES_Object* container_props, OpTabAPIListener::TabAPIItemId parent_win_id, ES_Object* before_what);
	/** Requests creation of a new tab group.

	    After successfully finished construction of a tab group result should
	    be of type RESULT_TAB_GROUP_CREATE and the id of newly constructed tab group
	    should be in created_tab_group_id subfield of m_query_result.

	    @param es_initial_win_content EcmaScript array of BrowserTab or BrowserTabProperties objects
	           which describe the initial content of newly created tab group.
	    @param es_tab_group_props Ecmascript BrowserTabGroupProperties object describing properties of new tab group.
	    @param window_id Id of a top-level browser window in which the tab group should be created.
	           if window_id == 0 then it will be created in currently active browser window. */

	void RequestCreateWindow(ES_Object* initial_content, ES_Object* container_props);
	/**< Requests creation of a new top-level browser window.

	     After successfully finished construction of a window result should
	     be of type RESULT_WINDOW_CREATE and the id of newly constructed window
	     should be in created_window_id subfield of m_query_result.

	     @param es_initial_win_content EcmaScript array of BrowserTab, BrowserTabGroup or BrowserTabProperties objects
	            which describe the initial content of newly created window.
	     @param es_window_props Ecmascript BrowserWindowProperties object describing properties of new window. */

protected:
	virtual void GCTrace();
	virtual void OnNotification(OP_STATUS status, QueryResult::ResultType type);
	virtual void OnTimeOut(OpTimer* timer);
	BOOL IsCreatingGroup();


	OpTabAPIListener::WindowTabInsertTarget m_insert_target;
	unsigned int m_initial_content_index;
	ES_Object* m_initial_content;
	BOOL m_querying_before;
	BOOL3 m_focus_container;
	OpTimer m_create_next_timer;

	OP_STATUS CheckBeforeWhat(ES_Object* before_what);
	void FindInsertDestination(OpTabAPIListener::TabAPIItemId parent_win_id);
	void PerformCreateStep();
	void SetInsertTypeToExisting();
	void CreatingContainerFinished();

	class ConversionCallback : public ES_AsyncCallback
	{
	public:
		ConversionCallback(DOM_TabsApiContainerCreateHelper* helper)
			: m_helper(helper) {}
		virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);
	private:
		DOM_TabsApiContainerCreateHelper* m_helper;
	};
	friend class ConversionCallback;
};


#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_TABSAPIHELPER_H

