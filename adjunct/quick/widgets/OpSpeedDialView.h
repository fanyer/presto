/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OP_SPEEDDIAL_H
#define OP_SPEEDDIAL_H

#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"
#include "adjunct/quick/widgets/OpSpeedDialListener.h"
#include "adjunct/quick/widgets/DocumentView.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/quick_toolkit/widgets/DialogListener.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"

#include "adjunct/quick/managers/SpeedDialManager.h"

class ExtensionPrefsController;
class OpImageBackground;
class QuickFlowLayout;
class SpeedDialConfigController;
class SpeedDialGlobalConfigDialog;

/***********************************************************************************
**
**	OpSpeedDialView
**
***********************************************************************************/

class OpSpeedDialView
	: public OpWidget
	, public DialogListener
	, public DialogContextListener
	, public SpeedDialConfigurationListener
	, public SpeedDialListener
	, public QuickScrollContainerListener
	, public QuickWidgetContainer
	, public DocumentView
	, public OpSpeedDialListener
{
public:
	OpSpeedDialView();
	~OpSpeedDialView();

	OP_STATUS							AddListener(OpSpeedDialListener* listener) { return m_listeners.Add(listener); }
	OP_STATUS							RemoveListener(OpSpeedDialListener* listener) { return m_listeners.Remove(listener); }
	OP_STATUS							Init();
	void								ReloadSpeedDial();
	Image								GetThumbnailImage();
	Image								GetThumbnailImage(UINT32 width, UINT32 height);

	// OpInputContext
	virtual BOOL						OnInputAction(OpInputAction* action);
	virtual const char*					GetInputContextName() {return "Speed Dial Widget";}

	// dialog listener hooks
	virtual void						OnClose(Dialog* dialog);

	// DialogContextListener
	virtual void						OnDialogClosing(DialogContext* context);

	// opwidget hooks
	virtual void						OnShow(BOOL show);
	virtual void						OnSettingsChanged(DesktopSettings* settings);
	virtual void						OnAdded();
	virtual	void						OnRemoved();
	virtual	void						OnDeleted();
	virtual void						OnResize(INT32* new_w, INT32* new_h);
	virtual void						OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual BOOL						OnMouseWheel(INT32 delta, BOOL vertical);
	virtual void						OnLayout();
	virtual BOOL						OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked);
	virtual void						OnDirectionChanged() { OnContentsChanged(); }
	virtual void						OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual BOOL						IsScrollable(BOOL vertical) { return TRUE; }
	virtual BOOL						OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE);
	virtual void						OnWindowActivated(BOOL activate);

	// DocumentView
	virtual OP_STATUS					Create(OpWidget* container);
	virtual void						SetFocus(FOCUS_REASON reason) { OpWidget::SetFocus(reason); }
	virtual void						Show(bool visible);
	virtual bool						IsVisible();
	virtual OpWidget*					GetOpWidget() { return this; }
	virtual void						SetRect(OpRect rect);
	virtual void						Layout();
	virtual DocumentViewTypes			GetDocumentType() { return DOCUMENT_TYPE_SPEED_DIAL; }
	virtual OP_STATUS					GetTitle(OpString& title) { return g_languageManager->GetString(Str::S_SPEED_DIAL, title); }
	virtual void						GetThumbnailImage(Image& image);
	virtual bool						HasFixedThumbnailImage() { return false; }
	virtual OP_STATUS					GetTooltipText(OpInfoText& text);
	virtual const OpWidgetImage*		GetFavIcon(BOOL force) { return &m_doc_view_fav_icon; }

	// OpZoomSliderSettings
	virtual double						GetWindowScale();
	virtual void						QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values);

	// OpSpeedDialListener
	virtual void						OnContentChanged(OpSpeedDialView* speeddial);

	// OpWidgetListener
	virtual void						OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	//MessageObject
	virtual void						HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Implementing the OpTreeModelItem interface
	virtual Type						GetType() {return WIDGET_TYPE_SPEEDDIAL;}

	// SpeedDialConfigurationListener
	virtual void						OnSpeedDialConfigurationStarted(const DesktopWindow& window);
	virtual void						OnSpeedDialLayoutChanged() {}
	virtual void						OnSpeedDialColumnLayoutChanged();
	virtual void						OnSpeedDialBackgroundChanged();
	virtual void						OnSpeedDialScaleChanged();

	// SpeedDialListener
	virtual void						OnSpeedDialAdded(const DesktopSpeedDial& entry);
	virtual void						OnSpeedDialRemoving(const DesktopSpeedDial& entry);
	virtual void						OnSpeedDialReplaced(const DesktopSpeedDial& old_entry, const DesktopSpeedDial& new_entry);
	virtual void						OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry);
	virtual void						OnSpeedDialPartnerIDAdded(const uni_char* partner_id) {}
	virtual void						OnSpeedDialPartnerIDDeleted(const uni_char* partner_id) {}
	virtual void						OnSpeedDialsLoaded();

	// OpTimerListener
	virtual void						OnTimeOut(OpTimer* timer);

	// QuickScrollContainerListener
	virtual void						OnContentScrolled(int scroll_current_value, int min, int max);

	// QuickWidgetContainer
	virtual void						OnContentsChanged() { Relayout(); }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindView;}
#endif

	/**
	 * Triggers a discoverabiliy effect for a thumbnail.
	 *
	 * @param pos thumbnail position
	 */
	void								ShowThumbnail(INT32 pos);

	// NOTE: GetSpeedDialThumbnail() must return a valid pointer, or NULL. In particular,
	//       it *must* return NULL on out-of-bounds indices.
	SpeedDialThumbnail*					GetSpeedDialThumbnail(int pos) { return m_thumbnails.Get(pos); }

	/**
	 * Notifies OpSpeedDialView of a mouse down event within a thumbnail.
	 *
	 * @param button the button that was pressed
	 * @param nclicks number of clicks
	 */
	void								OnMouseDownOnThumbnail(MouseButton button, UINT8 nclicks);
	unsigned							GetVisibleThumbnailCount() const;

private:
	class TopLineLayout;

	OP_STATUS							InitSpeedDialHidden(OpWidget* container);
	OP_STATUS							InitSpeedDial(OpWidget* container);

	void								SetCellsSizes();
	void								SetCellSize(int pos);

	void								DoLayout();

	OP_STATUS							CreateThumbnails();
	OP_STATUS							CreateThumbnail(const DesktopSpeedDial& entry, bool animate);

	void								UpdateZoomButton();

	void								StartAdding();
	void								EndAdding();

	inline BOOL							IsReadOnly() const;
	inline bool							IsPlusButtonShown() const;

	/**
	 * @return @c TRUE iff the thumbnail add dialog or extensions pref dialog is currently shown
	 *		for @a thumbnail
	 */
	inline BOOL							IsConfiguring(const SpeedDialThumbnail& thumbnail) const;

	/* Show a configuration dialog to the user */
	OP_STATUS							ConfigureSpeeddial(SpeedDialThumbnail& thumbnail, SpeedDialConfigController* controller);

	/**
	 * Close all open dialogs.
	 */
	void								CloseAllDialogs();

	/* If there is an 'add new dial' dialog shown, close it immediately */
	void								CloseAddDialog();

	/* If there is an 'extension preferences' dialog shown, close it immediately */
	void								CloseExtensionPrefsDialog();

	/* If there is configuration dialog shown, close it immediately */
	void								CloseGlobalConfigDialog();

	void								UpdateButtonVisibility();
	void								UpdateBackground();

	void								ZoomSpeedDial();

	void								NotifyContentChanged();

	void								SetMinimumAndPreferredSizes();

	void								UpdateScale();

	OpVector<SpeedDialThumbnail>		m_thumbnails;
	QuickWidget*						m_search_field;
	QuickWidget*						m_config_button;
	SpeedDialGlobalConfigDialog*		m_global_config_dialog;
	bool								m_adding;
	double								m_scale;

	/**
	 * At most one dial can be configured at a time.
	 * The config dialog can move between dials, though.
	 */
	SpeedDialConfigController*			m_config_controller;
	ExtensionPrefsController*			m_extension_prefs_controller;

	OpListeners<OpSpeedDialListener>	 m_listeners;

	QuickScrollContainer*				m_content;
	QuickFlowLayout*					m_thumbnail_flow;
	OpImageBackground*					m_background;
	SpeedDialThumbnail*					m_focused_thumbnail;
	TopLineLayout*						m_top_line_layout;
	OpTimer								m_reload_on_resize_timer;

	OpButton*							m_speeddial_show;
	OpWidgetImage						m_doc_view_fav_icon;
};

#endif // OP_SPEEDDIAL_H
