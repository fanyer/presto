/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef FILE_CHOOSER_RECEIVER_H
# define FILE_CHOOSER_RECEIVER_H
# ifndef DESKTOP_FILECHOOSER

/**
 * This is a utility class used to easily get to filenames selected by a filechooser.
 *
 * Simple listener that can be put on the stack and to be used
 * with modal file selectors
 */
class FileChooserReceiver : public OpFileChooserListener
{
public:
	/**
	 * Implementation of OpFileChooserListener interface.
	 */
	OP_STATUS OnFileSelected(const uni_char* files)
		{
			return m_files.Set(files);
		}


	/**
	 *
	 */
	const OpString& GetFiles() const { return m_files; }


	/**
	 *
	 */
	OP_STATUS GetFileList(OpAutoVector<OpString>& list)
		{
			const uni_char* p = m_files.CStr();
			while(p)
			{
				const uni_char* start = 0;
				const uni_char* stop = 0;

				start = uni_strchr(p, '"');
				stop = start ? uni_strchr(start+1, '"') : 0;

				if( !start || !stop )
				{
					if( !start && !stop && p == m_files.CStr() )
					{
						// Allow a single non-quoted file as well
						OpString* s = OP_NEW(OpString, ());
						if (s == NULL) return OpStatus::ERR_NO_MEMORY;

						OP_STATUS status1 = s->Set(p);
						OP_STATUS status2 = list.Add(s);
						if (status1 != OpStatus::OK || status2 != OpStatus::OK)
						{
							OP_DELETE(s);
							return OpStatus::ERR_NO_MEMORY;
						}
					}
					break;
				}

				if( stop > start+1 )
				{
					OpString* s = OP_NEW(OpString, ());
					if (s == NULL) return OpStatus::ERR_NO_MEMORY;

					OP_STATUS status1 = s->Set(start+1, stop-start-1 );
					OP_STATUS status2 = list.Add(s);
					if (status1 != OpStatus::OK || status2 != OpStatus::OK)
					{
						OP_DELETE(s);
						return OpStatus::ERR_NO_MEMORY;
					}
				}
				p = stop+1;
			}
			return OpStatus::OK;
		}


	/**
	 * Implementation of OpFileChooserListener interface.
	 */
	void OnCancelled() { }

private:

	OpString m_files;
};
# endif //!DESKTOP_FILECHOOSER
#endif // FILE_CHOOSER_RECEIVER_H
