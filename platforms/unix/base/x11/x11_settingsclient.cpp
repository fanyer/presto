/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

// Implements client side of the XSettings protocol
// http://standards.freedesktop.org/xsettings-spec/xsettings-spec-0.5.html

#include "core/pch.h"

#include "x11_settingsclient.h"
#include "x11_globals.h"

X11SettingsClient* g_xsettings_client;

//#define DEBUG_SETTINGS_CLIENT

class XSettingsData
{
public:
	XSettingsData(unsigned char* b, unsigned long s) :buffer(b), pos(0), size(s), swap(false) {}

#if defined(DEBUG_SETTINGS_CLIENT)
	void DumpBin()
	{
		for (unsigned long i=0; i<size; i++)
		{
			printf("%02x ", buffer[i]);
			if( i>0 && (i+1)%16 == 0)
				puts("");
		}
		puts("");
	}

	void DumpText()
	{
		for (unsigned long i=0; i<size; i++)
		{
			printf("%c", buffer[i]);
		}
		puts("");
	}
#endif

	BOOL Advance(unsigned long num_bytes)
	{
		if (pos+num_bytes > size)
			return FALSE;
		pos += num_bytes;
		return TRUE;
	}

	BOOL ReadByteOrder()
	{
		UINT8 byte_order;
		if (!ReadUINT8(byte_order))
			return FALSE;
		if (byte_order != MSBFirst && byte_order != LSBFirst)
			return FALSE;

		UINT32 int_flag = 0x01020304;
		UINT8 local_byte_order = (*(UINT8 *)&int_flag == 1) ? MSBFirst : LSBFirst;
		swap = byte_order != local_byte_order;
		return TRUE;
	}

	BOOL ReadUINT8(UINT8& target)
	{
		if (pos+1 > size)
			return FALSE;

		target = *(UINT8 *)&buffer[pos++];
		return TRUE;
	}

	BOOL ReadUINT16(UINT16& target)
	{
		if (pos+2 > size)
			return FALSE;

		UINT16 t = *(UINT16 *)&buffer[pos];
		pos += 2;

		if (swap)
			target = (t << 8) | (t >> 8);
		else
			target = t;
		return TRUE;
	}

	BOOL ReadUINT32(UINT32& target)
	{
		if (pos+4 > size)
			return FALSE;

		UINT32 t = *(UINT32 *)&buffer[pos];
		pos += 4;

		if (swap)
			target = (t << 24) | ((t & 0xff00) << 8) | ((t & 0xff0000) >> 8) | (t >> 24);
		else
			target = t;
		return TRUE;
	}

	BOOL ReadString(OpString8& target, unsigned long num_bytes)
	{
		if (pos+num_bytes > size)
			return FALSE;

		if (!target.Reserve(num_bytes+1))
			return FALSE;

		target.Set((const char*)&buffer[pos], num_bytes);
		pos += num_bytes;

		return TRUE;
	}

private:
	unsigned char* buffer;
	unsigned long pos;
	unsigned long size;
	bool swap;
};


class XSettingsItem
{
public:
	XSettingsItem() : m_id(-1), m_type(X11SettingsClient::TypeUnknown) { m_value.string=0; }
	~XSettingsItem() { if(m_type == X11SettingsClient::TypeString) OP_DELETE(m_value.string); }

	OP_STATUS SetKey(const OpString8 key, INT32 id) {RETURN_IF_ERROR(m_key.Set(key)); m_id = id; return OpStatus::OK;}
	OP_STATUS SetValue(UINT32 value)
	{
		if(m_type == X11SettingsClient::TypeString)
			OP_DELETE(m_value.string);
		m_value.integer = value; m_type = X11SettingsClient::TypeInteger; return OpStatus::OK; 
	}
	OP_STATUS SetValue(const OpString8& value)
	{
		if(m_type == X11SettingsClient::TypeString)
		{
			m_type = X11SettingsClient::X11SettingsClient::TypeUnknown;
			OP_DELETE(m_value.string);
		}
		m_value.string = OP_NEW(OpString8,());
		if(!m_value.string)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(m_value.string->Set(value)); m_type = X11SettingsClient::TypeString; return OpStatus::OK;
	}
	OP_STATUS SetValue(const X11SettingsClient::Color& value)
	{
		if(m_type == X11SettingsClient::TypeString)
		{
			m_type = X11SettingsClient::X11SettingsClient::TypeUnknown;
			OP_DELETE(m_value.string);
		}
		m_value.color = value; m_type = X11SettingsClient::TypeColor; return OpStatus::OK; 
	}

	BOOL Match(const XSettingsItem& item)
	{
		if (m_type != item.m_type)
			return FALSE;
		else if (m_type == X11SettingsClient::TypeInteger)
			return m_value.integer == item.m_value.integer;
		else if (m_type == X11SettingsClient::TypeString)
			return m_value.string->Compare(*item.m_value.string) == 0;
		else if (m_type == X11SettingsClient::TypeColor)
			return m_value.color == item.m_value.color;
		return FALSE;
	}

public:
	OpString8 m_key;
	INT32 m_id;
	X11SettingsClient::DataType m_type;
	union
	{
		UINT32 integer;
		OpString8* string;
		X11SettingsClient::Color color;
	} m_value;
};


// static
OP_STATUS X11SettingsClient::Create()
{
	if (!g_xsettings_client)
	{
		g_xsettings_client = OP_NEW(X11SettingsClient,());
		if (!g_xsettings_client)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS rc = g_xsettings_client->Init();
		if (OpStatus::IsError(rc))
		{
			OP_DELETE(g_xsettings_client);
			g_xsettings_client = 0;
			return rc;
		}
	}
	return OpStatus::OK;
}

// static
void X11SettingsClient::Destroy()
{
	OP_DELETE(g_xsettings_client);
	g_xsettings_client = 0;
}


X11SettingsClient::X11SettingsClient()
	:m_manager_atom(None)
	,m_selection_atom(None)
	,m_xsettings_atom(None)
	,m_root_window(None)
	,m_manager_window(None)
	,m_list(0)
{
}


X11SettingsClient::~X11SettingsClient()
{
	OP_DELETE(m_list);
}


OP_STATUS X11SettingsClient::Init()
{
	X11Types::Display* display = g_x11->GetDisplay();
	int screen = g_x11->GetDefaultScreen();

	OpString8 selection_name;
	RETURN_IF_ERROR(selection_name.AppendFormat("_XSETTINGS_S%d", screen));
	m_manager_atom = XInternAtom(display, "MANAGER", False);
	m_selection_atom = XInternAtom(display, selection_name.CStr(), False);
	m_xsettings_atom = XInternAtom(display, "_XSETTINGS_SETTINGS", False);

	m_root_window = RootWindow(display, screen);

	// Receive MANAGER events
	XWindowAttributes attr;
	XGetWindowAttributes (display, m_root_window, &attr);
	XSelectInput(display, m_root_window, attr.your_event_mask | StructureNotifyMask);

	UpdateManagerWindow();

	// Here we can call @ref UpdateState(), but I prefer to do it from X11DesktopGlobalApplication::OnStart()
	// after listeners have connected. Then they will be notified about the setting values

	return OpStatus::OK;
}


void X11SettingsClient::UpdateManagerWindow()
{
	X11Types::Display* display = g_x11->GetDisplay();

	XGrabServer(display);
	m_manager_window = XGetSelectionOwner(display, m_selection_atom);
	if (m_manager_window != None)
	{
		XSelectInput (display, m_manager_window, PropertyChangeMask | StructureNotifyMask);
	}
	XUngrabServer(display);
	XFlush(display);
}


BOOL X11SettingsClient::HandleEvent(XEvent* event)
{
	if (event->xany.window == m_root_window /*RootWindow (client->display, client->screen)*/)
	{
		if (event->xany.type == ClientMessage)
		{
			XClientMessageEvent* e = (XClientMessageEvent*)event;
			if (e->message_type == m_manager_atom && e->data.l[1] == (long)m_selection_atom)
			{
				UpdateManagerWindow();
				return TRUE;
			}
		}
	}
	else if (event->xany.window == m_manager_window)
	{
		switch (event->xany.type)
		{
			case DestroyNotify:
				m_manager_window = None;
				return TRUE;

			case PropertyNotify:
				ReadXSettings();
				return TRUE;
		}
	}

	return FALSE;
}


void X11SettingsClient::ReadXSettings()
{
	if (m_manager_window == None)
		return;

	X11Types::Display* display = g_x11->GetDisplay();

	X11Types::Atom type;
	int format;
	unsigned long nitems, after;
	unsigned char *data = 0;

	int rc = XGetWindowProperty(display, m_manager_window, m_xsettings_atom, 0, LONG_MAX,
								False, m_xsettings_atom, &type, &format, &nitems, &after, &data);
	if (rc == Success && type != None && data)
	{
		if (type != m_xsettings_atom)
			printf("opera: XSETTINGS property failure (invalid type)\n");
		else if (format != 8)
			printf("opera: XSETTINGS property failure (invalid format)\n");
		else
		{
			XSettingsData settings(data, nitems);
			ParseXSettings(settings);
		}
	}
	if (data)
		XFree (data);
}


void X11SettingsClient::ParseXSettings(XSettingsData& settings)
{
#if defined(DEBUG_SETTINGS_CLIENT)
	settings.DumpBin();
	settings.DumpText();
#endif

	// Header 
	UINT32 serial, num_settings;
	if (!settings.ReadByteOrder() || !settings.Advance(3) || 
		!settings.ReadUINT32(serial) || !settings.ReadUINT32(num_settings))
	{
		printf("opera: failed to parse XSettings header section\n");
		return;
	}

	OpAutoVector<XSettingsItem>* list = OP_NEW(OpAutoVector<XSettingsItem>,());
	if (!list)
	{
		printf("opera: failed to allocate XSettings list\n");
		return;
	}

	// Data
	BOOL error = FALSE;
	for (UINT32 i=0; !error && i<num_settings; i++)
	{
		UINT8 type;
		UINT16 name_size;
		OpString8 name;
		UINT32 last_change_serial;

		if (!settings.ReadUINT8(type) || !settings.Advance(1) || !settings.ReadUINT16(name_size) ||
			!settings.ReadString(name, name_size) || !settings.Advance(name_size%4 ? 4-(name_size%4) : 0) ||
			!settings.ReadUINT32(last_change_serial))
		{
			printf("opera: failed to parse XSettings section %d of %d\n", i, num_settings);
			error = TRUE; continue;
		}

#if defined(DEBUG_SETTINGS_CLIENT)
		printf("%d: key: %s\n", i, name.CStr());
#endif

		switch (type)
		{
			case TypeInteger:
			{
				UINT32 value;
				if (!settings.ReadUINT32(value))
				{
					printf("opera: failed to parse XSettings %d of %d (integer)\n", i, num_settings);
					error = TRUE; break;
				}

				INT32 id = GetId(name);
				if (id >= 0)
				{
					XSettingsItem *item = OP_NEW(XSettingsItem,());
					if (!item || OpStatus::IsError(item->SetKey(name, id)) || OpStatus::IsError(item->SetValue(value)) || OpStatus::IsError(list->Add(item)))
					{
						printf("opera: failed to save XSettings\n");
						error = TRUE; break;
					}
				}
#if defined(DEBUG_SETTINGS_CLIENT)
				printf("%d: value (integer): %d\n", i, value);
#endif
				break;
			}

			case TypeString:
			{
				UINT32 value_size;
				OpString8 value;
				if (!settings.ReadUINT32(value_size) || !settings.ReadString(value, value_size) ||
					!settings.Advance(value_size%4 ? 4-(value_size%4) : 0))
				{
					printf("opera: failed to parse XSettings %d of %d (string)\n", i, num_settings);
					error = TRUE; break;
				}

				INT32 id = GetId(name);
				if (id >= 0)
				{
					XSettingsItem *item = OP_NEW(XSettingsItem,());
					if (!item || OpStatus::IsError(item->SetKey(name, id)) || OpStatus::IsError(item->SetValue(value)) || OpStatus::IsError(list->Add(item)))
					{
						printf("opera: failed to save XSettings\n");
						error = TRUE; break;
					}
				}
#if defined(DEBUG_SETTINGS_CLIENT)
				printf("%d: value (string): %s\n", i, value.CStr());
#endif
				break;
			}

			case TypeColor:
			{
				Color value;
				if (!settings.ReadUINT16(value.red) || !settings.ReadUINT16(value.blue) ||
					!settings.ReadUINT16(value.green) || !settings.ReadUINT16(value.alpha))
				{
					printf("opera: failed to parse XSettings %d of %d (color)\n", i, num_settings);
					error = TRUE; break;
				}

				INT32 id = GetId(name);
				if (id >= 0)
				{
					XSettingsItem *item = OP_NEW(XSettingsItem,());
					if (!item || OpStatus::IsError(item->SetKey(name, id)) || OpStatus::IsError(item->SetValue(value)) || OpStatus::IsError(list->Add(item)))
					{
						printf("opera: failed to save XSettings\n");
						error = TRUE; break;
					}
				}
#if defined(DEBUG_SETTINGS_CLIENT)
				printf("%d: value (color) R:%02x, G:%02x, B:%02x A:%02x\n", i, value.red, value.green, value.blue, value.alpha);
#endif
			}

			default:
				printf("opera: failed to parse XSettings %d of %d (unknown type %d)\n", i, num_settings, type);
				error = TRUE;
		}
	}

	if (error)
		OP_DELETE(list);
	else
	{
		NotifyChanges(list);
	}
}


void X11SettingsClient::NotifyChanges(OpAutoVector<XSettingsItem>* list)
{
	UINT32 max_count = 0;
	if (m_list)
		max_count = MAX(list->GetCount(), m_list->GetCount());
	else
		max_count = list->GetCount();

	for( UINT32 i=0; i<max_count; i++)
	{
		XSettingsItem* new_item = i < list->GetCount() ? list->Get(i) : 0;
		XSettingsItem* old_item = m_list && i < m_list->GetCount() ? m_list->Get(i) : 0;

		int cmp;

		if (old_item && new_item)
			cmp = old_item->m_key.Compare(new_item->m_key);
		else if (old_item)
			cmp = -1;
		else if (new_item)
			cmp = 1;

		XSettingsItem* item = 0;
		int cmd;

		if (cmp == 0)
		{
			if (!old_item->Match(*new_item))
			{
				item = new_item;
				cmd = ResourceChanged;
			}
		}
		else if (cmp < 0)
		{
			item = old_item;
			cmd = ResourceRemoved;
		}
		else if (cmp > 0)
		{
			item = new_item;
			cmd = ResourceAdded;
		}

		if (item)
		{
			if (item->m_type == X11SettingsClient::TypeInteger)
				for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
					m_listeners.GetNext(iterator)->OnResourceUpdated(item->m_id, cmd, item->m_value.integer);
			else if (item->m_type == X11SettingsClient::TypeString)
				for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
					m_listeners.GetNext(iterator)->OnResourceUpdated(item->m_id, cmd, *item->m_value.string);
			else if (item->m_type == X11SettingsClient::TypeColor)
				for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
					m_listeners.GetNext(iterator)->OnResourceUpdated(item->m_id, cmd, new_item->m_value.color);
		}
	}

#if defined(DEBUG_SETTINGS_CLIENT)
	for (UINT32 i=0; i<list->GetCount(); i++)
	{
		XSettingsItem* item = list->Get(i);

		printf("%d: %s ", i, item->m_key.CStr());
		if (item->m_type == X11SettingsClient::TypeString)
			printf("-> %s\n", item->m_value.string->CStr());
		else if (item->m_type == X11SettingsClient::TypeInteger)
			printf("-> %d\n", item->m_value.integer);
		else
			puts("");
	}
#endif

	OP_DELETE(m_list);
	m_list = list;
}

void X11SettingsClient::UpdateState()
{
	OP_DELETE(m_list);
	m_list = 0;
	ReadXSettings();
}


OP_STATUS X11SettingsClient::GetStringValue(INT32 id, OpString8& value)
{
	if (m_list)
	{ 
		UINT32 count = m_list->GetCount();
		for (UINT32 i=0; i<count; i++)
		{
			XSettingsItem* item = m_list->Get(i);
			if (item->m_id == id)
			{
				if (item->m_type == TypeString)
					return value.Set(*item->m_value.string);
				break;
			}
		}
	}
	return OpStatus::ERR;
}


OP_STATUS X11SettingsClient::GetIntegerValue(INT32 id, UINT32& value)
{
	if (m_list)
	{ 
		UINT32 count = m_list->GetCount();
		for (UINT32 i=0; i<count; i++)
		{
			XSettingsItem* item = m_list->Get(i);
			if (item->m_id == id)
			{
				if (item->m_type == TypeInteger)
				{
					value = item->m_value.integer;
					return OpStatus::OK;
				}
				break;
			}
		}
	}
	return OpStatus::ERR;
}


INT32 X11SettingsClient::GetId(const OpString8& name)
{
	if (name.IsEmpty())
		return -1;

	if (name.CStr()[0] == 'G')
	{
		if (!name.Compare("Gtk/CursorThemeName"))
			return GTK_CURSOR_THEME_NAME;
		else if (!name.Compare("Gtk/CursorThemeSize"))
			return GTK_CURSOR_THEME_SIZE;
		else if (!name.Compare("Gtk/MenuBarAcc"))
			return GTK_MENUBAR_ACCELERATOR;
		else if (!name.Compare("Gtk/ColorPalette"))
			return GTK_COLOR_PALETTE;
	}
	if (name.CStr()[0] == 'N')
	{
		if (!name.Compare("Net/CursorBlink"))
			return NET_CURSOR_BLINK;
		else if (!name.Compare("Net/CursorBlinkTime"))
			return NET_CURSOR_BLINK_TIME;
		else if (!name.Compare("Net/DndDragThreshold"))
			return NET_DND_DRAG_THRESHOLD;
		else if (!name.Compare("Net/DoubleClickTime"))
			return NET_DOUBLE_CLICK_TIME;
		else if (!name.Compare("Net/EnableEventSounds"))
			return NET_ENABLE_EVENT_SOUNDS;
		else if (!name.Compare("Net/EnableInputFeedbackSounds"))
			return NET_ENABLE_FEEDBACK_SOUNDS;
	}

	return -1;
}


