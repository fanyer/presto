/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef ELF_IDENTIFIER_H
#define ELF_IDENTIFIER_H

/** @brief Helper object to identify ELF object files
  * Information taken from http://www.sco.com/developers/gabi/latest/ch4.eheader.html
  */
class ElfIdentifier
{
public:
	/** Initialize with a file to load
	  * @param path Path to file to load
	  * @return OpStatus::OK on success, OpStatus::ERR if the file could
	  * not be loaded or is not an ELF object file, or error values
	  */
	OP_STATUS Init(const char* path);

	enum Class
	{
		ELFCLASSNONE = 0, ///< Unknown class.
		ELFCLASS32 = 1,	///< 32-bit architecture.
		ELFCLASS64 = 2 ///< 64-bit architecture.
	};

	/** @return the class (architecture) for this ELF object file
	  */
	Class GetClass() const { return (Class)m_header.e_ident[EI_CLASS]; }

	enum OSABI
	{
		ELFOSABI_NONE = 0, ///< No extensions or unspecified
		ELFOSABI_HPUX = 1, ///< Hewlett-Packard HP-UX
		ELFOSABI_NETBSD = 2, ///< NetBSD
		ELFOSABI_GNU = 3, ///< GNU
		ELFOSABI_LINUX = 3, ///< Linux historical - alias for ELFOSABI_GNU
		ELFOSABI_SOLARIS = 6, ///< Sun Solaris
		ELFOSABI_AIX = 7, ///< AIX
		ELFOSABI_IRIX = 8, ///< IRIX
		ELFOSABI_FREEBSD = 9, ///< FreeBSD
		ELFOSABI_TRU64 = 10, ///< Compaq TRU64 UNIX
		ELFOSABI_MODESTO = 11, ///< Novell Modesto
		ELFOSABI_OPENBSD = 12, ///< Open BSD
		ELFOSABI_OPENVMS = 13, ///< Open VMS
		ELFOSABI_NSK = 14, ///< Hewlett-Packard Non-Stop Kernel
		ELFOSABI_AROS = 15, ///< Amiga Research OS
		ELFOSABI_FENIXOS = 16 ///< The FenixOS highly scalable multi-core OS
	};

	/** @return The OS ABI for this ELF object file.
	  * @note This field is often left to ELFOSABI_NONE, although it's common
	  * on FreeBSD to have it filled in.
	  */
	OSABI GetOSABI() const { return (OSABI)m_header.e_ident[EI_OSABI]; }

	enum FileType
	{
		ET_NONE = 0, ///< No file type
		ET_REL = 1, ///< Relocatable file
		ET_EXEC = 2, ///< Executable file
		ET_DYN = 3, ///< Shared object file
		ET_CORE = 4 ///< Core file
	};

	/** @return The file type for this ELF object file
	  */
	FileType GetFileType() const { return (FileType)m_header.e_type; }

	enum Machine
	{
		EM_NONE = 0, ///< No machine
		EM_SPARC = 2, ///< SPARC
		EM_386 = 3, ///< Intel 80386
		EM_PPC = 20, ///< PowerPC
		EM_PPC64 = 21, ///< 64-bit PowerPC
		EM_ARM = 40, ///< Advanced RISC Machines ARM
		EM_X86_64 = 62 ///< AMD x86-64 architecture
	};

	/** @return Machine for this ELF object file
	  */
	Machine GetMachine() const { return (Machine)m_header.e_machine; }

private:
	enum IdentIndex
	{
		EI_MAG0 = 0, ///< Magic number, byte 0.
		EI_MAG1 = 1, ///< Magic number, byte 1.
		EI_MAG2 = 2, ///<  Magic number, byte 2.
		EI_MAG3 = 3, ///< Magic number, byte 3.
		EI_CLASS = 4, ///< Class of machine.
		EI_DATA = 5, ///< Data format.
		EI_VERSION = 6, ///< ELF format version.
		EI_OSABI = 7, ///< Operating system / ABI identification
		EI_ABIVERSION = 8, ///< ABI version
		EI_PAD = 9, ///< Start of padding (per SVR4 ABI).
		EI_NIDENT = 16 ///< Size of e_ident[]
	};

	static const unsigned char ELFMAG0 = 0x7f; ///< e_ident[EI_MAG0]
	static const unsigned char ELFMAG1 = 'E'; ///< e_ident[EI_MAG1]
	static const unsigned char ELFMAG2 = 'L'; ///< e_ident[EI_MAG2]
	static const unsigned char ELFMAG3 = 'F'; ///< e_ident[EI_MAG3]

	enum DataEncoding
	{
		ELFDATANONE = 0, ///< Invalid data encoding
		ELFDATA2LSB = 1, ///< Little-endian (least significant byte first)
		ELFDATA2MSB = 2,  ///< Big-endian (most significant byte first)
#ifdef OPERA_BIG_ENDIAN
		ELFDATA_NATIVE = ELFDATA2MSB,
#else
		ELFDATA_NATIVE = ELFDATA2LSB,
#endif // OPERA_BIG_ENDIAN
	};

	struct ElfHeader
	{
		unsigned char e_ident[EI_NIDENT]; // ARRAY OK molsson 2012-01-25
		UINT16 e_type;
		UINT16 e_machine;
		// This was the part that stays the same regardless of whether this is
		// a 32-bit or 64-bit ELF object. there is more information, but we're
		// currently not interested in it.
	};

	ElfHeader m_header;
};

#endif // ELF_IDENTIFIER_H
