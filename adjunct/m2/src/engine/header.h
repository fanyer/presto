// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.	It may not be distributed
// under any circumstances.

#ifndef HEADER_H
#define HEADER_H

#ifdef M2_SUPPORT

#include "modules/util/opstring.h"
#include "modules/util/adt/opvector.h"

#include "adjunct/m2/src/include/defs.h"
#include "modules/util/simset.h"

#define MAX_HEADER_LENGTH 997

class Message;
class Header : public Link
{
public:
	/*
	According to RFC2822:
			address 		= mailbox / group
			address-list	= (address *("," address)) / obs-addr-list
			addr-spec		= local-part "@" domain
			angle-addr		= [CFWS] "<" addr-spec ">" [CFWS] / obs-angle-addr
			argument		= 1*<ASCII printable character>
			atext			= ALPHA / DIGIT /	; Any character except controls,
							  "!" / "#" / "$" / ;  SP, and specials.
							  "%" / "&" / "'" / ;  Used for atoms
							  "&" / "'" /
							  "*" / "+" /
							  "-" / "/" /
							  "=" / "?" /
							  "^" / "_" /
							  "`" / "{" /
							  "|" / "}" /
							  "~"
			atom			= [CFWS] 1*atext [CFWS]
			attribute		= token 			; Matching of attributes is ALWAYS case-insensitive.
			CFWS			= *([FWS] comment) (([FWS] comment) / FWS)
			ccontent		= ctext / quoted-pair / comment
			comment 		= "(" *([FWS] ccontent) [FWS] ")"
			composite-type	= "message" / "multipart" / extension-token
			CRLF			= %d10 / %d13
			ctext			= NO-WS-CTL /		; Non white space controls
							  %d33-39 / 		; The rest of the US-ASCII
							  %d42-91 / 		; characters not including "(",
							  %d93-126			; ")", or "\"
			date			= day month year
			date-time		= [ day-of-week "," ] date FWS time [CFWS]
			day 			= ([FWS] 1*2DIGIT) / obs-day
			day-name		= "Mon" / "Tue" / "Wed" / "Thu" / "Fri" / "Sat" / "Sun"
			day-of-week 	= ([FWS] day-name) / obs-day-of-week
			dcontent		= dtext / quoted-pair
			discrete-type	= "text" / "image" / "audio" / "video" / "application" / extension-token
			display-name	= phrase
			domain			= dot-atom / domain-literal / obs-domain
			domain-literal	= [CFWS] "[" *([FWS] dcontent) [FWS] "]" [CFWS]
			dot-atom		= [CFWS] dot-atom-text [CFWS]
			dot-atom-text	= 1*atext *("." 1*atext)
			DQUOTE			= %d34
			dtext			= NO-WS-CTL /		; Non white space controls
							  %d33-90 / 		; The rest of the US-ASCII
							  %d94-126			;  characters not including "[", "]", or "\"
			extension-token = ietf-token / x-token
			FWS 			= ([*WSP CRLF] 1*WSP) / ; Folding white space
							  obs-FWS
			group			= display-name ":" [mailbox-list / CFWS] ";" [CFWS]
			hour			= 2DIGIT / obs-hour
			HTAB			= %d9
			iana-token		= <A publicly-defined extension token. Tokens of this form
							   must be registered with IANA as specified in RFC 2048.>
			id-left 		= dot-atom-text / no-fold-quote / obs-id-left
			id-right		= dot-atom-text / no-fold-literal / obs-id-right
			ietf-token		= <An extension token defined by a standards-track RFC and registered with IANA.>
			local-part		= dot-atom / quoted-string / obs-local-part
			mailbox 		= name-addr / addr-spec
			mailbox-list	= (mailbox *("," mailbox)) / obs-mbox-list
			mechanism		= "7bit" / "8bit" / "binary" / "quoted-printable" /
							  "base64" / ietf-token / x-token
			minute			= 2DIGIT / obs-minute
			month			= (FWS month-name FWS) / obs-month
			month-name		= "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun" /
							  "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"
			msg-id			= [CFWS] "<" id-left "@" id-right ">" [CFWS]
			name-addr		= [display-name] angle-addr
			no-fold-literal = "[" *(dtext / quoted-pair) "]"
			no-fold-quote	= DQUOTE *(qtext / quoted-pair) DQUOTE
			NO-WS-CTL		= %d1-8 /			; US-ASCII control characters
							  %d11 /			; that do not include the
							  %d12 /			; carriage return, line feed,
							  %d14-31 / 		; and white space characters
							  %d127
			obs-addr-list	= 1*([address] [CFWS] "," [CFWS]) [address]
			obs-angle-addr	= [CFWS] "<" [obs-route] addr-spec ">" [CFWS]
			obs-char		= %d0-9 / %d11 /	; %d0-127 except CR and
							  %d12 / %d14-127	; LF
			obs-day 		= [CFWS] 1*2DIGIT [CFWS]
			obs-day-of-week = [CFWS] day-name [CFWS]
			obs-domain		= atom *("." atom)
			obs-domain-list = "@" domain *(*(CFWS / "," ) [CFWS] "@" domain)
			obs-FWS 		= 1*WSP *(CRLF 1*WSP)
			obs-id-left 	= local-part
			obs-id-right	= domain
			obs-local-part	= word *("." word)
			obs-hour		= [CFWS] 2DIGIT [CFWS]
			obs-mbox-list	= 1*([mailbox] [CFWS] "," [CFWS]) [mailbox]
			obs-minute		= [CFWS] 2DIGIT [CFWS]
			obs-month		= CFWS month-name CFWS
			obs-path		= obs-angle-addr
			obs-phrase		= word *(word / "." / CFWS)
			obs-qp			= "\" (%d0-127)
			obs-second		= [CFWS] 2DIGIT [CFWS]
			obs-route		= [CFWS] obs-domain-list ":" [CFWS]
			obs-text		= *LF *CR *(obs-char *LF *CR)
			obs-utext		= obs-text
			obs-year		= [CFWS] 2*DIGIT [CFWS]
			obs-zone		= "UT" / "GMT" /	; Universal Time
												; North American UT offsets
							  "EST" / "EDT" /	; Eastern:	- 5/ - 4
							  "CST" / "CDT" /	; Central:	- 6/ - 5
							  "MST" / "MDT" /	; Mountain: - 7/ - 6
							  "PST" / "PDT" /	; Pacific:	- 8/ - 7
							  %d65-73 / 		; Military zones - "A"
							  %d75-90 / 		; through "I" and "K"
							  %d97-105 /		; through "Z", both
							  %d107-122 		; upper and lower case
			parameter		= attribute "=" value
			path			= ([CFWS] "<" ([CFWS] / addr-spec) ">" [CFWS]) / obs-path
			phrase			= 1*word / obs-phrase
			qcontent		= qtext / quoted-pair
			qtext			= NO-WS-CTL /		; Non white space controls
							  %d33 /			; The rest of the US-ASCII
							  %d35-91 / 		; characters not including "\"
							  %d93- 			; or the quote character
			quoted-pair 	= ("\" text) / obs-qp
			quoted-string	= [CFWS] DQUOTE *([FWS] qcontent) [FWS] DQUOTE [CFWS]
			second			= 2DIGIT / obs-second
			SP				= %d32
			space			= 1*(HTAB / SP)
			subtype 		= extension-token / iana-token
			text			= %d1-9 /			; Characters excluding CR and LF
							  %d11 /
							  %d12 /
							  %d14-127 /
							  obs-text
			time			= time-of-day FWS zone
			time-of-day 	= hour ":" minute [ ":" second ]
			token			= 1*<any (US-ASCII) CHAR except SPACE, CTLs, or tspecials>
			tspecials		= "(" / ")" / "<" / ">" / "@" / ; Must be in quoted-string,
							  "," / ";" / ":" / "\" / <"> / ; to use within parameter values
							  "/" / "[" / "]" / "?" / "="
			type			= discrete-type / composite-type
			unstructured	= *([FWS] utext) [FWS]
			utext			= NO-WS-CTL /		; Non white space controls
							  %d33-126 /		; The rest of US-ASCII
							  obs-utext
			value			= token / quoted-string
			verb			= 1*( ALPHA / DIGIT )
			word			= atom / quoted-string
			WSP 			= %d9 / %d32
			x-token 		= <The two characters "X-" or "x-" followed, with no intervening white space, by any token>
			year			= 4*DIGIT / obs-year
			zone			= (( "+" / "-" ) 4DIGIT) / obs-zone
	*/

	enum HeaderType
	{
		ALSOCONTROL,	 // so1036 - "Also-Control" 						'"Also-Control:" verb *( space argument )'
		APPROVED,		 //   1036 - "Approved" 		   AddressHeader
		ARCHIVE,		 // d-ietf - "Archive"
		ARTICLENAMES,	 // so1036 - "Article-Names"
		ARTICLEUPDATES,  // so1036 - "Article-Updates"
		BCC,			 //    822 - "Bcc"				   AddressHeader
		CC, 			 //    822 - "Cc"				   AddressHeader
		COMMENTS,		 //   2822 - "Comments" 							'"Comments:" unstructured CRLF'
		COMPLAINTSTO,	 // d-ietf - "Complaints-To"
		CONTENTDISPOSITION, // 2046 - "Content-Disposition"
		CONTENTTRANSFERENCODING, // 2046 - "Content-Transfer-Encoding"		'"Content-Transfer-Encoding:" mechanism'
		CONTENTTYPE,	 //   2046 - "Content-Type" 						'"Content-Type:" type "/" subtype *(";" parameter)
		CONTROL,		 //    850 - "Control"
		DATE,			 //   2822 - "Date" 			   DateHeader		'"Date:" date-time CRLF'
		DISTRIBUTION,	 //    850 - "Distribution"
		ENCRYPTED,		 //    822 - "Encrypted"
		EXPIRES,		 //    850 - "Expires"			   DateHeader
		FOLLOWUPTO, 	 //    850 - "Followup-To"		   NewsgroupsHeader
		FROM,			 //   2822 - "From" 			   AddressHeader	'"From:" mailbox-list CRLF'
		INREPLYTO,		 //   2822 - "In-Reply-To"		   AddressHeader	'"In-Reply-To:" 1*msg-id CRLF'
		INJECTORINFO,	 // d-ietf - "Injector-Info"
		KEYWORDS,		 //   2822 - "Keywords" 							'"Keywords:" phrase *("," phrase) CRLF'
		LINES,			 //   1036 - "Lines"
		LISTID, 		 //   2919 - "List-Id"
		LISTPOST,		 //   RFC2369 - "List-Post"
		MAILINGLIST,	 //          "Mailing-List"
		MAILCOPIESTO,	 // d-ietf - "Mail-Copies-To"
		MESSAGEID,		 //   2822 - "Message-ID"		   MessageIdHeader	'"Message-ID:" msg-id CRLF'
		MIMEVERSION,	 //   2046 - "MIME-Version"
		NEWSGROUPS, 	 //    850 - "Newsgroups"		   NewsgroupsHeader, DO NOT FOLD!
		OPERARECEIVED,	 // Opera-spesific - "Opera-Received" DateHeader (time when Store got message)
		OPERAAUTOCC,	 // Opera-spesific - "Opera-Auto-Cc"  AddressHeader
		OPERAAUTOBCC,	 // Opera-spesific - "Opera-Auto-Bcc" AddressHeader
		ORGANIZATION,	 //    850 - "Organization"
		POSTEDANDMAILED, // d-ietf - "Posted-And-Mailed"
		RECEIVED,		 //    822 - "Received" 		   DateHeader		'"Received:" name-val-list ";" date-time CRLF'
		REFERENCES, 	 //   2822 - "References"		   ReferencesHeader '"References:" 1*msg-id CRLF'
		REPLYTO,		 //   2822 - "Reply-To" 		   AddressHeader	'"Reply-To:" address-list CRLF'
		RESENTBCC,		 //    822 - "Resent-bcc"		   AddressHeader
		RESENTCC,		 //    822 - "Resent-cc"		   AddressHeader
		RESENTDATE, 	 //   2822 - "Resent-Date"		   DateHeader		'"Resent-Date:" date-time CRLF'
		RESENTFROM, 	 //   2822 - "Resent-From"		   AddressHeader	'"Resent-From:" mailbox-list CRLF'
		RESENTMESSAGEID, //    822 - "Resent-Message-ID"   MessageIdHeader
		RESENTREPLYTO,	 //    822 - "Resent-Reply-To"	   AddressHeader
		RESENTSENDER,	 //    822 - "Resent-Sender"	   AddressHeader
		RESENTTO,		 //    822 - "Resent-To"		   AddressHeader
		RETURNPATH, 	 //    822 - "Return-path"							'"Return-Path:" path CRLF'
		SEEALSO,		 // so1036 - "See-Also"
		SENDER, 		 //   2822 - "Sender"			   AddressHeader	'"Sender:" mailbox CRLF'
		STATUS,
		SUBJECT,		 //   2822 - "Subject"			   SubjectHeader	'"Subject:" unstructured CRLF'
		SUMMARY,		 //   1036 - "Summary"
		SUPERSEDES, 	 // so1036 - "Supersedes"
		TO, 			 //    822 - "To"				   AddressHeader
		USERAGENT,		 // d-ietf - "User-Agent"
		XFACE,
		XMAILER,
		XPRIORITY,		 // "X-Priority"
		XREF,			 //   1036 - "Xref"
		XMAILINGLIST,	 //          "X-Mailing-List"
		UNKNOWN,
		ERR_NO_MEMORY	 // Used in copy-contructors and =operators for OOM
	};

	class HeaderValue : public OpStringC16
	{
		friend class Header;
	public:
		HeaderValue() : OpStringC16() {}
		void Empty() { m_value_buffer.Empty(); iBuffer = NULL; }
		OP_STATUS Set(const OpStringC16& value) { return Set(value.CStr(), KAll); }
		OP_STATUS Set(const uni_char* value, int aLength=KAll);
		OP_STATUS Append(const uni_char* string);

	private:
		OpString16 m_value_buffer;
	};

	class From : public Link
	{
	private:
		OpString16 m_name;
		OpString16 m_address_localpart; //NOT QotedPair-encoded!
		OpString16 m_address_domain; //NOT QotedPair-encoded (it isn't allowed in domains, anyway)
		OpString16 m_comment; //This value should include the needed '('s and ')'s
		OpString16 m_group;
		OpString16 m_address;
	public:
		UINT m_tagged:1; //Big hack to allow tagging all items in for/while-loop, to see who is already processed
	public:
		From() : m_tagged(0) {}

		From(const From& src);
		From& operator=(const From& src);

	public:
		static BOOL NeedQuotes(const OpStringC16& name, BOOL quote_qp, BOOL dot_atom_legal=FALSE);
	private:
		BOOL NeedQuotes(BOOL quote_qp, BOOL dot_atom_legal=FALSE) const {return NeedQuotes(m_name, quote_qp, dot_atom_legal);}

	public:
		const OpStringC16& GetName() const { return m_name; }
		const OpStringC16& GetAddress() const { return m_address; }
		const OpStringC16& GetAddressLocalpart() const { return m_address_localpart; }
		const OpStringC16& GetAddressDomain() const { return m_address_domain; }
		const OpStringC16& GetComment() const { return m_comment; }
		const OpStringC16& GetGroup() const {return m_group; }

		       OP_STATUS   GetIMAAAddress(OpString8& imaa_address, BOOL* failed_imaa=NULL) const;
		static OP_STATUS   GetAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain, OpString16& address);

		OP_STATUS SetName(const OpStringC16& name) {return m_name.Set(name);}
		OP_STATUS SetAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain);
		OP_STATUS SetIMAAAddress(const OpStringC8& address);
		OP_STATUS SetComment(const OpStringC16& comment) {return m_comment.Set(comment);}
		OP_STATUS SetGroup(const OpStringC16& group) {return m_group.Set(group);}

		int 	  CompareName(const OpStringC16& name) const {return m_name.Compare(name);}
		int 	  CompareAddress(const OpStringC16& address) const;
		int 	  CompareAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain) const;
		int 	  CompareComment(const OpStringC16& comment) const {return m_comment.Compare(comment);}
		int 	  CompareGroup(const OpStringC16& group) const {return m_group.Compare(group);}

		BOOL	  HasName() const {return m_name.HasContent();}
		BOOL	  HasAddress() const {return m_address_localpart.HasContent() || m_address_domain.HasContent();}
		BOOL	  HasAddressLocalpart() const {return m_address_localpart.HasContent();}
		BOOL	  HasAddressDomain() const {return m_address_domain.HasContent();}
		BOOL	  HasComment() const {return m_comment.HasContent();}
		BOOL	  HasGroup() const {return m_group.HasContent();}

		static OP_STATUS ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& address_localpart, OpString16& address_domain);
		static OP_STATUS ConvertToIMAAAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain, OpString8& imaa_address, BOOL* failed_imaa=NULL);

		static OP_STATUS GetFormattedEmail(const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, OpString16& result, BOOL do_quote_pair_encode=TRUE, BOOL quote_qp=TRUE, BOOL quote_name_atoms=TRUE);
		static OP_STATUS GetFormattedEmail(const OpStringC16& name, const OpStringC16& address_localpart, const OpStringC16& address_domain, const OpStringC16& comment, OpString16& result, BOOL do_quote_pair_encode=TRUE, BOOL quote_qp=TRUE, BOOL quote_name_atoms=TRUE);
		static OP_STATUS GetFormattedEmail(OpString8& encoding, const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, BOOL allow_8bit, OpString8& result, BOOL do_quote_pair_encode=TRUE, BOOL quote_name_atoms=TRUE);
		static OP_STATUS GetFormattedEmail(OpString8& encoding, const OpStringC16& name, const OpStringC16& address_localpart, const OpStringC16& address_domain, const OpStringC16& comment, BOOL allow_8bit, OpString8& result, BOOL do_quote_pair_encode=TRUE, BOOL quote_name_atoms=TRUE);
		OP_STATUS GetFormattedEmail(OpString16& result, BOOL do_quote_pair_encode=TRUE, BOOL quote_name_atoms=TRUE) const;
		OP_STATUS GetFormattedEmail(OpString8& encoding, BOOL allow_8bit, OpString8& result, BOOL do_quote_pair_encode=TRUE, BOOL quote_name_atoms=TRUE) const;

		static OP_STATUS GetCommentStringWithoutCchar(const OpStringC& comment, OpString& result, BOOL only_first_level=TRUE);
	};

private:
	HeaderType m_type;
	OpString16 m_value;
	Message*   m_message;
	UINT	   m_allow_8bit:1;
	UINT	   m_allow_incoming_quotedstring_qp:1;
	UINT	   m_is_outgoing:1; //Used only when detached from message
	OpString8  m_charset;		//Used only when detached from message

	time_t	   m_time_utc;		  //DateHeaders
	Head	   m_address_list;	  //FromHeaders
	OpAutoVector<OpString8> m_string_list;	//MessageIdHeaders, ReferencesHeaders, NewsgroupsHeaders
	OpString8  m_headername;	  //CustomHeaders

private:
	void	  ClearStringList() {m_string_list.DeleteAll();}
	OP_STATUS GetStringByIndexFromStart(OpString8& string, UINT16 index_from_start) const;
	OP_STATUS GetStringByIndexFromEnd(OpString8& string, UINT16 index_from_end) const;
	INT16   FindStringIndex(const OpStringC8& string) const; //return index in m_string_list, or -1 if it does not exist

	OP_STATUS FoldHeader(OpString8& folded_header, const OpStringC8& unfolded_header) const;
	OP_STATUS FoldHeader(OpString8& folded_header, const OpStringC8& unfolded_header, UINT32 max_line_length, BOOL force, BOOL& is_folded) const;
	OP_STATUS UnfoldHeader(OpString16& unfolded_header, const OpStringC16& folded_header) const;
	OP_STATUS UnfoldHeader(OpString8& unfolded_header, const OpStringC8& folded_header) const;

	OP_STATUS GetCharset(OpString8& charset) const;

	BOOL	  HeaderSupportsFolding() const;

//	OP_STATUS Parse();

public:
	Header(Message* message = NULL, HeaderType type = Header::UNKNOWN);
	~Header();

	Header(Header& src);
	Header& operator=(Header& src);
	OP_STATUS CopyValuesFrom(Header& src);

// General Header-functions
	OP_STATUS  DetachFromMessage();

	HeaderType GetType() const {return m_type;}
	static HeaderType GetType(const OpStringC8& header_name);
	static OP_STATUS  GetName(HeaderType type, OpString8& header_name);
	static HeaderType GetTypeForResend(HeaderType type);
	OP_STATUS  GetName(OpString8& header_name) const;
	OP_STATUS  GetTranslatedName(OpString16& translated_name) const;
	BOOL	   GetAllow8Bit() const {return m_allow_8bit;}
	void	   SetAllow8Bit(BOOL allow_8bit) {m_allow_8bit = allow_8bit;}
	BOOL	   GetAllowIncomingQuotedstringQP() const {return m_allow_incoming_quotedstring_qp;}
	void	   SetAllowIncomingQuotedstringQP(BOOL allow_incoming_quotedstring_qp) {m_allow_incoming_quotedstring_qp=allow_incoming_quotedstring_qp;}
	BOOL	   HeaderSupportsQuotePair() const;

	OP_STATUS  SetCharset(const OpStringC8& charset) { return m_charset.Set(charset); }

	BOOL	   IsEmpty() const {return m_value.IsEmpty() && m_time_utc==0 && m_address_list.Empty() && m_string_list.GetCount()==0;}
	BOOL	   IsInternalHeader() const {return IsInternalHeader(m_type);}
	static BOOL	IsInternalHeader(HeaderType type);

	OP_STATUS  GetNameAndValue(OpString8& header_string); //Return complete header-string (including '<Header>: ' and EOL, ready for 8-bit transfering

	OP_STATUS  GetValue(OpString8& header_value, BOOL do_quote_pair_encode=FALSE);
	OP_STATUS  GetValue(HeaderValue& header_value, BOOL do_quote_pair_encode=FALSE, BOOL qp_encode_atoms=FALSE);
	void	   GetValue(time_t& time) {time = m_time_utc;}

	OP_STATUS  SetValue(const OpStringC8& header_value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
	OP_STATUS  SetValue(const OpStringC16& header_value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
	OP_STATUS  SetValue(UINT32 value);
	void	   SetDateValue(time_t time_utc); //DateHeaders, 0 = ::Now

	BOOL		IsDateHeader() const {return IsDateHeader(m_type);}
	static BOOL	IsDateHeader(HeaderType type) {return type==DATE || type==EXPIRES || type==OPERARECEIVED || /*type==RECEIVED ||*/ //Received is removed because of different formatting, and because we quite simply don't care about the date in it
											type==RESENTDATE;}
	BOOL		IsAddressHeader() const {return IsAddressHeader(m_type);}
	static BOOL	IsAddressHeader(HeaderType type) {return type==APPROVED || type==BCC || type==CC || type==FROM ||
											   type==LISTID || type==REPLYTO ||
											   type==RESENTBCC || type==RESENTCC || type==RESENTFROM ||
											   type==RESENTREPLYTO || type==RESENTSENDER || type==RESENTTO ||
											   type==SENDER || type==TO ||
											   type==OPERAAUTOCC || type==OPERAAUTOBCC;}
	BOOL		IsSubjectHeader() const {return IsSubjectHeader(m_type);}
	static BOOL	IsSubjectHeader(HeaderType type) {return type==SUBJECT;}

	BOOL		IsMessageIdHeader() const {return IsMessageIdHeader(m_type);}
	static BOOL	IsMessageIdHeader(HeaderType type) {return type==MESSAGEID || type==RESENTMESSAGEID;}

	BOOL		IsReferencesHeader() const {return IsReferencesHeader(m_type);}
	static BOOL	IsReferencesHeader(HeaderType type) {return type==REFERENCES || type==INREPLYTO;}

	BOOL		IsNewsgroupsHeader() const {return IsNewsgroupsHeader(m_type);}
	static BOOL	IsNewsgroupsHeader(HeaderType type) {return type==FOLLOWUPTO || type==NEWSGROUPS;}

	BOOL		IsXFaceHeader() const {return IsXFaceHeader(m_type);}
	static BOOL	IsXFaceHeader(HeaderType type) {return type==XFACE;}

	BOOL		IsCustomHeader() const {return IsCustomHeader(m_type);}
	static BOOL	IsCustomHeader(HeaderType type) {return type==UNKNOWN;}

// Functions spesific for DateHeaders (DATE, EXPIRES, RESENTDATE)
private:
	time_t	   UTCtoLocaltime(struct tm* time, const uni_char* tz) const {return LocaltimetoUTC(time, tz, TRUE);}
	time_t	   LocaltimetoUTC(struct tm* time, const uni_char* tz, BOOL inverse=FALSE) const;
	BOOL	   IsStartOfTimezone(const uni_char* string) const;

public:

// Functions spesific for AddressHeaders (APPROVED, BCC, CC, FROM, INREPLYTO, LISTID, REPLYTO, RESENTBCC, RESENTCC, RESENTFROM, RESENTREPLYTO, RESENTSENDER, RESENTTO, SENDER, TO, OPERAAUTOCC, OPERAAUTOBCC)
private:
//	OP_STATUS  CopyAtom(IN const uni_char* start, IN const uni_char* end,
//						OUT OpString16& name, OUT OpString16& address, OUT OpString16& comment,
//						BOOL in_address, BOOL in_comment) const; //Helper-function for SplitValues

//	  OP_STATUS  SplitValues(IN const OpStringC8& header_value, OUT OpString16& name, OUT OpString16& address, OUT OpString16& comment, OUT BOOL& valid) const;
//	  OP_STATUS  SplitValues(IN const OpStringC16& header_value, OUT OpString16& name, OUT OpString16& address, OUT OpString16& comment, OUT BOOL& valid) const;
//	  OP_STATUS  GenerateHeaderFromAddressList(OpString16& header_value) const;
//	  OP_STATUS  GenerateHeaderFromAddressList(OpString8& header_value, OpString8& encoding, BOOL allow_8bit) const;
//	  OP_STATUS  GenerateHeaderFromAddressList(BOOL destination_is_16bit=TRUE);
public:
	const From* GetFirstAddress() const { return static_cast<From*>(m_address_list.First()); }
	const From* GetLastAddress() const { return static_cast<From*>(m_address_list.Last()); }
	void	   ClearAddressList() {m_address_list.Clear();}
//	  OP_STATUS  AddAddress(const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, BOOL regenerate_header=TRUE, BOOL is_parsing=FALSE);
	OP_STATUS  AddAddress(const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, const OpStringC16& group);
	OP_STATUS  AddAddress(const OpStringC16& name, const OpStringC16& address_localpart, const OpStringC16& address_domain, const OpStringC16& comment, const OpStringC16& group);
	OP_STATUS  AddAddress(const From& address);
	OP_STATUS  SetAddresses(const OpStringC8& rfc822_addresses, int* parseerror_start=NULL, int* parseerror_end=NULL) {ClearAddressList(); return AddAddresses(rfc822_addresses, parseerror_start, parseerror_end);}
	OP_STATUS  SetAddresses(const OpStringC16& addresses, int* parseerror_start=NULL, int* parseerror_end=NULL) {ClearAddressList(); return AddAddresses(addresses, FALSE, parseerror_start, parseerror_end);}
	OP_STATUS  AddAddresses(const OpStringC8& rfc822_addresses, int* parseerror_start=NULL, int* parseerror_end=NULL);
	OP_STATUS  AddAddresses(const OpStringC16& addresses, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
private:
	OP_STATUS  AddAddress(const OpStringC16& rfc822_address, const OpStringC16& rfc822_group, BOOL do_quote_pair_decode=FALSE); //This is private, as it expects input to be QP-decoded (and otherwise RFC822-compatible)
public:
	OP_STATUS  DeleteAddress(const OpStringC16& name /*Can be empty */, const OpStringC16& address);
	OP_STATUS  DeleteAddress(From* address/*, BOOL regenerate_header=TRUE*/);

// Functions spesific for SubjectHeaders (SUBJECT)
private:
public:
	OP_STATUS  AddReplyPrefix();
	OP_STATUS  AddForwardPrefix();
	OP_STATUS  GetValueWithoutPrefix(OpString16& header_value);

// Functions spesific for MessageIdHeaders (MESSAGEID, RESENTMESSAGEID)
private:
public:
	OP_STATUS GenerateNewMessageId();
	void	  ClearMessageIdList() {ClearStringList();}

// Functions spesific for ReferencesHeaders (REFERENCES)
private:
public:
	OP_STATUS GetMessageId(OpString8& message_id, INT16 index_from_end) const; //index_from_end is 0 for last element, 1 for the last-but-one, etc.  (-1 for first element)
	OP_STATUS AddMessageId(const OpStringC8& message_id);

// Functions spesific for NewsgroupsHeaders (FOLLOWUPTO, NEWSGROUPS)
private:
//	  OP_STATUS RemoveSpaceFromNewsgroups();
public:
	void	  ClearNewsgroupList() {ClearStringList();}
	OP_STATUS GetNewsgroup(OpString8& newsgroup, UINT16 index_from_start);
	OP_STATUS AddNewsgroup(const OpStringC8& newsgroup);
	OP_STATUS RemoveNewsgroup(const OpStringC8& newsgroup);

// Functions spesific for CustomHeaders
private:
public:
	OP_STATUS SetName(const OpStringC8& header_name);
};

#endif //M2_SUPPORT

#endif // HEADER_H
