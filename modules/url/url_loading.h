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


#ifndef _URL_LOADING_H_
#define _URL_LOADING_H_

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT

/** An object of this class prevents the document contained in the URL member from being 
 *  updated (e.g. if a second caller wants to (re)load the document) or automatically freed during cache cleanups
 *	To accomplish this the IncLoading/DecLoading functions of the URL class are used.
 *
 *	In case of OOM a reservation will fail. Given the procedure for use this is unlikely to happen.
 *
 *	Procedure for use:
 *
 *		1.	Start loading the URL
 *		2.	Use IAmLoadingThisURL::SetURL or similar functionality to reserve the URL
 *		3.	Perform all loading operation 
 *		4.	When the loading operation is completely finished use IAmLoadingThisURL::UnsetURL 
 *			or similar functionality (e.g. destructor) to unreserve the URL. Other
 *			callers may now (re)load the URL.
 *
 *	!!NOTE!!: Redirects ARE NOT blocked automatically in this manner. The owner must
 *	update blocking as is required by the functionality requirement of the owner.
 *
 *  URLs may be assigned/set after construction
 *	Empty URLs are not blocked
 */
class IAmLoadingThisURL
{
private:
	/** The url being reserved */
	URL url;

public:
	/** Default Constructor */
	IAmLoadingThisURL(){};

	/** Copy Constructor */
	IAmLoadingThisURL(URL_InUse &old){URL temp(old.GetURL()); OpStatus::Ignore(SetURL(temp));}

	/** Reserves the given url 
	 *
	 *	@param p_url	URL to reserve
	 */
	IAmLoadingThisURL(URL &p_url){OpStatus::Ignore(SetURL(p_url));}

	/** Destructor */ 
	~IAmLoadingThisURL();

	/** Assignment operator */
	IAmLoadingThisURL &operator =(IAmLoadingThisURL &old){OpStatus::Ignore(SetURL(old)); return *this;}

	/** Assignment operator 
	 *
	 *	@param p_url	URL to reserve
	 */
	IAmLoadingThisURL &operator =(URL &p_url){OpStatus::Ignore(SetURL(p_url)); return *this;}

	/** Reserves the same url as the other object
	 *
	 *	@param p_url	Contains URL to reserve
	 *	@return		OP_STATUS	Will return non-OK in case of OOM
	 */
	OP_STATUS SetURL(IAmLoadingThisURL &old){return SetURL(old.url);}

	/** Reserves the given url 
	 *
	 *	@param p_url	URL to reserve
	 *	@return		OP_STATUS	Will return non-OK in case of OOM
	 */
	OP_STATUS SetURL(URL &);

	/** Unreserves the current url */
	void UnsetURL();

	/** Get the current URL */
	URL GetURL(){return url;};

	/** Access fucntions in the contained URL */
	URL *operator ->(){return &url; }

	/** Returns a reference to the contained URL. DO NOT USE in assignement operations !! */
    operator URL &(){return url;}

	/** Returns a pointer to the contained URL. DO NOT USE in assignement operations !! */
	operator URL *(){return &url;}
};


#endif


#endif // _URL_LOADING_H_
