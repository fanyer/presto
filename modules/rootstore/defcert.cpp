/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)

#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"

#include "modules/url/tools/arrays.h"

#include "modules/rootstore/rootstore_api.h"
#include "modules/rootstore/rootstore_table.h"

#if !defined LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/rootstore/root_aux.h"
#endif

#include "modules/olddebug/tstdump.h"

OP_STATUS ParseCommonName(const OpString_list &info, OpString &title);

OP_STATUS RootStore_API::InitAuthorityCerts(SSL_Options *optionsManager,uint32 ver)
{
	LOCAL_DECLARE_CONST_ARRAY(DEFCERT_st, defcerts);

    SSL_CertificateItem *current_cert;
    const DEFCERT_st *defcertitem;
    OpString_list info;
    SSL_DistinguishedName certname;
    SSL_ASN1Cert certificate;
    SSL_CertificateHandler *handler;
    
    if(optionsManager== NULL)
		return OpStatus::ERR_NULL_POINTER;
	
	LOCAL_CONST_ARRAY_INIT(defcerts, OpStatus::ERR_NO_MEMORY);

    handler = g_ssl_api->CreateCertificateHandler();
    if(handler == NULL)
		return OpStatus::ERR_NO_MEMORY;

#ifdef _DEBUG
	uint32 highest_ver = 0;
#endif
	
    for( defcertitem = defcerts; defcertitem->dercert_data != NULL; defcertitem++)
    {
#if !defined LIBSSL_AUTO_UPDATE_ROOTS
#ifdef _DEBUG
		if (highest_ver < defcertitem->version)
			highest_ver = defcertitem->version;
#endif

		if(ver && ver != SSL_Options_Version_NewBase && ver >= defcertitem->version)
			continue;
#endif

        certificate.Set(defcertitem->dercert_data,defcertitem->dercert_len);
        if(certificate.ErrorRaisedFlag)
		{
#if defined _DEBUG && defined YNP_WORK
			OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
			continue;
        }
        
		handler->ResetError();
        handler->LoadCertificate(certificate);
        if(handler->ErrorRaisedFlag)
		{
#if defined _DEBUG && defined YNP_WORK
			OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
			continue;
        }
		
        handler->GetSubjectName(0, certname);
        BOOL bWarnIfUsed = defcertitem->def_warn;
        BOOL bDenyIfUsed = defcertitem->def_deny;
		
        current_cert = optionsManager->FindTrustedCA(certname);
        if(current_cert != NULL)
		{
            if(current_cert->certificate !=  certificate || (defcertitem->def_replace && defcertitem->def_replace_flags) )
			{
				if(!defcertitem->def_replace)
				{
#if defined _DEBUG && defined YNP_WORK
					OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
                    // Not the same cert and not a replacement. Examine (not yet)
                    continue;
                }
				if(ver && ver >= defcertitem->version)
					continue; // Has been replaced in previous update, do not replace; To avoid having local and remote repository beating each other do death
				
				SSL_CertificateHandler *handler2;
				SSL_CertificateVerification_Info *info;
				uint24 count;
				
				handler2 = g_ssl_api->CreateCertificateHandler();
				if(handler2 == NULL)
					continue;
				
				handler2->LoadCertificate(current_cert->certificate);
				if(handler2->ErrorRaisedFlag)
				{
					OP_DELETE(handler2);
					continue;
				}

				/** disabled check. May replace intermediates
				if(!handler->SelfSigned(0) || !handler2->SelfSigned(0))
				{
					// Names match but either or both are not selfsigned
					OP_DELETE(handler2);
					continue;
				}
				*/

				SSL_varvector16 hash_1;
				SSL_varvector16 hash_2;
				handler->GetPublicKeyHash(0, hash_1);
				handler2->GetPublicKeyHash(0, hash_2);
				if(handler->ErrorRaisedFlag || handler2->ErrorRaisedFlag ||
					hash_1.Error() || hash_2.Error() ||
					hash_1 != hash_2)
				{
					OP_DELETE(handler2);
					continue;
				}
				if(!handler->VerifySignatures(SSL_Purpose_Any, NULL,
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
					NULL,
#endif
					optionsManager, TRUE))
				{
#if defined _DEBUG && defined YNP_WORK
					OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
					OP_DELETE(handler2);
					continue;
				}
				count = 0;
				info = handler->ExtractVerificationStatus(count);
				if(/*count != 1 ||  */info == NULL || 
					(info[0].status & SSL_CERT_INVALID) != 0 ||
					((info[0].status & SSL_CERT_NO_ISSUER) != 0))
				{
#if defined _DEBUG && defined YNP_WORK
					OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
					OP_DELETEA(info);
					OP_DELETE(handler2);
					continue;
				}
				OP_DELETEA(info);
				info = NULL;
				
				if(!defcertitem->def_replace_flags)
				{
					bWarnIfUsed = current_cert->WarnIfUsed;
					bDenyIfUsed = current_cert->DenyIfUsed;
				}
				
				OP_DELETE(handler2);
				current_cert->Out();
				OP_DELETE(current_cert);
			}
			else
			{
				current_cert->PreShipped = TRUE;
				continue;  
			}
        }
		else
		{
			// verify certificate before insertion
			if(!handler->VerifySignatures(SSL_Purpose_Any
#ifndef SSL_OLD_VERIFICATION_CHECK
					, NULL,
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
					NULL,
#endif
					optionsManager, TRUE
#endif
				))
			{
#if defined _DEBUG && defined YNP_WORK
				OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
				continue;
			}
#if defined _DEBUG && defined YNP_WORK
			SSL_Alert exp_error;
			if(handler->Error(&exp_error))
			{
				OP_ASSERT(exp_error.GetDescription() != SSL_Certificate_Expired); // Should this certificate still be included?
			}
#endif

			SSL_CertificateVerification_Info *info;
			uint24 count;

			count = 0;
			info = handler->ExtractVerificationStatus(count);
			if(count == 0 || 
				(count != 1 && handler->SelfSigned(0)) ||
				info == NULL || 
				(info[0].status & SSL_CERT_INVALID) != 0 ||
				(info[count-1].status & SSL_CERT_INVALID) != 0
#ifndef SSL_OLD_VERIFICATION_CHECK
				|| ((info[count-1].status & SSL_CERT_NO_ISSUER) != 0) 
				|| ((info[0].status & SSL_CERT_NO_ISSUER) != 0)
#endif
				)
			{
#if defined _DEBUG && defined YNP_WORK
				OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
				OP_DELETEA(info);
				continue;
			}
			OP_DELETEA(info);
			info = NULL;
		}
        
        current_cert = OP_NEW(SSL_CertificateItem, ()); 
		
        if(current_cert == NULL)
		{
            break;  // This is OOM, should we return an indication? 
        }
        
        current_cert->certificate =  certificate;
        //current_cert->certificate.Set(defcertitem->dercert_data,defcertitem->dercert_len);
        if(current_cert->certificate.ErrorRaisedFlag)
		{
#if defined _DEBUG && defined YNP_WORK
			OP_ASSERT(FALSE); // Should this certificate still be included?
#endif
			OP_DELETE(current_cert);
			continue;
        }
        current_cert->certificatetype = handler->CertificateType(0);
        current_cert->WarnIfUsed = bWarnIfUsed;
        current_cert->DenyIfUsed = bDenyIfUsed;
        current_cert->PreShipped = TRUE;

#if defined _DEBUG && defined YNP_WORK
		{
			OP_STATUS sret;
			sret = handler->GetSubjectName(0,info); 
			
			if (OpStatus::IsSuccess(sret))
			{
				int i,n = info.Count();

				for(i=0; i< n; i++)
				{
					if(info.Item(i).HasContent())
					{
						OpString8 text;

						text.SetUTF8FromUTF16(info.Item(i));

						if(text.HasContent())
							PrintfTofile("rootstorelist.txt", "%s\n", text.CStr());
					}
				}
				if(defcertitem->nickname)
					PrintfTofile("rootstorelist.txt", "Friendly Name: %s\n", defcertitem->nickname);
				PrintfTofile("rootstorelist.txt", "Key Length: %d\n", handler->KeyBitsLength(0));
				PrintfTofile("rootstorelist.txt", "\n");
			}
		}
#endif

        //handler->GetSubjectName(0,current_cert->name);
        current_cert->name = certname;
		if(!defcertitem->nickname || !*defcertitem->nickname)
		{
			OP_STATUS sret;
			sret = handler->GetSubjectName(0,info); 
			if (OpStatus::IsError(sret))
				break;  // Uncertain about this, alternatively return OP_STATUS
			else if (OpStatus::IsError(ParseCommonName(info, current_cert->cert_title)))
					break;
		}
		else
		{
			OP_STATUS sret;
			sret = current_cert->cert_title.Set(defcertitem->nickname);
			if (OpStatus::IsError(sret))
				break;  // Uncertain about this, alternatively return OP_STATUS
		}
        
		optionsManager->AddCertificate(handler->SelfSigned(0) ? SSL_CA_Store : SSL_IntermediateCAStore, current_cert);
        
    }
    OP_DELETE(handler);  

#if defined _DEBUG && defined YNP_WORK
	CloseDebugFile("rootstorelist.txt");
#endif
#ifdef _DEBUG
	OP_ASSERT(highest_ver <= SSL_Options_Version);
#endif

#if !defined LIBSSL_AUTO_UPDATE_ROOTS
	RETURN_IF_ERROR(optionsManager->Init(SSL_UntrustedSites));

	LOCAL_DECLARE_CONST_ARRAY(DEFCERT_UNTRUSTED_cert_st, defcerts_untrusted_repository_list);

    const DEFCERT_UNTRUSTED_cert_st *untrusted_item;
	
	LOCAL_CONST_ARRAY_INIT(defcerts_untrusted_repository_list, OpStatus::ERR_NO_MEMORY);

	int n = LOCAL_CONST_ARRAY_SIZE(defcerts_untrusted_repository_list);
	int i;

	for(i = 0;i<n; i++)
	{
		untrusted_item = &defcerts_untrusted_repository_list[i];

		SSL_ASN1Cert untrusted_cert;
		untrusted_cert.Set(untrusted_item->dercert_data, untrusted_item->dercert_len);

		if(!optionsManager->FindUnTrustedCert(untrusted_cert))
			optionsManager->AddUnTrustedCert(untrusted_cert);
	}


#endif

	return OpStatus::OK;
}

#endif
