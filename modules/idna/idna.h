/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _IDNA_H_
#define _IDNA_H_

#define IDNA_PREFIX "xn--"
#define IDNA_PREFIX_L UNI_L("xn--")
#define IMAA_PREFIX "iesg--"
#define IMAA_PREFIX_L UNI_L("iesg--")

// Strategy class for the ProcessDomainComponents_L method
class ComponentProcessor;

class IDNA
{
public:
	/**
	 * Compute the IDN security level required for this domain name.
	 *
	 * @param domain Domain name in its UTF-16 representation.
	 * @return Minimum IDN security level setting that accepts this domain name.
	 */
	static URL_IDN_SecurityLevel ComputeSecurityLevel_L(const uni_char *domain);

	/**
	 * Retrieves existing or new ServerName from the store object, sets it up according to the parameters and returns.
	 */
	static ServerName* GetServerName(ServerName_Store &store, OP_STATUS &op_err, const uni_char *name, unsigned short &port, BOOL create = FALSE, BOOL Normalize=TRUE);

	/**
	 * Converts whole server name by replacing A-Labels with U-Labels where needed.
	 */
	static void ConvertFromIDNA_L(const char *source, uni_char *target, size_t targetbufsize, BOOL& is_mailaddress);

	/**
	 * Converts single A-Label into U-Label.
	 */
	static uni_char *ConvertALabelToULabel_L(const char *source1, uni_char *target1, size_t targetbufsize, BOOL& is_mailaddress);

	/**
	 * Converts single U-Label into A-Label.
	 */
	static uni_char *ConvertULabelToALabel_L(const uni_char *source, size_t source_size, uni_char *target, size_t targetbufsize, BOOL is_mailaddress, BOOL strict_mode);

	/**
	 * Converts whole server name by replacing U-Labels with A-Labels where needed.
	 */
	static void ConvertToIDNA_L(const uni_char *source, uni_char *target, size_t targetbufsize, BOOL is_mailaddress);

	/**
	 * Returns TRUE for full stop ("."), ideographic full stop, fullwidth full stop, half width full stop, as per IDNA
	 */
	static BOOL IsIDNAFullStop(uni_char ch);

private:
	/**
	 * Does the mapping specified in rfc5895.
	 */
	static void MapFromUserInput_L(uni_char *string, uni_char *temp_buffer, size_t max_len);
	static unsigned long Punycode_adapt(unsigned long delta, unsigned long numpoints, BOOL firsttime);
	static void ProcessDomainComponents_L(const uni_char *source, uni_char *target, size_t targetbufsize, BOOL is_mailaddress, ComponentProcessor &processor);
	static URL_IDN_SecurityLevel ComputeSecurityLevelRecursive(const uni_char *domain, int len, BOOL split);
};

class UnicodeStringIterator;

class IDNALabelValidator
{
public:
	/**
	 * Values defined in this enum are widely discussed in rfc5892.
	 */
	enum IDNAProperty
	{
		IDNA_PVALID,	//Code points that are allowed to be used in IDNs.
		IDNA_CONTEXTJ,	//Code points that are not be used in labels unless other characters with specific joining type are present.
		IDNA_CONTEXTO,	//Other code points that are not be used in labels unless specific other characters or properties are present.
		IDNA_DISALLOWED,//Code points that should clearly not be included in IDNs.
		IDNA_UNASSIGNED	//Code points that are not designated (i.e., are unassigned) in the Unicode Standard.
	};

	/**
	 * Check whether given domain name is valid to be looked up. This method uses IsLabelIDNAValid to check each label of the domain
	 * name. In case of BIDI domain names (see rfc5893) also CheckBidiRule is applied.
	 */
	static BOOL IsDomainNameIDNAValid_L(const uni_char *domainname, BOOL additionalBlocking = FALSE);

	/**
	 * Check whether given label is valid to be looked up. The decision is based on the value of
	 * IDNAProperty of each of the characters in the label and some contextual rules.
	 * The algorithm and contextual rules are defined by rfc5892 "IDNA Code Points".
	 * The additionalBlocking param controls blocking Opera blacklisted codepoints (e.g. some BMP dead scripts).
	 */
	static BOOL IsLabelIDNAValid_L(const uni_char *label, int len = -1, BOOL additionalBlocking = FALSE);

	/**
	 * Returns IDNAProperty value for single codepoint.
	 */
	static IDNAProperty GetIDNAProperty_L(UnicodePoint codepoint);
private:
	static BOOL IsCodePointIDNAValid_L(const UnicodeStringIterator &it);
	static BOOL IsInExceptions(UnicodePoint codepoint, IDNAProperty &exception_value);
	static BOOL IsInBackwardCompatible(UnicodePoint codepoint, IDNAProperty &exception_value);
	static BOOL IsUnassigned(UnicodePoint codepoint);
	static BOOL IsUnstable_L(UnicodePoint codepoint);
	static BOOL IsLDH(UnicodePoint codepoint);
	static BOOL IsIgnorableProperties(UnicodePoint codepoint);
	static BOOL IsIgnorableBlocks(UnicodePoint codepoint);
	static BOOL IsOldHangulJamo(UnicodePoint codepoint);
	static BOOL IsLetterDigit(UnicodePoint codepoint);
	static BOOL CheckContextualRules(const UnicodeStringIterator &it);
	static BOOL IsBlockedAdditionally(UnicodePoint cp);
#ifdef SUPPORT_TEXT_DIRECTION
	static BOOL CheckBidiRule(const uni_char *label, int len = -1);
#endif // SUPPORT_TEXT_DIRECTION
};

#endif //_IDNA_H_
