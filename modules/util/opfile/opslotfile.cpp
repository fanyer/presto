/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UTIL_HAVE_OP_SLOTFILE

#include "opslotfile.h"
#include "modules/util/opfile/opsafefile.h"

#include "modules/zlib/zlib.h"

OpSlotFile::OpSlotFile()
	: m_slotsize(0),
	  m_max_nr_slots(0),
	  m_idsize(0),
	  m_version(0),
	  m_slots_start(0),
	  m_readonly(FALSE),
	  m_max_data_size(-1),
	  m_header_loaded(FALSE),
	  m_slots_usage(NULL),
	  m_slot_order_table(NULL)
{
}

OpSlotFile::~OpSlotFile()
{
	EraseIndex();
	delete[] m_slots_usage;
	delete[] m_slot_order_table;
}

OP_STATUS OpSlotFile::Construct(const uni_char* data_path, const uni_char* info_path, INT32 max_data_size, OpFileFolder folder)
{
	OP_STATUS result = m_data_file.Construct(data_path, folder);
	if (OpStatus::IsSuccess(result))
		result = m_info_file.Construct(info_path, folder);
	if (OpStatus::IsSuccess(result))
	{
		if (max_data_size > -1)
			m_max_data_size = max_data_size;
	}

	return result;
}


 
void OpSlotFile::Delete()
{
	m_data_file.Delete(FALSE);
	m_info_file.Delete(FALSE);

	EraseIndex();
}


BOOL OpSlotFile::Exists()
{
	BOOL data = FALSE;
	BOOL info = FALSE;

	m_data_file.Exists(data);
	m_info_file.Exists(info);

	return data & info;
}


OP_STATUS OpSlotFile::Init(UINT32 slotsize, UINT32 nr_slots, int idsize)
{
	if (nr_slots > MAX_NR_SLOTS)
		return OpStatus::ERR_OUT_OF_RANGE;

	RETURN_IF_ERROR(m_data_file.Open(OPFILE_WRITE));

	RETURN_IF_ERROR(RecreateIndex(nr_slots));

	m_slotsize = slotsize;
	m_idsize = idsize;
	m_version = GetVersion();
	m_slots_start = 0;
	m_readonly = FALSE;

	RETURN_IF_ERROR(WriteHeader(slotsize, idsize));
	RETURN_IF_ERROR(WriteDummySlots(m_data_file, nr_slots, slotsize));
	RETURN_IF_ERROR(m_data_file.Close());

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::Open(BOOL read_only)
{
	if(m_data_file.IsOpen())
	{
		if(read_only != m_readonly)
		{
			//change file open mode - close it first
			Close();
		}
		else
		{
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(m_data_file.Open(read_only ? OPFILE_READ : OPFILE_UPDATE));

	if(!m_header_loaded)
	{
		OP_STATUS result = ReadHeader();
		if (OpStatus::IsError(result))
		{
			m_data_file.Close();
			return result;
		}
	}

	m_readonly = read_only;

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::Close(BOOL write_header)
{
	if(!m_data_file.IsOpen())
		return OpStatus::OK;

	if (write_header && !m_readonly)
		RETURN_IF_ERROR(WriteHeader(m_slotsize, m_idsize));
	
	OP_STATUS result1 = m_data_file.Close();

	m_readonly = FALSE;

	if (OpStatus::IsError(result1))
		return result1;

	return OpStatus::OK;
}


OP_STATUS OpSlotFile::Flush()
{
	if(!m_data_file.IsOpen()||m_readonly)
		return OpStatus::OK;

	RETURN_IF_ERROR(WriteHeader(m_slotsize, m_idsize));
	
	// Maybe set a flag that tells header is same on disk and memory

	return OpStatus::OK;
}


OP_STATUS OpSlotFile::CheckValidity(UINT32 nr_slots, UINT32 slotsize, int idsize)
{
	BOOL is_open = m_data_file.IsOpen();
	if(!is_open)
		RETURN_IF_ERROR(Open(TRUE)); //try open file - if fails either files does not exists or is corrupted
		
	OP_STATUS result = OpStatus::OK;

	if (!(nr_slots == m_max_nr_slots && slotsize == m_slotsize && ((idsize != -1 && idsize == m_idsize) || (idsize == -1))))
		result = OpStatus::ERR;

#ifdef SLOTFILE_USES_CRC
	if (OpStatus::IsSuccess(result))
		result = CheckSlotsValidity(FALSE);
#endif // SLOTFILE_USES_CRC
	
	if(!is_open)
		Close();
	
	return result;
}

OP_STATUS OpSlotFile::CheckSlotsValidity(BOOL remove_corrupt)
{
	if (!m_data_file.IsOpen())
		return OpStatus::ERR;

	if (!m_header_loaded)
		return OpStatus::ERR;

	char* buffer = (char*)op_malloc(m_slotsize);
	if (NULL == buffer)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = OpStatus::OK;
	OP_STATUS remove_result = OpStatus::OK;

	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		if (m_slot_order_table[ix] && m_slots_usage[m_slot_order_table[ix]->slot_nr] == SLOT_USED)
		{
			OP_STATUS read_result;
			if (OpStatus::IsError(read_result = ReadSlotData(m_slot_order_table[ix]->slot_nr, buffer, m_slotsize)))
			{
				result = read_result;
			}

			UINT32 crcsize = MIN(m_slotsize, m_slot_order_table[ix]->crcsize);
			UINT32 crc = CalculateCRC(buffer, crcsize);
			if (crc != m_slot_order_table[ix]->crc)
			{
 				result = OpStatus::ERR;
				if (remove_corrupt)
				{
					OP_STATUS tmp_result;
					tmp_result = RemoveSlot(ix);
					if (OpStatus::IsSuccess(remove_result))
						remove_result = tmp_result;
				}
			}
		}
	}

	op_free(buffer);

	if (remove_corrupt)
		return remove_result;
	else
		return result;
}

OP_STATUS OpSlotFile::RemoveCorruptSlots()
{
	BOOL was_open = m_data_file.IsOpen();

	if (!was_open)
	{
		RETURN_IF_ERROR(Open());
	}

	OP_STATUS result = CheckSlotsValidity(TRUE);

	if (!was_open)
	{
		OP_STATUS close_result = Close();
		if (OpStatus::IsSuccess(result))
			result = close_result;
	}

	return result;
}

UINT32 OpSlotFile::CalculateCRC(char* buffer, int size)
{
	UINT32 crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (unsigned char*)buffer, size);
	return crc;
}

OP_STATUS OpSlotFile::WriteHeader(UINT32 slotsize, int idsize)
{
	OpStackAutoPtr<OpSafeFile> safefile(NULL);
	safefile.reset(new OpSafeFile);
	
	if(!safefile.get())
		return OpStatus::ERR_NO_MEMORY;
		
	RETURN_IF_ERROR(safefile->Construct(static_cast<OpFile *>(&m_info_file)));
	
	OpFile * file = safefile.get();
	
	RETURN_IF_ERROR(file->Open(OPFILE_WRITE | OPFILE_TEXT));
	
	OP_STATUS status = file->SetFileLength(0);
	if(OpStatus::IsError(status))
		return status;

	// First write a header and then version
	// NB! NB! NB! These two lines must ALWAYS come first, otherwise backwards compability will be broken, which is not acceptable

	// Write header
	OpString str;
	RETURN_IF_ERROR(str.Set(GetHeaderString()));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	// Write version
	char tmp[128]; // ARRAY OK 2009-02-26 adame
	op_snprintf(tmp, 127, "Version=%d", GetVersion());
	RETURN_IF_ERROR(str.Set(tmp));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	op_snprintf(tmp, 127, "maxslots=%d", m_max_nr_slots);
	RETURN_IF_ERROR(str.Set(tmp));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	op_snprintf(tmp, 127, "slotsize=%d", slotsize);
	RETURN_IF_ERROR(str.Set(tmp));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	op_snprintf(tmp, 127, "idsize=%d", idsize);
	RETURN_IF_ERROR(str.Set(tmp));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	// Write ID table
	RETURN_IF_ERROR(str.Set("[IDTable]"));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	// IDs have to be written in exact same order as CRCs
	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		RETURN_IF_ERROR(str.Set("slot:"));		
			
		if (m_slot_order_table[ix])
		{
			//append slot no
			op_snprintf(tmp, 127, "%d:", m_slot_order_table[ix]->slot_nr);
			RETURN_IF_ERROR(str.Append(tmp));

			RETURN_IF_ERROR(str.Append(m_slot_order_table[ix]->id.CStr()));
		}
		else
		{
			//no more entries
			break;
		}

		RETURN_IF_ERROR(file->WriteUTF8Line(str));
	}

#ifdef SLOTFILE_USES_CRC

	// Write CRCs afterwards, to be backwards compatible during development
	// CRCs have to be written in exact same order as IDs

	RETURN_IF_ERROR(str.Set(""));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));
	RETURN_IF_ERROR(str.Set("[CRCTable]"));
	RETURN_IF_ERROR(file->WriteUTF8Line(str));

	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		if (m_slot_order_table[ix])
		{
			// Set crc
			op_snprintf(tmp, 127, "%d:%d", m_slot_order_table[ix]->crc, m_slot_order_table[ix]->crcsize);
			RETURN_IF_ERROR(str.Set(tmp));
		}
		else
		{
			//no more entries
			break;
		}

		RETURN_IF_ERROR(file->WriteUTF8Line(str));
	}

#endif // SLOTFILE_USES_CRC

	RETURN_IF_ERROR(safefile->SafeClose());

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::ReadHeader()
{
	m_header_loaded = FALSE;

	RETURN_IF_ERROR(m_info_file.Open(OPFILE_READ));

	m_version = 0;
	m_max_nr_slots = 0;
	m_slotsize = 0;
	m_idsize = 0;

	RETURN_IF_ERROR(m_info_file.SetFilePos(0));

	OpString str;
	OpString nrstr;

	// Read header
	RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	if (!(str == GetHeaderString()))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}

	// Read version
	RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	RETURN_IF_ERROR(nrstr.Set(str.SubString(str.FindFirstOf('=') + 1)));
	m_version = uni_atoi(nrstr.CStr());

	if (!IsSupportedVersion(m_version))
		return OpStatus::ERR_PARSING_FAILED;

	RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	if (0 != str.Compare("maxslots=", op_strlen("maxslots=")))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}
	RETURN_IF_ERROR(nrstr.Set(str.SubString(str.FindFirstOf('=') + 1)));
	m_max_nr_slots = uni_atoi(nrstr.CStr());
	m_max_nr_slots = MIN(m_max_nr_slots, MAX_NR_SLOTS);

	RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	if (0 != str.Compare("slotsize=", op_strlen("slotsize=")))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}
	RETURN_IF_ERROR(nrstr.Set(str.SubString(str.FindFirstOf('=') + 1)));
	m_slotsize = uni_atoi(nrstr.CStr());

	RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	if (0 != str.Compare("idsize=", op_strlen("idsize=")))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}
	RETURN_IF_ERROR(nrstr.Set(str.SubString(str.FindFirstOf('=') + 1)));
	m_idsize = uni_atoi(nrstr.CStr());

	// Read ID table
	RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	if (!(str == "[IDTable]"))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}

	RETURN_IF_ERROR(RecreateIndex(m_max_nr_slots));

	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		if(OpStatus::IsSuccess(m_info_file.ReadUTF8Line(str)) && str.Length())
		{
			if (0 != str.Compare("slot:", op_strlen("slot:")))
				return OpStatus::ERR_PARSING_FAILED;	//bad input - maybe file is corupted?
				
			UINT32 slot_no_start = str.FindFirstOf(':') + 1;
			UINT32 slot_no_end = str.FindFirstOf(UNI_L(":"), slot_no_start);

			if (slot_no_end == KNotFound)
				return OpStatus::ERR_PARSING_FAILED;
			
			OpString slot_no;		
			RETURN_IF_ERROR(slot_no.Set(str.SubString(slot_no_start,  slot_no_end - slot_no_start)));
			int slot_number = uni_atoi(slot_no);
			
			if(slot_number >= m_max_nr_slots || slot_number < 0 || str.Length() - (slot_no_end + 1) <= 0)
				return OpStatus::ERR_PARSING_FAILED; //bad slot data

			OpString idstr;

			RETURN_IF_ERROR(idstr.Set(str.SubString(slot_no_end + 1)));
			
			SlotOrder *entry = new SlotOrder;
			if(!entry)
				return OpStatus::ERR_NO_MEMORY;
				
			entry->id.Set(idstr.Strip());
			if(entry->id.CStr() == NULL)
			{
				delete entry;
				return OpStatus::ERR_NO_MEMORY;
			}
				
			entry->slot_nr = slot_number;
			entry->crc = 0;
			entry->crcsize = 0;

			m_slot_order_table[ix] = entry;
			m_slots_usage[slot_number] = SLOT_USED;
		}
		else
		{
			//no more data
			break;
		}
	}

#ifdef SLOTFILE_USES_CRC
	
	// Read any empty lines between IDTable and CRCTable
	while (str.IsEmpty() && !m_info_file.Eof())
	{
		RETURN_IF_ERROR(m_info_file.ReadUTF8Line(str));
	}

	// We can continue even if no CRCTable, so that we can read files from older builds
	if (str == "[CRCTable]")
	{
		
		for (int ix = 0; ix < m_max_nr_slots; ix++)
		{
			if(OpStatus::IsSuccess(m_info_file.ReadUTF8Line(str)) && str.Length())
			{
				int crcsize_start = str.FindFirstOf(':') + 1;
				
				OpString tmp;
				RETURN_IF_ERROR(tmp.Set(str.SubString(0, crcsize_start)));

				UINT32 crc = uni_atoi(tmp);

				RETURN_IF_ERROR(tmp.Set(str.SubString(crcsize_start)));
				UINT32 crcsize = uni_atoi(tmp);

				m_slot_order_table[ix]->crc = crc;
				m_slot_order_table[ix]->crcsize = crcsize;
			}
			else
			{
				//no more data
				break;
			}
		}
	}

#endif // SLOTFILE_USES_CRC

	m_header_loaded = TRUE;
	m_info_file.Close();

	return OpStatus::OK;
}


int OpSlotFile::GetNumFreeSlots()
{
	int num = 0;
	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		if (m_slots_usage[ix] == SLOT_FREE)
			num++;
	}
	return num;
}


int OpSlotFile::GetFreeSlot()
{
	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		if (m_slots_usage[ix] == SLOT_FREE)
			return ix;
	}
	return -1;
}

int OpSlotFile::FindIndex(const char* id)
{
	for (int idx = 0; idx < m_max_nr_slots; idx++)
	{
		if (m_slot_order_table[idx])
		{
			if(m_slot_order_table[idx]->id.Compare(id) == 0)
				return idx;
		}
		else
		{
			break;
		}
	}
	return -1;
}

OP_STATUS OpSlotFile::MoveEntryAfter(const char* id1, const char* id2)
{
	if (NULL == id1)
		return OpStatus::ERR;

	int idx1 = FindIndex(id1);
	int idx2;
	if (id2)
	{
		if ((idx2 = FindIndex(id2)) == -1)
			return OpStatus::ERR;
	}
	else
		idx2 = -1;

	return MoveEntryAfter(idx1, idx2);
}

OP_STATUS OpSlotFile::MoveEntryAfter(int idx_1, int idx_2)
{
	// idx_2 == -1 means move first
	if (idx_1 < 0 || idx_1 >= m_max_nr_slots || idx_2 < -1 && idx_2 >= m_max_nr_slots)
		return OpStatus::ERR;
	
	if(idx_1 == idx_2 + 1)
		return OpStatus::OK;
	
	SlotOrder *tmp = m_slot_order_table[idx_1];
	
	if (idx_2 > idx_1)
	{
		//move down
		for(int i = idx_1; i < idx_2; i++)
		{
			m_slot_order_table[i] = m_slot_order_table[i + 1];
		}
		
		m_slot_order_table[idx_2] = tmp;
	}
	else
	{
		//move up
		for(int i = idx_1; i > idx_2 + 1; i--)
		{
			m_slot_order_table[i] = m_slot_order_table[i - 1];
		}
		
		m_slot_order_table[idx_2 + 1] = tmp;
	}
		
	return OpStatus::OK;
}

OP_STATUS OpSlotFile::ReadSlotData(const char* id, char* buffer, UINT32 buffer_size)
{
	UINT32 idx = FindIndex(id);
	if (idx == -1)
		return OpStatus::ERR;

	return ReadSlotData(m_slot_order_table[idx]->slot_nr, buffer, buffer_size);
}

OP_STATUS OpSlotFile::ReadSlotData(UINT32 slot_no, char* buffer, UINT32 buffer_size)
{
	if (buffer == NULL)
		return OpStatus::ERR_NULL_POINTER;

	if (buffer_size > m_slotsize)
		return OpStatus::ERR;

	if (slot_no >= m_max_nr_slots)
		return OpStatus::ERR;

	//TODO: try to minimize seeks needed by checking current position
	RETURN_IF_ERROR(m_data_file.SetFilePos(m_slots_start));
	RETURN_IF_ERROR(m_data_file.SetFilePos(slot_no * m_slotsize, SEEK_FROM_CURRENT));

	RETURN_IF_ERROR(m_data_file.Read(buffer, buffer_size));

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::WriteSlotData(const char* id, char* buffer, UINT32 buffer_size)
{
	INT32 idx = FindIndex(id);
	INT32 slot = -1;
	
	if (idx == -1)
	{
		slot = GetFreeSlot();
	}
	else
	{
		slot = m_slot_order_table[idx]->slot_nr;
	}
	
	// Full
	if (slot == -1)
	{
 		return OpStatus::ERR_OUT_OF_RANGE;
	}
		
	if(idx == -1)
	{
		idx = 0;
		while(m_slot_order_table[idx] && idx < m_max_nr_slots) idx++;
		
		if(idx == m_max_nr_slots)
			idx = -1;
	}
	
	if(idx == -1)
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
		
	RETURN_IF_ERROR(WriteSlotData(slot, buffer, buffer_size));	
	
	SlotOrder *entry = new SlotOrder;
	if(!entry)
		return OpStatus::ERR_NO_MEMORY;
		
	entry->id.Set(id);
	if(entry->id.CStr() == NULL)
		return OpStatus::ERR_NO_MEMORY;
		
	entry->slot_nr = slot;		
		
	UINT32 crc = CalculateCRC(buffer, buffer_size);

	entry->crc = crc;
	entry->crcsize = buffer_size;

	if(m_slot_order_table[idx])
		delete m_slot_order_table[idx];
		
	m_slot_order_table[idx] = entry;
	m_slots_usage[slot] = SLOT_USED;

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::WriteSlotData(UINT32 slot_no, char* buffer, UINT32 buffer_size)
{
	if(m_readonly)
		return OpStatus::ERR_NO_ACCESS;

	if (buffer == NULL)
		return OpStatus::ERR_NULL_POINTER;

	if (buffer_size > m_slotsize)
		return OpStatus::ERR;

	if (slot_no >= m_max_nr_slots)
		return OpStatus::ERR;

	if (m_max_data_size > -1 && m_slots_start + slot_no * m_slotsize + buffer_size >= m_max_data_size)
		return OpStatus::ERR_OUT_OF_RANGE;

	RETURN_IF_ERROR(m_data_file.SetFilePos(m_slots_start));
	RETURN_IF_ERROR(m_data_file.SetFilePos(slot_no * m_slotsize, SEEK_FROM_CURRENT));

	RETURN_IF_ERROR(m_data_file.Write(buffer, buffer_size));

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::RemoveSlot(const char* id)
{
	return RemoveSlot(FindIndex(id));
}

OP_STATUS OpSlotFile::RemoveSlot(int idx)
{
	if (idx == -1)
		return OpStatus::ERR;

	m_slots_usage[m_slot_order_table[idx]->slot_nr] = SLOT_FREE;

	delete m_slot_order_table[idx];
	m_slot_order_table[idx] = NULL;
	
	for(int i = idx; i < m_max_nr_slots - 1; i++)
	{
		if(m_slot_order_table[i + 1] == NULL)
			break; //no more entries
			
		m_slot_order_table[i] = m_slot_order_table[i + 1];
		m_slot_order_table[i + 1] = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS OpSlotFile::RemoveUnusedSlots(OpVector<OpString8>& id)
{
	for (int idx = 0; idx < m_max_nr_slots; idx++)
	{
		if( m_slot_order_table[idx] )
		{
			BOOL used = FALSE;
			for (UINT32 i=0; i<id.GetCount(); i++)
			{
				OpString8 tmp;
				tmp.Set(m_slot_order_table[idx]->id);

				if( m_slot_order_table[idx]->id.Compare(*id.Get(i)) == 0)
				{
					used = TRUE;
					break;
				}
			}
			if( !used )
			{
				// Remove this entry

				m_slots_usage[m_slot_order_table[idx]->slot_nr] = SLOT_FREE;

				delete m_slot_order_table[idx];
				m_slot_order_table[idx] = NULL;

				for(int i = idx; i < m_max_nr_slots - 1; i++)
				{
					if(m_slot_order_table[i + 1] == NULL)
						break; //no more entries
					
					m_slot_order_table[i] = m_slot_order_table[i + 1];
					m_slot_order_table[i + 1] = NULL;
				}

				idx--; // idx++ in outer for-loop
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS OpSlotFile::WriteDummySlots(OpFile& data_file, UINT32 nr_slots, UINT32 slotsize)
{
	if (!data_file.IsOpen())
		return OpStatus::ERR;

	// Try to write one larger buffer since this is faster and nicer to NAND on some devices
	// Don't allocate larger buffer than 320 kB to be sure not to allocate too much
	char* tmpbuf = NULL;
	if (nr_slots*slotsize < 320*1024 && (tmpbuf = (char*)op_malloc(nr_slots*slotsize)))
	{
		op_memset(tmpbuf, 0, nr_slots*slotsize);
		OP_STATUS result = data_file.Write(tmpbuf, nr_slots*slotsize);
		op_free(tmpbuf);
		return result;
	}
	else
	{	
		char buf[1024]; // ARRAY OK 2009-02-26 adame
		op_memset(buf, 0, 1024);
		
		for (UINT32 ix = 0; ix < nr_slots; ix++)
		{
			for (UINT32 iy = 0 ; iy < (slotsize / 1024) + 1; iy++)
			{
				if(iy == slotsize / 1024)
				{
					RETURN_IF_ERROR(data_file.Write(buf, slotsize % 1024));
				}
				else
				{
					RETURN_IF_ERROR(data_file.Write(buf, 1024));
				}
			}
		}

		return OpStatus::OK;
	}
}

OP_STATUS OpSlotFile::RecreateIndex(int max_nr_slots)
{
	SlotUsage* slots_usage = new SlotUsage[max_nr_slots];
	if (NULL == slots_usage)
		return OpStatus::ERR_NO_MEMORY;
	SlotOrder** slot_order_table = new SlotOrder*[max_nr_slots];
	if (NULL == slot_order_table)
	{
		delete[] slots_usage;
		return OpStatus::ERR_NO_MEMORY;
	}

	for (int ix = 0; ix < max_nr_slots; ix++)
	{
		slots_usage[ix] = SLOT_FREE;
		slot_order_table[ix] = NULL;
	}

	delete[] m_slots_usage;
	m_slots_usage = slots_usage;

	if (m_slot_order_table)
	{
		for (int ix = 0; ix < m_max_nr_slots; ix++)
		{
			delete m_slot_order_table[ix];
		}
		delete[] m_slot_order_table;
	}
	m_slot_order_table = slot_order_table;

	m_max_nr_slots = max_nr_slots;

	m_header_loaded = FALSE;

	return OpStatus::OK;
}

void OpSlotFile::EraseIndex()
{
	for (int ix = 0; ix < m_max_nr_slots; ix++)
	{
		m_slots_usage[ix] = SLOT_FREE;
		delete m_slot_order_table[ix];
		m_slot_order_table[ix] = NULL;
	}
	m_header_loaded = FALSE;
}

int OpSlotFile::GetSlotId(int index, char * buf, int buf_size)
{
	if(index < 0 || index >= m_max_nr_slots || !m_slot_order_table[index])
		return 0;

	return m_slot_order_table[index]->id.UTF8(buf, buf_size);
}

#endif // UTIL_HAVE_OP_SLOTFILE
