//Format: {key_path, value_name, value_type (or REG_DELETE or REG_DEL_KEY), value_data, min_os_version, root_key_limit, product_limit, clean_parents}
//
//If value_name is null, the default value of the key is affeected
//If REG_DELETE is used, the value is deleted
//If REG_DEL_KEY is used, the whole key is deleted

//min_os_version is the minimal OS version needed for the change to be executed

OperaInstaller::RegistryOperation OEX_assoc[] = {
	{UNI_L("Software\\Classes\\.oex"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Extension"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.oex\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};

OperaInstaller::RegistryOperation HTM_assoc[] = {
	{UNI_L("Software\\Classes\\.htm"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.htm\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation HTML_assoc[] = {
	{UNI_L("Software\\Classes\\.html"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.html\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation XHT_assoc[] = {
	{UNI_L("Software\\Classes\\.xht"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.xht\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation XHTM_assoc[] = {
	{UNI_L("Software\\Classes\\.xhtm"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.xhtm\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation XHTML_assoc[] = {
	{UNI_L("Software\\Classes\\.xhtml"),															NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.xhtml\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation MHT_assoc[] = {
	{UNI_L("Software\\Classes\\.mht"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.mht\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation MHTML_assoc[] = {
	{UNI_L("Software\\Classes\\.mhtml"),															NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.mhtml\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation XML_assoc[] = {
	{UNI_L("Software\\Classes\\.xml"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.xml\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation TORRENT_assoc[] = {
	{UNI_L("Software\\Classes\\.torrent"),															NULL,								REG_SZ,		UNI_L("Opera{Product}.HTML"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.torrent\\UserChoice"),UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation BMP_assoc[] = {
	{UNI_L("Software\\Classes\\.bmp"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.bmp\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation JPG_assoc[] = {
	{UNI_L("Software\\Classes\\.jpg"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.jpg\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation JPEG_assoc[] = {
	{UNI_L("Software\\Classes\\.jpeg"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.jpeg\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation PNG_assoc[] = {
	{UNI_L("Software\\Classes\\.png"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.png\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation SVG_assoc[] = {
	{UNI_L("Software\\Classes\\.svg"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.svg\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation GIF_assoc[] = {
	{UNI_L("Software\\Classes\\.gif"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.gif\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation XBM_assoc[] = {
	{UNI_L("Software\\Classes\\.xbm"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.xbm\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation OGA_assoc[] = {
	{UNI_L("Software\\Classes\\.oga"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.oga\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation OGV_assoc[] = {
	{UNI_L("Software\\Classes\\.ogv"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.ovg\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation OGM_assoc[] = {
	{UNI_L("Software\\Classes\\.ogm"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.ogm\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation OGG_assoc[] = {
	{UNI_L("Software\\Classes\\.ogg"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.ogg\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation WEBM_assoc[] = {
	{UNI_L("Software\\Classes\\.webm"),																NULL,								REG_SZ,		UNI_L("Opera{Product}.Image"),								OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.webm\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};

OperaInstaller::RegistryOperation HTTP_assoc[] = {
	{UNI_L("Software\\Classes\\http"),																UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http"),																UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http\\DefaultIcon"),													NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,1"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http\\shell\\open\\command"),										NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http\\shell\\open\\ddeexec"),										NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http\\shell\\open\\ddeexec"),										NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\http\\shell\\open\\ddeexec\\Topic"),									NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation HTTPS_assoc[] = {
	{UNI_L("Software\\Classes\\https"),																UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https"),																UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https\\DefaultIcon"),												NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,1"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https\\shell\\open\\command"),										NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https\\shell\\open\\ddeexec"),										NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https\\shell\\open\\ddeexec"),										NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\https\\shell\\open\\ddeexec\\Topic"),								NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice"),UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation FTP_assoc[] = {
	{UNI_L("Software\\Classes\\ftp"),																UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp"),																UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp\\DefaultIcon"),													NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,1"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp\\shell\\open\\command"),											NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp\\shell\\open\\ddeexec"),											NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp\\shell\\open\\ddeexec"),											NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\ftp\\shell\\open\\ddeexec\\Topic"),									NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\ftp\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation MAILTO_assoc[] = {	
	{UNI_L("Software\\Classes\\mailto"),															UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto"),															UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto\\DefaultIcon"),												NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,3"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto\\shell\\open\\command"),										NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto\\open\\ddeexec"),												NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto\\shell\\open\\ddeexec"),										NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\mailto\\shell\\open\\ddeexec\\Topic"),								NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\mailto\\UserChoice"),UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation NNTP_assoc[] = {
	{UNI_L("Software\\Classes\\nntp"),																UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp"),																UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp\\DefaultIcon"),													NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,3"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp\\shell\\open\\command"),										NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp\\open\\ddeexec"),												NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp\\shell\\open\\ddeexec"),										NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\nntp\\shell\\open\\ddeexec\\Topic"),									NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\nntp\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation NEWS_assoc[] = {
	{UNI_L("Software\\Classes\\news"),																UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news"),																UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news\\DefaultIcon"),													NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,3"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news\\shell\\open\\command"),										NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news\\open\\ddeexec"),												NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news\\shell\\open\\ddeexec"),										NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\news\\shell\\open\\ddeexec\\Topic"),									NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\news\\UserChoice"),	UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
OperaInstaller::RegistryOperation SNEWS_assoc[] = {
	{UNI_L("Software\\Classes\\snews"),																UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews"),																UNI_L("URL Protocol"),				REG_SZ,		NULL,														OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews\\DefaultIcon"),												NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,3"),							OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews\\shell\\open\\command"),										NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews\\open\\ddeexec"),												NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews\\shell\\open\\ddeexec"),										NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews\\shell\\open\\ddeexec\\Application"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Classes\\snews\\shell\\open\\ddeexec\\Topic"),								NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{UNI_L("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\snews\\UserChoice"),UNI_L("Progid"),					REG_DELETE,	NULL,														WINVISTA, HKEY_CURRENT_USER, OperaInstaller::PRODUCT_OPERA | OperaInstaller::PRODUCT_OPERA_NEXT, 0},
	{NULL, NULL, 0, NULL, FAILED, NULL, 0, 0}
};
