/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OPURL_INL_H__
#define __OPURL_INL_H__

inline OpURL &OpURL::operator=(const OpURL &url) { m_url = url.m_url; return *this; }

inline BOOL OpURL::operator==(const OpURL &url) const { return m_url.GetRep()->NameEquals(url.m_url.GetRep()); }

inline BOOL OpURL::IsEmpty() const { return m_url.IsEmpty(); }

inline URLType OpURL::GetType() const { return (URLType) m_url.GetAttribute(URL::KType); }

inline URLType OpURL::GetRealType() const { return (URLType) m_url.GetAttribute(URL::KRealType); }

inline OP_STATUS OpURL::GetTypeName(OpString8 &val) const { return m_url.GetAttribute(URL::KProtocolName, val); }

inline unsigned short OpURL::GetResolvedPort() const { return (unsigned short) m_url.GetAttribute(URL::KResolvedPort); }

inline unsigned short OpURL::GetServerPort() const { return (unsigned short) m_url.GetAttribute(URL::KServerPort); }

inline BOOL OpURL::IsSameServer(OpURL other, OpURL::Server_Check include_port /*= OpURL::KNoPort*/) const  { return m_url.SameServer(other.GetURL(), (URL::Server_Check)include_port); }

inline BOOL OpURL::HasServerName() const { return (BOOL) m_url.GetAttribute(URL::KHaveServerName); }

inline OP_STATUS OpURL::GetPath(OpString8 &val) const { return m_url.GetAttribute(URL::KPath, val); }

inline OP_STATUS OpURL::GetPath(OpString &val, BOOL escaped /*= FALSE*/, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(escaped ? URL::KUniPath_Escaped : URL::KUniPath, charsetID, val); }

inline OP_STATUS OpURL::GetFragmentName(OpString8 &val) const { return m_url.GetAttribute(URL::KFragment_Name, val); }

inline OP_STATUS OpURL::GetFragmentName(OpString &val) const { return m_url.GetAttribute(URL::KUniFragment_Name, val); }

inline OP_STATUS OpURL::GetHostName(OpString8 &val) const { return m_url.GetAttribute(URL::KHostName, val); }

inline OP_STATUS OpURL::GetHostName(OpString &val) const { return m_url.GetAttribute(URL::KUniHostName, val); }

inline OP_STATUS OpURL::GetUsername(OpString8 &val) const { return m_url.GetAttribute(URL::KUserName, val); }

inline OP_STATUS OpURL::GetPassword(OpString8 &val) const { return m_url.GetAttribute(URL::KPassWord, val); }

inline OP_STATUS OpURL::GetQuery(OpString8 &val) const { return m_url.GetAttribute(URL::KQuery, val); }

inline OP_STATUS OpURL::GetQuery(OpString &val) const { return m_url.GetAttribute(URL::KUniQuery_Escaped, val); }

inline OP_STATUS OpURL::GetHostNameAndPort(OpString8 &val) const { OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KHostNameAndPort_L, val)); return result; }

inline OP_STATUS OpURL::GetHostNameAndPort(OpString &val) const {OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KUniHostNameAndPort_L, val)); return result; }

inline OP_STATUS OpURL::GetName(OpString8 &val, BOOL escaped /*= FALSE*/) const { return m_url.GetAttribute(escaped ? URL::KName_Escaped : URL::KName, val); }

inline OP_STATUS OpURL::GetName(OpString &val, BOOL escaped /*= FALSE*/, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(escaped ? URL::KUniName_Escaped : URL::KUniName, charsetID, val); }

inline OP_STATUS OpURL::GetNameWithFragment(OpString8 &val, BOOL escaped /*= FALSE*/) const { return m_url.GetAttribute(escaped ? URL::KName_With_Fragment_Escaped : URL::KName_With_Fragment, val); }

inline OP_STATUS OpURL::GetNameWithFragment(OpString &val, BOOL escaped /*= FALSE*/, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(escaped ? URL::KUniName_With_Fragment_Escaped : URL::KUniName_With_Fragment, charsetID, val); }

inline OP_STATUS OpURL::GetPathAndQuery(OpString &val, BOOL escaped /*= FALSE*/, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(escaped ? URL::KUniPathAndQuery_Escaped : URL::KUniPathAndQuery, charsetID, val); }

inline OP_STATUS OpURL::GetPathAndQuery(OpString8 &val) const { OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KPathAndQuery_L, val)); return result; }

inline OP_STATUS OpURL::GetNameForData(OpString &val, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(URL::KUniName_For_Data, charsetID, val); }

inline OP_STATUS OpURL::GetPathFollowSymlink(OpString &val, BOOL escaped /*= FALSE*/, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(escaped ? URL::KUniPath_FollowSymlink_Escaped : URL::KUniPath_FollowSymlink, charsetID, val); }

inline protocol_selentry *OpURL::GetProtocolScheme() const { protocol_selentry *entry = NULL; m_url.GetAttribute(URL::KProtocolScheme, entry); return entry; }

#ifdef URL_FILE_SYMLINKS
inline OP_STATUS OpURL::GetPathSymlinkTarget(OpString &val, BOOL escaped /*= FALSE*/, unsigned short charsetID /*= 0*/) const { return m_url.GetAttribute(escaped ? URL::KUniPath_SymlinkTarget_Escaped : URL::KUniPath_SymlinkTarget, charsetID, val); }
#endif // URL_FILE_SYMLINKS

#ifdef ERROR_PAGE_SUPPORT
inline OpIllegalHostPage::IllegalHostKind OpURL::GetInvalidURLKind() const { return (OpIllegalHostPage::IllegalHostKind) m_url.GetAttribute(URL::KInvalidURLKind); }
#endif // ERROR_PAGE_SUPPORT

#endif
