/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file encodings-capabilities.h
 *
 * Defines the capabilites available in this version of the encodings module.
 */

#ifndef ENCODINGS_CAPABILITIES_H
#define ENCODINGS_CAPABILITIES_H

#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
/** The OutputConverter::GetInvalidCharacters() API is available.
  * Added 2003-06 on Rosetta 3. Still used by Desktop/M2 (2011-01-20) */
# define ENCODINGS_CAP_SPECIFY_INVALID
#endif

/** CharsetDetector has utf8_threshold parameter.
  * Added 2005-11 on Rosetta 4. Still used by Desktop/M2 (2011-01-20) */
#define ENCODINGS_CAP_UTF8_THRESHOLD

/** The following API methods now support the allowUTF7 flag (FALSE by default):
  * - CharsetDetector::CharsetDetector(...)
  * - static const char *CharsetDetector::GetXMLEncoding(...)
  * - static const char *CharsetDetector::GetCSSEncoding(...)
  * - static const char *CharsetDetector::GetHTMLEncoding(...)
  * - static const char *CharsetDetector::GetLanguageFileEncoding(...)
  * - static const char *CharsetDetector::GetUTFEncodingFromBOM(...)
  * - InputConverter *createInputConverter(...)
  * - static OP_STATUS InputConverter::CreateCharConverter(...) (x2)
  *
  * Reasons for adding this flag and setting it FALSE by default:
  * 1. UTF-7 combined with social engineering may be considered a security
  *    issue, since it allows content that seems harmless in a different
  *    charset to suddenly become malicious when interpreted as UTF-7.
  * 2. HTML5 required user agents to _not_ support this encoding at all.
  * 3. UTF-7 has never been seen used in real-life websites, so removing it is
  *    not considered to break existing web content.
  * 4. IMPORTANT: UTF-7 is only being deprecated for the web. We want it to
  *    still be available for use in other contexts, most notably:
  *    UTF-7 support is still required for e-mail (some servers send all their
  *    error messages in it) and IMAP (the protocol requires it).
  *
  * Users of InputConverter and CharsetDetector classes are required to set the
  * allowUTF7 flag to TRUE whenever they are in a (non-web) context where UTF-7
  * support is required.
  *
  * Please refer to bug #313069 for more information.
  *
  * Added 2008-02 on Rosetta 6. Still used by Desktop/M2 (2011-01-20) */
#define ENCODINGS_CAP_MUST_ALLOW_UTF7

#endif // ENCODINGS_CAPABILITIES_H
