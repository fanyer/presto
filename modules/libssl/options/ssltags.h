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

#ifndef SSLTAGS_H
#define SSLTAGS_H

#ifdef _NATIVE_SSL_SUPPORT_

// version of file 0x05000001 or higher (0x05000000 is valid for thirdparty files)

// opssl5.dat records

#define TAG_SSL_MAIN_PREFERENCES			0x01		// record of TAG_SSL_PREFS_* records, only one in file.

// opcacrt5.dat records
#define TAG_SSL_CACERTIFICATE_RECORD		0x02		// record with TAG_SSL_CERT_* and TAG_SSL_CACERT_* tags, CA certificates and  preferences

// opcert5.dat
#define TAG_SSL_USER_CERTIFICATE_RECORD		0x03		// record with TAG_SSL_CERT_* and TAG_SSL_USERCERT_* tags, User private keys, certificates and preferences
#define TAG_SSL_USER_PASSWORD_RECORD		0x04		// record with TAG_SSL_PASSWD_* records. The random part of the password protecting the soft private keys.

#define TAG_SSL_TRUSTED_CERTIFICATE_RECORD	0x06		// record with TAG_SSL_CERT_* and TAG_SSL_TRUSTED_* tags, servername, port and trusted until information for certificates that are trusted for a given site
#define TAG_SSL_UNTRUSTED_CERTIFICATE_RECORD	0x07	// record with TAG_SSL_CERT_* , Certificates that are not trusted under any circumstance.

// Unused
#define TAG_SSL_LAST_USED_CERT_DOWNLOAD_ETAG	0x08	// String with Last used Entity tag for downloading the updated certificate data.
// Unused
#define TAG_SSL_LAST_USED_EVOID_DOWNLOAD_ETAG	0x09	// String with Last used Entity tag for downloading the updated EV OID data.

#define TAG_SSL_PREFS_PASSWORD_AGING		0x10		// uint16 
#define TAG_SSL_PREFS_v3_CIPHERS			0x11		// record with a CipherID ciphers<0..2^16-1> vector; selects the active ciphers for the v3+ cipher suite
#define TAG_SSL_PREFS_v2_CIPHERS			0x12		// record with a CipherID ciphers<0..2^16-1> vector; selects the active ciphers for the v2 cipher suite
//#define TAG_SSL_LAST_ROOT_CERTIFICATE_ETAG	0x13		// string with an HTTP ETag for the last downloaded root certificate update
//#define TAG_SSL_LAST_EV_OID_ETAG			0x14		// string with an HTTP ETag for the last downloaded EV OID update
#define TAG_SSL_REPOSITORY_ITEM				0x15		// record with the binary ID of a known root available in the root repository.
#define TAG_SSL_SERVER_OPTIONS				0x16		// Record containing the configuration flags for a given server
#define TAG_SSL_CRL_OVERRIDE_ITEM			0x17		// Record containing a CRL override item. If an issuer with the ID of this record is used, add the CRL from the associated URL
#define TAG_SSL_OCSP_POST_OVERRIDE_ITEM		0x18		// String8 (URL) identifying OCSP URLs that will not use GET, but POST for the request
#define TAG_SSL_UNTRUSTED_REPOSITORY_ITEM	0x19		// record with the binary ID of a known untrusted certificate available in the root repository.
#define TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_ITEM 0x1A  // Flag: If present in file, then this server supports STS and the value stored is the expiry date.

// shared tags TAG_SSL_CACERTIFICATE_RECORD and TAG_SSL_USER_CERTIFICATE_RECORD

#define TAG_SSL_CERT_TYPE					0x20	 // uint16 TLS SSL_ClientCertificateType enumerated value
#define TAG_SSL_CERT_TITLE					0x21	// String: Human readable title
#define TAG_SSL_CERT_NAME					0x22	// Binary datavector: DER encoded X509 DistiguishedName structure (subject name of certificate)
#define TAG_SSL_CERT_CERTIFICATE			0x23	// Binary datavector: DER encoded X509 Certificate


// Tags only used by TAG_SSL_CACERTIFICATE_RECORD

#define TAG_SSL_CACERT_WARN					(0x30 | MSB_VALUE)	// Flag: If present, display warning if this certificate is in the chain of the presented certificate
#define TAG_SSL_CACERT_DENY					(0x31 | MSB_VALUE)	// Flag: If present, do not allow connection if this certificate is in the chain of the presented certificate
#define TAG_SSL_CACERT_PRESHIPPED			(0x32 | MSB_VALUE)	// Flag: If present, this certificate was automatically installed
#define TAG_SSL_EV_OIDS						0x33	// Binary datavector : (Currently undefined format) Extended Validation authorized by Opera and the CA

// Tags only used by TAG_SSL_USER_CERTIFICATE_RECORD 

#define TAG_SSL_USERCERT_PRIVATEKEY			0x40	// Binary datavector: Encrypted private key, encoded in PrivateKey (PKCS#1) structure
#define TAG_SSL_USERCERT_PRIVATEKEY_SALT	0x41	// Binary datavector: 8 bytes Salt data used to decrypt/encrypt private key
#define TAG_SSL_USERCERT_PUBLICKEY_HASH		0x42	// Binary datavector: SHA1 hash of the public key associated with the private key, used to find the key when installing the certificate

// presently unused
#define TAG_SSL_USERCERT_PRIVATEKEY_LOCATIONTYPE	0x43	// uint32: designates location type of the private key. Will be used to identify smartcards
#define TAG_SSL_USERCERT_PRIVATEKEY_LOCATION		0x44	// String: designates location of the private key. human readable or machinereadable location (e.g. filename)

// tags only used by TAG_SSL_CRL_OVERRIDE_ITEM

#define TAG_SSL_CRL_OVERRIDE_ID				0x48	//  record with the binary ID of a certificate (based on the issuer name)
#define TAG_SSL_CRL_OVERRIDE_URL			0x49	//  String with URL of the CRL to be loaded

// Tags only used by TAG_SSL_USER_PASSWORD_RECORD

#define TAG_SSL_PASSWD_MAINBLOCK			0x50	// Binary datavector: Encrypted part of the complete password encrypted using the saltvalue and the user's password
													// the concatenation of the user password and the mainblock is used as the password to the private keys, combined with  the key's salt
#define TAG_SSL_PASSWD_MAINSALT				0x51	// Binary datavector: 8 bytes Salt data used with to encrypt the mainblock

#define TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_SERVERNAME			0x58
#define TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_EXPIRY				0x59
#define TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_INCLUDE_DOMAINS	(0x5A | MSB_VALUE) // FLAG

// Tags used by trusted certificate records
#define TAG_SSL_TRUSTED_SERVER_NAME			0x60	// String, punycode encoded servername of site for which the certificate is accepted, despite warnings
#define TAG_SSL_TRUSTED_SERVER_PORT			0x61	// uint16, port number of site for which the certificate is accepted, despite warnings
#define TAG_SSL_TRUSTED_SERVER_UNTIL		0x62	// time_t, how long is the override valid? 0 means until the certificate expires. 
#define TAG_SSL_TRUSTED_FOR_APPLET			(0x63 | MSB_VALUE)  // Flag: If present this indicates that the certificate is accepted for granting extended privilegdes to Java applets hosted at the given server.

// Tags used by TAG_SSL_SERVER_OPTIONS records
#define TAG_SSL_SERVOPT_UNTIL				0x70	// time_t, how long is this testresult valid, usually 4 weeks after last test. Only checked on preference load. 
#define TAG_SSL_SERVOPT_NAME				0x71	// String, A-Label Name of server 
#define TAG_SSL_SERVOPT_PORT				0x72	// uint16	portnumber of site for which this profile applies
#define TAG_SSL_SERVOPT_TLSVER				0x73	// SSL_Protocol version: Which TLS version to use with the site
#define TAG_SSL_SERVOPT_TLSEXT				(0x74 | MSB_VALUE)  // Flag: Can TLS extensions be used with this site
#define TAG_SSL_SERVOPT_DHE_DEPRECATE		(0x75 | MSB_VALUE)  // Flag: Is DHE deprecated for this server? (happens when the server is using a weak DHE key) True if present


#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLTAGS_H */
