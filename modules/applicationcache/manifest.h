/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 */

#ifndef MANIFEST
#define MANIFEST

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/url/url2.h"

class Manifest {
	public:

		/**
		 * Section Type
		 *
		 * Enum enlist all sections, according to HTML 5 spec.
		 */
		enum SectionType
		{
			CACHE,
			NETWORK,
			FALLBACK,
			UNKNOWN
		};

		class Namespace
		{
				const OpAutoPtr<const OpString> m_namespace_url_str;

				// No default constructor
				Namespace ();

				// No default assign operator
				Namespace& operator = (const Namespace&);


		protected:

				explicit inline
				Namespace (const OpString*const namespace_url_str)
					:   m_namespace_url_str (namespace_url_str)
				{
					OP_ASSERT (namespace_url_str); // namespace must not be NULL!
				}


			public:

				static
				OP_STATUS BuildNamespace (
					const uni_char*const ns_url_str,
					const Namespace*& new_namespace
				);

				OP_STATUS Clone (const Namespace*& dst) const;

				inline
				const OpStringC& GetNemespaceUrl () const
				{
					return *m_namespace_url_str.get ();
				}

				// in order to properly handle deletion of inherited classes
				virtual
				~Namespace() {}
		};

		class Fallback : public Namespace
		{
				const OpAutoPtr<const OpString> m_entity_url_str;

				// No default constructor
				Fallback ();

				// No default assign operator
				Fallback& operator = (const Fallback&);

				explicit inline
				Fallback (const OpString*const namespace_url_str, const OpString*const entity_url_str)
					:   Namespace (namespace_url_str)
					,   m_entity_url_str (entity_url_str)
				{
					OP_ASSERT(entity_url_str);
				}

			public:

				static
				OP_STATUS BuildFallback (
					const uni_char*const namespace_url_str,
					const uni_char*const entry_url_str,
					const Fallback*& new_fallback
				);

				OP_STATUS Clone (const Fallback*& dst) const;

				inline
				const OpStringC& GetEntityUrl () const
				{
					return *m_entity_url_str.get ();
				}

				// inherited and implemented by default
				virtual
				~Fallback () {}
		};

		class LexicographicMap : private OpGenericVector
		{
			mutable BOOL m_is_fallback_sorted;

			/**
			 * Process and Continue
			 *
			 * This is an inner function that is called each time when data is being obtained from the vector.
			 * In case when the vector is empty, it simply skip the rest of its inner steps, but if the vector
			 * is not empty and if the data are not sorted, then the function sorts inner data and prepare them
			 * for the future using.
			 *
			 * @return FALSE when the vector is empty and TRUE in the rest of cases
			 */
			BOOL ProcessAndContinue () const;

		public:

			LexicographicMap ();
			~LexicographicMap ();

			inline
			UINT32 GetCount() const
			{
				return OpGenericVector::GetCount();
			}

			inline
			const Namespace* Get(UINT32 idx) const
			{
				return static_cast<const Namespace*>(OpGenericVector::Get (idx));
			}

			OP_STATUS Add (const Namespace* item);
			OP_STATUS Remove (const Namespace* item);

			const Namespace* Match (const OpStringC& namespace_url_str) const;

			BOOL IsPresent (const OpStringC& namespace_url_str) const;
	};

	OP_STATUS static MakeManifestFromDisk(Manifest *&manifest, const URL &manifest_url, const uni_char* manifest_file_path, OpFileFolder folder);

	Manifest (const URL& manifest_url);

	~Manifest () ;

	// Add Cache | Network | Fallback URLs

	OP_STATUS AddCacheUrl (const OpStringC& url_str);

	OP_STATUS AddNetworkUrl (const OpStringC& url_str);

	OP_STATUS AddFallbackUrls (const OpStringC& namespace_url_str, const OpStringC& url_str);


	BOOL CheckCacheUrl (const uni_char*const url_str) const;

	BOOL MatchNetworkUrl (const uni_char*const url_str) const;

	BOOL MatchFallback (const uni_char*const nemespace_url_str, const uni_char*& fallback_url_str) const;


	// TODO: documentation
	void SetOnlineOpen (BOOL is_online_open);

	// TODO: documentation
	BOOL IsOnlineOpen () const ;

	/**
	 * Get an iterator to iterate the HashBackend.
	 * The returned iterator is to be freed by the caller.
	 *
	 * @return the iterator for the HashBackend, or NULL if OOM.
	 */
	OpHashIterator* GetCacheIterator();
	OpAutoStringHashTable <const OpString> &GetEntryTable(){ return m_cache_entries; }

	OP_STATUS SetHash (const OpStringC& hash);
	const OpStringC& GetHash() const ;

	OP_STATUS Clone (Manifest*& dst) const;

	URL GetManifestUrl() const { return m_manifest_url; }
private:

	explicit
	Manifest (const Manifest& manifest);

	OP_STATUS ProcessUrl(const OpStringC& url_str,SectionType section_type);

	OP_STATUS ProcessUrl(const OpStringC& namespace_url_str, const OpStringC& entity_url_str);

	/**
	 * Process URL
	 *
	 * The function performs a standard procedure of URL processing that encompasses the following steps:
	 *  # resolve URL
	 *  # check that URL belong to the same
	 */
	OP_STATUS ProcessUrlCommon (
			const OpStringC& url_str,
			SectionType section_type,
			BOOL is_namespace,
			OpString& new_url_str,
			BOOL& status
	);

	OP_STATUS NormilizeUrl (const OpStringC& src, OpString& dst, URL &normalized_url);

	BOOL  AnalyzeUrl (URL &url, BOOL is_namespace, SectionType section_type);

	BOOL MatchFallbackCommon (const OpStringC& namespace_str, const uni_char*& fallback_entity_url_str) const;

	OpAutoStringHashTable <const OpString> m_cache_entries;
	LexicographicMap m_network_entries;
	LexicographicMap m_fallback_maps;

	OpString m_hash_value;

	URL m_manifest_url;

	BOOL m_is_online_open;
};

#endif // APPLICATION_CACHE_SUPPORRT
#endif// MANIFEST
