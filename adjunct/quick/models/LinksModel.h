/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LINKS_MODEL_H
# define LINKS_MODEL_H

# include "adjunct/desktop_util/treemodel/optreemodel.h"
# include "adjunct/quick/hotlist/HotlistModel.h"
# include "modules/util/OpHashTable.h"

class OpWindowCommander;
class HTML_Element;
class FramesDocument;
class LinksModel;

/***********************************************************************************
**
**	LinksModelItem
**
***********************************************************************************/

class LinksModelItem : public TreeModelItem<LinksModelItem, LinksModel>,
					   public DownloadContext
#ifdef WIC_CAP_URLINFO_REUSE
					   ,public URLInformation::DoneListener
#endif // WIC_CAP_URLINFO_REUSE
{
	public:
							LinksModelItem(OpWindowCommander * wc, 
										   URLInformation * url_info, 
										   const uni_char* url)
							: m_font_color(0), 
							  m_font_weight(0), 
							  m_font_size(0), 
							  m_url_info(url_info), 
							  m_window_commander(wc), 
							  m_init(FALSE), 
							  m_has_true_title(FALSE), 
							  m_last_element(NULL), 
							  m_frames_doc(NULL)
							{
								m_url.Set(url);
#ifdef WIC_CAP_URLINFO_REUSE								
								if (url_info)
									url_info->SetDoneListener(this);
#endif // WIC_CAP_URLINFO_REUSE
							}

		// URLInformation::DoneListener API implementation
#ifdef WIC_CAP_URLINFO_REUSE
		virtual BOOL		OnDone(URLInformation* url_info);
#endif // WIC_CAP_URLINFO_REUSE

		// DonwloadContext API implementation (used in URLInformation methods)
		virtual OpWindowCommander * GetWindowCommander(){ return m_window_commander; }

							LinksModelItem(const uni_char* url) : m_font_color(0), m_font_weight(0), m_font_size(0), m_init(FALSE), m_has_true_title(FALSE), m_last_element(NULL), m_frames_doc(NULL) {m_url.Set(url);}

		virtual				~LinksModelItem();

		virtual OP_STATUS	GetItemData(ItemData* item_data);

		virtual Type		GetType() {return UNKNOWN_TYPE;}
		virtual int			GetID() {return 0;}

		UINT32				m_font_color;
		INT32				m_font_weight;
		INT32				m_font_size;
		URLInformation*		m_url_info;
		OpWindowCommander*	m_window_commander;
		OpString			m_url;
		OpString			m_title;
		BOOL				m_init;
		BOOL				m_has_true_title;
		HTML_Element*		m_last_element;
		FramesDocument*		m_frames_doc;
};

/***********************************************************************************
**
**	LinksModel
**
***********************************************************************************/

class LinksModel : public TreeModel<LinksModelItem>
{
	public:
									LinksModel()  							  
										: m_hash_table(TRUE /* case sensitive */) {}
		virtual						~LinksModel() {}

		void						UpdateLinks(OpWindowCommander* window_commander, BOOL redraw = FALSE);
		void						UpdateLinks(OpWindowCommander* window_commander, FramesDocument* frame_doc, BOOL redraw, INT32 parent, BOOL in_sub_doc);

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

	private:

		OpStringHashTable<LinksModelItem>	m_hash_table;
};

#endif // LINKS_MODEL_H
