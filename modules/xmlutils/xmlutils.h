/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLUTILS_H
#define XMLUTILS_H

#include "modules/xmlutils/xmltypes.h"

/**

  A collection of smaller utility functions.

*/
class XMLUtils
{
public:
	enum
	{
		XML_SPACE =           0x01, ///< White space character.
		XML_NAMEFIRST =       0x02, ///< Valid as first character in a Name.
		XML_NAME =            0x04, ///< Valid as non-first character in a Name.
		XML_HEXDIGIT =        0x08, ///< Hexadecimal digit.
		XML_DECDIGIT =        0x10, ///< Decimal digit.
		XML_VALID_10 =        0x20, ///< Valid character in XML 1.0.
		XML_VALID_11 =        0x40, ///< Valid character in XML 1.1.
		XML_UNRESTRICTED_11 = 0x80
		/**< Character that can appear unescaped in XML 1.1. */
	};

	static const unsigned char characters[128]; /* ARRAY OK jl 2008-02-07 */
	/**< Classification information for ASCII characters 0 through
	     127.  Use expressions such as "characters[X] & XML_SPACE" to
	     determine whether character X is a white space character. */

	static BOOL IsSpace(unsigned ch);
	/**< Returns TRUE if 'ch' is a white space character (or more
	     specificly, if 'ch' matches the S production in XML.) */

	static BOOL IsSpaceExtended(XMLVersion version, unsigned ch);
	/**< Returns TRUE if 'ch' is a white space character according to
	     IsSpace(), or would be normalized into such character by the
	     line break normalization in the specified XML version. */

	static BOOL IsNameFirst(XMLVersion version, unsigned ch);
	/**< Returns TRUE if 'ch' is valid as first character in a Name in
	     the specified XML version. */

	static BOOL IsName(XMLVersion version, unsigned ch);
	/**< Returns TRUE if 'ch' is valid as non-first character in a
	     Name, or anywhere in a NmToken, in the specified XML
	     version. */

	static BOOL IsDecDigit(unsigned ch);
	/**< Returns TRUE if 'ch' is a hexadecimal digit ('0' to '9'). */

	static BOOL IsHexDigit(unsigned ch);
	/**< Returns TRUE if 'ch' is a hexadecimal digit ('0' to '9', 'A'
	     to 'F' or 'a' to 'f'). */

	static BOOL IsChar10(unsigned ch);
	/**< Returns TRUE if 'ch' is a valid character in XML 1.0. */

	static BOOL IsChar11(unsigned ch);
	/**< Returns TRUE if 'ch' is a valid character in XML 1.1. */

	static BOOL IsChar(XMLVersion version, unsigned ch);
	/**< Returns TRUE if 'ch' is a valid character in the specified
	     XML version. */

	static BOOL IsRestrictedChar(unsigned ch);
	/**< Returns TRUE if 'ch' can appear unescaped in XML 1.1. */

	static unsigned GetNextCharacter(const uni_char *&string, unsigned &string_length);
	/**< Returns the next 32-bit character in string (translating
	     surrogate pairs, if they occur) and updates string and
	     string_length as appropriate to "consume" the character. */

	static BOOL IsValidName(XMLVersion version, const uni_char *name, unsigned name_length = ~0u);
	/**< Returns TRUE if the first 'name_length' characters in 'name'
	     form a valid Name in the specified XML version.  If
	     'name_length' is ~0u, 'name' must be null terminated, and
	     uni_strlen is used to calculate its length. */

	static BOOL IsValidNmToken(XMLVersion version, const uni_char *name, unsigned name_length = ~0u);
	/**< Returns TRUE if the first 'name_length' characters in 'name'
	     form a valid NmToken in the specified XML version.  If
	     'name_length' is ~0u, 'name' must be null terminated, and
	     uni_strlen is used to calculate its length. */

	static BOOL IsValidQName(XMLVersion version, const uni_char *name, unsigned name_length = ~0u);
	/**< Returns TRUE if the first 'name_length' characters in 'name'
	     form a valid QName (or NCName) in the specified XML version.
	     If 'name_length' is ~0u, 'name' must be null terminated, and
	     uni_strlen is used to calculate its length. */

	static BOOL IsValidNCName(XMLVersion version, const uni_char *name, unsigned name_length = ~0u);
	/**< Returns TRUE if the first 'name_length' characters in 'name'
	     form a valid NCName in the specified XML version.  If
	     'name_length' is ~0u, 'name' must be null terminated, and
	     uni_strlen is used to calculate its length. */

	static BOOL IsWhitespace(const uni_char *string, unsigned string_length = ~0u);
	/**< Returns TRUE if the first 'string_length' characters in
	     'string' are all white space characters.  If 'string_length'
	     is ~0u, 'string' must be null terminated, and uni_strlen is
	     used to calculate its length. */

	static BOOL GetLine(XMLVersion version, const uni_char *&data, unsigned &data_length, const uni_char *&line, unsigned &line_length);
	/**< Returns TRUE if 'data' contains a line break (according to
	     the specified XML version), and if so, sets 'line' to 'data'
	     and 'line_length' to the number of characters before the line
	     break, and updates 'data' and 'data_length' to consume all
	     characters up to and including the characters constituting
	     the line break.  If there is no line break in 'data', FALSE
	     is returned, but 'line' and 'line_length' may be modified. */
};

#endif // XMLUTILS_H
