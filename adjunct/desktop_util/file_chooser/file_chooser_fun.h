/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef __FILE_CHOOSER_FUN_H__
#define __FILE_CHOOSER_FUN_H__

#include "adjunct/desktop_pi/desktop_file_chooser.h"

// Move all functions below into this namespace and simplify their names
namespace FileChooserUtil
{
	/**
	 * Save the Open/Save directory to preferences for later use
	 *
	 * @param action The selector action to determine what directory type to save
	 * @param result Uses the first of te selected files (if any) as the path to save
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the error
	 */
	OP_STATUS SavePreferencePath(DesktopFileChooserRequest::Action action, const DesktopFileChooserResult& result);

	/**
	 * Save the Open/Save directory to preferences for later use
	 *
	 * @param request Uses the selector action to determine what directory type to save
	 * @param result Uses the first of te selected files (if any) as the path to save
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	inline OP_STATUS SavePreferencePath(const DesktopFileChooserRequest& request, const DesktopFileChooserResult& result) { return SavePreferencePath( request.action, result); }


	/**
	 * Create a new DesktopFileChooserRequest object and copy the state of the source object
	 * The new object is only valid if OpStatus::OK is returned amd must be destroyed by caller
	 * using OP_DELETE
	 *
	 * @param src Source object
	 * @param copy On return the new object
	 *
	 * @param OpStatus::OK on success, otherwise an error code describing the error
	 */
	OP_STATUS CopySettings(const DesktopFileChooserRequest& src, DesktopFileChooserRequest*& copy);

	/**
	 * Return the index of a given extension. The comparison is case insensitive
	 *
	 * @param extension The extension to search for
	 * @param filters List of extensions
	 *
	 * @return A positive value corresponding to the first match in the list, otherwise -1
	 */
	INT32 FindExtension(const OpStringC& extension, OpVector<OpFileSelectionListener::MediaType>* filters);

	/**
	 * Reset DesktopFileChooserRequest so it can be reused
	 *
	 * @param request Item to reset
	 */
	void Reset(DesktopFileChooserRequest& request);

	/**
	 * Appends file extension filters to the media description string if no such filters
	 * have already beed added
	 *
	 * @param media_type Item to extend
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the error
	 */
	OP_STATUS AppendExtensionInfo(OpFileSelectionListener::MediaType& media_type);

	/**
	 * Return the first extension from the default selected extension
	 *
	 * @param request Object that provides extensions
	 * @param extension On return the retrived extension
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the error
	 */
	OP_STATUS GetInitialExtension(const DesktopFileChooserRequest& request, OpString& extension);

	/**
	 * Convert a string filter of the form '| description | extensions | description | extensions...'
	 * to the to a vector of DesktopFileChooserExtensionFilters
	 *
	 * @param string_filter The string filter to convert
	 * @param extension_filters On return the list list of converted filters
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the error
	 */
	OP_STATUS SetExtensionFilter(const uni_char* string_filter, OpAutoVector<OpFileSelectionListener::MediaType> * extension_filters);

	/**
	 * Make a copy the filter with content and append the new copy to the list
	 *
	 * @param filter Filter data to duplicate
	 * @param list The target list
	 * @param add_asterix_dot If TRUE, prepend "*." to the extensions if they do not contain it.
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the error
	 */
	OP_STATUS AddMediaType(const OpFileSelectionListener::MediaType* filter, OpAutoVector<OpFileSelectionListener::MediaType>* list, BOOL add_asterix_dot);

};


// Hooks for compatibility with existing code. To be removed 

DEPRECATED(INT32 FindExtension(const OpStringC& extension, OpVector<OpFileSelectionListener::MediaType>* filters));
DEPRECATED(void ResetDesktopFileChooserRequest(DesktopFileChooserRequest& request));
DEPRECATED(OP_STATUS ExtendMediaTypeWithExtensionInfo(OpFileSelectionListener::MediaType& media_type));
DEPRECATED(OP_STATUS GetInitialExtension(const DesktopFileChooserRequest& request, OpString& extension));
DEPRECATED(OP_STATUS StringFilterToExtensionFilter(const uni_char* string_filter, OpAutoVector<OpFileSelectionListener::MediaType> * extension_filters));
DEPRECATED(OP_STATUS CopyAddMediaType(const OpFileSelectionListener::MediaType* filter, OpAutoVector<OpFileSelectionListener::MediaType>* list, BOOL add_asterix_dot));

#endif // __FILE_CHOOSER_FUN_H__
