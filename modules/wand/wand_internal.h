/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef WAND_INTERNAL
#define WAND_INTERNAL

#ifdef WAND_SUPPORT

// == This is for the Wand system ONLY ==============

#define WAND_FLAG_NEVER_ON_THIS_PAGE	0x00000001
#define WAND_FLAG_AUTO_SUBMIT_DEPRECATED 0x00000010 // Not used anymore but need to kept so we don't accidently reuse it
#define WAND_FLAG_ON_THIS_SERVER		0x00000100

#ifdef WAND_ECOMMERCE_SUPPORT
# define WAND_FLAG_STORE_ECOMMERCE					0x00000200
# define WAND_FLAG_STORE_NOTHING_BUT_ECOMMERCE		0x00000400
#endif

/** Reads a string from the file and decrypts it */
OP_STATUS ReadWandString(OpFile &file, OpString &str, long version);

/** Encrypts a file and Writes it to the file */
OP_STATUS WriteWandString(OpFile &file, OpString &str);

/** Returns the url of doc without anything after "?" */
OP_STATUS GetWandUrl(FramesDocument* doc, OpString &url);

/** Removes everything after "?" */
void MakeWandUrl(OpString &url);

/** Will turn f.ex this: "http://www.google.com/yadayada/yada/"
			  into this: "http://www.google.com" */
void MakeWandServerUrl(OpString &url);

/** Is the element a element that can be stored in wand */
BOOL IsValidWandObject(HTML_Element* he);

/** Is the element protected from being stored in wand. (The autocomplete=off attribute) */
BOOL IsProtectedFormObject(FramesDocument* doc, HTML_Element* he);

/**
 * Inserts a password in the wand module either
 * by asking the user or by using the obfuscation password.
 *
 * @param[in] parent_window The window to use as parent for a dialog.
 */
//BOOL SetupWandPassword(OpWindow *parent_window);

/**
 * Deletes the password in the wand module.
 */
//void ClearWandPassword();

#endif // WAND_SUPPORT

#endif // !WAND_INTERNAL
