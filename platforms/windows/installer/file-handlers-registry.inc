//Format: {key_path, value_name, value_type (or REG_DELETE or REG_DEL_KEY), value_data, min_os_version, root_key_limit, product_limit, clean_parents}
//
//If value_name is null, the default value of the key is affeected
//If REG_DELETE is used, the value is deleted
//If REG_DEL_KEY is used, the whole key is deleted

//min_os_version is the minimal OS version needed for the change to be executed

{UNI_L("Software\\Classes\\Opera{Product}.HTML"),												UNI_L("FriendlyTypeName"),			REG_SZ,		UNI_L("Opera Web Document"),								OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.HTML\\DefaultIcon"),									NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,1"),							OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.HTML\\shell\\open\\command"),							NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.HTML\\shell\\open\\ddeexec"),							NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 0},
{UNI_L("Software\\Classes\\Opera{Product}.HTML\\shell\\open\\ddeexec"),							NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.HTML\\shell\\open\\ddeexec\\Application"),			NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.HTML\\shell\\open\\ddeexec\\Topic"),					NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},

{UNI_L("Software\\Classes\\Opera{Product}.Extension"),											UNI_L("FriendlyTypeName"),			REG_SZ,		UNI_L("Opera Extension"),									OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Extension\\DefaultIcon"),								NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,4"),							OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Extension\\shell\\open\\command"),					NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.Extension\\shell\\open\\ddeexec"),					NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 0},
{UNI_L("Software\\Classes\\Opera{Product}.Extension\\shell\\open\\ddeexec"),					NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.Extension\\shell\\open\\ddeexec\\Application"),		NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Extension\\shell\\open\\ddeexec\\Topic"),				NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},

{UNI_L("Software\\Classes\\Opera{Product}.Image"),												UNI_L("FriendlyTypeName"),			REG_SZ,		UNI_L("Opera Image"),										OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Image\\DefaultIcon"),									NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,1"),							OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Image\\shell\\open\\command"),						NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.Image\\shell\\open\\ddeexec"),						NULL,								REG_DEL_KEY,NULL,														OLDER_THAN_WIN2K, HKEY_LOCAL_MACHINE, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 0},
{UNI_L("Software\\Classes\\Opera{Product}.Image\\shell\\open\\ddeexec"),						NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.Image\\shell\\open\\ddeexec\\Application"),			NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Image\\shell\\open\\ddeexec\\Topic"),					NULL,								REG_SZ,		UNI_L(""),													OLDER_THAN_WIN2K, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},

{UNI_L("Software\\Classes\\Opera{Product}.Protocol"),											UNI_L("EditFlags"),					REG_DWORD,	(void*)2,													WINVISTA, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol"),											UNI_L("FriendlyTypeName"),			REG_SZ,		UNI_L("Opera Protocol"),									WINVISTA, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol"),											UNI_L("URL Protocol"),				REG_SZ,		NULL,														WINVISTA, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol\\DefaultIcon"),								NULL,								REG_SZ,		UNI_L("{Resources}Opera.exe,1"),							WINVISTA, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol\\shell\\open\\command"),						NULL,								REG_SZ,		UNI_L("\"{Resources}Opera.exe\" \"%1\""),					WINVISTA, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol\\shell\\open\\ddeexec"),						NULL,								REG_DEL_KEY,NULL,														WINVISTA, HKEY_LOCAL_MACHINE, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 0},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol\\shell\\open\\ddeexec"),						NULL,								REG_SZ,		UNI_L(""),													WINVISTA, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 2},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol\\shell\\open\\ddeexec\\Application"),		NULL,								REG_SZ,		UNI_L(""),													WINVISTA, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
{UNI_L("Software\\Classes\\Opera{Product}.Protocol\\shell\\open\\ddeexec\\Topic"),				NULL,								REG_SZ,		UNI_L(""),													WINVISTA, HKEY_CURRENT_USER, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1},
