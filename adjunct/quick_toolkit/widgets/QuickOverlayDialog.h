/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_OVERLAY_DIALOG_H
#define QUICK_OVERLAY_DIALOG_H

#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"
#include "modules/widgets/OpWidget.h"

class OpOverlayLayoutWidget;
class QuickAnimationWindowObject;

/**
 * Calculates the placement of a QuickOverlayDialog.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
class QuickOverlayDialogPlacer
{
public:
	virtual ~QuickOverlayDialogPlacer() {}

	/**
	 * Calculates the dialog placement within some bounds.
	 *
	 * @param dialog_size the current dialog size
	 * @param bounds the bounding area
	 * @return new dialog placement within @a bounds
	 */
	virtual OpRect CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size) = 0;

	/**
	 * This is where the placer can draw an arrow from the dialog.
	 *
	 * @param arrow_skin the skin to use when drawing the arrow
	 * @param placement dialog placement in parent window coordinates
	 */
	virtual void PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement) = 0;

	/**
	 * This is where placer can communicate, if returned rect will be relative
	 * to root window or not.
	 *
	 * @retval TRUE  if relative to root window
	 * @retval FALSE if relative to parent window
	 */
	virtual BOOL UsesRootCoordinates() = 0;
};


/**
 * An overlaid QuickDialog.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
class QuickOverlayDialog : public QuickDialog, public OpWidgetListener
{
public:
	enum AnimationType
	{
		DIALOG_ANIMATION_NONE,
		DIALOG_ANIMATION_FADE,
		DIALOG_ANIMATION_FADE_AND_SLIDE,
		DIALOG_ANIMATION_CALLOUT,
		DIALOG_ANIMATION_SHEET
	};

	QuickOverlayDialog();
	virtual ~QuickOverlayDialog();

	/**
	 * Sets the object to use when calculating dialog placement.
	 *
	 * If left unset, a default placer is used.
	 *
	 * @param placer the placer object.  Ownership is transferred.
	 */
	void SetDialogPlacer(QuickOverlayDialogPlacer* placer);

	/**
	 * Sets the widget to overlay the dialog upon.
	 *
	 * If left unset, the dialog is required to have a parent DesktopWindow,
	 * and the root widget of that window is used as the bounding widget.
	 *
	 * @param bounding_widget the dialog will be placed within the area of this
	 * 		widget
	 */
	void SetBoundingWidget(OpWidget& bounding_widget) { m_bounding_widget = &bounding_widget; }

	/**
	 * Sets the type of show/hide animation for the dialog.
	 *
	 * @param type the animation type
	 */
	void SetAnimationType(AnimationType type) { m_animation_type = type; }

	/**
	 * Makes the dialog dim the page it is shown over or not.
	 *
	 * Overlaid dialogs do not dim pages by default.
	 *
	 * @param dims_page whether the dialog should dim the page it is shown over
	 */
	void SetDimsPage(bool dims_page) { m_dims_page = dims_page; }

	// QuickDialog
	virtual OP_STATUS SetTitle(const OpStringC& title);
	virtual const char* GetDefaultHeaderSkin() { return "Overlay Dialog Header Skin"; }
	virtual const char* GetDefaultContentSkin() { return "Overlay Dialog Page Skin"; }
	virtual const char* GetDefaultButtonSkin() { return "Overlay Dialog Button Border Skin"; }

	// OpWidgetListener
	virtual void OnRelayout(OpWidget* widget) { UpdatePlacement(); }

	// DesktopWindowListener
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

protected:
	// QuickWindow
	virtual const char* GetDefaultSkin() const { return "Overlay Dialog Skin"; }
	virtual BOOL SkinIsBackground() const { return TRUE; }
	virtual OP_STATUS OnShowing();

private:
	/**
	 * The default placer centers the dialog within the parent window, and
	 * doesn't use an arrow.
	 */
	class DefaultPlacer : public QuickOverlayDialogPlacer
	{
	public:
		virtual OpRect CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size)
		{
			return OpRect((bounds.width - dialog_size.width) / 2,
					(bounds.height - dialog_size.height) / 2,
					dialog_size.width, dialog_size.height);
		}
		virtual void PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement) {}
		virtual BOOL UsesRootCoordinates() { return FALSE; }
	};

	/**
	 * The "sheet" placer centers the dialog horizontally within the parent
	 * window, and aligns the top edges of the parent window and the dialog.
	 * It doesn't use an arrow.
	 */
	class SheetPlacer : public QuickOverlayDialogPlacer
	{
	public:
		virtual OpRect CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size)
		{
			return OpRect((bounds.width - dialog_size.width) / 2, 0,
					dialog_size.width, dialog_size.height);
		}
		virtual void PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement) {}
		virtual BOOL UsesRootCoordinates() { return FALSE; }
	};

	/**
	 * The "dummy" placer shouldn't be used for placing an OverlayDialog but 
	 * is a fallback in case your anchor widget gets deleted for a short time.
	 * The advantage is that it won't cause a relayout of the dialog until you
	 * set a proper QuickOverlayDialogPlacer
	 */
	class DummyPlacer : public QuickOverlayDialogPlacer
	{
		public:
			virtual OpRect CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size) { return dialog_size; }
			virtual void PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement) {}
			virtual BOOL UsesRootCoordinates() { return FALSE; }
	};

	OpRect CalculatePlacement();
	void UpdatePlacement();

	OP_STATUS AnimateIn();
	OP_STATUS AnimateOut();
	void PrepareInAnimation(QuickAnimationWindowObject& animation);
	void PrepareOutAnimation(QuickAnimationWindowObject& animation);

	OpWidget* m_bounding_widget;
	OpOverlayLayoutWidget* m_placer_widget;
	QuickOverlayDialogPlacer* m_placer;
	QuickWindowMover* m_mover_widget;
	AnimationType m_animation_type;
	bool m_dims_page;
};

#endif // QUICK_OVERLAY_DIALOG_H
