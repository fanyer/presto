/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_POLICY_H_
#define _URL_POLICY_H_

/** What is the reload policy */
enum URL_Reload_Policy {
	/** No reload (Must-revalidate will override). */
	URL_Reload_None,
	/**  Normal unconditional, no reload. */
	URL_Load_Normal,
	/** Reload if expired or not loaded. */
	URL_Reload_If_Expired,
	/** Conditional reload (If-Modified-Since). */
	URL_Reload_Conditional,
	/** Perform a resume if possible. */
	URL_Resume,
	/** Unconditional reload, from server. */
	URL_Reload_Full,
	/** Unconditional reload, without caching the result. */
	URL_Reload_Exclusive
};

/** This class specifies the circumstances that should be considered when 
 *	deciding whether or not to load a document and how to load it
 *
 *	Circumstances presently considered are;
 *		Is the load request based on a history walk operation?
 *		Is the document used as an inline element
 *		What is the reload policy for this specific loading
 *		Did the user request the load, or is it requested by a document?
 *
 *	Several predefined subclasses exists
 */
class URL_LoadPolicy
{
protected:
	/** Is this load triggered by changing document in the history (true) or is it a normal load */
	BOOL history_walk;

	/** TRUE if we should bypass any proxy */
	BOOL bypass_proxy;

	/** The reload policy to use */
	URL_Reload_Policy reload_policy;

	/** TRUE if the user actually clicked the URL */
	BOOL user_initiated;

	/** TRUE if the resource is an inline element */
	BOOL inline_element;

	/** Internal flag. Suppresses COMM_REQUEST_FINISHED so that documents that have been redirected since last visited are fully reloaded. */
	BOOL history_walk_was_redirected;

	/** third party determined flag */
	BOOL thirdparty_determined;

	EnteredByUser entered_by_user;

public:

	/** Constructor
	 *
	 *	@param	hist_walk	History walk flag, default FALSE
	 *	@param	reload_pol	Reload policy, default URL_Reload_If_Expired
	 *	@param	user		Was this initiated by the user? Default FALSE
	 *	@param	inline_elm	Is this an inline element? Default FALSE
	 */
	URL_LoadPolicy(BOOL hist_walk = FALSE, URL_Reload_Policy reload_pol = URL_Reload_If_Expired, BOOL user = FALSE, BOOL inline_elm = FALSE, BOOL a_bypass_proxy = FALSE)
		: history_walk(hist_walk), bypass_proxy(a_bypass_proxy), reload_policy(reload_pol), user_initiated(user), inline_element(inline_elm), history_walk_was_redirected(FALSE), thirdparty_determined(FALSE), entered_by_user(NotEnteredByUser) {}

	/** Set the history walk flag */
	void SetHistoryWalk(BOOL hist_walk){history_walk = hist_walk;}

	/** Get the history walk flag */
	BOOL IsHistoryWalk() const{return history_walk;}

	/** Set the third party determined flag */
	void SetDeterminedThirdparty(BOOL thirdpart_det){thirdparty_determined = thirdpart_det;}

	/** Get the third party determined flag */
	BOOL HasDeterminedThirdparty() const{return thirdparty_determined;}

	/** Set the entered by user value */
	void SetEnteredByUser(EnteredByUser entered){entered_by_user = entered;}

	/** Get the entered by user value */
	EnteredByUser WasEnteredByUser() const{return entered_by_user;}

	/** Set the bypass proxy flag */
	void SetProxyBypass(BOOL a_bypass_proxy){bypass_proxy = a_bypass_proxy;}

	/** Get the bypass proxy flag */
	BOOL IsProxyBypass() const{return bypass_proxy;}

	/** Set the Reload policy */
	void SetReloadPolicy(URL_Reload_Policy reload_pol){reload_policy = reload_pol;}
	/** Get the Reload policy */
	URL_Reload_Policy GetReloadPolicy() const{return reload_policy;}
	
	/** Set the user initiated flag */
	void SetUserInitiated(BOOL user){user_initiated = user;}
	/** Get the user initiated flag */
	BOOL IsUserInitiated() const{return user_initiated;}

	/** Set the inline element flag */
	void SetInlineElement(BOOL inline_elm){inline_element = inline_elm;}
	/** Get the inline element flag */
	BOOL IsInlineElement() const{return inline_element;}

	/** Set the redirected history walk flag */
	void SetRedirectedHistoryWalk(BOOL redirected){history_walk_was_redirected = redirected;}
	/** Get the redirected history walk flag */
	BOOL IsRedirectedHistoryWalk() const{return history_walk_was_redirected;}
};

/** Normal reload policy */
typedef URL_LoadPolicy URL_NormalLoad;

/** Reload the document completely */ 
class URL_FullReload : public URL_LoadPolicy
{
public:
	/** Constructor
	 *
	 *	@param	p_inline	Inline element flag, default FALSE
	 *	@param	user		Was this initiated by the user? Default FALSE
	 */
	URL_FullReload(BOOL user = FALSE)
		: URL_LoadPolicy(FALSE, URL_Reload_Full, user){}
};

/** Normal history walk operation */
class URL_HistoryNormal : public URL_LoadPolicy
{
public:
	URL_HistoryNormal()
		: URL_LoadPolicy(TRUE, URL_Reload_If_Expired, FALSE){}
};


/** Inline document, conditional reload */
class URL_ConditionalLoad: public URL_LoadPolicy
{
public:
	URL_ConditionalLoad()
		: URL_LoadPolicy(FALSE, URL_Reload_Conditional, FALSE){}
};

#endif // _URL_POLICY_H_
