/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_) && defined(_SSL_USE_OPENSSL_)

#include "modules/url/url2.h"

#include "modules/libssl/sslbase.h"

#include "modules/olddebug/tstdump.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libssl/external/openssl/certhand1.h"
#include "modules/libopeay/addon/purposes.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/dsa.h"
#include "modules/libopeay/openssl/dh.h"
#include "modules/libopeay/openssl/x509v3.h"
#include "modules/libopeay/openssl/asn1t.h"
#include "modules/libopeay/openssl/pkcs7.h"
#include "modules/libssl/external/openssl/cert_store.h"
#include "modules/about/opgenerateddocument.h"

uint32 BN_bn2Vector(BIGNUM *source, SSL_varvector32 &target);
OP_STATUS ParseObject(ASN1_OBJECT *obj, Str::LocaleString unknown, OpString &target, int &id);
OP_STATUS ConvertString(ASN1_STRING *string, OpString &output);
extern "C" {
int ASN1_UTCTIME_unisprint(uni_char *sp, ASN1_UTCTIME *tm_string);
}

enum SSLeay_Key_Type {No_Keys, RSA_Keys, DSA_Keys, DH_Keys};


static const struct RSA_fields_and_names_st{
	Str::StringList1 name[2];
	BIGNUM *(RSA::*field);
} RSA_fields_and_names[] = {
	{{Str::SI_MSG_SSL_TEXT_Modulus  , Str::SI_MSG_SSL_TEXT_modulus         }, &RSA::n    },
	{{Str::SI_MSG_SSL_TEXT_Exponent , Str::SI_MSG_SSL_TEXT_publicExponent  }, &RSA::e    },
	{{Str::SI_MSG_SSL_TEXT_privateExponent , Str::SI_MSG_SSL_TEXT_privateExponent }, &RSA::d    },
	{{Str::SI_MSG_SSL_TEXT_prime1   , Str::SI_MSG_SSL_TEXT_prime1          }, &RSA::p    },
	{{Str::SI_MSG_SSL_TEXT_prime2   , Str::SI_MSG_SSL_TEXT_prime2          }, &RSA::q    },
	{{Str::SI_MSG_SSL_TEXT_exponent1, Str::SI_MSG_SSL_TEXT_exponent1       }, &RSA::dmp1 },
	{{Str::SI_MSG_SSL_TEXT_exponent2, Str::SI_MSG_SSL_TEXT_exponent2       }, &RSA::dmq1 },
	{{Str::SI_MSG_SSL_TEXT_coefficient, Str::SI_MSG_SSL_TEXT_coefficient   }, &RSA::iqmp }
};

#define RSA_FIELDS ARRAY_SIZE(RSA_fields_and_names)

#ifdef SSL_DSA_SUPPORT
static const struct DSA_fields_and_names_st{
	Str::StringList1 name;
	BIGNUM *(DSA::*field);
} DSA_fields_and_names[] = {
	{ Str::S_MSG_SSL_TEXT_DSA_X, &DSA::priv_key },
	{ Str::S_MSG_SSL_TEXT_DSA_Y, &DSA::pub_key },
	{ Str::S_MSG_SSL_TEXT_DSA_P, &DSA::p },
	{ Str::S_MSG_SSL_TEXT_DSA_Q, &DSA::q },
	{ Str::S_MSG_SSL_TEXT_DSA_G, &DSA::g }
};

#define DSA_FIELDS ARRAY_SIZE(DSA_fields_and_names)
#endif

#ifdef SSL_DH_SUPPORT
static const struct DH_fields_and_names_st{
	Str::StringList1 name;
	BIGNUM *(DH::*field);
} DH_fields_and_names[] = {
	{ Str::S_MSG_SSL_TEXT_DH_PRIV, &DH::priv_key },
	{ Str::S_MSG_SSL_TEXT_DH_PUB,  &DH::pub_key },
	{ Str::S_MSG_SSL_TEXT_DH_P,    &DH::p },
	{ Str::S_MSG_SSL_TEXT_DH_G,    &DH::q }
};

#define DH_FIELDS ARRAY_SIZE(DH_fields_and_names)
#endif


OP_STATUS SSLEAY_CertificateHandler::GetCertificateInfo(uint24 item, URL &target, Str::LocaleString description, SSL_Certinfo_mode info_type) const
{
	if(target.IsEmpty())
	{
		target = g_url_api->GetNewOperaURL();
		if (target.IsEmpty())
			return OpStatus::ERR_NO_MEMORY;
	}

	if(item >= certificatecount)
		return OpStatus::ERR_OUT_OF_RANGE;

	X509 *cert = (item == 0 ? endcertificate : certificate_stack[item].certificate);

	CertInfoWriter writer(target, info_type, cert, description, item, fingerprints_sha1[item], fingerprints_sha256[item] );

	return writer.GenerateData();
}

OP_STATUS SSLEAY_CertificateHandler::CertInfoWriter::WriteLocaleString(const OpStringC &prefix, Str::LocaleString id, const OpStringC &postfix)
{
	OpString tr;
	m_url.WriteDocumentData(URL::KNormal, prefix);
	RETURN_IF_ERROR(SetLangString(id, tr));
	//OP_ASSERT(tr.HasContent()); // Language string missing - check LNG file
	m_url.WriteDocumentData(URL::KNormal, tr.CStr());
	m_url.WriteDocumentData(URL::KNormal, postfix);
	return OpStatus::OK;
}

#define UNI_L_COND(c, s) (c ? UNI_L(s) : UNI_L(""))
#define UNI_L_COND2(c, s1, s2) (c ? UNI_L(s1) : UNI_L(s2))

OP_STATUS SSLEAY_CertificateHandler::CertInfoWriter::GenerateData()
{
	uint16 i,len;
	OpString text, alg_name;
	int nid,count;
	X509_PUBKEY *key;
	X509_EXTENSION *ex;
	STACK_OF(X509_EXTENSION) *extensionstack;
	ASN1_BIT_STRING *bitstring;
	X509_CINF *ci;
	BOOL opened_doc = FALSE;

	if(info_output_type == SSL_Cert_XML_end)
		info_output_type = SSL_Cert_XML;
	else if (info_output_type == SSL_Cert_Text_end)
		info_output_type = SSL_Cert_Text;

	BOOL use_xml = (info_output_type >= SSL_Cert_XML && info_output_type < SSL_Cert_XML_end);
	BOOL produce_subject = (info_output_type == SSL_Cert_XML  || info_output_type == SSL_Cert_XML_Subject  ||
							info_output_type == SSL_Cert_Text  || info_output_type == SSL_Cert_Text_Subject);
	BOOL produce_issuer = (info_output_type == SSL_Cert_XML  || info_output_type == SSL_Cert_XML_Issuer  ||
							info_output_type == SSL_Cert_Text  || info_output_type == SSL_Cert_Text_Issuer);
	BOOL produce_general = (info_output_type == SSL_Cert_XML  || info_output_type == SSL_Cert_XML_General ||
							info_output_type == SSL_Cert_Text  || info_output_type == SSL_Cert_Text_General);

	BOOL produce_headers = (info_output_type == SSL_Cert_XML  || info_output_type == SSL_Cert_Text);


	if(m_url.IsEmpty() || m_cert == NULL)
		return OpStatus::ERR_NULL_POINTER;

	if((URLStatus)m_url.GetAttribute(URL::KLoadStatus) == URL_UNLOADED)
	{
		if(use_xml)
		{
#ifdef _LOCALHOST_SUPPORT_
			RETURN_IF_ERROR(OpenDocument(Str::NOT_A_STRING, PrefsCollectionFiles::StyleCertificateInfoPanelFile));
#else
			RETURN_IF_ERROR(OpenDocument(Str::NOT_A_STRING));
#endif
			RETURN_IF_ERROR(OpenBody());
		}
		else
		{
			// Set up MIME type and required attributes
			RETURN_IF_ERROR(m_url.SetAttribute(URL::KMIME_ForceContentType, "text/plain;charset=utf-16"));

			// Write a BOM
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("\xFEFF")));
			RETURN_IF_ERROR(m_url.SetAttribute(URL::KIsGeneratedByOpera, TRUE));
		}
		opened_doc = TRUE;
		m_url.SetAttribute(URL::KLoadStatus, URL_LOADING);
	}

	//m_url.WriteDocumentData(UNI_L(""));
	//WriteLocaleString(UNI_L(""), Str::, UNI_L(""));

	ci=m_cert->cert_info;

	if(use_xml)
	{
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<certificate>\r\n<table class=\"certificate\">\r\n"));
		if(produce_headers && m_description != Str::NOT_A_STRING )
		{
			WriteLocaleString(UNI_L("<caption>"), m_description, NULL);
			m_url.WriteDocumentDataUniSprintf(UNI_L(" %u</caption>\r\n"), m_item + 1);
		}
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<col/><col/>\r\n"));
	}

	if(produce_subject)
	{
		if(produce_headers)
			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::DI_IDM_CERT_NAME_LABEL, UNI_L_COND2(use_xml, "</th><td><address><subject>","\r\n\r\n"));
		Parse_name(ci->subject, use_xml);
		if(produce_headers)
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</subject></address></td></tr>\r\n", "\r\n\r\n"));
	}

	if(produce_issuer)
	{
		if(produce_headers)
			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::DI_IDM_CERT_ISSUER_LABEL, UNI_L_COND2(use_xml, "</th><td><address><issuer>","\r\n\r\n"));
		Parse_name(ci->issuer, use_xml);
		if(produce_headers)
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</issuer></address></td></tr>\r\n", "\r\n\r\n"));
	}

	if(produce_general)
	{
		WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::SI_MSG_SSL_TEXT_CertVersion, UNI_L_COND2(use_xml, "</th>", ": "));
		m_url.WriteDocumentDataUniSprintf(UNI_L_COND2(use_xml, "<td><version>%lu</version></td></tr>\r\n", "%lu"), ASN1_INTEGER_get(ci->version) +1);
	}
	if(produce_general || info_output_type == SSL_Cert_Text_SerialNumber)
	{
		if(info_output_type != SSL_Cert_Text_SerialNumber)
			WriteLocaleString(UNI_L_COND2(use_xml, "<tr><th>", "\r\n"), Str::SI_MSG_SSL_TEXT_SerialNumber, UNI_L_COND2(use_xml, "</th><td><serialnumber><pre>", ": "));

		unsigned long serial = ASN1_INTEGER_get(ci->serialNumber);
		if(serial != 0xffffffff)
		{
			m_url.WriteDocumentDataUniSprintf(UNI_L_COND2(use_xml, "0x%lx", "0x%lx\r\n" ), serial);
		}
		else if (ci->serialNumber)
		{
			m_url.WriteDocumentData(URL::KNormal, (ci->serialNumber->type & V_ASN1_NEG) != 0 ? UNI_L("-0x") : UNI_L("0x"));

			StringAppendHexOnly(NULL, ci->serialNumber->data, ci->serialNumber->length, 35, NULL);
		}

		if(info_output_type != SSL_Cert_Text_SerialNumber)
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND(use_xml, "</pre></serialnumber></td></tr>"));
	}

	if(produce_general || info_output_type == SSL_Cert_Text_ValidFrom)
	{
		if(info_output_type != SSL_Cert_Text_ValidFrom)
			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::SI_MSG_SSL_TEXT_NotValidBefore, UNI_L_COND2(use_xml, "</th><td><issued>", ": "));
		uni_char * tempdate = (uni_char *) g_memory_manager->GetTempBufUni();
		OP_ASSERT(g_memory_manager->GetTempBufUniLenUniChar() > 50);
		ASN1_UTCTIME_unisprint(tempdate, ci->validity->notBefore);
		m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), tempdate);
		if(info_output_type != SSL_Cert_Text_ValidFrom)
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</issued></td></tr>\r\n", "\r\n"));
	}

	if(produce_general || info_output_type == SSL_Cert_Text_ValidTo)
	{
		if(info_output_type != SSL_Cert_Text_ValidTo)
			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::SI_MSG_SSL_TEXT_NotValidAfter, UNI_L_COND2(use_xml, "</th><td><expires>", ": "));
		uni_char * tempdate = (uni_char *) g_memory_manager->GetTempBufUni();
		OP_ASSERT(g_memory_manager->GetTempBufUniLenUniChar() > 50);
		ASN1_UTCTIME_unisprint(tempdate, ci->validity->notAfter);
		m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), tempdate);

		if(info_output_type != SSL_Cert_Text_ValidTo)
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</expires></td></tr>\r\n", "\r\n"));
	}

	if(produce_general)
	{

		len = m_fingerprint_sha1.GetLength();
		if(len>0)
		{
			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::SI_MSG_SSL_TEXT_FingerPrint, UNI_L_COND2(use_xml, " (SHA-1)</th><td><fingerprint class=\"sha-1\">", " (SHA-1): "));
			StringAppendHexLine(NULL, m_fingerprint_sha1.GetDirect(), 0, len, len);
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</fingerprint></td></tr>\r\n", "\r\n"));
		}

		len = m_fingerprint_sha256.GetLength();
		if(len>0)
		{
			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::SI_MSG_SSL_TEXT_FingerPrint, UNI_L_COND2(use_xml, " (SHA-256)</th><td><fingerprint class=\"sha-256\">", " (SHA-256): "));
			StringAppendHexLine(NULL, m_fingerprint_sha256.GetDirect(), 0, len, len);
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</fingerprint></td></tr>\r\n", "\r\n"));
		}

		{
			int priv1, len;
			const unsigned char *s;
			RSA *rsa=NULL;
#ifdef SSL_DSA_SUPPORT
			DSA *dsa=NULL;
#endif
#ifdef SSL_DH_SUPPORT
			DH *dh=NULL;
#endif
			EVP_PKEY *pkey=NULL;
			BIGNUM *modul=NULL,*priv=NULL;
			SSLeay_Key_Type keytype = No_Keys;

			key = ci->key;

			RETURN_IF_ERROR(ParseObject(key->algor->algorithm, Str::S_LIBSSL_UNKNOWN_ALGORITHM, text,nid));
			bitstring = key->public_key;
			s= bitstring->data;
			len= bitstring->length;
			if ((pkey = d2i_PublicKey(nid,NULL, &s, len)) == NULL)
				return OpStatus::ERR_NO_MEMORY;
			OP_ASSERT(pkey->type == nid);
			count = 0;
			switch (pkey->type)
			{
			case NID_rsaEncryption:
			case NID_rsa:
				rsa = pkey->pkey.rsa;
				if(rsa == NULL)
					break;
				priv = rsa->d;
				modul = rsa->n;
				count = RSA_FIELDS;
				keytype = RSA_Keys;
				break;
#ifdef SSL_DSA_SUPPORT
			case NID_dsa:
			case NID_dsaWithSHA:
			case NID_dsaWithSHA1:
				dsa = pkey->pkey.dsa;
				if(dsa == NULL)
					break;
				priv = dsa->priv_key;
				modul = dsa->g;
				count = DSA_FIELDS;
				keytype = DSA_Keys;
				break;
#endif
#ifdef SSL_DH_SUPPORT
			case NID_dhKeyAgreement:
				dh = pkey->pkey.dh;
				if(dh == NULL)
					break;
				priv = dh->priv_key;
				modul = dh->p;
				count = DH_FIELDS;
				keytype = DH_Keys;
				break;
#endif
			default:
				count = 0;
			}

			if(count>1)
			{
				WriteLocaleString(UNI_L_COND(use_xml, "<tr><th><keytype>"), (priv != NULL ? Str::S_SSL_PRIVATE_KEY : Str::S_SSL_PUBLIC_KEY), UNI_L_COND2(use_xml, "</keytype>", ": "));
				if(modul)
				{
					m_url.WriteDocumentDataUniSprintf((use_xml ? UNI_L("(<keysize>%d</keysize> ") : UNI_L("(%d ")), BN_num_bits(modul));
					WriteLocaleString(NULL , Str::S_BITS , UNI_L(")"));
				}
				WriteLocaleString(UNI_L_COND2(use_xml, "</th><td><keyconfiguration><table>\r\n<tr><th>", "\r\n           "), Str::SI_MSG_SSL_TEXT_PubKeyAlg, UNI_L_COND2(use_xml, "</th><td><keyalg>" , ": "));
				m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), text);
				m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</keyalg></td></tr>", "\r\n"));

				priv1 = (priv == NULL ? 0 : 1);
				for(i=0;i<count;i++)
				{
					Str::LocaleString key_name = Str::NOT_A_STRING;
					BIGNUM *key_field = NULL;

					switch (keytype)
					{
					case RSA_Keys:
						key_name = RSA_fields_and_names[i].name[priv1];
						key_field = rsa->*RSA_fields_and_names[i].field;
						break;
#ifdef SSL_DSA_SUPPORT
					case DSA_Keys:
						key_name = DSA_fields_and_names[i].name;
						key_field = dsa->*DSA_fields_and_names[i].field;
						break;
#endif
#ifdef SSL_DH_SUPPORT
					case DH_Keys:
						key_name = DH_fields_and_names[i].name;
						key_field = dh->*DH_fields_and_names[i].field;
						break;
#endif
					}
					if(key_field && key_name != Str::NOT_A_STRING)
					{
						WriteLocaleString(UNI_L_COND2(use_xml, "<tr><th><keyname>", "           "), key_name, UNI_L_COND2(use_xml, "</keyname></th><td><keydata><pre>","\r\n"));
						if(BN_num_bytes(key_field) > 0)
						{
							SSL_varvector32 keydata;
							BN_bn2Vector(key_field, keydata);
							StringAppendHexOnly((use_xml ? UNI_L("<!--    -->") : UNI_L("                 ")), keydata.GetDirect(), keydata.GetLength(), 16, NULL);
						}
						m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</pre></keydata></td></tr>\r\n","\r\n"));
					}
				}
			}
			EVP_PKEY_free(pkey);

			if(use_xml)
				m_url.WriteDocumentData(URL::KNormal, UNI_L("</table></keyconfiguration></td></tr>\r\n"));
		}

		WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::S_MSG_SSL_TEXT_SIGNATURE, UNI_L_COND2(use_xml, "</th><td>\r\n<signatureconfiguration><table>\r\n", "\r\n"));
		WriteLocaleString(UNI_L_COND2(use_xml, "<tr><th>","           "), Str::SI_MSG_SSL_TEXT_SigAlg, UNI_L_COND2(use_xml, "</th><td><signaturealgorithm>", ": "));
		ParseObject(m_cert->sig_alg->algorithm, Str::SI_MSG_SSL_TEXT_Unknown_SignatureID,text, nid);
		m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), text);
		WriteLocaleString(UNI_L_COND2(use_xml, "</signaturealgorithm></td></tr>\r\n<tr><th>", "\r\n           "), Str::S_MSG_SSL_TEXT_SIGNATURE, UNI_L_COND2(use_xml, "</th><td><signaturedata><pre>", "\r\n"));
		StringAppendHexOnly((use_xml ? UNI_L("<!--    -->") : UNI_L("                ")),m_cert->signature->data, m_cert->signature->length ,16,NULL);
		m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</pre></signaturedata></td></tr>\r\n</table></signatureconfiguration></td></tr>", "\r\n"));

		extensionstack = ci->extensions;
		count = (extensionstack != NULL ? X509v3_get_ext_count(extensionstack) : 0);
		if(count) do
		{
			ASN1_OBJECT *obj;
			char *data;
			long len;

			WriteLocaleString(UNI_L_COND(use_xml, "<tr><th>"), Str::SI_MSG_SSL_TEXT_Extension, UNI_L_COND2(use_xml, "</th><td>\r\n<extensions><table>\r\n", "\r\n"));

			BIO *bio = BIO_new(BIO_s_mem());
			if(bio == NULL)
				break;

			for(i=0;i<count;i++)
			{
				ex=X509v3_get_ext(extensionstack,i);

				obj=X509_EXTENSION_get_object(ex);
				m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "<tr><th><extensionname>", "  "));
				RETURN_IF_ERROR(ParseObject(obj, Str::SI_MSG_SSL_TEXT_Unknown_ExtensionID,text,nid));
				m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), text);
				if(X509_EXTENSION_get_critical(ex))
					WriteLocaleString(UNI_L_COND(use_xml, "<extensioncritical>"), Str::SI_MSG_SSL_TEXT_Critical, UNI_L_COND(use_xml, "</extensioncritical>"));
				m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</extensionname></th><td><extensiondata>", ": "));

				BIO_ctrl(bio, BIO_CTRL_RESET, 0, NULL);
				X509V3_EXT_print(bio, ex, 0, 2);

				len = BIO_get_mem_data(bio, &data);

				if(data)
				{
					text.Set(data,len);
					text.Strip();
					m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), text);
				}
				else if(ex->value)
				{
					if(use_xml)
						m_url.WriteDocumentData(URL::KNormal, UNI_L("<pre>\r\n"));
					StringAppendHexAndPrintable((use_xml ? UNI_L("<!--    -->") : UNI_L("                ")),ex->value->data,ex->value->length,
						(ex->value->length < 12? ex->value->length : 12),UNI_L("\r\n"));
					if(use_xml)
						m_url.WriteDocumentData(URL::KNormal, UNI_L("\r\n</pre>"));
				}
				m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</extensiondata></td></tr>\r\n", "\r\n"));

			}
			BIO_free(bio);

			if(use_xml)
				m_url.WriteDocumentData(URL::KNormal, UNI_L("\r\n</table></extensions></td></tr>\r\n"));
		} while(0);

		if(use_xml)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\r\n</certificate>\r\n"));
	}
	if(opened_doc)
	{
		if(use_xml)
			RETURN_IF_ERROR(CloseDocument());
		m_url.SetAttribute(URL::KLoadStatus, URL_LOADED);
	}

	return OpStatus::OK;
}

void SSLEAY_CertificateHandler::CertInfoWriter::StringAppendHexLine(const OpStringC &head, const byte *source,uint32 offset, uint32 maxlen, uint16 linelen)
{
	uint32 j;
	unsigned temp1 = 0;
	uint32 len = offset + maxlen;

	m_url.WriteDocumentData(URL::KNormal, head);

	if(offset >0 || len > linelen)
	{
		int prec = 2;
		if(len > 0xff)
		{
			prec+= prec;
			if(len > 0xffff)
				prec += prec;
		}
 		m_url.WriteDocumentDataUniSprintf(UNI_L("%.*lX:"),prec,(unsigned long) offset);
   }


	for(j=0;j< linelen ;j++)
	{
		if(j < maxlen)
			temp1 =(unsigned) *(source++);
		m_url.WriteDocumentDataUniSprintf((j < maxlen ? (j>0 || offset >0 || len>linelen ? UNI_L(" %.2X") : UNI_L("%.2X")) : UNI_L("   ")),temp1);
	}
}

void SSLEAY_CertificateHandler::CertInfoWriter::StringAppendHexOnly(const OpStringC &head, const byte *source, uint32 len, uint16 linelen,const OpStringC &tail)
{
	uint32 i;

	if(len == 0)
		return;

	for(i=0;i<len;i+=linelen)
	{
		StringAppendHexLine(head,source,i,len-i,linelen);
		source += linelen;

		if(tail.HasContent())
		{
			m_url.WriteDocumentData(URL::KNormal, UNI_L(" "));
			m_url.WriteDocumentData(URL::KNormal, tail);
		}
		m_url.WriteDocumentData(URL::KNormal, UNI_L("\r\n"));
	}
}

// Generate a Hex and character listing

void SSLEAY_CertificateHandler::CertInfoWriter::StringAppendHexAndPrintable(const OpStringC &head, const byte *source, uint32 len, uint16 linelen,const OpStringC &tail)
{
	uint32 i,j,k;

	uni_char temp[32];  /* ARRAY OK 2009-04-19 yngve */

	if(len == 0)
		return;

	for(i=0;i<len;i+=linelen)
	{
		StringAppendHexLine(head,source,i,len-i,linelen);

		for(k=0,j=0;j<linelen ;j++)
		{
			temp[k++] = (i+j<len ? (op_isprint(source[j]) ? source[j] : '.') : ' ');
			if(k == 32)
			{
				m_url.WriteDocumentData(URL::KHTMLify, temp,k);
				k = 0;
			}
		}
		if(k)
			m_url.WriteDocumentData(URL::KHTMLify, temp,k);
		source += linelen;

		if(tail.HasContent())
		{
			m_url.WriteDocumentData(URL::KNormal, UNI_L(" "));
			m_url.WriteDocumentData(URL::KNormal, tail);
		}
		m_url.WriteDocumentData(URL::KNormal, UNI_L("\r\n"));
	}
}

static const int	name_profile_nids[] = {
	NID_commonName,
	NID_organizationName,
	NID_organizationalUnitName,
	NID_localityName,
	NID_stateOrProvinceName,
	NID_countryName,
};

OP_STATUS SSLEAY_CertificateHandler::CertInfoWriter::Parse_name(X509_NAME *name, BOOL use_xml)
{
	X509_NAME_ENTRY *ne;
	int n, fields, item, nid;

	if (name == NULL)
		return OpStatus::ERR_NULL_POINTER;

	size_t i;
	for (i = 0; i < ARRAY_SIZE(name_profile_nids); i++)
	{
		BOOL first = TRUE;
		const uni_char *tag_start = NULL, *tag_end= NULL;
		nid = name_profile_nids[i];
		item = X509_NAME_get_index_by_NID(name, nid, -1);
		if(item != -1)
		{
			if(use_xml)
			{
				switch(nid)
				{
				case NID_commonName:
					tag_start = UNI_L("<cn>");
					tag_end = UNI_L("</cn>");
					break;
				case NID_organizationName:
					tag_start = UNI_L("<o>");
					tag_end = UNI_L("</o>");
					break;
				case NID_organizationalUnitName:
					tag_start = UNI_L("<ou>");
					tag_end = UNI_L("</ou>");
					break;
				case NID_localityName:
					tag_start = UNI_L("<lo>");
					tag_end = UNI_L("</lo>");
					break;
				case NID_stateOrProvinceName:
					tag_start = UNI_L("<pr>");
					tag_end = UNI_L("</pr>");
					break;
				case NID_countryName:
					tag_start = UNI_L("<co>");
					tag_end = UNI_L("</co>");
					break;
				default:
					OP_ASSERT(0); // Update switch statements;
				}
			}
			m_url.WriteDocumentData(URL::KNormal, tag_start);
			while(item != -1)
			{
				ne = X509_NAME_get_entry(name, item);
				if(ne != NULL)
				{
					if(!first)
						m_url.WriteDocumentData(URL::KNormal, UNI_L(", "));

					RETURN_IF_ERROR(ConvertString(ne->value, use_xml));
					first = FALSE;
				}

				item = X509_NAME_get_index_by_NID(name, nid, item);
			}
			m_url.WriteDocumentData(URL::KNormal, tag_end);
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "<br/>\r\n", "\r\n"));
		}
	}

	OpString tempstring;

	fields = X509_NAME_entry_count(name);

	int i1;
	for (i1=0; i1<fields; i1++)
	{
		ne=X509_NAME_get_entry(name,i1);
		RETURN_IF_ERROR(ParseObject(ne->object, Str::S_LIBSSL_UNKOWN_NAME_FIELD, tempstring, n));

		switch(n)
		{
		case NID_commonName :
		case NID_organizationName :
		case NID_organizationalUnitName :
		case NID_localityName :
		case NID_stateOrProvinceName :
		case NID_countryName :
			break; // already handled
		default :
			if(use_xml)
				m_url.WriteDocumentData(URL::KNormal, UNI_L("<namex><namex_id>"));
			m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), tempstring);
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, ":</namex_id> ", ": "));
			RETURN_IF_ERROR(ConvertString(ne->value, use_xml));
			m_url.WriteDocumentData(URL::KNormal, UNI_L_COND2(use_xml, "</namex><br/>\r\n", "\r\n"));
			break;
		}
	}

	return OpStatus::OK;
}

OP_STATUS SSLEAY_CertificateHandler::CertInfoWriter::ConvertString(ASN1_STRING *string, BOOL use_xml)
{
	OpString buffer;

	RETURN_IF_ERROR(::ConvertString(string, buffer));

	m_url.WriteDocumentData((use_xml ? URL::KHTMLify : URL::KNormal), buffer);

	return OpStatus::OK;
}

#endif // relevant support
