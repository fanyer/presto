/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file security_info_parser.cpp
 *
 * Contains function definitions for the classes defined in the corresponding
 * header file.
 */

#include "core/pch.h"
#ifdef SECURITY_INFORMATION_PARSER
# include "modules/doc/frm_doc.h"
# include "modules/url/url2.h"
# include "modules/xmlutils/xmlparser.h"
# include "modules/xmlutils/xmltoken.h"
# include "modules/xmlutils/xmltokenhandler.h"
# include "modules/dochand/win.h"
# include "modules/dochand/fraud_check.h"
# include "modules/doc/doc.h"
# ifdef URL_FILTER
#  include "modules/content_filter/content_filter.h"
# endif // URL_FILTER
# include "modules/security_manager/include/security_manager.h"
# include "modules/pi/network/OpSocketAddress.h"
# include "modules/pi/OpLocale.h"

#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslcctx.h"
#include "modules/libssl/ssldlg.h"

#include "modules/windowcommander/src/SecurityInfoParser.h"

/***********************************************************************************
**
**	OpSecurityInformationContainer::SetSecurityIssue
**
***********************************************************************************/

OP_STATUS OpSecurityInformationContainer::SetSecurityIssue(const uni_char* tag)
{
	if (!tag)
	{
		return OpStatus::ERR;
	}
	if (!uni_strcmp(tag, UNI_L("userconfirmedcertificate")))
	{
		user_confirmed_certificate = TRUE;
	}
	if (!uni_strcmp(tag, UNI_L("permanentlyconfirmed")))
	{
		permanently_confirmed_certificate = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("Anonymous_Connection")))
	{
		anonymous_connection = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("unknownca")))
	{
		unknown_ca = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("Certexpired")))
	{
		certexpired = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("CertNotYetValid")))
	{
		cert_not_yet_valid = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("ConfiguredWarning")))
	{
		configured_warning = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("AuthenticationOnly_NoEncryption")))
	{
		authentication_only_no_encryption = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("LowEncryptionlevel")))
	{
		low_encryptionlevel = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("ServernameMismatch")))
	{
		servername_mismatch = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("OcspRequestFailed")))
	{
		ocsp_request_failed = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("OcspResponseFailed")))
	{
		ocsp_response_failed = TRUE;
	}
	else if (!uni_strcmp(tag, UNI_L("RenegExtensionUnsupported")))
	{
		reneg_extension_unsupported = TRUE;
	}	
	else
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OpSecurityInformationContainer::GetButtonTextPtr
**
***********************************************************************************/

OpString* OpSecurityInformationContainer::GetButtonTextPtr()
{
	if (m_button_text.IsEmpty())
	{
		m_button_text.AppendFormat(UNI_L("%s (%s)"), m_subject_organization.CStr(), m_subject_country.CStr() ? m_subject_country.CStr() : UNI_L(""));
	}
	return &m_button_text;
}

/***********************************************************************************
**
**	OpSecurityInformationContainer::GetSubjectSummaryString
**
***********************************************************************************/

OpString* OpSecurityInformationContainer::GetSubjectSummaryString()
{
	if (m_subject_summary.IsEmpty())
	{
		BOOL missing = FALSE; // Flag to signal that one field is missing so we can insert another in stead.
		if (!AppendCommaSeparated(m_subject_common_name, m_subject_summary))
			missing = TRUE;
		if (!AppendCommaSeparated(m_subject_organization, m_subject_summary))
			missing = TRUE;
		if (missing)
			AppendCommaSeparated(m_subject_organizational_unit, m_subject_summary);
	}
	return &m_subject_summary;
}

/***********************************************************************************
**
**	OpSecurityInformationContainer::GetIssuerSummaryString
**
***********************************************************************************/

OpString* OpSecurityInformationContainer::GetIssuerSummaryString()
{
	if (m_issuer_summary.IsEmpty())
	{
		BOOL missing = FALSE; // Flag to signal that one field is missing so we can insert another in stead.
		if (!AppendCommaSeparated(m_issuer_common_name, m_issuer_summary))
			missing = TRUE;
		if (!AppendCommaSeparated(m_issuer_organization, m_issuer_summary))
			missing = TRUE;
		if (missing)
			AppendCommaSeparated(m_issuer_organizational_unit, m_issuer_summary);
	}
	return &m_issuer_summary;
}

/***********************************************************************************
**
**	OpSecurityInformationContainer::GetFormattedExpiryDate
**
***********************************************************************************/

OpString* OpSecurityInformationContainer::GetFormattedExpiryDate()
{
	if (m_formatted_expiry_date.IsEmpty())
	{
		tm time;
		if (OpStatus::IsSuccess(GetTMStructTime(m_expires, time)))
		{
			//m_formatted_expiry_date.Reserve(64);
			//g_oplocale->op_strftime(m_formatted_expiry_date.CStr(), m_formatted_expiry_date.Capacity(), UNI_L("%x"), &time);
//			int buffer_size = 64;
//			uni_char* buffer = new uni_char[buffer_size];
//			int length = g_oplocale->op_strftime(buffer, buffer_size, UNI_L("%x"), &time);
//			m_formatted_expiry_date.Set(buffer, length);
		}
	}
	return &m_formatted_expiry_date;
}

/***********************************************************************************
**
**	OpSecurityInformationContainer::AppendCommaSeparated
**
***********************************************************************************/

BOOL OpSecurityInformationContainer::AppendCommaSeparated(const OpString &src, OpString &trg)
{
	if (src.IsEmpty())
		return FALSE;
	if (!(trg.IsEmpty()))
		trg.Append(UNI_L(", "));
	trg.Append(src);
	return TRUE;
}

/***********************************************************************************
**
**	OpSecurityInformationContainer::GetTMStructTime
**
***********************************************************************************/

OP_STATUS OpSecurityInformationContainer::GetTMStructTime(const OpString &src, tm &trg)
{
	// time format: 03.02.2008 12:19:19 GMT

	OpString time_string, date_substring, time_substring, day, month, year, hour, minute, second; // Substrings
	int date_len, time_len, day_len, month_len, hour_len, minute_len; // Length of the substrings

	uni_char space  = ' ';
	uni_char period = '.';
	uni_char colon  = ':';

	time_string.Set(src);
	date_len = time_string.FindFirstOf(space);
	time_len = time_string.FindLastOf(space) - date_len - 1;
	date_substring.Set( time_string.SubString(0,            date_len).CStr(), date_len );
	time_substring.Set( time_string.SubString(date_len + 1, time_len).CStr(), time_len );

	day_len   = date_substring.FindFirstOf(period);
	month_len = date_substring.FindLastOf(period) - day_len - 1;
	day.Set(   date_substring.SubString(0,                       day_len).CStr()  , day_len   );
	month.Set( date_substring.SubString(day_len + 1,             month_len).CStr(), month_len );
	year.Set(  date_substring.SubString(day_len + month_len + 2));

	hour_len   = time_substring.FindFirstOf(colon);
	minute_len = time_substring.FindLastOf(colon) - hour_len - 1;
	hour.Set(   time_substring.SubString(0,                       hour_len).CStr(), hour_len );
	minute.Set( time_substring.SubString(hour_len + 1,            hour_len).CStr(), minute_len );
	second.Set( time_substring.SubString(hour_len + minute_len + 2));

	trg.tm_year = uni_atoi(year.CStr()) - 1900;
	trg.tm_mon  = uni_atoi(month.CStr());
	trg.tm_mday = uni_atoi(day.CStr());
	trg.tm_hour = uni_atoi(hour.CStr());
	trg.tm_min  = uni_atoi(minute.CStr());
	trg.tm_sec  = uni_atoi(second.CStr());
	trg.tm_isdst = (int)(g_op_time_info->DaylightSavingsTimeAdjustmentMS(g_op_time_info->GetTimeUTC()) / 3600000);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Class SecurityInformationParser::SecurityInfoNode
**
***********************************************************************************/

SecurityInformationParser::SecurityInfoNode::SecurityInfoNode(const uni_char* tag_name, SecurityInfoNode* parent)
{
	this->tag_name.Set(tag_name);
	this->header.Empty();
	text.Empty();
	this->parent = parent;
	children.Clear();
	position     = -1;
	iterator     = 0;
}

SecurityInformationParser::SecurityInfoNode::~SecurityInfoNode()
{
	while (children.GetCount())
	{
		SecurityInfoNode* child = children.Remove(0);
		OP_DELETE(child);
	}
}

SecurityInformationParser::SecurityInfoNode* SecurityInformationParser::SecurityInfoNode::GetNextChild()
{
	SecurityInfoNode* return_value = NULL;
	if (children.GetCount() > iterator)
	{
		return_value = children.Get(iterator);
		iterator++;
	}
	return return_value;
}

SecurityInformationParser::SecurityInfoNode* SecurityInformationParser::SecurityInfoNode::GetChild(unsigned int index)
{
	if (children.GetCount() > index)
	{
		return children.Get(index);
	}
	else
	{
		return NULL;
	}
}

/***********************************************************************************
**
**	Class TagStack
**
***********************************************************************************/

SecurityInformationParser::ParsingState::TagStack::TagStack()
{
	tag_stack.Clear();
}

SecurityInformationParser::ParsingState::TagStack::~TagStack()
{
	tag_stack.DeleteAll();
}

void SecurityInformationParser::ParsingState::TagStack::Init()
{
	tag_stack.DeleteAll();
}

void SecurityInformationParser::ParsingState::TagStack::Push(OpString &tag)
{
	OpString* tag_string = OP_NEW(OpString, ());
	if (tag_string)
	{
		tag_string->Set(tag);
		tag_stack.Insert(0, tag_string);
	}
}

OpString* SecurityInformationParser::ParsingState::TagStack::Pop(OpString &tag)
{
	if (tag_stack.GetCount() <= 0)
	{
		tag.Empty();
	}
	else
	{
		OpString* tmp = tag_stack.Remove(0);
		tag.Set(*tmp);
		OP_DELETE(tmp);
	}
	return &tag;
}

OpString* SecurityInformationParser::ParsingState::TagStack::Top(OpString &tag)
{
	if (tag_stack.GetCount() <= 0)
	{
		tag.Empty();
	}
	else
	{
		tag.Set(*(tag_stack.Get(0)));
	}
	return &tag;
}

/***********************************************************************************
**
**	Class ParsingState
**
***********************************************************************************/

SecurityInformationParser::ParsingState::ParsingState()
{
	Init();
}

SecurityInformationParser::ParsingState::~ParsingState()
{
}

void SecurityInformationParser::ParsingState::Init()
{
	// The state machine needs initialization.
	m_tag_stack.Init();

	m_certificate_number = 0;
	m_protocol_number    = 0;
	m_certificate_servername_number = 0;
	m_url_number         = 0;
	m_port_number        = 0;
	m_subject_number     = 0;
	m_issuer_number      = 0;
	m_issued_depth       = 0;
	m_expires_depth      = 0;
	m_current_certificate_name = NULL;
	m_current_node_text.Empty();
}

void SecurityInformationParser::ParsingState::StartTag(OpString &tag)
{
	m_tag_stack.Push(tag);
	if (!tag.Compare("certificate"))
	{
		m_certificate_number++;
	}
	else if (!tag.Compare("protocol"))
	{
		m_protocol_number++;
	}
	else if (!tag.Compare("certificate_servername"))
	{
		m_certificate_servername_number++;
	}
	else if (!tag.Compare("url"))
	{
		m_url_number++;
	}
	else if (!tag.Compare("port"))
	{
		m_port_number++;
	}
	else if (!tag.Compare("subject"))
	{
		m_subject_number++;
	}
	else if (!tag.Compare("issuer"))
	{
		m_issuer_number++;
	}
	else if (!tag.Compare("issued"))
	{
		m_issued_depth++;
	}
	else if (!tag.Compare("expires"))
	{
		m_expires_depth++;
	}
}

OpString* SecurityInformationParser::ParsingState::EndTag(OpString &tag)
{
	/* Consitency check... The assert should never trigger, the parser should return error long before that...
	OpString dummy;
	m_tag_stack.Top(dummy);
	OP_ASSERT(!tag.Compare(dummy));
	*/
	if (!tag.Compare("issued"))
	{
		m_issued_depth--;
	}
	else if (!tag.Compare("expires"))
	{
		m_expires_depth--;
	}
	return m_tag_stack.Pop(tag);
}

OpString* SecurityInformationParser::ParsingState::GetTopTag(OpString &tag)
{
	return m_tag_stack.Top(tag);
}


BOOL SecurityInformationParser::ParsingState::IsURL()
{
	return (m_url_number == 1);
}

BOOL SecurityInformationParser::ParsingState::IsPort()
{
	return (m_port_number == 1);
}

BOOL SecurityInformationParser::ParsingState::IsProtocol()
{
	return (m_protocol_number == 1);
}

BOOL SecurityInformationParser::ParsingState::IsCertificateServerName()
{
	return (m_certificate_servername_number == 1);
}

BOOL SecurityInformationParser::ParsingState::IsSubject()
{
	return (m_certificate_number == 1) && (m_subject_number == 1) && (m_issuer_number == 0);
}

BOOL SecurityInformationParser::ParsingState::IsIssuer()
{
	return (m_certificate_number == 1) && (m_subject_number == 1) && (m_issuer_number == 1);
}

BOOL SecurityInformationParser::ParsingState::IsFirstCert()
{
	return (m_certificate_number == 1);
}

BOOL SecurityInformationParser::ParsingState::IsIssuedDate()
{
	return (m_certificate_number == 1) && (m_issued_depth == 1);
}

BOOL SecurityInformationParser::ParsingState::IsExpiredDate()
{
	return (m_certificate_number == 1) && (m_expires_depth == 1);
}

SecurityInformationParser::TagClass SecurityInformationParser::ParsingState::GetTopTagClass()
{
	OpString tag_name;
	m_tag_stack.Top(tag_name);
	if      (!tag_name.Compare("securityinformation") ||
		     !tag_name.Compare("url")                 ||
			 !tag_name.Compare("port")                ||
		     !tag_name.Compare("certchain")           ||
		     !tag_name.Compare("certificate")         ||
		     !tag_name.Compare("servercertchain")     ||
		     !tag_name.Compare("validatedcertchain")  ||
		     !tag_name.Compare("clientcertchain")     ||
		     !tag_name.Compare("tr")                    )
	{
		return NEW_NODE_TAG;
	}
	else if (!tag_name.Compare("caption"))
	{
		return IGNORE_TAG;
	}
	if      (!tag_name.Compare("keytype")           ||
		     !tag_name.Compare("keysize")           ||
		     !tag_name.Compare("keyconfiguration")  ||
		     !tag_name.Compare("keyalg")            ||
		     !tag_name.Compare("keyname")           ||
		     !tag_name.Compare("keydata")           ||
		     !tag_name.Compare("extensions")        ||
		     !tag_name.Compare("extensionname")     ||
		     !tag_name.Compare("extensiondata")        )
	{
		return MARKER_TAG;
	}
	else if (!tag_name.Compare("th"))
	{
		return CONTENT_IS_PARENT_HEADING;
	}
	else if (!tag_name.Compare("extensions"))
	{
		return TAG_NAME_DEFINES_PARENT_HEADING;
	}
	else if (!tag_name.Compare("a"))
	{
		return REFERENCE_TAG;
	}
	else if (!tag_name.Compare("userconfirmedcertificate")        ||
		     !tag_name.Compare("AnonymousConnection")             ||
		     !tag_name.Compare("unknownca")                       ||
		     !tag_name.Compare("Certexpired")                     ||
		     !tag_name.Compare("CertNotYetValid")                 ||
		     !tag_name.Compare("ConfiguredWarning")               ||
		     !tag_name.Compare("AuthenticationOnly_NoEncryption") ||
		     !tag_name.Compare("LowEncryptionlevel")              ||
		     !tag_name.Compare("ServernameMismatch")              ||          	     
		     !tag_name.Compare("RenegExtensionUnsupported"))
	{
		 return BOOLEAN_TAG;
	}
	else
	{
		return ORDINARY_TAG;
	}
}

SecurityInformationParser::TagClass SecurityInformationParser::ParsingState::GetTopNonMarkerTagClass()
{
	UINT i = 0;
	OpString marker_tag_elm;

	// Pop our way down the stack until we find a tag that is not a MARKER_TAG
	while (GetTopTagClass() == MARKER_TAG)
	{
		m_tag_stack.Pop(marker_tag_elm);
		i++;
	}

	// Get the class that has meaning
	TagClass tag_class = GetTopTagClass();

	// Replace the MARKER_TAGs in the stack
	for (UINT j = 0; j < i; j++)
	{
		m_tag_stack.Push(marker_tag_elm);
	}

	return tag_class;
}

void SecurityInformationParser::ParsingState::SetCurrentNodeText(OpString &text)
{
	m_current_node_text.Set(text);
}

void SecurityInformationParser::ParsingState::AppendCurrentNodeText(OpString &text)
{
	m_current_node_text.Append(text);
}

OpString* SecurityInformationParser::ParsingState::GetCurrentNodeText(OpString &text)
{
	text.Set(m_current_node_text);
	return &text;
}

void SecurityInformationParser::ParsingState::ClearCurrentNodeText()
{
	m_current_node_text.Empty();
}

void SecurityInformationParser::ParsingState::SetCurrentCertificateNamePtr(OpString* name_ptr)
{
	m_current_certificate_name = name_ptr;
}

OpString* SecurityInformationParser::ParsingState::GetCurrentCertificateNamePtr()
{
	return m_current_certificate_name;
}

/***********************************************************************************
**
**	Class SecurityInformationParser
**
***********************************************************************************/

SecurityInformationParser::SecurityInformationParser() :
	m_tree_model(NULL),
	m_validated_tree_model(NULL),
	m_client_tree_model(NULL),
	m_info_container(NULL),
	m_security_information_tree_root(NULL),
	m_current_sec_info_element(NULL),
	m_server_cert_chain_root(NULL),
	m_validated_cert_chain_root(NULL),
	m_client_cert_chain_root(NULL)
{
}

OP_STATUS SecurityInformationParser::SetSecInfo(URL security_information_url)
{
	m_security_information_url = security_information_url;
	return OpStatus::OK;
}

SecurityInformationParser::~SecurityInformationParser()
{
	OP_DELETE(m_info_container);
	OP_DELETE(m_security_information_tree_root);
	OP_DELETE(m_tree_model);
	OP_DELETE(m_validated_tree_model);
	OP_DELETE(m_client_tree_model);
}

SimpleTreeModel* SecurityInformationParser::GetServerTreeModelPtr()
{
	if (!m_tree_model)
	{
		if (!(m_tree_model = OP_NEW(SimpleTreeModel, ()))
			|| OpStatus::IsError(MakeServerTreeModel(*m_tree_model)))
			return NULL;
	}
	return m_tree_model;
}

SimpleTreeModel* SecurityInformationParser::GetValidatedTreeModelPtr()
{
	if (!m_validated_tree_model)
	{
		if (!(m_validated_tree_model = OP_NEW(SimpleTreeModel, ()))
			|| OpStatus::IsError(MakeValidatedTreeModel(*m_validated_tree_model)))
			return NULL;
	}
	return m_validated_tree_model;
}

SimpleTreeModel* SecurityInformationParser::GetClientTreeModelPtr()
{
	if (!m_client_tree_model)
	{
		if (!(m_client_tree_model = OP_NEW(SimpleTreeModel, ()))
			|| OpStatus::IsError(MakeClientTreeModel(*m_client_tree_model)))
			return NULL;
	}
	return m_client_tree_model;
}

OP_STATUS SecurityInformationParser::MakeServerTreeModel(SimpleTreeModel &tree_model)
{
	if (!m_server_cert_chain_root)
		return OpStatus::ERR;
	tree_model.RemoveAll();
	SecurityInfoNode* traverser = m_server_cert_chain_root;
	while (traverser && (traverser != m_server_cert_chain_root->GetParent()) ) // Stop when the entire subtree has been searced. Do not allow the search to proceed further up the tree if a parent exists.
	// ISSUE: This function will fail if it is reused. Will only work once!
	{
		// Insert the node in the tree model.
		if (traverser && (traverser != m_server_cert_chain_root) && !traverser->IsInTree()) // Nodes must not be written to the tree model when they are visited a second time.
		{
			traverser->ResetChildIterator(); // This is the first time this node is visited, so an iterator reset is healthy.
			traverser->SetTreePosition(tree_model.AddItem(traverser->GetHeader(), NULL, 0, (traverser->GetParent() ? traverser->GetParent()->GetPositionInTree() : -1), traverser->GetText()));
		}

		// Iterating the traverser to the next node. Return to the parent if all children have been visited.
		SecurityInfoNode* tmp_node_ptr = traverser->GetNextChild();
		if (tmp_node_ptr)
		{
			traverser = tmp_node_ptr;
		}
		else
		{
			traverser = traverser->GetParent();
		}
	}
	return OpStatus::OK;
}

OP_STATUS SecurityInformationParser::MakeValidatedTreeModel(SimpleTreeModel &tree_model)
{
	if (!m_validated_cert_chain_root)
		return OpStatus::ERR;
	tree_model.RemoveAll();
	SecurityInfoNode* traverser = m_validated_cert_chain_root;
	while (traverser && (traverser != m_validated_cert_chain_root->GetParent()) ) // Stop when the entire subtree has been searced. Do not allow the search to proceed further up the tree if a parent exists.
	// ISSUE: This will cause the function to fail if it is reused. Will only work once!
	{
		// Insert the node in the tree model.
		if (traverser && (traverser != m_validated_cert_chain_root) && !traverser->IsInTree()) // Nodes must not be written to the tree model when they are visited a second time.
		{
			traverser->ResetChildIterator(); // This is the first time this node is visited, so an iterator reset is healthy.
			traverser->SetTreePosition(tree_model.AddItem(traverser->GetHeader(), NULL, 0, (traverser->GetParent() ? traverser->GetParent()->GetPositionInTree() : -1), traverser->GetText()));
		}

		// Iterating the traverser to the next node. Return to the parent if all children have been visited.
		SecurityInfoNode* tmp_node_ptr = traverser->GetNextChild();
		if (tmp_node_ptr)
		{
			traverser = tmp_node_ptr;
		}
		else
		{
			traverser = traverser->GetParent();
		}
	}
	return OpStatus::OK;
}

OP_STATUS SecurityInformationParser::MakeClientTreeModel(SimpleTreeModel &tree_model)
{
	if (!m_client_cert_chain_root)
		return OpStatus::ERR;
	tree_model.RemoveAll();
	SecurityInfoNode* traverser = m_client_cert_chain_root;
	while (traverser && (traverser != m_client_cert_chain_root->GetParent()) ) // Stop when the entire subtree has been searced. Do not allow the search to proceed further up the tree if a parent exists.
	// ISSUE: This will cause the function to fail if it is reused. Will only work once!
	{
		// Insert the node in the tree model.
		if (traverser && (traverser != m_client_cert_chain_root) && !traverser->IsInTree()) // Nodes must not be written to the tree model when they are visited a second time.
		{
			traverser->ResetChildIterator(); // This is the first time this node is visited, so an iterator reset is healthy.
			traverser->SetTreePosition(tree_model.AddItem(traverser->GetHeader(), NULL, 0, (traverser->GetParent() ? traverser->GetParent()->GetPositionInTree() : -1), traverser->GetText()));
		}

		// Iterating the traverser to the next node. Return to the parent if all children have been visited.
		SecurityInfoNode* tmp_node_ptr = traverser->GetNextChild();
		if (tmp_node_ptr)
		{
			traverser = tmp_node_ptr;
		}
		else
		{
			traverser = traverser->GetParent();
		}
	}
	return OpStatus::OK;
}

OP_STATUS SecurityInformationParser::Parse(BOOL limited_parse)
{
	if (!m_info_container)
	{
		m_info_container = OP_NEW(OpSecurityInformationContainer, ()); // Initializing the info container.
		if (!m_info_container)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	URL_DataDescriptor *url_dd_tmp = m_security_information_url.GetDescriptor(NULL, TRUE); // Seems to cause crashing on second run...
	if (!url_dd_tmp)
	{
		return OpStatus::ERR_NULL_POINTER;
	}
	OpAutoPtr<URL_DataDescriptor> url_dd(url_dd_tmp);

	XMLParser *parser = NULL;
	RETURN_IF_ERROR(XMLParser::Make(parser, (XMLParser::Listener*) NULL, (MessageHandler*)NULL, this, m_security_information_url));
	OpAutoPtr<XMLParser> p(parser);

	// Initialize the state.
	if (!(m_state = OP_NEW(ParsingState, ())))
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<ParsingState> state(m_state);

	XMLParser::Configuration xml_config;
	xml_config.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	parser->SetConfiguration(xml_config);

	OP_MEMORY_VAR unsigned long len;
	BOOL more = TRUE;
	while (more)
	{
		// Get the buffer and its length
		RETURN_IF_LEAVE(len = url_dd->RetrieveDataL(more));
		uni_char* buffer = (uni_char*)url_dd->GetBuffer();
		unsigned long uni_len = UNICODE_DOWNSIZE(len);

		// We parse the output in smaller chunks
		// so we can abort sooner after getting
		// the necessary fields if we're doing
		// a limited parse.
		while (uni_len > 0)
		{
			// Get chunk size.
			unsigned long chunk_size = 500;
			if (uni_len < chunk_size )
				chunk_size = uni_len;

			// In this inner loop, there is always more, unless parsing the
			// very last chunk of the last block of data retrieved.
			BOOL inner_more = TRUE;
			if (!more && uni_len <= chunk_size)
				inner_more = FALSE;

			// Parse chunk
			RETURN_IF_ERROR(parser->Parse(buffer, chunk_size, inner_more));

			// Jump to next chunk.
			buffer  += chunk_size;
			uni_len -= chunk_size;

			// Check if the needed fields have been found.
			if (limited_parse && !(m_info_container->GetSubjectCommonNamePtr()->IsEmpty()) && (!m_info_container->GetSubjectCountryPtr()->IsEmpty()))
			{
				return OpStatus::OK;
			}
		}

		url_dd->ConsumeData(len);
	}

	return OpStatus::OK;
}

OP_STATUS SecurityInformationParser::LimitedParse(OpString &button_text, BOOL isEV)
{
 	RETURN_IF_ERROR(Parse(TRUE));

	// For Extended security return the full Text with Organisation etc
	if (isEV)
		return button_text.Set(m_info_container ? m_info_container->GetButtonTextPtr()->CStr() : NULL);
	else // High security gets only the certificate_servername
		return button_text.Set(m_info_container ? m_info_container->GetCertificateServerNamePtr()->CStr() : NULL);
}

/*virtual*/ XMLTokenHandler::Result
SecurityInformationParser::HandleToken(XMLToken &token)
{
	switch (token.GetType())
	{
	case XMLToken::TYPE_STag:
		StartElementHandler(token);
		break;

	case XMLToken::TYPE_ETag:
		EndElementHandler(token);
		break;
	case XMLToken::TYPE_EmptyElemTag:
		EmptyElementHandler(token);
		break;

	case XMLToken::TYPE_Text:
		{
			uni_char *text_data = token.GetLiteralAllocatedValue();
			CharacterDataHandler(text_data, token.GetLiteralLength());
			OP_DELETEA(text_data);
		}
		break;
	}

	return XMLTokenHandler::RESULT_OK;
}

void SecurityInformationParser::StartElementHandler(XMLToken &token)
{
	OpString tag_name;
	tag_name.Set(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength());
	m_state->StartTag(tag_name);
	TagClass tag_class = m_state->GetTopTagClass();

	if (tag_class == MARKER_TAG)
	{
		tag_class = m_state->GetTopNonMarkerTagClass();
	}
	else
	{
		m_state->ClearCurrentNodeText();
	}

	switch (tag_class)
	{
	case NEW_NODE_TAG:
		{
			SecurityInfoNode* element = OP_NEW(SecurityInfoNode, (tag_name.CStr(), m_current_sec_info_element));
			if (!element)
				break;

			if (m_current_sec_info_element)
			{
				m_current_sec_info_element->AddChild(element);
			}
			if (!m_security_information_tree_root)
			{
				m_security_information_tree_root = element;
			}
			m_current_sec_info_element = element;

			// Keep track of the root of the certificate chain trees
			if (!tag_name.Compare(UNI_L("servercertchain")))
			{
				m_server_cert_chain_root = m_current_sec_info_element;
			}
			else if (!tag_name.Compare(UNI_L("clientcertchain")))
			{
				m_client_cert_chain_root = m_current_sec_info_element;
			}
			else if (!tag_name.Compare(UNI_L("validatedcertchain")))
			{
				m_validated_cert_chain_root = m_current_sec_info_element;
			}
			else if (!tag_name.Compare(UNI_L("certificate")))
			{
				m_state->SetCurrentCertificateNamePtr(m_current_sec_info_element->GetHeaderPtr());
				if (!m_server_cert_chain_root)
					m_server_cert_chain_root = m_current_sec_info_element; // This ensures that the server cert chain always points to something (the first <certificate>), even if there is no servercertchain in the document.
			}
			break;
		}
	case TAG_NAME_DEFINES_PARENT_HEADING:
		{
			if (m_current_sec_info_element)
				m_current_sec_info_element->SetHeader(tag_name.CStr(), tag_name.Length());
			break;
		}
	case REFERENCE_TAG:
		{
			if (XMLToken::Attribute *href_attr = token.GetAttribute(UNI_L("href"), 4))
			{
				m_current_sec_info_element->AppendToText(href_attr->GetValue(), href_attr->GetValueLength());
			}
			break;
		}
	}
}

void SecurityInformationParser::EndElementHandler(XMLToken &token)
{
	TagClass tag_class = m_state->GetTopTagClass();
	OpString tag_name;
	tag_name.Set(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength());
	CheckForAndExtractSpecialInfo(tag_name);
	m_state->EndTag(tag_name);

	if (tag_class == MARKER_TAG)
	{
		tag_class = m_state->GetTopNonMarkerTagClass();
	}
	else
	{
		m_state->ClearCurrentNodeText();
	}
	BOOL is_new_node_tag = (tag_class == NEW_NODE_TAG);

	if (is_new_node_tag)
	{
		if (m_current_sec_info_element)
			m_current_sec_info_element = m_current_sec_info_element->GetParent();
	}
}

void SecurityInformationParser::EmptyElementHandler(XMLToken &token)
{
	OpString tag_name;
	tag_name.Set(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength());
	CheckForAndExtractSpecialInfo(tag_name);
}

void SecurityInformationParser::CharacterDataHandler(const uni_char *s, int len)
{
	OpString node_text;
	node_text.Set(s, len);
	m_state->AppendCurrentNodeText(node_text);
	OpString tag_name_from_stack;
	m_state->GetTopTag(tag_name_from_stack);

	switch (m_state->GetTopNonMarkerTagClass())
	{
	case NEW_NODE_TAG:
		{
			// if the tag is a url tag, the content is not a pair, but just the url. This requires special attention, which is done here.
			if ( !tag_name_from_stack.Compare("url") ||
				 !tag_name_from_stack.Compare("port")   )
			{
				if (m_current_sec_info_element)
					m_current_sec_info_element->AppendToText(s, len);
			}
			break;
		}
	case CONTENT_IS_PARENT_HEADING:
		{
			if (m_current_sec_info_element)
			{
				OpString current_node_text;
				m_state->GetCurrentNodeText(current_node_text);
				m_current_sec_info_element->SetHeader(current_node_text.CStr(), current_node_text.Length());
			}
			break;
		}
	case ORDINARY_TAG:
		{
			if (m_current_sec_info_element)
				m_current_sec_info_element->AppendToText(s, len);

			break;
		}
	}
}

void SecurityInformationParser::CheckForAndExtractSpecialInfo(OpString &current_XML_tag)
{
	if      ( m_state->IsURL() && (!current_XML_tag.Compare("url")) )
	{
		m_info_container->GetServerURLPtr()->Set(*m_current_sec_info_element->GetTextPtr());
	}
	else if ( m_state->IsPort() && (!current_XML_tag.Compare("port")) )
	{
		m_info_container->GetServerPortNumberPtr()->Set(*m_current_sec_info_element->GetTextPtr());
	}
	else if ( m_state->IsProtocol() && (!current_XML_tag.Compare("protocol")) )
	{
		m_info_container->GetProtocolPtr()->Set(*m_current_sec_info_element->GetTextPtr());
	}
	else if ( m_state->IsCertificateServerName() && (!current_XML_tag.Compare("certificate_servername")) )
	{
		m_info_container->GetCertificateServerNamePtr()->Set(*m_current_sec_info_element->GetTextPtr());
	}
	else if ( m_state->IsIssuedDate() && (!current_XML_tag.Compare("issued")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuedPtr());
	}
	else if ( m_state->IsExpiredDate() && (!current_XML_tag.Compare("expires")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetExpiresPtr());
	}
	else if ( m_state->IsSubject() && (!current_XML_tag.Compare("cn")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetSubjectCommonNamePtr());
	}
	else if ( m_state->IsSubject() && (!current_XML_tag.Compare("o")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetSubjectOrganizationPtr());
	}
	else if ( m_state->IsSubject() && (!current_XML_tag.Compare("ou")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetSubjectOrganizationalUnitPtr());
	}
	else if ( m_state->IsSubject() && (!current_XML_tag.Compare("lo")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetSubjectLocalityPtr());
	}
	else if ( m_state->IsSubject() && (!current_XML_tag.Compare("pr")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetSubjectProvincePtr());
	}
	else if ( m_state->IsSubject() && (!current_XML_tag.Compare("co")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetSubjectCountryPtr());
	}
	else if ( m_state->IsIssuer() && (!current_XML_tag.Compare("cn")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuerCommonNamePtr());
	}
	else if ( m_state->IsIssuer() && (!current_XML_tag.Compare("o")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuerOrganizationPtr());
	}
	else if ( m_state->IsIssuer() && (!current_XML_tag.Compare("ou")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuerOrganizationalUnitPtr());
	}
	else if ( m_state->IsIssuer() && (!current_XML_tag.Compare("lo")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuerLocalityPtr());
	}
	else if ( m_state->IsIssuer() && (!current_XML_tag.Compare("pr")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuerProvincePtr());
	}
	else if ( m_state->IsIssuer() && (!current_XML_tag.Compare("co")) )
	{
		m_state->GetCurrentNodeText(*m_info_container->GetIssuerCountryPtr());
	}

	// If the tag is one of the special warning tags, this function will set the correct bool.
	m_info_container->SetSecurityIssue(current_XML_tag.CStr());

	// The following is to fill the root element of each certificate with a meaningful name.
	OpString* m_current_certificate_name = m_state->GetCurrentCertificateNamePtr()	;
	if (m_current_certificate_name            &&
		m_current_certificate_name->IsEmpty() &&
		( !current_XML_tag.Compare(UNI_L("cn")) ||
		  !current_XML_tag.Compare(UNI_L("o"))  ||
		  !current_XML_tag.Compare(UNI_L("ou"))   ) )
	{
		m_state->GetCurrentNodeText(*m_current_certificate_name);
	}
}
#endif //SECURITY_INFORMATION_PARSER
