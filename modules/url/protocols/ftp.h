/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _FTP_
#define _FTP_

#include "modules/url/protocols/connman.h"

#define FTP_TYPECODE_NULL 0
#define FTP_TYPECODE_A    1
#define FTP_TYPECODE_I    2
#define FTP_TYPECODE_D    3
// 05/10/98 YNP 
#define FTP_TYPECODE_SERVER_EXIST 128 

class FTP;

class FTP_Data : public ProtocolComm {
private: 
	BOOL record_mode;
	BOOL finished;
	char *buffer;
	unsigned char header[3]; /* ARRAY OK 2009-07-08 yngve */
	unsigned int headerpos;
	unsigned int buffer_len;
	unsigned int unread_block;
	BOOL is_marker;
	unsigned recordpos, recordlength;
	
protected:
	
	void    Clear();
	virtual void    ProcessReceivedData();
	virtual void    RequestMoreData(){SetDoNotReconnect(TRUE);};
	
public:
	
	FTP_Data(MessageHandler* msg_handler);
	virtual ~FTP_Data();

	virtual unsigned int   ReadData(char* buf, unsigned int blen);
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	void SetRecordMode(){record_mode = TRUE;};
	virtual void Stop();
	
	void RestartLoading();
	BOOL Finished(){return finished;};
	void StartNext(){finished = FALSE;};
	void SetTransferMode(BOOL rec = TRUE){record_mode = rec;};
};

class FTP_Request;

class FTP_Server_Manager : public Connection_Manager_Element
{
	private:
		BOOL disable_size;
		BOOL disable_mode;
		BOOL disable_mdtm;
		BOOL disable_full_path;
		BOOL disable_cwd_when_size;
		BOOL restart_tested;
		BOOL restart_supported;
		BOOL use_tilde_when_cwd_home;
		BOOL disable_epsv;
		BOOL tested_epsv;

	public:
		FTP_Server_Manager(ServerName *name, unsigned short port_num
//#ifdef _SSL_SUPPORT_
//			, BOOL sec
//#endif
			);
		virtual ~FTP_Server_Manager();

		CommState AddRequest(FTP_Request *);
		void RemoveRequest(FTP_Request *);
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		FTP_Request *GetNewRequest(FTP *conn);

		BOOL GetDisableSize(){return disable_size;}
		BOOL GetDisableMode(){return disable_mode;}
		BOOL GetDisableMDTM(){return disable_mdtm;}
		BOOL GetDisableFullPath(){return disable_full_path;}
		BOOL GetDisableCwdWhenSize(){return disable_cwd_when_size;}
		BOOL GetDisableEPSV(){return disable_epsv;}
		BOOL GetTestedEPSV(){return tested_epsv;}
		BOOL GetRestartTested(){return restart_tested;}
		BOOL GetRestartSupported(){return restart_supported;}
		BOOL GetUseTildeWhenCwdHome(){return use_tilde_when_cwd_home;}
		void DisableSize(){disable_size = TRUE;}
		void DisableMode(){disable_mode= TRUE;}
		void DisableMDTM(){disable_mdtm = TRUE;}
		void DisableFullPath(){disable_full_path= TRUE;}
		void DisableCwdWhenSize(){disable_cwd_when_size= TRUE;}
		void DisableEPSV(){disable_epsv= TRUE;}
		void SetTestedEPSV(){tested_epsv= TRUE;}
		void SetRestartTested(BOOL rest_supp){restart_tested = TRUE; restart_supported = rest_supp;}
		void SetUseTildeWhenCwdHome(){use_tilde_when_cwd_home= TRUE;}
		virtual BOOL Preserve() { return HostName()->IsHostResolved(); }
};

class FTP_Manager : public Connection_Manager
{

	public:
		FTP_Server_Manager *FindServer(ServerName *name, unsigned short port_num,
//#ifdef _SSL_SUPPORT_
//			BOOL sec,
//#endif

			BOOL create = FALSE);
};


class FTP_Connection : public Connection_Element
{
	public:
		FTP *conn;
		OpString8 user_name;

	public:
		FTP_Connection(FTP *nconn){conn = nconn;};
		virtual ~FTP_Connection();

		virtual BOOL Idle();
		virtual BOOL AcceptNewRequests();
		virtual unsigned int Id();
		virtual BOOL SafeToDelete();
		virtual BOOL HasId(unsigned int sid);
		virtual void SetNoNewRequests();

};


class FTP : public ProtocolComm {
	private:
		FTP_Request *ftp_request;
		
		FTP_Server_Manager *manager;
		FTP_Data* data_conn;
		int     data_conn_status;
		enum {FTP_MODE_STREAM, FTP_MODE_BLOCK} datamode;
		
		int		current_typecode;
		int		what_to_send;

		char*   reply_buf;
		char*   reply_buf_next;
		int     reply_buf_len;
		int     reply_buf_loaded;

		int     error;

		OpString8	cwd;
		OpString8	new_cwd;
		OpString8	m_start_path;
		bool	has_start_path;
		char*   ftp_path;

		char*   pathname; // pointer into ftp_path - DON'T DELETE THIS
		char*   next_dir; // pointer into ftp_path - DON'T DELETE THIS
		int     path_status;
		
		BOOL    header_loaded_sent;

		BOOL disable_size;
		BOOL disable_mode;
		BOOL tried_full_path;
		BOOL disable_mdtm;
		BOOL disable_full_path;
		BOOL tested_restart;
		BOOL restart_supported;

		BOOL need_transfer_finished;
		BOOL received_transfer_finished;
		BOOL disable_cwd_when_size;
		BOOL connection_active;
		BOOL accept_requests;
		
		char*   request;  // 02/04/97  YNP
		int     request_buf_len;
		
		time_t	last_active;
		
		void	SendCommand();
		void    CheckRequestBufL(int min_len);
		
		const char* GetNextDir(int& len);
		void    ResetPathParser();
		BOOL    ParsePath();
		BOOL    MoreDirs();
		
		int     CheckReply();
		void	MoveToNextReply();
		BOOL    GetPasvHostAndPort(int& h1, int& h2, int &h3, int& h4, int& p);
		
		void    MakeDirMsg();
		
		void    DeleteDataConn(BOOL force);
		void	BuildCommandL(int &len, const char *command, const char *parameter, BOOL replace_escaped, BOOL add_intial_slash= FALSE);
		int		DecideToSendTypeOrMode(int action);

		void	StartNextRequest();
		
	protected:
		
		virtual void	RequestMoreData();
		char*   ComposeRequestL(int& len);
		void    Clear();
		virtual void    ProcessReceivedData();
		virtual void	SecondaryProcessReceivedData();
		virtual BOOL    PendingClose() const;
		//int     PendingError();

	private:
		void InternalInit();
		void InternalDestruct();
		
	public:
		
		FTP(MessageHandler* msg_handler);

		virtual ~FTP();
		
		const char* GetUser(BOOL not_default);
		void  SetUser(const char* val);
		void  SetPassword(const char* val);
		
		virtual void  Stop();
		
		virtual unsigned int   ReadData(char* buf, unsigned int blen);
		
		const char* GetPath() { return ftp_path; };
		
		virtual void  HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		virtual OP_STATUS  SetCallbacks(MessageObject* master, MessageObject* parent);		
		BOOL SameAuthorization(const char* auth_user, const char* auth_pass);
		
		virtual BOOL SafeToDelete() const;
		BOOL Idle() const;
		BOOL AcceptNewRequests(){return accept_requests;};
		BOOL SetNewRequest(FTP_Request *, BOOL autostart = TRUE);
		BOOL MatchCWD(const OpStringC8 &);

		BOOL HasStartPath() const { return has_start_path; }
		const char* GetStartPath() const { return m_start_path.CStr(); }

		int MapStatusToAction(int status, int &error);
		int StartDirActions();

		void SetManager(FTP_Server_Manager *mgr);

		time_t LastActive(){ return (ftp_request == NULL ? last_active : 0);};

		int ActivatePassiveConnection(ServerName *data_server, unsigned short data_port, int &error);

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		/* Pause a FTP request by stop doing recv() on the socket,
		loading can be resumed without issuing further commands. */
		void PauseLoading();
		/* Continues loading a FTP request which has been paused with PauseLoading(). */
		OP_STATUS ContinueLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
};


class FTP_Request: public ProtocolComm, public Link
{
	friend class FTP_Server_Manager;
	friend class FTP;

	private:
		FTP *ftp_conn;
		OpSmartPointerNoDelete<FTP_Server_Manager> master;

		BOOL used;

		OpString8	path;
		OpString8	username;
		OpString8	password;
		OpString8	m_start_path;
		int typecode;
#ifdef FTP_RESUME
		BOOL use_resume;
		BOOL using_resume;
		OpFileLength resume_pos;
		OpString8 mdtm_date;
#endif
#ifdef _ENABLE_AUTHENTICATE
		unsigned long auth_id;
#endif

		OpString8	dir_msg;
		OpFileLength file_size;

		OpSocketAddressNetType use_nettype; // Which nettypes are permitted

	public:
		FTP_Request(MessageHandler* msg_handler);
		OP_STATUS Construct(
			ServerName *server,
			unsigned short port,
			const OpStringC8 &pth, 
			const OpStringC8 &usr_name,
			OpStringS8 &passw, 
			int typecode 
		 );

		virtual ~FTP_Request();
		virtual void  HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		virtual OP_STATUS  SetCallbacks(MessageObject* master, MessageObject* parent);

		virtual CommState	Load();
		virtual CommState	ConnectionEstablished() { return COMM_LOADING; }
		virtual void		SendData(char *buf, unsigned blen){}; // buf is always freed by the handling object
		virtual void		RequestMoreData(){};
		virtual unsigned	ReadData(char* buf, unsigned blen){return (ftp_conn ? ftp_conn->ReadData(buf, blen) : 0);};
		virtual void		EndLoading(){if(InList() || ftp_conn) master->RemoveRequest(this);};

		/**
		 * Sets the "home folder" for this site.
		 *
		 * If assignment can't be done, the string is set to "". The fallback in this case
		 * will be to try "CWD" or "CWD ~" to go to home folder (which might fail on some servers).
		 *
		 * @return OpStatus::OK or OpStatus::ERR_NO_MEM if assignment couldn't be done
		 */
		OP_STATUS			SetStartPath(const OpString8& start_path);

		/**
		 * Returns a link to the parent path 
		 * 
		 * With respect to current path. Possibly in format "ftp://user@server//path" or even ".."
		 *
		 * @return The parent path encoded in 8-bit, or 0 if there is no parent path
		 */
		const char*			GetParentPath();

#ifdef _ENABLE_AUTHENTICATE
		void SetUserNameAndPassword(const OpStringC8 &username, OpStringS8 &password);
		BOOL AnoynymousUser(){return (auth_id == 0 ? TRUE : FALSE);};
		unsigned long	GetAuthorizationId(){return auth_id;};
		void		SetAuthorizationId(unsigned long aid){auth_id=aid;};
#endif
		virtual void Stop(){if(InList() || ftp_conn) master->RemoveRequest(this);};

#ifdef FTP_RESUME
		void SetResume(OpFileLength pos){resume_pos = pos; use_resume = (pos != 0);}
		BOOL GetHasResumed(){return using_resume;};
		BOOL GetSupportResume(){return master->GetRestartSupported();};
		OpStringC8 &MDTM_Date(){return mdtm_date;}
		void SetMDTM_Date(const OpStringC8 &src){OpStatus::Ignore(mdtm_date.Set(src));}; // not so dangerous if this fail;
#endif
		void GetDirMsgCopy(OpString8 &);
		OpFileLength GetSize() const{return file_size;};

		void Set_NetType(OpSocketAddressNetType val){use_nettype = val;} // Which nettypes are permitted
		OpSocketAddressNetType Get_NetType() const {return use_nettype;} // Which nettypes are permitted

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		void PauseLoading();
		OP_STATUS ContinueLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
	private:
		void InternalInit();
		void InternalDestruct();

};

#endif /* _FTP_ */
