#ifndef _UTIL_MISC_H_
#define _UTIL_MISC_H_

#include "adjunct/m2/src/include/defs.h"

namespace OpMisc
{
    enum CharTypes {
        UPPERCASE=1,
        LOWERCASE=2,
        ALPHA=UPPERCASE|LOWERCASE,
        DIGIT=4
    };

	/** @brief Helper class to reset a variable when this object goes out of scope
	  */
	template<typename T> class Resetter
	{
	public:
		/** Constructor
		  * @param reset Variable that should be reset when this resetter is destroyed
		  * @param reset_to Value to reset the variable to
		  */
		Resetter(T& reset, T reset_to) : m_reset(reset), m_reset_to(reset_to) {}
		~Resetter() { m_reset = m_reset_to; }
	private:
		T& m_reset;
		T  m_reset_to;
	};

	/** @brief String that wipes its data when it goes out of scope
	    T is either OpString8 or OpString16
	  */
	template<class T>
	class SafeString : public T
	{
		public:
			~SafeString()
				{ this->Wipe(); }

			OP_STATUS Set(const OpStringC8& aString)
				{ this->Wipe(); return T::Set(aString); }

			OP_STATUS Set(const OpStringC16& aString)
				{ this->Wipe(); return T::Set(aString.CStr()); }

			void Empty()
				{ this->Wipe(); T::Empty(); }
	};


	/** Encodes a string so that it's usable as an (IMAP) keyword
	  * @param source String to encode
	  * @param dest Encoded string, if successful
	  */
	OP_STATUS EncodeKeyword(const OpStringC16& source, OpString8& dest);

	/** Decode an (IMAP) keyword or mailbox name
	  * @param source String to decode
	  * @param dest Decoded string, if successful
	  */
	OP_STATUS DecodeMailboxOrKeyword(const OpStringC8& source, OpString& dest);

    OP_STATUS CalculateMD5Checksum(const char* buffer, int buffer_length, OpString8& md5_checksum);

	const uni_char* uni_rfc822_skipCFWS(const uni_char* string, OpString& comment);
	const uni_char* uni_rfc822_skip_until_CFWS(const uni_char* string);
	const uni_char* uni_rfc822_strchr(const uni_char* string, uni_char c,
											 bool skip_quotes=TRUE /* Skip anything inbetween '"' and '"' */,
											 bool skip_comments=TRUE /* Skip anything inbetween '(' and ')' */);
	const char* rfc822_strchr(const char* string, char c,
									 bool skip_quotes=TRUE /* Skip anything inbetween '"' and '"' */,
									 bool skip_comments=TRUE /* Skip anything inbetween '(' and ')' */);

    OP_STATUS StripQuotes(OpString8& string);
    OP_STATUS StripQuotes(OpString16& string);
    OP_STATUS StripWhitespaceAndQuotes(OpString16& string, BOOL strip_whitespace=TRUE, BOOL strip_quotes=TRUE);
    void      StripTrailingLinefeeds(OpString16& string);

    OP_STATUS DecodeQuotePair(OpString8& string);
    OP_STATUS DecodeQuotePair(OpString16& string);
    OP_STATUS EncodeQuotePair(OpString8& string, BOOL encode_dobbeltfnutt=TRUE);
    OP_STATUS EncodeQuotePair(OpString16& string, BOOL encode_dobbeltfnutt=TRUE);

	/** Make a quoted string out of string (puts quote around the string and escapes '"' and '\\' with a \)
	  * @param source String to quote
	  * @param target Where to put quoted string
	  */
	OP_STATUS QuoteString(const OpStringC8& source, OpString8& target);

	/** Generate a normal string from a quoted string (See QuoteString above)
	  * @param source String to decode
	  * @param target Unquoted string
	  */
	OP_STATUS UnQuoteString(const OpStringC8& source, OpString8& target);

	OP_STATUS ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& username, OpString16& domain);
	OP_STATUS ConvertToIMAAAddressList(const OpStringC16& addresslist, BOOL is_mailadresses, OpString8& imaa_addresslist, char separator_char, BOOL* failed_imaa=NULL);
	OP_STATUS ConvertToIMAAAddress(const OpStringC16& address, BOOL is_mailaddress, OpString8& imaa_address, BOOL* failed_imaa=NULL);
	OP_STATUS ConvertToIMAAAddress(const OpStringC16& username, const OpStringC16& domain, OpString8& imaa_address, BOOL* failed_imaa=NULL);
	OP_STATUS SplitMailAddress(const OpStringC& complete_address, OpString& username, OpString& domain);
	OP_STATUS SplitMailAddress(const OpStringC8& complete_address, OpString8& username, OpString8& domain);

	/*! Check if this is an ipv4 address. */
	OP_STATUS IsIPV4Address(BOOL& is_ipv4, const OpStringC& ip_address);

	/*! Check if this is an ipv6 address. Currently it will only accept the
		first two representations outlined in rfc2373 (i.e. not the mixed
		ipv4 / ipv6 format). */
	OP_STATUS IsIPV6Address(BOOL& is_ipv6, const OpStringC& ip_address);

	/*! Check that a given ip address is a public ip address. Will only check
		for ipv4 addresses at the moment. All ipv6 addresses will be assumed
		to be public. */
	OP_STATUS IsPublicIP(BOOL& is_public, const OpStringC& ip_address);

#ifdef NEEDS_RISC_ALIGNMENT
	UINT32 UnSerialize32(const UINT8 *buf);
	UINT16 UnSerialize16(const UINT8 *buf);
#endif
	// Assuming that sizeof(long) == 4 is BAD! FIXME: Should use INT32 and INT16 instead
	void LongToChars(unsigned char* dest, long val);
	void WordToChars(unsigned char* dest, long val);
	long CharsToLong(unsigned char* text);
	long CharsToWord(unsigned char* text);
    BOOL StringConsistsOf(const OpStringC8& string, int legal_types, const OpStringC8& legal_extrachars);

	OP_STATUS RelativeURItoAbsoluteURI(OpString& absolute_uri, const OpStringC& relative_uri, const OpStringC& base_uri);
};


#endif
