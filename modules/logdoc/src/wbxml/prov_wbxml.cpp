/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
**
*/

/* This is the implementation of WAP Provisioning binary XML decoder, as
 * specified in OMA-WAP-ProvCont-v1_1-20021112-C
*/


CONST_ARRAY(Provisioning_WBXML_tag_tokens, uni_char*)
	CONST_ENTRY(UNI_L("wap-provisioningdoc")), // 0x05
	CONST_ENTRY(UNI_L("characteristic")),
	CONST_ENTRY(UNI_L("parm"))
CONST_END(Provisioning_WBXML_tag_tokens)

// code page 0
CONST_ARRAY(Provisioning_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("name=\"")), // 0x05
	CONST_ENTRY(UNI_L("value=\"")),
	CONST_ENTRY(UNI_L("name=\"NAME")),
	CONST_ENTRY(UNI_L("name=\"NAP-ADDRESS")), // 0x08
	CONST_ENTRY(UNI_L("name=\"NAP-ADDRTYPE")),
	CONST_ENTRY(UNI_L("name=\"CALLTYPE")),
	CONST_ENTRY(UNI_L("name=\"VALIDUNTIL")),
	CONST_ENTRY(UNI_L("name=\"AUTHTYPE")),
	CONST_ENTRY(UNI_L("name=\"AUTHNAME")),
	CONST_ENTRY(UNI_L("name=\"AUTHSECRET")),
	CONST_ENTRY(UNI_L("name=\"LINGER")),
	CONST_ENTRY(UNI_L("name=\"BEARER")), // 0x10
	CONST_ENTRY(UNI_L("name=\"NAPID")),
	CONST_ENTRY(UNI_L("name=\"COUNTRY")),
	CONST_ENTRY(UNI_L("name=\"NETWORK")),
	CONST_ENTRY(UNI_L("name=\"INTERNET")),
	CONST_ENTRY(UNI_L("name=\"PROXY-ID")),
	CONST_ENTRY(UNI_L("name=\"PROXY-PROVIDER-ID")),
	CONST_ENTRY(UNI_L("name=\"DOMAIN")),
	CONST_ENTRY(UNI_L("name=\"PROVURL")), // 0x18
	CONST_ENTRY(UNI_L("name=\"PXAUTH-TYPE")),
	CONST_ENTRY(UNI_L("name=\"PXAUTH-ID")),
	CONST_ENTRY(UNI_L("name=\"PXAUTH-PW")),
	CONST_ENTRY(UNI_L("name=\"STARTPAGE")),
	CONST_ENTRY(UNI_L("name=\"BASAUTH-ID")),
	CONST_ENTRY(UNI_L("name=\"BASAUTH-PW")),
	CONST_ENTRY(UNI_L("name=\"PUSHENABLED")),
	CONST_ENTRY(UNI_L("name=\"PXADDR")), // 0x20
	CONST_ENTRY(UNI_L("name=\"PXADDRTYPE")),
	CONST_ENTRY(UNI_L("name=\"TO-NAPID")),
	CONST_ENTRY(UNI_L("name=\"PORTNBR")),
	CONST_ENTRY(UNI_L("name=\"SERVICE")),
	CONST_ENTRY(UNI_L("name=\"LINKSPEED")),
	CONST_ENTRY(UNI_L("name=\"DNLINKSPEED")),
	CONST_ENTRY(UNI_L("name=\"LOCAL-ADDR")),
	CONST_ENTRY(UNI_L("name=\"LOCAL-ADDRTYPE")), // 0x28
	CONST_ENTRY(UNI_L("name=\"CONTEXT-ALLOW")),
	CONST_ENTRY(UNI_L("name=\"TRUST")),
	CONST_ENTRY(UNI_L("name=\"MASTER")),
	CONST_ENTRY(UNI_L("name=\"SID")),
	CONST_ENTRY(UNI_L("name=\"SOC")),
	CONST_ENTRY(UNI_L("name=\"WSP-VERSION")),
	CONST_ENTRY(UNI_L("name=\"PHYSICAL-PROXY-ID")),
	CONST_ENTRY(UNI_L("name=\"CLIENT-ID")), // 0x30
	CONST_ENTRY(UNI_L("name=\"DELIVERY-ERR-SDU")),
	CONST_ENTRY(UNI_L("name=\"DELIVERY-ORDER")),
	CONST_ENTRY(UNI_L("name=\"TRAFFIC-CLASS")),
	CONST_ENTRY(UNI_L("name=\"MAX-SDU-SIZE")),
	CONST_ENTRY(UNI_L("name=\"MAX-BITRATE-UPLINK")),
	CONST_ENTRY(UNI_L("name=\"MAX-BITRATE-DNLINK")),
	CONST_ENTRY(UNI_L("name=\"RESIDUAL-BER")),
	CONST_ENTRY(UNI_L("name=\"SDU-ERROR-RATIO")), // 0x38
	CONST_ENTRY(UNI_L("name=\"TRAFFIC-HANDL-PRIO")),
	CONST_ENTRY(UNI_L("name=\"TRANSFER-DELAY")),
	CONST_ENTRY(UNI_L("name=\"GUARANTEED-BITRATE-UPLINK")),
	CONST_ENTRY(UNI_L("name=\"GUARANTEED-BITRATE-DNLINK")),
	CONST_ENTRY(UNI_L("name=\"PXADDR-FQDN")),
	CONST_ENTRY(UNI_L("name=\"PROXY-PW")),
	CONST_ENTRY(UNI_L("name=\"PPGAUTH-TYPE")),
	CONST_ENTRY(0), // 0x40
	CONST_ENTRY(0), // 0x41
	CONST_ENTRY(0), // 0x42
	CONST_ENTRY(0), // 0x43
	CONST_ENTRY(0), // 0x44
	CONST_ENTRY(UNI_L("version=\"")), // 0x45
	CONST_ENTRY(UNI_L("version=\"1.0")),
	CONST_ENTRY(UNI_L("name=\"PULLENABLED")),
	CONST_ENTRY(UNI_L("name=\"DNS-ADDR")),
	CONST_ENTRY(UNI_L("name=\"MAX-NUM-RETRY")),
	CONST_ENTRY(UNI_L("name=\"FIRST-RETRY-TIMEOUT")),
	CONST_ENTRY(UNI_L("name=\"REREG-THRESHOLD")),
	CONST_ENTRY(UNI_L("name=\"T-BIT")),
	CONST_ENTRY(0), // 0x4d
	CONST_ENTRY(UNI_L("name=\"AUTH-ENTITY")),
	CONST_ENTRY(UNI_L("name=\"SPI")),
	CONST_ENTRY(UNI_L("type=\"")), // 0x50
	CONST_ENTRY(UNI_L("type=\"PXLOGICAL")),
	CONST_ENTRY(UNI_L("type=\"PXPHYSICAL")),
	CONST_ENTRY(UNI_L("type=\"PORT")),
    CONST_ENTRY(UNI_L("type=\"VALIDITY")),
	CONST_ENTRY(UNI_L("type=\"NAPDEF")),
	CONST_ENTRY(UNI_L("type=\"BOOTSTRAP")),
	CONST_ENTRY(UNI_L("type=\"VENDORCONFIG")),
	CONST_ENTRY(UNI_L("type=\"CLIENTIDENTITY")),
	CONST_ENTRY(UNI_L("type=\"PXAUTHINFO")),
	CONST_ENTRY(UNI_L("type=\"NAPAUTHINFO")),
	CONST_ENTRY(UNI_L("type=\"ACCESS"))
CONST_END(Provisioning_WBXML_attr_start_tokens)

// codes shared between code page 0 and 1
const UINT8 Provisioning_WBXML_attr_start_tokens_shared[] = {
	0x05, 0x06, 0x07, 0x14, 0x1c, 0x22, 0x23, 0x24, 0x50, 0x53, 0x58 };

// code page 1
CONST_ARRAY(Provisioning_WBXML_attr_start_tokens_cp1, uni_char*)
	CONST_ENTRY(UNI_L("name=\"AACCEPT")), // 0x2e
	CONST_ENTRY(UNI_L("name=\"AAUTHDATA")),
	CONST_ENTRY(UNI_L("name=\"AAUTHLEVEL")), // 0x30
	CONST_ENTRY(UNI_L("name=\"AAUTHNAME")),
	CONST_ENTRY(UNI_L("name=\"AAUTHSECRET")),
	CONST_ENTRY(UNI_L("name=\"AAUTHTYPE")),
	CONST_ENTRY(UNI_L("name=\"ADDR")),
	CONST_ENTRY(UNI_L("name=\"ADDRTYPE")),
	CONST_ENTRY(UNI_L("name=\"APPID")),
	CONST_ENTRY(UNI_L("name=\"APROTOCOL")),
	CONST_ENTRY(UNI_L("name=\"PROVIDER-ID")), // 0x38
	CONST_ENTRY(UNI_L("name=\"TO-PROXY")),
	CONST_ENTRY(UNI_L("name=\"URI")),
	CONST_ENTRY(UNI_L("name=\"RULE")),
	CONST_ENTRY(UNI_L(0)), // 0x3c
	CONST_ENTRY(UNI_L(0)), // 0x3d
	CONST_ENTRY(UNI_L(0)), // 0x3e
	CONST_ENTRY(UNI_L(0)), // 0x3f
	CONST_ENTRY(UNI_L(0)), // 0x40
	CONST_ENTRY(UNI_L(0)), // 0x41
	CONST_ENTRY(UNI_L(0)), // 0x42
	CONST_ENTRY(UNI_L(0)), // 0x43
	CONST_ENTRY(UNI_L(0)), // 0x44
	CONST_ENTRY(UNI_L(0)), // 0x45
	CONST_ENTRY(UNI_L(0)), // 0x46
	CONST_ENTRY(UNI_L(0)), // 0x47
	CONST_ENTRY(UNI_L(0)), // 0x48
	CONST_ENTRY(UNI_L(0)), // 0x49
	CONST_ENTRY(UNI_L(0)), // 0x4a
	CONST_ENTRY(UNI_L(0)), // 0x4b
	CONST_ENTRY(UNI_L(0)), // 0x4c
	CONST_ENTRY(UNI_L(0)), // 0x4d
	CONST_ENTRY(UNI_L(0)), // 0x4e
	CONST_ENTRY(UNI_L(0)), // 0x4f
	CONST_ENTRY(UNI_L(0)), // 0x50 // take from code page 0
	CONST_ENTRY(UNI_L(0)), // 0x51
	CONST_ENTRY(UNI_L(0)), // 0x52
	CONST_ENTRY(UNI_L(0)), // 0x53 // take from code page 0
	CONST_ENTRY(UNI_L(0)), // 0x54
	CONST_ENTRY(UNI_L("type=\"APPLICATION")),
	CONST_ENTRY(UNI_L("type=\"APPADDR")),
	CONST_ENTRY(UNI_L("type=\"APPAUTH")),
	CONST_ENTRY(UNI_L(0)), // 0x58 // take from code page 0
	CONST_ENTRY(UNI_L("type=\"RESOURCE"))
CONST_END(Provisioning_WBXML_attr_start_tokens_cp1)

// code page 0
CONST_ARRAY(Provisioning_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L("IPV4")), // 0x85
	CONST_ENTRY(UNI_L("IPV6")),
	CONST_ENTRY(UNI_L("E164")),
	CONST_ENTRY(UNI_L("ALPHA")),
	CONST_ENTRY(UNI_L("APN")),
	CONST_ENTRY(UNI_L("SCODE")),
	CONST_ENTRY(UNI_L("TETRA-ITSI")),
	CONST_ENTRY(UNI_L("MAN")),
	CONST_ENTRY(UNI_L(0)), // 0x8d
	CONST_ENTRY(UNI_L(0)), // 0x8e
	CONST_ENTRY(UNI_L(0)), // 0x8f
	CONST_ENTRY(UNI_L("ANALOG-MODEM")),
	CONST_ENTRY(UNI_L("V.120")),
	CONST_ENTRY(UNI_L("V.110")),
	CONST_ENTRY(UNI_L("X.31")),
	CONST_ENTRY(UNI_L("BIT-TRANSPARENT")),
	CONST_ENTRY(UNI_L("DIRECT-ASYNCHRONOUS-DATA-SERVICE")),
	CONST_ENTRY(UNI_L(0)), // 0x96
	CONST_ENTRY(UNI_L(0)), // 0x97
	CONST_ENTRY(UNI_L(0)), // 0x98
	CONST_ENTRY(UNI_L(0)), // 0x99
	CONST_ENTRY(UNI_L("PAP")),
	CONST_ENTRY(UNI_L("CHAP")),
	CONST_ENTRY(UNI_L("HTTP-BASIC")),
	CONST_ENTRY(UNI_L("HTTP-DIGEST")),
	CONST_ENTRY(UNI_L("WTLS-SS")),
	CONST_ENTRY(UNI_L("MD5")),
	CONST_ENTRY(UNI_L(0)), // 0xa0
	CONST_ENTRY(UNI_L(0)), // 0xa1
	CONST_ENTRY(UNI_L("GSM-USSD")),
	CONST_ENTRY(UNI_L("GSM-SMS")),
	CONST_ENTRY(UNI_L("ANSI-136-GUTS")),
	CONST_ENTRY(UNI_L("IS-95-CDMA-SMS")),
	CONST_ENTRY(UNI_L("IS-95-CDMA-CSD")),
	CONST_ENTRY(UNI_L("IS-95-CDMA-PACKET")),
	CONST_ENTRY(UNI_L("ANSI-136-CSD")),
	CONST_ENTRY(UNI_L("ANSI-136-GPRS")),
	CONST_ENTRY(UNI_L("GSM-CSD")),
	CONST_ENTRY(UNI_L("GSM-GPRS")),
	CONST_ENTRY(UNI_L("AMPS-CDPD")),
	CONST_ENTRY(UNI_L("PDC-CSD")),
	CONST_ENTRY(UNI_L("PDC-PACKET")),
	CONST_ENTRY(UNI_L("IDEN-SMS")),
	CONST_ENTRY(UNI_L("IDEN-CSD")),
	CONST_ENTRY(UNI_L("IDEN-PACKET")),
	CONST_ENTRY(UNI_L("FLEX/REFLEX")),
	CONST_ENTRY(UNI_L("PHS-SMS")),
	CONST_ENTRY(UNI_L("PHS-CSD")),
	CONST_ENTRY(UNI_L("TETRA-SDS")),
	CONST_ENTRY(UNI_L("TETRA-PACKET")),
	CONST_ENTRY(UNI_L("ANSI-136-GHOST")),
	CONST_ENTRY(UNI_L("MOBITEX-MPAK")),
	CONST_ENTRY(UNI_L("CDMA2000-1X-SIMPLE-IP")),
	CONST_ENTRY(UNI_L("CDMA2000-1X-MOBILE-IP")),
	CONST_ENTRY(UNI_L(0)), // 0xbb
	CONST_ENTRY(UNI_L(0)), // 0xbc
	CONST_ENTRY(UNI_L(0)), // 0xbd
	CONST_ENTRY(UNI_L(0)), // 0xbe
	CONST_ENTRY(UNI_L(0)), // 0xbf
	CONST_ENTRY(UNI_L(0)), // 0xc0
	CONST_ENTRY(UNI_L(0)), // 0xc1
	CONST_ENTRY(UNI_L(0)), // 0xc2
	CONST_ENTRY(UNI_L(0)), // 0xc3
	CONST_ENTRY(UNI_L(0)), // 0xc4
	CONST_ENTRY(UNI_L("AUTOBAUDING")),
	CONST_ENTRY(UNI_L(0)), // 0xc6
	CONST_ENTRY(UNI_L(0)), // 0xc7
	CONST_ENTRY(UNI_L(0)), // 0xc8
	CONST_ENTRY(UNI_L(0)), // 0xc9
	CONST_ENTRY(UNI_L("CL-WSP")),
	CONST_ENTRY(UNI_L("CO-WSP")),
	CONST_ENTRY(UNI_L("CL-SEC-WSP")),
	CONST_ENTRY(UNI_L("CO-SEC-WSP")),
	CONST_ENTRY(UNI_L("CL-SEC-WTA")),
	CONST_ENTRY(UNI_L("CO-SEC-WTA")),
	CONST_ENTRY(UNI_L("OTA-HTTP-TO")), // 0xd0
	CONST_ENTRY(UNI_L("OTA-HTTP-TLS-TO")),
	CONST_ENTRY(UNI_L("OTA-HTTP-PO")),
	CONST_ENTRY(UNI_L("OTA-HTTP-TLS-PO")),
	CONST_ENTRY(UNI_L(0)), // 0xd4
	CONST_ENTRY(UNI_L(0)), // 0xd5
	CONST_ENTRY(UNI_L(0)), // 0xd6
	CONST_ENTRY(UNI_L(0)), // 0xd7
	CONST_ENTRY(UNI_L(0)), // 0xd8
	CONST_ENTRY(UNI_L(0)), // 0xd9
	CONST_ENTRY(UNI_L(0)), // 0xda
	CONST_ENTRY(UNI_L(0)), // 0xdb
	CONST_ENTRY(UNI_L(0)), // 0xdc
	CONST_ENTRY(UNI_L(0)), // 0xdd
	CONST_ENTRY(UNI_L(0)), // 0xde
	CONST_ENTRY(UNI_L(0)), // 0xdf
	CONST_ENTRY(UNI_L("AAA")), // 0xe0
	CONST_ENTRY(UNI_L("HA"))
CONST_END(Provisioning_WBXML_attr_value_tokens)

// codes shared between code page 0 and 1
const UINT8 Provisioning_WBXML_attr_value_tokens_shared[] = {
	0x86, 0x87, 0x88
};

// code page 1
CONST_ARRAY(Provisioning_WBXML_attr_value_tokens_cp1, uni_char*)
	CONST_ENTRY(UNI_L(",")), // 0x80
	CONST_ENTRY(UNI_L("HTTP-")),
	CONST_ENTRY(UNI_L("BASIC")),
	CONST_ENTRY(UNI_L("DIGEST")),
	CONST_ENTRY(UNI_L(0)), // 0x84
	CONST_ENTRY(UNI_L(0)), // 0x85
	CONST_ENTRY(UNI_L(0)), // 0x86 // take from code page 0
	CONST_ENTRY(UNI_L(0)), // 0x87 // take from code page 0
	CONST_ENTRY(UNI_L(0)), // 0x88 // take from code page 0
	CONST_ENTRY(UNI_L(0)), // 0x89
	CONST_ENTRY(UNI_L(0)), // 0x8a
	CONST_ENTRY(UNI_L(0)), // 0x8b
	CONST_ENTRY(UNI_L(0)), // 0x8c
	CONST_ENTRY(UNI_L("APPSRV")), // 0x8d
	CONST_ENTRY(UNI_L("OBEX"))
CONST_END(Provisioning_WBXML_attr_value_tokens_cp1)

class Provisioning_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
	Provisioning_WBXML_ContentHandler(WBXML_Parser *parser)
		: WBXML_ContentHandler(parser) {}

	OP_WXP_STATUS Init();

	const uni_char *TagHandler(UINT32 tok);
	const uni_char *AttrStartHandler(UINT32 tok);
	const uni_char *AttrValueHandler(UINT32 tok);
};

OP_WXP_STATUS Provisioning_WBXML_ContentHandler::Init()
{
	return OpStatus::OK;
}

const uni_char *Provisioning_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x07)
		return 0;

	return Provisioning_WBXML_tag_tokens[tok - 0x05];
}

const uni_char *Provisioning_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	UINT32 cp = GetParser()->GetCodePage();
	if (cp != 0 && cp != 1)
		return 0;

	if (cp == 0)
	{
		if (tok < 0x05 || tok > 0x5b)
		return 0;

	return Provisioning_WBXML_attr_start_tokens[tok - 0x05];
}

	int i;
	for (i=0; i<sizeof(Provisioning_WBXML_attr_start_tokens_shared); i++)
	{
		if (tok == Provisioning_WBXML_attr_start_tokens_shared[i])
			return Provisioning_WBXML_attr_start_tokens[tok - 0x05];
	}

	if (tok < 0x2e || tok > 0x59)
		return 0;
	return Provisioning_WBXML_attr_start_tokens_cp1[tok - 0x2e];
}

const uni_char *Provisioning_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	UINT32 cp = GetParser()->GetCodePage();
	if (cp != 0 && cp != 1)
		return 0;

	if (cp == 0)
	{
		if (tok < 0x85 || tok > 0xe1)
			return 0;

		return Provisioning_WBXML_attr_value_tokens[tok - 0x85];
	}

	int i;
	for (i=0; i<sizeof(Provisioning_WBXML_attr_value_tokens_shared); i++)
	{
		if (tok == Provisioning_WBXML_attr_value_tokens_shared[i])
	return Provisioning_WBXML_attr_value_tokens[tok - 0x85];
	}

	if (tok < 0x80 || tok > 0x8e)
		return 0;
	return Provisioning_WBXML_attr_value_tokens_cp1[tok - 0x80];
}
