#ifndef _OPQUOTE_H_
#define _OPQUOTE_H_

#include "modules/util/opstring.h"

class OpQuoteBuffer
{
private:
    uni_char*    const m_tmp_buffer;
    unsigned int m_tmp_buffer_pos;
    OpString*    const m_destination;
    int          m_destination_length;
    unsigned int const m_buffer_size;
    unsigned int const m_rollback_size;

public:
    OpQuoteBuffer(OpString* destination, unsigned int buffer_size, unsigned int rollback_size=0);
    ~OpQuoteBuffer();
    OP_STATUS    Validate() const {return m_tmp_buffer==NULL?OpStatus::ERR_NO_MEMORY:OpStatus::OK;}

    OP_STATUS    Append(uni_char append_char);
	OP_STATUS	 Append(const OpStringC& append_string);
	OP_STATUS	 Append(const uni_char* append_string_ptr, int length);
    OP_STATUS    Delete(unsigned int delete_count=1);
    uni_char     Peek(unsigned int back_count=0) const;
    unsigned int BufferLength() const {return m_tmp_buffer_pos;}
    unsigned int DestinationLength() const {return m_destination->Length();}

};


class OpQuote {
public:
    OpQuote(BOOL is_flowed=TRUE, BOOL has_delsp=TRUE, BOOL should_autowrap=TRUE, unsigned int max_line_length=76, unsigned int max_quote_depth=32);
	virtual ~OpQuote();

    OP_STATUS QuoteAndWrapText(OpString& quoted_and_wrapped_text, const OpStringC& original_text, uni_char quote_char='>');

    OP_STATUS QuoteText(OpString& quoted_text, const OpStringC& original_text, uni_char quote_char='>');

    virtual OP_STATUS WrapText(OpString& wrapped_text, const OpStringC& original_text, BOOL strip_nonquoted_flows);

	OP_STATUS UnwrapText(OpString& unwrapped_text, const OpStringC& original_text);

	BOOL HasDelsp() const { return m_has_delsp; }

	unsigned GetMaxLineLength() const { return m_max_line_length; }

	unsigned GetMaxQuoteDepth() const { return m_max_quote_depth; }

private:
	OP_STATUS VerifyBufferCapacity(OpString& buffer, uni_char*& current_position, int needed, int grow_size) const;

	unsigned int m_max_line_length;
	unsigned int m_max_quote_depth;
	BOOL         m_is_flowed;
	BOOL		 m_has_delsp;
	BOOL		 m_should_autowrap;

};


//#endif // _NEWS2_SUPPORT_

#endif
