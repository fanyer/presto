/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_DOCUMENT_MANAGER_H
#define SCOPE_DOCUMENT_MANAGER_H

#ifdef SCOPE_DOCUMENT_MANAGER

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_document_manager_interface.h"
#include "modules/scope/src/scope_idtable.h"
#include "modules/scope/scope_document_listener.h"
#include "modules/scope/scope_readystate_listener.h"

class OpScopeDocumentManager
	: public OpScopeDocumentManager_SI
{
public:
	OpScopeDocumentManager();
	virtual ~OpScopeDocumentManager();

	/**
	 * Initialize document manager with ID tables. It will increase the
	 * reference counter in them and keep a pointer.
	 */
	virtual OP_STATUS Construct(OpScopeIDTable<DocumentManager> *frame_ids,
                                OpScopeIDTable<FramesDocument> *document_ids,
                                OpScopeIDTable<unsigned> *resource_ids);

	virtual OP_STATUS OnPostInitialization();

	// OpScopeService
	virtual OP_STATUS OnServiceEnabled();
	virtual OP_STATUS OnServiceDisabled();

	void ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state);
	OP_STATUS OnDocumentEvent(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state);
	OP_STATUS OnAboutToLoadDocument(DocumentManager *docman, const OpScopeDocumentListener::AboutToLoadDocumentArgs &args);

	// Request/Response functions
	virtual OP_STATUS DoListDocuments(const ListDocumentsArg &in, DocumentList &out);
	virtual OP_STATUS DoLoadDocument(const LoadDocumentArg &in);
	virtual OP_STATUS DoReloadDocument(const ReloadDocumentArg &in);
	virtual OP_STATUS DoSearchText(const SearchTextArg &in, HighlightList &out);
	virtual OP_STATUS DoGetHighlights(const GetHighlightsArg &in, HighlightList &out);
	virtual OP_STATUS DoGetPageText(const GetPageTextArg &in, PageText &out);

private:

#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT
	/**
	 * This class takes a UnicodeOuputStream and stores it into a TempBuffer.
	 *
	 * UnicodeOutputStream is used by FramesDocument::SaveCurrentDocAsText,
	 * which "serializes" a document into a stream of Unicode characters. This
	 * API is again used by GetPageText to provide the visible text of a
	 * document to the client.
	 */
	class DocumentTextStream
		: public UnicodeOutputStream
	{
	public:
		virtual ~DocumentTextStream();

		/**
		 * Get the current contents of the stream.
		 */
		const uni_char *GetString();

		// From UnicodeOutputStream
		virtual void WriteStringL(const uni_char* str, int len);
		virtual OP_STATUS Close();

	private:
		TempBuffer buf;
	};
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

	/**
	 * Find the Window with the specified ID. This function iterates
	 * through all Windows until it finds a match.
	 *
	 * @param [in] id The ID to look for.
	 * @param [out] window Variable to store the Window pointer.
	 * @return OpStatus::OK, or SetCommandError(BadRequest) if no such Window exist.
	 */
	OP_STATUS GetWindow(unsigned id, Window *&window);

	/**
	 * Get (or create) the ID for a certain FramesDocument.
	 *
	 * @param [in] frm_doc The document to get (or create) an ID for.
	 * @param [out] id Variable to store the ID in.
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS GetDocumentID(FramesDocument *frm_doc, unsigned &id);

	/**
	 * Get (or create) the ID for a certain DocumentManager.
	 *
	 * @param [in] docman The frame to get (or create) an ID for.
	 * @param [out] id Variable to store the ID in.
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS GetFrameID(DocumentManager *docman, unsigned &id);

	/**
	 * Get the DocumentManager with certain frame ID. Note that this function
	 * has to verify that the DocumentManager pointer is still valid. It does
	 * this by iterating through the document tree until it finds a match.
	 *
	 * @param wid [in] The Window ID which contains the frame.
	 * @param fid [in] The frame ID.
	 * @param docman [out] Variable to store the DocumentManager in.
	 * @return OpStatus::OK, or SetCommandError(BadRequest) if the window or
	 *         frame turns out to be invalid.
	 */
	OP_STATUS GetFrame(unsigned wid, unsigned fid, DocumentManager *&docman);

	/**
	 * Sets 'pid' to the ID of the parent document, and 'fid' to the ID
	 * of the parent_frame.
	 *
	 * If 'child' is a top-level document, both return values will be set
	 * to zero.
	 *
	 * @return OpStatus::OK or OOM.
	 */
	OP_STATUS GetParentIDs(DocumentManager *child, unsigned &pid, unsigned &fid);

	/**
	 * Get (or create) the ID for a certain URL_Rep (resource).
	 *
	 * @param [in] url The URL_Rep to get (or create) an ID for.
	 * @param [out] id Variable to store the ID in.
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS GetResourceID(URL_Rep *url, unsigned &id);

	/**
	 * Convert from ready state to the document event type.
	 *
	 * @param [in] state_in The ready state to convert from.
	 * @param [out] state_out The document event type corresponding to the given state.
	 * @return OpStatus::ERR_NOT_SUPPORTED in case there is no matching event for the
	 *         given state, OpStatus::OK otherwise.
	 */
	OP_STATUS Convert(OpScopeReadyStateListener::ReadyState state_in, DocumentEventType &state_out);

	/**
	 * Check whether a Window is included in the window filter.
	 *
	 * @param window The Window to check.
	 * @return TRUE if it's part of the filter, FALSE otherwise.
	 */
	BOOL AcceptWindow(Window *window);
#ifdef SEARCH_MATCHES_ALL
	/**
	 * Add highlights for the specified DocumentManager.
	 *
	 * @param docman The DocumentManager to get highlights for. Can be NULL, but will
	 *               OpStatus::ERR in this case.
	 * @param out The output object.
	 * @return OpStatus::OK, OOM, or SetCommandError.
	 */
	OP_STATUS AddHighlights(DocumentManager *docman, HighlightList &out);

	/**
	 * Find the first common ancestor of e1 and e2.
	 *
	 * @return The first common ancestor, or NULL.
	 */
	HTML_Element *FindCommonAncestor(HTML_Element *e1, HTML_Element *e2);
#endif // SEARCH_MATCHES_ALL

	/**
	 * If frame_id is set, return the corresponding DocumentManager. Otherwise,
	 * return the DocumentManager for the specified window.
	 *
	 * @param window_id The ID of the window to get the DocumentManager for. If
	 *                  'frame_id' is also provided, this must be the ID for the
	 *                  Window that contains that frame.
	 * @param frame_id The ID for the DocumentManager to get.
	 */
	DocumentManager *GetDocumentManager(unsigned window_id, unsigned frame_id = 0);

	OpScopeIDTable<DocumentManager> *frame_ids;
	OpScopeIDTable<FramesDocument> *document_ids;
	OpScopeIDTable<unsigned> *resource_ids;
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowManager *window_manager;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
};

#endif // SCOPE_DOCUMENT_MANAGER
#endif // SCOPE_DOCUMENT_MANAGER_H
