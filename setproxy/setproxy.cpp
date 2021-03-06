// setproxy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <atlbase.h>
#include <stdio.h>
#include <windows.h>
#include <wininet.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include "ras.h"
#include "raserror.h"
#pragma comment(lib, "rasapi32.lib")

#pragma comment(lib, "wininet.lib")

enum class SETPROXY_ACTION
{
	AUTO,
	DIRECT,
	MANUAL,
	MANUALSERVER,
	BYPASS,
	AUTOCONFIG,
	SHOW,
	RESET
};

//need to find out maximum length for connection name
TCHAR *ConnectionName=NULL;

BOOL EnumConnections();
void LogString(const TCHAR* lpsz, ...);
BOOL ConfigureProxy(SETPROXY_ACTION action);
BOOL SetProxyOption(__in_opt INT iProxyOption, __in_opt TCHAR* pszValue);
BOOL GetProxySettings();
BOOL SetProxyServer(__in_opt TCHAR* pszHostPort);
BOOL SetProxyByPass(__in_opt TCHAR* pszByPass);
BOOL SetProxyAutoConfig(__in_opt TCHAR* pszAutoURL);


void Usage()
{
	printf("SetProxy.exe Version 1.4\n");
	printf("SetProxy.exe usage|reset|autoconfigURL|auto|direct|manual|manual HOST:PORT[;https=HOST:PORT][;ftp=HOST:PORT]|bypass <bypass ports> [ConnectionName]\n");
	printf("Running setproxy with no parameters displays help and connections\n");
	printf("Connection name is optional. By default, setproxy will use the Local Area Network (LAN) Settings connection\n");
	printf("connections    --  shows the Connections.\n");
	printf("show    --  shows current configuration for connection.\n");
	printf("reset   --  Resets proxy settings (DIRECT included).\n");
	printf("autoconfigURL http://proxy/autoconfig.pac \n");
	printf("auto    --  Auto detect proxy settings.\n");
	printf("direct  --  Direct Internet Access, proxy disabled.\n");
	printf("manual  --  Manually Set proxy settings (use existing settings).\n");
	printf("bypass  --  Set the Proxy Bypass addresses.\n");
	printf("manual http=HOST:PORT[;ftp=HOST:PORT]  --  (configure server)\n");
	printf("manual http=http://proxy:80;https=https://proxy:80\n");
	printf("bypass \"172.*;157.*;10.*;127.*;<local>\"\n");
	printf("\nReferences\n**********\n");
	printf("Setting and Retrieving Internet Options https://msdn.microsoft.com/en-us/library/aa385384(v=vs.85).aspx\n");
	printf("How to programmatically query and set proxy settings under WinINet:\n");
	printf("https://support.microsoft.com/en-us/help/226473/how-to-programmatically-query-and-set-proxy-settings-under-internet-ex\n");
	printf("InternetQueryOption function https://msdn.microsoft.com/en-us/library/aa385101(v=vs.85).aspx\n");
	printf("You can use psexec (http://live.sysinternals.com) -s to run setproxy using the System (S-1-5-18) account: psexec -s c:\\tools\\setproxy.exe show\n");
	printf("You can use psexec -u \"NT AUTHORITY\\LOCALSERVICE\" to run setproxy using the Local Service (S-1-5-19) account\n");
	printf("You can use psexec -u \"NT AUTHORITY\\NETWORKSERVICE\" to run setproxy using the Network Service  (S-1-5-20) account\n");
}

INT __cdecl main(int argc, __in_ecount(argc) LPSTR* argv)
{
	INT iReturn = -1;

	SETPROXY_ACTION action = SETPROXY_ACTION::SHOW;
	HRESULT hrCoInit = CoInitialize(NULL);


	// this not getting called if the CoInit() fails won't be fatal
	// g_pIStatus will still be NULL, so all the LogString() calls will 
	// just skip over making the call into Piper for that portion of the logging
	if (FAILED(hrCoInit))
	{
		exit(-1L);
	}

	TCHAR* pszBuffer = NULL;
	if (argc == 1)
	{
		Usage();
		return 0L;
	}
	if (argc > 1)
	{
		// AutoconfigURL should be before Auto since first 4 letters are the same 
		if (0 == _strnicmp("autoconfigURL", argv[1], 13))
		{
			// Adding the Action before if (argc >2) statement since user might specify blank as the autoconfig 
			// Incase user specifies blank in Inetcpl.cpl then the connection is returned to Direct. We are simulating this behaviour			
			action = SETPROXY_ACTION::AUTOCONFIG;
			LogString("Setting AutoConfig url");
			if (argc > 2)
			{
				pszBuffer = argv[2];
				if (argc > 3)
				{
					ConnectionName = argv[3];
				}
			}
			else
			{
				pszBuffer = "";
			}

		}
		else if (0 == _strnicmp("auto", argv[1], 4))
		{
			action = SETPROXY_ACTION::AUTO;
			LogString("Setting Auto detect proxy settings.");
			if (argc > 2)
			{
				ConnectionName = argv[2];
			}
		}
		else if (0 == _strnicmp("connections", argv[1], 4))
		{
			EnumConnections();
			exit(1L);
		}
		else if (0 == _strnicmp("manual", argv[1], 6))
		{
			action = SETPROXY_ACTION::MANUAL;
			if (argc > 2)
			{
				action = SETPROXY_ACTION::MANUALSERVER;
				LogString("Setting Hardcoded proxy");
				pszBuffer = argv[2];
			}
			if (argc > 3)
			{
				ConnectionName = argv[3];
			}
		}
		else if (0 == _strnicmp("direct", argv[1], 6))
		{
			action = SETPROXY_ACTION::DIRECT;
			LogString("Setting Direct Internet Access, proxy disabled.");
			if (argc > 2)
			{
				ConnectionName = argv[2];
			}
		}
		else if (0 == _strnicmp("bypass", argv[1], 6))
		{
			if (argc > 2)
			{
				action = SETPROXY_ACTION::BYPASS;
				LogString("Setting the Proxy Bypass addresses");
				pszBuffer = argv[2];
			}
			if (argc > 3)
			{
				ConnectionName = argv[3];
			}
		}
		else if (0 == _strnicmp("reset", argv[1], 5))
		{
			action = SETPROXY_ACTION::RESET;
			LogString("Resetting the proxy settings");
			if (argc > 2)
			{
				ConnectionName = argv[2];
			}
		}
		else if (0 == _strnicmp("show", argv[1], 5))
		{
			action = SETPROXY_ACTION::SHOW;
			if (argc > 2)
			{
				ConnectionName = argv[2];
			}
		}
		else
		{
			Usage();
			exit(0L);
		}
	}

	if (ConnectionName != NULL)
	{
		LogString("Warning : if connection name  %s does not exits , it will be created with LAN Settings!\n", ConnectionName);
		LogString("Warning if connection name contains non ASCII character (accentuated), the created entry will not be correct on Windows Creators Update\n");
	}

	switch (action)
	{
	case SETPROXY_ACTION::AUTO:
		if (TRUE == ConfigureProxy(SETPROXY_ACTION::AUTO))
		{
			LogString("Successfully configured WinINet to use the Automatic Proxy detection.");
			iReturn = 0;
		}
		else
		{
			LogString("Failed to configure the Automatic Proxy detection.");
		}
		break;
	case SETPROXY_ACTION::RESET:
		if (TRUE == ConfigureProxy(SETPROXY_ACTION::RESET))
		{
			LogString("Successfully reset of WinINet proxy settings (including removing DIRECT!)");
			iReturn = 0;
		}
		else
		{
			LogString("Failed to reset WinINet proxy settings.");
		}
		break;
	case SETPROXY_ACTION::MANUAL:
		if (TRUE == ConfigureProxy(SETPROXY_ACTION::MANUAL))
		{
			LogString("Successfully configured WinINet to use Manual Proxy settings.");
			iReturn = 0;
		}
		else
		{
			LogString("Failed to configure the Proxy settings to Manual.");
		}
		break;

	case SETPROXY_ACTION::MANUALSERVER:
		if (TRUE == ConfigureProxy(SETPROXY_ACTION::MANUAL))
		{
			if (TRUE == SetProxyServer(pszBuffer))
			{
				LogString("Successfully configured WinINet Manual Proxy server to %S.", pszBuffer);
				iReturn = 0;
			}
			else
			{
				LogString("Failed to configure the Manual Proxy server using %S.", pszBuffer);
			}
		}
		else
		{
			LogString("Failed to configure the Proxy settings to Manual.");
		}
		break;
	case SETPROXY_ACTION::DIRECT:
		if (TRUE == ConfigureProxy(SETPROXY_ACTION::DIRECT))
		{
			LogString("Successfully configured WinINet to use Direct Internet access. The Proxy setting have been turned off.");
			iReturn = 0;
		}
		else
		{
			LogString("Failed to configure the Proxy setting for Direct Internet Access.");
		}
		break;
	case SETPROXY_ACTION::BYPASS:
		if (TRUE == SetProxyByPass(pszBuffer))
		{
			LogString("Successfully configured WinINet Proxy ByPass settings to %S.", pszBuffer);
			iReturn = 0;
		}
		else
		{
			LogString("Failed to configure the Proxy ByPass settings.");
		}
		break;
	case SETPROXY_ACTION::AUTOCONFIG:
		if (TRUE == ConfigureProxy(SETPROXY_ACTION::AUTOCONFIG))
		{

			if (TRUE == SetProxyAutoConfig(pszBuffer))
			{
				LogString("Successfully configured WinINet AutoconfigURL to %S.", pszBuffer);
				iReturn = 0;
			}
			else
			{
				LogString("Failed to configure IE to AutoconfigURL using %S.", pszBuffer);
			}
		}
		else
		{
			LogString("Failed to configure the Proxy settings to Manual.");
		}
		break;
	case SETPROXY_ACTION::SHOW:
		if (ConnectionName != NULL)
		{
			LogString("Currrent proxy settings for Connection : %s\n", ConnectionName);
		}
		else
		{
			LogString("Currrent proxy settings for Lan Settings\n");
		}
		LogString("***********************\n");
		GetProxySettings();
		LogString("\n***********************\n");
		return 0L;
	default:
		Usage();
		return 0L;
	}

	if (ConnectionName != NULL)
	{
		LogString("New proxy settings for Connection : %s\n", ConnectionName);
	}
	else
	{
		LogString("New proxy settings for Lan Settings\n");
	}
	LogString("***********************\n");
	GetProxySettings();
	LogString("\n***********************\n");

	if (SUCCEEDED(hrCoInit))
	{
		CoUninitialize();
	}

	return iReturn;

}

BOOL EnumConnections()
{
	DWORD dwCb = 0;
	DWORD dwRet = ERROR_SUCCESS;
	DWORD dwEntries = 0;
	LPRASENTRYNAME lpRasEntryName = NULL;

	// Call RasEnumEntries with lpRasEntryName = NULL. dwCb is returned with the required buffer size and 
	// a return code of ERROR_BUFFER_TOO_SMALL
	dwRet = RasEnumEntries(NULL, NULL, lpRasEntryName, &dwCb, &dwEntries);

	if (dwRet == ERROR_BUFFER_TOO_SMALL) {
		// Allocate the memory needed for the array of RAS entry names.
		lpRasEntryName = (LPRASENTRYNAME)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwCb);
		if (lpRasEntryName == NULL) {
			wprintf(L"HeapAlloc failed!\n");
			return FALSE;
		}
		// The first RASENTRYNAME structure in the array must contain the structure size
		lpRasEntryName[0].dwSize = sizeof(RASENTRYNAME);

		// Call RasEnumEntries to enumerate all RAS entry names
		dwRet = RasEnumEntries(NULL, NULL, lpRasEntryName, &dwCb, &dwEntries);

		// If successful, print the RAS entry names 
		if (ERROR_SUCCESS == dwRet) {
			wprintf(L"The following RAS entry names were found:\n");
			for (DWORD i = 0; i < dwEntries; i++) {
				wprintf(L"%S\n", lpRasEntryName[i].szEntryName);
			}
		}
		//Deallocate memory for the connection buffer
		HeapFree(GetProcessHeap(), 0, lpRasEntryName);
		lpRasEntryName = NULL;
		return FALSE;
	}

	// There was either a problem with RAS or there are RAS entry names to enumerate    
	if (dwEntries >= 1) {
		wprintf(L"The operation failed to acquire the buffer size.\n");
	}
	else {
		wprintf(L"There were no RAS entry names found:.\n");
	}
	return TRUE;
}

void LogString(const TCHAR *lpsz, ...)
{
	TCHAR szBuffer[1024] = { 0 };
	va_list     list;
	va_start(list, lpsz);
	HRESULT hr = StringCchVPrintf(szBuffer, ARRAYSIZE(szBuffer), lpsz, list);

	if (SUCCEEDED(hr) && strlen(szBuffer))
	{
		printf("%s\r\n", szBuffer);	
	}
	else
	{
		printf("LogString error\n");
	}
	va_end(list);
}


void NotifyProxyChanged()
{
	//To alert all available WinInet instances, set the Buffer parameter of InternetSetOption to NULL and BufferLength to 0 when passing this option
	LogString("Calling InternetSetOption with NULL handle and option INTERNET_OPTION_PROXY_SETTINGS_CHANGED");
	InternetSetOption(NULL, INTERNET_OPTION_PROXY_SETTINGS_CHANGED, NULL, 0);
}


/*
#define INTERNET_PER_CONN_FLAGS                         1
#define INTERNET_PER_CONN_PROXY_SERVER                  2
#define INTERNET_PER_CONN_PROXY_BYPASS                  3
#define INTERNET_PER_CONN_AUTOCONFIG_URL                4
#define INTERNET_PER_CONN_AUTODISCOVERY_FLAGS           5
#define INTERNET_PER_CONN_AUTOCONFIG_SECONDARY_URL      6
#define INTERNET_PER_CONN_AUTOCONFIG_RELOAD_DELAY_MINS  7
#define INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_TIME   8
#define INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_URL    9
#define INTERNET_PER_CONN_FLAGS_UI                      10
*/

BOOL bProxyTypeDirect = FALSE;
BOOL bProxyTypeProxy = FALSE;
BOOL bProxyTypeAutoConfigUrl = FALSE;
BOOL bProxyTypeAutoDetect = FALSE;

BOOL DumpPerConnOption(INTERNET_PER_CONN_OPTION &Option)
{
	if (Option.Value.dwValue == 0)
		return TRUE;
	switch (Option.dwOption)
	{
	case INTERNET_PER_CONN_FLAGS: //1
	case INTERNET_PER_CONN_FLAGS_UI: //10
		if (Option.dwOption == INTERNET_PER_CONN_FLAGS)
		{
			printf("(%d)INTERNET_PER_CONN_FLAGS. Connection type value: %d\n", Option.dwOption, Option.Value.dwValue);
		}
		if (Option.dwOption == INTERNET_PER_CONN_FLAGS_UI)
		{
			printf("\n(%d)INTERNET_PER_CONN_FLAGS_UI. Connection type value: %d\n", Option.dwOption, Option.Value.dwValue);
		}
		/*Sets or retrieves the connection type. The Value member will contain one or more of the following values: 
			PROXY_TYPE_DIRECT
			The connection does not use a proxy server. 
			PROXY_TYPE_PROXY
			The connection uses an explicitly set proxy server. 
			PROXY_TYPE_AUTO_PROXY_URL
			The connection downloads and processes an automatic configuration script at a specified URL. 
			PROXY_TYPE_AUTO_DETECT
			The connection automatically detects settings. 
			//
			// PER_CONN_FLAGS
			//
			#define PROXY_TYPE_DIRECT                               0x00000001   // direct to net
			#define PROXY_TYPE_PROXY                                0x00000002   // via named proxy
			#define PROXY_TYPE_AUTO_PROXY_URL                       0x00000004   // autoproxy URL
			#define PROXY_TYPE_AUTO_DETECT                          0x00000008   // use autoproxy detection
		*/

		if (Option.Value.dwValue & PROXY_TYPE_DIRECT)
		{
			printf("\tPROXY_TYPE_DIRECT(%d)\tThe connection does not use a proxy server.\n", PROXY_TYPE_DIRECT);	
			bProxyTypeDirect = TRUE;
			if (Option.Value.dwValue == PROXY_TYPE_DIRECT)
			{
				//No need to look at other options
				return FALSE;
			}
		}
		if (Option.Value.dwValue & PROXY_TYPE_PROXY)
		{
			printf("\tPROXY_TYPE_PROXY(%d)\tThe connection uses an explicitly set proxy server.\n", PROXY_TYPE_PROXY);
			bProxyTypeProxy = TRUE;
		}
		if (Option.Value.dwValue & PROXY_TYPE_AUTO_PROXY_URL)
		{
			printf("\tPROXY_TYPE_AUTO_PROXY_URL(%d)\tThe connection downloads and processes an automatic configuration script at a specified URL.\n", PROXY_TYPE_AUTO_PROXY_URL);
			bProxyTypeAutoConfigUrl = TRUE;
		}
		if (Option.Value.dwValue & PROXY_TYPE_AUTO_DETECT)
		{
			printf("\tPROXY_TYPE_AUTO_DETECT(%d)\tThe connection automatically detects settings.\n", PROXY_TYPE_AUTO_DETECT);
			bProxyTypeAutoDetect = TRUE;
		}
		break;

	case INTERNET_PER_CONN_PROXY_SERVER: //2
		//Sets or retrieves a string containing the proxy servers.
		if (bProxyTypeProxy)
		{
			printf("\n(%d)INTERNET_PER_CONN_PROXY_SERVER\n", Option.dwOption);
			if (Option.Value.pszValue != NULL)
			{
				printf("\tProxy server : %s\n", Option.Value.pszValue);
				GlobalFree(Option.Value.pszValue);
			}
		}
		break;
	case INTERNET_PER_CONN_PROXY_BYPASS:  //3
		if (bProxyTypeProxy)
		{
			printf("\n(%d)INTERNET_PER_CONN_PROXY_BYPASS\n", Option.dwOption);
			//Sets or retrieves a string containing the URLs that do not use the proxy server. 
			if (Option.Value.pszValue != NULL)
			{
				printf("\tProxy bypass list :%s\n", Option.Value.pszValue);
				GlobalFree(Option.Value.pszValue);
			}
		}
		break;
	case INTERNET_PER_CONN_AUTOCONFIG_URL: //4
		if (bProxyTypeAutoConfigUrl)
		{
			printf("\n(%d)INTERNET_PER_CONN_AUTOCONFIG_URL\n", Option.dwOption);
			//Sets or retrieves a string containing the URL to the automatic configuration script. 
			if (Option.Value.pszValue != NULL)
			{
				printf("\tAutoConfig url : %s\n", Option.Value.pszValue);
				GlobalFree(Option.Value.pszValue);
			}
		}
		break;
	case INTERNET_PER_CONN_AUTODISCOVERY_FLAGS: //5
		if (bProxyTypeAutoDetect)
		{
			printf("\n(%d)INTERNET_PER_CONN_AUTODISCOVERY_FLAGS value: %d\n", Option.dwOption, Option.Value.dwValue);
			//Sets or retrieves the automatic discovery settings.The Value member will contain one or more of the following values :
			/*	AUTO_PROXY_FLAG_ALWAYS_DETECT
				Always automatically detect settings.
				AUTO_PROXY_FLAG_CACHE_INIT_RUN
				Indicates that the cached results of the automatic proxy configuration script should be used, instead of actually running the script, unless the cached file has expired.
				AUTO_PROXY_FLAG_DETECTION_RUN
				Automatic detection has been run at least once on this connection.
				AUTO_PROXY_FLAG_DETECTION_SUSPECT
				Not currently supported.
				AUTO_PROXY_FLAG_DONT_CACHE_PROXY_RESULT
				Do not allow the caching of the result of the automatic proxy configuration script.
				AUTO_PROXY_FLAG_MIGRATED
				The setting was migrated from a Microsoft Internet Explorer 4.0 installation, and automatic detection should be attempted once.
				AUTO_PROXY_FLAG_USER_SET
				The user has explicitly set the automatic detection.*/
			switch (Option.Value.dwValue)
			{
			case AUTO_PROXY_FLAG_ALWAYS_DETECT:
				printf("\tAUTO_PROXY_FLAG_ALWAYS_DETECT(0x%X)\tAlways automatically detect settings.\n", Option.Value.dwValue);
				break;
			case AUTO_PROXY_FLAG_CACHE_INIT_RUN:
				printf("\tAUTO_PROXY_FLAG_CACHE_INIT_RUN(0x%X)\tThe cached results of the automatic proxy configuration script should be used, instead of actually running the script, unless the cached file has expired.\n", Option.Value.dwValue);
				break;
			case AUTO_PROXY_FLAG_DETECTION_RUN:
				printf("\tAUTO_PROXY_FLAG_DETECTION_RUN(0x%X)\tAutomatic detection has been run at least once on this connection.\n", Option.Value.dwValue);
				break;
			case AUTO_PROXY_FLAG_DETECTION_SUSPECT:
				printf("\tAUTO_PROXY_FLAG_DETECTION_SUSPECT(0x%X)\tNot currently supported. \n", Option.Value.dwValue);
				break;
			case AUTO_PROXY_FLAG_DONT_CACHE_PROXY_RESULT:
				printf("\tAUTO_PROXY_FLAG_DONT_CACHE_PROXY_RESULT(0x%X)\tDo not allow the caching of the result of the automatic proxy configuration script.\n", Option.Value.dwValue);
				break;
			case AUTO_PROXY_FLAG_MIGRATED:
				printf("\tAUTO_PROXY_FLAG_FLAG_MIGRATED(0x%X)\tThe setting was migrated from a Microsoft Internet Explorer 4.0 installation, and automatic detection should be attempted once.\n", Option.Value.dwValue);
				break;
			case AUTO_PROXY_FLAG_USER_SET:
				//not true
				//printf("\tUSER_SET(0x%X)\tThe user has explicitly set the automatic detection.\n", Option.Value.dwValue);
				printf("\tAUTO_PROXY_FLAG_USER_SET(0x%X)\tThe user changed the setting.\n", Option.Value.dwValue);
				break;
			default:
				printf("\tUnknown automatic descovery setting %d\n", Option.Value.dwValue);
				break;
			}
		}
		break;
	case INTERNET_PER_CONN_AUTOCONFIG_SECONDARY_URL: //6
		//Chained autoconfig URL.Used when the primary autoconfig URL points to an INS file that sets a second autoconfig URL for proxy information.
		if (bProxyTypeAutoConfigUrl)
		{
			printf("\n(%d)INTERNET_PER_CONN_AUTOCONFIG_SECONDARY_URL\n", Option.dwOption);
		}
		break;
	case INTERNET_PER_CONN_AUTOCONFIG_RELOAD_DELAY_MINS: //7
		//of minutes until automatic refresh of autoconfig URL by autodiscovery
		if (bProxyTypeAutoConfigUrl)
		{
			printf("\n(%d)INTERNET_PER_CONN_AUTOCONFIG_RELOAD_DELAY_MINS\n", Option.dwOption);
		}
		break;
	case INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_TIME: //8
		//Read only option.Returns the time the last known good autoconfig URL was found using autodiscovery.
		if (bProxyTypeAutoDetect)
		{
			printf("\n(%d)INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_TIME\n", Option.dwOption);
		}
		break;
	case INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_URL: //9
		//Read only option.Returns the last known good URL found using autodiscovery.
		if (bProxyTypeAutoDetect)
		{
			printf("(%d)INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_URL\n", Option.dwOption);
		}
		break;
	default:
		printf("Unknown option %d\n", Option.dwOption);
		break;
	}
	return TRUE;
}

BOOL GetProxySettings()
{
#define MAX_OPTIONS_NUMBER 10

	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[10];
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	bProxyTypeDirect = FALSE;
	bProxyTypeProxy = FALSE;
	bProxyTypeAutoConfigUrl = FALSE;
	bProxyTypeAutoDetect = FALSE;

	Option[0].dwOption = INTERNET_PER_CONN_FLAGS;
	Option[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
	Option[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
	Option[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
	Option[4].dwOption = INTERNET_PER_CONN_AUTODISCOVERY_FLAGS;
	Option[5].dwOption = INTERNET_PER_CONN_AUTOCONFIG_SECONDARY_URL;
	Option[6].dwOption = INTERNET_PER_CONN_AUTOCONFIG_RELOAD_DELAY_MINS;
	Option[7].dwOption = INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_TIME;
	Option[8].dwOption = INTERNET_PER_CONN_AUTOCONFIG_LAST_DETECT_URL;
	Option[9].dwOption = INTERNET_PER_CONN_FLAGS_UI;

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = ConnectionName;
	List.dwOptionCount = MAX_OPTIONS_NUMBER;
	List.dwOptionError = 0;
	List.pOptions = Option;

	LogString("Calling InternetQueryOption with NULL handle and option INTERNET_OPTION_PER_CONNECTION_OPTION and 10 options");
	if (!InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &nSize))
	{
		printf("InternetQueryOption failed! (%d)\n", GetLastError());
		exit(-1L);
	}

	/*INTERNET_VERSION_INFO      Version;
	nSize = sizeof(INTERNET_VERSION_INFO);

	LogString("Calling InternetQueryOption with NULL handle and option INTERNET_OPTION_VERSION");
	InternetQueryOption(NULL, INTERNET_OPTION_VERSION, &Version, &nSize);
	printf("Version : %d.%d\n", Version.dwMajorVersion, Version.dwMinorVersion);*/

	for (unsigned int numOption = 0; numOption < MAX_OPTIONS_NUMBER; numOption++)
	{
		if (DumpPerConnOption(Option[numOption]) == FALSE)
		{
			break;
		}
	}

	return TRUE;
}
BOOL ConfigureProxy(SETPROXY_ACTION action)
{
	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[2];
	unsigned long                    cbList = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	BOOL bReturn = FALSE;

	if (ConnectionName != NULL)
	{
		LogString("Currrent proxy settings for Connection : %s\n", ConnectionName);
	}
	else
	{
		LogString("Currrent proxy settings for Lan Settings\n");
	}

	LogString("***********************\n");
	GetProxySettings();
	LogString("\n***********************\n");

	/*
	** First have to query for autodiscovery flags. They're used later in the
	** case of setting autodetect.
	*/
	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = ConnectionName;
	List.dwOptionCount = 2;
	List.dwOptionError = 0;
	List.pOptions = Option;

	Option[1].dwOption = INTERNET_PER_CONN_AUTODISCOVERY_FLAGS;
	Option[0].dwOption = INTERNET_PER_CONN_FLAGS;
	LogString("Calling InternetQueryOption with NULL handle and option INTERNET_OPTION_PER_CONNECTION_OPTION/INTERNET_PER_CONN_AUTODISCOVERY_FLAGS and INTERNET_PER_CONN_FLAGS");
	if (FALSE != InternetQueryOption(
		NULL,
		INTERNET_OPTION_PER_CONNECTION_OPTION,
		&List,
		&cbList))
	{
		LogString("InternetQueryOption succeeded");

		/*
		** InetCPL sets PROXY_TYPE_DIRECT regardless of the state of the
		** checkboxes on the dialog.
		**
		** The reason this needs to be done is so that if autodetect fails for
		** whatever reason (autodetect script fails, doesn't produce a correct
		** result, etc.), wininet will fall back to direct.
		**
		** InetCPL also sets AUTO_PROXY_FLAG_USER_SET when the user checks the
		** autodetect box, which is essentially what's happening here. It also
		** clears AUTO_PROXY_FLAG_DETECTION_RUN when autodetect is turned off.
		**
		** AUTO_PROXY_FLAG_USER_SET needs to be set to inform wininet that the
		** user explicitly chose autodetection. This disables a mechanism
		** wininet employs to disable autodetection if it fails, useful in the
		** case of an out-of-box machine that comes pre-configured with
		** autodetect enabled.
		*/

		//Should be in the retrieved option
		//Option[1].Value.dwValue = PROXY_TYPE_DIRECT;
		//LogString("Option:INTERNET_PER_CONN_FLAGS Value:PROXY_TYPE_DIRECT");
		if (SETPROXY_ACTION::AUTO == action)
		{
			Option[0].Value.dwValue |= PROXY_TYPE_AUTO_DETECT;
			LogString("INTERNET_PER_CONN_FLAGS Value |= PROXY_TYPE_AUTO_DETECT");
			Option[1].Value.dwValue |= AUTO_PROXY_FLAG_USER_SET;
			LogString("INTERNET_PER_CONN_AUTODISCOVERY_FLAGS Value |= AUTO_PROXY_FLAG_USER_SET");
		}
		else
		{
			Option[1].Value.dwValue &= ~AUTO_PROXY_FLAG_DETECTION_RUN;
			LogString("INTERNET_PER_CONN_AUTODISCOVERY_FLAGS Value &= ~AUTO_PROXY_FLAG_DETECTION_RUN");
			if (SETPROXY_ACTION::MANUAL == action)
			{
				Option[0].Value.dwValue |= PROXY_TYPE_PROXY;
				LogString("INTERNET_PER_CONN_FLAGS Value |= PROXY_TYPE_PROXY");
			}
			else if (SETPROXY_ACTION::AUTOCONFIG == action)
			{
				Option[0].Value.dwValue |= PROXY_TYPE_AUTO_PROXY_URL;
				LogString("INTERNET_PER_CONN_FLAGS Value |= PROXY_TYPE_AUTO_PROXY_URL");
			}
			else if (action == SETPROXY_ACTION::RESET)
			{
				Option[0].Value.dwValue = 0;
				LogString("INTERNET_PER_CONN_FLAGS Value set to zero");
				Option[1].Value.dwValue &= ~AUTO_PROXY_FLAG_USER_SET;
				LogString("INTERNET_PER_CONN_AUTODISCOVERY_FLAGS Value &= ~AUTO_PROXY_FLAG_USER_SET");
			}
			else if (action == SETPROXY_ACTION::DIRECT)
			{
				Option[0].Value.dwValue |= PROXY_TYPE_DIRECT;
				LogString("INTERNET_PER_CONN_FLAGS |= PROXY_TYPE_DIRECT");
			}
		}

		LogString("Calling InternetSetOption with NULL handle and option INTERNET_OPTION_PER_CONNECTION_OPTION");

		if (InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, cbList))
		{
			//Notify IE of the Proxy Setting change
			NotifyProxyChanged();
			bReturn = TRUE;
		}
		else
		{
			LogString("InternetSetOption failed for ConfigureProxy! (%d)\n", GetLastError());
		}
	}
	else
	{
		LogString("InternetQueryOption failed for ConfigureProxy! (%d)\n", GetLastError());
	}
	return bReturn;
}

BOOL SetProxyOption(__in_opt INT iProxyOption, __in_opt TCHAR * pszValue)
{
	BOOL bReturn = FALSE;

	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[1];
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	Option[0].dwOption = iProxyOption;
	Option[0].Value.pszValue = pszValue;

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = ConnectionName;
	List.dwOptionCount = 1;
	List.dwOptionError = 0;
	List.pOptions = Option;

	LogString("Calling InternetSetOption with NULL handle and option INTERNET_OPTION_PER_CONNECTION_OPTION with option %d/%X value : %s", iProxyOption, iProxyOption, pszValue);
	if (InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, nSize))
	{
		NotifyProxyChanged();
		bReturn = TRUE;
	}
	else
	{
		LogString("InternetSetOption failed for SetProxyOption! (%d)\n", GetLastError());
	}
	return bReturn;

}
BOOL SetProxyServer(__in_opt TCHAR * pszHostPort)
{
	return SetProxyOption(INTERNET_PER_CONN_PROXY_SERVER, pszHostPort);
}
BOOL SetProxyByPass(__in_opt TCHAR * pszByPass)
{
	return SetProxyOption(INTERNET_PER_CONN_PROXY_BYPASS, pszByPass);
}
BOOL SetProxyAutoConfig(__in_opt TCHAR * pszAutoURL)
{
	return SetProxyOption(INTERNET_PER_CONN_AUTOCONFIG_URL, pszAutoURL);
}

