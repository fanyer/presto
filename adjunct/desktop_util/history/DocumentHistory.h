/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef QUICK_HISTORY_MANAGER
#define QUICK_HISTORY_MANAGER

#include "adjunct/quick/windows/DocumentWindowListener.h"

class HistoryInformation;

#define MAX_FF_SCORE 1000

/**
 * A per-tab history item.
 *
 * Instances are created by the getters in DocumentHistory.
 */
struct DocumentHistoryInformation
{
	enum HistoryType
	{
		HISTORY_NAVIGATION,

		/**
		 * Special flag which tells that a history info is of fast-forward
		 * type.
		 *
		 * Fast-forward history of this type represents urls/addresses whose
		 * filename extension is of image type.
		 */
		HISTORY_IMAGE,

		/**
		 * Special flag which is used to differentiate WAND type history
		 * and normal ones(which are navigation type). This is only used
		 * when a fast-forward history list is produced.
		 * 
		 * Special WAND related processing is performed if the item with
		 * this flag set is selected from the fast forward history.
		 */
		HISTORY_WAND_LOGIN 
	};

	explicit DocumentHistoryInformation(HistoryType history_type): number(0), type(history_type) { }

	/**
	 * Resets members to its default state
	 */
	void Reset()
	{
		number = 0;
		title.Empty();
		url.Empty();
	}

	/**
	 * Each history element begins at a certain window global history number
	 * and continues to the next element's number minus one. It is basically
	 * a position in history.
	 *
	 * If number contains 0 then it represents invalid history item.
	 */
	int number;

	/**
	 * Holds url of html page
	 */
	OpString url;

	/**
	 * Holds title of html page
	 */
	OpString title;

	/**
	 * Identifies HistoryType.
	 */
	HistoryType type;


	/**
	 * Score. A higher score means a higher priority.
	 */
	int score;
};

/**
 * @brief DocumentHistory wraps Core's history access
 * functionality and at the same time extends it by providing
 * fast-forward history feature.
 */
class DocumentHistory
	: public DocumentWindowListener
{
public:
	explicit DocumentHistory(OpWindowCommander* commander);

	/**
	 * @brief Position is used to differentiate between the
	 * core history list and fast forward list maintained in
	 * DocumentHistory class.
	 * Core history list is accessed using OpWindowCommander
	 * history interface.
	 */
	struct Position
	{
		enum Type
		{
			INVALID,
			NORMAL,			// Identifies core history list
			FAST_FORWARD	// Identifies fast forward list from DocumentHistory class
		};

		Type type;
		int index;

		Position():type(INVALID) { }
		Position(Type t, int i):type(t), index(i) { }
	};

	/**
	 * To test whether there is any visited URL to navigate back.
	 *
	 * @return true if there is back history, otherwise false.
	 */
	bool HasBackHistory() const;

	/**
	 * To test whether there is any visited URL to navigate forward.
	 *
	 * @return true if there is forward history, otherwise false.
	 */
	bool HasForwardHistory() const ;

	/**
	 * Jumping between the visited pages' domains is known as rewind
	 * / fast-foward depending on the type of operation.
	 *
	 * To test whether there is any visited URLs to fast rewind.
	 *
	 * @return true if there is rewind history, otherwise false.
	 */
	bool HasRewindHistory() const;

	/**
	 * Jumping between the visited pages' domains is known as rewind
	 * / fast-foward depending on the type of operation.
	 *
	 * To test whether there is any visited URLs to fast foward.
	 * 
	 * Fast-forward is not just jumping between the domains of visited URLS,
	 * but also helps to navigate below URLs in the listed order.
	 * a. Distinct Server: If any distinct server found it's a candidate for 
	 *    fast forward history
	 * b. Link Elements: If any information.rel is Next, then it's a candidate 
	 *    for fast forward history
	 * c. Usable wand info
	 * d. Usable next elements
	 * 
	 * @return true if there is fast-forward history, otherwise false.
	 */
	bool HasFastForwardHistory() const;

	/**
	 * Returns the current history position.
	 * 
	 * The returned value is between the minimum and the maximum values
	 * returned by GetHistoryRange().
	 * @see OpWindowCommander::GetHistoryRange()
	 * @see OpWindowCommander::SetCurrentHistoryPos()
	 * 
	 * @return the history position.
	 */
	int GetCurrentHistoryPos() const;

	/**
	 * Returns number of elements in history.
	 * 
	 * @return the history length.
	 */
	int GetHistoryLength() const;

	/**
	 * Navigates history forward or back depending on argument.
	 * Function would return 0 if history is empty(not created).
	 *
	 * @param back if set to true makes navigate back in history 
	 * otherwise forward
	 *
	 * @return Current history position after move operation.
	 */
	int MoveInHistory(bool back);

	/**
	 * Navigates history fast-forward or rewind depending on argument.
	 * In case, if history is empty or there is nothing to move in
	 * history, then current history position will be returned.
	 *
	 * @param back if set to true makes rewind in history 
	 * otherwise fast-forward
	 *
	 * @return Current history position after move operation.
	 */
	Position MoveInFastHistory(bool back);

	/**
	 * Fetches HistoryInformation based on input history position
	 * 
	 * @param pos indicates history pos.
	 * @param[out] result contains fetched info.
	 * @return status.
	 */
	OP_STATUS GetHistoryInformation(Position pos, HistoryInformation* result) const;

	/**
	 * Jumps to given history position.
	 * 
	 * @param pos indicates history pos.
	 * @return status.
	 */
	OP_STATUS GotoHistoryPos(Position pos);

	/**
	 * Fetches list of DocumentHistoryInformation information
	 * 
	 * @param[out] history_list contains fetched info
	 * @param forward if set to true fetches forward history info otherwise back.
	 * @return status.
	 */
	OP_STATUS GetHistoryList(OpAutoVector<DocumentHistoryInformation>& history_list, bool forward) const;

	/**
	 * Fetches list of fast-forward/rewind information
	 * 
	 * @param[out] history_list contains fetched info
	 * @param forward if set to true fetches forward history info otherwise back.
	 * @return status.
	 */
	OP_STATUS GetFastHistoryList(OpAutoVector<DocumentHistoryInformation>& history_list, bool forward) const;

	/**
	 * Fetches history info w.r.t current position.
	 * 
	 * @param is_back if sets to true will fetches back history info 
	 * in reference to current history position, 
	 * i.e 'current history pos - 1' otherwise 'current history pos + 1'
	 *
	 * @return DocumentHistoryInformation info
	 */
	const DocumentHistoryInformation& GetAdjacentHistoryInfo(bool is_back) const;

private:
	virtual void OnStartLoading(DocumentDesktopWindow* document_window) { m_is_fast_forward_list_updated = false; }
	virtual void OnLoadingFinished(DocumentDesktopWindow* document_window,
								  OpLoadingListener::LoadingFinishStatus,
								  BOOL was_stopped_by_user = FALSE);

	/**
	 * Updates back and forward history info with reference
	 * to current history position. This info is cached and
	 * gets updated whenever user navigates forward/back in
	 * history.
	 * @see m_back_history_info
	 * @see m_forward_history_info
	 *
	 * @param is_back if true, back history info gets updated, otherwise
	 * forward history info.
	 */
	OP_STATUS UpdateHistoryInfo(bool is_back) const;

	/**
	 * Updates fast-forward reference history info, which basically
	 * is a current history/page info.
	 */
	OP_STATUS UpdateFastForwardRefURL() const;

	/**
	 * Retrieves history info whose domain does not match the
	 * current history info. Search for nonmatching domain
	 * begins at 'current_history_pos - 1' in the list,
	 * maintained at core side, and traverse towards start until
	 * suitable match is found.
	 *
	 * @param[out] info contains nonmatching domain's history info
	 * @return if search succeeds, position of history info is returned,
	 * otherwise invalid history position which has value 0.
	 */
	int GetLastHistoryInfoDifferentDomain(HistoryInformation* info) const;

	/**
	 * The order of items in the fast forward list is governed by
	 * priority defined in the fastforward.ini file.
	 *
	 * @param which identifies fast-forward keyword.
	 * @return priority associated with fast-forward keyword.
	 */
	int GetFastForwardValue(OpStringC which) const;

	/**
	 * Creates fast-forward list maintained in this class.
	 * @see m_fast_forward_list
	 */
	OP_STATUS CreateFastForwardList() const;

	/**
	 * Creates fast-forward list from the list of addesses whose filename
	 * extension of image type.
	 *
	 * Once all items in 'm_fast_forward_img_list' are navigated
	 * 'm_fast_forward_list' will points to 'm_fast_forward_ref_url', starting
	 * page address from which 'm_fast_forward_img_list' is constructed.
	 *
	 * @see m_fast_forward_img_list
	 * @see m_fast_forward_list
	 */
	OP_STATUS CreateFastForwardListFromImageList() const;

	/**
	 * Add the first image from the image list into the fast forward list.
	 *
	 * @return OpStatus::OK if the item was found and added, OpStatus::ERR otherwise.
	 */
	OP_STATUS AddFastForwardItemFromImageList() const;

	/**
	 * Adds an item to a given list.
	 *
	 * @param history_list identifies the list to which a given
	 * history item will be added.
	 * @param info identifies history info.
	 */
	static OP_STATUS AddToList(OpAutoVector<DocumentHistoryInformation>& history_list, const HistoryInformation &info, int score = MAX_FF_SCORE);

	/**
	 * Adds an item to a given list.
	 * This method is mostly used for adding an item to fast-forward
	 * list.
	 *
	 * @param history_list identifies the list to which a given
	 * history item will be added.
	 * @param info identifies history info
	 */
	static OP_STATUS AddToList(OpAutoVector<DocumentHistoryInformation>& history_list, const DocumentHistoryInformation &info, int score = MAX_FF_SCORE);

	OpWindowCommander* m_window_commander;

	/**
	 * Maintains tips related info which are shown on hover over
	 * back and forward buttons.
	 * On hover over said buttons, tips flash and which is
	 * usually title of a page w.r.t current history position.
	 * These members are updated every time user navigates back and
	 * forward.
	 */
	mutable DocumentHistoryInformation m_back_history_info;  // holds 'current_history_pos - 1' history info.
	mutable DocumentHistoryInformation m_forward_history_info;  // holds 'current_history_pos + 1' history info.
	mutable DocumentHistoryInformation m_fast_forward_ref_url;  // holds 'current_history_pos' history info.

	/**
	 * Contains information related to fast-forward, FF, history
	 * 
	 * Fast-forward history is special and handled differently
	 * than rewind, forward and back which are handled and maintained
	 * by Core.
	 *
	 * FF list contains following types of URLs:
	 * a. Distinct Server: If any distinct server found it's a candidate for 
	 *    fast forward history
	 * b. Link Elements: If any information.rel is Next, then it's a candidate 
	 *    for fast forward history
	 * c. Usable wand info
	 * d. Usable next elements
	 */
	mutable OpAutoVector<DocumentHistoryInformation> m_fast_forward_list;

	/**
	 * Contains fast-forward history info of type HISTORY_NAVIGATION.
	 *
	 * This list, also known as side show urls, is used to navigate between
	 * the history items whose addesses have filename extension of image type.
	 */
	mutable OpAutoVector<DocumentHistoryInformation> m_fast_forward_img_list;

	mutable bool m_is_fast_forward_list_updated;
};

#endif //QUICK_HISTORY_MANAGER
