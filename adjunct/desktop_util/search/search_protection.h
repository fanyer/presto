// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef SEARCH_PROTECTION_H
#define SEARCH_PROTECTION_H

#ifdef DESKTOP_UTIL_SEARCH_ENGINES

#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/libssl/sslv3.h"
#include "modules/locale/oplanguagemanager.h"

class OpFileDescriptor;
class SearchTemplate;

// Two special macros can be defined to disable parts of search protection in Debug builds:
// SEARCH_PROTECTION_DISABLE_PROTECTION - when defined default searches are assumed to be ok
//                                        (CheckSearch always returns IS_TRUE)
// SEARCH_PROTECTION_DISABLE_SIGNATURES - when defined search.ini files are assumed to be ok
//                                        (SignatureChecker::VerifyFile always returns IS_TRUE)

namespace SearchProtection
{
	class Digest;

	/**
	 * Types of protected searche engines.
	 */
	enum SearchType
	{
		DEFAULT_SEARCH = 1,
		SPEED_DIAL_SEARCH = 2
	};

	/**
	 * Hardcoded search engine.
	 */
	class SearchEngine
	{
	public:
		SearchEngine();

#ifdef SELFTEST
		void Reset();
#endif

		/**
		 * Init this object as search engine with given index in given prefs file.
		 * Pointer passed in prefs must remain valid until this object is deleted
		 * (or re-initialized with different prefs).
		 *
		 * @param prefs INI file with search engine definitions
		 * @param index index of search engine in prefs (zero-based)
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR if prefs does not contain search engine with specified index
		 */
		OP_STATUS Init(PrefsFile* prefs, size_t index);

		/**
		 * Match this hardcoded search engine against given search template.
		 * Matching is similar to ui matching of search templates with the
		 * following differences:
		 * - search key and personalbar position are ignored
		 * - suggest URL is checked in addition to search URL
		 *
		 * @param search search template to compare
		 *
		 * @return
		 *  - IS_TRUE on match
		 *  - IS_FALSE if different
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_BOOLEAN IsMatch(const SearchTemplate* search) const;

		/**
		 * Initialize search template with data from this hardcoded search.
		 * Sets SearchStore of search template to HARDCODED.
		 *
		 * @param search search template to initialize
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS InitSearchTemplate(SearchTemplate& search) const;

		/**
		 * Create new search template and initialize it with data from
		 * this hardcoded search.
		 * SearchStore of created search template is set to HARDCODED.
		 *
		 * @param search on success gets pointer to new search template
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS CreateSearchTemplate(SearchTemplate*& search) const;

		BOOL IsValid() const { return m_is_valid; }

		/**
		 * Get hash of hardcoded search engine represented by this object.
		 *
		 * @return hash or empty string
		 */
		OpStringC GetHashL() const { return ReadStringL("Hash"); }

		/**
		 * Get GUID of hardcoded search engine represented by this object.
		 *
		 * @return GUID or empty string
		 */
		OpStringC GetUniqueIdL() const { return ReadStringL("UNIQUEID"); }

	private:
		BOOL m_is_valid;
		PrefsFile* m_prefs;       // owned by HardcodedSearchEngines
		OpString8 m_section_name;

		// used to speed up recurring calls to IsMatch
		mutable OpAutoPtr<SearchTemplate> m_search;
		mutable OpString m_suggest_url;
		mutable BOOL m_suggest_url_inited;

		OpStringC ReadStringL(const char* name) const
		{
			if (!IsValid())
				LEAVE(OpStatus::ERR);
			return m_prefs->ReadStringL(m_section_name.CStr(), name);
		}
	};

	/**
	 * Configuration of default search engines for region and language.
	 */
	struct SearchConfiguration
	{
		const char* m_region; // OpStringC8 here breaks PGO builds on Unix (DSK-339814)
		const char* m_language; // OpStringC8 here breaks PGO builds on Unix (DSK-339814)
		unsigned short m_default_engine_index; // index of hardcoded search engine
		unsigned short m_speeddial_engine_index; // index of hardcoded search engine
	};

	/**
	 * Set of hardcoded search engines.
	 */
	class HardcodedSearchEngines
	{
	public:
		HardcodedSearchEngines() : m_search_engines_count(0) {}

#ifdef SELFTEST
		virtual ~HardcodedSearchEngines() {}
#endif

		/**
		 * Load hardcoded search engines and decide which of them should be used as fallbacks
		 * for tampered search engines.
		 * Package searches passed as arguments are only used to select fallback search
		 * engines.
		 *
		 * @param package_default_search default search from package search.ini, or NULL
		 * @param package_speeddial_search speed dial search from package search.ini, or NULL
		 *
		 * @return
		 *  - OK if fallback search engines for all search types were initialized successfully
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR if not all fallback search engines were initliazed successfully
		 */
		OP_STATUS Init(const SearchTemplate* package_default_search = NULL,
					   const SearchTemplate* package_speeddial_search = NULL);

		/**
		 * Get fallback search engine for given search type.
		 *
		 * @param type type of protected search
		 *
		 * @return pointer to fallback search engine (owned by HardcodedEngines), or NULL if not found
		 */
		const SearchEngine* GetSearchEngine(SearchType type) const;

		/**
		 * Check if fallback search engines for all search types are valid.
		 *
		 * @return TRUE if valid, FALSE otherwise.
		 */
		BOOL IsValid() const { return m_default_engine.IsValid() && m_speeddial_engine.IsValid(); }

		/**
		 * Check if there exists custom configuration of hardcoded searches for current region and/or language.
		 *
		 * @return TRUE if custom configuration exists
		 */
		static BOOL HasLocalizedSearches();

	protected:

		// Second (LoadSearchEngines) and third (InitSearchEngines) stage of initialization.
		// In normal usage Init calls both (so it is usual two-stage initialization) - they
		// are separated only to simplify selftests.

		/**
		 * Load search engine definitions from INI file.
		 * Search engines in ini_file must be numbered from 0 to count-1.
		 * In addtion to normal search engine options each section should
		 * have 'Hash' option with hash of all other options.
		 *
		 * @param ini_file INI file with definitions of search engines (copied by this function)
		 * @param count number of search engine definitions in ini_file
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR on other errors
		 */
		OP_STATUS LoadSearchEngines(OpFileDescriptor* ini_file, size_t count);

		/**
		 * Initialize fallback search engines for all search types.
		 * IDs of fallback searches are stored in operaprefs.ini. If ID is missing
		 * or invalid then fallback search is selected from the following list:
		 * - hardcoded search that matches package search,
		 * - hardcoded search specified by search configuration for current UI language,
		 * - hardcoded search from global configuration.
		 *
		 * @param package_default_search default search from package search.ini, or NULL
		 * @param package_speeddial_search speed dial search from package search.ini, or NULL
		 * @param global_default_search_index index of default search for locales without customized search.ini
		 * @param global_speeddial_search_index index of speed dial search for locales without customized search.ini
		 * @param configurations search configurations for locales with custom package search.ini's (sorted by locale), or NULL
		 * @param configurations_count number of configurations
		 *
		 * @return
		 *  - OK if fallback search engines for all search types were initialized successfully
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR if not all fallback search engines could be initialized
		 */
		OP_STATUS InitSearchEngines(const SearchTemplate* package_default_search,
									const SearchTemplate* package_speeddial_search,
									size_t global_default_search_index,
									size_t global_speeddial_search_index,
									const SearchConfiguration* configurations,
									size_t configurations_count);

#ifdef SELFTEST
		// Implemented by selftests to override region settings in Opera.
		// If override is active function should set argument and return IS_TRUE.
		virtual OP_BOOLEAN GetRegionOverride(OpString8& region) const { return OpBoolean::IS_FALSE; }
		virtual OP_BOOLEAN GetDefaultLanguageOverride(OpString8& language) const { return OpBoolean::IS_FALSE; }
		virtual OP_BOOLEAN GetLanguageOverride(OpString8& language) const { return OpBoolean::IS_FALSE; }
#endif // SELFTEST

	private:

		OpAutoPtr<PrefsFile> m_search_engines; // INI file with hardcoded search engines
		size_t m_search_engines_count; // number of search engines in the INI file

		SearchEngine m_default_engine; // fallback search engine used if default search is tampered
		SearchEngine m_speeddial_engine; // fallback search engine used if speed dial search is tampered

		BOOL IsValidEngineIndex(size_t index) const { return index < m_search_engines_count; }


		/**
		 * Get user's region, ignore override set by selftests.
		 *
		 * @param region gets name of the region
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		static OP_STATUS GetRegionNoOverride(OpString8& region)
		{
			return region.Set(g_region_info->m_region);
		}

		/**
		 * Get default language for region returned by GetRegionNoOverride, ignore override
		 * set by selftests.
		 *
		 * @param default_language gets name of the language or empty string if there is no default language for the region
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		static OP_STATUS GetDefaultLanguageNoOverride(OpString8& default_language)
		{
			return default_language.Set(g_region_info->m_default_language);
		}

		/**
		 * Get UI language, ignore override set by selftests.
		 *
		 * @param language gets name of the language
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		static OP_STATUS GetLanguageNoOverride(OpString8& language)
		{
			return language.Set(g_languageManager->GetLanguage());
		}

		/**
		 * Get user's region.
		 *
		 * @param region gets name of the region
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS GetRegion(OpString8& region) const
		{
#ifdef SELFTEST
			OP_BOOLEAN result = GetRegionOverride(region);
			RETURN_IF_ERROR(result);
			if (result == OpBoolean::IS_TRUE) return OpStatus::OK;
#endif // SELFTEST
			return GetRegionNoOverride(region);
		}

		/**
		 * Get default language for region returned by GetRegion.
		 *
		 * @param language gets name of the language or empty string if there is no default language for the region
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS GetDefaultLanguage(OpString8& language) const
		{
#ifdef SELFTEST
			OP_BOOLEAN result = GetDefaultLanguageOverride(language);
			RETURN_IF_ERROR(result);
			if (result == OpBoolean::IS_TRUE) return OpStatus::OK;
#endif // SELFTEST
			return GetDefaultLanguageNoOverride(language);
		}

		/**
		 * Get UI language.
		 *
		 * @param language gets name of the language
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS GetLanguage(OpString8& language) const
		{
#ifdef SELFTEST
			OP_BOOLEAN result = GetLanguageOverride(language);
			RETURN_IF_ERROR(result);
			if (result == OpBoolean::IS_TRUE) return OpStatus::OK;
#endif // SELFTEST
			return GetLanguageNoOverride(language);
		}

		/**
		 * Create string that contains concatenation of region name and UI language.
		 */
		OP_STATUS GetRegionAndLanguage(OpString8& region_and_language) const
		{
			OpString8 lang;
			RETURN_IF_ERROR(GetLanguage(lang));
			OpString8 result;
			RETURN_IF_ERROR(GetRegion(result));
			RETURN_IF_ERROR(result.Append(lang));
			return region_and_language.TakeOver(result);
		}


		/**
		 * Find index of hardcoded search engine by ID stored in operaprefs.ini.
		 *
		 * @param type type of search
		 * @param index gets index of hardcoded search engine or UINT_MAX
		 *
		 * @return
		 *  - OK if there were no errors (also if not found)
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS FindByStoredId(SearchType type, size_t& index) const;

		/**
		 * Find indexes of hardcoded searches by searches read from package search.ini.
		 *
		 * @param package_default_search default search read from package search.ini
		 * @param default_engine_index gets index of hardcoded search engine or UINT_MAX
		 * @param package_speeddial_search speed dial search read from package search.ini
		 * @param speeddial_engine_index gets index of hardcoded search engine or UINT_MAX
		 *
		 *
		 * @return
		 *  - OK if there were no errors (also if not all indexes were found)
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS FindByPackageSearches(const SearchTemplate* package_default_search,
									    size_t& default_engine_index,
										const SearchTemplate* package_speeddial_search,
										size_t& speeddial_engine_index) const;

		/**
		 * Set hardcoded search engine with specified index to act as a fallback search
		 * engine for specified search type.
		 * ID of hardcoded search is stored in operaprefs.ini.
		 *
		 * @param type search type
		 * @param index index of hardcoded search (in m_search_engines)
		 * @param engine fallback search engine to initialize
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR on other errors
		 */
		OP_STATUS InitFromEngineIndex(SearchType type, size_t index, SearchEngine& engine);
	};

	/**
	 * RSA signatures of Opera resource files.
	 */
	class SignatureStore
	{
	public:
		/**
		 * Read signatures from a file.
		 * Each line in file should specify signature for single resource file:
		 * <path_or_digest> <signature>
		 * path_or_digest: path relative to resources folder or custom resources folder
		 *                 or digest of a file (this is used for files_old.sig, which can contain
		 *                 many signatures for single path)
		 * signature: RSA signature as a hex string
		 *
		 * Line for file [destop/work]/adjunct/resources/locale/en/search.ini
		 * that is installed into [installdir]/locale/en/search.ini should look like:
		 * locale/en/search.ini 3cba9cb7aab4be186be5...
		 * Line for file [ubs/work]/platforms/win-package/special_builds/Yandex2/contents/custom/defaults/search.ini
		 * that is installed into [profiledir]/custom/defaults/search.ini should look like:
		 * defaults/search.ini 1e189f00fec3e4addaa6...
		 *
		 * @param path path to file with signatures
		 *
		 * @return
		 *  - OK on success (also if file does not exist or is empty)
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR on IO errors (file exists, but can't be read) and other errors
		 */
		OP_STATUS Init(OpStringC path);

#ifdef SELFTEST
		/**
		 * Copy signatures from string. Format of signatures string is the same
		 * as format of signatures file.
		 *
		 * @param signatures string with signatures (can be NULL)
		 * @param length length of signatures string
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS Init(const char* signatures, size_t length);
#endif // SELFTEST

		/**
		 * Find signature of a file by its digest.
		 *
		 * @param digest digest of a file
		 * @param signature gets signature if found
		 *
		 * @return
		 *  - IS_TRUE if signature was found
		 *  - IS_FALSE if signature was not found
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_BOOLEAN GetByDigest(const OpStringC8& digest, OpString8& signature);

		/**
		 * Find signature of a file by its path.
		 *
		 * @param path path of a file
		 * @param signature gets signature if found
		 *
		 * @return
		 *  - IS_TRUE if signature was found
		 *  - IS_FALSE if signature was not found
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_BOOLEAN GetByPath(const OpStringC& path, OpString8& signature);

	private:
		OpAutoArray<char> m_strings; // contents of signatures file broken into words
		OpString8HashTable<const char> m_signatures; // path/digest -> signature; keys and values are pointers into m_strings

		/**
		 * Split m_strings into words and init m_signatures.
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		OP_STATUS InitSignatures();
	};

	/**
	 * Validation of package search.ini files
	 */
	class SignatureChecker
	{
	public:

		/**
		 * Verify that file folder/name matches RSA signature stored in files.sig.
		 * This function assumes that file folder/name exists and is readable.
		 *
		 * @param name name of the file
		 * @param folder where to look for file; also determines location of files.sig
		 *
		 * @return
		 *  - IS_TRUE if file matches signature
		 *  - IS_FALSE if file does not match signature or there is no signature for it
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR on IO errors and other errors
		 */
		OP_BOOLEAN VerifyFile(const uni_char* name, OpFileFolder folder);

		/**
		 * Compute path to folder/name as seen from base_folder. This function only
		 * works if folder is descendat of base_folder or if folder == base_folder.
		 * Otherwise it returns absolute path.
		 *
		 * @param name name of the file
		 * @param folder folder of the file
		 * @param base_folder base folder for relative path
		 * @param rel_path gets relative path
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		static OP_STATUS GetRelativePath(const uni_char* name, OpFileFolder folder, OpFileFolder base_folder, OpString& rel_path);

	private:
		OpAutoPtr<SSL_PublicKeyCipher> m_cipher;                  // RSA cipher used to verify files against signatures
		OpAutoPtr<SignatureStore> m_regular_signatures; // signatures for resource files from regular build
		OpAutoPtr<SignatureStore> m_custom_signatures;  // signatures for resource files from repacked build
		OpAutoPtr<SignatureStore> m_legacy_signatures;  // signatures for resource files from old repacked builds

		/**
		 * Create signature store and load it with signatures from folder/name.
		 *
		 * @param name name of file
		 * @param folder folder
		 * @param store gets pointer to created signature store
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR on IO errors and other errors
		 */
		static OP_STATUS LoadSignatures(const uni_char* name, OpFileFolder folder, OpAutoPtr<SignatureStore>& store);

		/**
		 * Encode contents of SSL_varvector as a hex string.
		 *
		 * @param src vector to encode
		 * @param dest gets encoded hex string
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 */
		static OP_STATUS EncodeHex(const SSL_varvector32& src, OpString8& dest);

		/**
		 * Decode hex string.
		 *
		 * @param src hex string to decode
		 * @param dest gets decoded string
		 *
		 * @return
		 *  - OK on success
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR if src is not a valid hex string
		 */
		static OP_STATUS DecodeHex(const OpStringC8& src, SSL_varvector32& dest);

		/**
		 * Verify SHA digest of a file against set of RSA signatures.
		 *
		 * @param digest digest of a file
		 * @param signature set of RSA signatures
		 * @param by_path search signature by path (TRUE) or by digest (FALSE)
		 * @param path path of a file (used when by_path is TRUE)
		 *
		 * @return
		 *  - IS_TRUE if signature was found and digest was verified
		 *  - IS_FALSE if digest could not be verified
		 *  - ERR_NO_MEMORY on OOM
		 *  - ERR on other errors
		 */
		OP_BOOLEAN VerifyDigest(Digest& digest, SignatureStore* signatures, BOOL by_path, const OpStringC& path);
	};

	/**
	 * Set internal flag that invalidates IDs of fallback search engines in operaprefs.ini.
	 * IDs will be initialized on first call to InitIdsOfHardcodedSearches or HardcodedSearchEngines::Init.
	 */
	void ClearIdsOfHardcodedSearches();

	/**
	 * Check IDs of fallback search engines and initialize those that are missing or invalid.
	 *
	 * @param package_default_search default search read from package search.ini, or NULL
	 * @param package_speeddial_search speed dial search read from package search.ini, or NULL
	 */
	OP_STATUS InitIdsOfHardcodedSearches(const SearchTemplate* package_default_search,
										 const SearchTemplate* package_speeddial_search);


	/**
	 * Check whether search is valid wrt. the protection data set for given search type.
	 *
	 * @param type type of protected search
	 * @param search search template to check, should be NULL if default search is not set for given type
	 * @return
	 *  - IS_TRUE if search is valid
	 *  - IS_FALSE if search is invalid
	 *  - ERR_NO_MEMORY on OOM
	 */
	OP_BOOLEAN CheckSearch(SearchType type, const SearchTemplate* search);

	/**
	 * Set protection data for given search type based on given search template.
	 * If this function succeeds subsequent calls to CheckSearch with the same
	 * arguments should return IS_TRUE.
	 *
	 * @param type type of protected search
	 * @param search search template to protect, should be NULL if default search is not set for given type
	 * @return
	 *  - OK on success
	 *  - ERR_NO_MEMORY on OOM
	 */
	OP_STATUS ProtectSearch(SearchType type, const SearchTemplate* search);

#ifdef SELFTEST

	/**
	 * Overrides for search protection prefs in operaprefs.ini.
	 */
	class ProtectionDataStore
	{
	public:
		virtual ~ProtectionDataStore() {}

		virtual const OpStringC ReadPref(PrefsCollectionUI::stringpref pref) = 0;

		virtual OP_STATUS WritePref(PrefsCollectionUI::stringpref pref, const OpStringC& value) = 0;
	};

	void SetProtectionDataStore(ProtectionDataStore* store);

	OP_STATUS ComputeDigest(const uni_char* name, OpFileFolder folder, OpString8& digest);

#endif // SELFTEST
}

#endif // DESKTOP_UTIL_SEARCH_ENGINES

#endif // !SEARCH_PROTECTION_H
