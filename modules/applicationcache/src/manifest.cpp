/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/applicationcache/manifest.h"
#include "modules/util/opfile/opfile.h"
#include "modules/applicationcache/src/manifest_parser.h"

// inner functions
struct Utility
{
	static inline
	OP_STATUS CloneStr (const uni_char*const src, OpAutoPtr<const OpString>& dst)
	{
		OpAutoPtr<OpString> tmp (OP_NEW (OpString, ()));
		if (tmp.get ())
			RETURN_IF_ERROR (tmp->Set (src));
		else
			return OpStatus::ERR_NO_MEMORY;

		dst.reset (tmp.release ());
		return OpStatus::OK;
	}

	template<typename A, typename B>
	static inline
	OP_STATUS Clone (A& src, OpAutoPtr<B>& dst)
	{
		A* tmp = NULL;
		RETURN_IF_ERROR (src.Clone (tmp));
		dst.reset (tmp);

		return OpStatus::OK;
	}

	static inline
	BOOL IsPresent (const OpStringC& key, const OpAutoStringHashTable<const OpString>& hash_table)
	{
		return hash_table.Contains (key.CStr ());
	}
};

struct CloneUtility
{
	static inline
	OP_STATUS CopyUrlHashTable(const OpAutoStringHashTable<const OpString>& src, OpAutoStringHashTable<const OpString>& dst)
	{
		if (0 == src.GetCount ())
			return OpStatus::OK;

		if (OpHashIterator* const tmp_hash_it = const_cast<OpAutoStringHashTable<const OpString>&> (src).GetIterator ())
		{
			OpAutoPtr<OpHashIterator> hash_iterator (tmp_hash_it);
			if (!OpStatus::IsSuccess (hash_iterator->First ()))
				return OpStatus::OK;

			do
			{
				OpAutoPtr<const OpString> new_url;
				RETURN_IF_ERROR (Utility::CloneStr (static_cast<const OpString*> (hash_iterator->GetData ())->CStr(), new_url));

				OP_ASSERT (new_url->Length());
				RETURN_IF_ERROR (dst.Add (new_url->CStr (), new_url.get ()));
				OP_ASSERT (dst.Contains(new_url->CStr ()));

				new_url.release ();
			} while (OpStatus::IsSuccess (hash_iterator->Next ()));

			OP_ASSERT (src.GetCount () == dst.GetCount ());
		}
		else
			return OpStatus::ERR_NO_MEMORY;

		return OpStatus::OK;
	}

	template <typename NamespaceType>
	static inline
	OP_STATUS CopyLexiMap (const Manifest::LexicographicMap& src, Manifest::LexicographicMap& dst)
	{
		if (!src.GetCount ())
			return OpStatus::OK;

		const unsigned size = src.GetCount ();

		for (unsigned ix = 0; ix < size; ix++)
		{
			const NamespaceType& tmp_ns = *static_cast<const NamespaceType*>(src.Get (ix));
			OP_ASSERT (tmp_ns.GetNemespaceUrl ().Length ());

			OpAutoPtr<const NamespaceType> new_ns (NULL);
			RETURN_IF_ERROR (Utility::Clone (tmp_ns, new_ns));
			OP_ASSERT (NULL != new_ns.get ());
			RETURN_IF_ERROR (dst.Add (new_ns.get ()));

			new_ns.release ();
		}

		// check that all data are completely copied
		OP_ASSERT (src.GetCount () == dst.GetCount ());

		return OpStatus::OK;
	}
};

OP_STATUS Manifest::Namespace::BuildNamespace (const uni_char*const namespace_url_str, const Manifest::Namespace*& new_namespace)
{
	OP_ASSERT (namespace_url_str);
	OP_ASSERT (OpString::Length (namespace_url_str));

	OpAutoPtr<const OpString> new_namespace_url_str;
	RETURN_IF_ERROR (Utility::CloneStr (namespace_url_str, new_namespace_url_str));

	new_namespace = OP_NEW (Manifest::Namespace, (new_namespace_url_str.get()));

	if (!new_namespace)
		return OpStatus::ERR_NO_MEMORY;

	new_namespace_url_str.release ();
	return OpStatus::OK;
}

OP_STATUS Manifest::Namespace::Clone (const Manifest::Namespace*& dst) const
{
	return BuildNamespace (m_namespace_url_str->CStr (), dst);
}

OP_STATUS Manifest::Fallback::BuildFallback (const uni_char*const namespace_url_str, const uni_char*const entry_url_str, const Fallback*& new_fallback)
{
	OP_ASSERT (namespace_url_str);
	OP_ASSERT (OpString::Length (namespace_url_str));

	OP_ASSERT (entry_url_str);
	OP_ASSERT (OpString::Length (entry_url_str));

	OpAutoPtr<const OpString> new_namespace_url_str;
	RETURN_IF_ERROR (Utility::CloneStr (namespace_url_str, new_namespace_url_str));

	OpAutoPtr<const OpString> new_entity_url_str;
	RETURN_IF_ERROR (Utility::CloneStr (entry_url_str, new_entity_url_str));

	new_fallback = OP_NEW (Manifest::Fallback, (new_namespace_url_str.get(), new_entity_url_str.get()));

	if (!new_fallback)
		return OpStatus::ERR_NO_MEMORY;

	new_namespace_url_str.release ();
	new_entity_url_str.release ();
	return OpStatus::OK;
}

OP_STATUS Manifest::Fallback::Clone (const Manifest::Fallback*& new_fallback) const {
	return BuildFallback (GetNemespaceUrl ().CStr (), m_entity_url_str->CStr (), new_fallback);
}

/* static */
OP_STATUS Manifest::MakeManifestFromDisk(Manifest *&manifest, const URL &manifest_url, const uni_char* manifest_file_path, OpFileFolder folder)
{
	/* We read the cache manifest from disk, and parses the file */
	manifest = NULL;
	OpFile manifest_file;

	ManifestParser* manifest_parser;
	RETURN_IF_ERROR(ManifestParser::BuildManifestParser(manifest_url, manifest_parser));
	OpAutoPtr<ManifestParser> manifest_parser_container(manifest_parser);

	manifest_file.Construct(manifest_file_path, folder);

	BOOL exist;
	if (OpStatus::IsError(manifest_file.Exists(exist)) || exist == FALSE)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}
	manifest_file.Open(OPFILE_READ);

	OpFileLength manifest_length, bytes_read;
	RETURN_IF_ERROR(manifest_file.GetFileLength(manifest_length));
	if (manifest_length > (static_cast<OpFileLength>(1)<<31) ) // just in case someone makes an totally insane manifest file.
		return OpStatus::ERR;

	OpString manifest_file_data;

	RETURN_OOM_IF_NULL(manifest_file_data.Reserve(static_cast<int>(manifest_length)/2 + 4));

	RETURN_IF_ERROR(manifest_file.Read(manifest_file_data.CStr(), manifest_length, &bytes_read));
	if (manifest_length != bytes_read)
		return OpStatus::ERR;

	manifest_file_data.CStr()[manifest_length/2] = 0;
	manifest_file_data.CStr()[manifest_length/2 + 1] = 0;

	unsigned consumed;
	RETURN_IF_ERROR(manifest_parser->Parse(manifest_file_data.CStr(), manifest_file_data.Length(), TRUE, consumed));

	OP_ASSERT ((int)consumed == manifest_file_data.Length()); // ensure that file has been parsed completely

	OP_STATUS status;
	if (OpStatus::IsError(status = manifest_parser->BuildManifest(manifest)))
	    return status;

	return OpStatus::OK;
}

Manifest::Manifest (const URL& manifest_url)
	:   m_manifest_url (manifest_url)
	,   m_is_online_open (FALSE) // by default the online is closed
{}

Manifest::~Manifest ()  {
}

/* ToDo: Stop using clone. It's not needed to clone the manifest, as we can just give away the pointer
 * instead. The cloing is a lot of extra uneeded code.
 */

OP_STATUS Manifest::Clone (Manifest*& dst) const
{
	if (Manifest*const tmp_manifest = OP_NEW (Manifest, (m_manifest_url))) {
		OpAutoPtr<Manifest> new_manifest (tmp_manifest);
		RETURN_IF_ERROR (CloneUtility::CopyUrlHashTable (m_cache_entries, tmp_manifest->m_cache_entries));

		RETURN_IF_ERROR (CloneUtility::CopyLexiMap <Manifest::Namespace> (m_network_entries, tmp_manifest->m_network_entries));
		RETURN_IF_ERROR (CloneUtility::CopyLexiMap <Manifest::Fallback> (m_fallback_maps, tmp_manifest->m_fallback_maps));
		tmp_manifest->SetOnlineOpen (m_is_online_open);

		dst = new_manifest.release ();
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

BOOL Manifest::CheckCacheUrl (const uni_char* const url) const {
	return m_cache_entries.Contains (url);
}

BOOL Manifest::MatchNetworkUrl (const uni_char* const url_str) const {
	return NULL != m_network_entries.Match (url_str);
}

BOOL Manifest::MatchFallback (const uni_char* const nemespace_url_str, const uni_char*& fallback_entity_url_str) const
{
	return MatchFallbackCommon (nemespace_url_str, fallback_entity_url_str);
}

OpHashIterator* Manifest::GetCacheIterator ()
{
	return m_cache_entries.GetIterator ();
}

OP_STATUS Manifest::SetHash (const OpStringC& hash)
{
	OP_ASSERT(0 < hash.Length());

	return m_hash_value.Set (hash);
}

const OpStringC& Manifest::GetHash () const
{
	OP_ASSERT(0 < m_hash_value.Length ());

	return m_hash_value;
}

OP_STATUS Manifest::AddCacheUrl (const OpStringC& url_str)
{
	RETURN_IF_ERROR (ProcessUrl (url_str, CACHE));

	return OpStatus::OK;
}

OP_STATUS Manifest::AddNetworkUrl (const OpStringC& url_str)
{
	RETURN_IF_ERROR (ProcessUrl (url_str, NETWORK));

	return OpStatus::OK;
}

OP_STATUS Manifest::AddFallbackUrls (const OpStringC& namespace_url_str, const OpStringC& entity_url_str)
{
	RETURN_IF_ERROR (ProcessUrl (namespace_url_str, entity_url_str));

	return OpStatus::OK;
}

void Manifest::SetOnlineOpen (BOOL is_online_open)
{
	m_is_online_open = is_online_open;
}

BOOL Manifest::IsOnlineOpen () const
{
	return m_is_online_open;
}


OP_STATUS Manifest::ProcessUrl (const OpStringC& url_str, SectionType section_type)
{
	OpString new_url_str;
	BOOL status = FALSE;
	RETURN_IF_ERROR (
		ProcessUrlCommon (
			url_str,
			section_type,
			section_type == Manifest::CACHE
				?   FALSE
				:   TRUE,
			new_url_str,
			status
		)
	);

	if (status)
	{
		OP_ASSERT (new_url_str.Length ()); // string must not be empty!

		switch (section_type)
		{
			case CACHE:
			{
				OpAutoPtr <const OpString> tmp_new_str;
				RETURN_IF_ERROR (Utility::CloneStr (new_url_str.CStr (), tmp_new_str));
				RETURN_IF_ERROR (m_cache_entries.Add (tmp_new_str->CStr (), tmp_new_str.get ()));
				tmp_new_str.release ();
				break;
			}

			case NETWORK:
			{
				const Namespace* tmp_ns = NULL;
				RETURN_IF_ERROR (Namespace::BuildNamespace (new_url_str.CStr(), tmp_ns));

				OpAutoPtr <const Namespace> ns_holder (tmp_ns);
				RETURN_IF_ERROR (m_network_entries.Add (tmp_ns));
				ns_holder.release ();
				break;
			}

			default:
				OP_ASSERT (!"Unknown case!");
		}
	}

	return OpStatus::OK;
}

OP_STATUS Manifest::ProcessUrl (const OpStringC& namespace_url_str, const OpStringC& entity_url_str)
{
	// TODO: and there are duplications either in cache or network..... :)
	BOOL status = FALSE;

	OpString new_namespace;
	RETURN_IF_ERROR (ProcessUrlCommon (namespace_url_str, FALLBACK, TRUE, new_namespace, status));
	if (!status) {
		return OpStatus::OK;
	}

	OpString new_entity;
	RETURN_IF_ERROR (ProcessUrlCommon (entity_url_str, FALLBACK, FALSE, new_entity, status));
	if (!status) {
		return OpStatus::OK;
	}


	if (!Utility::IsPresent(new_entity, m_cache_entries)) {
		OpAutoPtr <const OpString> tmp_new_entity;
		RETURN_IF_ERROR (Utility::CloneStr (new_entity.CStr (), tmp_new_entity));
		RETURN_IF_ERROR (m_cache_entries.Add (tmp_new_entity->CStr (), tmp_new_entity.get ()));
		tmp_new_entity.release ();
	}

	const Fallback* tmp_new_fallback;
	RETURN_IF_ERROR (Fallback::BuildFallback (new_namespace.CStr (), new_entity.CStr (), tmp_new_fallback));
	OpAutoPtr<const Fallback> tmp_fallback (tmp_new_fallback);

	RETURN_IF_ERROR (m_fallback_maps.Add (tmp_new_fallback));

	tmp_fallback.release();

	return OpStatus::OK;
}

OP_STATUS Manifest::ProcessUrlCommon(
	const OpStringC& url_str,
	SectionType section_type,
	BOOL is_namespace,
	OpString& str,
	BOOL& status
)
{
	// default initializations
	status = FALSE;

	URL url;
	RETURN_IF_ERROR (NormilizeUrl (url_str, str, url));

	status = AnalyzeUrl (url, is_namespace, section_type);

	if (status)
	{
		switch (section_type)
		{
			case CACHE:
				status = !Utility::IsPresent (str, m_cache_entries);
			break;

			case FALLBACK:
				if (is_namespace)
				{
					status = !m_fallback_maps.IsPresent (str);
				}
				else
				{
					// despite the fact that the entry is present, in case of
					//  fallback entries it still has to be taken into consideration, because
					//  the entry may be used for a different namespace, hence the value is always `FALSE'
					status = TRUE;
				}
			break;

			case NETWORK:
				status = !m_network_entries.IsPresent (str);
			break;

			default:
				OP_ASSERT (!"Unknown case!");
		}
	}

	return OpStatus::OK;
}

OP_STATUS Manifest::NormilizeUrl (const OpStringC& url_src, OpString& dst, URL &normalized_url)
{
	// get URL itself
	normalized_url = g_url_api->GetURL (m_manifest_url, url_src, TRUE);

	OpString name;
	RETURN_IF_ERROR (normalized_url.GetAttribute (URL::KUniName_Username_Password_Hidden_Escaped, name));

	// Note, that fraction will be excluded automatically

	RETURN_IF_ERROR (dst.Set (name));

	return OpStatus::OK;
}

BOOL Manifest::AnalyzeUrl (URL &url, BOOL is_namespace, SectionType section_type)
{
	URLType manifest_url_type = (URLType) m_manifest_url.GetAttribute(URL::KType);
	URLType normalized_url_type = (URLType) url.GetAttribute(URL::KType);

	switch (section_type) {
		case CACHE:
		{
			if (manifest_url_type != normalized_url_type)
				return FALSE;

			if (manifest_url_type == URL_HTTPS && !m_manifest_url.SameServer (url, URL::KCheckResolvedPort))
				return FALSE;
			break;
		}
		case NETWORK:
		case FALLBACK:
		{
			if (manifest_url_type != normalized_url_type || !m_manifest_url.SameServer (url, URL::KCheckResolvedPort))
				return FALSE;
		}
		default:
			break;
	}

	OpString path;
	if (OpStatus::IsError (url.GetAttribute (URL::KUniPath_Escaped, path)))
		return FALSE;

	if (!is_namespace && (0 == path.Length () || 0 == path.Compare (UNI_L ("/"))) )
		return FALSE;

	return TRUE;
}

//
class LexicographicUtility
{
	static inline
	int Compare (const uni_char*const str1, int len1, const uni_char*const str2, int len2)
	{
		for (int ix = 0; ix < MIN (len1, len2); ix++) {
			int result = str1[ix] - str2[ix];
			if (result != 0) {
				return result;
			}
		}

		return len1 - len2;
	}

	public:

	static int SoftCompare (const OpStringC& str1, const OpStringC& str2)
	{
		OP_ASSERT (0 != str1.Length());
		OP_ASSERT (0 != str2.Length());

		const int len1 = str1.Length();
		const int len2 = str2.Length();

		const int lenm = MIN (len1, len2);

		const int min_cmp = Compare(str1.CStr(), lenm, str2.CStr(), lenm);

		if (0 != min_cmp)
			return min_cmp;

		return lenm == len1 ? 0 : len2 - len1;
	}

	static int HardCompare (const OpStringC& str1, const OpStringC& str2)
	{
		return Compare (str1.CStr(), str1.Length(), str2.CStr(), str2.Length());
	}
};


BOOL Manifest::MatchFallbackCommon (const OpStringC& namespace_str, const uni_char*& fallback_url_str) const
{
	const Fallback* fallback = static_cast<const Fallback*>(m_fallback_maps.Match (namespace_str));
	// NB: Brew's ADS compiler can't cope if you declare fallback inside the if (...) :-(
	// inner structure
	if (fallback)
	{
		fallback_url_str = fallback->GetEntityUrl().CStr();
		return TRUE;
	}
	else
	{
		fallback_url_str = NULL;
		return FALSE;
	}
}

Manifest::LexicographicMap::LexicographicMap ()
	:   m_is_fallback_sorted (FALSE)
{}

Manifest::LexicographicMap::~LexicographicMap ()
{
	const unsigned size = GetCount();
	for (unsigned ix = 0; ix < size; ix++)
		OP_DELETE (Get (ix));
}

OP_STATUS Manifest::LexicographicMap::Add(const Manifest::Namespace *item)
{
	OP_ASSERT (NULL != item);
	RETURN_IF_ERROR (OpGenericVector::Add (static_cast<void*>(const_cast<Manifest::Namespace *>(item))));
	m_is_fallback_sorted = FALSE;

	return OpStatus::OK;
}

BOOL Manifest::LexicographicMap::ProcessAndContinue () const
{
	struct Inner
	{
		static
		int AdoptedCompare (const void* ns1, const void* ns2)
		{
			OP_ASSERT (NULL != ns1);
			OP_ASSERT (NULL != ns2);

			const Manifest::Namespace& tmp_ns1 = **static_cast<const Manifest::Namespace*const*>(ns1);
			const Manifest::Namespace& tmp_ns2 = **static_cast<const Manifest::Namespace*const*>(ns2);

			const OpStringC& tmp_ns_str1 = tmp_ns1.GetNemespaceUrl ();
			const OpStringC& tmp_ns_str2 = tmp_ns2.GetNemespaceUrl ();
			OP_ASSERT (tmp_ns_str1.Length ());
			OP_ASSERT (tmp_ns_str2.Length ());

			return LexicographicUtility::HardCompare (tmp_ns_str1, tmp_ns_str2);
		}
	};

	// nothing to resolve, return FALSE and stop processing
	if (0 == OpGenericVector::GetCount ())
		return FALSE;

	// lazy sorting is implemented here that is to say only when it is necessary only
	if (!m_is_fallback_sorted)
	{
		op_qsort (
			OpGenericVector::m_items,
			OpGenericVector::GetCount (),
			sizeof (const Manifest::Fallback*),
			Inner::AdoptedCompare
		);
		m_is_fallback_sorted = TRUE;
	}

	return TRUE;
}

OP_STATUS Manifest::LexicographicMap::Remove (const Manifest::Namespace *item)
{
	OP_ASSERT (NULL != item);
	RETURN_IF_ERROR (OpGenericVector::RemoveByItem (static_cast<void*>(const_cast<Namespace *>(item))));
	m_is_fallback_sorted = FALSE;

	return OpStatus::OK;
}

BOOL Manifest::LexicographicMap::IsPresent (const OpStringC& namespace_url_str) const
{
	struct Inner
	{
			static
			int AdoptedCompare (const void* key, const void* values)
			{
				const OpStringC& tmp_ns1 = *static_cast<const OpStringC*>(key);
				const OpStringC& tmp_ns2 = (*static_cast<const Manifest::Fallback*const*>(values))->GetNemespaceUrl ();

				return LexicographicUtility::SoftCompare (tmp_ns1, tmp_ns2);
			}
	};

	if (!ProcessAndContinue ())
		return FALSE;

	OP_ASSERT (m_is_fallback_sorted);

	// binary search of the result + incremental search
	const Namespace*const*const ns = static_cast<const Manifest::Namespace**>(
			op_bsearch (
				static_cast<const void*>(&namespace_url_str),
				OpGenericVector::m_items,
				GetCount(),
				sizeof (const Manifest::Fallback*),
				Inner::AdoptedCompare
			)
	);

	return NULL != ns;
}

const Manifest::Namespace* Manifest::LexicographicMap::Match (const OpStringC& namespace_url_str) const
{
	struct Inner
	{
		static
		BOOL AdoptedMatch (const void*const*const iterator, const OpStringC& str) {
			const Manifest::Namespace*const*const ns = (const Manifest::Namespace*const*) iterator;
			const OpStringC& ns_str = (*ns)->GetNemespaceUrl ();
			return 0 == LexicographicUtility::SoftCompare (ns_str, str);
		}
	};

	if (!ProcessAndContinue ())
		return NULL;

	OP_ASSERT (m_is_fallback_sorted);

	// at this moment the search is a very trivial and quite slow
	void** ns_end = OpGenericVector::m_items + OpGenericVector::GetCount ();
	const Namespace*const* ns = NULL;
	for (const void*const* ns_it = OpGenericVector::m_items; ns_it < ns_end; ns_it++)
	{
		if (Inner::AdoptedMatch (ns_it, namespace_url_str))
			ns = (const Namespace*const*) ns_it;
	}

	return ns ? *ns : NULL;
}

#endif // APPLICATION_CACHE_SUPPORT
