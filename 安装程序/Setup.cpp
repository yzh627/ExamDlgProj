// 河北对口升学计算机程序设计练习系统 - 安装程序
// 功能：内嵌主程序+题库 → 选择目录安装 → 建快捷方式 → 写卸载信息（自带卸载）
// 用法：直接双击安装；以后在安装目录里运行"卸载.exe"即可卸载
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600      // RegDeleteTreeW 需要
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#define APP_TITLE       L"河北对口升学计算机程序设计练习系统"
#define APP_SHORTNAME   L"对口升学练习系统"
#define UNINSTALL_KEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\HebeiExamPractice"
#define DEFAULT_DIR     L"C:\\对口升学练习系统"

// 老 SDK 里可能没有 ARM64 这个常量，补一个（值固定为 12）
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

static HINSTANCE g_hInst = NULL;
static WCHAR g_szDir[MAX_PATH] = { 0 };

// 记录最近一次"从资源写文件"失败的细节，方便报错时把原因说清楚
static WCHAR g_szLastFailFile[MAX_PATH] = { 0 };
static DWORD g_dwLastFailErr = 0;

// 当前系统是不是 64 位（决定装 64 位还是 32 位主程序）
static BOOL Is64BitOS()
{
	SYSTEM_INFO si;
	ZeroMemory(&si, sizeof(si));
	GetNativeSystemInfo(&si);
	return (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
		|| si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64
		|| si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64);
}

// 主程序是不是还在运行（开着会占用文件，导致安装时写不进去）
static BOOL IsMainAppRunning()
{
	BOOL bFound = FALSE;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return FALSE;
	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(pe);
	if (Process32FirstW(hSnap, &pe))
	{
		do
		{
			if (_wcsicmp(pe.szExeFile, L"ExamDlgProj.exe") == 0)
			{
				bFound = TRUE;
				break;
			}
		} while (Process32NextW(hSnap, &pe));
	}
	CloseHandle(hSnap);
	return bFound;
}

// ================= 工具函数 =================

// 从资源里写出文件
static BOOL WriteResourceToFile(int nResId, const WCHAR* pszPath)
{
	// 每次进来先清空上次的失败记录
	g_szLastFailFile[0] = L'\0';
	g_dwLastFailErr = 0;

	HRSRC hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(nResId), RT_RCDATA);
	if (hRes == NULL)
	{
		wcsncpy_s(g_szLastFailFile, MAX_PATH, pszPath, _TRUNCATE);
		g_dwLastFailErr = GetLastError();
		return FALSE;
	}
	DWORD dwSize = SizeofResource(g_hInst, hRes);
	HGLOBAL hGlob = LoadResource(g_hInst, hRes);
	if (hGlob == NULL || dwSize == 0)
	{
		wcsncpy_s(g_szLastFailFile, MAX_PATH, pszPath, _TRUNCATE);
		g_dwLastFailErr = GetLastError();
		return FALSE;
	}
	const void* pData = LockResource(hGlob);
	if (pData == NULL)
	{
		wcsncpy_s(g_szLastFailFile, MAX_PATH, pszPath, _TRUNCATE);
		g_dwLastFailErr = GetLastError();
		return FALSE;
	}

	HANDLE hFile = CreateFileW(pszPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		wcsncpy_s(g_szLastFailFile, MAX_PATH, pszPath, _TRUNCATE);
		g_dwLastFailErr = GetLastError();
		return FALSE;
	}
	DWORD dwWritten = 0;
	BOOL bOK = WriteFile(hFile, pData, dwSize, &dwWritten, NULL) && (dwWritten == dwSize);
	if (!bOK)
	{
		wcsncpy_s(g_szLastFailFile, MAX_PATH, pszPath, _TRUNCATE);
		g_dwLastFailErr = GetLastError();
	}
	CloseHandle(hFile);
	if (!bOK)
		DeleteFileW(pszPath);
	return bOK;
}

// 递归创建目录
static BOOL MakeDirRecursive(const WCHAR* pszDir)
{
	if (CreateDirectoryW(pszDir, NULL))
		return TRUE;
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		return TRUE;

	WCHAR szTmp[MAX_PATH];
	wcsncpy_s(szTmp, MAX_PATH, pszDir, _TRUNCATE);
	for (WCHAR* p = szTmp + 3; *p != L'\0'; p++)
	{
		if (*p == L'\\')
		{
			*p = L'\0';
			CreateDirectoryW(szTmp, NULL);
			*p = L'\\';
		}
	}
	if (CreateDirectoryW(pszDir, NULL))
		return TRUE;
	return (GetLastError() == ERROR_ALREADY_EXISTS);
}

static BOOL GetSpecialDir(int nFolder, WCHAR* pszOut)
{
	return SUCCEEDED(SHGetFolderPathW(NULL, nFolder, NULL, 0, pszOut));
}

// 读注册表里记录的"已安装位置"，没有就返回空串
static void GetInstalledDir(WCHAR* pszOut, int cch)
{
	if (pszOut == NULL || cch <= 0)
		return;
	pszOut[0] = L'\0';
	HKEY hKey = NULL;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return;
	DWORD cb = (DWORD)(cch * sizeof(WCHAR)), dwType = 0;
	RegQueryValueExW(hKey, L"InstallLocation", NULL, &dwType, (BYTE*)pszOut, &cb);
	RegCloseKey(hKey);
}

// 目录里还有东西吗（含隐藏/系统文件，"." ".." 不算）
// 用来判断卸载"是不是真的删干净了"——只看目录还在不在不够准
static BOOL DirHasAnyEntry(const WCHAR* pszDir)
{
	WCHAR szPat[MAX_PATH];
	swprintf_s(szPat, MAX_PATH, L"%s\\*", pszDir);
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(szPat, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return FALSE;   // 打不开：要么不存在，要么没权限，都当作"没有可删的"
	BOOL bFound = FALSE;
	do
	{
		if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
		{
			bFound = TRUE;
			break;
		}
	} while (FindNextFileW(hFind, &fd));
	FindClose(hFind);
	return bFound;
}

// 创建快捷方式
static BOOL CreateShortcut(const WCHAR* pszLnk, const WCHAR* pszTarget, const WCHAR* pszWorkDir)
{
	IShellLinkW* pLink = NULL;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pLink);
	if (FAILED(hr))
		return FALSE;

	pLink->SetPath(pszTarget);
	pLink->SetWorkingDirectory(pszWorkDir);
	pLink->SetDescription(APP_TITLE);

	IPersistFile* pFile = NULL;
	hr = pLink->QueryInterface(IID_IPersistFile, (void**)&pFile);
	if (SUCCEEDED(hr))
	{
		hr = pFile->Save(pszLnk, TRUE);
		pFile->Release();
	}
	pLink->Release();
	return SUCCEEDED(hr);
}

// 取自己所在目录（带结尾反斜杠）
static void GetSelfDir(WCHAR* pszOut, int cch)
{
	GetModuleFileNameW(NULL, pszOut, cch);
	WCHAR* p = wcsrchr(pszOut, L'\\');
	if (p != NULL)
		*(p + 1) = L'\0';
}

// ================= 公共判断：静默模式、日志、命令行、目录安全 =================

// 换位置重装时，如果旧目录里还留着用户自己的文件，在这里记一下，
// 装完在"安装完成"提示里补一句（否则用户会以为旧目录被删干净了）
static WCHAR g_szOldDirKept[MAX_PATH] = { 0 };

// 静默模式（/S）：全程不弹窗，结果写 install.log，靠退出码告诉批处理成败
static BOOL g_bSilent = FALSE;
static WCHAR g_szLogPath[MAX_PATH] = { 0 };
static FILE* g_pLog = NULL;

// 日志：静默模式下唯一能看清"到底做了什么"的地方（机房装失败时先看它）。
// 从 cmd 启动的话顺手在控制台回显一份，现场不用再翻文件。
static void LogLine(const WCHAR* pszFmt, ...)
{
	WCHAR szBuf[1024];
	va_list ap;
	va_start(ap, pszFmt);
	_vsnwprintf_s(szBuf, 1024, _TRUNCATE, pszFmt, ap);
	va_end(ap);

	if (g_pLog != NULL)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		WCHAR szAll[1200];
		swprintf_s(szAll, 1200, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, szBuf);
		// 统一按 UTF-8 写盘，不依赖 CRT 的区域设置，记事本双击就能看
		int cb = WideCharToMultiByte(CP_UTF8, 0, szAll, -1, NULL, 0, NULL, NULL);
		if (cb > 1)
		{
			char* pszUtf8 = (char*)malloc(cb);
			if (pszUtf8 != NULL)
			{
				WideCharToMultiByte(CP_UTF8, 0, szAll, -1, pszUtf8, cb, NULL, NULL);
				fwrite(pszUtf8, 1, cb - 1, g_pLog);
				free(pszUtf8);
			}
		}
		fflush(g_pLog);
	}
	// 控制台默认按 ANSI 收字，这里直接用 WriteConsoleW，中文才不乱码
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != NULL && hOut != INVALID_HANDLE_VALUE && GetConsoleWindow() != NULL)
	{
		DWORD dwW = 0;
		WCHAR szOut[1100];
		swprintf_s(szOut, 1100, L"%s\r\n", szBuf);
		WriteConsoleW(hOut, szOut, (DWORD)wcslen(szOut), &dwW, NULL);
	}
}

// 开日志。bNextToExe=TRUE 放安装程序旁边（现场最好找）；
// 卸载时放 %TEMP% —— 安装目录马上要删光，日志写在里面会让目录清不空。
static void OpenLog(BOOL bNextToExe)
{
	if (g_pLog != NULL)
		return;
	WCHAR szDir[MAX_PATH] = { 0 };
	if (bNextToExe)
	{
		GetSelfDir(szDir, MAX_PATH);
		swprintf_s(g_szLogPath, MAX_PATH, L"%sinstall.log", szDir);
		if (_wfopen_s(&g_pLog, g_szLogPath, L"wb") == 0 && g_pLog != NULL)
		{
			fwrite("\xEF\xBB\xBF", 1, 3, g_pLog);      // UTF-8 BOM
			return;
		}
	}
	if (GetTempPathW(MAX_PATH, szDir) == 0)
		wcsncpy_s(szDir, MAX_PATH, L".\\", _TRUNCATE);
	swprintf_s(g_szLogPath, MAX_PATH, L"%s对口升学练习系统_install.log", szDir);
	if (_wfopen_s(&g_pLog, g_szLogPath, L"wb") != 0)
		g_pLog = NULL;
	else
		fwrite("\xEF\xBB\xBF", 1, 3, g_pLog);
}

static void CloseLog()
{
	if (g_pLog != NULL)
	{
		fclose(g_pLog);
		g_pLog = NULL;
	}
}

// 命令行里有 /Xxx 或 -Xxx 吗。
// 只认"参数名后面紧跟结束/空格/引号/等号"，免得 /S 误命中 /Setup 之类。
static BOOL HasSwitch(const WCHAR* pszCmd, const WCHAR* pszName)
{
	if (pszCmd == NULL || pszName == NULL || pszName[0] == L'\0')
		return FALSE;
	size_t n = wcslen(pszName);
	for (const WCHAR* p = pszCmd; *p != L'\0'; p++)
	{
		if (*p != L'/' && *p != L'-')
			continue;
		if (_wcsnicmp(p + 1, pszName, n) != 0)
			continue;
		WCHAR c = p[1 + n];
		if (c == L'\0' || c == L' ' || c == L'\t' || c == L'"' || c == L'=')
			return TRUE;
	}
	return FALSE;
}

// 取 /D=目录（写了多个就取最后一个）。允许带引号，也允许路径里有空格。
static BOOL GetDirSwitch(const WCHAR* pszCmd, WCHAR* pszOut, int cch)
{
	pszOut[0] = L'\0';
	if (pszCmd == NULL)
		return FALSE;
	const WCHAR* pFound = NULL;
	for (const WCHAR* p = pszCmd; *p != L'\0'; p++)
	{
		if ((*p == L'/' || *p == L'-') && (p[1] == L'D' || p[1] == L'd') && p[2] == L'=')
			pFound = p + 3;
	}
	if (pFound == NULL)
		return FALSE;

	WCHAR szTmp[MAX_PATH * 2] = { 0 };
	wcsncpy_s(szTmp, MAX_PATH * 2, pFound, _TRUNCATE);
	size_t n = wcslen(szTmp);
	if (n >= 2 && szTmp[0] == L'"')
	{
		size_t k = 1;
		while (k < n && szTmp[k] != L'"')
			k++;
		szTmp[k] = L'\0';
		memmove(szTmp, szTmp + 1, k * sizeof(WCHAR));
	}
	else
	{
		// 不带引号：只取一个"词"，遇到空白就停。
		// 这样 /S /D=D:\对口升学练习系统 /NORUN 不会把后面的开关一起吞进路径；
		// 路径里真有空格，请写成 /D="D:\我的 目录"。
		size_t k = 0;
		while (szTmp[k] != L'\0' && szTmp[k] != L' ' && szTmp[k] != L'\t')
			k++;
		szTmp[k] = L'\0';
	}
	if (szTmp[0] == L'\0')
		return FALSE;
	wcsncpy_s(pszOut, cch, szTmp, _TRUNCATE);
	return TRUE;
}

// 目录里有多少个条目（含隐藏/系统，"." ".." 不算）
static int CountDirEntries(const WCHAR* pszDir)
{
	WCHAR szPat[MAX_PATH];
	swprintf_s(szPat, MAX_PATH, L"%s\\*", pszDir);
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(szPat, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	int n = 0;
	do
	{
		if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
			n++;
	} while (FindNextFileW(hFind, &fd));
	FindClose(hFind);
	return n;
}

// 安装位置安全校验。
// 为什么要有这一条：卸载是"删掉本程序的文件、目录空了才删目录"，
// 但把程序直接散在盘根（C:\）或 Windows 目录里本身就是隐患（重名、权限、清理难），
// 所以这几类位置一律当场拦住，并直接告诉用户该装哪儿。
static BOOL IsSafeInstallDir(const WCHAR* pszDir, WCHAR* pszWhy, int cchWhy)
{
	WCHAR szIn[MAX_PATH];
	wcsncpy_s(szIn, MAX_PATH, pszDir, _TRUNCATE);
	size_t n = wcslen(szIn);
	while (n > 3 && szIn[n - 1] == L'\\')
		szIn[--n] = L'\0';

	// 1) 盘根：C:\ 或 C:
	if ((n == 2 && szIn[1] == L':') ||
		(n == 3 && szIn[1] == L':' && szIn[2] == L'\\'))
	{
		swprintf_s(pszWhy, cchWhy,
			L"不能把程序装在盘根目录（%s）。\r\n\r\n"
			L"程序要装在自己的文件夹里，散在盘根上不好找、也不好卸。\r\n"
			L"请改成带文件夹的路径，例如：C:\\对口升学练习系统",
			szIn);
		return FALSE;
	}

	// 2) Windows 系统目录（含子目录）
	{
		WCHAR szWin[MAX_PATH] = { 0 };
		DWORD dw = GetWindowsDirectoryW(szWin, MAX_PATH);
		if (dw > 0 && dw < MAX_PATH)
		{
			size_t nw = wcslen(szWin);
			if (_wcsnicmp(szIn, szWin, nw) == 0 && (szIn[nw] == L'\0' || szIn[nw] == L'\\'))
			{
				swprintf_s(pszWhy, cchWhy,
					L"不能装在 Windows 系统目录里（%s）。\r\n\r\n"
					L"请改成普通文件夹，例如：C:\\对口升学练习系统",
					szIn);
				return FALSE;
			}
		}
	}

	// 3) 和系统/个人关键目录完全重合
	{
		static const int nFolders[] = {
			CSIDL_WINDOWS, CSIDL_SYSTEM, CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86,
			CSIDL_PROGRAM_FILES_COMMON, CSIDL_DESKTOPDIRECTORY, CSIDL_PERSONAL,
			CSIDL_APPDATA, CSIDL_LOCAL_APPDATA, CSIDL_COMMON_APPDATA,
			CSIDL_STARTMENU, CSIDL_PROGRAMS, CSIDL_PROFILE
		};
		for (int i = 0; i < (int)(sizeof(nFolders) / sizeof(nFolders[0])); i++)
		{
			WCHAR szSys[MAX_PATH] = { 0 };
			if (GetSpecialDir(nFolders[i], szSys) && szSys[0] != L'\0'
				&& _wcsicmp(szSys, szIn) == 0)
			{
				swprintf_s(pszWhy, cchWhy,
					L"这是系统的特殊文件夹（%s），不能把程序装进去。\r\n\r\n"
					L"请改成自己新建的文件夹，例如：C:\\对口升学练习系统",
					szIn);
				return FALSE;
			}
		}
	}
	return TRUE;
}

// ================= 安装目录解析（盘符回退 + 同名加序号，界面版/静默版共用）=================

// 目录比较用的归一化：统一正反斜杠、去掉结尾的分隔符（保留盘根那种）。
// 注册表里的 InstallLocation 常带结尾反斜杠，甚至写成正斜杠；不归一化就会把
// "同一个目录"误判成"另一个目录"——那会导致就地升级时反而去"清理旧位置"、把成绩删了。
static void NormalizeDirForCompare(const WCHAR* pszIn, WCHAR* pszOut, int cchOut)
{
	if (pszOut == NULL || cchOut <= 0)
		return;
	pszOut[0] = L'\0';
	if (pszIn == NULL)
		return;
	wcsncpy_s(pszOut, cchOut, pszIn, _TRUNCATE);
	size_t n = wcslen(pszOut);
	for (size_t i = 0; i < n; i++)
	{
		if (pszOut[i] == L'/')
			pszOut[i] = L'\\';
	}
	while (n > 3 && pszOut[n - 1] == L'\\')
		pszOut[--n] = L'\0';
}

// 两个目录是不是"同一个地方"（忽略大小写、正反斜杠、结尾反斜杠）
static BOOL SameDir(const WCHAR* pszA, const WCHAR* pszB)
{
	if (pszA == NULL || pszB == NULL)
		return FALSE;
	WCHAR szA[MAX_PATH] = { 0 };
	WCHAR szB[MAX_PATH] = { 0 };
	NormalizeDirForCompare(pszA, szA, MAX_PATH);
	NormalizeDirForCompare(pszB, szB, MAX_PATH);
	return (_wcsicmp(szA, szB) == 0);
}

// 取目录所在的盘根，如 "C:\对口升学练习系统" -> "C:\"。不是 "X:\..." 形式时返回 FALSE。
static BOOL GetDirDriveRoot(const WCHAR* pszDir, WCHAR* pszRoot, int cchRoot)
{
	if (pszDir == NULL || pszDir[0] == L'\0' || pszRoot == NULL || cchRoot < 4)
		return FALSE;
	WCHAR c = pszDir[0];
	if (((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')) && pszDir[1] == L':')
	{
		swprintf_s(pszRoot, cchRoot, L"%c:\\", c);
		return TRUE;
	}
	return FALSE;
}

// 一个盘是不是"存在且可写"。bRequireFixed=TRUE 时只认固定盘（回退选盘用：
// 光驱、网络盘、U 盘一律跳过）。只看 GetDriveTypeW 还不够 —— 网络盘在线返回
// DRIVE_REMOTE、U 盘也能写，所以这里再在盘根上实打实建一个临时目录再删掉，
// 确认"真的能写"（用户要求的"实打实校验"）。
static BOOL IsDriveUsable(const WCHAR* pszRoot, BOOL bRequireFixed)
{
	if (pszRoot == NULL || pszRoot[0] == L'\0')
		return FALSE;
	UINT t = GetDriveTypeW(pszRoot);
	if (t == DRIVE_NO_ROOT_DIR || t == DRIVE_UNKNOWN)
		return FALSE;
	if (bRequireFixed && t != DRIVE_FIXED)
		return FALSE;
	WCHAR szProbe[MAX_PATH];
	swprintf_s(szProbe, MAX_PATH, L"%s__hebei_write_probe_%u__", pszRoot, GetCurrentProcessId());
	if (CreateDirectoryW(szProbe, NULL))
	{
		RemoveDirectoryW(szProbe);
		return TRUE;
	}
	return (GetLastError() == ERROR_ALREADY_EXISTS);
}

// 旧安装位置值不值得"换位置后清理"：必须是有效目录、和最终目录确实不同，
// 而且里面确实有本程序的痕迹（免得注册表指向别处时误删人家的东西）。
// ★ 数据安全红线：宁可少清一次，也绝不多删一个。
static BOOL ShouldCleanupOldDir(const WCHAR* pszOld, const WCHAR* pszFinal)
{
	if (pszOld == NULL || pszOld[0] == L'\0')
		return FALSE;
	if (GetFileAttributesW(pszOld) == INVALID_FILE_ATTRIBUTES)
		return FALSE;
	// ★ 归一化后再比：注册表里的旧位置可能带结尾反斜杠/正斜杠，原样比较会把
	//   同一个目录当成"另一个目录"，于是"就地升级"反而去清理旧位置、把成绩删了。
	if (pszFinal != NULL && pszFinal[0] != L'\0' && SameDir(pszOld, pszFinal))
		return FALSE;
	WCHAR szProbe[MAX_PATH];
	swprintf_s(szProbe, MAX_PATH, L"%s\\ExamDlgProj.exe", pszOld);
	if (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES)
		return TRUE;
	swprintf_s(szProbe, MAX_PATH, L"%s\\卸载.exe", pszOld);
	if (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES)
		return TRUE;
	swprintf_s(szProbe, MAX_PATH, L"%s\\questions.dat", pszOld);
	if (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES)
		return TRUE;
	return FALSE;
}

// 把"原定目录"解析成"最终目录"，界面版和静默版走同一套：
//   1) 盘符回退：原定目录所在盘不可用 -> 从 C: 到 Z: 找第一个能用的固定盘，
//      目标改成 <盘符>:\对口升学练习系统（只认固定盘，U盘/光驱/网络盘跳过）
//   2) 同名加序号：目标已存在且不是"本程序上次装的地方" -> 依次试 <目录>1..99，
//      取第一个不存在的（例如 C:\对口升学练习系统 被别人占了 -> C:\对口升学练习系统1）
// 返回 TRUE 正常；FALSE 表示原定目录为空（调用方报错退出）。
// pbFallback / pbNumbered（可空）：告诉调用方发生了哪种调整，便于回显给用户。
static BOOL ResolveInstallDir(const WCHAR* pszWant, WCHAR* pszOut, int cchOut,
	BOOL* pbFallback, BOOL* pbNumbered)
{
	if (pbFallback != NULL) *pbFallback = FALSE;
	if (pbNumbered != NULL) *pbNumbered = FALSE;
	if (pszOut == NULL || cchOut <= 0)
		return FALSE;

	WCHAR szCur[MAX_PATH];
	wcsncpy_s(szCur, MAX_PATH, (pszWant != NULL) ? pszWant : L"", _TRUNCATE);
	size_t n = wcslen(szCur);
	while (n > 3 && szCur[n - 1] == L'\\')
		szCur[--n] = L'\0';
	if (szCur[0] == L'\0')
		return FALSE;

	// ---- 1) 盘符回退 ----
	WCHAR szRoot[8] = { 0 };
	BOOL bDriveOK = TRUE;
	if (GetDirDriveRoot(szCur, szRoot, 8))
		bDriveOK = IsDriveUsable(szRoot, FALSE);

	if (!bDriveOK)
	{
		WCHAR szNew[MAX_PATH] = { 0 };
		for (WCHAR c = L'C'; c <= L'Z'; c++)
		{
			WCHAR szR[8];
			swprintf_s(szR, 8, L"%c:\\", c);
			if (IsDriveUsable(szR, TRUE))
			{
				swprintf_s(szNew, MAX_PATH, L"%c:\\%s", c, APP_SHORTNAME);
				break;
			}
		}
		if (szNew[0] != L'\0')
		{
			LogLine(L"原定目录 %s 所在盘不可用 -> 回退到 %s", szCur, szNew);
			wcsncpy_s(szCur, MAX_PATH, szNew, _TRUNCATE);
			if (pbFallback != NULL) *pbFallback = TRUE;
		}
		else
		{
			LogLine(L"原定目录 %s 所在盘不可用，且 C:~Z: 都没有可用的固定盘，仍按原目录处理。", szCur);
		}
	}

	// ---- 2) 同名加序号 ----
	if (GetFileAttributesW(szCur) != INVALID_FILE_ATTRIBUTES)
	{
		WCHAR szOld[MAX_PATH] = { 0 };
		GetInstalledDir(szOld, MAX_PATH);

		// 用 SameDir 归一化比较：旧位置可能带结尾反斜杠/正斜杠（见 NormalizeDirForCompare）
		if (szOld[0] != L'\0' && SameDir(szOld, szCur))
		{
			LogLine(L"目标目录已存在，且就是本程序上次的安装位置 -> 就地升级，不改名：%s", szCur);
		}
		else
		{
			WCHAR szTry[MAX_PATH] = { 0 };
			BOOL bFound = FALSE;
			for (int i = 1; i <= 99; i++)
			{
				swprintf_s(szTry, MAX_PATH, L"%s%d", szCur, i);
				if (GetFileAttributesW(szTry) == INVALID_FILE_ATTRIBUTES)
				{
					bFound = TRUE;
					break;
				}
			}
			if (bFound)
			{
				LogLine(L"目标目录已被占用（且不是本程序上次的安装位置）-> 改用：%s", szTry);
				wcsncpy_s(szCur, MAX_PATH, szTry, _TRUNCATE);
				if (pbNumbered != NULL) *pbNumbered = TRUE;
			}
			else
			{
				LogLine(L"目标目录已被占用，且 目录1~目录99 都被占用，仍按原目录处理：%s", szCur);
			}
		}
	}

	wcsncpy_s(pszOut, cchOut, szCur, _TRUNCATE);
	return TRUE;
}

// 把最终安装目录写进一个 ANSI（本机代码页）小文件，专供 静默安装.bat 读取：
// bat/cmd 是 GBK，而 install.log 是 UTF-8，从 log 里取中文路径会乱码、if exist 也会对不上。
// 优先放自己旁边（和 install.log 一致），写不进去就放 %TEMP%。
static void WriteLastDirFile(const WCHAR* pszDir)
{
	if (pszDir == NULL || pszDir[0] == L'\0')
		return;
	WCHAR szSelf[MAX_PATH] = { 0 };
	WCHAR szPath[MAX_PATH] = { 0 };
	GetSelfDir(szSelf, MAX_PATH);
	swprintf_s(szPath, MAX_PATH, L"%slast_dir.txt", szSelf);

	FILE* fp = NULL;
	_wfopen_s(&fp, szPath, L"wb");
	if (fp == NULL)
	{
		WCHAR szTmp[MAX_PATH] = { 0 };
		DWORD dw = GetTempPathW(MAX_PATH, szTmp);
		if (dw == 0 || dw >= MAX_PATH)
			return;
		swprintf_s(szPath, MAX_PATH, L"%s对口升学练习系统_last_dir.txt", szTmp);
		_wfopen_s(&fp, szPath, L"wb");
		if (fp == NULL)
			return;
	}
	int cb = WideCharToMultiByte(CP_ACP, 0, pszDir, -1, NULL, 0, NULL, NULL);
	if (cb > 1)
	{
		char* p = (char*)malloc(cb);
		if (p != NULL)
		{
			WideCharToMultiByte(CP_ACP, 0, pszDir, -1, p, cb, NULL, NULL);
			fwrite(p, 1, cb - 1, fp);
			free(p);
		}
	}
	fflush(fp);
	fclose(fp);
}

// ================= 题库信息（让界面上的题数永远跟实际题库一致）=================
// 题库格式：8 字节魔数 "EXBANK1" + 逐字节异或的 UTF-8 明文（与主程序 PublicDef.h 一致）。
// 以前"159 道题"是写死在 .rc 里的，加了几道题忘了改文案就会对不上。
static BYTE* LoadBankPlain(DWORD* pdwLen)
{
	*pdwLen = 0;
	HRSRC hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(IDR_PAYLOAD_BANK), RT_RCDATA);
	if (hRes == NULL)
		return NULL;
	DWORD dwSize = SizeofResource(g_hInst, hRes);
	HGLOBAL hGlob = LoadResource(g_hInst, hRes);
	if (hGlob == NULL || dwSize <= 8)
		return NULL;
	const BYTE* p = (const BYTE*)LockResource(hGlob);
	if (p == NULL || memcmp(p, "EXBANK1", 7) != 0)
		return NULL;                    // 格式变了就老实返回失败，界面用兜底文案

	static const BYTE s_key[16] = {
		0x5A, 0x37, 0xC1, 0x9E, 0x24, 0x8B, 0x6D, 0xF0,
		0x13, 0xAA, 0x4C, 0x77, 0xE5, 0x08, 0x9B, 0x31
	};
	DWORD n = dwSize - 8;
	BYTE* pOut = (BYTE*)malloc(n + 1);
	if (pOut == NULL)
		return NULL;
	for (DWORD i = 0; i < n; i++)
		pOut[i] = p[8 + i] ^ s_key[i % 16];
	pOut[n] = 0;
	*pdwLen = n;
	return pOut;
}

// 题数：行首形如 "[编号]" 的是一道题的开头；标题行里带"真题"的算真题。
static BOOL GetBankStats(int* pnTotal, int* pnReal)
{
	DWORD n = 0;
	BYTE* p = LoadBankPlain(&n);
	if (p == NULL)
		return FALSE;

	const char* psz = (const char*)p;
	const char* pEnd = psz + n;
	int nTotal = 0, nReal = 0;
	static const char s_title[] = "\xE6\xA0\x87\xE9\xA2\x98";     // "标题"
	static const char s_real[] = "\xE7\x9C\x9F\xE9\xA2\x98";      // "真题"
	const char* pLine = psz;
	while (pLine < pEnd)
	{
		const char* pNL = (const char*)memchr(pLine, '\n', (size_t)(pEnd - pLine));
		const char* pNext = (pNL != NULL) ? (pNL + 1) : pEnd;
		size_t nLine = (size_t)(pNext - pLine);
		while (nLine > 0 && (pLine[nLine - 1] == '\r' || pLine[nLine - 1] == ' '))
			nLine--;

		if (nLine > 2 && pLine[0] == '[' && pLine[1] >= '0' && pLine[1] <= '9')
			nTotal++;
		if (nLine > 7 && memcmp(pLine, s_title, 6) == 0 && pLine[6] == '=')
		{
			for (size_t i = 7; i + 6 <= nLine; i++)
			{
				if (memcmp(pLine + i, s_real, 6) == 0)
				{
					nReal++;
					break;
				}
			}
		}
		pLine = pNext;
	}
	free(p);
	*pnTotal = nTotal;
	*pnReal = nReal;
	return (nTotal > 0);
}

// 本程序会装进去的文件名。
// 这是"兜底清单"：万一注册表里的清单丢了（用户用清理软件扫过注册表），
// 或者程序是旧版本装的（旧版根本没写清单），照样能按这份默认清单删对。
static const WCHAR* k_pInstalledNames[] = {
	L"ExamDlgProj.exe",
	L"ExamDlgProj.exe.manifest",
	L"questions.dat",
	L"support.png",
	L"说明.txt",
	L"卸载.exe",
};
#define INSTALLED_NAME_COUNT (int)(sizeof(k_pInstalledNames) / sizeof(k_pInstalledNames[0]))
#define MAX_UNINST_FILES     64

// ================= 安装 =================

static BOOL DoInstall(const WCHAR* pszDir, BOOL bDesktop, BOOL bStartMenu, BOOL bRun, WCHAR* pszMsg, int cchMsg)
{
	if (!MakeDirRecursive(pszDir))
	{
		swprintf_s(pszMsg, cchMsg, L"无法创建目录：\r\n%s\r\n\r\n请换一个目录（例如 C:\\对口升学练习系统），\r\n或者右键安装程序选\"以管理员身份运行\"。", pszDir);
		return FALSE;
	}

	WCHAR szExe[MAX_PATH], szMan[MAX_PATH], szDat[MAX_PATH], szUninst[MAX_PATH];
	swprintf_s(szExe, MAX_PATH, L"%s\\ExamDlgProj.exe", pszDir);
	swprintf_s(szMan, MAX_PATH, L"%s\\ExamDlgProj.exe.manifest", pszDir);
	swprintf_s(szDat, MAX_PATH, L"%s\\questions.dat", pszDir);
	swprintf_s(szUninst, MAX_PATH, L"%s\\卸载.exe", pszDir);

	// 按系统位数选主程序：64 位系统装 64 位版，32 位系统（老机房）装 32 位版
	int nExeRes = Is64BitOS() ? IDR_PAYLOAD_EXE : IDR_PAYLOAD_EXE32;
	BOOL b64 = Is64BitOS();
	if (!WriteResourceToFile(nExeRes, szExe) ||
		!WriteResourceToFile(IDR_PAYLOAD_MANIFEST, szMan) ||
		!WriteResourceToFile(IDR_PAYLOAD_BANK, szDat))
	{
		swprintf_s(pszMsg, cchMsg,
			L"写入文件失败。\r\n\r\n失败的文件：\r\n%s\r\n错误码：%lu\r\n\r\n"
			L"常见原因：这个目录没有写权限、磁盘空间不足、或该文件正被其它程序占用。\r\n"
			L"请换一个目录（例如 C:\\对口升学练习系统），\r\n"
			L"或者右键安装程序选\"以管理员身份运行\"。",
			(g_szLastFailFile[0] != L'\0') ? g_szLastFailFile : L"(未知)",
			g_dwLastFailErr);
		return FALSE;
	}

	// "给作者回血"的图片：安装包旁边放了就用放的那张（方便临时更换），
	// 没放就用安装程序里内置的那张，保证装上就有图
	WCHAR szSelfDir[MAX_PATH], szImgSrc[MAX_PATH], szImgDst[MAX_PATH];
	GetSelfDir(szSelfDir, MAX_PATH);
	swprintf_s(szImgSrc, MAX_PATH, L"%ssupport.png", szSelfDir);
	swprintf_s(szImgDst, MAX_PATH, L"%s\\support.png", pszDir);
	if (GetFileAttributesW(szImgSrc) != INVALID_FILE_ATTRIBUTES)
	{
		CopyFileW(szImgSrc, szImgDst, FALSE);
	}
	else
	{
		WriteResourceToFile(IDR_PAYLOAD_SUPPORT, szImgDst);
	}

	// 说明文字：同样优先用安装包旁边放的 说明.txt，改文字不用重新编译程序
	WCHAR szAboutSrc[MAX_PATH], szAboutDst[MAX_PATH];
	swprintf_s(szAboutSrc, MAX_PATH, L"%s说明.txt", szSelfDir);
	swprintf_s(szAboutDst, MAX_PATH, L"%s\\说明.txt", pszDir);
	if (GetFileAttributesW(szAboutSrc) != INVALID_FILE_ATTRIBUTES)
	{
		CopyFileW(szAboutSrc, szAboutDst, FALSE);
	}
	else
	{
		WriteResourceToFile(IDR_PAYLOAD_ABOUT, szAboutDst);
	}

	// 快捷方式
	BOOL bDesktopOK = TRUE, bStartMenuOK = TRUE;
	if (bDesktop)
	{
		WCHAR szDir2[MAX_PATH], szLnk[MAX_PATH];
		if (GetSpecialDir(CSIDL_DESKTOPDIRECTORY, szDir2))
		{
			swprintf_s(szLnk, MAX_PATH, L"%s\\%s.lnk", szDir2, APP_SHORTNAME);
			bDesktopOK = CreateShortcut(szLnk, szExe, pszDir);
		}
		else
			bDesktopOK = FALSE;
	}
	if (bStartMenu)
	{
		WCHAR szDir2[MAX_PATH], szLnk[MAX_PATH];
		if (GetSpecialDir(CSIDL_PROGRAMS, szDir2))
		{
			swprintf_s(szLnk, MAX_PATH, L"%s\\%s.lnk", szDir2, APP_SHORTNAME);
			bStartMenuOK = CreateShortcut(szLnk, szExe, pszDir);
		}
		else
			bStartMenuOK = FALSE;
	}

	// 【修复】卸载程序改成"内嵌的小体积专用卸载程序"（约 220 KB），
	// 不再把 5 MB 的安装程序整份复制进去——以前安装目录里 64% 的体积都是它。
	BOOL bUninstOK = WriteResourceToFile(IDR_PAYLOAD_UNINST, szUninst);
	if (!bUninstOK)
	{
		// 兜底：万一这个构建没打包卸载程序资源，退回"复制自身"，功能一样不少
		WCHAR szSelfPath[MAX_PATH];
		GetModuleFileNameW(NULL, szSelfPath, MAX_PATH);
		bUninstOK = CopyFileW(szSelfPath, szUninst, FALSE);
	}
	LogLine(bUninstOK ? L"已写入卸载程序：%s" : L"卸载程序写入失败：%s", szUninst);

	// 估算占用空间（给控制面板的"大小"列用）
	DWORD dwTotalKB = 0;
	{
		const WCHAR* pszFiles[6] = { szExe, szMan, szDat, szImgDst, szAboutDst, szUninst };
		for (int i = 0; i < 6; i++)
		{
			HANDLE hF = CreateFileW(pszFiles[i], GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hF != INVALID_HANDLE_VALUE)
			{
				LARGE_INTEGER li;
				if (GetFileSizeEx(hF, &li))
					dwTotalKB += (DWORD)(li.QuadPart / 1024);
				CloseHandle(hF);
			}
		}
	}

	BOOL bRegOK = FALSE;
	HKEY hKey = NULL;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		WCHAR szUninstCmd[MAX_PATH + 32];
		swprintf_s(szUninstCmd, MAX_PATH + 32, L"\"%s\" /uninstall", szUninst);
		SYSTEMTIME st;
		GetLocalTime(&st);
		WCHAR szInstallDate[16];
		swprintf_s(szInstallDate, 16, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
		DWORD dwOne = 1;
		bRegOK = TRUE;
		RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)APP_TITLE, (DWORD)((wcslen(APP_TITLE) + 1) * sizeof(WCHAR)));
		RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)APP_VERSION_STR_W, (DWORD)((wcslen(APP_VERSION_STR_W) + 1) * sizeof(WCHAR)));
		// 发布者写作者本人，以前这里又填了一遍产品名，控制面板里看着像重复
		RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)APP_AUTHOR_STR_W, (DWORD)((wcslen(APP_AUTHOR_STR_W) + 1) * sizeof(WCHAR)));
		RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)pszDir, (DWORD)((wcslen(pszDir) + 1) * sizeof(WCHAR)));
		RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)szExe, (DWORD)((wcslen(szExe) + 1) * sizeof(WCHAR)));
		RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)szUninstCmd, (DWORD)((wcslen(szUninstCmd) + 1) * sizeof(WCHAR)));
		// 【修复】记下"本程序装了哪些文件"：卸载就照这份清单删，用户自己放进目录的东西不碰。
		// （以前卸载是清空整个目录，用户放在里面的文件会被一起删掉——这条是数据安全的关键。）
		{
			const WCHAR* pszList[INSTALLED_NAME_COUNT + 1] = {
				L"ExamDlgProj.exe", L"ExamDlgProj.exe.manifest", L"questions.dat",
				L"support.png", L"说明.txt", L"卸载.exe", NULL
			};
			int cb = 0;
			for (int i = 0; pszList[i] != NULL; i++)
				cb += (int)((wcslen(pszList[i]) + 1) * sizeof(WCHAR));
			cb += sizeof(WCHAR);                 // REG_MULTI_SZ 结尾要额外一个空字符
			BYTE* pBuf = (BYTE*)malloc(cb);
			if (pBuf != NULL)
			{
				ZeroMemory(pBuf, cb);
				BYTE* pCur = pBuf;
				for (int i = 0; pszList[i] != NULL; i++)
				{
					size_t cbOne = (wcslen(pszList[i]) + 1) * sizeof(WCHAR);
					memcpy(pCur, pszList[i], cbOne);
					pCur += cbOne;
				}
				RegSetValueExW(hKey, L"InstalledFiles", 0, REG_MULTI_SZ, pBuf, (DWORD)cb);
				free(pBuf);
			}
		}

		// 下面几项让控制面板里显示得更完整（大小、安装日期，并隐藏"修改/修复"按钮）
		RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (const BYTE*)&dwTotalKB, sizeof(dwTotalKB));
		RegSetValueExW(hKey, L"InstallDate", 0, REG_SZ, (const BYTE*)szInstallDate, (DWORD)((wcslen(szInstallDate) + 1) * sizeof(WCHAR)));
		RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&dwOne, sizeof(dwOne));
		RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&dwOne, sizeof(dwOne));
		RegCloseKey(hKey);
	}

	if (bRun)
		ShellExecuteW(NULL, L"open", szExe, NULL, pszDir, SW_SHOWNORMAL);

	// 把没做成的步骤如实说明（正常环境一般不会出现）
	WCHAR szNote[600] = L"";
	if (!bDesktopOK)
		wcscat_s(szNote, MAX_PATH * 2, L"\r\n· 桌面快捷方式没建成（可能没有权限），可以直接去安装目录运行 ExamDlgProj.exe。");
	if (!bStartMenuOK)
		wcscat_s(szNote, MAX_PATH * 2, L"\r\n· 开始菜单快捷方式没建成（可能没有权限）。");
	if (!bUninstOK)
				wcscat_s(szNote, MAX_PATH * 2, L"\r\n· 卸载程序没写成功，卸载时直接删除安装目录即可。");
	if (!bRegOK)
		wcscat_s(szNote, MAX_PATH * 2, L"\r\n· 卸载信息没写进注册表（不影响使用，控制面板里可能看不到卸载项）。");

	swprintf_s(pszMsg, cchMsg,
		L"安装完成！\r\n\r\n安装位置：%s\r\n版本：%s（%s）\r\n\r\n"
		L"桌面/开始菜单里的【%s】可以直接打开；\r\n要卸载就运行该目录下的「卸载.exe」。\r\n\r\n"
		L"作者：%s　　反馈：%s%s",
		pszDir, APP_VERSION_STR_W, b64 ? L"64 位" : L"32 位",
		APP_SHORTNAME, APP_AUTHOR_STR_W, APP_FEEDBACK_STR_W, szNote);
	return TRUE;
}

// ================= 卸载 =================

// 读注册表里记的"本程序装过哪些文件"（REG_MULTI_SZ）。
// 取不到就返回 0，调用方用 k_pInstalledNames 兜底。
static int LoadInstalledManifest(WCHAR pszNames[][MAX_PATH], int nMax)
{
	int n = 0;
	HKEY hKey = NULL;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return 0;
	DWORD dwType = 0, cb = 0;
	if (RegQueryValueExW(hKey, L"InstalledFiles", NULL, &dwType, NULL, &cb) == ERROR_SUCCESS
		&& (dwType == REG_MULTI_SZ || dwType == REG_SZ) && cb > 0)
	{
		BYTE* pBuf = (BYTE*)malloc(cb + sizeof(WCHAR));
		if (pBuf != NULL)
		{
			ZeroMemory(pBuf, cb + sizeof(WCHAR));
			if (RegQueryValueExW(hKey, L"InstalledFiles", NULL, &dwType, pBuf, &cb) == ERROR_SUCCESS)
			{
				const WCHAR* p = (const WCHAR*)pBuf;
				const WCHAR* pEnd = (const WCHAR*)(pBuf + cb);
				while (p < pEnd && *p != L'\0' && n < nMax)
				{
					// 只认纯文件名：清单里万一被人塞进 ..\\..\\ 这种路径，也删不到目录外面去
					if (wcschr(p, L'\\') == NULL && wcschr(p, L'/') == NULL)
					{
						wcsncpy_s(pszNames[n], MAX_PATH, p, _TRUNCATE);
						n++;
					}
					p += wcslen(p) + 1;
				}
			}
			free(pBuf);
		}
	}
	RegCloseKey(hKey);
	return n;
}

// 删单个文件，被占用时小睡一下再试几次。
// （父进程刚发起卸载就退出那几十毫秒里"卸载.exe"还锁着，不重试就会剩一个删不掉的文件）
static BOOL DeleteFileRetry(const WCHAR* pszPath)
{
	for (int i = 0; i < 6; i++)
	{
		if (DeleteFileW(pszPath))
			return TRUE;
		if (GetFileAttributesW(pszPath) == INVALID_FILE_ATTRIBUTES)
			return TRUE;              // 已经不在了，一样算成功
		Sleep(150);
	}
	return FALSE;
}

// ============ 程序"运行时自己产生"的过程文件（卸载要一并清掉）============
// draft\                 草稿目录（每道题一个 <题号>.txt）
// exam\                  【VS调试运行】生成的工程目录（exam.sln / exam.csproj / obj / bin …）
// answer.txt / answer.cs 保存答案时写在程序目录里的临时文件
// 这些不是安装时装进去的，却是本程序运行时生成的。以前卸载不管它们，
// 用户卸完发现目录里还留着 draft\ 、exam\ ，会以为没卸干净。
// ★ 只认这几个确定的名字，目录里其它任何东西一律不碰（和"绝不删用户文件"一个原则）。
static const WCHAR* const k_pOwnedArtifacts[] = {
	L"draft",
	L"exam",
	L"answer.txt",
	L"answer.cs",
};
#define OWNED_ARTIFACT_COUNT (int)(sizeof(k_pOwnedArtifacts) / sizeof(k_pOwnedArtifacts[0]))

// 往"删不掉的清单"里追加一个名字（重复的不再加；放不下就不写，绝不越界）
static void AppendLeftName(WCHAR* pszLeftList, int cchLeft, const WCHAR* pszName)
{
	if (pszLeftList == NULL || cchLeft <= 0 || pszName == NULL)
		return;
	if (wcsstr(pszLeftList, pszName) != NULL)
		return;
	size_t nCur = wcslen(pszLeftList);
	size_t nAdd = wcslen(pszName) + ((nCur > 0) ? 1 : 0);
	if (nCur + nAdd + 1 >= (size_t)cchLeft)
		return;
	if (nCur > 0)
		wcscat_s(pszLeftList, cchLeft, L"、");
	wcscat_s(pszLeftList, cchLeft, pszName);
}

// ============ 学生做题留下的成绩文件（不是安装时装进去的）============
//  score.dat     成绩数据本体：一行一题，同一题只留最近一次，按题号排序
//  成绩表.xlsx    由 score.dat 生成的漂亮表格，Excel/WPS 直接能打开
//  score.txt     老版本的成绩文件；新版本第一次运行时会自动并进 score.dat 并删掉它
// ★ 删除 / 备份 / 换目录搬运都走这一份清单，以后再加文件只改这里一处。
static const WCHAR* const k_pScoreFiles[] = {
	L"score.dat",
	L"成绩表.xlsx",
	L"score.txt",
};
#define SCORE_FILE_COUNT (int)(sizeof(k_pScoreFiles) / sizeof(k_pScoreFiles[0]))

// 安装目录里有没有成绩文件（卸载时用来决定要不要问"留不留成绩"）
static BOOL HasAnyScoreFile(const WCHAR* pszDir)
{
	for (int i = 0; i < SCORE_FILE_COUNT; i++)
	{
		WCHAR szFull[MAX_PATH];
		swprintf_s(szFull, MAX_PATH, L"%s\\%s", pszDir, k_pScoreFiles[i]);
		if (GetFileAttributesW(szFull) != INVALID_FILE_ATTRIBUTES)
			return TRUE;
	}
	return FALSE;
}

// 把成绩文件从旧目录搬到新目录（新目录已有同名文件就不覆盖，不拿旧的盖新的）
static void MoveScoreFiles(const WCHAR* pszOldDir, const WCHAR* pszNewDir)
{
	for (int i = 0; i < SCORE_FILE_COUNT; i++)
	{
		WCHAR szOld[MAX_PATH], szNew[MAX_PATH];
		swprintf_s(szOld, MAX_PATH, L"%s\\%s", pszOldDir, k_pScoreFiles[i]);
		swprintf_s(szNew, MAX_PATH, L"%s\\%s", pszNewDir, k_pScoreFiles[i]);
		if (GetFileAttributesW(szOld) == INVALID_FILE_ATTRIBUTES)
			continue;
		if (GetFileAttributesW(szNew) != INVALID_FILE_ATTRIBUTES)
			continue;
		CopyFileW(szOld, szNew, FALSE);
	}
}

// 递归删一个目录（先清空、再删目录本身）。删不掉的登记"下次开机自动删除"。
// 返回删不掉的文件个数；目录里还有东西时 RemoveDirectoryW 自然失败，目录会保留。
static int DeleteDirRecursiveW(const WCHAR* pszDir)
{
	WCHAR szPat[MAX_PATH];
	swprintf_s(szPat, MAX_PATH, L"%s\\*", pszDir);
	int nLeft = 0;
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(szPat, &fd);
	if (h != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			WCHAR szChild[MAX_PATH];
			swprintf_s(szChild, MAX_PATH, L"%s\\%s", pszDir, fd.cFileName);
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				nLeft += DeleteDirRecursiveW(szChild);
			}
			else
			{
				SetFileAttributesW(szChild, FILE_ATTRIBUTE_NORMAL);
				if (!DeleteFileRetry(szChild))
				{
					MoveFileExW(szChild, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
					nLeft++;
				}
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}
	SetFileAttributesW(pszDir, FILE_ATTRIBUTE_NORMAL);
	RemoveDirectoryW(pszDir);
	return nLeft;
}

// 清掉本程序运行时留下的过程文件。
// 除了安装目录里的 draft\ / exam\ / answer.txt / answer.cs，
// 还有 %LOCALAPPDATA%\对口升学练习系统\ 下的判题编译产物（那个目录整个是本程序建的）。
// 删不掉的把（顶层）名字追加进 pszLeftList，让调用方如实汇报，不谎报成功。
static void DeleteOwnedArtifacts(const WCHAR* pszDir, int* pnLeft,
	WCHAR* pszLeftList, int cchLeft)
{
	int nLeft = 0;

	// 1) 安装目录里的过程文件
	for (int i = 0; i < OWNED_ARTIFACT_COUNT; i++)
	{
		WCHAR szFull[MAX_PATH];
		swprintf_s(szFull, MAX_PATH, L"%s\\%s", pszDir, k_pOwnedArtifacts[i]);
		DWORD dwAttr = GetFileAttributesW(szFull);
		if (dwAttr == INVALID_FILE_ATTRIBUTES)
			continue;
		int nThis = 0;
		if ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			nThis = DeleteDirRecursiveW(szFull);
		}
		else
		{
			SetFileAttributesW(szFull, FILE_ATTRIBUTE_NORMAL);
			if (!DeleteFileRetry(szFull))
			{
				MoveFileExW(szFull, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
				nThis = 1;
			}
		}
		if (nThis > 0)
		{
			nLeft += nThis;
			AppendLeftName(pszLeftList, cchLeft, k_pOwnedArtifacts[i]);
		}
	}

	// 2) %LOCALAPPDATA%\对口升学练习系统\ —— 判题要写 .cs 再编 .exe，产物都在这。
	//    只删确定是自己建的 temp\ 子目录；父目录空了才顺手删掉，绝不递归清人家的目录。
	WCHAR szLocal[MAX_PATH] = { 0 };
	if (GetEnvironmentVariableW(L"LOCALAPPDATA", szLocal, MAX_PATH) > 0)
	{
		WCHAR szTempDir[MAX_PATH], szRoot[MAX_PATH];
		swprintf_s(szRoot, MAX_PATH, L"%s\\对口升学练习系统", szLocal);
		swprintf_s(szTempDir, MAX_PATH, L"%s\\temp", szRoot);
		if (GetFileAttributesW(szTempDir) != INVALID_FILE_ATTRIBUTES)
		{
			int nThis = DeleteDirRecursiveW(szTempDir);
			if (nThis > 0)
			{
				nLeft += nThis;
				AppendLeftName(pszLeftList, cchLeft, L"%LOCALAPPDATA%\\对口升学练习系统\\temp");
			}
		}
		RemoveDirectoryW(szRoot);   // 空了才删得掉；里面还有别的东西就保留
	}

	if (pnLeft != NULL)
		*pnLeft += nLeft;
}

// 按清单删"本程序自己的文件"。
// 这是"绝不碰用户文件"的关键：
// 以前卸载是"清空整个安装目录"，用户把程序装进一个已经有资料的文件夹，
// 卸载时那些资料会被一起删掉——不可逆。现在只删清单里的文件名，别的一律不动。
// bWithScore：要不要连成绩文件（score.dat / 成绩表.xlsx / score.txt）一起删
//             （要保留的话调用方已先备份到桌面）
// 返回 TRUE 表示清单里的文件都删干净了；没删净的把名字写进 pszLeftList
static BOOL DeleteInstalledOnly(const WCHAR* pszDir, BOOL bWithScore, int* pnLeft,
	WCHAR* pszLeftList, int cchLeft)
{
	WCHAR szNames[MAX_UNINST_FILES][MAX_PATH];
	int nNames = LoadInstalledManifest(szNames, MAX_UNINST_FILES);
	if (nNames <= 0)
	{
		for (int i = 0; i < INSTALLED_NAME_COUNT && i < MAX_UNINST_FILES; i++)
			wcsncpy_s(szNames[i], MAX_PATH, k_pInstalledNames[i], _TRUNCATE);
		nNames = INSTALLED_NAME_COUNT;
	}

	int nLeft = 0;
	if (pszLeftList != NULL && cchLeft > 0)
		pszLeftList[0] = L'\0';

	for (int i = 0; i < nNames; i++)
	{
		WCHAR szFull[MAX_PATH];
		swprintf_s(szFull, MAX_PATH, L"%s\\%s", pszDir, szNames[i]);
		if (GetFileAttributesW(szFull) == INVALID_FILE_ATTRIBUTES)
			continue;
		SetFileAttributesW(szFull, FILE_ATTRIBUTE_NORMAL);
		if (DeleteFileRetry(szFull))
			continue;
		// 实在删不掉（被占用等）：登记"下次开机自动删除"，并如实记下来
		MoveFileExW(szFull, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
		nLeft++;
		if (pszLeftList != NULL && cchLeft > 0)
		{
			if (pszLeftList[0] != L'\0')
				wcscat_s(pszLeftList, cchLeft, L"、");
			wcscat_s(pszLeftList, cchLeft, szNames[i]);
		}
	}

	// 成绩记录不是安装时装进去的，是学生做题留下的：默认一起删（和以前一致）。
	// 选了"保留"的，调用方已经先备份到桌面；这里照样删掉，避免留下残留。
	if (bWithScore)
	{
		for (int i = 0; i < SCORE_FILE_COUNT; i++)
		{
			WCHAR szScore[MAX_PATH];
			swprintf_s(szScore, MAX_PATH, L"%s\\%s", pszDir, k_pScoreFiles[i]);
			if (GetFileAttributesW(szScore) == INVALID_FILE_ATTRIBUTES)
				continue;
			SetFileAttributesW(szScore, FILE_ATTRIBUTE_NORMAL);
			if (DeleteFileRetry(szScore))
				continue;
			MoveFileExW(szScore, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
			nLeft++;
			if (pszLeftList != NULL && cchLeft > 0)
				AppendLeftName(pszLeftList, cchLeft, k_pScoreFiles[i]);
		}
	}

	if (pnLeft != NULL)
		*pnLeft = nLeft;
	return (nLeft == 0);
}

// 把成绩文件备份到桌面（文件名带日期时间，不会覆盖以前的备份）
// 扩展名跟着原文件走：备份出来的 .xlsx 用 Excel/WPS 直接能看，
// .dat 是数据本体（重装后放回安装目录就能接着记）。
static void BackupScoreToDesktop(const WCHAR* pszDir)
{
	WCHAR szDesk[MAX_PATH] = { 0 };
	if (!GetSpecialDir(CSIDL_DESKTOPDIRECTORY, szDesk))
		return;
	SYSTEMTIME st;
	GetLocalTime(&st);
	for (int i = 0; i < SCORE_FILE_COUNT; i++)
	{
		WCHAR szSrc[MAX_PATH];
		swprintf_s(szSrc, MAX_PATH, L"%s\\%s", pszDir, k_pScoreFiles[i]);
		if (GetFileAttributesW(szSrc) == INVALID_FILE_ATTRIBUTES)
			continue;
		const WCHAR* pszDot = wcsrchr(k_pScoreFiles[i], L'.');
		WCHAR szDst[MAX_PATH];
		swprintf_s(szDst, MAX_PATH, L"%s\\对口升学成绩备份_%04d%02d%02d_%02d%02d%s",
			szDesk, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
			(pszDot != NULL) ? pszDot : L"");
		CopyFileW(szSrc, szDst, FALSE);
	}
}

// 删掉桌面/开始菜单里的快捷方式
static void UnlinkShortcuts()
{
	WCHAR szLnk[MAX_PATH];
	if (GetSpecialDir(CSIDL_DESKTOPDIRECTORY, szLnk))
	{
		wcscat_s(szLnk, MAX_PATH, L"\\" APP_SHORTNAME L".lnk");
		DeleteFileW(szLnk);
	}
	if (GetSpecialDir(CSIDL_PROGRAMS, szLnk))
	{
		wcscat_s(szLnk, MAX_PATH, L"\\" APP_SHORTNAME L".lnk");
		DeleteFileW(szLnk);
	}
}

// 让 cmd 等本进程退出后把临时副本删掉（比等重启更即时）。
// 两个坑：
//   1) 必须等所有对话框都关掉之后再调用。以前排在"卸载完成"提示前面，
//      用户多看几秒（超过 ping 的 2 秒）时文件还被自己锁着，cmd 删不掉也不重试，
//      %TEMP% 里的 uninst_*.exe 就这么一天天积起来了（本机实测积了 24 个 / 128 MB）。
//   2) 只删一次不够。刚退出那一瞬间文件可能还被系统占着，所以退避重试三次。
static void ScheduleSelfDelete()
{
	WCHAR szSelf[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, szSelf, MAX_PATH);
	WCHAR szCmd[MAX_PATH * 5 + 128];
	swprintf_s(szCmd, MAX_PATH * 5 + 128,
		L"/c ping -n 2 127.0.0.1 >nul & del /f /q \"%s\" >nul 2>&1 & "
		L"ping -n 3 127.0.0.1 >nul & del /f /q \"%s\" >nul 2>&1 & "
		L"ping -n 5 127.0.0.1 >nul & del /f /q \"%s\" >nul 2>&1",
		szSelf, szSelf, szSelf);

	WCHAR szComSpec[MAX_PATH] = { 0 };
	if (GetEnvironmentVariableW(L"ComSpec", szComSpec, MAX_PATH) == 0)
		wcsncpy_s(szComSpec, MAX_PATH, L"cmd.exe", _TRUNCATE);

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (CreateProcessW(szComSpec, szCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

static BOOL ExtractDirArg(WCHAR* pszOut, int cch)
{
	if (pszOut == NULL || cch <= 0)
		return FALSE;
	pszOut[0] = L'\0';

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == NULL)
		return FALSE;

	BOOL bOK = FALSE;
	for (int i = 1; i < argc; i++)          // argv[0] 是程序自身路径，跳过
	{
		if (argv[i][0] == L'/' || argv[i][0] == L'-')
			continue;                       // 开关参数，跳过
		wcsncpy_s(pszOut, cch, argv[i], _TRUNCATE);
		bOK = (pszOut[0] != L'\0');
		break;
	}
	LocalFree(argv);
	return bOK;
}

static void DoUninstall(BOOL bFromTemp, const WCHAR* pszDirArg)
{
	WCHAR szDir[MAX_PATH] = { 0 };

	// 目录来源：1) 命令行参数（临时副本用，最可靠）2) 注册表 3) 自己所在目录（仅限直接从安装目录运行时）
	if (pszDirArg != NULL && pszDirArg[0] != L'\0')
		wcsncpy_s(szDir, MAX_PATH, pszDirArg, _TRUNCATE);
	else
		GetInstalledDir(szDir, MAX_PATH);

	if (szDir[0] == L'\0' && !bFromTemp)
	{
		GetModuleFileNameW(NULL, szDir, MAX_PATH);
		WCHAR* p = wcsrchr(szDir, L'\\');
		if (p != NULL)
			*p = L'\0';
	}
	// 临时副本又拿不到目录：绝不乱删（否则会删到临时目录里）
	if (szDir[0] == L'\0')
	{
		if (bFromTemp)
			MessageBoxW(NULL, L"卸载失败：没有找到安装目录，请手动删除程序目录。",
				L"卸载", MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return;
	}

	// 把自己的工作目录挪到临时目录，否则删不掉"正在被当作工作目录"的安装目录
	{
		WCHAR szTempPath[MAX_PATH] = { 0 };
		if (GetTempPathW(MAX_PATH, szTempPath) > 0)
			SetCurrentDirectoryW(szTempPath);
	}

	if (!bFromTemp)
	{
		// 【修复】程序还在运行就先拦住。
		// 以前不检查，结果是：主程序被占用删不掉 → 却弹"卸载完成" → 而且卸载入口已被删掉，
		// 用户再也无法用正规方式清理，只剩手动删目录。
		if (IsMainAppRunning())
		{
			MessageBoxW(NULL,
				L"检测到【" APP_SHORTNAME L"】正在运行。\r\n\r\n"
				L"请先关闭程序再卸载。\r\n"
				L"程序开着时，正在使用的主程序文件删不掉，\r\n"
				L"会留下一个再也卸载不掉的空壳目录。",
				L"请先关闭程序", MB_OK | MB_ICONWARNING | MB_TOPMOST);
			return;
		}

		// 成绩记录让学生自己决定留不留（默认不留，与以前一致）
		BOOL bKeepScore = FALSE;
		if (HasAnyScoreFile(szDir))
		{
			bKeepScore = (MessageBoxW(NULL,
				L"要保留成绩记录吗？\r\n\r\n"
				L"点【是】：先把成绩表备份到桌面，再卸载。\r\n"
				L"　　　　备份出来是 对口升学成绩备份_日期.xlsx（Excel/WPS 直接能看）\r\n"
				L"　　　　和同名的 .dat（成绩数据本体，重装后放回安装目录可接着记）\r\n"
				L"点【否】：连成绩记录一起删除",
				L"卸载", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES);
		}

		// 【修复】提示得跟实际行为对上。
		// 以前写的是"将删除安装目录下的所有文件"，而现在卸载是照清单删，
		// 用户自己放进这个文件夹的东西一律不动，所以这句话必须改——既不能吓人，也不能骗人。
		WCHAR szMsg[MAX_PATH + 460];
		swprintf_s(szMsg, MAX_PATH + 460,
			L"确定要卸载【%s】吗？\r\n\r\n"
			L"安装目录：%s\r\n"
			L"只删本程序自己的东西：装进去的 6 个文件，以及程序运行时\r\n"
			L"产生的草稿（draft）、VS调试工程（exam）和临时答案文件；\r\n"
			L"你自己放进这个文件夹的其它文件会原样保留。\r\n%s",
			APP_SHORTNAME, szDir,
			bKeepScore ? L"（成绩记录会先备份到桌面）" : L"（成绩记录一并删除）");
		if (MessageBoxW(NULL, szMsg, L"卸载", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) != IDYES)
			return;

		// 自己正在安装目录里运行，直接删不掉自己：把自己复制到临时目录再删
		WCHAR szTemp[MAX_PATH], szTmpExe[MAX_PATH], szSelf[MAX_PATH];
		GetTempPathW(MAX_PATH, szTemp);
		swprintf_s(szTmpExe, MAX_PATH, L"%suninst_%u.exe", szTemp, GetCurrentProcessId());
		GetModuleFileNameW(NULL, szSelf, MAX_PATH);
		if (CopyFileW(szSelf, szTmpExe, FALSE))
		{
			WCHAR szArgs[MAX_PATH + 64];
			swprintf_s(szArgs, MAX_PATH + 64, L"/uninstall /fromtemp \"%s\"%s",
				szDir, bKeepScore ? L" /keepscore" : L"");
			if ((INT_PTR)ShellExecuteW(NULL, L"open", szTmpExe, szArgs, szTemp, SW_SHOWNORMAL) > 32)
				return;
		}
		// 复制临时副本失败（极少见）：继续往下就地删，能删多少删多少
	}
	else if (wcsstr(GetCommandLineW(), L"/keepscore") != NULL)
	{
		// 临时副本模式：先把成绩备份到桌面，再开删
		BackupScoreToDesktop(szDir);
	}

	// 【修复】先删文件、确认结果，再动快捷方式和注册表。
	// 以前是反过来的：先删快捷方式/注册表，再删文件——一旦文件删不掉，
	// 卸载入口已经没了，用户彻底失去正规清理途径。
	WCHAR szLeft[MAX_PATH * 3] = { 0 };
	int nLeft = 0;
	DeleteInstalledOnly(szDir, TRUE, &nLeft, szLeft, MAX_PATH * 3);
	// 程序运行时产生的草稿 draft\、VS调试工程 exam\、临时答案一并清掉
	DeleteOwnedArtifacts(szDir, &nLeft, szLeft, MAX_PATH * 3);

	// 【修复】只有目录真的空了才删目录。
	// 以前是"递归清空整个目录再删"，用户放在里面的东西会一起没；
	// 现在删完清单里的文件后，只要还剩东西（用户自己的文件），就把目录留着。
	BOOL bDirExists = (GetFileAttributesW(szDir) != INVALID_FILE_ATTRIBUTES);
	BOOL bDirEmpty = !DirHasAnyEntry(szDir);
	if (bDirEmpty)
		RemoveDirectoryW(szDir);

	UnlinkShortcuts();
	// 删注册表（用 RegDeleteTreeW：键下万一有子键也能删掉，RegDeleteKeyW 会静默失败）
	RegDeleteTreeW(HKEY_CURRENT_USER, UNINSTALL_KEY);

	if (nLeft > 0)
	{
		// 有文件删不掉（被占用等）。这些文件已登记为"下次重启时删除"，
		// 所以这里如实告知，不要像以前那样谎报"卸载完成"。
		WCHAR szMsg2[MAX_PATH + 560];
		swprintf_s(szMsg2, MAX_PATH + 560,
			L"卸载没完全完成。\r\n\r\n"
			L"下面这些文件删不掉，通常是程序还在运行，或文件被其它软件占用：\r\n%s\r\n\r\n"
			L"它们已经登记为「下次开机时自动删除」。\r\n"
			L"重启一次电脑即可清干净；如果重启后还在，请手动删除：\r\n%s",
			szLeft, szDir);
		MessageBoxW(NULL, szMsg2, L"卸载未完成", MB_OK | MB_ICONWARNING | MB_TOPMOST);
	}
	else if (bDirExists && !bDirEmpty)
	{
		// 本程序的文件都删掉了，但目录里还有用户自己的东西 → 保留目录，说清楚
		WCHAR szMsg3[MAX_PATH + 460];
		swprintf_s(szMsg3, MAX_PATH + 460,
			L"本程序的文件已全部删除。\r\n\r\n"
			L"这个文件夹里还有你自己的文件，所以目录保留了、没有一起删掉：\r\n%s\r\n\r\n"
			L"确认不要了的话，你自己删掉这个文件夹即可。",
			szDir);
		MessageBoxW(NULL, szMsg3, L"卸载完成", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	}
	else
	{
		MessageBoxW(NULL, L"卸载完成，谢谢使用！", L"卸载", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	}

	// 【修复】自删必须排在最后一个弹窗之后：以前排在"卸载完成"框之前，
	// 用户多看几秒时临时副本还被自己锁着，cmd 删不掉也不重试，%TEMP% 就慢慢积下来了。
	if (bFromTemp)
		ScheduleSelfDelete();
}

// 静默卸载：不弹任何窗，结果写 install.log + 退出码。
// 退出码：0 成功 / 2 找不到安装目录 / 5 有文件删不掉（已登记下次开机自动删除）
static int DoUninstallSilent(BOOL bFromTemp, const WCHAR* pszCmd, const WCHAR* pszDirArg)
{
	OpenLog(FALSE);
	LogLine(L"======== 静默卸载开始 ========");

	// 目录来源：命令行给的位置 → /D= → 注册表
	WCHAR szDir[MAX_PATH] = { 0 };
	if (pszDirArg != NULL && pszDirArg[0] != L'\0')
		wcsncpy_s(szDir, MAX_PATH, pszDirArg, _TRUNCATE);
	else if (!GetDirSwitch(pszCmd, szDir, MAX_PATH))
		GetInstalledDir(szDir, MAX_PATH);
	if (szDir[0] == L'\0')
	{
		LogLine(L"找不到安装目录，静默卸载结束（退出码 2）。");
		CloseLog();
		return 2;
	}
	LogLine(L"卸载目录：%s", szDir);

	// 挪开工作目录，否则删不掉"正被当作工作目录"的安装目录
	{
		WCHAR szTempPath[MAX_PATH] = { 0 };
		if (GetTempPathW(MAX_PATH, szTempPath) > 0)
			SetCurrentDirectoryW(szTempPath);
	}

	// 自己就在要删的目录里（卸载.exe /S）：先复制一份到 %TEMP%，让副本去干活。
	// Windows 上"正在运行的 exe"删不掉自己，不绕这一步就必然剩一个文件删不掉。
	WCHAR szSelf[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, szSelf, MAX_PATH);
	size_t nDirLen = wcslen(szDir);
	if (nDirLen > 0 && _wcsnicmp(szSelf, szDir, nDirLen) == 0 && szSelf[nDirLen] == L'\\')
	{
		WCHAR szTemp[MAX_PATH], szTmpExe[MAX_PATH];
		GetTempPathW(MAX_PATH, szTemp);
		swprintf_s(szTmpExe, MAX_PATH, L"%suninst_%u.exe", szTemp, GetCurrentProcessId());
		if (CopyFileW(szSelf, szTmpExe, FALSE))
		{
			WCHAR szCmdLine[MAX_PATH * 3], szArgs[MAX_PATH + 64];
			swprintf_s(szArgs, MAX_PATH + 64, L"/uninstall /fromtemp \"%s\" /S", szDir);
			swprintf_s(szCmdLine, MAX_PATH * 3, L"\"%s\" %s", szTmpExe, szArgs);
			STARTUPINFOW si;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));
			if (CreateProcessW(NULL, szCmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, szTemp, &si, &pi))
			{
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				LogLine(L"已交给临时副本处理（本进程必须立刻退出，否则删不掉自己）。");
				CloseLog();
				return 0;
			}
		}
		LogLine(L"复制临时副本失败，改为就地删除。");
	}

	// 就地删除：从别处启动时走这里，临时副本也走这里
	WCHAR szLeft[MAX_PATH * 3] = { 0 };
	int nLeft = 0;
	DeleteInstalledOnly(szDir, TRUE, &nLeft, szLeft, MAX_PATH * 3);
	// 程序运行时产生的草稿 draft\、VS调试工程 exam\、临时答案一并清掉
	DeleteOwnedArtifacts(szDir, &nLeft, szLeft, MAX_PATH * 3);
	if (!DirHasAnyEntry(szDir) && RemoveDirectoryW(szDir))
		LogLine(L"安装目录已删除：%s", szDir);
	else
		LogLine(L"安装目录保留了（里面还有不属于本程序的文件）：%s", szDir);
	UnlinkShortcuts();
	RegDeleteTreeW(HKEY_CURRENT_USER, UNINSTALL_KEY);
	LogLine(L"桌面/开始菜单快捷方式、注册表卸载信息已清理。");

	int rc = 0;
	if (nLeft == 0)
		LogLine(L"静默卸载完成（退出码 0）。");
	else
	{
		rc = 5;
		LogLine(L"静默卸载未完全完成：删不掉 %s（已登记下次开机自动删除），退出码 %d。", szLeft, rc);
	}

	// 临时副本删自己：静默模式没有等人点按钮的弹窗，进程马上退出，删得掉
	if (bFromTemp)
		ScheduleSelfDelete();

	LogLine(L"======== 静默卸载结束 ========");
	CloseLog();
	return rc;
}

// ===== 静默安装 =====
// 用法：对口升学练习系统_安装程序.exe /S [/D=目录] [/NORUN] [/NODESKTOP] [/NOSTARTMENU] [/RUN]
//   · /D=目录（路径有空格就用引号：/D="C:\\我的 目录"）
//   · 静默模式默认"建桌面和开始菜单快捷方式、不自动运行"（机房要接着装下一台）
// 退出码：0 成功 / 1 未知错误 / 2 目录不合法或写不进去 / 4 主程序正在运行
// 全程写 install.log（安装程序旁边，写不进去就放 %TEMP%）
static int DoSilentInstall(const WCHAR* pszCmd)
{
	OpenLog(TRUE);
	LogLine(L"======== 静默安装开始 ========");
	LogLine(L"命令行：%s", (pszCmd != NULL) ? pszCmd : L"");

	// 目标目录：/D= 优先，其次"上次装的位置"，最后默认目录
	WCHAR szDir[MAX_PATH] = { 0 };
	if (GetDirSwitch(pszCmd, szDir, MAX_PATH))
	{
		LogLine(L"按 /D= 指定安装到：%s", szDir);
	}
	else
	{
		WCHAR szOld[MAX_PATH] = { 0 };
		GetInstalledDir(szOld, MAX_PATH);
		if (szOld[0] != L'\0')
			wcsncpy_s(szDir, MAX_PATH, szOld, _TRUNCATE);
		else
			wcsncpy_s(szDir, MAX_PATH, DEFAULT_DIR, _TRUNCATE);
		LogLine(L"没有 /D= 参数，安装到：%s", szDir);
	}
	size_t n = wcslen(szDir);
	while (n > 3 && szDir[n - 1] == L'\\')
		szDir[--n] = L'\0';
	if (szDir[0] == L'\0')
	{
		LogLine(L"安装目录为空，静默安装结束（退出码 2）。");
		CloseLog();
		return 2;
	}

	// 目录合法性：和界面版同一套规则。静默模式没人在旁边看，更要拦住。
	WCHAR szWhy[MAX_PATH + 300] = { 0 };
	if (!IsSafeInstallDir(szDir, szWhy, MAX_PATH + 300))
	{
		LogLine(L"安装目录不合适：%s", szWhy);
		LogLine(L"静默安装结束（退出码 2）。");
		CloseLog();
		return 2;
	}

	// 盘符回退 + 同名加序号（和界面版 ResolveInstallDir 同一套逻辑，别再各写一份）
	{
		WCHAR szResolved[MAX_PATH] = { 0 };
		BOOL bFallback = FALSE, bNumbered = FALSE;
		if (!ResolveInstallDir(szDir, szResolved, MAX_PATH, &bFallback, &bNumbered))
		{
			LogLine(L"安装目录解析失败（目录为空），静默安装结束（退出码 2）。");
			CloseLog();
			return 2;
		}
		if (bFallback || bNumbered)
			LogLine(L"最终安装目录：%s（盘符回退=%s，同名加序号=%s）",
				szResolved, bFallback ? L"是" : L"否", bNumbered ? L"是" : L"否");
		wcsncpy_s(szDir, MAX_PATH, szResolved, _TRUNCATE);
		// 回退/加序号后的目录再校验一次（正常必然通过；万一不通过就按住不动）
		if (!IsSafeInstallDir(szDir, szWhy, MAX_PATH + 300))
		{
			LogLine(L"回退/加序号后的目录不合适：%s", szWhy);
			LogLine(L"静默安装结束（退出码 2）。");
			CloseLog();
			return 2;
		}
	}

	// 最终装到哪儿：日志里留一行 FINAL_DIR=（给人看/可解析），
	// 另写一个 ANSI 小文件 last_dir.txt 给「静默安装.bat」直接读（bat 是 GBK，读 UTF-8 的 log 会乱码）
	LogLine(L"FINAL_DIR=%s", szDir);
	WriteLastDirFile(szDir);

	// 主程序在跑就装不进去（文件被占用）
	if (IsMainAppRunning())
	{
		LogLine(L"检测到主程序正在运行，静默安装结束（退出码 4）。");
		CloseLog();
		return 4;
	}

	// 装在别的地方：先把旧位置的本程序文件按清单删掉，
	// 免得留下一个再也卸载不掉的孤儿目录。（静默模式不问用户：/D= 说装哪儿就装哪儿）
	{
		WCHAR szOld[MAX_PATH] = { 0 };
		GetInstalledDir(szOld, MAX_PATH);
		if (ShouldCleanupOldDir(szOld, szDir))
		{
			WCHAR szLeft[MAX_PATH * 3] = { 0 };
			int nLeft = 0;
			DeleteInstalledOnly(szOld, TRUE, &nLeft, szLeft, MAX_PATH * 3);
			// 程序运行时产生的草稿 draft\、VS调试工程 exam\、临时答案一并清掉
			DeleteOwnedArtifacts(szOld, &nLeft, szLeft, MAX_PATH * 3);
			if (!DirHasAnyEntry(szOld))
				RemoveDirectoryW(szOld);
			LogLine(L"清理旧安装位置：%s（未删掉：%s）", szOld, (nLeft == 0) ? L"无" : szLeft);
		}
	}

	// 快捷方式开关
	BOOL bDesktop = !HasSwitch(pszCmd, L"NODESKTOP");
	BOOL bStartMenu = !HasSwitch(pszCmd, L"NOSTARTMENU");
	BOOL bRun = HasSwitch(pszCmd, L"RUN") && !HasSwitch(pszCmd, L"NORUN");
	LogLine(L"桌面快捷方式=%s，开始菜单快捷方式=%s，装完立即运行=%s",
		bDesktop ? L"建" : L"不建", bStartMenu ? L"建" : L"不建", bRun ? L"是" : L"否");

	WCHAR szMsg[1024] = { 0 };
	BOOL bOK = DoInstall(szDir, bDesktop, bStartMenu, bRun, szMsg, 1024);
	// DoInstall 的返回文案是给人看的（带换行），日志里压成一行
	{
		WCHAR szOne[1024];
		wcsncpy_s(szOne, 1024, szMsg, _TRUNCATE);
		for (WCHAR* p = szOne; *p != L'\0'; p++)
		{
			if (*p == L'\r' || *p == L'\n')
				*p = L' ';
		}
		LogLine(bOK ? L"安装成功：%s" : L"安装失败：%s", szOne);
	}

	int rc = bOK ? 0 : 2;
	LogLine(L"======== 静默安装结束（退出码 %d）========", rc);
	CloseLog();
	return rc;
}

// ================= 界面 =================

static void OnBrowse(HWND hDlg)
{
	BROWSEINFOW bi;
	ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner = hDlg;
	bi.lpszTitle = L"选择安装位置";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
	if (pidl != NULL)
	{
		WCHAR szPath[MAX_PATH] = { 0 };
		if (SHGetPathFromIDListW(pidl, szPath))
			SetDlgItemTextW(hDlg, IDC_EDIT_DIR, szPath);
		CoTaskMemFree(pidl);
	}
}

static INT_PTR CALLBACK SetupProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_SETUP)));
		SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_SETUP)));
		SetDlgItemTextW(hDlg, IDC_EDIT_DIR, DEFAULT_DIR);
		CheckDlgButton(hDlg, IDC_CHK_DESKTOP, BST_CHECKED);
		CheckDlgButton(hDlg, IDC_CHK_STARTMENU, BST_CHECKED);
		CheckDlgButton(hDlg, IDC_CHK_RUN, BST_CHECKED);
		SetDlgItemTextW(hDlg, IDC_STATIC_STATUS, L"点【开始安装】即可完成安装。");

		// 题数从内嵌题库里现算：以后加题只用重新打包题库，不用回来改这几行文案。
		// （以前"159 道题"是写死在 .rc 里的三处，加题忘了改就会对不上。）
		{
			WCHAR szLine[MAX_PATH];
			swprintf_s(szLine, MAX_PATH, L"版本 %s · 作者 %s", APP_VERSION_STR_W, APP_AUTHOR_STR_W);
			SetDlgItemTextW(hDlg, IDC_STATIC_AUTHOR, szLine);

			int nTotal = 0, nReal = 0;
			if (GetBankStats(&nTotal, &nReal))
			{
				swprintf_s(szLine, MAX_PATH, L"%d 道题 · %d 道真题 · 提交即判分", nTotal, nReal);
				SetDlgItemTextW(hDlg, IDC_STATIC_BRIEF, szLine);
				swprintf_s(szLine, MAX_PATH, L"与省考场同环境 · 每题 8 个用例 · 满分 12 分");
				SetDlgItemTextW(hDlg, IDC_STATIC_INFO, szLine);
			}
			else
			{
				// 题库读不出来（格式变了等）：不写数字，免得界面上的数字是假的
				SetDlgItemTextW(hDlg, IDC_STATIC_BRIEF, L"提交即判分 · 与省考场同环境");
				SetDlgItemTextW(hDlg, IDC_STATIC_INFO, L"题库已内嵌，装完即用");
			}
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BTN_BROWSE:
			OnBrowse(hDlg);
			return TRUE;

		case IDOK:
		{
			WCHAR szDir[MAX_PATH] = { 0 };
			GetDlgItemTextW(hDlg, IDC_EDIT_DIR, szDir, MAX_PATH);
			// 去掉结尾的反斜杠
			size_t n = wcslen(szDir);
			while (n > 3 && szDir[n - 1] == L'\\')
				szDir[--n] = L'\0';
			if (szDir[0] == L'\0')
			{
				MessageBoxW(hDlg, L"请先填写安装位置。", L"提示", MB_OK | MB_ICONWARNING | MB_TOPMOST);
				return TRUE;
			}

			// 路径太长会被截断（Windows 传统路径上限 260 字符），
			// 截断后可能装到完全不同的地方去，所以先量一下用户真正敲了多少字符
			{
				int nTyped = GetWindowTextLengthW(GetDlgItem(hDlg, IDC_EDIT_DIR));
				if (nTyped >= MAX_PATH - 1)
				{
					MessageBoxW(hDlg,
						L"安装路径太长了。\r\n\r\n"
						L"请把安装位置改短一些（总共不超过 255 个字符），\r\n"
						L"例如 C:\\对口升学练习系统。",
						L"提示", MB_OK | MB_ICONWARNING | MB_TOPMOST);
					return TRUE;
				}
			}

			// 【修复】安装位置安全校验：盘根 / Windows 目录 / 系统特殊目录一律拦住。
			// 以前完全不校验，用户真把路径填成 C:\，6 个文件就散在盘根上了。
			{
				WCHAR szWhy[MAX_PATH + 300] = { 0 };
				if (!IsSafeInstallDir(szDir, szWhy, MAX_PATH + 300))
				{
					MessageBoxW(hDlg, szWhy, L"安装位置不合适", MB_OK | MB_ICONWARNING | MB_TOPMOST);
					return TRUE;
				}
			}

			// 【新增】盘符回退 + 同名加序号（和静默版共用 ResolveInstallDir）。
			// 用户填的目录可能"所在盘没了"或"同名文件夹被占了"，统一在这里定下最终目录。
			{
				WCHAR szTyped[MAX_PATH];
				wcsncpy_s(szTyped, MAX_PATH, szDir, _TRUNCATE);
				WCHAR szResolved[MAX_PATH] = { 0 };
				BOOL bFallback = FALSE, bNumbered = FALSE;
				if (!ResolveInstallDir(szDir, szResolved, MAX_PATH, &bFallback, &bNumbered))
				{
					MessageBoxW(hDlg, L"请先填写安装位置。", L"提示", MB_OK | MB_ICONWARNING | MB_TOPMOST);
					return TRUE;
				}
				wcsncpy_s(szDir, MAX_PATH, szResolved, _TRUNCATE);
				if (bFallback || bNumbered)
				{
					// 把最终目录回显到输入框，并明确告知，别让用户以为装到了自己填的那个路径
					SetDlgItemTextW(hDlg, IDC_EDIT_DIR, szDir);
					WCHAR szWhy2[160];
					if (bFallback)
						wcsncpy_s(szWhy2, 160, L"（原定目录所在盘不可用，已改用其它可用固定盘）", _TRUNCATE);
					else
						wcsncpy_s(szWhy2, 160, L"（原定同名文件夹已被占用，已自动加序号）", _TRUNCATE);
					WCHAR szNote2[MAX_PATH * 3];
					swprintf_s(szNote2, MAX_PATH * 3,
						L"安装位置已自动调整。\r\n\r\n原定：%s\r\n实际：%s\r\n\r\n%s",
						szTyped, szDir, szWhy2);
					MessageBoxW(hDlg, szNote2, L"安装位置已调整", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
				}
			}

			// 【修复】先拦住"主程序正在运行"，必须排在所有删除动作之前。
			// 以前这条检查排在后面，结果是：用户选了"清掉旧目录"→ 旧目录已经被拆掉
			//（连 卸载.exe 都没了），才提示"请先关闭程序"，旧安装直接报废，
			// 注册表里的卸载入口也变成死链。
			if (IsMainAppRunning())
			{
				MessageBoxW(hDlg,
					L"检测到【对口升学练习系统】正在运行。\r\n\r\n程序开着会占用文件，安装会失败。\r\n请先关闭它，再点【开始安装】。",
					L"请先关闭程序", MB_OK | MB_ICONWARNING | MB_TOPMOST);
				return TRUE;
			}

			// 【修复】已经装在别的地方时先问清楚。
			// 以前不问，结果是：换目录重装后旧目录整个留下成了没法卸载的孤儿。
			{
				WCHAR szOld[MAX_PATH] = { 0 };
				GetInstalledDir(szOld, MAX_PATH);
				if (ShouldCleanupOldDir(szOld, szDir))
				{
					WCHAR szAsk[MAX_PATH * 2];
					swprintf_s(szAsk, MAX_PATH * 2,
						L"检测到本机已经安装在：\r\n%s\r\n\r\n"
						L"点【是】：清掉旧目录，装到新位置\r\n%s\r\n"
						L"点【否】：装回原来的位置（覆盖安装）",
						szOld, szDir);
					if (MessageBoxW(hDlg, szAsk, L"已经安装过",
						MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES)
					{
						// 旧目录里的成绩文件先搬到新目录，别跟着旧目录一起删了
						// （score.dat 数据 + 成绩表.xlsx + 老版本的 score.txt 都在清单里）
						MoveScoreFiles(szOld, szDir);
						// 【修复】旧位置也照清单删：只删本程序自己的文件，用户放在旧目录里的东西不碰。
						{
							WCHAR szLeft[MAX_PATH * 3] = { 0 };
							int nLeft = 0;
							DeleteInstalledOnly(szOld, TRUE, &nLeft, szLeft, MAX_PATH * 3);
							// 程序运行时产生的草稿 draft\、VS调试工程 exam\、临时答案一并清掉
							DeleteOwnedArtifacts(szOld, &nLeft, szLeft, MAX_PATH * 3);
							if (!DirHasAnyEntry(szOld))
								RemoveDirectoryW(szOld);
							LogLine(L"已清理旧安装位置：%s（未删掉：%s）", szOld, (nLeft == 0) ? L"无" : szLeft);

							// 【修复】本程序自己的文件没删掉（被占用）就别继续：否则旧目录残留、
							// 卸载入口却已经没了，用户只剩手动删。用户自己的文件不影响继续安装。
							if (nLeft > 0)
							{
								WCHAR szErr[MAX_PATH + 360];
								swprintf_s(szErr, MAX_PATH + 360,
									L"旧目录里的本程序文件删不掉（正被占用）：\r\n%s\r\n\r\n"
									L"涉及文件：%s\r\n\r\n"
									L"本次安装已取消，不会装到新位置。\r\n"
									L"请关闭旧版本程序、或重启电脑后再试一次。",
									szOld, szLeft);
								MessageBoxW(hDlg, szErr, L"无法清理旧目录",
									MB_OK | MB_ICONERROR | MB_TOPMOST);
								return TRUE;
							}
							// 旧目录里如果还剩用户自己的东西，目录就保留；装完在完成提示里说明一句
							if (DirHasAnyEntry(szOld))
								wcsncpy_s(g_szOldDirKept, MAX_PATH, szOld, _TRUNCATE);
						}
						}
					else
					{
						wcsncpy_s(szDir, MAX_PATH, szOld, _TRUNCATE);

						SetDlgItemTextW(hDlg, IDC_EDIT_DIR, szDir);
					}
				}
			}

			// 【修复】装到"非空目录"先确认一句（默认选【否】）。
			// 以前不问：用户可能把程序装进一个已经有资料的文件夹，虽然现在卸载只删自己的文件，
			// 但混在一起终究容易出事，让用户自己确认一次。
			{
			BOOL bIsOurs = FALSE;
			{
				WCHAR szProbe[MAX_PATH];
				swprintf_s(szProbe, MAX_PATH, L"%s\\ExamDlgProj.exe", szDir);
				bIsOurs = (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES);
			}
			if (!bIsOurs && GetFileAttributesW(szDir) != INVALID_FILE_ATTRIBUTES)
			{
				int nEntries = CountDirEntries(szDir);
				if (nEntries > 0)
				{
					WCHAR szAsk[MAX_PATH + 460];
					swprintf_s(szAsk, MAX_PATH + 460,
						L"这个文件夹不是空的，里面已经有 %d 个文件或文件夹：\r\n%s\r\n\r\n"
						L"点【否】（推荐）：返回去，重新选一个空文件夹\r\n"
						L"点【是】：还是装在这里\r\n\r\n"
						L"（装在这里也能正常卸载：只删本程序自己的文件，以及程序\r\n"
						L"　运行时产生的草稿（draft）和调试工程（exam）文件夹，\r\n"
						L"　这个文件夹里原有的东西不会被删掉。）",
						nEntries, szDir);
					if (MessageBoxW(hDlg, szAsk, L"文件夹不是空的",
						MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_TOPMOST) != IDYES)
						return TRUE;
				}
			}
			}

			BOOL bDesktop = (IsDlgButtonChecked(hDlg, IDC_CHK_DESKTOP) == BST_CHECKED);
			BOOL bStartMenu = (IsDlgButtonChecked(hDlg, IDC_CHK_STARTMENU) == BST_CHECKED);
			BOOL bRun = (IsDlgButtonChecked(hDlg, IDC_CHK_RUN) == BST_CHECKED);

			SetDlgItemTextW(hDlg, IDC_STATIC_STATUS, L"正在安装，请稍候……");
			EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
			UpdateWindow(hDlg);

			WCHAR szMsg[1024] = { 0 };
			BOOL bOK = DoInstall(szDir, bDesktop, bStartMenu, bRun, szMsg, 1024);

			EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
			if (bOK)
			{
				if (g_szOldDirKept[0] != L'\0')
				{
					WCHAR szTip[MAX_PATH + 520];
					swprintf_s(szTip, MAX_PATH + 520,
						L"%s\r\n\r\n另外：旧文件夹里还有你自己的文件，所以那个目录保留了：\r\n%s\r\n"
						L"确认不要了的话，你自己删掉它即可。",
						szMsg, g_szOldDirKept);
					MessageBoxW(hDlg, szTip, L"安装完成", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
				}
				else
				{
					MessageBoxW(hDlg, szMsg, L"安装完成", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
				}

				EndDialog(hDlg, IDOK);
			}
			else
			{
				SetDlgItemTextW(hDlg, IDC_STATIC_STATUS, L"安装失败，请看提示信息。");
				MessageBoxW(hDlg, szMsg, L"安装失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
			}
			return TRUE;
		}

		case IDCANCEL:
			if (MessageBoxW(hDlg, L"确定要退出安装吗？", L"安装程序",
				MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES)
				EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

int wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{
	g_hInst = hInstance;
	CoInitialize(NULL);

	WCHAR szCmd[MAX_PATH * 4] = { 0 };
	wcsncpy_s(szCmd, MAX_PATH * 4, (lpCmdLine != NULL) ? lpCmdLine : L"", _TRUNCATE);

	// 静默模式：/S（也认 /SILENT）。全程不弹窗，靠退出码 + install.log 汇报。
	g_bSilent = (HasSwitch(szCmd, L"S") || HasSwitch(szCmd, L"SILENT"));

	// 卸载模式判断：
	//   1）这个构建就是专用卸载程序（编译时带 PURE_UNINSTALL）
	//   2）命令行带 /uninstall（注册表里的卸载命令用这个）
	//   3）程序自己的文件名以"卸载"开头（安装目录里那份，双击即可卸载）
	WCHAR szSelf[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, szSelf, MAX_PATH);
	const WCHAR* pszName = wcsrchr(szSelf, L'\\');
	pszName = (pszName != NULL) ? (pszName + 1) : szSelf;

#ifdef PURE_UNINSTALL
	BOOL bUninstallMode = TRUE;
#else
	BOOL bUninstallMode = (wcsncmp(pszName, L"卸载", 2) == 0)
		|| (wcsncmp(pszName, L"Uninst", 6) == 0)
		|| HasSwitch(szCmd, L"uninstall");
#endif

	if (bUninstallMode)
	{
		BOOL bFromTemp = HasSwitch(szCmd, L"fromtemp");
		WCHAR szDirArg[MAX_PATH] = { 0 };
		if (bFromTemp)
			ExtractDirArg(szDirArg, MAX_PATH);

		if (g_bSilent)
		{
			int rc = DoUninstallSilent(bFromTemp, szCmd, szDirArg);
			CoUninitialize();
			return rc;
		}
		DoUninstall(bFromTemp, szDirArg);
		CoUninitialize();
		return 0;
	}

#ifndef PURE_UNINSTALL
	if (g_bSilent)
	{
		int rc = DoSilentInstall(szCmd);
		CoUninitialize();
		return rc;
	}
	DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_SETUP), NULL, SetupProc);
#endif

	CoUninitialize();
	return 0;
}
