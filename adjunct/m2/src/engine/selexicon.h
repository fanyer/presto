// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef SE_LEXICON_H
#define SE_LEXICON_H

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "adjunct/desktop_util/thread/DesktopThread.h"
#include "adjunct/desktop_util/thread/DesktopMutex.h"
#include "modules/search_engine/StringTable.h"

/** @brief Search-engine based, threaded lexicon
  * @author Arjan van Leeuwen
  */
class SELexicon : public MessageObject, private DesktopThread
{
public:
	/** Constructor
	  */
	SELexicon() : DesktopThread(1), m_req_commit(FALSE), m_indexed_messages(NULL) {}

	/** Destructor
	  */
	~SELexicon();

	/** Initialization - run this function before using other functions!
	  * @param path file directory
	  * @param table_name table name to cunstruct the file names
	  * @return error code / IS_TRUE if the tables existed / IS_FALSE if the table was newly created
	  */
	OP_BOOLEAN Init(const uni_char* path, const uni_char* table_name);

	/** Find all the file IDs belonging to at least one of the words, Flushes all cached data before the search
	  * @param words words (or a word prefixes) to search, separated be given characters
	  * @param result resulting (sorted) file IDs, mustn't be NULL
	  * @param match_any if FALSE, document must contain all the words, if TRUE, document must contain at least one word
	  * @param prefix_search if not 0, apply a prefix search for the last word and search for max. prefix_search prefixes
	  */
	OP_STATUS MultiSearch(const uni_char *words, OpINT32Vector *result, BOOL match_any, int prefix_search = 0);

	/** Find all the message IDs belonging to at least one of the words specified (partial matches allowed)
	  * Will call back with a MSG_M2_SEARCH_RESULTS message with results of search
	  * @param words Words to search for
	  * @param callback Where to deliver the result
	  */
	OP_STATUS AsyncMultiSearch(const OpStringC& words, MH_PARAM_1 callback);

	/** Find the indexed words, Flushes all cached data before the search
	  * @param word word (or a word prefix) to search
	  * @param result resulting words, mustn't be NULL, the fields must be freed by caller
	  * @param result_size maximum number of results on input, number of results on output
	  */
	OP_STATUS WordSearch(const uni_char *word, uni_char **result, int *result_size);

	/** Parse and insert a message into the lexicon
	  * @param message_id Message to insert
	  */
	OP_STATUS InsertMessage(message_gid_t message_id, BOOL headers_only = FALSE)
		{ return PostMessage(MSG_M2_INSERT_MESSAGE, (MH_PARAM_1)message_id, (MH_PARAM_2)headers_only); }

	/** Parse and remove a message from the lexicon and the store
	  * Will post message MSG_M2_MESSAGE_DELETED_FROM_LEXICON to the main thread when done
	  * NB: MH_PARAM_1 for callback message is specified here, MH_PARAM_2 will be message ID
	  * @param message_id Message to remove
	  * @param callback_msg_parameter First parameter to use on MSG_M2_MESSAGE_DELETED_FROM_LEXICON message
	  */
	OP_STATUS RemoveMessage(message_gid_t message_id, MH_PARAM_1 callback_msg_parameter)
		{ return PostMessage(MSG_M2_DELETE_MESSAGE, (MH_PARAM_1)message_id, (MH_PARAM_2)callback_msg_parameter); }

	/** From MessageObject
	  */
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
		{ OpStatus::Ignore(PostMessage(msg, par1, par2, 1)); }

	/** Checks the consitency of the stringtable and fixes it if needed
	  */
	void CheckConsistency();

private:
	struct IndexedMessage
	{
		message_gid_t   id;
		IndexedMessage* next;

		IndexedMessage(message_gid_t p_id, IndexedMessage* p_next) : id(p_id), next(p_next) {}
	};

	void	  HandleMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	OP_STATUS ChangeLexicon(message_gid_t message_id, BOOL add, BOOL headers_only);
	OP_STATUS ChangeWordsInLexicon(message_gid_t id, const OpStringC16& words, BOOL add);
	OP_STATUS HandleSearch(OpString* words, MH_PARAM_1 callback);
	OP_STATUS RequestCommit();
	OP_STATUS Commit();
	void	  CleanupIndexedMessages();

	OpString	 m_path;
	OpString	 m_table_name;
	StringTable  m_text_table;
	DesktopMutex m_table_mutex;
	int			 m_flush_mode;
	BOOL		 m_req_commit;
	IndexedMessage* m_indexed_messages;

	/* Looks up text in the message store for post-processing results in phrase search */
	class MessageSource: public DocumentSource<INT32>
	{
	public:
		MessageSource() {}
		const uni_char*			GetDocument(const INT32& id);

		StreamBuffer<uni_char>	m_doc;
	} m_message_source;
};


#endif // SE_LEXICON_H
