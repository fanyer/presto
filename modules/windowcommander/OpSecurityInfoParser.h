/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpSecurityInfoParser.h
 **/

#ifndef OPSECURITY_INFO_PARSER_H
# define OPSECURITY_INFO_PARSER_H

# ifdef SECURITY_INFORMATION_PARSER

/**
 * Class used for keeping the data structures and strings that make up the security information that is relevant for display in the dialog.
 * The purpose is to avoid a large number of loose strings in different classes making it hard to avoid memory leaks and null pointers. Also contains access functions
 * that can create some of these structures from other members where that is desirable.
 *
 * Modify strings by using the getters that returns pointers to them, and then perform the action on the strings.
 *
 * Members are generally kept public for easy access without the need for setters and getters.
 */
class OpSecurityInformationContainer
{
private:
	OpString m_server_url;                  ///< The url of the secure server.
	OpString m_server_port;					///< The port of the secure server.
	OpString m_protocol;                    ///< The communication protocol details.
	OpString m_certificate_servername;		///< Validated name of the server.
	OpString m_issued;                      ///< The time of issue.
	OpString m_expires;                     ///< The expiry date.

	OpString m_subject_common_name;         ///< The common name of the subject
	OpString m_subject_organization;        ///< The subject organization
	OpString m_subject_organizational_unit; ///< The subject organizational unit
	OpString m_subject_locality;            ///< The subject locality
	OpString m_subject_province;            ///< The subject province
	OpString m_subject_country;             ///< The subject country

	OpString m_issuer_common_name;          ///< The common name of the issuer
	OpString m_issuer_organization;         ///< The issuer organization
	OpString m_issuer_organizational_unit;  ///< The issuer organizational unit
	OpString m_issuer_locality;             ///< The issuer locality
	OpString m_issuer_province;             ///< The issuer province
	OpString m_issuer_country;              ///< The issuer country

	OpString m_button_text;                 ///< The "organization (country)" string that is displayed in the address bar.
	OpString m_subject_summary;				///< The cn, o, ou string that is used on the summary pages of information/warning dialogs.
	OpString m_issuer_summary;				///< The cn, o, ou string that is used on the summary pages of information/warning dialogs.
	OpString m_formatted_expiry_date;		///< The date of expiry formatted in the locale's time format.

	BOOL user_confirmed_certificate;         ///< Set true if the certificate is user approved
	BOOL permanently_confirmed_certificate;  ///< Set true if the certificate is permanently user approved
	BOOL anonymous_connection;               ///< Set true if the server has not provided a certificate
	BOOL unknown_ca;                         ///< Set true if the certificate is signed by an untrusted CA
	BOOL certexpired;                        ///< Set true if the expiry date of the certificate has passed
	BOOL cert_not_yet_valid;                 ///< Set true if the issued date has not passed yet
	BOOL configured_warning;                 ///< Set true if the user has asked for warnings from this issuer
	BOOL authentication_only_no_encryption;  ///< Set true if the cert info indicates that no encryption is taking place
	BOOL low_encryptionlevel;                ///< Set true if the encryption is weak
	BOOL servername_mismatch;                ///< Set true when the server name in the cert is a mismatch.
	BOOL ocsp_request_failed;                ///< Set true when the ocsp request failed
	BOOL ocsp_response_failed;               ///< Set true when the ocsp response failed
	BOOL reneg_extension_unsupported;        ///< Set true when the server does not support the TLS Renegotiation Indication Extension 

	/**
	 * Used to help making subject/issuer summary strings
	 * @param src source string to append. If empty, nothing will happen.
	 * @param trg target string to append to. Will be modified if src is not empty
	 * @return TRUE if trg was modified, false otherwise (src was empty)
	 */
	BOOL AppendCommaSeparated(const OpString &src, OpString &trg);
	/**
	 * Used  to help making tm structs from the strings in the info, which can then
	 * be used to format the time according to locale.
	 * Example expected format of time string:
	 *
	 * 03.02.2008 12:19:19 GMT
	 *
	 * @param src The string that needs to be converted to a tm
	 * @param tm to fill with the info
	 * @return OK if ok, ERR if problem detected.
	 */
	OP_STATUS GetTMStructTime(const OpString &src, tm &trg);

public:
	/** Constructor. */
	OpSecurityInformationContainer() : user_confirmed_certificate(FALSE),
		                             permanently_confirmed_certificate(FALSE),
		                             anonymous_connection(FALSE),
									 unknown_ca(FALSE),
									 certexpired(FALSE),
									 cert_not_yet_valid(FALSE),
									 configured_warning(FALSE),
									 authentication_only_no_encryption(FALSE),
									 low_encryptionlevel(FALSE),
									 servername_mismatch(FALSE),
									 ocsp_request_failed(FALSE),
									 ocsp_response_failed(FALSE),
									 reneg_extension_unsupported(FALSE)
	{
	}

	/**
	 * Setter for the security issue booleans.
	 * @param the text of the corresponding security info document tag
	 * @return OK if the tag was found and set, ERROR if the tag wasn't found.
	 */
	OP_STATUS SetSecurityIssue(const uni_char* tag);

	// Getters
	OpString* GetServerURLPtr()                 {return &m_server_url;}
	OpString* GetServerPortNumberPtr()			{return &m_server_port;}
	OpString* GetProtocolPtr()                  {return &m_protocol;}
	OpString* GetCertificateServerNamePtr()		{return &m_certificate_servername;}
	OpString* GetIssuedPtr()                    {return &m_issued;}
	OpString* GetExpiresPtr()                   {return &m_expires;}

	OpString* GetSubjectCommonNamePtr()         {return &m_subject_common_name;}
	OpString* GetSubjectOrganizationPtr()       {return &m_subject_organization;}
	OpString* GetSubjectOrganizationalUnitPtr() {return &m_subject_organizational_unit;}
	OpString* GetSubjectLocalityPtr()           {return &m_subject_locality;}
	OpString* GetSubjectProvincePtr()           {return &m_subject_province;}
	OpString* GetSubjectCountryPtr()            {return &m_subject_country;}

	OpString* GetIssuerCommonNamePtr()          {return &m_issuer_common_name;}
	OpString* GetIssuerOrganizationPtr()        {return &m_issuer_organization;}
	OpString* GetIssuerOrganizationalUnitPtr()  {return &m_issuer_organizational_unit;}
	OpString* GetIssuerLocalityPtr()            {return &m_issuer_locality;}
	OpString* GetIssuerProvincePtr()            {return &m_issuer_province;}
	OpString* GetIssuerCountryPtr()             {return &m_issuer_country;}

	BOOL GetUserConfirmedCertificate()          {return user_confirmed_certificate;}
	BOOL GetPermanentlyConfirmedCertificate()   {return permanently_confirmed_certificate;}
	BOOL GetAnonymousConnection()               {return anonymous_connection;}
	BOOL GetUnknownCA()                         {return unknown_ca;}
	BOOL GetCertExpired()                       {return certexpired;}
	BOOL GetCertNotYetValid()                   {return cert_not_yet_valid;}
	BOOL GetConfiguredWarning()                 {return configured_warning;}
	BOOL GetAuthenticationOnlyNoEncryption()    {return authentication_only_no_encryption;}
	BOOL GetLowEncryptionLevel()                {return low_encryptionlevel;}
	BOOL GetServernameMismatch()                {return servername_mismatch;}
	BOOL GetOcspRequestFailed()                 {return ocsp_request_failed;}
	BOOL GetOcspResponseFailed()                {return ocsp_response_failed;}
	BOOL GetOCSPProblem()                       {return (ocsp_request_failed || ocsp_response_failed);}
	BOOL GetRenegotiationExtensionUnsupported() {return	reneg_extension_unsupported;}

	/**
	 * Fetch the text for the security button text.
	 */
	OpString* GetButtonTextPtr();
	/**
	 * Fetch comma separated subject summary string to display in information/warning dialogs
	 */
	OpString* GetSubjectSummaryString();
	/**
	 * Fetch comma separated issuer summary string to display in information/warning dialogs
	 */
	OpString* GetIssuerSummaryString();
	/**
	 * Get locale formated expiry date string
	 */
	OpString* GetFormattedExpiryDate();
};

class OpSecurityInformationParser
{
	public:

		virtual ~OpSecurityInformationParser(){}
		/**
		 * Get a tree model from the parsed information.
		 *
		 * If the tree model has not been created already, then MakeTreeModel will be called, but that will only happen once.
		 * A member pointer will keep track of the model, and it will be deleted on deletion of the parser.
		 *
		 * @return A pointer to a treemodel that represents the contents of the security information. If some error occurs, it returns NULL.
		 *
		 * @see MakeTreeModel()
		 */
		virtual SimpleTreeModel* GetServerTreeModelPtr() = 0;

		/**
		 *
		 */
		virtual SimpleTreeModel* GetClientTreeModelPtr() = 0;
		/**
		 *
		 */
		virtual SimpleTreeModel* GetValidatedTreeModelPtr() = 0;

		virtual OpSecurityInformationContainer* GetSecurityInformationContainterPtr() = 0;
};

# endif // SECURITY_INFORMATION_PARSER
#endif /*OPSECURITY_INFO_PARSER_H*/
