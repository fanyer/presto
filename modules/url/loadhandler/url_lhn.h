/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_LHN_H_
#define _URL_LHN_H_

// URL NameResolver Load Handlers

class Comm;

struct NameCandidate : public Link
{
private:
	NameCandidate();

    /**
	 * Second phase constructor. This method must be called prior to using the object,
	 * unless it was created using the Create() method.
     */
	OP_STATUS Construct(const OpStringC &prefix, const OpStringC &kernel, const OpStringC &postfix);

public:
	OpString name;

	/**
	 * Creates and initializes a NameCandidate object.
	 *
	 * @param namecandidate  Set to the created object if successful and to NULL otherwise.
	 *                       DON'T use this as a way to check for errors, check the
	 *                       return value instead!
	 * @param prefix
	 * @param kernel
	 * @param postfix
	 *
	 * @return OP_STATUS - Always check this.
	 */
	static OP_STATUS Create(NameCandidate **namecandidate, const OpStringC &prefix, const OpStringC &kernel, const OpStringC &postfix);

	virtual ~NameCandidate(){};
};

class URL_NameResolve_LoadHandler : public URL_LoadHandler
{
	private:
		ServerName_Pointer base;

		AutoDeleteHead candidates;
		ServerName_Pointer candidate;

		Comm *lookup;
		URL *proxy_request;
		URL_InUse proxy_block;
		BOOL force_direct_lookup;
/*
		char   			*permuted_host;
		char   			*permute_front;
		char   			*permute_end;
		char   			*permute_front_pos;
		char   			*permute_end_pos;
		const char		*permute_template;
		struct{
			BYTE	checkexpandedhostname:1;
			BYTE	name_completetion_mode:1;
		} info;
*/

	private:
		void HandleHeaderLoaded();
		void HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2);
		void HandleLoadingFinished();
		CommState StartNameResolve();

	public:
		URL_NameResolve_LoadHandler(URL_Rep *url_rep, MessageHandler *mh);
		virtual ~URL_NameResolve_LoadHandler();

		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		virtual CommState Load();
		virtual unsigned ReadData(char *buffer, unsigned buffer_len);
		virtual void EndLoading(BOOL force=FALSE);
		virtual CommState ConnectionEstablished();

		virtual void SetProgressInformation(ProgressState progress_level, 
											 unsigned long progress_info1, 
											 const void *progress_info2);
};

#endif // _URL_LHN_H_
