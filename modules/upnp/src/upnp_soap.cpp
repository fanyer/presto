/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UPNP_SUPPORT

#include "modules/upnp/upnp_upnp.h"
#include "modules/upnp/src/upnp_soap.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlerrorreport.h"


OP_STATUS HTTPMessageWriter::setSoapAction(const uni_char *service_type, const uni_char *action, const uni_char *ns)
{
	RETURN_IF_ERROR(soap_action_ns.Set(ns));
	RETURN_IF_ERROR(soap_action.Set(UNI_L("\"")));
	RETURN_IF_ERROR(soap_action.Append(service_type));
	RETURN_IF_ERROR(soap_action.Append("#"));
	RETURN_IF_ERROR(soap_action.Append(action));
	RETURN_IF_ERROR(soap_action.Append(UNI_L("\"")));
	
	return OpStatus::OK;
}

OP_STATUS HTTPMessageWriter::setURL(const uni_char *hostAddress, const uni_char *pageURL, UINT16 port)
{
	RETURN_IF_ERROR(host.Set(hostAddress));
	
	if(!pageURL)
		return OpStatus::ERR_NULL_POINTER;
		
	if(URLStartWithHTTP(pageURL))
	{
		URL url=g_url_api->GetURL(pageURL);
		RETURN_IF_ERROR(url.GetAttribute(URL::KPathAndQuery_L, page));
		httpPort=url.GetServerPort();
	}
	else
	{
		RETURN_IF_ERROR(page.Set(pageURL));
		httpPort=port;
	}
	
	return OpStatus::OK;
}

OP_STATUS HTTPMessageWriter::GetMessage(OpString *out_message, const char* http_action, const OpString *body)
{
	OP_ASSERT(out_message);
	
	if(!out_message)
	  return OpStatus::ERR_NO_MEMORY;
	
	OP_ASSERT(!page.IsEmpty() && !host.IsEmpty());
	
	if(page.IsEmpty() || host.IsEmpty())
	  return OpStatus::ERR_OUT_OF_RANGE;
	  
	out_message->Empty();
	
	// Add the HTTP headers
	char buf_int[12];   /* ARRAY OK 2009-04-23 lucav */
	
	op_itoa(httpPort, buf_int, 10);
	RETURN_IF_ERROR(out_message->Append(http_action));
	RETURN_IF_ERROR(out_message->Append(" "));
	RETURN_IF_ERROR(out_message->Append(page));
	RETURN_IF_ERROR(out_message->Append(" HTTP/1.1\r\n"));
	RETURN_IF_ERROR(out_message->Append("HOST: "));
	RETURN_IF_ERROR(out_message->Append(host));
	RETURN_IF_ERROR(out_message->Append(":"));
	RETURN_IF_ERROR(out_message->Append(buf_int));
	RETURN_IF_ERROR(out_message->Append("\r\n"));
	
	if(!content_type.IsEmpty())
	{
		RETURN_IF_ERROR(out_message->Append("CONTENT-TYPE: "));
		RETURN_IF_ERROR(out_message->Append(content_type));
		RETURN_IF_ERROR(out_message->Append(";"));
		
		if(!charset.IsEmpty())
		{
			RETURN_IF_ERROR(out_message->Append("charset=\""));
			RETURN_IF_ERROR(out_message->Append(charset));
			RETURN_IF_ERROR(out_message->Append("\";"));
		}
		
		RETURN_IF_ERROR(out_message->Append("\r\n"));
	}
	
	RETURN_IF_ERROR(WriteHeader(out_message, "CALLBACK", &callback));
	RETURN_IF_ERROR(WriteHeader(out_message, "NT", &nt));
	RETURN_IF_ERROR(WriteHeaderNS(out_message, "MAN", &mandatory, &mandatory_ns));
	RETURN_IF_ERROR(WriteHeader(out_message, "OPT", &optional));
	
	OpString str_timeout;
	
	RETURN_IF_ERROR(str_timeout.Append("Second-"));
	
	if(timeout>0)
	{
		op_ltoa(timeout, buf_int, 10);
		
		RETURN_IF_ERROR(str_timeout.Append(buf_int));
		RETURN_IF_ERROR(WriteHeader(out_message, "TIMEOUT", &str_timeout));
	}
	else
		RETURN_IF_ERROR(WriteHeader(out_message, "TIMEOUT", UNI_L("infinite")));
	
	OpString8 soap_header;
	
	if(!soap_action_ns.IsEmpty())
	{
		RETURN_IF_ERROR(soap_header.Set(soap_action_ns.CStr()));
		RETURN_IF_ERROR(soap_header.Append("-"));
	}
	RETURN_IF_ERROR(soap_header.Append("SOAPACTION"));
	RETURN_IF_ERROR(WriteHeader(out_message, soap_header.CStr(), &soap_action));
	
	
    if(body)
	{
		op_itoa(body->Length(), buf_int, 10);
		
		RETURN_IF_ERROR(out_message->Append("Content-Length: "));
		RETURN_IF_ERROR(out_message->Append(buf_int));
		RETURN_IF_ERROR(out_message->Append("\r\n"));
	}
	else
		RETURN_IF_ERROR(out_message->Append("Content-Length: 0\r\n"));
    
    RETURN_IF_ERROR(out_message->Append("\r\n")); // End of the HTTP header
    
    if(body)
		RETURN_IF_ERROR(out_message->Append(*body));

	return OpStatus::OK;
}

OP_STATUS HTTPMessageWriter::WriteHeader(OpString *out_message, const char *header, OpString *value)
{
	OP_ASSERT(out_message && header && value);
	
	if(!value)
		return OpStatus::ERR_NO_MEMORY;
		
	if(value->IsEmpty())
		return OpStatus::OK;

	return WriteHeader(out_message, header, value->CStr());
}

OP_STATUS HTTPMessageWriter::WriteHeader(OpString *out_message, const char *header, const uni_char *value)
{
	OP_ASSERT(out_message && header && value);
	
	if(!out_message || !header || !value)
		return OpStatus::ERR_NO_MEMORY;
	
    if(out_message && value && value[0])
	{
		RETURN_IF_ERROR(out_message->Append(header));
		RETURN_IF_ERROR(out_message->Append(": "));
		RETURN_IF_ERROR(out_message->Append(value));
		RETURN_IF_ERROR(out_message->Append("\r\n"));
	}
   
   return OpStatus::OK;
}

OP_STATUS HTTPMessageWriter::WriteHeaderNS(OpString *out_message, const char *header, OpString *value, OpString *ns)
{
	OP_ASSERT(out_message && header && value);
	
    if(out_message && value && !value->IsEmpty())
	{
		RETURN_IF_ERROR(out_message->Append(header));
		RETURN_IF_ERROR(out_message->Append(": "));
		RETURN_IF_ERROR(out_message->Append("\""));
		RETURN_IF_ERROR(out_message->Append(value->CStr()));
		RETURN_IF_ERROR(out_message->Append("\";"));
		if(ns && !ns->IsEmpty())
		{
			RETURN_IF_ERROR(out_message->Append(" ns="));
			RETURN_IF_ERROR(out_message->Append(ns->CStr()));
		}
		RETURN_IF_ERROR(out_message->Append("\r\n"));
	}
   
   return OpStatus::OK;
}

OP_STATUS HTTPMessageWriter::WriteHeader(OpString *out_message, const char *header, long value)
{
	char buf[16];	/* ARRAY OK 2008-08-12 lucav */
	OpString op;
	
	op_ltoa(value, buf, 10);
	RETURN_IF_ERROR(op.Append(buf));
	
	return WriteHeader(out_message, header, &op);
}

BOOL HTTPMessageWriter::URLStartWithHTTP(const uni_char *url)
{
	OP_ASSERT(url);

	if(!url)
		return FALSE;

	return !uni_strncmp(url, UNI_L("http://"), 7) || !uni_strncmp(url, UNI_L("HTTP://"), 7) || !uni_strncmp(url, UNI_L("https://"), 8) || !uni_strncmp(url, UNI_L("HTTPS://"), 8);
}


OP_STATUS SOAPMessage::GetActionCallXML(OpString &soap_mex, const uni_char *actionName, const uni_char *service_type, const uni_char *prefix, UINT32 num_params, ...)
{
	OpAutoVector<NamedValue> params;
	
    if(num_params>0)  // Read all the parameters
	{
		va_list param_pt;
	    
		va_start(param_pt,num_params);
		for (UINT32 index = 0 ; index < num_params ; index++) 
		{
			const uni_char* cur=va_arg(param_pt,const uni_char *);
			const uni_char* val=va_arg(param_pt,const uni_char *);
			
			OP_ASSERT(cur);
			
			if(cur)
			{
				NamedValue *nv=OP_NEW(NamedValue, ());
				OP_STATUS ops;
				
				if(!nv)
				  return OpStatus::ERR_NO_MEMORY;
				
				if(OpStatus::IsError(ops=params.Add(nv)))
				{
					OP_DELETE(nv);
					
					va_end (param_pt);
					
					return ops;
				}
				RETURN_IF_ERROR(nv->Construct(cur, val));
			}
		}
		va_end (param_pt);
	}
    
    return CostructMethod(soap_mex, actionName, service_type, (prefix)?prefix:UNI_L("m"), &params);
}
     
OP_STATUS SOAPMessage::CostructMethod(OpString &soap_mex, const uni_char *actionName, const uni_char *service_type, const uni_char *prefix, OpAutoVector<NamedValue> *params)
{
    OP_ASSERT(soap_mex.IsEmpty() && actionName && service_type);
    
    if(!actionName || !service_type)
      return OpStatus::ERR_NO_MEMORY;
      
	if(!soap_mex.IsEmpty())
		soap_mex.Empty();
		
	if(!prefix)
	  prefix=UNI_L("u");
	
	RETURN_IF_ERROR(soap_mex.Append(SOAP_HEADER));
	RETURN_IF_ERROR(soap_mex.Append("<"));
	RETURN_IF_ERROR(soap_mex.Append(prefix));
	RETURN_IF_ERROR(soap_mex.Append(":"));
	RETURN_IF_ERROR(soap_mex.Append(actionName));
	RETURN_IF_ERROR(soap_mex.Append(" xmlns:"));
	RETURN_IF_ERROR(soap_mex.Append(prefix));
	RETURN_IF_ERROR(soap_mex.Append("=\""));
	RETURN_IF_ERROR(soap_mex.Append(service_type));
	RETURN_IF_ERROR(soap_mex.Append("\">"));
	
	
	for(UINT i=0; i<params->GetCount(); i++)
	{
		OP_ASSERT(params && params->Get(i) && params->Get(i)->GetName());
		
		if(params && params->Get(i) && params->Get(i)->GetName())
		{
			RETURN_IF_ERROR(soap_mex.Append("<"));
			//RETURN_IF_ERROR(soap_mex.Append(prefix));
			//RETURN_IF_ERROR(soap_mex.Append(":"));
			RETURN_IF_ERROR(soap_mex.Append(params->Get(i)->GetName()->CStr()));
			RETURN_IF_ERROR(soap_mex.Append(">"));
			RETURN_IF_ERROR(soap_mex.Append(params->Get(i)->GetValue()->CStr()));
			RETURN_IF_ERROR(soap_mex.Append("</"));
			//RETURN_IF_ERROR(soap_mex.Append(prefix));
			//RETURN_IF_ERROR(soap_mex.Append(":"));
			RETURN_IF_ERROR(soap_mex.Append(params->Get(i)->GetName()->CStr()));
			RETURN_IF_ERROR(soap_mex.Append(">"));
		}
	}
	
	RETURN_IF_ERROR(soap_mex.Append("</"));
	RETURN_IF_ERROR(soap_mex.Append(prefix));
	RETURN_IF_ERROR(soap_mex.Append(":"));
	RETURN_IF_ERROR(soap_mex.Append(actionName));
	RETURN_IF_ERROR(soap_mex.Append(">"));
	RETURN_IF_ERROR(soap_mex.Append(SOAP_FOOTER));

	return OpStatus::OK;
}

Simple_XML_Handler::TagsParser::TagsParser(Simple_XML_Handler *uxh)
{
	open_father=FALSE;
	uxh_father=uxh;
	required_tags=0;
}

BOOL Simple_XML_Handler::TagsParser::AllValuesRead()
{
    for(UINT32 i=0; i<GetCountRequired(); i++)
	{
		if(!IsRead(i))
			return FALSE;
	}
    
    return TRUE;
}

OP_STATUS Simple_XML_Handler::TagsParser::Construct(const uni_char *father_tag, UINT32 required, UINT32 optional, int reset, ...)
{
	OP_ASSERT(GetCountAll()==0); // Construct() usually must be called only once
	OP_ASSERT(required>0);
	
	required_tags=required;
	
	UINT32 howmany_tags=required+optional;
	
	if(!required)
	  return OpStatus::ERR_OUT_OF_RANGE;
	
	if(reset<0)
		reset=(int)(required+optional);
		
	RETURN_IF_ERROR(father.Set(father_tag));
	
	// Set the array
	
	va_list param_pt;
	
    va_start(param_pt,reset);

	for (int index = 0 ; index < (int)howmany_tags ; index++) 
	{
		uni_char* cur=va_arg(param_pt,uni_char *);
		OpString * dest=va_arg(param_pt,OpString *);
		
		OP_ASSERT(NULL!=cur);
		
		TagValue *tv=OP_NEW(TagValue, ());
		OP_STATUS ops;
		
		if(!tv)
		  return OpStatus::ERR_NO_MEMORY;
		  
		tv->SetDest(dest);
		if(index<reset)
			tv->SetReset(TRUE);
		  
		if(OpStatus::IsError(ops=tv->SetName(cur)) || OpStatus::IsError(ops=tags.Add(tv)))
		{
			OP_DELETE(tv);
			
			va_end (param_pt);
			
			return ops;			
		}
	}
	
	va_end (param_pt);
	
	return OpStatus::OK;
}

OP_STATUS Simple_XML_Handler::TagsParser::Reset()
{
	open_father=FALSE;
	
	for(UINT32 i=0; i<GetCountAll(); i++)
		tags.Get(i)->AutoReset();
	
	return OpStatus::OK;
}

XMLTokenHandler::Result Simple_XML_Handler::TagsParser::HandleToken(XMLToken &token)
{
	//#ifdef _DEBUG
		// Save (a part of) the XML, for debugging
		if(uxh_father && uxh_father->GetXMLMaxLogSize()>0 && xml_read_debug.Length()<uxh_father->GetXMLMaxLogSize())
		{
			if(token.GetType()==XMLToken::TYPE_STag)
			{
				const uni_char *tag_name=token.GetName().GetLocalPart();
				int tag_len=token.GetName().GetLocalPartLength();
				
				xml_read_debug.Append("<");
				xml_read_debug.Append(tag_name, tag_len);
				xml_read_debug.Append(">");
			}
			else if(token.GetType()==XMLToken::TYPE_ETag)
			{
				const uni_char *tag_name=token.GetName().GetLocalPart();
				int tag_len=token.GetName().GetLocalPartLength();
				
				xml_read_debug.Append("</");
				xml_read_debug.Append(tag_name, tag_len);
				xml_read_debug.Append(">");
			}
			else if(token.GetType()==XMLToken::TYPE_Text)
			{
				XMLToken::Literal text;
	  
				if(!OpStatus::IsError(token.GetLiteral(text)))
				{
					for(unsigned int lp=0; lp<text.GetPartsCount(); lp++)
						xml_read_debug.AppendL(text.GetPart(lp), text.GetPartLength(lp));
				}
			}
			if(xml_read_debug.Length()>=uxh_father->GetXMLMaxLogSize())
			  xml_read_debug.Append(" (...)");
		}
	//#endif
	//const uni_char *name=(token.GetType()==XMLToken::TYPE_STag)?token.GetName().GetLocalPart():NULL;
	// Identify the father
	if(token.GetType()==XMLToken::TYPE_STag && father.Length()>0 && !uni_strncmp(token.GetName().GetLocalPart(), father.CStr(), token.GetName().GetLocalPartLength()))
		open_father=TRUE;
	// Manage the father closing
	else if(token.GetType()==XMLToken::TYPE_ETag && father.Length()>0 && !uni_strncmp(token.GetName().GetLocalPart(), father.CStr(), token.GetName().GetLocalPartLength()))
	{
		open_father=FALSE;
   
		for(UINT32 i=0; i<GetCountAll(); i++)
		{
			OP_ASSERT(tags.Get(i)!=NULL && !tags.Get(i)->IsOpened());
	     
			tags.Get(i)->SetOpened(FALSE);
		}
	}
	// Manage children opening
	else if(token.GetType()==XMLToken::TYPE_STag && (father.Length()==0 || open_father))
	{
		const uni_char *tagName=token.GetName().GetLocalPart();
		int tagLen=token.GetName().GetLocalPartLength();
		
		for(UINT32 i=0; i<GetCountAll(); i++)
		{
			OpString *curName=(!tags.Get(i))?NULL:tags.Get(i)->GetName();
			
			if(curName && 
			  !curName->Compare(tagName, tagLen) && curName->Length()==tagLen)
			{
				OP_ASSERT(!tags.Get(i)->IsOpened());
				
				tags.Get(i)->SetOpened(TRUE);
				tags.Get(i)->SetEmptyValue();
				break;
			}
		}
	}
	// Manage children closing
	else if(token.GetType()==XMLToken::TYPE_ETag && (father.Length()==0 || open_father))
	{
		const uni_char *tagName=token.GetName().GetLocalPart();
		int tagLen=token.GetName().GetLocalPartLength();
		
		for(UINT32 i=0; i<GetCountAll(); i++)
		{
			if(tags.Get(i) && tags.Get(i)->GetName() && 
			  !tags.Get(i)->GetName()->Compare(tagName, tagLen) && tags.Get(i)->GetName()->Length()==tagLen)
			{
				OP_ASSERT(tags.Get(i)->IsOpened());
				
				tags.Get(i)->SetOpened(FALSE);
				tags.Get(i)->SetRead(TRUE);
				break;
			}
		}
	}
	// Manage the text of the tag (it checks if the father is opened only for speed))
	else if(token.GetType()==XMLToken::TYPE_Text && (father.Length()==0 || open_father))
	{
		for(UINT32 i=0; i<GetCountAll(); i++)
		{
			OP_ASSERT(tags.Get(i));
		
			if(tags.Get(i) && tags.Get(i)->IsOpened())
			{
				OP_ASSERT(tags.Get(i)->IsValueEmpty());
			
				// Just in case, keep only the first value encountered
				if(!tags.Get(i)->IsValueEmpty())
					break;
				
				// Reconstruction of the text
				OP_STATUS ops;
				XMLToken::Literal text;
				unsigned int lp;
  
				if(OpStatus::IsError(ops=token.GetLiteral(text)))
					return XMLTokenHandler::RESULT_ERROR;
		
				for(lp=0; lp<text.GetPartsCount(); lp++)
					RETURN_VALUE_IF_ERROR(tags.Get(i)->AppendValue(text.GetPart(lp), text.GetPartLength(lp)), XMLTokenHandler::RESULT_ERROR);
			
				break;
			}
		}
	}
	else if (token.GetType()==XMLToken::TYPE_Finished && uxh_father)
		RETURN_VALUE_IF_ERROR(uxh_father->Finished(), XMLTokenHandler::RESULT_ERROR);
  
 return XMLTokenHandler::RESULT_OK;
}

OP_STATUS Simple_XML_Handler::GenerateErrorReport(XMLParser *parser, OpString *xml_err)
{
	OP_ASSERT(parser && xml_err);
	
	if(!parser || !xml_err)
		return OpStatus::ERR_NULL_POINTER;
		
#ifdef XML_ERRORS
	const XMLErrorReport *xer=parser->GetErrorReport();
	    
    // Generate the XML report of the error
    //OP_ASSERT(xer);
    
    if(xer)
    {
		for(unsigned int i=0; i<xer->GetItemsCount(); i++)
		{
			const XMLErrorReport::Item *item=xer->GetItem(i);
			
			OP_ASSERT(item);
			
			if(item)
			{
				RETURN_IF_ERROR(xml_err->Append(item->GetString()));
				RETURN_IF_ERROR(xml_err->Append("\n"));
			}
		}
    }
#else
	RETURN_IF_ERROR(xml_err->Append("XML error"));
#endif
    
    return OpStatus::OK;
}

void Simple_XML_Handler::ParsingStopped(XMLParser *parser)
{
	if(!this)   // I know, it is silly...  :-( but if it prevent the crash, it's ok...
		return;
		
	if(!xml_parser || !parser) // The Handler has already deleted the parser...
		return;
		
	OP_ASSERT(xml_parser==parser);
	
	if(upnp)
		OpStatus::Ignore(upnp->GetDiscoveryListener().RemoveXMLParser(parser));
	
	if(parser->IsFailed())
	{
		OpString8 err;
		OpString8 err2;
		OpString err16;
		const char *error_description=NULL;
		const char *error_uri=NULL;
		const char *error_fragment=NULL;
		
		#ifdef XML_ERRORS
			parser->GetErrorDescription(error_description, error_uri, error_fragment);
		#else
			error_description="XML Error";
		#endif
		
		err.Set("XML Parsing Error: ");
		
		// Generate a description of the context and of the error
		if(GetDescription())
		{
			err.Append(GetDescription());
			err.Append(" phase ");
		}
		else
			err.Append("unknown phase ");
			
		err.Append(" - Action Name: ");
		err2.Set(GetActionName());
		err.Append(err2.CStr());
		
		if(error_fragment)
		{
			err.Append(" - error: ");
			err.Append(error_description);
		}
		if(error_uri)
		{
			err.Append(" - uri: ");
			err.Append(error_uri);
		}
		if(error_fragment)
		{
			err.Append(" - fragment_id: ");
			err.Append(error_fragment);
		}
		
		BOOL do_call=tags_parser.AllValuesRead();
		
		err.Append(" - All variables?: ");
		err.Append((do_call)?"TRUE":"FALSE");
		err.Append(" - Handler called: ");
		err.Append((num_handler_calls)?"TRUE":"FALSE");
		if(tags_parser.GetCountAll()>0)
		{
			err.Append(" - First Param: ");
			err2.Set(tags_parser.getTagName(0)->CStr());
			err.Append(err2.CStr());
		}
		
		OP_NEW_DBG("Simple_XML_Handler::ParsingStopped", "upnp_trace_debug");
	    OP_DBG((UNI_L("*** Partial reconstruction of the XML: %s\n"), tags_parser.xml_read_debug.CStr()));
	    
	    OpString xml_err;
	    
	    OpStatus::Ignore(GenerateErrorReport(parser, &xml_err));
	    OP_DBG((UNI_L("*** XML Error Report: %s\n"), xml_err.CStr()));
	    
	    err16.Set(err);
	    
	    OP_DBG((UNI_L("*** Error parsing the XML: %s\n"), err16.CStr()));
	    
	    if(ErrorRecovered(parser, error_description, error_uri, error_fragment, err16))
	    {
			OP_DBG((UNI_L("*** Error has been recovered...\n")));
			ParsingSuccess(parser);
	    }
	    else if(num_handler_calls>0)
	    {
			OP_DBG((UNI_L("*** 'Late' Error, so skipping it...\n")));
			ParsingSuccess(parser);
	    }
	    else if(trust_http_on_fault)
	    {
			int http_response=parser->GetLastHTTPResponse();
		
			if(http_response==200)
			{
				OP_DBG((UNI_L("*** 'Late' Error, so skipping it...\n")));
				ParsingSuccess(parser);
			}
			else
			{
				ParsingError(parser, http_response, err.CStr());
			}
	    }
	    else
	    {
			ParsingError(parser, 0xFFFFFFFF, err.CStr());
		}
	}
	else if(parser->IsFinished())
	{
		if(num_handler_calls==0 && notify_missing_variables)
			ParsingError(parser, 0xFFFFFFFF, "Missing Variables");
		else if(upnp) // No UPnP no success, also to please Coverity
			ParsingSuccess(parser);
		else
			ParsingError(parser, 0xFFFFFFFF, "Parser Finished but no UPnP object");
	}
	
	//XMLParser *xp=xml_parser;
	
	xml_parser=NULL;
	//OP_DELETE(xp);
} 

XMLTokenHandler::Result UPnPXH_Auto::HandleToken(XMLToken &token)
{
	XMLTokenHandler::Result res=tags_parser.HandleToken(token);
	
	OP_ASSERT(upnp);
	
	if(!upnp)
		return XMLTokenHandler::RESULT_ERROR;
	
	if(res!=XMLTokenHandler::RESULT_OK)
		return res;
		
	BOOL do_call=tags_parser.AllValuesRead();
	
	if(do_call)
	{
		if(OpStatus::IsError(ManageHandlerCalls()))
			return XMLTokenHandler::RESULT_ERROR;
	}
	
	return XMLTokenHandler::RESULT_OK;
}

OP_STATUS UPnPXH_Auto::ManageHandlerCalls()
{
	BOOL do_call=tags_parser.AllValuesRead();
  
    if(do_call || enable_tags_subset)
	{
		num_handler_calls++;
		
		// Update the destination of the variable
		for(UINT32 i=0; i<tags_parser.GetCountAll(); i++)
		{
			OpString *dest=tags_parser.GetDest(i);
        
			if(dest)
			{
				OP_NEW_DBG("UPnPXH_Auto::HandleToken", "upnp_trace");
				
				if(tags_parser.getTagValue(i))
				{
					RETURN_IF_ERROR(dest->Set(tags_parser.getTagValue(i)->CStr()));
					OP_DBG((UNI_L("*** auto Variable %s: %s\n"), (tags_parser.getTagName(i))?tags_parser.getTagName(i)->CStr():UNI_L(""), tags_parser.getTagValue(i)->CStr()));
				}
				else
				{
					dest->Set("");
					OP_DBG((UNI_L("*** auto Variable %s: %s\n"), (tags_parser.getTagName(i))?tags_parser.getTagName(i)->CStr():UNI_L(""), UNI_L("")));
				}
			}
		}
		
		RETURN_IF_ERROR(HandleTokenAuto());
		RETURN_IF_ERROR(tags_parser.Reset());
	}
	
	return OpStatus::OK;
}

void UPnPXH_Auto::ParsingStopped(XMLParser *parser)
{
	if(!this)   // I know, it is silly...  :-( but if it prevent the crash, it's ok...
		return;
		
	// Notify the handler only if all the variables are available, or if it does not care; this is done even in the case of a failure
	BOOL do_call=tags_parser.AllValuesRead();
  
	if(!do_call && enable_tags_subset)
	{
		if(OpStatus::IsError(ManageHandlerCalls()))
			NotifyFailure("UPnPXH_Auto::ManageHandlerCalls()");
	}

#ifdef UPNP_LOG
	if(xml_log_max_size>0 && parser->IsFinished())
	{
		OpString8 temp;
		
		temp.Set(this->tags_parser.GetPartialXMLRead());
		
		OpString8 path;
		BOOL failed=parser->IsFailed();

		parser->GetURL().GetAttribute(URL::KPathAndQuery_L, path);

		upnp->LogInPacket(UNI_L("XML Mex"), path.CStr(), (failed)?UNI_L("KO"):UNI_L("OK"), temp.CStr());
	}
#endif // defined(_DEBUG) && defined(UPNP_LOG)
	
	Simple_XML_Handler::ParsingStopped(parser);
}




OP_STATUS UPnPXH_Auto::NotifyFailure(const char *mex)
{
	if(notify_failure && upnp)
		return upnp->GetDiscoveryListener().HandlerFailed(this, mex); return OpStatus::OK;
}

void UPnPXH_Auto::SetUPnP(UPnP *upnp_father, UPnPDevice *upnp_device, UPnPLogic *upnp_logic)
{
	upnp=upnp_father;
	if(upnp_logic)
		logic_ref.AddReference(upnp_logic->GetReferenceObjectPtr());
	//logic=upnp_logic;
	
	OP_ASSERT(upnp);
	
	if(upnp_device)
		xh_device_ref.AddReference(upnp_device->GetReferenceObjectPtrAuto());
	
	if(upnp && upnp->IsLogging())
		xml_log_max_size=MAX_XML_LOG_SIZE;
}

BOOL UPnPXH_Auto::ErrorRecovered(XMLParser *parser, const char *error_description, const char *error_uri, const char *error_fragment, OpString &errMex)
{
	BOOL do_call=tags_parser.AllValuesRead();
	
	if(do_call || enable_tags_subset)
	{
		if(OpStatus::IsSuccess(ManageHandlerCalls()))
			return TRUE;
	}
	
	return FALSE;	
}
#endif
