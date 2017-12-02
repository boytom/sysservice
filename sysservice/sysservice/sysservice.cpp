// sysservice.cpp : Defines the entry point for the console application.
//

//                            _ooOoo_  
//                           o8888888o  
//                           88" . "88  
//                           (| -_- |)  
//                            O\ = /O  
//                        ____/`---'\____  
//                      .   ' \\| |// `.  
//                       / \\||| : |||// \  
//                     / _||||| -:- |||||- \  
//                       | | \\\ - /// | |  
//                     | \_| ''\---/'' | |  
//                      \ .-\__ `-` ___/-. /  
//                   ___`. .' /--.--\ `. . __  
//                ."" '< `.___\_<|>_/___.' >'"".  
//               | | : `- \`.;`\ _ /`;.`/ - ` : | |  
//                 \ \ `-. \_ __\ /__ _/ .-` / /  
//         ======`-.____`-.___\_____/___.-`____.-'======  
//                            `=---='  
//  
//         .............................................  
//                  佛祖保佑             永无BUG 
//          佛曰:  
//                  写字楼里写字间，写字间里程序员；  
//                  程序人员写程序，又拿程序换酒钱。  
//                  酒醒只在网上坐，酒醉还来网下眠；  
//                  酒醉酒醒日复日，网上网下年复年。  
//                  但愿老死电脑间，不愿鞠躬老板前；  
//                  奔驰宝马贵者趣，公交自行程序员。  
//                  别人笑我忒疯癫，我笑自己命太贱；  
//                  不见满街漂亮妹，哪个归得程序员？  

#include "stdafx.h"

#pragma comment(lib, "advapi32.lib")

static const wchar_t *SVCNAME = L"sysupdate";
static SERVICE_STATUS          gSvcStatus; 
static SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
static HANDLE                  ghSvcStopEvent = NULL;
static DWORD                   gdwStartType = SERVICE_AUTO_START; //SERVICE_DEMAND_START

static VOID DisplayUsage(VOID);

static VOID WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv);
static VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv); 
static VOID WINAPI SvcCtrlHandlerEx(_In_  DWORD dwControl, _In_  DWORD dwEventType, _In_  LPVOID lpEventData, _In_  LPVOID lpContext);
static VOID ReportSvcStatus_for_autostart(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
static VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint );
static VOID SvcReportEvent(LPTSTR szFunction);

//------------------ Service Config function
static VOID DoInstallSvc(VOID);
static VOID DoQuerySvc(VOID);
static VOID DoUpdateSvcDesc(VOID);
static VOID DoDisableSvc(VOID);
static VOID DoEnableSvc(VOID);
static VOID DoDeleteSvc(VOID);
//------------------ Service Control function
static VOID DoStartSvc(VOID);
static VOID DoUpdateSvcDacl(VOID);
static VOID DoStopSvc(VOID);
static VOID DoPauseSvc(VOID);
static VOID DoContinueSvc(VOID);
static BOOL StopDependentServices(SC_HANDLE schSCManager, SC_HANDLE schService);

static int cmdcompare(const void *arg1, const void *arg2);
static const wchar_t *mystrerror(DWORD errcode);

struct CmdTable
{
	const wchar_t *cmd;
	void (*func)(void);
};

struct CmdTable cmdtable[] = {
	{L"install",  DoInstallSvc},
	{L"query",    DoQuerySvc},
	{L"describe", DoUpdateSvcDesc},
	{L"disable",  DoDisableSvc},
	{L"enable",	  DoEnableSvc},
	{L"delete",   DoDeleteSvc},
	{L"start",    DoStartSvc},
	{L"dacl",     DoUpdateSvcDacl},
	{L"stop",     DoStopSvc},
	{L"pause",    DoPauseSvc},
	{L"continue", DoContinueSvc},
};

int _tmain(int argc, _TCHAR* argv[])
{
	_wsetlocale(LC_ALL, L"chs_china.936");
	
	if(_isatty(_fileno(stdout))) {
#if 0
		printconfig(stdout);
#endif
		if(argc < 2)
			exit(-1);
		qsort(cmdtable, sizeof(cmdtable) / sizeof(cmdtable[0]), sizeof(cmdtable[0]), cmdcompare);
		argv++;
		_wcslwr(*argv);
		const struct CmdTable *cmd;
		struct CmdTable key = {*argv, NULL};
		if((cmd = (const struct CmdTable *)bsearch(&key, cmdtable, sizeof(cmdtable) / sizeof(cmdtable[0]), 
			sizeof(cmdtable[0]), cmdcompare)) != NULL)
			cmd->func();
	}
	else {
		// TO_DO: Add any additional services for the process to this table.
		SERVICE_TABLE_ENTRY DispatchTable[] = 
		{ 
			{ (wchar_t *)SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
			{ NULL, NULL } 
		}; 
 
		// This call returns when the service has stopped. 
		// The process should simply terminate when the call returns.
		initcontenttype();
		if (!StartServiceCtrlDispatcher( DispatchTable )) 
			SvcReportEvent(TEXT("StartServiceCtrlDispatcher")); 
		destroycontenttype();
	}

	return 0;
}

static VOID DisplayUsage(VOID)
{
    wprintf(L"Description:\n");
    wprintf(L"\tCommand-line tool that configures and control a service.\n\n");
    wprintf(L"Usage:\n");
    wprintf(L"\tsysupdate [command] [service_name]\n\n");
    wprintf(L"\t[command]\n");
    for(int unsigned i = 0U; i < sizeof(cmdtable) / sizeof(cmdtable[0]); i++)
		wprintf(L"\t%s\n", cmdtable[i].cmd);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
static VOID WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandlerEx( 
        SVCNAME, 
        (LPHANDLER_FUNCTION_EX)SvcCtrlHandlerEx, NULL);

    if( !gSvcStatusHandle )  { 
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler")); 
        return; 
    } 

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    gSvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM
    // Call the SetServiceStatus function to report SERVICE_RUNNING but accept no controls until 
	// initialization is finished. The service does this by calling SetServiceStatus with 
	// dwCurrentState set to SERVICE_RUNNING and dwControlsAccepted set to 0 in 
	// the SERVICE_STATUS structure. This ensures that the SCM will not send any control requests
	// to the service before it is ready and frees the SCM to manage other services. This approach
	// to initialization is recommended for performance, especially for autostart services.
	if(gdwStartType == SERVICE_AUTO_START)
		ReportSvcStatus_for_autostart(SERVICE_RUNNING, NO_ERROR, 0);
	else
        ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000UL);

    // Perform service-specific initialization and work.

    SvcInit( dwArgc, lpszArgv );
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
static VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEvent(
                         NULL,    // default security attributes
                         TRUE,    // manual reset event
                         FALSE,   // not signaled
                         NULL);   // no name

    if ( ghSvcStopEvent == NULL) {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0 );
        return;
    }

	// Report running status when initialization is complete.
	if(gdwStartType == SERVICE_AUTO_START)
		ReportSvcStatus_for_autostart(SERVICE_RUNNING, NO_ERROR, 0);
	else
        ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0 );

    // TO_DO: Perform work until service stops.
	starttask();

    // Check whether to stop the service.
    WaitForSingleObject(ghSvcStopEvent, INFINITE);
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwControl [in]    The control code. This parameter can be one of the following values.
//   dwEventType [in]  The type of event that has occurred. This parameter is used if dwControl
//                       is SERVICE_CONTROL_DEVICEEVENT, SERVICE_CONTROL_HARDWAREPROFILECHANGE, SERVICE_CONTROL_POWEREVENT, or SERVICE_CONTROL_SESSIONCHANGE. Otherwise, it is zero.
//   lpEventData [in]  Additional device information, if required. The format of this data depends on the value of the dwControl and dwEventType parameters.
//   pContext [in]     User-defined data passed from RegisterServiceCtrlHandlerEx. When multiple services share a process, the lpContext parameter can help identify the service.
// 
// Return value:
//   None
//
static VOID WINAPI SvcCtrlHandlerEx(_In_  DWORD dwControl, _In_  DWORD dwEventType, _In_  LPVOID lpEventData, _In_  LPVOID lpContext)
{
   // Handle the requested control code. 

   switch(dwControl) 
   {  
	  case SERVICE_CONTROL_CONTINUE:
		  ReportSvcStatus(SERVICE_CONTINUE_PENDING, NO_ERROR, 0LU);
		  continuetask();
		  ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0LU);
		  break;
       case SERVICE_CONTROL_INTERROGATE: 
		   ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0LU);
         break; 
	  case SERVICE_CONTROL_PAUSE:
		  ReportSvcStatus(SERVICE_PAUSE_PENDING, NO_ERROR, 0LU);
		  pausetask();
		  ReportSvcStatus(SERVICE_PAUSED, NO_ERROR, 0LU);
		  break;
	  case SERVICE_CONTROL_PRESHUTDOWN:
		  break;
	  case SERVICE_CONTROL_POWEREVENT:
		  break;
	  case SERVICE_CONTROL_SESSIONCHANGE:
		  break;
	  case SERVICE_CONTROL_STOP: 
	  case SERVICE_CONTROL_SHUTDOWN:
         ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0LU);
 		 stoptask();
         // Signal the service to stop.
         SetEvent(ghSvcStopEvent);
         ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0LU);
         return;
      default: 
         break;
   }
}

static VOID ReportSvcStatus_for_autostart(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static int count;

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

	if(count == 0) {
		 gSvcStatus.dwControlsAccepted = 0;
		 count++;
	}
	else//SERVICE_CONTROL_INTERROGATE | SERVICE_ACCEPT_PARAMCHANGE | SERVICE_ACCEPT_PRESHUTDOWN
		 gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;

	gSvcStatus.dwCheckPoint = 0;

    // Report the status of the service to the SCM.
    SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
static VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1LU;
	static DWORD dwLastState = SERVICE_START_PENDING;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if(dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else
        gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;

	// The check-point value the service increments periodically to report its progress 
	// during a lengthy start, stop, pause, or continue operation. For example, the 
	// service should increment this value as it completes each step of its initialization
	// when it is starting up. The user interface program that invoked the operation on 
	// the service uses this value to track the progress of the service during a lengthy
	// operation. This value is not valid and should be zero when the service does not have
	// a start, stop, pause, or continue operation pending.
	if((dwCurrentState == SERVICE_START_PENDING 
		|| dwCurrentState == SERVICE_STOP_PENDING
		|| dwCurrentState == SERVICE_CONTINUE_PENDING
		|| dwCurrentState == SERVICE_PAUSE_PENDING) && dwLastState == dwCurrentState) // && 后的条件是根据 VCL 代码添加的。
		gSvcStatus.dwCheckPoint = dwCheckPoint++;
	else
		gSvcStatus.dwCheckPoint = 0;

	dwLastState = dwCurrentState;

    // Report the status of the service to the SCM.
    SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
static VOID SvcReportEvent(LPTSTR szFunction)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[BUFSIZ];

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if( NULL != hEventSource )
    {
        swprintf(Buffer, BUFSIZ, TEXT("%s failed with %s"), szFunction, mystrerror(GetLastError()));

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0,                   // event category
                    SVC_ERROR,           // event identifier
                    NULL,                // no security identifier
                    2,                   // size of lpszStrings array
                    0,                   // no binary data
                    lpszStrings,         // array of strings
                    NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}

//------------------ Service Config function

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoInstallSvc(VOID)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[_MAX_PATH];

    if(!GetModuleFileNameW(NULL, szPath, MAX_PATH )) {
        wprintf(L"Cannot install service (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the SCM database. 
     schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Create the service

    schService = CreateService( 
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        gdwStartType,              // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 
 
    if (schService == NULL) {
        wprintf(L"CreateService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }
    else wprintf(L"Service installed successfully.\n"); 

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Retrieves and displays the current service configuration.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoQuerySvc(VOID)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    LPQUERY_SERVICE_CONFIG lpsc; 
    LPSERVICE_DESCRIPTION lpsd;
    DWORD dwBytesNeeded, cbBufSize, dwError; 

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,          // SCM database 
        SVCNAME,             // name of service 
        SERVICE_QUERY_CONFIG); // need query config access 
 
    if (schService == NULL) { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError()));
        CloseServiceHandle(schSCManager);
        return;
    }

    // Get the configuration information.
 
    if( !QueryServiceConfig( 
        schService, 
        NULL, 
        0, 
        &dwBytesNeeded)) {
        dwError = GetLastError();
        if( ERROR_INSUFFICIENT_BUFFER == dwError )  {
            cbBufSize = dwBytesNeeded;
            lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, cbBufSize);
        }
        else  {
            wprintf(L"QueryServiceConfig failed (%s).", mystrerror(dwError));
            goto cleanup; 
        }
    }
  
    if( !QueryServiceConfig( 
        schService, 
        lpsc, 
        cbBufSize, 
        &dwBytesNeeded) ) {
        wprintf(L"QueryServiceConfig failed (%s).", mystrerror(GetLastError()));
        goto cleanup;
    }

    if( !QueryServiceConfig2( 
        schService, 
        SERVICE_CONFIG_DESCRIPTION,
        NULL, 
        0, 
        &dwBytesNeeded))
    {
        dwError = GetLastError();
        if( ERROR_INSUFFICIENT_BUFFER == dwError ) {
            cbBufSize = dwBytesNeeded;
            lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_FIXED, cbBufSize);
        }
        else {
            wprintf(L"QueryServiceConfig2 failed (%s).", mystrerror(dwError));
            goto cleanup; 
        }
    }
 
    if (! QueryServiceConfig2( 
        schService, 
        SERVICE_CONFIG_DESCRIPTION,
        (LPBYTE) lpsd, 
        cbBufSize, 
        &dwBytesNeeded) )  {
        wprintf(L"QueryServiceConfig2 failed (%s).", mystrerror(GetLastError()));
        goto cleanup;
    }
 
    // Print the configuration information.
 
    _tprintf(TEXT("%s configuration: \n"), SVCNAME);
    _tprintf(TEXT("  Type: 0x%x\n"), lpsc->dwServiceType);
    _tprintf(TEXT("  Start Type: 0x%x\n"), lpsc->dwStartType);
    _tprintf(TEXT("  Error Control: 0x%x\n"), lpsc->dwErrorControl);
    _tprintf(TEXT("  Binary path: %s\n"), lpsc->lpBinaryPathName);
    _tprintf(TEXT("  Account: %s\n"), lpsc->lpServiceStartName);

    if (lpsd->lpDescription != NULL && lstrcmp(lpsd->lpDescription, TEXT("")) != 0)
        _tprintf(TEXT("  Description: %s\n"), lpsd->lpDescription);
    if (lpsc->lpLoadOrderGroup != NULL && lstrcmp(lpsc->lpLoadOrderGroup, TEXT("")) != 0)
        _tprintf(TEXT("  Load order group: %s\n"), lpsc->lpLoadOrderGroup);
    if (lpsc->dwTagId != 0)
        _tprintf(TEXT("  Tag ID: %d\n"), lpsc->dwTagId);
    if (lpsc->lpDependencies != NULL && lstrcmp(lpsc->lpDependencies, TEXT("")) != 0)
        _tprintf(TEXT("  Dependencies: %s\n"), lpsc->lpDependencies);
 
    LocalFree(lpsc); 
    LocalFree(lpsd);

cleanup:
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Updates the service description to "This is a test description".
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoUpdateSvcDesc(VOID)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_DESCRIPTION sd;
    LPTSTR szDesc = TEXT("This is a test description");

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,            // SCM database 
        SVCNAME,               // name of service 
        SERVICE_CHANGE_CONFIG);  // need change config access 
 
    if (schService == NULL) { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Change the service description.

    sd.lpDescription = szDesc;

    if( !ChangeServiceConfig2(
        schService,                 // handle to service
        SERVICE_CONFIG_DESCRIPTION, // change: description
        &sd) )                      // new description
        wprintf(L"ChangeServiceConfig2 failed.\n");
    else
		wprintf(L"Service description updated successfully.\n");

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Disables the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoDisableSvc(VOID)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,            // SCM database 
        SVCNAME,               // name of service 
        SERVICE_CHANGE_CONFIG);  // need change config access 
 
    if (schService == NULL)  { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError()));
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Change the service start type.

    if (! ChangeServiceConfig( 
        schService,        // handle of service 
        SERVICE_NO_CHANGE, // service type: no change 
        SERVICE_DISABLED,  // service start type 
        SERVICE_NO_CHANGE, // error control: no change 
        NULL,              // binary path: no change 
        NULL,              // load order group: no change 
        NULL,              // tag ID: no change 
        NULL,              // dependencies: no change 
        NULL,              // account name: no change 
        NULL,              // password: no change 
        NULL) )            // display name: no change
        wprintf(L"ChangeServiceConfig failed (%s).\n", mystrerror(GetLastError()));
    else
		wprintf(L"Service disabled successfully.\n"); 

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Enables the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoEnableSvc(VOID)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,            // SCM database 
        SVCNAME,               // name of service 
        SERVICE_CHANGE_CONFIG);  // need change config access 
 
    if (schService == NULL)  { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Change the service start type.

    if (! ChangeServiceConfig( 
        schService,            // handle of service 
        SERVICE_NO_CHANGE,     // service type: no change 
        SERVICE_DEMAND_START,  // service start type 
        SERVICE_NO_CHANGE,     // error control: no change 
        NULL,                  // binary path: no change 
        NULL,                  // load order group: no change 
        NULL,                  // tag ID: no change 
        NULL,                  // dependencies: no change 
        NULL,                  // account name: no change 
        NULL,                  // password: no change 
        NULL) )                // display name: no change
        wprintf(L"ChangeServiceConfig failed (%s).\n", mystrerror(GetLastError())); 
    else 
		wprintf(L"Service enabled successfully.\n"); 

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Deletes a service from the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoDeleteSvc(VOID)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    //SERVICE_STATUS ssStatus; 

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager)  {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,       // SCM database 
        SVCNAME,          // name of service 
        DELETE);            // need delete access 
 
    if (schService == NULL) { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }

    // Delete the service.
 
    if (!DeleteService(schService)) 
        wprintf(L"DeleteService failed (%s).\n", mystrerror(GetLastError())); 
    else
		wprintf(L"Service deleted successfully.\n"); 
 
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//------------------ Service Control function

//
// Purpose: 
//   Starts the service if possible.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoStartSvc(VOID)
{
	SC_HANDLE schSCManager;
    SC_HANDLE schService;

    SERVICE_STATUS_PROCESS ssStatus; 
    DWORD dwOldCheckPoint; 
    DWORD dwStartTickCount;
    DWORD dwWaitTime;
    DWORD dwBytesNeeded;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,         // SCM database 
        SVCNAME,            // name of service 
        SERVICE_ALL_ACCESS);  // full access 
 
    if (schService == NULL)
    { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Check the status in case the service is not stopped. 

    if (!QueryServiceStatusEx( 
            schService,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // information level
            (LPBYTE) &ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded ) )              // size needed if buffer is too small
    {
        wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
        CloseServiceHandle(schService); 
        CloseServiceHandle(schSCManager);
        return; 
    }

    // Check if the service is already running. It would be possible 
    // to stop the service here, but for simplicity this example just returns. 

    if(ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
    {
        wprintf(L"Cannot start the service because it is already running.\n");
        CloseServiceHandle(schService); 
        CloseServiceHandle(schSCManager);
        return; 
    }

    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    // Wait for the service to stop before attempting to start it.

    while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 
 
        dwWaitTime = ssStatus.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        // Check the status until the service is no longer stop pending. 
 
        if (!QueryServiceStatusEx( 
                schService,                     // handle to service 
                SC_STATUS_PROCESS_INFO,         // information level
                (LPBYTE) &ssStatus,             // address of structure
                sizeof(SERVICE_STATUS_PROCESS), // size of structure
                &dwBytesNeeded ) )              // size needed if buffer is too small
        {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
            CloseServiceHandle(schService); 
            CloseServiceHandle(schSCManager);
            return; 
        }

        if ( ssStatus.dwCheckPoint > dwOldCheckPoint )
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if(GetTickCount()-dwStartTickCount > ssStatus.dwWaitHint)
            {
                wprintf(L"Timeout waiting for service to stop.\n");
                CloseServiceHandle(schService); 
                CloseServiceHandle(schSCManager);
                return; 
            }
        }
    }

    // Attempt to start the service.

    if (!StartService(
            schService,  // handle to service 
            0,           // number of arguments 
            NULL) )      // no arguments 
    {
        wprintf(L"StartService failed (%s).\n", mystrerror(GetLastError()));
        CloseServiceHandle(schService); 
        CloseServiceHandle(schSCManager);
        return; 
    }
    else wprintf(L"Service start pending...\n"); 

    // Check the status until the service is no longer start pending. 
 
    if (!QueryServiceStatusEx( 
            schService,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // info level
            (LPBYTE) &ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded ) )              // if buffer too small
    {
        wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
        CloseServiceHandle(schService); 
        CloseServiceHandle(schSCManager);
        return; 
    }
 
    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    while (ssStatus.dwCurrentState == SERVICE_START_PENDING) 
    { 
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth the wait hint, but no less than 1 second and no 
        // more than 10 seconds. 
 
        dwWaitTime = ssStatus.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        // Check the status again. 
 
        if (!QueryServiceStatusEx( 
            schService,             // handle to service 
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE) &ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded ) )              // if buffer too small
        {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
            break; 
        }
 
        if ( ssStatus.dwCheckPoint > dwOldCheckPoint )
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if(GetTickCount()-dwStartTickCount > ssStatus.dwWaitHint)
            {
                // No progress made within the wait hint.
                break;
            }
        }
    } 

    // Determine whether the service is running.

    if (ssStatus.dwCurrentState == SERVICE_RUNNING) 
    {
        wprintf(L"Service started successfully.\n"); 
    }
    else 
    { 
        wprintf(L"Service not started. \n");
        wprintf(L"  Current State: %lu\n", ssStatus.dwCurrentState); 
        wprintf(L"  Exit Code: %lu\n", ssStatus.dwWin32ExitCode); 
        wprintf(L"  Check Point: %lu\n", ssStatus.dwCheckPoint); 
        wprintf(L"  Wait Hint: %lu\n", ssStatus.dwWaitHint); 
    } 

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Updates the service DACL to grant start, stop, delete, and read
//   control access to the Guest account.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoUpdateSvcDacl(VOID)
{
	SC_HANDLE schSCManager;
    SC_HANDLE schService;

    EXPLICIT_ACCESS      ea;
    SECURITY_DESCRIPTOR  sd;
    PSECURITY_DESCRIPTOR psd            = NULL;
    PACL                 pacl           = NULL;
    PACL                 pNewAcl        = NULL;
    BOOL                 bDaclPresent   = FALSE;
    BOOL                 bDaclDefaulted = FALSE;
    DWORD                dwError        = 0;
    DWORD                dwSize         = 0;
    DWORD                dwBytesNeeded  = 0;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service

    schService = OpenService( 
        schSCManager,              // SCManager database 
        SVCNAME,                 // name of service 
        READ_CONTROL | WRITE_DAC); // access
 
    if (schService == NULL)
    { 
        wprintf(L"OpenService failed (%d).\n", GetLastError()); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Get the current security descriptor.

    if (!QueryServiceObjectSecurity(schService,
        DACL_SECURITY_INFORMATION, 
        &psd,           // using NULL does not work on all versions
        0, 
        &dwBytesNeeded))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            dwSize = dwBytesNeeded;
            psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(),
                    HEAP_ZERO_MEMORY, dwSize);
            if (psd == NULL)
            {
                // Note: HeapAlloc does not support GetLastError.
                wprintf(L"HeapAlloc failed.\n");
                goto dacl_cleanup;
            }
  
            if (!QueryServiceObjectSecurity(schService,
                DACL_SECURITY_INFORMATION, psd, dwSize, &dwBytesNeeded))
            {
                wprintf(L"QueryServiceObjectSecurity failed (%s).\n", mystrerror(GetLastError()));
                goto dacl_cleanup;
            }
        }
        else 
        {
            wprintf(L"QueryServiceObjectSecurity failed (%s).\n", mystrerror(GetLastError()));
            goto dacl_cleanup;
        }
    }

    // Get the DACL.

    if (!GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl,
                                   &bDaclDefaulted))
    {
        wprintf(L"GetSecurityDescriptorDacl failed(%s).\n", mystrerror(GetLastError()));
        goto dacl_cleanup;
    }

    // Build the ACE.

    BuildExplicitAccessWithName(&ea, TEXT("GUEST"),
        SERVICE_START | SERVICE_STOP | READ_CONTROL | DELETE,
        SET_ACCESS, NO_INHERITANCE);

    dwError = SetEntriesInAcl(1, &ea, pacl, &pNewAcl);
    if (dwError != ERROR_SUCCESS)
    {
        wprintf(L"SetEntriesInAcl failed(%s).\n", mystrerror(dwError));
        goto dacl_cleanup;
    }

    // Initialize a new security descriptor.

    if (!InitializeSecurityDescriptor(&sd, 
        SECURITY_DESCRIPTOR_REVISION))
    {
        wprintf(L"InitializeSecurityDescriptor failed(%s).\n", mystrerror(GetLastError()));
        goto dacl_cleanup;
    }

    // Set the new DACL in the security descriptor.

    if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE))
    {
        wprintf(L"SetSecurityDescriptorDacl failed(%s).\n", mystrerror(GetLastError()));
        goto dacl_cleanup;
    }

    // Set the new DACL for the service object.

    if (!SetServiceObjectSecurity(schService, 
        DACL_SECURITY_INFORMATION, &sd))
    {
        wprintf(L"SetServiceObjectSecurity failed(%s).\n", mystrerror(GetLastError()));
        goto dacl_cleanup;
    }
    else wprintf(L"Service DACL updated successfully.\n");

dacl_cleanup:
    CloseServiceHandle(schSCManager);
    CloseServiceHandle(schService);

    if(NULL != pNewAcl)
        LocalFree((HLOCAL)pNewAcl);
    if(NULL != psd)
        HeapFree(GetProcessHeap(), 0, (LPVOID)psd);
}

// Purpose: 
//   Stops the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
static VOID DoStopSvc(VOID)
{
	SC_HANDLE schSCManager;
    SC_HANDLE schService;

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,         // SCM database 
        SVCNAME,            // name of service 
        SERVICE_STOP | 
        SERVICE_QUERY_STATUS | 
        SERVICE_ENUMERATE_DEPENDENTS);  
 
    if (schService == NULL)
    { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Make sure the service is not already stopped.

    if ( !QueryServiceStatusEx( 
            schService, 
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp, 
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded ) )
    {
        wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
        goto stop_cleanup;
    }

    if ( ssp.dwCurrentState == SERVICE_STOPPED )
    {
        wprintf(L"Service is already stopped.\n");
        goto stop_cleanup;
    }

    // If a stop is pending, wait for it.

    while ( ssp.dwCurrentState == SERVICE_STOP_PENDING ) 
    {
        wprintf(L"Service stop pending...\n");

        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 
 
        dwWaitTime = ssp.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        if ( !QueryServiceStatusEx( 
                 schService, 
                 SC_STATUS_PROCESS_INFO,
                 (LPBYTE)&ssp, 
                 sizeof(SERVICE_STATUS_PROCESS),
                 &dwBytesNeeded ) )
        {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError())); 
            goto stop_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_STOPPED )
        {
            wprintf(L"Service stopped successfully.\n");
            goto stop_cleanup;
        }

        if ( GetTickCount() - dwStartTime > dwTimeout )
        {
            wprintf(L"Service stop timed out.\n");
            goto stop_cleanup;
        }
    }

    // If the service is running, dependencies must be stopped first.

    StopDependentServices(schSCManager, schService);

    // Send a stop code to the service.

    if ( !ControlService( 
            schService, 
            SERVICE_CONTROL_STOP, 
            (LPSERVICE_STATUS) &ssp ) ) {
        wprintf(L"ControlService failed (%s).\n", mystrerror(GetLastError()));
        goto stop_cleanup;
    }

    // Wait for the service to stop.

    while ( ssp.dwCurrentState != SERVICE_STOPPED ) 
    {
        Sleep( ssp.dwWaitHint );
        if ( !QueryServiceStatusEx( 
                schService, 
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&ssp, 
                sizeof(SERVICE_STATUS_PROCESS),
                &dwBytesNeeded ) ) {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
            goto stop_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_STOPPED )
            break;

        if ( GetTickCount() - dwStartTime > dwTimeout ) {
            wprintf(L"Wait timed out.\n" );
            goto stop_cleanup;
        }
    }
    wprintf(L"Service stopped successfully.\n");

stop_cleanup:
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

static VOID DoPauseSvc(VOID)
{
	SC_HANDLE schSCManager;
    SC_HANDLE schService;

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,         // SCM database 
        SVCNAME,            // name of service 
        SERVICE_PAUSE_CONTINUE | 
        SERVICE_QUERY_STATUS | 
        SERVICE_ENUMERATE_DEPENDENTS);  
 
    if (schService == NULL)
    { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Make sure the service is not already stopped.

    if ( !QueryServiceStatusEx( 
            schService, 
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp, 
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded ) )
    {
        wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
        goto pause_cleanup;
    }

    if(ssp.dwCurrentState == SERVICE_STOPPED || ssp.dwCurrentState == SERVICE_STOP_PENDING)
    {
        wprintf(L"Service is not running.\n");
        goto pause_cleanup;
        return; 
    }

    if ( ssp.dwCurrentState == SERVICE_PAUSED )  {
        wprintf(L"Service is already paused.\n");
        goto pause_cleanup;
    }

	// If a pause is pending, wait for it.

    while ( ssp.dwCurrentState == SERVICE_PAUSE_PENDING ) 
    {
        wprintf(L"Service pause pending...\n");

        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 
 
        dwWaitTime = ssp.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        if ( !QueryServiceStatusEx( 
                 schService, 
                 SC_STATUS_PROCESS_INFO,
                 (LPBYTE)&ssp, 
                 sizeof(SERVICE_STATUS_PROCESS),
                 &dwBytesNeeded ) )
        {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError())); 
            goto pause_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_PAUSED )
        {
            wprintf(L"Service paused successfully.\n");
            goto pause_cleanup;
        }

        if ( GetTickCount() - dwStartTime > dwTimeout )
        {
            wprintf(L"Service pause timed out.\n");
            goto pause_cleanup;
        }
    }

    if ( !ControlService( 
            schService, 
            SERVICE_CONTROL_PAUSE, 
            (LPSERVICE_STATUS) &ssp ) ) {
        wprintf(L"ControlService failed (%s).\n", mystrerror(GetLastError()));
        goto pause_cleanup;
    }

    // Wait for the service to pause.

    while ( ssp.dwCurrentState != SERVICE_PAUSED ) 
    {
        Sleep( ssp.dwWaitHint );
        if ( !QueryServiceStatusEx( 
                schService, 
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&ssp, 
                sizeof(SERVICE_STATUS_PROCESS),
                &dwBytesNeeded ) ) {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
            goto pause_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_PAUSED )
            break;

        if ( GetTickCount() - dwStartTime > dwTimeout ) {
            wprintf(L"Wait timed out.\n" );
            goto pause_cleanup;
        }
    }
    wprintf(L"Service pause successfully.\n");

pause_cleanup:
    CloseServiceHandle(schSCManager);
    CloseServiceHandle(schService);
}

static VOID DoContinueSvc(VOID)
{
	SC_HANDLE schSCManager;
    SC_HANDLE schService;

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        wprintf(L"OpenSCManager failed (%s).\n", mystrerror(GetLastError()));
        return;
    }

    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,         // SCM database 
        SVCNAME,            // name of service 
        SERVICE_PAUSE_CONTINUE | 
        SERVICE_QUERY_STATUS | 
        SERVICE_ENUMERATE_DEPENDENTS);  
 
    if (schService == NULL)
    { 
        wprintf(L"OpenService failed (%s).\n", mystrerror(GetLastError())); 
        CloseServiceHandle(schSCManager);
        return;
    }    

    // Make sure the service is not already stopped.

    if ( !QueryServiceStatusEx( 
            schService, 
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp, 
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded ) )
    {
        wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
        goto continue_cleanup;
    }

    if(ssp.dwCurrentState == SERVICE_STOPPED || ssp.dwCurrentState == SERVICE_STOP_PENDING)
    {
        wprintf(L"Service is not running.\n");
        goto continue_cleanup;
        return; 
    }

	if(ssp.dwCurrentState != SERVICE_PAUSED && ssp.dwCurrentState != SERVICE_PAUSE_PENDING) {
	    wprintf(L"Service is not paused.\n");
        goto continue_cleanup;
	}

    if ( ssp.dwCurrentState == SERVICE_CONTINUE_PENDING )  {
        wprintf(L"Service is already paused.\n");
        goto continue_cleanup;
    }

	// If a continue is pending, wait for it.

    while ( ssp.dwCurrentState == SERVICE_CONTINUE_PENDING ) 
    {
        wprintf(L"Service pause pending...\n");

        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 
 
        dwWaitTime = ssp.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        if ( !QueryServiceStatusEx( 
                 schService, 
                 SC_STATUS_PROCESS_INFO,
                 (LPBYTE)&ssp, 
                 sizeof(SERVICE_STATUS_PROCESS),
                 &dwBytesNeeded ) )
        {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError())); 
            goto continue_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_RUNNING )
        {
            wprintf(L"Service continued successfully.\n");
            goto continue_cleanup;
        }

        if ( GetTickCount() - dwStartTime > dwTimeout )
        {
            wprintf(L"Service continue timed out.\n");
            goto continue_cleanup;
        }
    }

    if ( !ControlService( 
            schService, 
            SERVICE_CONTROL_CONTINUE, 
            (LPSERVICE_STATUS) &ssp ) ) {
        wprintf(L"ControlService failed (%s).\n", mystrerror(GetLastError()));
        goto continue_cleanup;
    }

    // Wait for the service to pause.

    while ( ssp.dwCurrentState != SERVICE_RUNNING ) 
    {
        Sleep( ssp.dwWaitHint );
        if ( !QueryServiceStatusEx( 
                schService, 
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&ssp, 
                sizeof(SERVICE_STATUS_PROCESS),
                &dwBytesNeeded ) ) {
            wprintf(L"QueryServiceStatusEx failed (%s).\n", mystrerror(GetLastError()));
            goto continue_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_RUNNING )
            break;

        if ( GetTickCount() - dwStartTime > dwTimeout ) {
            wprintf(L"Wait timed out.\n" );
            goto continue_cleanup;
        }
    }
    wprintf(L"Service continue successfully.\n");

continue_cleanup:
    CloseServiceHandle(schSCManager);
    CloseServiceHandle(schService);
}

static BOOL StopDependentServices(SC_HANDLE schSCManager, SC_HANDLE schService)
{
    DWORD i;
    DWORD dwBytesNeeded;
    DWORD dwCount;

    LPENUM_SERVICE_STATUS   lpDependencies = NULL;
    ENUM_SERVICE_STATUS     ess;
    SC_HANDLE               hDepService;
    SERVICE_STATUS_PROCESS  ssp;

    DWORD dwStartTime = GetTickCount();
    DWORD dwTimeout = 30000; // 30-second time-out

    // Pass a zero-length buffer to get the required buffer size.
    if ( EnumDependentServices( schService, SERVICE_ACTIVE, 
         lpDependencies, 0, &dwBytesNeeded, &dwCount ) ) 
    {
         // If the Enum call succeeds, then there are no dependent
         // services, so do nothing.
         return TRUE;
    } 
    else 
    {
        if ( GetLastError() != ERROR_MORE_DATA )
            return FALSE; // Unexpected error

        // Allocate a buffer for the dependencies.
        lpDependencies = (LPENUM_SERVICE_STATUS) HeapAlloc( 
            GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded );
  
        if ( !lpDependencies )
            return FALSE;

        __try {
            // Enumerate the dependencies.
            if ( !EnumDependentServices( schService, SERVICE_ACTIVE, 
                lpDependencies, dwBytesNeeded, &dwBytesNeeded,
                &dwCount ) )
            return FALSE;

            for ( i = 0; i < dwCount; i++ ) 
            {
                ess = *(lpDependencies + i);
                // Open the service.
                hDepService = OpenService( schSCManager, 
                   ess.lpServiceName, 
                   SERVICE_STOP | SERVICE_QUERY_STATUS );

                if ( !hDepService )
                   return FALSE;

                __try {
                    // Send a stop code.
                    if ( !ControlService( hDepService, 
                            SERVICE_CONTROL_STOP,
                            (LPSERVICE_STATUS) &ssp ) )
                    return FALSE;

                    // Wait for the service to stop.
                    while ( ssp.dwCurrentState != SERVICE_STOPPED ) 
                    {
                        Sleep( ssp.dwWaitHint );
                        if ( !QueryServiceStatusEx( 
                                hDepService, 
                                SC_STATUS_PROCESS_INFO,
                                (LPBYTE)&ssp, 
                                sizeof(SERVICE_STATUS_PROCESS),
                                &dwBytesNeeded ) )
                        return FALSE;

                        if ( ssp.dwCurrentState == SERVICE_STOPPED )
                            break;

                        if ( GetTickCount() - dwStartTime > dwTimeout )
                            return FALSE;
                    }
                } 
                __finally 
                {
                    // Always release the service handle.
                    CloseServiceHandle( hDepService );
                }
            }
        } 
        __finally 
        {
            // Always free the enumeration buffer.
            HeapFree( GetProcessHeap(), 0, lpDependencies );
        }
    } 
    return TRUE;
}

static int cmdcompare(const void *arg1, const void *arg2)
{
	const struct CmdTable *c1 = (const struct CmdTable *)arg1;
	const struct CmdTable *c2 = (const struct CmdTable *)arg2;
	return wcscmp(c1->cmd, c2->cmd);
}

static const wchar_t *mystrerror(DWORD errcode)
{
	static wchar_t wmsg[8192];
	int unsigned long wmsgwchars;

	wmemset(wmsg, L'\0', sizeof(wmsg) / sizeof(wmsg[0]));

	wmsgwchars = 0LU;
#if 0
	wmsgwchars += FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		errcode,  MAKELANGID(LANG_CHINESE_SIMPLIFIED, SUBLANG_CHINESE_SIMPLIFIED), 
		wmsg + wmsgwchars, sizeof(wmsg) / sizeof(wmsg[0]) - 1 - wmsgwchars, NULL);
#endif

	wmsgwchars += FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		errcode,  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		wmsg + wmsgwchars, sizeof(wmsg) / sizeof(wmsg[0]) - 1 - wmsgwchars, NULL);

	wmsgwchars += FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		errcode,  MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		wmsg + wmsgwchars, sizeof(wmsg) / sizeof(wmsg[0]) - 1 - wmsgwchars, NULL);

	wmsgwchars += _snwprintf(wmsg + wmsgwchars, sizeof(wmsg) / sizeof(wmsg[0]) - 1 - wmsgwchars, L"(%lu)", errcode);
	return wmsg;
}

