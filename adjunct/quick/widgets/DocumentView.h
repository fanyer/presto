/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOCUMENT_VIEW_H
#define DOCUMENT_VIEW_H

#include "modules/inputmanager/inputmanager_constants.h"
#include "modules/widgets/OpSlider.h"

/**
 * @brief DocumentView is an abstract layer which can be used by
 * classes(inheriting OpWidget) interested to be hosted by
 * DocumentDesktopWindow.
 * 
 * DocumentDesktopWindow class is a host or it can be called as
 * as a container which takes responsibilities of managing
 * life-cycle of objects which are interested to be shown in 
 * a container.
 * 
 * This container from UI perspective provides standard skeleton
 * consisting of address-bar, search-field, standard toolbars,
 * etc.
 * 
 * Those classes which would like to show such standard UI skeleton
 * could inherit from this and implement abstract interfaces.
 * 
 * Below is the list of classes which are interested to be hosted by
 * DocumentDesktopWindow.
 * a. OpBrowserView
 * b. OpSpeedDialView
 * c. OpCollectionView
 */
class DocumentView
	: public OpWidgetListener
{
public:
	enum DocumentViewTypes
	{
		DOCUMENT_TYPE_BROWSER,
		DOCUMENT_TYPE_SPEED_DIAL,
		DOCUMENT_TYPE_COLLECTION,
	};

	enum DocumentViewFlags
	{
		ALLOW_NOTHING			= 0,
		ALLOW_ADD_TO_SPEED_DIAL = (1 << 0),
		ALLOW_ADD_TO_BOOKMARK	= (1 << 1),
		ALLOW_ADD_TO_TRASH_BIN	= (1 << 2),
		ALLOW_SHOW_URL			= (1 << 3),
	};

	/**
	 * Maps given string(which is address) to appropriate type
	 *
	 * @param str holds pointer to webpage address
	 * @return one of the matched document type
	 */
	static DocumentViewTypes GetType(const uni_char* str);

	/**
	 * Document views are required to mask some options and UI elements,
	 * which are:
	 * a. visibility of URI in addressbar
	 * b. visibility of favorite/star menu
	 * c. ability to add to bookmark
	 * d. ability to add to trashbin on delete
	 *
	 * Depending of which document view is active, standard behavior/ui features
	 * varies a bit.
	 * For an example, in respect to current scope, speed-dial view return false
	 * , or mask, all the above listed requirements.
	 * @param str holds pointer to webpage address
	 * @param flag one of the DocumentViewFlags.
	 * @return depending on predefined criteria it either returns true or false.
	 */
	static bool IsUrlRestrictedForViewFlag(const uni_char* str, DocumentViewFlags flag);

	/**
	 * Fetches image for a given image string
	 *
	 * @param str holds pointer to webpage address
	 * @param[out] img holds image for a given string
	 */
	static void GetThumbnailImage(const uni_char* str, Image &img);

	virtual ~DocumentView(){ }

	/**
	 * @param container holds pointer to widget which hosts document view
	 * @return status of create call
	 */
	virtual OP_STATUS Create(OpWidget* container) = 0;

	/**
	 * OnIputAction is called to let the input context handle an action, and if so return TRUE.
	 * @param action pointer to inputaction object
	 */
	virtual BOOL OnInputAction(OpInputAction* action) = 0;

	/**
	 * Sets focus on a input context
	 * @param reason one of the focus reason type from FOCUS_REASON
	 */
	virtual void SetFocus(FOCUS_REASON reason) = 0;

	/**
	 * @param visible, true means make document view visible 
	 */
	virtual void Show(bool visible) = 0;

	/**
	 * Tests visiblity
	 * @return true is document view is visible otherwise false
	 */
	virtual bool IsVisible() = 0;

	/**
	 * @return OpWidget related to DocumentView
	 */
	virtual OpWidget* GetOpWidget() = 0;

	/**
	 * Layouts documentview
	 */
	virtual void Layout() = 0;

	/**
	 * @param rect of document view
	 */
	virtual void SetRect(OpRect rect) = 0;

	/**
	 * @return of the type from DocumentViewTypes
	 */
	virtual DocumentViewTypes GetDocumentType() = 0;

	/**
	 * @param[out] title associated with documentview
	 */
	virtual OP_STATUS GetTitle(OpString& title) = 0;

	/**
	 * @param[out] image of document view
	 */
	virtual void GetThumbnailImage(Image& image) = 0;

	/**
	 * @return whether image is fixed or not
	 */
	virtual bool HasFixedThumbnailImage() = 0;

	/**
	 * @param[out] text contains tools tips info.
	 */
	virtual OP_STATUS GetTooltipText(OpInfoText& text) = 0;

	/**
	 * @param force identifies whether to update favicon
	 * @return favicon associated with document view
	 */
	virtual const OpWidgetImage* GetFavIcon(BOOL force) = 0;

	/**
	 * Fetches zoom related information
	 *
	 * @param[out] min holds minimum zoom factor
	 * @param[out] max holds maximum zoom factor
	 * @param[out] step holds step factor
	 * @param[out] tick_values holds array of tick values for slider
	 * @param[out] num_tick_values holds number of items in tick_values array
	 */
	virtual void	QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values) = 0;

	/**
	 * @return the document window scale factor
	 * @retval 1 for 100% zoom
	 */
	virtual double	GetWindowScale() = 0;
};

#endif // DOCUMENT_VIEW_H
