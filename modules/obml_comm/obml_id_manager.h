/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _OBML_ID_MANAGER_H
#define _OBML_ID_MANAGER_H
#ifdef WEB_TURBO_MODE
#undef CRYPTO_ENCRYPTED_FILE_SUPPORT

class OBML_Id_Manager
	: public MessageObject
{
public:
	static OBML_Id_Manager* CreateL(OBML_Config *config);
	~OBML_Id_Manager();

	// From MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1, MH_PARAM_2);

	OP_STATUS	UpdateTurboClientId();
	const char*	GetTurboClientId() { return m_turbo_proxy_client_id.HasContent() ? m_turbo_proxy_client_id.CStr() : NULL; }

	OP_STATUS SetTurboProxyAuthChallenge(const char* challenge) { return m_turbo_proxy_auth_challenge.Set(challenge); }
	OP_STATUS CreateTurboProxyAuth(OpString8& auth, unsigned int auth_id);

private:
	OBML_Id_Manager(OBML_Config* config) :
		m_turbo_proxy_auth_buf(NULL),
		m_turbo_proxy_auth_buf_len(0),
		m_config(config)
		{ }
	void ConstructL();

	void LoadDataFileL();
	void WriteDataFileL();

	/** Generates a new random OBML id in the m_obml_client_id member variable */
	void GenerateRandomObmlIdL();
	static void HexAsciiEncode(byte* indata, unsigned int indata_size, char* outdata);

private:
	/** Random client ID generated at first startup and possibly updated by Mini servers.
	  * Stored in encrypted format in optrb.dat file by the OBML_Config class.
	  */
	OpString8 m_obml_client_id;

	/** Hexascii encoded SHA256 hash of m_client_id used as client identifier for Turbo Proxy */
	OpString8 m_turbo_proxy_client_id;

	/** Authentication challenge from Turbo Proxy */
	OpString8 m_turbo_proxy_auth_challenge;

	/** Helpers to avoid re-allocating buffer */
	byte* m_turbo_proxy_auth_buf;
	size_t m_turbo_proxy_auth_buf_len;

	OBML_Config* m_config; // Not owned
};

#endif // WEB_TURBO_MODE

#endif // _OBML_ID_MANAGER_H
