
#include <windows.h>
#include <newdev.h>
#include <setupapi.h>
#include <locale.h>

//#include <stdio.h>
#include <tchar.h>
#include <iostream>

#include "Util.h"

#include "WdmDriverInstall.h"

#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "setupapi.lib")

using namespace std;

#ifndef MAX_DEVICE_ID_LEN
#define MAX_DEVICE_ID_LEN     200
#define MAX_DEVNODE_ID_LEN    MAX_DEVICE_ID_LEN
#define MAX_GUID_STRING_LEN   39          // 38 chars + terminator null
#define MAX_CLASS_NAME_LEN    32
#endif


WORD g_wVender = 0;
WORD g_wHardware = 0;
TCHAR g_strVender[20][64] = { 0 };
TCHAR g_strHardware[20][64] = { 0 };
TCHAR g_strHID[MAX_PATH + 1] = { 0 };

//��ӡ����
VOID ShowErrorMsg(int Count, LPCWSTR szData)
{
	printf("%d\n%s", Count, &szData);
}


//�����ַ�
VOID FindComma(LPSTR szData)
{
	WORD wLen = (WORD)strlen(szData);
	WORD wIdx;
	WORD wLoop;
	CHAR szTmp[128] = { 0 };

	for (wIdx = 0, wLoop = 0; wLoop < wLen; wLoop++)
	{
		if (szData[wLoop] == ',')
			szData[wLoop] = '.';
		else if (szData[wLoop] == ' ')
			continue;
		szTmp[wIdx++] = szData[wLoop];
	}
	memcpy(szData, szTmp, wIdx * sizeof(char));
	szData[wIdx] = 0;
}

//ȥ���ַ�����ߵĿո�
VOID StrLTrim(LPSTR szData)
{
	LPSTR ptr = szData;
	//�ж��Ƿ�Ϊ�ո�
	while (isspace(*ptr))
		ptr++;

	if (strcmp(ptr, szData))
	{
		WORD wLen = (WORD)(strlen(szData) - (ptr - szData));
		memmove(szData, ptr, (wLen + 1) * sizeof(char));
	}
}

//ȥ���ַ����ұߵĿո�
VOID StrRTrim(LPSTR szData)
{
	LPSTR ptr = szData;
	LPSTR pTmp = NULL;

	//debugģʽ�� ʹ��isspace�ж����� ��Ҫ���ñ���
#if defined(WIN32) && defined(_DEBUG)
	char* locale = setlocale(LC_ALL, ".OCP");
#endif 

	while (*ptr != 0)
	{
		//�ж��Ƿ�Ϊ�ո�
		if (isspace(*ptr))
		{
			if (!pTmp)
				pTmp = ptr;
		}
		else
			pTmp = NULL;
		ptr++;
	}

	if (pTmp)
	{
		*pTmp = 0;
		memmove(szData, szData, strlen(szData) - strlen(pTmp));
	}
}

//���ַ����ұ߿�ʼ��ȡ�ַ���
VOID StrRight(LPSTR szData, WORD wCount)
{
	WORD wLen = (WORD)strlen(szData) - wCount;

	if (wCount > 0x7FFF)//����
		wCount = 0;
	if (wCount >= (WORD)strlen(szData))
		return;

	memmove(szData, szData + wLen, wCount * sizeof(char));
	szData[wCount] = 0;
}

VOID ConvertGUIDToString(const GUID guid, LPSTR pData)
{
	CHAR szData[30] = { 0 };
	CHAR szTmp[3] = { 0 };
	WORD wLoop;

	sprintf_s(pData, _countof(szData), "%04X-%02X-%02X-", guid.Data1, guid.Data2, guid.Data3);
	for (wLoop = 0; wLoop < 8; wLoop++)
	{
		if (wLoop == 2)
			strcat_s(szData, "-");
		sprintf_s(szTmp, _countof(szTmp), "%02X", guid.Data4[wLoop]);
		strcat_s(szData, szTmp);
	}

	memcpy(pData + strlen(pData), szData, strlen(szData));
}

BOOL AnsiToUnicode(LPCSTR Source, const WORD sLen, LPWSTR Destination, const WORD wLen)
{
	return MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, Source, sLen, Destination, wLen);
}

BOOL UnicodeToAnsi(LPCWSTR Source, const WORD wLen, LPSTR Destination, const WORD sLen)
{
	return WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, Source, wLen, Destination, sLen, 0L, 0L);
}

// ��ʼ��ȫ�ֱ���
__inline VOID InitialGlobalVar()
{
	WORD wLoop;

	g_wVender = g_wHardware = 0;
	for (wLoop = 0; wLoop < 20; wLoop++)
	{
		RtlZeroMemory(g_strVender[wLoop], sizeof(TCHAR) * 64);
		RtlZeroMemory(g_strHardware[wLoop], sizeof(TCHAR) * 64);
	}
}

//��װ��������
__inline BOOL IsInstalled()
{
	HDEVINFO hDevInfo = 0L;
	SP_DEVINFO_DATA spDevInfoData = { 0L };
	WORD wIdx;
	BOOL bIsFound;

	//�õ��豸��Ϣ�ṹ�ľ��
	hDevInfo = SetupDiGetClassDevs(0L, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiGetClassDevs"));
		return FALSE;
	}

	spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	wIdx = 0;
	bIsFound = 0;
	while (++wIdx)
	{
		//�ҵ����е�Ӳ���豸,���ҿ��Եõ����е�Ӳ���豸����ϸ��Ϣ
		if (SetupDiEnumDeviceInfo(hDevInfo, wIdx, &spDevInfoData))
		{
			LPTSTR ptr;
			LPBYTE pBuffer = NULL;
			DWORD dwData = 0L;
			DWORD dwRetVal;
			DWORD dwBufSize = 0L;

			while (TRUE)
			{
				//������ǰ��õ���ָ��ĳһ�������豸��Ϣ���ϵ�ָ����ȡ��ĳһ����Ϣ
				dwRetVal = SetupDiGetDeviceRegistryProperty(hDevInfo, &spDevInfoData, SPDRP_HARDWAREID,
					&dwData, (PBYTE)pBuffer, dwBufSize, &dwBufSize);
				if (!dwRetVal)
					dwRetVal = GetLastError();
				else
					break;
				if (dwRetVal == ERROR_INVALID_DATA)
					break;
				else if (dwRetVal == ERROR_INSUFFICIENT_BUFFER)
				{
					if (pBuffer)
						LocalFree(pBuffer);
					pBuffer = (LPBYTE)LocalAlloc(LPTR, dwBufSize);
				}
				else
				{
					ShowErrorMsg(dwRetVal, _T("SetupDiGetDeviceRegistryProperty"));
					//����һ���豸��Ϣ����
					SetupDiDestroyDeviceInfoList(hDevInfo);
					return FALSE;
				}
			}

			if (dwRetVal == ERROR_INVALID_DATA)
				continue;

			for (ptr = (LPTSTR)pBuffer; *ptr && (ptr < (LPTSTR)&pBuffer[dwBufSize]); ptr += _tcslen(ptr) + sizeof(TCHAR))
			{
				WORD wLoop;

				for (wLoop = 0; wLoop < g_wHardware; wLoop++)
				{
					if (!_tcscmp(g_strHardware[wLoop], ptr))
					{
						bIsFound = TRUE;
						break;
					}
				}
			}
			if (pBuffer)
				LocalFree(pBuffer);
			if (bIsFound)
				break;
		}
	}
	//����һ���豸��Ϣ����
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return bIsFound;
}

//Ѱ��ָ���Ľ��� ����ҵ�����TRUE ��֮����FALSE
BOOL FindSectionName(FILE *pFile, const char *szKey)
{
	char szData[256] = { 0 };

	if (!pFile)
		return FALSE;

	//���ļ��ڲ���λ��ָ������ָ��һ������������/�ļ����Ŀ�ͷ
	rewind(pFile);
	//ѭ����ȡ�ļ�����
	while (!feof(pFile))
	{
		//��ȡһ��
		fgets(szData, 255, pFile);
		//ȥ��ǰ��ո�
		StrLTrim(szData);
		StrRTrim(szData);

		if (strcmp(szKey, szData) == 0)
			return TRUE;
	}
	return FALSE;
}

//�õ�INF�ļ��нڵ�����
__inline BOOL GetSectionData(FILE* pFile, const char* szKey, const char bIsVender)
{
	char szData[128] = { 0 };

	if (bIsVender)
		strcpy_s(szData, szKey);
	else
		sprintf_s(szData, _countof(szData), "[%s]", szKey);

	if (FindSectionName(pFile, szData) == FALSE)
		return FALSE;

	RtlZeroMemory(szData, sizeof(char) * 128);
	while (!feof(pFile))
	{
		char *str = NULL;
		fgets(szData, 127, pFile);
		szData[strlen(szData) - 1] = 0;
		StrLTrim(szData);
		StrRTrim(szData);
		if (!*szData)
			continue;
		if (szData[0] == ';')
			continue;

		if (strchr(szData, '['))
		{
			StrLTrim(szData);
			if (szData[0] != ';')
				return 1;
			else
				continue;
		}

		if (bIsVender)
			str = strchr(szData, '=');
		else
			str = strchr(szData, ',');

		if (*str)
		{
			char szTmp[128] = { 0 };
			WORD pos = (WORD)(str - szData + 1);

			StrRight(szData, (short)(strlen(szData) - pos));
			StrLTrim(szData);
			StrRTrim(szData);
			FindComma(szData);
			if (bIsVender)
			{
				AnsiToUnicode(szData, strlen(szData), g_strVender[g_wVender++], 64);
			}
			else
			{
				AnsiToUnicode(szData, strlen(szData), g_strHardware[g_wHardware++], 64);
			}
		}/* end if */
	}
	return TRUE;
}

//�õ�INF�ļ��������
BOOL GetINFData(FILE *pFile)
{
	WORD wLoop;

	if (!g_wVender || !g_wHardware)
		InitialGlobalVar();
	if (GetSectionData(pFile, "[Manufacturer]", TRUE) == FALSE)
		return 0;

	for (wLoop = 0; wLoop < g_wVender; wLoop++)
	{
		CHAR szVender[64] = { 0 };
		UnicodeToAnsi(g_strVender[wLoop], _tcslen(g_strVender[wLoop]), szVender, 64);
		GetSectionData(pFile, szVender, FALSE);
	}
	if (g_wHardware != 0)
	{
		if (IsInstalled() == TRUE)//����Ѿ���װ
			return FALSE;
		else
			return TRUE;
	}
	return FALSE;
}

//ʵ���Եİ�װ����
__inline BOOL InstallClassDriver(LPCTSTR theINFName)
{
	GUID guid = { 0 };
	SP_DEVINFO_DATA spDevData = { 0 };
	HDEVINFO hDevInfo = 0L;
	TCHAR className[MAX_CLASS_NAME_LEN] = { 0 };
	LPTSTR pHID = NULL;
	WORD wLoop;
	BOOL bRebootRequired;

	//ȡ�ô�������GUIDֵ
	if (!SetupDiGetINFClass(theINFName, &guid, className, MAX_CLASS_NAME_LEN, 0))
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiGetINFClass"));
		return FALSE;
	}

	//�����豸��Ϣ���б�
	hDevInfo = SetupDiCreateDeviceInfoList(&guid, 0);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiCreateDeviceInfoList"));
		return FALSE;
	}

	spDevData.cbSize = sizeof(SP_DEVINFO_DATA);
	//�����豸��Ϣ��
	if (!SetupDiCreateDeviceInfo(hDevInfo, className, &guid, 0L, 0L, DICD_GENERATE_ID, &spDevData))
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiCreateDeviceInfo"));
		//����һ���豸��Ϣ����
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return FALSE;
	}

	for (wLoop = 0; wLoop < g_wHardware; wLoop++)
	{
		if (pHID)
			LocalFree(pHID);

		pHID = (LPTSTR)LocalAlloc(LPTR, _tcslen(g_strHardware[wLoop]) * 2 * sizeof(TCHAR));
		if (!pHID)
		{
			ShowErrorMsg(GetLastError(), _T("LocalAlloc"));
			//����һ���豸��Ϣ����
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return FALSE;
		}

		_tcscpy_s(pHID, _tcslen(g_strHardware[wLoop]) * 2, g_strHardware[wLoop]);
		//�趨Ӳ��ID
		if (!SetupDiSetDeviceRegistryProperty(hDevInfo, &spDevData, SPDRP_HARDWAREID, (PBYTE)pHID,
			(DWORD)(_tcslen(g_strHardware[wLoop]) * 2 * sizeof(TCHAR))))
		{
			ShowErrorMsg(GetLastError(), _T("SetupDiSetDeviceRegistryProperty"));
			//����һ���豸��Ϣ����
			SetupDiDestroyDeviceInfoList(hDevInfo);
			LocalFree(pHID);
			return FALSE;
		}
		//������Ӧ���������ע���豸
		if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, hDevInfo, &spDevData))
		{
			ShowErrorMsg(GetLastError(), _T("SetupDiCallClassInstaller"));
			//����һ���豸��Ϣ����
			SetupDiDestroyDeviceInfoList(hDevInfo);
			LocalFree(pHID);
			return FALSE;
		}

		bRebootRequired = FALSE;
		//��װ���º�Ӳ��ID��ƥ�����������
		if (!UpdateDriverForPlugAndPlayDevices(0L, g_strHardware[wLoop], theINFName,
			INSTALLFLAG_FORCE, &bRebootRequired))
		{
			DWORD dwErrorCode = GetLastError();
			//������Ӧ����������Ƴ��豸
			if (!SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo, &spDevData))
				ShowErrorMsg(GetLastError(), _T("SetupDiCallClassInstaller(Remove)"));
			ShowErrorMsg((WORD)dwErrorCode, _T("UpdateDriverForPlugAndPlayDevices"));
			//����һ���豸��Ϣ����
			SetupDiDestroyDeviceInfoList(hDevInfo);
			LocalFree(pHID);
			return FALSE;
		}
		LocalFree(pHID);
		pHID = NULL;
	}
	//����һ���豸��Ϣ����
	SetupDiDestroyDeviceInfoList(hDevInfo);
	_tprintf(_T("Install Successed\n"));
	return TRUE;
}

// ��װWDM�����Ĳ��Թ���
BOOL StartInstallWDMDriver(LPCTSTR theInfName)
{
	HDEVINFO hDevInfo = 0L;
	GUID guid = { 0L };
	SP_DEVINSTALL_PARAMS spDevInst = { 0L };
	TCHAR strClass[MAX_CLASS_NAME_LEN] = { 0L };

	//ȡ�ô�������GUIDֵ
	if (!SetupDiGetINFClass(theInfName, &guid, strClass, MAX_CLASS_NAME_LEN, 0))
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiGetINFClass"));
		return FALSE;
	}

	//�õ��豸��Ϣ�ṹ�ľ��
	hDevInfo = SetupDiGetClassDevs(&guid, 0L, 0L, DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_PROFILE);
	if (!hDevInfo)
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiGetClassDevs"));
		return FALSE;
	}


	spDevInst.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
	//���ָ���豸�İ�װ��Ϣ
	if (!SetupDiGetDeviceInstallParams(hDevInfo, 0L, &spDevInst))
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiGetDeviceInstallParams"));
		return FALSE;
	}

	spDevInst.Flags = DI_ENUMSINGLEINF;
	spDevInst.FlagsEx = DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
	_tcscpy_s(spDevInst.DriverPath, _countof(spDevInst.DriverPath), theInfName);

	//Ϊ�豸��Ϣ��������һ��ʵ�ʵ��豸��Ϣ��Ԫ���û�����లװ����
	if (!SetupDiSetDeviceInstallParams(hDevInfo, 0, &spDevInst))
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiSetDeviceInstallParams"));
		return FALSE;
	}

	//��ȡ����豸������������Ϣ�б�
	if (!SetupDiBuildDriverInfoList(hDevInfo, 0, SPDIT_CLASSDRIVER))
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiDeviceInstallParams"));
		return FALSE;
	}

	//����һ���豸��Ϣ����
	SetupDiDestroyDeviceInfoList(hDevInfo);

	//���밲װ�豸��������
	return InstallClassDriver(theInfName);
}

// ж��WDM����
VOID UninstallWDMDriver(LPCTSTR theHardware)
{
	SP_DEVINFO_DATA spDevInfoData = { 0 };
	HDEVINFO hDevInfo = 0L;
	WORD wIdx, wCount = 0;

	//�õ��豸��Ϣ�ṹ�ľ��
	hDevInfo = SetupDiGetClassDevs(0L, 0L, 0L, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		ShowErrorMsg(GetLastError(), _T("SetupDiGetClassDevs"));
		return;
	}

	wIdx = 0;
	while (TRUE)
	{
		spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		//�ҵ����е�Ӳ���豸,���ҿ��Եõ����е�Ӳ���豸����ϸ��Ϣ
		if (SetupDiEnumDeviceInfo(hDevInfo, wIdx, &spDevInfoData))
		{
			char Buffer[2048] = { 0 };

			//������ǰ��õ���ָ��ĳһ�������豸��Ϣ���ϵ�ָ����ȡ��ĳһ����Ϣ
			if (SetupDiGetDeviceRegistryProperty(hDevInfo, &spDevInfoData, SPDRP_HARDWAREID,
				0L, (PBYTE)Buffer, 2048, 0L))
			{
				if (!_tcscmp(theHardware, (LPTSTR)Buffer))
				{
					//��ϵͳ��ɾ��һ��ע����豸�ӿ�
					if (!SetupDiRemoveDevice(hDevInfo, &spDevInfoData))
						ShowErrorMsg(GetLastError(), _T("SetupDiRemoveDevice"));
					wCount++;
				}
			}
		}
		else
			break;
		wIdx++;
	}

	if (wCount != 0)
		_tprintf(_T("UnInstall Successed\n"));

	//����һ���豸��Ϣ����
	SetupDiDestroyDeviceInfoList(hDevInfo);
	InitialGlobalVar();
	return;
}

//INF�ļ�·��
const LPTSTR g_pInfPath = _T("E:\\VirtualDisk.inf");

//��ں���
int _tmain(int argc, _TCHAR* argv[])
{
	CHAR szInfPath[MAX_PATH] = { 0 };
	UnicodeToAnsi(g_pInfPath, _tcslen(g_pInfPath), szInfPath, MAX_PATH);
	FILE* pInf;
	errno_t err;

	//DebugBreak();
	PrintUsage();

	if ((err = fopen_s(&pInf, szInfPath, "r")) != 0)
	{
		_tprintf(_T("can not open file %s\n"), g_pInfPath);
		return 0;
	}

	// ��ȡINF�ļ�����
	GetINFData(pInf);
	fclose(pInf);

	// ��װWDM����
	if (_tcscmp(argv[1], TEXT("-install")) == 0)
	{
		if (StartInstallWDMDriver(g_pInfPath) == FALSE)
		{
			_tprintf(_T("Start Install WMD Driver failed\n"));
			return 0;
		}
	}
	// ж��WDM����
	else if (_tcscmp(argv[1], TEXT("-uninstall")) == 0)
	{
		for (WORD wLoop = 0; wLoop < g_wHardware; wLoop++)
			UninstallWDMDriver(g_strHardware[wLoop]);
	}

	else
	{
		cout << "the input is wrong" << endl;
	}

	return 1;
}


