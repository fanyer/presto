/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGA_BACKEND_BLOCKLIST_FILE_H
#define VEGA_BACKEND_BLOCKLIST_FILE_H

#include "modules/util/adt/opvector.h"
#include "platforms/vega_backends/vega_blocklist_device.h"

class OpFile;

void DeleteBlocklistEntryPair(const void* key, void* val);

/**
   a regex in a blocklist entry. this regex is applied to a string
   fetched from the platform (associated with m_key). any
   subexpressions are assumed to be integers, and are used to do
   version number comparisons.
 */
struct VEGABlocklistRegexEntry
{
	/**
	   the avialable comparisons to do on driver version entries
	 */
	enum Comparison
	{
		EqualTo,
		NotEqualTo,
		GreaterThan,
		GreaterThanOrEqual,
		LessThan,
		LessThanOrEqual,
		CompCount // last!
	};
	static Comparison GetComp(const uni_char* comp);

	/**
	   a regex comparison entry, consising of a comparison operator
	   and a set of values. all comparisons are always true if
	   m_values is empty.
	 */
	struct RegexComp
	{
		BOOL Matches(const OpINT32Vector& values);
		Comparison m_comp;
		OpINT32Vector m_values;
	};

	VEGABlocklistRegexEntry() : m_key(0), m_exp(0) {}
	~VEGABlocklistRegexEntry()
	{
		OP_DELETEA(m_key);
		OP_DELETEA(m_exp);
	}

	/**
	   check whether regex m_exp matches str. if m_exp contains
	   subexpressions, m_comp is used to perform version number
	   comparisons.
	 */
	OP_BOOLEAN Matches(const uni_char* str);

	const uni_char* m_key; /// the key in the blocklist associated with the regex
	const uni_char* m_exp; /// the regular expression to apply
	RegexComp m_comp[2];   /// 0-2 comparisons to apply on the subexpressions of the regex
};

struct VEGABlocklistRegexCollection
{
	// obtains ownership of e
	OP_STATUS AddEntry(VEGABlocklistRegexEntry* e)
	{
		OP_ASSERT(e);
		return regex_entries.Add(e->m_key, e);
	}

	/**
	   compares a blocklist entry against a device
	   @param dev the device to compare against
	   @return TRUE if this blocklist entry matches the info fetched from the device
	 */
	OP_BOOLEAN Matches(VEGABlocklistDevice::DataProvider* provider);

	OpAutoStringHashTable<VEGABlocklistRegexEntry> regex_entries;
};

/**
   data structure corresponding to an entry in a blocklist file.
 */
struct VEGABlocklistFileEntry : public VEGABlocklistRegexCollection
{
	VEGABlocklistFileEntry():
		status2d(VEGABlocklistDevice::Supported),
		status3d(VEGABlocklistDevice::Supported),
		reason2d(0),
		reason3d(0)
	{}
	~VEGABlocklistFileEntry()
	{
		OP_DELETEA(reason2d);
		OP_DELETEA(reason3d);
		string_entries.ForEach(DeleteBlocklistEntryPair);
	}
	// obtains ownership of key and val
	OP_STATUS AddEntry(const uni_char* key, uni_char* val)
	{
		OP_ASSERT(key);
		OP_ASSERT(val);
		return string_entries.Add(key, val);
	}

	OpStringHashTable<uni_char> string_entries;
	VEGABlocklistDevice::BlocklistStatus status2d, status3d;
	uni_char *reason2d, *reason3d;
};

struct VEGABlocklistDriverLinkEntry : public VEGABlocklistRegexCollection
{
	// obtains ownership of driver link
	VEGABlocklistDriverLinkEntry() : m_driver_link(0) {}
	~VEGABlocklistDriverLinkEntry() { OP_DELETEA(m_driver_link); }

	const uni_char* m_driver_link;
};


/**
   mixin for loading blocklists. each time an entry is loaded
   OnElementLoaded is called.
 */
class VEGABlocklistFileLoaderListener
{
public:
	virtual ~VEGABlocklistFileLoaderListener() {}

	/**
	   called when the version of the blocklist has been read
	   @param blocklist_version the version of the blocklist
	   @return OpStatus::OK if all went well (loading will continue),
	   or some kind of error (loading will be aborted)
	 */
	virtual OP_STATUS OnVersionLoaded(const uni_char* blocklist_version) { return OpStatus::OK; }
	/**
	   called when a driver link entry has been loaded
	   @param driver_link the loaded entry - ownership is passed on to
	   listener and should be OP_DELETEd
	   @return OpStatus::OK if all went well (loading will continue),
	   or some kind of error (loading will be aborted)
	 */
	virtual OP_STATUS OnDriverEntryLoaded(VEGABlocklistDriverLinkEntry* driver_link) { OP_DELETE(driver_link); return OpStatus::OK; }
	/**
	   called when an entry has been read from the blocklist
	   @param entry the loaded entry - ownership is passed on to
	   listener and should be OP_DELETEd
	   @return OpStatus::OK if all went well (loading will continue),
	   or some kind of error (loading will be aborted)
	*/
	virtual OP_STATUS OnElementLoaded(VEGABlocklistFileEntry* blocklist_entry) { OP_DELETE(blocklist_entry); return OpStatus::OK; }
};

/**
   abstract interface for loading a blocklist file. progress is
   reported via VEGABlocklistFileLoaderListener::OnElementLoaded.
 */
class VEGABlocklistFileLoader
{
public:
	virtual ~VEGABlocklistFileLoader() {}

	/**
	   blocklist file loader factory - there should be one blend
	   only. to be provided by implementation.
	   @param loader (out) the created loader
	   @param listener the listener to which loaded entries are reported
	   @return
	   OpStatus::OK if the blocklist file loader was successfully created - caller should OP_DELETE
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS Create(VEGABlocklistFileLoader*& loader, VEGABlocklistFileLoaderListener* listener);
	/**
	   load the blocklist file and report loaded elements to m_listener
	   OpStatus::OK if the blocklist file was successfully loaded
	   OpStatus::ERR on format error (can also be signalled by listener)
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS Load(OpFile* file) = 0;

protected:
	VEGABlocklistFileLoader(VEGABlocklistFileLoaderListener* listener)
	: m_listener(listener)
	{}
	VEGABlocklistFileLoaderListener* m_listener;
};

/**
   handle to a blocklist file. contains static functionality to keep
   track of and updating blocklist files.
 */
class VEGABlocklistFile : public VEGABlocklistFileLoaderListener
{
#ifdef SELFTEST
	friend class ST_VEGABlocklistFileAccessor;
#endif // SELFTEST

public:
	VEGABlocklistFile() : m_blocklist_version(0)
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
						, m_blocklist_fetched(0)
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH
	{}
	~VEGABlocklistFile() { OP_DELETEA(m_blocklist_version); }

	/**
	   load device blocklist for type. will trigger re-fetch if
	   blocklist could not be loaded or has expired.

	   @param type the blocklist to load
	   @return
	   OpStatus::OK if list was successfully loaded
	   OpStatus::ERR if blocklist could not be found or was corrupt
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Load(VEGABlocklistDevice::BlocklistType type);

	const uni_char* GetVersion() const { return m_blocklist_version; }
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	/// @return time blocklist file was fetched, in UTC seconds.
	time_t GetFetched() const { return m_blocklist_fetched; }
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	/**
	   looks for first entry in blocklist matching a device
	   @param data_provider the data provider associated with the device
	   @param entry (out) the first matching entry, or NULL if none is found
	   @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise
	 */
	OP_STATUS FindMatchingEntry(VEGABlocklistDevice::DataProvider* const data_provider, VEGABlocklistFileEntry*& entry) const;

	/**
	   looks for first entry in driver list matching a device
	   @param data_provider the data provider associated with the device
	   @param entry (out) the first matching entry, or NULL if none is found
	   @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise
	 */
	OP_STATUS FindMatchingDriverLink(VEGABlocklistDevice::DataProvider* const data_provider, VEGABlocklistDriverLinkEntry*& entry) const;

	/**
	   fetch name of the blocklist file associated with type.
	   @return the name of the local blocklist file associated with
	   type. the name corresponds to a file located in one of the
	   folders specified by tweaks:
	   TWEAK_VEGA_BACKENDS_BLOCKLIST_FETCHED_FOLDER (fetched) and
	   TWEAK_VEGA_BACKENDS_BLOCKLIST_SHIPPED_FOLDER (bundled).
	*/
	static const uni_char* GetName(VEGABlocklistDevice::BlocklistType type);

	/**
	   check if the blocklist file can be opened and parsed; return
	   appropriate status.
	   @param file the file to check
	   @param fetched_time if non-NULL, will be set to the time the blocklist file was fetched, in UTC seconds, on success
	   @return OpStatus::OK if the file opens and parses fine
	*/
	static OP_STATUS CheckFile(OpFile* file
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
		, time_t* fetched_time = 0
#endif
		);

private:
	static OP_STATUS OpenFile(VEGABlocklistDevice::BlocklistType type, OpFile* file, BOOL* from_resources);

	/**
	   load device blocklist file
	   @param file the blocklist file
	 */
	OP_STATUS Load(OpFile* file);
	/**
	   see VEGABlocklistFileLoaderListener::OnVersionLoaded
	 */
	OP_STATUS OnVersionLoaded(const uni_char* version) { return UniSetStr(m_blocklist_version, version); }
	/**
	   see VEGABlocklistFileLoaderListener::OnDriverEntryLoaded
	 */
	OP_STATUS OnDriverEntryLoaded(VEGABlocklistDriverLinkEntry* driver_link);
	/**
	   see VEGABlocklistFileLoaderListener::OnElementLoaded
	 */
	OP_STATUS OnElementLoaded(VEGABlocklistFileEntry* entry);
	/// the entries from the loaded blocklist file
	OpAutoVector<VEGABlocklistFileEntry> m_entries;

	// static functionality

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	/**
	   fetch URL to the blocklist file associated with type. this file
	   is continuously updated and stored in GetName(type) relative to
	   the folder specified by tweak VEGA_BACKENDS_BLOCKLIST_SHIPPED_FOLDER
	   @return the URL to the blocklist file associated with type.
	 */
	static OP_STATUS GetURL(VEGABlocklistDevice::BlocklistType type, OpString& s);
	/**
	   triggers asynchronous fetch of blocklist file via VegaBackendsModule.
	   @param type the blocklist file to fetch
	   @param delay the delay (in ms) before fetching is started
	*/
	static void Fetch(VEGABlocklistDevice::BlocklistType type, unsigned long delay);
	/**
	   triggers re-fetch when blocklist file expires. update interval
	   is controlled by TWEAK_VEGA_BACKENDS_BLOCKLIST_UPDATE_INTERVAL.
	   @param type the blocklist file to fetch
	   @param mod the time the blocklist file was last fetched, in UTC seconds
	*/
	static void FetchWhenExpired(VEGABlocklistDevice::BlocklistType type, time_t mod);
	/**
	   called when fetching fails, or when reading of the fetched file
	   fails. triggers a new attempt at a later time. retry interval
	   is controlled by TWEAK_VEGA_BACKENDS_BLOCKLIST_RETRY_INTERVAL.
	*/
	static void FetchLater(VEGABlocklistDevice::BlocklistType type);
	/**
	   called when a new version of the blocklist file has been
	   downloaded. will load the new file and replace it in
	   GetName(type) relative to the folder specified by tweak
	   TWEAK_VEGA_BACKENDS_BLOCKLIST_FETCHED_FOLDER, or trigger
	   re-fetch on error.
	   @param url the URL of the loaded blocklist file
	   @param type the type associated with the blocklist file
	   @return OpStatus::OK on success
	   OpStatus::ERR on failure to read the file
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS OnBlocklistFetched(URL url, VEGABlocklistDevice::BlocklistType type);
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	friend class VegaBackendsModule;

	uni_char* m_blocklist_version;
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	/// time blocklist file was fetched, in UTC seconds.
	time_t m_blocklist_fetched;
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	OpAutoVector<VEGABlocklistDriverLinkEntry> m_driver_links;
};

#ifdef SELFTEST
/**
   allow selftests to access to some internal structures
 */
class ST_VEGABlocklistFileAccessor
{
public:
	static const uni_char* GetName(VEGABlocklistDevice::BlocklistType type) { return VEGABlocklistFile::GetName(type); }
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	static void Fetch(VEGABlocklistDevice::BlocklistType type, unsigned long delay) { return VEGABlocklistFile::Fetch(type, delay); }
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH
	ST_VEGABlocklistFileAccessor(VEGABlocklistFile* file) : m_file(file) {}
	OpAutoVector<VEGABlocklistFileEntry>* GetEntries() { return &m_file->m_entries; }
	OP_STATUS Load(OpFile* file) { return m_file->Load(file); }
	VEGABlocklistFile* m_file;
};
#endif // SELFTEST

#endif // VEGA_BACKEND_BLOCKLIST_FILE_H
