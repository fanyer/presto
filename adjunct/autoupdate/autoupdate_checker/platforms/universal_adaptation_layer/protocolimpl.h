/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef PROTOCOL_IMPL_H
# define PROTOCOL_IMPL_H

# include "adjunct/autoupdate/autoupdate_checker/common/common.h"
# include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/tinyxml/tinyxml.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/protocol.h"

class OAUCRequestImpl : public opera_update_checker::protocol::Request
{
private:
  TiXmlDocument document_;
  TiXmlDocument product_res_document_;
  TiXmlPrinter* printer_;
  friend opera_update_checker::protocol::Request* opera_update_checker::protocol::Request::Create();
  static OAUCRequestImpl self_;
public:
  OAUCRequestImpl();
  ~OAUCRequestImpl();
  virtual opera_update_checker::status::Status ComposeMainUpdatePart(const UpdateDataProvider& provider, bool with_metrics);
  virtual opera_update_checker::status::Status SetProductResourcesUpdatePart(const char* raw_data);
  virtual opera_update_checker::status::Status AddProductUpdatePart(const UpdateDataProvider& provider);
  virtual const char* GetAllData();
  virtual void Clear();
};

class OAUCResponseImpl : public opera_update_checker::protocol::Response
{
private:
  TiXmlDocument document_;
  TiXmlPrinter* printer_;
  class ResponseDataFinder : public TiXmlVisitor
  {
    bool find_uuid_;
    bool find_lut_;
    bool find_retry_time_;
    bool in_lut_;
    bool in_uuid_;
    bool in_retry_time_;
    bool has_retry_time_;
    const char* uuid_;
    const char* lut_;
    unsigned retry_time_;
  public:
    enum SearchedItemType
    {
      LUT = 1,
      UUID = 2,
      RETRY_TIME= 4
    };
    ResponseDataFinder(unsigned items);
    virtual bool VisitEnter( const TiXmlElement& element, const TiXmlAttribute* firstAttribute );
    virtual bool VisitExit (const TiXmlElement& element);
    virtual bool Visit (const TiXmlText& element);
    const char* GetUUID() { return uuid_; }
    const char* GetLUT() { return lut_; }
    unsigned HasRetryTime() { return has_retry_time_; }
    unsigned GetRetryTime() { return retry_time_; }
    void Clear() { uuid_ = lut_ = NULLPTR; has_retry_time_ = false; retry_time_ = 0; }
  };
  ResponseDataFinder finder_;
  friend opera_update_checker::protocol::Response* opera_update_checker::protocol::Response::Create();
  static OAUCResponseImpl self_;
  char* data_wr_; // Write pointer into the shared working buffer.
public:
  OAUCResponseImpl();
  ~OAUCResponseImpl();
  virtual opera_update_checker::status::Status AddData(const char* data, unsigned data_len);
  virtual opera_update_checker::status::Status Parse();
  virtual const char* GetUUID();
  virtual const char* GetLUT();
  virtual unsigned GetRetryTime();
  virtual const char* GetAllRawData();
  virtual void Clear();
};

#endif // PROTOCOL_IMPL_H
