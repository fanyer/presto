/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef AUTOUPDATECHECKER_H
# define AUTOUPDATECHECKER_H

# include "adjunct/autoupdate/autoupdate_checker/common/common.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/ipc.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/network.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/protocol.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_info.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_utils.h"
# include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/all_includes.h"

using namespace opera_update_checker::ipc;
using namespace opera_update_checker::network;
using namespace opera_update_checker::global_storage;
using namespace opera_update_checker::protocol;
using namespace opera_update_checker::status;

namespace opera_update_checker {
  /** The main checker class. Takes care of checking for the update and communicates with Opera. */
  class OperaAutoupdateChecker : public Network::ResponseObserver, public Request::UpdateDataProvider
  {
  private:
    Channel* channel_;
    GlobalStorage* storage_;
    Network* network_;
    Request* request_;
    Response* response_;
    enum
    {
      MAX_LOCALIZATION_LEN = 16,
      MAX_TIMESTAMP_LEN = 42,
      MAX_OPERA_VERSION_LEN = 64,
      MAX_UPDATE_HOST_LEN = 128,
      MAX_PRODUCT_TYPE_LEN = 129
    };
    char opera_version[MAX_OPERA_VERSION_LEN + 1];
    char firstrun_opera_version[MAX_OPERA_VERSION_LEN + 1];
    char firstrun_opera_timestamp[MAX_TIMESTAMP_LEN + 1];
    char update_host[MAX_UPDATE_HOST_LEN + 1];
    char region[MAX_LOCALIZATION_LEN + 1];
    char detected_localization[MAX_LOCALIZATION_LEN + 1];
    char active_localization[MAX_LOCALIZATION_LEN + 1];
    char user_localization[MAX_LOCALIZATION_LEN + 1];
    char language[MAX_LOCALIZATION_LEN + 1];
    char opera_product_type[MAX_PRODUCT_TYPE_LEN + 1];
    char *path_to_certificate;
    unsigned pipe_id_;
    mutable const char* uuid_;
    mutable const char* lut_;
    unsigned highprio_retries;
    unsigned mediumprio_retries;
    Status Initialize();
    enum State
    {
      INITIALIZED,
      COMPOSED_MAIN_UPDATE,
      COMPOSED_PRODUCT_PART_UPDATE,
      COMMUNICATED_WITH_SERVER,
      STORED_METRICS_DATA,
      SENT_RESPONSE_TO_OPERA,
      // Must be after the rest.
      EMPTY,
      COMMAND_LINE_PROCESSED
    };
    State state_;
    class OperaAutoupdateCheckerCommandLineProcessor
    {
      OperaAutoupdateChecker* checker_;
    public:
      OperaAutoupdateCheckerCommandLineProcessor(OperaAutoupdateChecker* checker);
      /** Parses main(int argc, const char** argv)-like command line.
       * @return false if arguments count is not correct.
       */
      bool ProcessCommandLine(int argc, char** argv);
    };
    friend class OperaAutoupdateCheckerCommandLineProcessor;
    OperaAutoupdateCheckerCommandLineProcessor cmd_processor_;
    Status CheckForUpdate();
    /** Checks for update avilability. */
    Status CheckForUpdate(int argc, char** argv);
    // Increase proper counter and return TRY_AGAIN or FAILED based on that.
    Status OnVeryImportantPartError();
    Status OnImportantPartError();
    /** Try again time got from the server. */
    unsigned try_again_server_time_;
  public:
    OperaAutoupdateChecker();
    ~OperaAutoupdateChecker();
    Status Execute(int argc, char** argv);
    /** Sets info about Opera checking the update i.e. its version. */
    void SetOperaVersion(const char* ver);
    /** Sets info about version of Opera run for the first time. */
    void SetFirstRunOperaVersion(const char* ver);
    /** Sets info about timestamp when Opera was run for the first time. */
    void SetFirstRunOperaTimestamp(const char* timestamp);
    /** Sets info about localization in a format user;detected;active;region e.g. pl;en;pl;eu. */
    void SetLocalizationInfo(char* localization);
    /** Sets info about Opera's language. */
    void SetProductLanguage(char* language);
    /** Sets the autoupdate host to communicate with. */
    void SetUpdateHost(const char* host);
    /** Sets Opera specific request part. */
    void SetOperaProductPart(const char* product_part);
    /** Sets Opera product type. */
    void SetOperaProductType(const char* product_type);
#ifdef NO_OPENSSL
    /** Sets the full path to the certificate which should be used to verify the update host. */
    void SetUpdateHostCertificatePath(const char* path);
#endif // NO_OPENSSL
    /** Sets pipe id needed to create unique pipe name common for both checker and Opera. */
    void SetPipeId(unsigned pipe_id);
    // Network::ResponseObserver's interface
    virtual void OnHeaderReceived(const char* header, unsigned long data_len) { /* NOTHING HERE. */ }
    virtual void OnDataReceived(const char* data, unsigned long data_len);
    // Request::UpdateDataProvider's interface
    virtual const char* GetProductName() const;
    virtual Status GetUUID(const char*&) const;
    virtual Status GetLUT(const char*&) const;
    virtual const char* GetOsName() const;
    virtual const char* GetOsVersion() const;
    virtual const char* GetArch() const;
    virtual const char* GetPackage() const;
    virtual const char* GetFirstRunVersion() const;
    virtual const char* GetFirstRunTimestamp() const;
    virtual const char* GetUserLocation() const;
    virtual const char* GetDetectedLocation() const;
    virtual const char* GetActiveLocation() const;
    virtual const char* GetRegion() const;
    virtual const char* GetProductLanguage() const;
    virtual const char* GetProductVersion() const;
#ifdef _DEBUG
    typedef void (*test_result_cb)(const char* test_name, bool passed);
    void TestCommandLineParsing(int gargc, char** gargv, test_result_cb);
    void TestGlobalStorage(test_result_cb cb);
    void TestProtocol(test_result_cb cb);
    void TestNetwork(test_result_cb cb);
    void TestIPC(test_result_cb cb, bool is_server);
    bool validated_pipe_id;
#endif // _DEBUG
  };
}

#endif // AUTOUPDATECHECKER_H
