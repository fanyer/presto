/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSCOMMON_FILEINFO_H
#define WINDOWSCOMMON_FILEINFO_H

namespace WindowsCommonUtils
{
	/**
	 * Retrieves version and regional language information from Windows
	 * executables and dll libaries.
	 */
	class FileInformation
	{
	public:
		FileInformation();
		~FileInformation();

		/**
		 * Intializes data structures with file information.
		 *
		 * @param process_path Path to the file.
		 *
		 * @return OpStatus::OK if initializing succeeded. Error status otherwise.
		 */
		OP_STATUS Construct(const uni_char* process_path);

		/**
		 * Overrides language identifier used for getting resources. Will affect
		 * subsequent calls to GetInfoItem().
		 *
		 * @param langid Languague identifier to use.
		 */
		void SetLanguageId(const uni_char* langid) { uni_strcpy(m_langid, langid); }

		/**
		 * Sets language to default value which is either first language identifier
		 * embedded in file or assumed 041904b0 value.
		 */
		void SetDefaultLanguage();

		/**
		 * Retrieves a string value specific to the language and code page indicated
		 * by current value of language identifer.
		 *
		 * @param info_item Resource identifier of resource that we want value of.
		 * @param result Reference to OpString that will be set with value.
		 * @param lang_override Currently set language identifier can be overriden
		 * temporarily for period of a call by specifing it here.
		 *
		 * @return OpStatus::OK if informations could be retrieved. If not or result
		 * was empty string then error status will be returned.
		 */
		OP_STATUS GetInfoItem(const uni_char* info_item, UniString& result, const uni_char* lang_override = NULL);

		/**
		 * Retrieves major version of file.
		 */
		WORD GetMajorVersion() { return HIWORD(m_versioninfo->dwFileVersionMS); }
		/**
		 * Retrieves minor version of file.
		 */
		WORD GetMinorVersion() { return LOWORD(m_versioninfo->dwFileVersionMS); }

	private:
		VS_FIXEDFILEINFO* m_versioninfo;
		LPVOID        m_data;
		uni_char      m_langid[9]; /* ARRAY OK 2011-05-19 rchlodnicki */
	};
};

#endif // WINDOWSCOMMON_FILEINFO_H
