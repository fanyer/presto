/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef __PRINT_DIALOG_H__
#define __PRINT_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "platforms/quix/toolkits/ToolkitPrinterHelper.h"

class DestinationInfo;

class PrintDialog : public Dialog
{
public:
	class PrinterState
	{
	public:
		enum PrintRange
		{
			PrintAll=0, PrintSelection, PrintPages
		};

	public:
		PrinterState()
			:destinations(NULL)
			,is_file(false)
			,reverse(false)
			,portrait(true) // Default in print dialog
			,fit_width(false)
			,background(false)
			,header(false)
			,media_width_mm(216)  // Letter
			,media_height_mm(279) // Letter
			,range(PrintAll)
			,from(1)
			,to(9999)
			,copies(1)
			,scale(100)
			// dmm is a deci-mm. Useful because that is the native prefs unit
			,top_dmm(0)
			,bottom_dmm(0)
			,left_dmm(0)
			,right_dmm(0)
			,horizontal_dpi(98)
			,vertical_dpi(98)
		{}

		~PrinterState() { OP_ASSERT(!destinations); }

		OP_STATUS Init(ToolkitPrinterHelper* helper);

		void Reset(ToolkitPrinterHelper* helper) { if(destinations) {helper->ReleaseDestinations(destinations); destinations = NULL;} }

		/**
		 * Writes current state to preferences
		 */
		void WritePrefsL();

	public:
		DestinationInfo** destinations;
		OpString printer_name;
		OpString file;
		bool is_file;
		bool reverse;
		bool portrait;
		bool fit_width;
		bool background;
		bool header;
		INT32 media_width_mm;
		INT32 media_height_mm;
		PrintRange range;
		INT32 from;
		INT32 to;
		INT32 copies;
		INT32 scale;
		// dmm is a deci-mm. Useful because that is the native prefs unit
		INT32 top_dmm;
		INT32 bottom_dmm;
		INT32 left_dmm;
		INT32 right_dmm;
		int horizontal_dpi;
		int vertical_dpi;
	};

	class PrintDialogListener
	{
	public:
		virtual void OnDialogClose(bool accepted) = 0;
	};

private:
	class PaperItem
	{
	public:
		PaperItem()
			:m_width_mm(0)
			,m_height_mm(0)
			,m_name_includes_size(FALSE)
			{}

		/**
		 * Prepare item using data from printer interface
		 *
		 * @param item The printer data

		 * @return OpStatus::OK on success, otherwise an error code describing the error
		 */
		OP_STATUS Init(const DestinationInfo::PaperItem& item);

		/**
		 * Prepare the item using the argumets
		 *
		 * @param name The new name of the item
		 * @param width_mm Paper width in mm
		 * @param height_mm Paper height in mm
		 *
		 * @return OpStatus::OK on success, otherwise an error code describing the error
		 */
		OP_STATUS Init(const OpStringC& name, int width_mm, int height_mm);

		/**
		 * Make a string that can be displayed in UI
		 *
		 * @param text, on return the formatted string
		 *
		 * @return OpStatus::OK on success, otherwise an error code describing the error
		 */
		OP_STATUS MakeUIString(OpString& text);

		/**
		 * Return TRUE if this item contains a media string
		 * that maches the candidate
		 *
		 * @param candidate Text to text
		 *
		 * @return TRUE on a match, otherwise FALSE
		 */
		BOOL HasMedia(const OpString& candidate);

	public:
		OpString m_name;
		UINT32 m_width_mm;
		UINT32 m_height_mm;
		BOOL   m_name_includes_size;
	};

	class PrinterItem
	{
	public:
		PrinterItem()
			:m_active_paper(0)
			,m_copies(1)
			,m_portrait(TRUE)
			{}

		/**
		 * Initalizes a printer item using data from system
		 * 
		 * @param item System data. @ref InitFallbackItem will be called
		 * if item is 0 or the printer name is not set
		 *
		 * @param OpStatus::OK on success, otherwise an error code describing the error
		 */
		OP_STATUS Init(DestinationInfo* info);

	private:
		/**
		 * Initalizes a printer item using fallback data
		 *
		 * @param OpStatus::OK on success, otherwise an error code describing the error
		 */
		OP_STATUS InitFallbackItem();

	public:
		OpString m_printer_name;
		OpString m_printer_location;
		OpAutoVector<PaperItem> m_paper;
		UINT32 m_active_paper;
		UINT32 m_copies;
		BOOL   m_portrait;
	};

public:
	struct SessionItem
	{
		OpString m_printer_name;
		UINT32   m_active_paper;
		BOOL     m_portrait;
	};

public:
	PrintDialog(PrintDialogListener* listener, PrinterState& state);

	virtual void				OnInit();
	virtual void 				OnSetFocus();

	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_PRINT;}
	virtual const char*			GetWindowName()			{return "Print Dialog";}
	virtual BOOL				GetShowPagesAsTabs()	{return TRUE;}

	INT32						GetButtonCount() { return 3; };
	void 						GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	void 						OnChange(OpWidget *widget, BOOL changed_by_mouse);

private:
	/**
	 * Save current dialog state
	 */
	void SaveSettings();

	void SelectPageRangeMode( BOOL use_page_range );
	INT32 GetSelectedDropDownValue(const char* name, int default_value );
	INT32 GetEditValue( const char* name, INT32 default_value, INT32 min, INT32 max );
	INT32 GetMarginValue( const char* name, INT32 default_value, INT32 min, INT32 max );
	void SetMarginValue(const char* name, INT32 value);

	/**
	 * Called whenever the user has change "print to file" state
	 *
	 * @param state TRUE when printing to file, otherwise FALSE
	 */
	void OnPrintToFileChanged(BOOL state);

	/**
	 * Called whenever the selected printer has been changed
	 *
	 * @param printer_index The new printer
	 */
	void OnPrinterChanged(UINT32 printer_index);

	/**
	 * Called whenever the selected paper orientation has been changed
	 *
	 * @param printer_index The active printer
	 * @param portrait TRUE is orientation is portrait, otherwise FALSE
	 */
	void OnOrientationChanged(UINT32 printer_index, BOOL portrait);

	/**
	 * Called whenever the selected paper type has been changed
	 *
	 * @param printer_index The active printer
	 * @param value Index into list of defined paper sizes for active printer
	 */
	void OnPaperChanged(UINT32 printer_index, int value);

	/**
	 * Update column text for the given printer
	 *
	 * @param printer_index The active printer
	 */
	void PrintPaperAndOrientation(UINT32 printer_index);

	/**
	 * Return the first session item that matches the name.
	 * A empty name and a session item with an empty name
	 * will match.
	 *
	 * @param name Printer name to match
	 *
	 * @return Pointer to session data or NULL on no match
	 */
	static SessionItem* FindSessionItem(const OpString& name);

	/**
	 * Save settings of the item to a session list so it
	 * can be restored with @ref GetSessionItem in the current
	 * opera session. At the moment paper size and orientation are saved
	 *
	 * @param item Printer data to be saved
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS SetSessionItem(const PrinterItem& item);

	/**
	 * Look up, using @ref FindSessionItem, saved setting
	 * and assign it to the item argument
	 *
	 * @param item The item to modify if data is present
	 */
	static void GetSessionItem(PrinterItem& item);

private:
	PrinterState& m_state;
	SimpleTreeModel	m_printer_model;
	PrintDialogListener* m_print_dialog_listener;
	OpAutoVector<PrinterItem> m_printer_list;

private:
	static OpAutoVector<SessionItem> m_session_data;

};

#endif
