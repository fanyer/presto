/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PREF_ACCESSORS_H
#define PREF_ACCESSORS_H

/**
 * @file
 * Generic preference accessors.
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */

#include "modules/prefs/prefsmanager/collections/pc_files.h"


namespace PrefUtils
{
	/**
	 * Reads from and writes to a single integer preference.
	 */
	class IntegerAccessor
	{
	public:
		virtual ~IntegerAccessor() {}

		/**
		 * @return the current preference value
		 */
		virtual int GetPref() = 0;

		/**
		 * Sets a new preference value.
		 *
		 * @param value the new value
		 * @return status
		 */
		virtual OP_STATUS SetPref(int value) = 0;
	};

	/**
	 * Implements IntegerAccessor for some preference collection type.
	 * @param C preference collection type
	 */
	template <typename C>
	class IntegerAccessorImpl : public IntegerAccessor
	{
	public:
		typedef typename C::integerpref PrefType;

		/**
		 * Constructs an accessor that will read from and write to a specific
		 * integer preference in a specific collection.
		 *
		 * @param collection the preference collection
		 * @param preference the preference in @a collection
		 */
		IntegerAccessorImpl(C& collection, PrefType preference)
				: m_collection(collection), m_preference(preference)  {}

		virtual int GetPref() { return m_collection.GetIntegerPref(m_preference); }
		virtual OP_STATUS SetPref(int value)
		{
			RETURN_IF_LEAVE(m_collection.WriteIntegerL(m_preference, value));
			return OpStatus::OK;
		}

	private:
		C& m_collection;
		PrefType m_preference;
	};


	/**
	 * Reads from and writes to a single string preference.
	 */
	class StringAccessor
	{
	public:
		virtual ~StringAccessor() {}

		/**
		 * @param value receives the current preference value
		 * @return status
		 */
		virtual OP_STATUS GetPref(OpString& value) = 0;

		/**
		 * Sets a new preference value.
		 *
		 * @param value the new value
		 * @return status
		 */
		virtual OP_STATUS SetPref(const OpStringC& value) = 0;
	};

	/**
	 * Implements StringAccessor for some preference collection type.
	 * @param C preference collection type
	 */
	template <typename C>
	class StringAccessorImpl : public StringAccessor
	{
	public:
		typedef typename C::stringpref PrefType;

		/**
		 * Constructs an accessor that will read from and write to a specific
		 * string preference in a specific collection.
		 *
		 * @param collection the preference collection
		 * @param preference the preference in @a collection
		 */
		StringAccessorImpl(C& collection, PrefType preference)
				: m_collection(collection), m_preference(preference)  {}

		virtual OP_STATUS GetPref(OpString& value)
		{
			return value.Set(m_collection.GetStringPref(m_preference));
		}

		virtual OP_STATUS SetPref(const OpStringC& value)
		{
			RETURN_IF_LEAVE(m_collection.WriteStringL(m_preference, value));
			return OpStatus::OK;
		}

	private:
		C& m_collection;
		PrefType m_preference;
	};

	/**
	 * A StringAccessor accessing the file preference collection.
	 */
	class FilePathAccessor : public StringAccessor
	{
	public:
		/**
		 * Constructs an accessor that will read from and write to a specific
		 * file preference in PrefsCollectionFiles.
		 *
		 * @param preference the preference
		 */
		explicit FilePathAccessor(PrefsCollectionFiles::filepref preference) : m_preference(preference)  {}

		virtual OP_STATUS GetPref(OpString& value)
		{
			return value.Set(g_pcfiles->GetFile(m_preference)->GetFullPath());
		}

		virtual OP_STATUS SetPref(const OpStringC& value)
		{
			OpFile file;
			RETURN_IF_ERROR(file.Construct(value));
			RETURN_IF_LEAVE(g_pcfiles->WriteFilePrefL(m_preference, &file));
			return OpStatus::OK;
		}

	private:
		PrefsCollectionFiles::filepref m_preference;
	};
}

#endif // PREF_ACCESSORS_H
