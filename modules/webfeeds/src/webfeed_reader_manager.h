// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef WEBFEED_READER_MANAGER_H_
#define WEBFEED_READER_MANAGER_H_

#ifdef WEBFEEDS_EXTERNAL_READERS

#include "modules/util/adt/opvector.h"
#include "modules/prefsfile/prefsfile.h"

/**
 * @brief This class maintains a list of external HTTP-based feedreaders.
 * 
 * This class can be used to request a list of known feed readers and their
 * subscription URLs.
 */
class WebFeedReaderManager
{
public:
	enum ReaderSource
	{
		READER_PREINSTALLED,  // Reader is shipped with Opera
		READER_CUSTOM         // Reader is added by user
	};

public:
	WebFeedReaderManager() : m_prefsfile(PREFS_INI), m_next_free_custom_id(0), m_custom_webfeed_id_origin(10000) { }

	/** Initialize the reader manager with default reader file
     * Reader file should be in OPFILE_INI_FOLDER/feedreaders.xml
     */
	OP_STATUS Init();

	/** Initialize the reader manager
	 * \param prefsfile List of available readers
	 */
	OP_STATUS Init(PrefsFile* prefs_file, BOOL preinstalled);

	/** \return Number of readers known
	 */
	unsigned GetCount() const;

	/** Get the unique identifier of a feed reader. This ID can
	 * be used to retrieve other details about the feed reader.
	 * \param index Index of reader, must be less than GetReaderCount()
	 * \return ID of reader at a specified index
	 */
	unsigned GetIdByIndex(unsigned index) const;

	/** Get the name for a reader
	 */
	OP_STATUS GetName(unsigned id, OpString& name) const;

	/** Get the source for a reader
	 */
	OP_STATUS GetSource(unsigned id, ReaderSource& source) const;

	/** Get a URL to subscribe a feed in a certain reader
	 * \param id ID of the feed reader to use
	 * \param feed_url URL of the feed to subscribe
	 * \param target_url If successful, contains the URL to use to subscribe to feed_url
	 */
	OP_STATUS GetTargetURL(unsigned id, const URL& feed_url, URL& target_url) const;

	/** Get an unused identifier
	 */
	unsigned GetFreeId();

#ifdef WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	/** Add a feed reader
	 *
	 * \param id[in] ID of feed reader to add
	 * \param feed_url[in] The url of a feed reader to add
	 * \param feed_name[in] The name of a feed reader to add */
	OP_STATUS AddReader(unsigned id, const uni_char *feed_url, const uni_char *feed_name);

	/** Check if a feed reader is already known
	 *
	 * \param feed_url[in] The url of a feed reader
	 * \param id[out] If not NULL and the function returns TRUE
	 * it contains the id of the known feed reader. Otherwise it's irrelevant.
	 *
	 * @note This function does string comparison and
	 * the same URL may have different string representation depending on
	 * e.g. the name attribute variant used. It's up to a caller to take care of
	 * passing proper url string form in order to get the proper answer.
	 * It basically needs to be the same as passed to AddReader().
	 * \see AddReader()
	 *
	 */
	BOOL HasReader(const uni_char *feed_url, unsigned *id) const;

	/** Deletes external feed reader
	 *
	 * \param id[in] ID of feed reader to delete */
	OP_STATUS DeleteReader(unsigned id);
#endif // WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS

private:
	struct WebfeedReader
	{
		unsigned id;
		ReaderSource source;
		OpString name;
		OpString subscribe_url;

		WebfeedReader() : id(0) {}
	};

	WebfeedReader* GetReaderById(unsigned id) const;

	OpAutoVector<WebfeedReader> m_readers;

	PrefsFile m_prefsfile;

	unsigned m_next_free_custom_id;
	const unsigned m_custom_webfeed_id_origin;
};

#endif // WEBFEEDS_EXTERNAL_READERS

#endif // WEBFEED_READER_MANAGER_H_
