/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _DOCUMENTREFERER_H_
#define _DOCUMENTREFERER_H_

#include "modules/url/url2.h"

class DocumentOrigin;
class FramesDocument;

/**
 * This class represents the cause of a document load. It's used both
 * to deduce HTTP Referrer and to deduce the best security context for
 * the new document in case the new document doesn't have an inherent
 * security context. If a document load has its origin in another
 * document, then the DocumentReferrer should preferably be generated
 * through that document's DocumentOrigin object.
 */
class DocumentReferrer
{
public:
	/**
	 * The referrer url. Will be set even if the non-URL version of the
	 * constructor is used.
	 */
	URL url;

	/**
	 * The DocumentOrigin of the document requesting the new document load.
	 */
	DocumentOrigin* origin; ///< Owns a reference to the origin if it's not NULL.

	/**
	 * Creates an empty referrer. Explicit since this is an opportunity
	 * to lose security information and should only be used to user
	 * started top level document loads.
	 */
	explicit DocumentReferrer() : origin(NULL) {}

	/**
	 * Creates a DocumentReferrer from an url. This is not the preferred
	 * constructor. If possible, use one based on a document since if
	 * the url sent in here is a data: url it doesn't have the security
	 * information we need.
	 */
	explicit DocumentReferrer(URL url) : url(url), origin(NULL) {}

	/**
	 * Creates a DocumentReferrer based on the DocumentOrigin of a
	 * document.
	 */
	DocumentReferrer(FramesDocument* doc);

	/**
	 * Creates a DocumentReferrer based on the DocumentOrigin of a
	 * document. The DocumentReferrer will add a reference to the
	 * DocumentOrigin.
	 */
	DocumentReferrer(DocumentOrigin* origin);

	/**
	 * Destructor. Will release the reference to the origin.
	 */
	~DocumentReferrer();

	/**
	 * Copy constructor.
	 */
	DocumentReferrer(const DocumentReferrer& other);

	/**
	 * Assignment operator.
	 */
	DocumentReferrer& operator=(const DocumentReferrer& other);

	/**
	 * Check if the referrer is empty (i.e. there is no information
	 * to base HTTP headers or future security contexts on.
	 */
	BOOL IsEmpty() { return url.IsEmpty(); }
};

#endif /* _DOCUMENTREFERER_H_ */
