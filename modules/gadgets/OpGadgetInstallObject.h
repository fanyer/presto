/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_GADGET_INSTALL_OBJECT_H
#define OP_GADGET_INSTALL_OBJECT_H
#ifdef GADGETS_INSTALLER_SUPPORT

class OpGadgetClass;
class OpGadget;

//////////////////////////////////////////////////////////////////////////
// OpGadgetInstallObject
//////////////////////////////////////////////////////////////////////////

/** Defines an installer for new widgets. The installer can be used to
  * create a temporary file which is then filled with binary data. Once
  * completed the temporary file will be updated with a more suitable filename
  * and installed in OpGadgetManager.
  *
  * First create the installer with OpGadgetInstallObject::Make().
  * Call Open() to allow data to be written to the file, call AppendData()
  * as many times as needed and then finally Close().
  *
  * Call Update() to figure out a better filename for the widget, this is done
  * by reading in the XML file from the widget file.
  *
  * Finally call OpGadgetManager::Install() to install a new widget or
  * OpGadgetManager::Upgrade() to upgrade an existing widget to the new widget
  * file.
  *
  * @note The temporary file is removed when this object is destructed.
  */
class OpGadgetInstallObject
	: public Link
{
public:
	/** Create a new installer object and create a temporary widget filename.
	  * @a type is required for correct handling in OpGadgetManager, it also
	  * determines naming of the temporary file as well as suffix for the
	  * installed file.
	  *
	  * @param new_obj The installer will be created and placed in this pointer.
	  * @param type Determines the type of gadget which is being installed.
	  */
	static OP_STATUS Make(OpGadgetInstallObject *&new_obj, GadgetClass type);

	virtual ~OpGadgetInstallObject();

	/** Open the temporary widget file for writing.
	  * @return OpStatus::OK if opening the file was successful, otherwise the error code from OpFile::Open()
	  */
	OP_STATUS Open();

	/** Appends binary data to the temporary widget.
	  *
	  * @note Must call Open() first.
	  * @return OpStatus::OK if writing to the file was successful, otherwise the error code from OpFile::Write()
	  */
	OP_STATUS AppendData(const ByteBuffer &data);

	/** Closes the temporary widget file that previously open.
	  * @return OpStatus::OK if closing the file was successful, otherwise the error code from OpFile::Close()
	  */
	OP_STATUS Close();

	/** Updates the widget file on disk by reading in the temporary one
	  * and using the widget information to build a new filename.
	  * The widget is then copied to the new filename.
	  *
	  * @note Do not call this while file IsOpen() is @c TRUE.
	  * @return OpStatus::OK if the widget was successfully copied to the new filename,
	  *         otherwise it will return OpStatus::ERR_NO_MEMORY for OOM or a specific error from OpFile.
	  */
	OP_STATUS Update();

	/** Returns @c TRUE if the widget file contains a valid widget.
	  *
	  * @note Do not call this while IsOpen() is @c TRUE.
	  */
	BOOL IsValidWidget();

	/** Return @c TRUE if the temporary file is open for writing, @c FALSE otherwise.
	  */
	BOOL IsOpen() const;

	/** Change whether the widget has been installed or not.
	  * @param is_installed @c TRUE if the widget was installed, @c FALSE otherwise.
	  */
	void SetIsInstalled(BOOL is_installed) { m_is_installed = is_installed; }

	/** Sets the gadget class that was created from the installed widget file.
	  * @param klass The widget which was installed, or @c NULL to reset.
	  */
	void SetGadgetClass(OpGadgetClass *klass) { m_klass = klass; }

	/** Sets the gadget object that is meant to be upgraded.
	  * @param gadget The gadget object which is to be upgraded, or @c NULL to reset.
	  */
	void SetGadget(OpGadget *gadget) { m_gadget = gadget; }

	/** Return the absolute path of the current widget file.
	  * This is either the temporary widget file, or the installed widget file (if Update() has been called).
	  */
	const OpString &GetFullPath() const { return m_path; }

	/** Return the filename of the current widget file.
	  * This is either the temporary widget file, or the installed widget file (if Update() has been called).
	  */
	const OpString &GetFilename() const { return m_filename; }

	/** Return the gadget class that was created as a result of installing the widget.
	  */
	OpGadgetClass *GetGadgetClass() const { return m_klass; }

	/** Return the gadget object that is meant to be upgraded.
	  */
	OpGadget *GetGadget() const { return m_gadget; }

	/** Returns the type of widget this installation is for.
	  */
	GadgetClass GetType() const { return m_type; }

	/** Retrieves the type of URL to use when installing this widget.
	  *
	  * @param[out] url_type The valid URL type that is retrieved.
	  * @return OK if the URL type is valid, ERR otherwise.
	  */
	OP_STATUS GetUrlType(URLContentType &url_type) const;

	/** Return whether the widget has been installed or not.
	  * @return @c TRUE if the widget has been installed, @c FALSE otherwise.
	  */
	BOOL GetIsInstalled() const { return m_is_installed; }

private:
	OpGadgetInstallObject(GadgetClass type);

	OP_STATUS Construct();

	/** Converts a GadgetClass to a URL Type.
	  *
	  * @param[out] url_type The valid URL type that is @a type is converted to.
	  * @param type The type of widget to convert.
	  * @return OK if @a type is valid and convertible, ERR otherwise.
	  */
	static OP_STATUS ToUrlType(URLContentType &url_type, GadgetClass type);

	/** Sanitizes the filename by removing any characters not in the range:
	  * a-z, A-Z, 0-9 or ()_.,- or space.
	  *
	  * If the filename does not contain any valid characters at all it will be
	  * replaced with @a alternative_name.
	  *
	  * @note Do not use this on paths only on the filename.
	  *
	  * @param[out] filename The filename to sanitize
	  * @param alternative_name String containing the alternative name
	  * @return OpStatus::OK if successful or OpStatus::ERR_NO_MEMORY on OOM.
	  */
	static OP_STATUS SanitizeFilename(OpString &filename, const uni_char *alternative_name);

	BOOL m_is_installed;
	OpString m_filename;
	OpString m_path;
	OpFile m_file;
	OpGadgetClass *m_klass;
	OpGadget *m_gadget;
	GadgetClass m_type;
};

#endif // GADGETS_INSTALLER_SUPPORT
#endif // !OP_GADGET_INSTALL_OBJECT_H
