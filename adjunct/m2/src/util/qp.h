#ifndef _UTIL_QP_H_
#define _UTIL_QP_H_

#include "adjunct/m2/src/include/defs.h"
#include "modules/encodings/charconverter.h"
#include "modules/util/opstring.h"

#if !defined IN
# define IN
#endif
#if !defined OUT
# define OUT
#endif
#if !defined IO
# define IO
#endif

class InputConverterManager;

class OpQP
{
public:
    OpQP() {}
    ~OpQP() {}

private:
	static BOOL		 LooksLikeQPEncodedWord(const OpStringC16& buffer, int& start_offset, int& end_offset);
    static void      SkipWhitespaceBeforeEncodedWord(IO const char** src);
    static OP_STATUS AppendText(const OpStringC8& charset, OpString16& destination, const char* source, int length=KAll, BOOL accept_illegal_characters=FALSE);
    static OP_STATUS StartNewLine(OpString8& buffer, const OpStringC8& charset, const OpStringC8& type, UINT8& chars_used, BOOL first_line=FALSE);
	static const char* StartOfEncodedWord(const char* buffer, BOOL allow_quotedstring_qp, const char** start_of_iso2022 = NULL);

    static OP_STATUS UnknownDecode(IO const char** src, IN const OpStringC8& charset, OpString16& decoded);

	static void		 Base64Encode(const char* source_3bytes, UINT8 bytes_to_convert, char* destination_4bytes);
    static OP_STATUS Base64LineEncode(IN const uni_char* source, OUT OpString8& encoded, const OpStringC8& charset); //Will not clear 'encoded' before encoding!

	static BOOL		 QPEncode(IN char source, OUT char* destination_2bytes); //Returns TRUE if source has been encoded into destination
    static OP_STATUS QPLineEncode(IN const uni_char* source, OUT OpString8& encoded, OpString8& charset); //Will not clear 'encoded' before encoding!

	static InputConverterManager* s_input_converter_manager;
	static InputConverterManager& GetInputConverterManager();

public:
	static OP_STATUS QPDecode(const BYTE*& src, const OpStringC8& charset, OpString16& decoded,
							  BOOL& warning_found, BOOL& error_found, BOOL encoded_word = TRUE);
    static OP_STATUS Base64Decode(const BYTE*& src, const OpStringC8& charset, OpString16& decoded,
                                  BOOL& warning_found, BOOL& error_found, BOOL encoded_word = TRUE);
    static OP_STATUS Base64Encode(IN const OpStringC16& source, OUT OpString8& encoded, OpString8& charset, BOOL add_controlchars=TRUE, BOOL is_subject=FALSE);
    static OP_STATUS Base64Encode(const char* source, int length, OpString8& encoded);
    static OP_STATUS QPEncode(IN const OpStringC16& source, OUT OpString8& encoded, OpString8& charset, BOOL add_controlchars=TRUE, BOOL is_subject=FALSE);
    static UINT8   BitsNeeded(const OpStringC16& buffer, int length=KAll); //returns 7, 8 or 16
    static UINT8   BitsNeeded(const OpStringC8& buffer, int length=KAll); //returns 7 or 8

    static OP_STATUS Encode(IN const OpStringC16& source, OUT OpString8& encoded, OpString8& charset, BOOL allow_8bit=FALSE, BOOL is_subject=FALSE, BOOL check_for_lookalikes=TRUE);
    static OP_STATUS Decode(IN const OpStringC8& source, OUT OpString16& decoded, const OpStringC8& preferred_charset,
                            BOOL& warning_found, BOOL& error_found, BOOL allow_quotedstring_qp=FALSE, BOOL is_rfc822=FALSE);

	static void		 SetInputConverterManager(InputConverterManager* input_converter_manager);
};


#endif
