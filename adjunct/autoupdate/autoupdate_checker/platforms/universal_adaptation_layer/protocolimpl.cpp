/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/protocolimpl.h"
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_utils.h"
#include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/tinyxml/tinystr.h"

namespace opera_update_checker { namespace protocol {

/* static */ Request* Request::Create()
{
  return &OAUCRequestImpl::self_;
}

/* static */ void Request::Destroy(Request* Request)
{
  // Nothing
}

/* static */ Response* Response::Create()
{
  return &OAUCResponseImpl::self_;
}

/* static */ void Response::Destroy(Response* Request)
{
  // Nothing
}

} }

OAUCRequestImpl OAUCRequestImpl::self_;
OAUCResponseImpl OAUCResponseImpl::self_;

namespace
{
  const char* update_request_root = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><autoupdate schema-version=\"2.0\"><system></system><product></product></autoupdate>";
  const char* update_metrics_part_template =
    "<platform>"
      "<opsys>%s</opsys>"
      "<opsys-version>%s</opsys-version>"
      "<arch>%s</arch>"
      "<package>%s</package>"
    "</platform>"
    "<metrics>"
      "<uid>%s</uid>"
      "<lut>%s</lut>"
      "<firstrun-version>%s</firstrun-version>"
      "<firstrun-timestamp>%s</firstrun-timestamp>"
      "<country>"
        "<user>%s</user>"
        "<detected>%s</detected>"
        "<active>%s</active>"
        "<region>%s</region>"
      "</country>"
    "</metrics>";

  const char* update_product_part_template =
    "<name>%s</name>"
    "<version>%s</version>"
    "<language>%s</language>"
    "<edition/>";

   const unsigned MAX_RESPONSE_LEN = 512 * 1024; // Based on worst-case response length.
   char shared_work_buffer[MAX_RESPONSE_LEN];
}

OAUCRequestImpl::OAUCRequestImpl() : printer_(NULLPTR)
{
}

OAUCRequestImpl::~OAUCRequestImpl()
{
  OAUC_DELETE(printer_);
}

/* virtual */ opera_update_checker::status::Status OAUCRequestImpl::ComposeMainUpdatePart(const UpdateDataProvider& provider, bool with_metrics)
{
  try
  {
    if (!document_.RootElement())
      document_.Parse(update_request_root);

    if (document_.Error())
      return opera_update_checker::status::StatusCode::FAILED;

    TiXmlDocument temp;
    TIXML_STRING os(provider.GetOsName() ? provider.GetOsName() : "");
    TIXML_STRING encoded_os;
    TIXML_STRING ver(provider.GetOsVersion() ? provider.GetOsVersion() : "");
    TIXML_STRING encoded_ver;
    TIXML_STRING arch(provider.GetArch() ? provider.GetArch() : "");
    TIXML_STRING encoded_arch;
    TIXML_STRING package(provider.GetPackage() ? provider.GetPackage() : "");
    TIXML_STRING encoded_package;
    const char* cuuid;
    opera_update_checker::status::Status status = provider.GetUUID(cuuid);
    if (status != opera_update_checker::status::StatusCode::OK)
      return status;
    TIXML_STRING uuid(cuuid ? cuuid : "");
    TIXML_STRING encoded_uuid;
    const char* clut;
    status = provider.GetLUT(clut);
    if (status != opera_update_checker::status::StatusCode::OK)
      return status;
    TIXML_STRING lut(clut ? clut : "");
    TIXML_STRING encoded_lut;
    TIXML_STRING firstrunversion(provider.GetFirstRunVersion() ? provider.GetFirstRunVersion() : "");
    TIXML_STRING encoded_firstrunversion;
    TIXML_STRING firstrunts(provider.GetFirstRunTimestamp() ? provider.GetFirstRunTimestamp() : "");
    TIXML_STRING encoded_firstrunts;
    TIXML_STRING user_loc(provider.GetUserLocation() ? provider.GetUserLocation() : "");
    TIXML_STRING encoded_user_loc;
    TIXML_STRING detected_loc(provider.GetDetectedLocation() ? provider.GetDetectedLocation() : "");
    TIXML_STRING encoded_detected_loc;
    TIXML_STRING active_loc(provider.GetActiveLocation() ? provider.GetActiveLocation() : "");
    TIXML_STRING encoded_active_loc;
    TIXML_STRING region(provider.GetRegion() ? provider.GetRegion() : "");
    TIXML_STRING encoded_region;

    document_.EncodeString(os, &encoded_os);
    document_.EncodeString(ver, &encoded_ver);
    document_.EncodeString(arch, &encoded_arch);
    document_.EncodeString(package, &encoded_package);
    document_.EncodeString(uuid, &encoded_uuid);
    document_.EncodeString(lut, &encoded_lut);
    document_.EncodeString(firstrunversion, &encoded_firstrunversion);
    document_.EncodeString(firstrunts, &encoded_firstrunts);
    document_.EncodeString(user_loc, &encoded_user_loc);
    document_.EncodeString(detected_loc, &encoded_detected_loc);
    document_.EncodeString(active_loc, &encoded_active_loc);
    document_.EncodeString(region, &encoded_region);
    sprintf(shared_work_buffer,
            update_metrics_part_template,
            encoded_os.c_str() ? encoded_os.c_str() : "",
            encoded_ver.c_str() ? encoded_ver.c_str() : "",
            encoded_arch.c_str() ? encoded_arch.c_str() : "",
            encoded_package.c_str() ? encoded_package.c_str() : "",
            encoded_uuid.c_str() ? encoded_uuid.c_str() : "",
            encoded_lut.c_str() ? encoded_lut.c_str() : "",
            encoded_firstrunversion.c_str() ? encoded_firstrunversion.c_str() : "",
            encoded_firstrunts.c_str() ? encoded_firstrunts.c_str() : "",
            encoded_user_loc.c_str() ? encoded_user_loc.c_str() : "",
            encoded_detected_loc.c_str() ? encoded_detected_loc.c_str() : "",
            encoded_active_loc.c_str() ? encoded_active_loc.c_str() : "",
            encoded_region.c_str() ? encoded_region.c_str() : "");

    temp.Parse(shared_work_buffer);
    if (temp.Error())
      return opera_update_checker::status::StatusCode::FAILED;

    OAUC_ASSERT(document_.RootElement() && document_.RootElement()->FirstChildElement());
    TiXmlElement* child = document_.RootElement()->FirstChildElement();
    while (opera_update_checker::system_utils::SystemUtils::strnicmp(child->Value(), "system", 6) != 0)
    {
      child = child->NextSiblingElement();
      OAUC_ASSERT(child);
    }

    TiXmlElement* elm = temp.RootElement();
    do
    {
      if (with_metrics || opera_update_checker::system_utils::SystemUtils::strnicmp(elm->Value(), "metrics", 7) != 0)
        child->InsertEndChild(*elm);

      elm = elm->NextSiblingElement();
    }
    while (elm);
  }
  catch (...)
  {
    return opera_update_checker::status::StatusCode::OOM;
  }
  return opera_update_checker::status::StatusCode::OK;
}

/* virtual */ opera_update_checker::status::Status OAUCRequestImpl::SetProductResourcesUpdatePart(const char* raw_data)
{
  try
  {
    product_res_document_.Clear();
    product_res_document_.Parse(raw_data);
    if (product_res_document_.Error())
      return opera_update_checker::status::StatusCode::FAILED;
  }
  catch (...)
  {
    return opera_update_checker::status::StatusCode::OOM;
  }
  return opera_update_checker::status::StatusCode::OK;
}

/* virtual */ opera_update_checker::status::Status OAUCRequestImpl::AddProductUpdatePart(const UpdateDataProvider& provider)
{
  try
  {
    if (!document_.RootElement())
    {
      document_.Parse(update_request_root);
      if (document_.Error())
        return opera_update_checker::status::StatusCode::FAILED;
    }

    TiXmlDocument temp;

    TIXML_STRING name(provider.GetProductName() ? provider.GetProductName() : "");
    TIXML_STRING encoded_name;
    TIXML_STRING ver(provider.GetProductVersion() ? provider.GetProductVersion() : "");
    TIXML_STRING encoded_ver;
    TIXML_STRING lang(provider.GetProductLanguage() ? provider.GetProductLanguage() : "");
    TIXML_STRING encoded_lang;
    document_.EncodeString(name, &encoded_name);
    document_.EncodeString(ver, &encoded_ver);
    document_.EncodeString(lang, &encoded_lang);

    sprintf(shared_work_buffer,
            update_product_part_template,
            encoded_name.c_str() ? encoded_name.c_str() : "",
            encoded_ver.c_str() ? encoded_ver.c_str() : "",
            encoded_lang.c_str() ? encoded_lang.c_str() : "");

      temp.Parse(shared_work_buffer);
      if (temp.Error())
        return opera_update_checker::status::StatusCode::FAILED;

      OAUC_ASSERT(document_.RootElement() && document_.RootElement()->FirstChildElement());
      TiXmlElement* child = document_.RootElement()->FirstChildElement();
      while (opera_update_checker::system_utils::SystemUtils::strnicmp(child->Value(), "product", 7) != 0)
      {
        child = child->NextSiblingElement();
        OAUC_ASSERT(child);
      }

      TiXmlElement* elm = temp.RootElement();
      do
      {
        child->InsertEndChild(*elm);
        elm = elm->NextSiblingElement();
      } while (elm);

      if (TiXmlElement* products = product_res_document_.RootElement())
        child->InsertEndChild(*products);
  }
  catch (...)
  {
    return opera_update_checker::status::StatusCode::OOM;
  }

  return opera_update_checker::status::StatusCode::OK;
}

/* virtual */ const char* OAUCRequestImpl::GetAllData()
{
  OAUC_DELETE(printer_);
  printer_ = OAUC_NEW(TiXmlPrinter, ());
  if (!printer_)
    return NULLPTR;
  document_.Accept(printer_);
  return printer_->CStr();
}

/* virtual */ void OAUCRequestImpl::Clear()
{
  TiXmlNode* elm = document_.FirstChild();
  while (elm)
  {
    TiXmlNode* next = elm->NextSibling();
    document_.RemoveChild(elm);
    elm = next;
  }

  elm = product_res_document_.FirstChild();
  while (elm)
  {
    TiXmlNode* next = elm->NextSibling();
    product_res_document_.RemoveChild(elm);
    elm = next;
  }
  memset(shared_work_buffer, 0, sizeof(shared_work_buffer));
}

OAUCResponseImpl::OAUCResponseImpl() : printer_(NULLPTR), finder_(ResponseDataFinder::LUT | ResponseDataFinder::UUID | ResponseDataFinder::RETRY_TIME)
{
  data_wr_ = shared_work_buffer;
}

OAUCResponseImpl::~OAUCResponseImpl()
{
  OAUC_DELETE(printer_);
}

/* virtual */ opera_update_checker::status::Status OAUCResponseImpl::AddData(const char* data, unsigned data_len)
{
  if (data)
  {
    memcpy(data_wr_, data, data_len);
    data_wr_ += data_len;
    *data_wr_ = 0;
    OAUC_ASSERT(data_wr_ - shared_work_buffer < MAX_RESPONSE_LEN);
  }
  return opera_update_checker::status::StatusCode::OK;
}

/* virtual */ opera_update_checker::status::Status OAUCResponseImpl::Parse()
{
  try
  {
    document_.Parse(shared_work_buffer);
    if (document_.Error())
      return opera_update_checker::status::StatusCode::FAILED;
  }
  catch (...)
  {
    return opera_update_checker::status::StatusCode::OOM;
  }
  return opera_update_checker::status::StatusCode::OK;
}

OAUCResponseImpl::ResponseDataFinder::ResponseDataFinder(unsigned items)
  : find_uuid_((items & UUID) != 0)
  , find_lut_((items & LUT) != 0)
  , find_retry_time_((items & RETRY_TIME) != 0)
  , in_lut_(false)
  , in_uuid_(false)
  , in_retry_time_(false)
  , has_retry_time_(false)
  , uuid_(NULLPTR)
  , lut_(NULLPTR)
  , retry_time_(0)
{
}

/* virtual */ bool OAUCResponseImpl::ResponseDataFinder::VisitEnter( const TiXmlElement& element, const TiXmlAttribute* firstAttribute )
{
  if (find_uuid_ && opera_update_checker::system_utils::SystemUtils::strnicmp(element.Value(), "uid", 3) == 0)
    in_uuid_ = true;
  else if (find_lut_ && opera_update_checker::system_utils::SystemUtils::strnicmp(element.Value(), "lut", 3) == 0)
    in_lut_ = true;
  else if (find_retry_time_ && opera_update_checker::system_utils::SystemUtils::strnicmp(element.Value(), "retry", 5) == 0)
    in_retry_time_ = true;

  return (find_uuid_ && !uuid_) || (find_lut_ && !lut_) || (find_retry_time_ && !has_retry_time_);
}

/* virtual */ bool OAUCResponseImpl::ResponseDataFinder::VisitExit (const TiXmlElement& element)
{
  if (in_uuid_ && opera_update_checker::system_utils::SystemUtils::strnicmp(element.Value(), "uid", 3) == 0)
    in_uuid_ = false;
  else if (in_lut_ && opera_update_checker::system_utils::SystemUtils::strnicmp(element.Value(), "lut", 3) == 0)
    in_lut_ = false;
  else if (find_retry_time_ && opera_update_checker::system_utils::SystemUtils::strnicmp(element.Value(), "retry", 5) == 0)
    in_retry_time_ = false;

  return true;
}

/* virtual */ bool OAUCResponseImpl::ResponseDataFinder::Visit (const TiXmlText& element)
{
  if (in_lut_)
    lut_ = element.Value();
  else if (in_uuid_)
    uuid_ = element.Value();
  else if (in_retry_time_)
  {
    retry_time_ = element.Value() && *element.Value() ? atoi(element.Value()) : 0;
    has_retry_time_ = true;
  }

  return false;
}

/* virtual */ const char* OAUCResponseImpl::GetUUID()
{
  try
  {
    if (!document_.RootElement())
    {
      document_.Parse(shared_work_buffer);

      if (document_.Error())
        return NULLPTR;
    }
  }
  catch (...)
  {
    return NULLPTR;
  }

  if (!finder_.GetUUID())
    document_.Accept(&finder_);
  return finder_.GetUUID();
}

/* virtual */ const char* OAUCResponseImpl::GetLUT()
{
  try
  {
    if (!document_.RootElement())
    {
      document_.Parse(shared_work_buffer);

      if (document_.Error())
        return NULLPTR;
    }
  }
  catch (...)
  {
    return NULLPTR;
  }

  if (!finder_.GetLUT())
    document_.Accept(&finder_);
  return finder_.GetLUT();
}

/* virtual */ unsigned OAUCResponseImpl::GetRetryTime()
{
  try
  {
    if (!document_.RootElement())
    {
      document_.Parse(shared_work_buffer);

      if (document_.Error())
        return 0;
    }
  }
  catch (...)
  {
    return 0;
  }

  if (!finder_.HasRetryTime())
    document_.Accept(&finder_);
  return finder_.GetRetryTime();
}

/* virtual */ const char* OAUCResponseImpl::GetAllRawData()
{
  try
  {
    if (!document_.RootElement())
        document_.Parse(shared_work_buffer);
  }
  catch(...)
  {
  }

  OAUC_DELETE(printer_);
  printer_ = OAUC_NEW(TiXmlPrinter, ());
  if (!printer_)
    return NULLPTR;
  document_.Accept(printer_);
  return printer_->CStr();
}

/* virtual */ void OAUCResponseImpl::Clear()
{
  TiXmlNode* elm = document_.FirstChild();
  while (elm)
  {
    TiXmlNode* next = elm->NextSibling();
    document_.RemoveChild(elm);
    elm = next;
  }

  memset(shared_work_buffer, 0, sizeof(shared_work_buffer));
  data_wr_ = shared_work_buffer;
  finder_.Clear();
}
