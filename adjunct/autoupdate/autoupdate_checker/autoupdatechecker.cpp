/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#include "adjunct/autoupdate/autoupdate_checker/common/common.h"
#include "adjunct/autoupdate/autoupdate_checker/autoupdatechecker.h"
#include "adaptation_layer/global_storage.h"

#include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/all_includes.h"

#include <string>

namespace
{
  const char* AUTOUPDATE_CERTIFICATE_FORMAT = "PEM";
  const char* CERTIFICATE_USED_BY_OPERA_SERVERS =
  "-----BEGIN CERTIFICATE-----\n"\
  "MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs\n"\
  "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"\
  "d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j\n"\
  "ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL\n"\
  "MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3\n"\
  "LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug\n"\
  "RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm\n"\
  "+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW\n"\
  "PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM\n"\
  "xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB\n"\
  "Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3\n"\
  "hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg\n"\
  "EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF\n"\
  "MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA\n"\
  "FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec\n"\
  "nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z\n"\
  "eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF\n"\
  "hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2\n"\
  "Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe\n"\
  "vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep\n"\
  "+OkuE6N36B9K\n"\
  "-----END CERTIFICATE-----\n";
  const char* AUTOUPDATE_CERTIFICATE =
#ifndef NO_OPENSSL
  CERTIFICATE_USED_BY_OPERA_SERVERS
#else
  NULLPTR
#endif // NO_OPENSSL
  ;
  Network::CertificateLocation AUTOUPDATE_CERTIFICATE_LOCATION =
#ifndef NO_OPENSSL
    Network::CERTIFICATE_MEMORY
#else // NO_OPENSSL
    Network::CERTIFICATE_FILE
#endif // NO_OPENSSL
    ;
#ifndef NO_OPENSSL
  const
#endif // NO_OPENSSL
  Network::CertificateInfo DEFAULT_CERTIFICATE_INFO =
  {
    AUTOUPDATE_CERTIFICATE,
    AUTOUPDATE_CERTIFICATE_FORMAT,
    AUTOUPDATE_CERTIFICATE_LOCATION
  };
  const char* AUTOUPDATE_DEFAULT_HOST = "https://autoupdate.geo.opera.com";

  const unsigned MAX_HIGHPRIO_RETRIES = 10;
  const unsigned MAX_MEDIUMPRIO_RETRIES = 5;

  const OAUCTime IPC_CONNECTION_TIMEOUT = 200;
  const OAUCTime IPC_COMMUNICATION_TIMEOUT = 250;

  const char* WHITESPACE = " \t\n\f\r";

  char* ProcessNextArgumentValueAndMoveToNext(char** argv, int& arg_idx)
  {
    char* value = argv[++arg_idx];
    value += strspn(value, WHITESPACE);
    int len = strcspn(value, WHITESPACE);
    value[len] = 0;
    ++arg_idx;
    return value;
  }

  char* ProcessNextArgumentValueWithWSAndMoveToNext(char** argv, int& arg_idx)
  {
    char* value = argv[++arg_idx];
    value += strspn(value, WHITESPACE);
    int val_end_idx = strlen(value) - 1;
    while(strchr(WHITESPACE, value[val_end_idx]))
      --val_end_idx;
    value[val_end_idx + 1] = 0;
    ++arg_idx;
    return value;
  }

  const unsigned HTTP_OK = 200;
}

namespace opera_update_checker
{
  OperaAutoupdateChecker::OperaAutoupdateChecker()
  : channel_(NULLPTR)
  , storage_(NULLPTR)
  , network_(NULLPTR)
  , request_(NULLPTR)
  , response_(NULLPTR)
  , path_to_certificate(NULLPTR)
  , pipe_id_(0)
  , uuid_(NULLPTR)
  , lut_(NULLPTR)
  , highprio_retries(0)
  , mediumprio_retries(0)
  , state_(EMPTY)
  , cmd_processor_(this)
  , try_again_server_time_(0)
#ifdef _DEBUG
  , validated_pipe_id(false)
#endif // _DEBUG
  {
    memset(update_host, 0, sizeof(update_host));
    memset(opera_version, 0, sizeof(opera_version));
    memset(firstrun_opera_version, 0, sizeof(firstrun_opera_version));
    memset(firstrun_opera_timestamp, 0, sizeof(firstrun_opera_timestamp));
    memset(region, 0, sizeof(region));
    memset(active_localization, 0, sizeof(active_localization));
    memset(user_localization, 0, sizeof(user_localization));
    memset(detected_localization, 0, sizeof(detected_localization));
    memset(language, 0, sizeof(language));
    strcpy(opera_product_type, "Opera");
  }

  OperaAutoupdateChecker::~OperaAutoupdateChecker()
  {
    OAUC_DELETEA(path_to_certificate);
    OAUC_DELETEA(uuid_);
    OAUC_DELETEA(lut_);
    GlobalStorage::Destroy(storage_);
    Response::Destroy(response_);
    Request::Destroy(request_);
    Network::Destroy(network_);
    Channel::Destroy(channel_);
  }

  namespace
  {
    template<typename T>
    class AutoDestroyer {
    public:
      AutoDestroyer(T* destroyableObject) :
          destroyableObject_(destroyableObject) {
      }

      ~AutoDestroyer() {
        if (destroyableObject_)
          T::Destroy(destroyableObject_);
      }

        T* Release() {
        T* obj = destroyableObject_;
        destroyableObject_ = NULLPTR;
        return obj;
      }
    private:
      T* destroyableObject_;
    };
  }

  Status OperaAutoupdateChecker::Initialize()
  {
    if (state_ == INITIALIZED)
      return StatusCode::OK;

#ifdef _DEBUG
    OAUC_ASSERT(validated_pipe_id);
#endif // _DEBUG

    channel_ = Channel::Create(false /* Checker is always the client */,
                               pipe_id_,
                               Channel::CHANNEL_MODE_READ_WRITE,
                               Channel::BIDIRECTIONAL);
    if (!channel_)
      return StatusCode::OOM;

    AutoDestroyer<Channel> auto_channel(channel_);

    network_ = Network::Create();
    if (!network_)
      return StatusCode::OOM;

    AutoDestroyer<Network> auto_network(network_);

    request_ = Request::Create();
    if (!request_)
      return StatusCode::OOM;

    AutoDestroyer<Request> auto_request(request_);

    response_ = Response::Create();
    if (!response_)
      return StatusCode::OOM;

    AutoDestroyer<Response> auto_response(response_);

    storage_ = GlobalStorage::Create();
    if (!storage_)
      return StatusCode::OOM;
    storage_->SetOperaProductType(opera_product_type);

    auto_channel.Release();
    auto_network.Release();
    auto_request.Release();
    auto_response.Release();
    state_ = INITIALIZED;
    return StatusCode::OK;
  }

  OperaAutoupdateChecker::OperaAutoupdateCheckerCommandLineProcessor::OperaAutoupdateCheckerCommandLineProcessor(OperaAutoupdateChecker* checker)
  : checker_(checker)
  {}

  bool OperaAutoupdateChecker::OperaAutoupdateCheckerCommandLineProcessor::ProcessCommandLine(int argc, char** argv)
  {
    if (argc > 1) //argv[0] is program's name
    {
      int current_idx = 1;
      while (current_idx < argc)
      {
        char* arg = argv[current_idx];
        if (!arg)
        {
          ++current_idx;
          continue;
        }
        arg += strspn(arg, WHITESPACE);
        if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-version", 8) == 0)
        {
          char* ver = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          checker_->SetOperaVersion(ver);
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-host", 5) == 0)
        {
          char* host = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          checker_->SetUpdateHost(host);
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-productspart", 13) == 0)
        {
          checker_->SetOperaProductPart(argv[++current_idx]);
          ++current_idx;
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-firstrunver", 12) == 0)
        {
          char* firstrunver = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          checker_->SetFirstRunOperaVersion(firstrunver);
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-firstrunts", 11) == 0)
        {
          char* firstrunts = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          checker_->SetFirstRunOperaTimestamp(firstrunts);
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-loc", 4) == 0)
        {
          checker_->SetLocalizationInfo(argv[++current_idx]);
          current_idx++;
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-lang", 5) == 0)
        {
          char* language = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          checker_->SetProductLanguage(language);
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-pipeid", 7) == 0)
        {
          char* pipeid = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          unsigned pipe_id = 0;
          if (pipeid && *pipeid)
          {
#ifdef _DEBUG
            checker_->validated_pipe_id = true;
#endif // _DEBUG
            pipe_id = atoi(pipeid);
          }

          checker_->SetPipeId(pipe_id);
        }
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-producttype", 12) == 0)
        {
          char* opera_product = ProcessNextArgumentValueAndMoveToNext(argv, current_idx);
          checker_->SetOperaProductType(opera_product);
        }
#ifdef NO_OPENSSL
        else if (opera_update_checker::system_utils::SystemUtils::strnicmp(arg, "-certfile", 9) == 0)
        {
          char* cert_file = ProcessNextArgumentValueWithWSAndMoveToNext(argv, current_idx);
          checker_->SetUpdateHostCertificatePath(cert_file);
        }
#endif // NO_OPENSSL
        else
          ++current_idx;
      }
    }
    else
      return false;

    return true;
  }

  Status OperaAutoupdateChecker::OnVeryImportantPartError()
  {
    highprio_retries++;
    return highprio_retries <= MAX_HIGHPRIO_RETRIES ? StatusCode::TRY_AGAIN_LATER : StatusCode::FAILED; /* :-( !!!! */
  }

  Status OperaAutoupdateChecker::OnImportantPartError()
  {
    mediumprio_retries++;
    return mediumprio_retries <= MAX_MEDIUMPRIO_RETRIES ? StatusCode::TRY_AGAIN_LATER : StatusCode::FAILED; /* :-( !!!! */
  }

  Status OperaAutoupdateChecker::CheckForUpdate(int argc, char** argv)
  {
    if (state_ == EMPTY)
    {
      if (!cmd_processor_.ProcessCommandLine(argc, argv))
        return StatusCode::FAILED;
      state_ = COMMAND_LINE_PROCESSED;
    }

    if (state_ == COMMAND_LINE_PROCESSED && Initialize() != StatusCode::OK)
      return OnVeryImportantPartError();
    return CheckForUpdate();
  }

  Status OperaAutoupdateChecker::CheckForUpdate()
  {
    if (!channel_->IsConnected())
      // Connect first so Opera doesn't wait.
      channel_->Connect(IPC_CONNECTION_TIMEOUT);

    try_again_server_time_ = 0;
    if (state_ < COMPOSED_MAIN_UPDATE)
    {
      if (request_->ComposeMainUpdatePart(*this, true) != StatusCode::OK)
        return OnVeryImportantPartError();
      state_ = COMPOSED_MAIN_UPDATE;
    }

    if (state_ < COMPOSED_PRODUCT_PART_UPDATE)
    {
      request_->AddProductUpdatePart(*this); // Important but a bit less important.
      state_ = COMPOSED_PRODUCT_PART_UPDATE;
    }

    State state_before_communication = state_;
    if (state_ < COMMUNICATED_WITH_SERVER)
    {
      response_->Clear(); // Clear the response as the new one will be got on retry.
      const char* use_host = strlen(update_host) > 0 ? update_host : AUTOUPDATE_DEFAULT_HOST;
      bool is_https = strstr(use_host, "https") == use_host;
      Network::RequestData request_data;
      request_data.data = request_->GetAllData();
      request_data.data_length = strlen(request_data.data) + 1;
#ifdef NO_OPENSSL
      if (!DEFAULT_CERTIFICATE_INFO.certificate)
        DEFAULT_CERTIFICATE_INFO.certificate = path_to_certificate;
#endif // NO_OPENSSL
      if (network_->SendHTTPRequest(use_host, is_https, &DEFAULT_CERTIFICATE_INFO, Network::POST, request_data, this) != StatusCode::OK)
        return OnVeryImportantPartError();
      if (network_->GetResponseCode() != HTTP_OK)
        return OnVeryImportantPartError();
      state_ = COMMUNICATED_WITH_SERVER;
    }

    // Store UUID and LUT and send further to Opera.
    if (state_ < STORED_METRICS_DATA)
    {
      const char* uuid = response_->GetUUID();
      const char* lut = response_->GetLUT();
      /* If there is no uuid or lut or server requested to try again
       * reset the state and comunicate again a bit later.
       */
      if (response_->GetRetryTime() != 0 || !lut || !uuid)
      {
        try_again_server_time_ = response_->GetRetryTime();
        state_ = state_before_communication;
        return OnVeryImportantPartError();
      }

      if (storage_->SetData(UUID_KEY, uuid, strlen(uuid)) != StatusCode::OK)
        return OnVeryImportantPartError();
      if (storage_->SetData(LUT_KEY, lut, strlen(lut)) != StatusCode::OK)
        return OnVeryImportantPartError();
      state_ = STORED_METRICS_DATA;
    }

    if (state_ < SENT_RESPONSE_TO_OPERA)
    {
      Message msg;
      msg.type = AUTOUPDATE_STATUS;
      msg.data = const_cast<char*>(response_->GetAllRawData());
      msg.data_len = strlen(msg.data) + 1;
      if (channel_->SendMessage(msg, IPC_COMMUNICATION_TIMEOUT))
        return OnImportantPartError();
      state_ = SENT_RESPONSE_TO_OPERA;
    }

    return state_ <= SENT_RESPONSE_TO_OPERA ? StatusCode::OK : StatusCode::FAILED;
  }

  Status OperaAutoupdateChecker::Execute(int argc, char** argv)
  {
    Status status = StatusCode::OK;
    do
    {
      status = CheckForUpdate(argc, argv);
      if (status == StatusCode::TRY_AGAIN_LATER)
        opera_update_checker::system_utils::SystemUtils::Sleep(try_again_server_time_ != 0 ? try_again_server_time_ : OAUC_PREFFERED_SLEEP_TIME_BETWEEN_RETRIES);
      else
        return status;
    }
    while (true);
  }

  void OperaAutoupdateChecker::SetOperaVersion(const char* info)
  {
    if (!info)
      return;
    OAUC_ASSERT(strlen(info) < MAX_OPERA_VERSION_LEN);
    strncpy(opera_version, info, MAX_OPERA_VERSION_LEN);
  }

  void OperaAutoupdateChecker::SetUpdateHost(const char* host)
  {
    if (!host)
      return;
    OAUC_ASSERT(strlen(host) < MAX_UPDATE_HOST_LEN);
    strncpy(update_host, host, MAX_UPDATE_HOST_LEN);
  }

  void OperaAutoupdateChecker::SetOperaProductPart(const char* product_part)
  {
    if (!product_part)
      return;
    request_->SetProductResourcesUpdatePart(product_part);
  }

#ifdef NO_OPENSSL
  void OperaAutoupdateChecker::SetUpdateHostCertificatePath(const char* cert_file_paths)
  {
    if (!cert_file_paths)
      return;
    path_to_certificate = OAUC_NEWA(char, strlen(cert_file_paths) + 1);
    if (path_to_certificate)
      strcpy(path_to_certificate, cert_file_paths);
  }
#endif // NO_OPENSSL

  void OperaAutoupdateChecker::SetOperaProductType(const char* product_type)
  {
    if (!product_type || strlen(product_type) == 0)
      strcpy(opera_product_type, "Opera");
    else if (strlen(product_type) + strlen("Opera ") < MAX_PRODUCT_TYPE_LEN)
      sprintf(opera_product_type, "Opera %s", product_type);
  }

  void OperaAutoupdateChecker::OnDataReceived(const char* data, unsigned long data_len)
  {
    if (data)
      response_->AddData(data, data_len);
  }

  void OperaAutoupdateChecker::SetFirstRunOperaVersion(const char* firstrunver)
  {
    if (!firstrunver)
      return;
    OAUC_ASSERT(strlen(firstrunver) < MAX_OPERA_VERSION_LEN);
    strncpy(firstrun_opera_version, firstrunver, MAX_OPERA_VERSION_LEN);
  }

  void OperaAutoupdateChecker::SetFirstRunOperaTimestamp(const char* firstrunts)
  {
    if (!firstrunts)
      return;
    OAUC_ASSERT(strlen(firstrunts) < MAX_TIMESTAMP_LEN);
    strncpy(firstrun_opera_timestamp, firstrunts, MAX_TIMESTAMP_LEN);
  }

  void OperaAutoupdateChecker::SetLocalizationInfo(char* localization)
  {
    if (!localization)
      return;

    const int info_cnt = 4;
    char* info[info_cnt];
    info[0] = user_localization;
    info[1] = detected_localization;
    info[2] = active_localization;
    info[3] = region;

    int info_idx = -1;
    bool break_loop = false;
    do
    {
      ++info_idx;
      char* start = localization;
      char* end = strchr(localization, ';');
      start += strspn(start, WHITESPACE);
      if (end)
      {
        *end = 0;
        localization = end + 1;
      }
      else
        break_loop = true;
      int len = strcspn(start, WHITESPACE);
      start[len] = 0;
      OAUC_ASSERT(len < MAX_LOCALIZATION_LEN);
      strncpy(info[info_idx], start, MAX_LOCALIZATION_LEN);
    } while (info_idx < info_cnt && !break_loop);
  }

  void OperaAutoupdateChecker::SetProductLanguage(char* lang)
  {
    if (!lang)
      return;
    OAUC_ASSERT(strlen(lang) < MAX_LOCALIZATION_LEN);
    strncpy(language, lang, MAX_LOCALIZATION_LEN);
  }

  void OperaAutoupdateChecker::SetPipeId(unsigned pipeid)
  {
    pipe_id_ = pipeid;
  }

  const char* OperaAutoupdateChecker::GetProductName() const
  {
    return opera_product_type;
  }

  Status OperaAutoupdateChecker::GetUUID(const char*& uuid) const
  {
    unsigned long len;
    Status status = StatusCode::OK;
    if (!uuid_)
      status = storage_->GetData(UUID_KEY, uuid_, len);
    uuid = uuid_;
    return status;
  }

  Status OperaAutoupdateChecker::GetLUT(const char*& lut) const
  {
    unsigned long len;
    Status status = StatusCode::OK;
    if (!lut_)
      status = storage_->GetData(LUT_KEY, lut_, len);
    lut = lut_;
    return status;
  }

  const char* OperaAutoupdateChecker::GetOsName() const
  {
    return opera_update_checker::system_info::SystemInfo::GetOsName();
  }

  const char* OperaAutoupdateChecker::GetOsVersion() const
  {
    return opera_update_checker::system_info::SystemInfo::GetOsVersion();
  }

  const char* OperaAutoupdateChecker::GetArch() const
  {
    return opera_update_checker::system_info::SystemInfo::GetArch();
  }

  const char* OperaAutoupdateChecker::GetPackage() const
  {
    return opera_update_checker::system_info::SystemInfo::GetPackageType();
  }

  const char* OperaAutoupdateChecker::GetFirstRunVersion() const
  {
    return firstrun_opera_version;
  }

  const char* OperaAutoupdateChecker::GetFirstRunTimestamp() const
  {
    return firstrun_opera_timestamp;
  }

  const char* OperaAutoupdateChecker::GetUserLocation() const
  {
    return user_localization;
  }

  const char* OperaAutoupdateChecker::GetDetectedLocation() const
  {
    return detected_localization;
  }

  const char* OperaAutoupdateChecker::GetActiveLocation() const
  {
    return active_localization;
  }

  const char* OperaAutoupdateChecker::GetRegion() const
  {
    return region;
  }

  const char* OperaAutoupdateChecker::GetProductLanguage() const
  {
    return language;
  }

  const char* OperaAutoupdateChecker::GetProductVersion() const
  {
    return opera_version;
  }

#ifdef _DEBUG
  void OperaAutoupdateChecker::TestCommandLineParsing(int gargc, char** gargv, test_result_cb cb)
  {
    if (state_ == EMPTY)
      Initialize();

    const char* test_name = "COMMAND LINE ARGUMENTS PARSING";

    if (state_ != INITIALIZED)
    {
      cb(test_name, false);
      return;
    }

    if (gargc < 2)
    {
      const char* test_platform_part_prefix = "    -productspart";
      const char* test_platform_part_arg = "    <resources>"
        "<browserjs>1234567890</browserjs>"
        "<spoof>1234567890</spoof>"
        "  <blocklist>1234567890</blocklist>"
        " <handlers-ignore>1234567890</handlers-ignore>"
        "<dictionary-list>1234567890</dictionary-list>"
        "<dictionaries>"
        "<language version=\"1.0\">pl</language>  "
        "<language version=\"2.0\">en</language>  "
        "</dictionaries>"
        "<plugins>"
          "  <plugin mime-type=\"octet/binarystream\" version=\"1.1.1.1.xxxx.yyyy-42\">Awesomenesses</plugin>"
        "</plugins> "
      "</resources>";
      const char* test_host_prefix = "       -host          ";
      const char* test_host_arg = "http://www.opera.com               ";
      const char* test_firstrun_timestamp_prefix = "-firstrunts";
      const char* test_firstrun_timestamp_arg = " 111111111111111111";
      const char* test_firstrun_ver_prefix = "   -firstrunver";
      const char* test_firstrun_ver_arg = "      11.60";
      const char* test_version_prefix = "-version ";
      const char* test_version_arg = "11.65.14678";
      const char* test_loc_prefix = "-loc";
      const char* test_loc_arg = " pl ; en;no;   eu   ";
      const char* test_lang_prefix = "-lang ";
      const char* test_lang_arg = "sk";
      const char* opera_autoupdater = "opera_autoupdater";
      const char* test_pipe_id_prefix = "-pipeid";
      const char* test_pipe_id = "    55553333    ";
      const char* test_product_type_prefix = "-producttype";
      const char* test_product_type = "";
      const char* test_cert_file_prefix = "-certfile";
      const char* test_cert_file = "   ROOT/Tools and Programs/Certificates/cert.pem   ";

      const int argc = 21;
      char* argv[argc];
      argv[0] = OAUC_NEWA(char, strlen(opera_autoupdater) + 1);
      strcpy(argv[0], opera_autoupdater);
      argv[1] = OAUC_NEWA(char, strlen(test_firstrun_ver_prefix) + 1);
      strcpy(argv[1], test_firstrun_ver_prefix);
      argv[2] = OAUC_NEWA(char, strlen(test_firstrun_ver_arg) + 1);
      strcpy(argv[2], test_firstrun_ver_arg);
      argv[3] = OAUC_NEWA(char, strlen(test_host_prefix) + 1);
      strcpy(argv[3], test_host_prefix);
      argv[4] = OAUC_NEWA(char, strlen(test_host_arg) + 1);
      strcpy(argv[4], test_host_arg);
      argv[5] = OAUC_NEWA(char, strlen(test_platform_part_prefix) + 1);
      strcpy(argv[5], test_platform_part_prefix);
      argv[6] = OAUC_NEWA(char, strlen(test_platform_part_arg) + 1);
      strcpy(argv[6], test_platform_part_arg);
      argv[7] = OAUC_NEWA(char, strlen(test_firstrun_timestamp_prefix) + 1);
      strcpy(argv[7], test_firstrun_timestamp_prefix);
      argv[8] = OAUC_NEWA(char, strlen(test_firstrun_timestamp_arg) + 1);
      strcpy(argv[8], test_firstrun_timestamp_arg);
      argv[9] = OAUC_NEWA(char, strlen(test_version_prefix) + 1);
      strcpy(argv[9], test_version_prefix);
      argv[10] = OAUC_NEWA(char, strlen(test_version_arg) + 1);
      strcpy(argv[10], test_version_arg);
      argv[11] = OAUC_NEWA(char, strlen(test_lang_prefix) + 1);
      strcpy(argv[11], test_lang_prefix);
      argv[12] = OAUC_NEWA(char, strlen(test_lang_arg) + 1);
      strcpy(argv[12], test_lang_arg);
      argv[13] = OAUC_NEWA(char, strlen(test_loc_prefix) + 1);
      strcpy(argv[13], test_loc_prefix);
      argv[14] = OAUC_NEWA(char, strlen(test_loc_arg) + 1);
      strcpy(argv[14], test_loc_arg);
      argv[15] = OAUC_NEWA(char, strlen(test_pipe_id_prefix) + 1);
      strcpy(argv[15], test_pipe_id_prefix);
      argv[16] = OAUC_NEWA(char, strlen(test_pipe_id) + 1);
      strcpy(argv[16], test_pipe_id);
      argv[17] = OAUC_NEWA(char, strlen(test_product_type_prefix) + 1);
      strcpy(argv[17], test_product_type_prefix);
      argv[18] = OAUC_NEWA(char, strlen(test_product_type) + 1);
      strcpy(argv[18], test_product_type);
      argv[19] = OAUC_NEWA(char, strlen(test_cert_file_prefix) + 1);
      strcpy(argv[19], test_cert_file_prefix);
      argv[20] = OAUC_NEWA(char, strlen(test_cert_file) + 1);
      strcpy(argv[20], test_cert_file);
      cmd_processor_.ProcessCommandLine(argc, argv);
      for (int cnt = 0; cnt < argc; cnt++)
        OAUC_DELETEA(argv[cnt]);
    }
    else
      cmd_processor_.ProcessCommandLine(gargc, gargv);

    bool passed = strcmp(GetFirstRunVersion(), "11.60") == 0;
    passed &= strcmp(GetFirstRunTimestamp(), "111111111111111111") == 0;
    passed &= strcmp(GetRegion(), "eu") == 0;
    passed &= strcmp(GetUserLocation(), "pl") == 0;
    passed &= strcmp(GetDetectedLocation(), "en") == 0;
    passed &= strcmp(GetActiveLocation(), "no") == 0;
    passed &= strcmp(GetProductLanguage(), "sk") == 0;
    passed &= strcmp(update_host, "http://www.opera.com") == 0;
    passed &= strcmp(opera_version, "11.65.14678") == 0;
    passed &= pipe_id_ == 55553333;
#ifdef NO_OPENSSL
    passed &= strcmp(path_to_certificate, "ROOT/Tools and Programs/Certificates/cert.pem") == 0;
#endif // NO_OPENSSL

    cb(test_name, passed);
  }

  void OperaAutoupdateChecker::TestGlobalStorage(test_result_cb cb)
  {
    if (state_ == EMPTY)
      Initialize();

    const char* test_name = "GLOBAL STORAGE";
    bool passed = false;

    char TEST_UUID_KEY[20];
    char TEST_LUT_KEY[20];
    sprintf(TEST_UUID_KEY, "testuuid%d", opera_update_checker::ipc::GetCurrentProcessId());
    sprintf(TEST_LUT_KEY, "testlut%d", opera_update_checker::ipc::GetCurrentProcessId());
    if (state_ == INITIALIZED)
    {
      unsigned long len;
      const char* uuid = NULLPTR;
      passed = storage_->GetData(TEST_UUID_KEY, uuid, len) == StatusCode::OK;
      passed &= !uuid && len == 0;
      passed &= storage_->SetData(TEST_UUID_KEY, NULL, 0) == StatusCode::OK;
      passed &= storage_->GetData(TEST_UUID_KEY, uuid, len) == StatusCode::OK;
      passed &= !uuid && len == 0;
      passed &= storage_->SetData(TEST_UUID_KEY, "0123456789", 11) == StatusCode::OK;
      passed &= storage_->GetData(TEST_UUID_KEY, uuid, len) == StatusCode::OK;
      passed &= uuid && len == 11 && strcmp(uuid, "0123456789") == 0;
      OAUC_DELETEA(uuid);

      // Overwrite UUID
      uuid = NULLPTR;
      passed &= storage_->SetData(TEST_UUID_KEY, "0100000099", 11) == StatusCode::OK;
      passed &= storage_->GetData(TEST_UUID_KEY, uuid, len) == StatusCode::OK;
      passed &= uuid && len == 11 && strcmp(uuid, "0100000099") == 0;
      OAUC_DELETEA(uuid);

      uuid = NULLPTR;
      passed &= storage_->GetData(TEST_LUT_KEY, uuid, len) == StatusCode::OK;
      passed &= !uuid && len == 0;
      passed &= storage_->SetData(TEST_LUT_KEY, NULL, 0) == StatusCode::OK;
      passed &= storage_->GetData(TEST_LUT_KEY, uuid, len) == StatusCode::OK;
      passed &= !uuid && len == 0;
      passed &= storage_->SetData(TEST_LUT_KEY, "0123456789", 11) == StatusCode::OK;
      passed &= storage_->GetData(TEST_LUT_KEY, uuid, len) == StatusCode::OK;
      passed &= uuid && len == 11 && strcmp(uuid, "0123456789") == 0;
      OAUC_DELETEA(uuid);

      // Overwrite LUT
      uuid = NULLPTR;
      storage_->SetData(TEST_LUT_KEY, "0100000099", 11);
      passed &= storage_->GetData(TEST_LUT_KEY, uuid, len) == StatusCode::OK;
      passed &= uuid && len == 11 && strcmp(uuid, "0100000099") == 0;
      OAUC_DELETEA(uuid);
      // Clean up
      passed &= storage_->SetData(TEST_UUID_KEY, NULL, 0) == StatusCode::OK;
      passed &= storage_->SetData(TEST_LUT_KEY, NULL, 0) == StatusCode::OK;
    }

    cb(test_name, passed);
  }

  namespace
  {
    class TestUpdateDataProvider : public Request::UpdateDataProvider
    {
    public:
      virtual const char* GetProductName() const { return "TESTPRODUCTNAME"; }
      virtual Status GetUUID(const char*& out) const { out = "TESTUUID"; return StatusCode::OK; }
      virtual Status GetLUT(const char*& out) const { out =  "TESTLUT"; return StatusCode::OK; }
      virtual const char* GetOsName() const { return "TESTOSNAME"; }
      virtual const char* GetOsVersion() const { return "TESTOSVERSION"; }
      virtual const char* GetArch() const { return "TESTARCH"; }
      virtual const char* GetPackage() const { return "TESTPACKAGE"; }
      virtual const char* GetFirstRunVersion() const { return "TESTFIRSTRUNVERSION"; }
      virtual const char* GetFirstRunTimestamp() const { return "TESTFIRSTRUNTIMESTAMP"; }
      virtual const char* GetUserLocation() const { return "TESTUSRLOCATION"; }
      virtual const char* GetDetectedLocation() const { return "TESTDETECTED"; }
      virtual const char* GetActiveLocation() const { return "TESTACTIVELOCATION"; }
      virtual const char* GetRegion() const { return "TESTREGION"; }
      virtual const char* GetProductLanguage() const { return "TESTPRODUCTLANGUAGE"; }
      virtual const char* GetProductVersion() const { return "TESTPRODUCTVERSION"; }
    };
  }

  void OperaAutoupdateChecker::TestProtocol(test_result_cb cb)
  {
    if (state_ == EMPTY)
      Initialize();

    const char* test_name = "REQUEST COMPOSITION, RESPONSE PARSING";
    bool passed = false;
    const char* test_resources = "<producttestresurcedata>PASSED</producttestresurcedata>";
    if (state_ == INITIALIZED)
    {
      TestUpdateDataProvider provider;
      request_->Clear();
      passed = request_->SetProductResourcesUpdatePart(test_resources) == StatusCode::OK;
      passed &= request_->ComposeMainUpdatePart(provider, true) == StatusCode::OK;
      passed &= request_->AddProductUpdatePart(provider) == StatusCode::OK;
      const char* data = request_->GetAllData();
      if (!data)
      {
        cb(test_name, false);
        return;
      }

      passed &= strstr(data, provider.GetProductName()) != NULLPTR;
      const char* uuid;
      passed &= provider.GetUUID(uuid) == StatusCode::OK;
      passed &= uuid && strstr(data, uuid) != NULLPTR;
      passed &= provider.GetLUT(uuid) == StatusCode::OK;
      passed &= uuid && strstr(data, uuid) != NULLPTR;
      passed &= strstr(data, provider.GetOsName()) != NULLPTR;
      passed &= strstr(data, provider.GetOsVersion()) != NULLPTR;
      passed &= strstr(data, provider.GetArch()) != NULLPTR;
      passed &= strstr(data, provider.GetPackage()) != NULLPTR;
      passed &= strstr(data, provider.GetFirstRunVersion()) != NULLPTR;
      passed &= strstr(data, provider.GetFirstRunTimestamp()) != NULLPTR;
      passed &= strstr(data, provider.GetUserLocation()) != NULLPTR;
      passed &= strstr(data, provider.GetDetectedLocation()) != NULLPTR;
      passed &= strstr(data, provider.GetActiveLocation()) != NULLPTR;
      passed &= strstr(data, provider.GetProductLanguage()) != NULLPTR;
      passed &= strstr(data, provider.GetRegion()) != NULLPTR;
      passed &= strstr(data, provider.GetProductVersion()) != NULLPTR;
      passed &= strstr(data, test_resources) != NULLPTR;
      request_->Clear();

      data = request_->GetAllData();
      passed &= !data || !*data;

      passed &= request_->AddProductUpdatePart(provider) == StatusCode::OK;
      passed &= request_->ComposeMainUpdatePart(provider, true) == StatusCode::OK;

      data = request_->GetAllData();
      if (!data)
      {
        cb(test_name, false);
        return;
      }

      passed &= strstr(data, provider.GetProductName()) != NULLPTR;
      passed &= provider.GetUUID(uuid) == StatusCode::OK;
      passed &= uuid && strstr(data, uuid) != NULLPTR;
      passed &= provider.GetLUT(uuid) == StatusCode::OK;
      passed &= uuid && strstr(data, uuid) != NULLPTR;
      passed &= strstr(data, provider.GetOsName()) != NULLPTR;
      passed &= strstr(data, provider.GetOsVersion()) != NULLPTR;
      passed &= strstr(data, provider.GetArch()) != NULLPTR;
      passed &= strstr(data, provider.GetPackage()) != NULLPTR;
      passed &= strstr(data, provider.GetFirstRunVersion()) != NULLPTR;
      passed &= strstr(data, provider.GetFirstRunTimestamp()) != NULLPTR;
      passed &= strstr(data, provider.GetUserLocation()) != NULLPTR;
      passed &= strstr(data, provider.GetDetectedLocation()) != NULLPTR;
      passed &= strstr(data, provider.GetActiveLocation()) != NULLPTR;
      passed &= strstr(data, provider.GetProductLanguage()) != NULLPTR;
      passed &= strstr(data, provider.GetRegion()) != NULLPTR;
      passed &= strstr(data, provider.GetProductVersion()) != NULLPTR;
      passed &= strstr(data, test_resources) == NULLPTR;
      request_->Clear();

      response_->Clear();
      data = response_->GetAllRawData();
      passed &= !data || !*data;
      const char *test_response = "<test><response><value><entity><uid>TESTUUID</uid></entity><entity><lut>TESTLUT</lut></entity><entity><retry>1800</retry></entity></value></response></test>";
      passed &= response_->AddData(test_response, strlen(test_response) + 1) == StatusCode::OK;
      passed &= response_->GetUUID() && strcmp(response_->GetUUID(), "TESTUUID") == 0;
      passed &= response_->GetLUT() && strcmp(response_->GetLUT(), "TESTLUT") == 0;
      passed &= response_->GetRetryTime() == 1800;
      response_->Clear();
      data = response_->GetAllRawData();
      passed &= !data || !*data;
      const char *test_response_no_retry = "<test><response><value><entity><uid>TESTUUID</uid></entity><entity><lut>TESTLUT</lut></entity></value></response></test>";
      passed &= response_->AddData(test_response_no_retry , strlen(test_response_no_retry) + 1) == StatusCode::OK;
      passed &= response_->GetRetryTime() == 0;
      response_->Clear();
      data = response_->GetAllRawData();
      passed &= !data || !*data;
    }

    cb(test_name, passed);
  }

  namespace
  {
    class TestNetworkResponseObserver : public Network::ResponseObserver
    {
    public:
      bool passed;
      TestNetworkResponseObserver() : passed(false) {}
      virtual void OnHeaderReceived(const char*, unsigned long) {}
      virtual void OnDataReceived(const char* data, unsigned long data_len)
      {
        passed = strstr(data, "<p>Note: Use your Opera Intranet password when logging in.</p>") != 0;
      }
    };
  }

  void OperaAutoupdateChecker::TestNetwork(test_result_cb cb)
  {
    if (state_ == EMPTY)
      Initialize();

    const char* test_name = "NETWORK";
    bool passed = false;
    if (state_ == INITIALIZED)
    {
      TestNetworkResponseObserver test_observer;
      Network::RequestData data;
      data.data = "Some Data";
      data.data_length = strlen(data.data) + 1;
#ifdef NO_OPENSSL
      const char* test_cert_name = "test_cert.pem";
      if (FILE* file = fopen(test_cert_name, "w"))
      {
        fwrite(CERTIFICATE_USED_BY_OPERA_SERVERS, 1, strlen(CERTIFICATE_USED_BY_OPERA_SERVERS), file);
        fclose(file);
      }
      DEFAULT_CERTIFICATE_INFO.certificate = test_cert_name;
#endif // NO_OPENSSL
      network_->SendHTTPRequest("https://ssl.opera.com", true, &DEFAULT_CERTIFICATE_INFO, Network::GET, data, &test_observer);
      passed = test_observer.passed;
      passed &= network_->GetResponseCode() == HTTP_OK;
#ifdef NO_OPENSSL
      remove(test_cert_name);
#endif // NO_OPENSSL
    }

    cb(test_name, passed);
  }

  void OperaAutoupdateChecker::TestIPC(test_result_cb cb, bool is_server)
  {
    Channel* channel = Channel::Create(is_server, 0, Channel::CHANNEL_MODE_READ_WRITE, Channel::BIDIRECTIONAL);
    AutoDestroyer<Channel> auto_channel(channel);
    if (channel)
    {
      if (channel->Connect(10000) == StatusCode::OK)
      {
        Message testmsg;
        const unsigned max_data_len = 500 * 1024 + 1;
        const char* message_text = "this is a very long message ";
        const unsigned message_len = strlen(message_text);
        testmsg.data = OAUC_NEWA(char, max_data_len);
        if (testmsg.data)
        {
          int i;
          for (i = 0; i + message_len < max_data_len; i += message_len)
            strncpy(&testmsg.data[i], message_text, message_len);
          testmsg.data[++i] = 0;
          unsigned data_len = i;

          testmsg.type = AUTOUPDATE_STATUS;
          testmsg.data_len = data_len;
          testmsg.owns_data = true;

          bool passed = false;
          if (!is_server)
          {
            const Message* msg = channel->GetMessage(10000);
            if (msg && msg->type == AUTOUPDATE_STATUS && strstr(msg->data, message_text) && msg->data_len == data_len)
              passed = channel->SendMessage(testmsg, 10000) == StatusCode::OK;
          }
          else
          {
            passed = channel->SendMessage(testmsg, 10000) == StatusCode::OK;
            const Message* msg = channel->GetMessage(10000);
            if (!msg || msg->type != AUTOUPDATE_STATUS || !strstr(msg->data, message_text) || msg->data_len != data_len)
              passed = false;
          }
          cb(is_server ? "IPC server part" : "IPC client part", passed);
          return;
        }
      }
    }
    cb(is_server ? "IPC server part" : "IPC client part", false);
  }
#endif // _DEBUG
}
