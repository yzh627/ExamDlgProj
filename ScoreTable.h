#pragma once
// ============================================================================
//  成绩记录模块（内部数据 score.dat + 展示用 成绩表.xlsx）
// ----------------------------------------------------------------------------
//  为什么这么设计：
//   · score.dat     唯一权威数据源。一行一题，同一道题只保留【最近一次】的成绩，
//                   整体按【题号】从小到大排列。UTF-8 纯文本，好读好改。
//   · 成绩表.xlsx   纯展示文件：每次写完 score.dat 立刻整份重新生成。
//                   删了不要紧，下次打开程序会自动重建，数据一点不丢。
//
//  xlsx 是本程序按 OOXML 最小结构自己拼出来的（xlsx 本质就是个 zip 包），
//  所以下面有一小段"只用 Store、不压缩"的 zip 写入代码。
//  好处：不依赖任何第三方库，也不需要学生机装 Excel/WPS ——
//  只要能打开 xlsx 的软件（Excel、WPS、LibreOffice、各种在线表格）都能看。
// ============================================================================

#include <afxwin.h>
#include <afxtempl.h>
#include <shellapi.h>
#include <shlwapi.h>                      // PathFileExists：打开前先确认文件在
#pragma comment(lib, "shlwapi.lib")       // 免得还要去改工程文件加依赖库
#include "PublicDef.h"

namespace ScoreTable
{

// ======================= 一、公用小工具 =======================

inline CString NowString()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	CString str;
	str.Format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	return str;
}

// 目录是否可写（程序装在 Program Files 等只读目录时用来判断兜底）
inline BOOL IsDirWritable(const CString& strDir)
{
	CString strTest = strDir + _T("_wtest.tmp");
	HANDLE h = CreateFile(strTest, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return FALSE;
	CloseHandle(h);
	return TRUE;
}

// 保存目录：优先 exe 所在目录；该目录不可写（例如装在 C:\Program Files）时
// 自动改用 我的文档\对口升学练习系统\。
// ★ 做题界面和主界面共用这一个函数，保证两边算出来的路径永远一致。
inline CString ResolveSaveDir()
{
	static CString s_strCached;
	if (!s_strCached.IsEmpty())
		return s_strCached;

	CString strDir = GetExeDir();
	if (!IsDirWritable(strDir))
	{
		CString strFallback;
		TCHAR szProfile[MAX_PATH] = { 0 };
		if (GetEnvironmentVariable(_T("USERPROFILE"), szProfile, MAX_PATH) > 0)
			strFallback.Format(_T("%s\\Documents\\对口升学练习系统\\"), szProfile);
		else
		{
			TCHAR szTmp[MAX_PATH] = { 0 };
			GetTempPath(MAX_PATH, szTmp);
			strFallback.Format(_T("%s对口升学练习系统\\"), szTmp);
		}
		CreateDirectory(strFallback, nullptr);
		if (IsDirWritable(strFallback))
			strDir = strFallback;
	}
	s_strCached = strDir;
	return s_strCached;
}

inline CString DataPath()      { return ResolveSaveDir() + _T("score.dat"); }
inline CString XlsxPath()      { return ResolveSaveDir() + _T("成绩表.xlsx"); }
inline CString LegacyTxtPath() { return ResolveSaveDir() + _T("score.txt"); }

// 读 UTF-8 文本（自动跳过 BOM）；读不到返回 FALSE
inline BOOL ReadTextUtf8(const CString& strPath, CString& strOut)
{
	strOut.Empty();
	CFile f;
	if (!f.Open(strPath, CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary))
		return FALSE;
	ULONGLONG nLen64 = f.GetLength();
	if (nLen64 == 0 || nLen64 > 16ULL * 1024 * 1024)
	{
		f.Close();
		return FALSE;
	}
	int nLen = (int)nLen64;
	BYTE* pBuf = new BYTE[nLen + 1];
	f.Read(pBuf, nLen);
	f.Close();
	pBuf[nLen] = 0;
	int nStart = 0;
	if (nLen >= 3 && pBuf[0] == 0xEF && pBuf[1] == 0xBB && pBuf[2] == 0xBF)
		nStart = 3;
	int nWide = MultiByteToWideChar(CP_UTF8, 0, (const char*)pBuf + nStart, nLen - nStart, nullptr, 0);
	if (nWide > 0)
	{
		MultiByteToWideChar(CP_UTF8, 0, (const char*)pBuf + nStart, nLen - nStart,
			strOut.GetBuffer(nWide + 1), nWide);
		strOut.ReleaseBuffer(nWide);
	}
	delete[] pBuf;
	return TRUE;
}

// 写 UTF-8 文本（默认带 BOM，记事本打开不乱码）
inline BOOL WriteTextUtf8(const CString& strPath, const CString& strText, BOOL bBom = TRUE)
{
	CFile f;
	if (!f.Open(strPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
		return FALSE;
	if (bBom)
	{
		BYTE bom[] = { 0xEF, 0xBB, 0xBF };
		f.Write(bom, 3);
	}
	int nLen = WideCharToMultiByte(CP_UTF8, 0, strText, strText.GetLength(), nullptr, 0, nullptr, nullptr);
	if (nLen > 0)
	{
		char* pBuf = new char[nLen];
		WideCharToMultiByte(CP_UTF8, 0, strText, strText.GetLength(), pBuf, nLen, nullptr, nullptr);
		f.Write(pBuf, (UINT)nLen);
		delete[] pBuf;
	}
	f.Close();
	return TRUE;
}

// ---------------------------------------------------------------------------
// SHOpenWithDialog 的参数。照抄 shellapi.h 里的 OPENASINFO 自己声明一份：
// 免得受 SDK 版本、_WIN32_WINNT 的影响（老 SDK 里根本没这个结构）。
// 字段顺序和类型必须和系统头文件里的一模一样。
// ---------------------------------------------------------------------------
struct OpenAsInfo
{
	LPCWSTR pcszFile;      // 要打开的文件（全路径）
	LPCWSTR pcszClass;     // 扩展名/ProgID，一般给 NULL 让系统自己认
	int     oaifInFlags;   // 下面那组 OAIF_ 标志
};
enum
{
	kOAIF_ALLOW_REGISTRATION = 0x00000001,  // 允许勾"始终使用此应用打开"，学生能顺手设默认
	kOAIF_REGISTER_EXT       = 0x00000002,
	kOAIF_EXEC               = 0x00000004,  // 选完就把文件打开（只弹框不打开的话就别加）
	kOAIF_FORCE_REGISTRATION = 0x00000008,
};

// ---------------------------------------------------------------------------
// 打开成绩表：把系统的"选择一个应用"对话框摆出来，让学生自己挑用什么软件看
// ---------------------------------------------------------------------------
// 为什么不能只用一句 ShellExecute(..., "open", ...)：
//   "open" 动词走的是"默认程序"。机器上把 .xlsx 默认关联成 Excel 2010 之后，
//   学生一点【查看成绩表】，Excel 就一声不响地起来了 —— 想换成 WPS、
//   或者想先看看这个文件到底放在哪个目录，界面上都没有入口。
//   要的是把"用哪个软件打开"这个决定权交回给学生，所以这里走 Windows
//   专门为这件事留的两条路：
//     ① SHOpenWithDialog —— 系统自带的"选择一个应用"对话框（Win8 及以后），
//        和右键"打开方式 → 选择其它应用"弹出来的是同一个窗口；
//        不管机器上有没有设过默认程序，它都一定会弹。
//     ② "openas" 动词      —— XP 时代就有的老"打开方式"，Win7 机房走这条。
//   这两条路都是【先把选择器放到学生面前、等他选完或关掉才返回】，
//   真正启动哪个程序由系统按学生的选择去办。
//
// 学生把选择器直接关掉 = 他自己不想看了，按"已处理"记（返回 TRUE），
// 不用再弹一个"是不是没装表格软件"去烦他。
inline BOOL ShellOpen(const CString& strPath)
{
	if (!::PathFileExists((LPCTSTR)strPath))
		return FALSE;

	// 选择器挂在本程序主窗口上：会跟着居中，也不会一点弹出好几个
	HWND hParent = nullptr;
	{
		CWnd* pMain = AfxGetMainWnd();
		if (pMain != nullptr && ::IsWindow(pMain->GetSafeHwnd()))
			hParent = pMain->GetSafeHwnd();
	}

	// ---------- ① 首选：官方接口 SHOpenWithDialog（Win8 及以后）----------
	// 动态取函数地址：Win7 的 shell32 里没有这个导出，静态链接会连程序都起不来
	// （注意：不要把 "openas" 动词排到它前面 —— 在较新的 Windows 11 上
	//   openas 会弹"该文件没有与之关联的应用"的报错框，实测 2026-09-18）
	{
		HMODULE hShell = ::GetModuleHandleW(L"shell32.dll");
		if (hShell == nullptr)
			hShell = ::LoadLibraryW(L"shell32.dll");
		if (hShell != nullptr)
		{
			typedef HRESULT(WINAPI* PFN_SHOPENWITHDIALOG)(HWND, const OpenAsInfo*);
			PFN_SHOPENWITHDIALOG pfnOpenWith =
				(PFN_SHOPENWITHDIALOG)::GetProcAddress(hShell, "SHOpenWithDialog");
			if (pfnOpenWith != nullptr)
			{
				OpenAsInfo oi;
				oi.pcszFile    = (LPCWSTR)strPath;
				oi.pcszClass   = nullptr;
				oi.oaifInFlags = kOAIF_ALLOW_REGISTRATION | kOAIF_EXEC;
				HRESULT hr = pfnOpenWith(hParent, &oi);
				if (SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
					return TRUE;   // 挑好了 / 学生自己关掉了，都算处理完
			}
		}
	}

	// ---------- ② 退路："openas" 动词（老系统没有 SHOpenWithDialog 时走这里）----------
	{
		SHELLEXECUTEINFO sei = { 0 };
		sei.cbSize = sizeof(sei);
		sei.hwnd   = hParent;
		sei.lpVerb = _T("openas");
		sei.lpFile = (LPCTSTR)strPath;
		sei.nShow  = SW_SHOWNORMAL;
		if (::ShellExecuteEx(&sei))
			return TRUE;
		if (::GetLastError() == ERROR_CANCELLED)
			return TRUE;       // 学生自己关掉的，不算失败
	}

	// ---------- ③ 最后兜底：默认程序直接开（连选择器都调不起来的老机器）----------
	{
		HINSTANCE hInst = ::ShellExecute(hParent, _T("open"), (LPCTSTR)strPath,
			nullptr, nullptr, SW_SHOWNORMAL);
		if ((INT_PTR)hInst > 32)
			return TRUE;
	}

	return FALSE;
}

// 打开资源管理器并选中该文件
inline void ShellRevealInFolder(const CString& strPath)
{
	CString strArg = _T("/select,\"") + strPath + _T("\"");
	ShellExecute(nullptr, _T("open"), _T("explorer.exe"), strArg, nullptr, SW_SHOWNORMAL);
}

// ======================= 二、数据模型与 score.dat =======================

struct Rec
{
	int nID;            // 题号
	int nScore;         // 得分
	int nTotal;         // 满分
	CString strTime;    // 最近一次作答时间
	CString strTitle;   // 题目标题
	CString strAnswer;  // 最近一次的答案

	Rec() : nID(0), nScore(0), nTotal(0) {}
};

#define SCORETAB_MAGIC      _T("SCORETAB1")   // 文件首行魔数（将来改格式时好认版本）
#define SCORETAB_MAX_ANSWER 200000            // 单条答案最多存 20 万字符，防异常数据撑爆文件
#define XLSX_MAX_CELL_CHARS 32000             // Excel 单元格上限 32767，留点余量

// score.dat 的字段转义：\ → \\   换行 → \n   制表符 → \t   （字段之间用真制表符分隔）
inline void EscapeField(const CString& str, CString& strOut)
{
	strOut.Empty();
	strOut.Preallocate(str.GetLength() + 8);
	for (int i = 0; i < str.GetLength(); i++)
	{
		TCHAR c = str[i];
		if (c == _T('\\'))
			strOut += _T("\\\\");
		else if (c == _T('\r'))
		{
			if (i + 1 < str.GetLength() && str[i + 1] == _T('\n'))
				continue;                    // \r\n 合成一个 \n
			strOut += _T("\\n");
		}
		else if (c == _T('\n'))
			strOut += _T("\\n");
		else if (c == _T('\t'))
			strOut += _T("\\t");
		else
			strOut += c;
	}
}

inline void UnescapeField(const CString& str, CString& strOut)
{
	strOut.Empty();
	strOut.Preallocate(str.GetLength());
	for (int i = 0; i < str.GetLength(); i++)
	{
		if (str[i] == _T('\\') && i + 1 < str.GetLength())
		{
			TCHAR n = str[i + 1];
			if (n == _T('n'))       { strOut += _T("\n"); i++; }
			else if (n == _T('t'))  { strOut += _T("\t"); i++; }
			else if (n == _T('\\')) { strOut += _T("\\"); i++; }
			else strOut += _T('\\');
		}
		else
			strOut += str[i];
	}
}

inline void SplitByChar(const CString& str, TCHAR chSep, CStringArray& arr)
{
	arr.RemoveAll();
	int nPos = 0;
	for (;;)
	{
		int nAt = str.Find(chSep, nPos);
		if (nAt < 0)
		{
			arr.Add(str.Mid(nPos));
			break;
		}
		arr.Add(str.Mid(nPos, nAt - nPos));
		nPos = nAt + 1;
	}
}

// 读 score.dat（文件不存在返回空数组）
inline BOOL Load(CArray<Rec, Rec&>& arr)
{
	arr.RemoveAll();
	CString strAll;
	if (!ReadTextUtf8(DataPath(), strAll))
		return FALSE;

	int nPos = 0;
	BOOL bFirstLine = TRUE;
	while (nPos <= strAll.GetLength())
	{
		int nEnd = strAll.Find(_T('\n'), nPos);
		CString strLine;
		if (nEnd < 0) { strLine = strAll.Mid(nPos); nPos = strAll.GetLength() + 1; }
		else          { strLine = strAll.Mid(nPos, nEnd - nPos); nPos = nEnd + 1; }
		strLine.TrimRight(_T('\r'));
		if (strLine.IsEmpty())
			continue;
		if (bFirstLine)
		{
			bFirstLine = FALSE;
			if (strLine == SCORETAB_MAGIC)
				continue;                    // 魔数行
		}
		CStringArray arrF;
		SplitByChar(strLine, _T('\t'), arrF);
		if (arrF.GetSize() < 6)
			continue;
		Rec r;
		r.nID    = _ttoi(arrF[0]);
		r.nScore = _ttoi(arrF[2]);
		r.nTotal = _ttoi(arrF[3]);
		UnescapeField(arrF[1], r.strTime);
		UnescapeField(arrF[4], r.strTitle);
		UnescapeField(arrF[5], r.strAnswer);
		if (r.nID <= 0)
			continue;
		arr.Add(r);
	}
	return TRUE;
}

// 插入/覆盖：同一题号只留一条（新的覆盖旧的），并保持按题号升序
// 注意：r 按值传 —— CArray 的 ARG_TYPE 是 Rec&，InsertAt 收的是非 const 引用
inline int Upsert(CArray<Rec, Rec&>& arr, Rec r)
{
	int n = (int)arr.GetSize();
	int i = 0;
	while (i < n && arr[i].nID < r.nID)
		i++;
	if (i < n && arr[i].nID == r.nID)
	{
		arr[i] = r;
		return i;
	}
	arr.InsertAt(i, r);
	return i;
}

inline BOOL Save(const CArray<Rec, Rec&>& arr)
{
	CString strAll;
	strAll.Preallocate(4096);
	strAll = SCORETAB_MAGIC;
	strAll += _T("\r\n");
	for (int i = 0; i < arr.GetSize(); i++)
	{
		const Rec& r = arr[i];
		CString strTime, strTitle, strAnswer, strAns;
		EscapeField(r.strTime, strTime);
		EscapeField(r.strTitle, strTitle);
		strAns = r.strAnswer;
		if (strAns.GetLength() > SCORETAB_MAX_ANSWER)
			strAns = strAns.Left(SCORETAB_MAX_ANSWER);
		EscapeField(strAns, strAnswer);
		CString strLine;
		strLine.Format(_T("%d\t%s\t%d\t%d\t%s\t%s\r\n"),
			r.nID, (LPCTSTR)strTime, r.nScore, r.nTotal,
			(LPCTSTR)strTitle, (LPCTSTR)strAnswer);
		strAll += strLine;
	}
	return WriteTextUtf8(DataPath(), strAll, TRUE);
}

// 生成 xlsx（实现在下面第五节；这里先声明，迁移完数据要立刻生成一份）
inline BOOL ExportXlsx(const CArray<Rec, Rec&>& arr, const CString& strPath);

// ======================= 三、旧版 score.txt 一次性迁移 =======================

// 老版本把成绩追加写在 score.txt 里（CSV：时间,题号,题目,得分,总分,答案），
// 这里把它导进 score.dat。★ 只有"每一行都解析成功"才删旧文件：
// 只要有一行没吃透，就原样留着那个 txt —— 宁可留个碍眼的文件，也绝不丢数据。
inline void MigrateLegacyTxt()
{
	CString strTxt = LegacyTxtPath();
	if (GetFileAttributes(strTxt) == INVALID_FILE_ATTRIBUTES)
		return;
	if (GetFileAttributes(DataPath()) != INVALID_FILE_ATTRIBUTES)
		return;   // 新数据已经在用了，不再重复导入

	CString strAll;
	if (!ReadTextUtf8(strTxt, strAll))
		return;

	CArray<Rec, Rec&> arr;
	int nPos = 0, nLines = 0, nOk = 0;
	while (nPos <= strAll.GetLength())
	{
		int nEnd = strAll.Find(_T('\n'), nPos);
		CString strLine;
		if (nEnd < 0) { strLine = strAll.Mid(nPos); nPos = strAll.GetLength() + 1; }
		else          { strLine = strAll.Mid(nPos, nEnd - nPos); nPos = nEnd + 1; }
		strLine.TrimRight(_T('\r'));
		if (strLine.IsEmpty())
			continue;
		nLines++;

		if (nLines == 1 && strLine.Left(2) == _T("时间"))
		{
			nOk++;            // 表头行，正常
			continue;
		}

		CStringArray arrF;
		SplitByChar(strLine, _T(','), arrF);
		if (arrF.GetSize() < 6)
			continue;         // 解析不了，nOk 不加 → 最后不会删旧文件

		Rec r;
		r.nID = _ttoi(arrF[1]);
		if (r.nID <= 0)
			continue;
		r.nScore = _ttoi(arrF[3]);
		r.nTotal = _ttoi(arrF[4]);
		r.strTime = arrF[0];
		r.strTitle = arrF[2];
		CString strAns = arrF[5];
		for (int k = 6; k < arrF.GetSize(); k++)
		{
			strAns += _T(",");
			strAns += arrF[k];
		}
		strAns.Replace(_T("\\n"), _T("\n"));   // 旧格式把换行存成了字面量 \n，还原回来
		r.strAnswer = strAns;
		Upsert(arr, r);       // 同一题后出现的覆盖先出现的 —— 正好就是"只留最近一次"
		nOk++;
	}

	if (nOk == 0 || nOk != nLines)
		return;               // 有行没解析成功，保守起见不动旧文件

	if (!Save(arr))
		return;
	ExportXlsx(arr, XlsxPath());
	SetFileAttributes(strTxt, FILE_ATTRIBUTE_NORMAL);
	DeleteFile(strTxt);
}

// ======================= 四、极简 ZIP（Store 不压缩，够 xlsx 用）=======================

struct ZipEntry
{
	CStringA strName;
	CStringA strData;
};

inline unsigned long ScoreCrc32(const BYTE* pData, int nLen)
{
	static unsigned long s_tab[256];
	static BOOL s_bInit = FALSE;
	if (!s_bInit)
	{
		for (unsigned long i = 0; i < 256; i++)
		{
			unsigned long c = i;
			for (int k = 0; k < 8; k++)
				c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
			s_tab[i] = c;
		}
		s_bInit = TRUE;
	}
	unsigned long c = 0xFFFFFFFFUL;
	for (int i = 0; i < nLen; i++)
		c = s_tab[(c ^ pData[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFUL;
}

inline void ZipPutU16(CByteArray& buf, unsigned short v)
{
	buf.Add((BYTE)(v & 0xFF));
	buf.Add((BYTE)((v >> 8) & 0xFF));
}

inline void ZipPutU32(CByteArray& buf, unsigned long v)
{
	for (int i = 0; i < 4; i++)
		buf.Add((BYTE)((v >> (8 * i)) & 0xFF));
}

inline void ZipPutBytes(CByteArray& buf, const void* pData, int nLen)
{
	if (nLen <= 0)
		return;
	int nOld = (int)buf.GetSize();
	buf.SetSize(nOld + nLen);
	memcpy(buf.GetData() + nOld, pData, (size_t)nLen);
}

inline CStringA WideToUtf8(const CString& str)
{
	CStringA strOut;
	int nLen = WideCharToMultiByte(CP_UTF8, 0, str, str.GetLength(), nullptr, 0, nullptr, nullptr);
	if (nLen > 0)
	{
		WideCharToMultiByte(CP_UTF8, 0, str, str.GetLength(), strOut.GetBuffer(nLen), nLen, nullptr, nullptr);
		strOut.ReleaseBuffer(nLen);
	}
	return strOut;
}

inline BOOL WriteZipFile(const CString& strPath, CArray<ZipEntry, ZipEntry&>& arrEntries)
{
	if (arrEntries.GetSize() <= 0)
		return FALSE;

	SYSTEMTIME st;
	GetLocalTime(&st);
	unsigned short uTime = (unsigned short)((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
	unsigned short uDate = (unsigned short)(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);

	CByteArray buf;
	buf.SetSize(0, 64 * 1024);          // 一次多要一点，避免反复搬内存
	CArray<unsigned long, unsigned long> arrCrc, arrSize, arrOff;

	// ---- 本地文件头 + 数据 ----
	for (int i = 0; i < arrEntries.GetSize(); i++)
	{
		ZipEntry& e = arrEntries[i];
		unsigned long nSize = (unsigned long)e.strData.GetLength();
		unsigned long nCrc = ScoreCrc32((const BYTE*)(LPCSTR)e.strData, (int)nSize);
		arrCrc.Add(nCrc);
		arrSize.Add(nSize);
		arrOff.Add((unsigned long)buf.GetSize());

		ZipPutU32(buf, 0x04034B50UL);
		ZipPutU16(buf, 20);              // version needed
		ZipPutU16(buf, 0);               // flags（文件名全 ASCII，不用 UTF-8 标志）
		ZipPutU16(buf, 0);               // method = 0（Store）
		ZipPutU16(buf, uTime);
		ZipPutU16(buf, uDate);
		ZipPutU32(buf, nCrc);
		ZipPutU32(buf, nSize);           // compressed size
		ZipPutU32(buf, nSize);           // uncompressed size
		ZipPutU16(buf, (unsigned short)e.strName.GetLength());
		ZipPutU16(buf, 0);               // extra len
		ZipPutBytes(buf, (LPCSTR)e.strName, e.strName.GetLength());
		ZipPutBytes(buf, (LPCSTR)e.strData, (int)nSize);
	}

	// ---- 中央目录 ----
	unsigned long nCdStart = (unsigned long)buf.GetSize();
	for (int i = 0; i < arrEntries.GetSize(); i++)
	{
		ZipEntry& e = arrEntries[i];
		ZipPutU32(buf, 0x02014B50UL);
		ZipPutU16(buf, 20);              // version made by
		ZipPutU16(buf, 20);              // version needed
		ZipPutU16(buf, 0);
		ZipPutU16(buf, 0);
		ZipPutU16(buf, uTime);
		ZipPutU16(buf, uDate);
		ZipPutU32(buf, arrCrc[i]);
		ZipPutU32(buf, arrSize[i]);
		ZipPutU32(buf, arrSize[i]);
		ZipPutU16(buf, (unsigned short)e.strName.GetLength());
		ZipPutU16(buf, 0);               // extra
		ZipPutU16(buf, 0);               // comment
		ZipPutU16(buf, 0);               // disk number start
		ZipPutU16(buf, 0);               // internal attrs
		ZipPutU32(buf, 0);               // external attrs
		ZipPutU32(buf, arrOff[i]);
		ZipPutBytes(buf, (LPCSTR)e.strName, e.strName.GetLength());
	}
	unsigned long nCdSize = (unsigned long)buf.GetSize() - nCdStart;

	// ---- 中央目录结束记录 ----
	ZipPutU32(buf, 0x06054B50UL);
	ZipPutU16(buf, 0);
	ZipPutU16(buf, 0);
	ZipPutU16(buf, (unsigned short)arrEntries.GetSize());
	ZipPutU16(buf, (unsigned short)arrEntries.GetSize());
	ZipPutU32(buf, nCdSize);
	ZipPutU32(buf, nCdStart);
	ZipPutU16(buf, 0);                   // comment len

	CFile f;
	if (!f.Open(strPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
		return FALSE;
	f.Write(buf.GetData(), (UINT)buf.GetSize());
	f.Close();
	return TRUE;
}

// ======================= 五、生成 xlsx =======================

// 单元格样式下标（要和 BuildStylesXml() 里的 cellXfs 顺序一一对应）
enum
{
	XF_DEF    = 0,   // 默认
	XF_HEAD   = 1,   // 表头：绿底白字加粗
	XF_CENTER = 2,   // 居中
	XF_LEFT   = 3,   // 左对齐 + 自动换行
	XF_FULL   = 4,   // 得分：满分（绿）
	XF_MID    = 5,   // 得分：已过大半（橙）
	XF_LOW    = 6,   // 得分：偏低（红）
	XF_RATE   = 7,   // 得分率（百分比格式）
	XF_SUM    = 8,   // 合计行
};

inline CString ColName(int nCol)   // 1 → A
{
	CString str;
	while (nCol > 0)
	{
		int r = (nCol - 1) % 26;
		str.Insert(0, (TCHAR)(_T('A') + r));
		nCol = (nCol - 1) / 26;
	}
	return str;
}

inline void XmlEscape(LPCTSTR psz, CString& strOut)
{
	strOut.Empty();
	int nLen = (psz != nullptr) ? (int)_tcslen(psz) : 0;
	strOut.Preallocate(nLen + 16);
	for (int i = 0; i < nLen; i++)
	{
		TCHAR c = psz[i];
		switch (c)
		{
		case _T('&'):  strOut += _T("&amp;"); break;
		case _T('<'):  strOut += _T("&lt;"); break;
		case _T('>'):  strOut += _T("&gt;"); break;
		case _T('\"'): strOut += _T("&quot;"); break;
		case _T('\''): strOut += _T("&apos;"); break;
		case _T('\t'): strOut += _T("&#9;"); break;
		case _T('\n'): strOut += _T("&#10;"); break;
		case _T('\r'): break;
		default:
			if (c >= 0x20)
				strOut += c;             // 其它控制字符直接丢掉（XML 不合法）
			break;
		}
	}
}

inline CString CellStr(int nRow, int nCol, int nStyle, const CString& strText)
{
	CString strVal, strCell;
	XmlEscape(strText, strVal);
	strCell.Format(_T("<c r=\"%s%d\" s=\"%d\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%s</t></is></c>"),
		(LPCTSTR)ColName(nCol), nRow, nStyle, (LPCTSTR)strVal);
	return strCell;
}

inline CString CellInt(int nRow, int nCol, int nStyle, int nVal)
{
	CString strCell;
	strCell.Format(_T("<c r=\"%s%d\" s=\"%d\"><v>%d</v></c>"),
		(LPCTSTR)ColName(nCol), nRow, nStyle, nVal);
	return strCell;
}

// 百分比写成"手工拼的十进制字符串"，避开 %f 在不同区域设置下小数点变逗号的坑
inline CString RatioStr(int nScore, int nTotal)
{
	if (nTotal <= 0)
		return _T("0");
	int nWhole = nScore / nTotal;
	int nFrac = (int)(((double)(nScore % nTotal) / (double)nTotal) * 10000.0 + 0.5);
	if (nFrac >= 10000) { nWhole++; nFrac = 0; }
	CString str;
	str.Format(_T("%d.%04d"), nWhole, nFrac);
	return str;
}

inline CString CellRatio(int nRow, int nCol, int nStyle, int nScore, int nTotal)
{
	CString strCell;
	strCell.Format(_T("<c r=\"%s%d\" s=\"%d\"><v>%s</v></c>"),
		(LPCTSTR)ColName(nCol), nRow, nStyle, (LPCTSTR)RatioStr(nScore, nTotal));
	return strCell;
}

inline CString BlockOfStyle(int nRow, int nCol, int nStyle)   // 空单元格（只为带上样式）
{
	CString strCell;
	strCell.Format(_T("<c r=\"%s%d\" s=\"%d\"/>"), (LPCTSTR)ColName(nCol), nRow, nStyle);
	return strCell;
}

inline int ScoreStyle(int nScore, int nTotal)
{
	if (nTotal > 0 && nScore >= nTotal)
		return XF_FULL;
	if (nTotal > 0 && nScore * 10 >= nTotal * 6)
		return XF_MID;
	return XF_LOW;
}

inline CString BuildStylesXml()
{
	CString s;
	s += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	s += _T("<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
	s += _T("<numFmts count=\"1\"><numFmt numFmtId=\"164\" formatCode=\"0%\"/></numFmts>");
	s += _T("<fonts count=\"5\">");
	s += _T("<font><sz val=\"11\"/><name val=\"Microsoft YaHei\"/><charset val=\"134\"/></font>");
	s += _T("<font><b/><sz val=\"11\"/><color rgb=\"FFFFFFFF\"/><name val=\"Microsoft YaHei\"/><charset val=\"134\"/></font>");
	s += _T("<font><b/><sz val=\"11\"/><color rgb=\"FF1E7B45\"/><name val=\"Microsoft YaHei\"/><charset val=\"134\"/></font>");
	s += _T("<font><b/><sz val=\"11\"/><color rgb=\"FFB25E00\"/><name val=\"Microsoft YaHei\"/><charset val=\"134\"/></font>");
	s += _T("<font><b/><sz val=\"11\"/><color rgb=\"FFC00000\"/><name val=\"Microsoft YaHei\"/><charset val=\"134\"/></font>");
	s += _T("</fonts>");
	s += _T("<fills count=\"6\">");
	s += _T("<fill><patternFill patternType=\"none\"/></fill>");
	s += _T("<fill><patternFill patternType=\"gray125\"/></fill>");
	s += _T("<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF217346\"/><bgColor indexed=\"64\"/></patternFill></fill>");
	s += _T("<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFE4F3E8\"/><bgColor indexed=\"64\"/></patternFill></fill>");
	s += _T("<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFFFF3DA\"/><bgColor indexed=\"64\"/></patternFill></fill>");
	s += _T("<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFFDECEA\"/><bgColor indexed=\"64\"/></patternFill></fill>");
	s += _T("</fills>");
	s += _T("<borders count=\"2\">");
	s += _T("<border><left/><right/><top/><bottom/><diagonal/></border>");
	s += _T("<border><left style=\"thin\"><color rgb=\"FFBFD8C9\"/></left><right style=\"thin\"><color rgb=\"FFBFD8C9\"/></right>");
	s += _T("<top style=\"thin\"><color rgb=\"FFBFD8C9\"/></top><bottom style=\"thin\"><color rgb=\"FFBFD8C9\"/></bottom><diagonal/></border>");
	s += _T("</borders>");
	s += _T("<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>");
	s += _T("<cellXfs count=\"9\">");
	s += _T("<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>");                                     // 0 默认
	s += _T("<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\" wrapText=\"1\"/></xf>");  // 1 表头
	s += _T("<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>");       // 2 居中
	s += _T("<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"left\" vertical=\"center\" wrapText=\"1\"/></xf>");  // 3 左对齐换行
	s += _T("<xf numFmtId=\"0\" fontId=\"2\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>");  // 4 满分
	s += _T("<xf numFmtId=\"0\" fontId=\"3\" fillId=\"4\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>");  // 5 中等
	s += _T("<xf numFmtId=\"0\" fontId=\"4\" fillId=\"5\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>");  // 6 偏低
	s += _T("<xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>");  // 7 得分率
	s += _T("<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>");  // 8 合计
	s += _T("</cellXfs>");
	s += _T("<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>");
	s += _T("</styleSheet>");
	return s;
}

// 冻结首行（表头一直看得见）
inline CString BuildSheetViews()
{
	CString s;
	s += _T("<sheetViews><sheetView workbookViewId=\"0\">");
	s += _T("<pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" state=\"frozen\"/>");
	s += _T("<selection pane=\"bottomLeft\" activeCell=\"A2\" sqref=\"A2\"/>");
	s += _T("</sheetView></sheetViews>");
	return s;
}

// 往 zip 里加一个部件（名字和内容都转成 UTF-8 字节）
inline void AddZipPart(CArray<ZipEntry, ZipEntry&>& arr, LPCTSTR pszName, LPCTSTR pszXml)
{
	ZipEntry e;
	e.strName = WideToUtf8(pszName);
	e.strData = WideToUtf8(pszXml);
	arr.Add(e);
}

// 生成 成绩表.xlsx（两个工作表：成绩总览 / 作答详情）
inline BOOL ExportXlsx(const CArray<Rec, Rec&>& arr, const CString& strPath)
{
	const int nData = (int)arr.GetSize();

	// ---------------- 工作表 1：成绩总览 ----------------
	CString s1;
	s1.Preallocate(8192);
	s1 += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	s1 += _T("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
	s1 += _T("<sheetPr><tabColor rgb=\"FF217346\"/></sheetPr>");   // 表签也走绿
	int nLast1 = (nData > 0) ? (nData + 1) : 2;
	{
		CString strDim;
		strDim.Format(_T("<dimension ref=\"A1:F%d\"/>"), nLast1);
		s1 += strDim;
	}
	s1 += BuildSheetViews();
	s1 += _T("<sheetFormatPr defaultRowHeight=\"16.5\"/>");
	s1 += _T("<cols>");
	s1 += _T("<col min=\"1\" max=\"1\" width=\"8\" customWidth=\"1\"/>");
	s1 += _T("<col min=\"2\" max=\"2\" width=\"56\" customWidth=\"1\"/>");
	s1 += _T("<col min=\"3\" max=\"3\" width=\"8\" customWidth=\"1\"/>");
	s1 += _T("<col min=\"4\" max=\"4\" width=\"8\" customWidth=\"1\"/>");
	s1 += _T("<col min=\"5\" max=\"5\" width=\"10\" customWidth=\"1\"/>");
	s1 += _T("<col min=\"6\" max=\"6\" width=\"22\" customWidth=\"1\"/>");
	s1 += _T("</cols>");
	s1 += _T("<sheetData>");

	{
		CString strRow = _T("<row r=\"1\" ht=\"24\" customHeight=\"1\">");
		const TCHAR* pHdr[] = { _T("题号"), _T("题目"), _T("得分"), _T("总分"), _T("得分率"), _T("最近作答") };
		for (int c = 0; c < 6; c++)
			strRow += CellStr(1, c + 1, XF_HEAD, pHdr[c]);
		strRow += _T("</row>");
		s1 += strRow;
	}

	int nSumScore = 0, nSumTotal = 0;
	for (int i = 0; i < nData; i++)
	{
		const Rec& r = arr[i];
		int nRow = i + 2;
		nSumScore += r.nScore;
		nSumTotal += r.nTotal;

		CString strRow;
		strRow.Format(_T("<row r=\"%d\">"), nRow);
		strRow += CellInt(nRow, 1, XF_CENTER, r.nID);
		strRow += CellStr(nRow, 2, XF_LEFT, r.strTitle);
		strRow += CellInt(nRow, 3, ScoreStyle(r.nScore, r.nTotal), r.nScore);
		strRow += CellInt(nRow, 4, XF_CENTER, r.nTotal);
		strRow += CellRatio(nRow, 5, XF_RATE, r.nScore, r.nTotal);
		strRow += CellStr(nRow, 6, XF_CENTER, r.strTime);
		strRow += _T("</row>");
		s1 += strRow;
	}

	if (nData == 0)
	{
		CString strRow = _T("<row r=\"2\">");
		strRow += CellStr(2, 1, XF_LEFT, _T("还没有成绩记录 —— 去练一道题，点了【提交判题】这里就会有"));
		strRow += _T("</row>");
		s1 += strRow;
	}
	else
	{
		int nRow = nData + 2;
		CString strRow;
		strRow.Format(_T("<row r=\"%d\" ht=\"22\" customHeight=\"1\">"), nRow);
		strRow += CellStr(nRow, 1, XF_SUM, _T("合计"));
		CString strInfo;
		strInfo.Format(_T("共练 %d 题（同一题只记最近一次）"), nData);
		strRow += CellStr(nRow, 2, XF_SUM, strInfo);
		strRow += CellInt(nRow, 3, XF_SUM, nSumScore);
		strRow += CellInt(nRow, 4, XF_SUM, nSumTotal);
		int nPct = (nSumTotal > 0) ? (int)((double)nSumScore * 100.0 / (double)nSumTotal + 0.5) : 0;
		CString strPct;
		strPct.Format(_T("%d%%"), nPct);
		strRow += CellStr(nRow, 5, XF_SUM, strPct);
		strRow += BlockOfStyle(nRow, 6, XF_SUM);
		strRow += _T("</row>");
		s1 += strRow;
	}
	s1 += _T("</sheetData>");
	if (nData > 0)
	{
		CString strFilter;
		strFilter.Format(_T("<autoFilter ref=\"A1:F%d\"/>"), nData + 1);
		s1 += strFilter;
	}
	s1 += _T("</worksheet>");

	// ---------------- 工作表 2：作答详情 ----------------
	CString s2;
	s2.Preallocate(8192);
	s2 += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	s2 += _T("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
	s2 += _T("<sheetPr><tabColor rgb=\"FF217346\"/></sheetPr>");   // 表签也走绿
	{
		CString strDim;
		strDim.Format(_T("<dimension ref=\"A1:C%d\"/>"), (nData > 0) ? (nData + 1) : 2);
		s2 += strDim;
	}
	s2 += BuildSheetViews();
	s2 += _T("<sheetFormatPr defaultRowHeight=\"16.5\"/>");
	s2 += _T("<cols>");
	s2 += _T("<col min=\"1\" max=\"1\" width=\"8\" customWidth=\"1\"/>");
	s2 += _T("<col min=\"2\" max=\"2\" width=\"40\" customWidth=\"1\"/>");
	s2 += _T("<col min=\"3\" max=\"3\" width=\"80\" customWidth=\"1\"/>");
	s2 += _T("</cols>");
	s2 += _T("<sheetData>");
	{
		CString strRow = _T("<row r=\"1\" ht=\"24\" customHeight=\"1\">");
		const TCHAR* pHdr[] = { _T("题号"), _T("题目"), _T("我的答案（最近一次）") };
		for (int c = 0; c < 3; c++)
			strRow += CellStr(1, c + 1, XF_HEAD, pHdr[c]);
		strRow += _T("</row>");
		s2 += strRow;
	}
	for (int i = 0; i < nData; i++)
	{
		const Rec& r = arr[i];
		int nRow = i + 2;
		CString strAns = r.strAnswer;
		if (strAns.GetLength() > XLSX_MAX_CELL_CHARS)
		{
			CString strTail;
			strTail.Format(_T("\r\n…（答案太长，超过 Excel 单元格字数上限，这里显示前 %d 个字符；完整内容见 score.dat）"),
				XLSX_MAX_CELL_CHARS);
			strAns = strAns.Left(XLSX_MAX_CELL_CHARS) + strTail;
		}
		CString strRow;
		strRow.Format(_T("<row r=\"%d\">"), nRow);
		strRow += CellInt(nRow, 1, XF_CENTER, r.nID);
		strRow += CellStr(nRow, 2, XF_LEFT, r.strTitle);
		strRow += CellStr(nRow, 3, XF_LEFT, strAns);
		strRow += _T("</row>");
		s2 += strRow;
	}
	if (nData == 0)
	{
		CString strRow = _T("<row r=\"2\">");
		strRow += CellStr(2, 1, XF_LEFT, _T("还没有作答记录"));
		strRow += _T("</row>");
		s2 += strRow;
	}
	s2 += _T("</sheetData>");
	s2 += _T("</worksheet>");

	// ---------------- 包内其它固定部分 ----------------
	CString sCT;
	sCT += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	sCT += _T("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">");
	sCT += _T("<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>");
	sCT += _T("<Default Extension=\"xml\" ContentType=\"application/xml\"/>");
	sCT += _T("<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>");
	sCT += _T("<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>");
	sCT += _T("<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>");
	sCT += _T("<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>");
	sCT += _T("</Types>");

	CString sRels;
	sRels += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	sRels += _T("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
	sRels += _T("<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>");
	sRels += _T("</Relationships>");

	CString sWb;
	sWb += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	sWb += _T("<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" ");
	sWb += _T("xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">");
	sWb += _T("<sheets>");
	sWb += _T("<sheet name=\"成绩总览\" sheetId=\"1\" r:id=\"rId1\"/>");
	sWb += _T("<sheet name=\"作答详情\" sheetId=\"2\" r:id=\"rId2\"/>");
	sWb += _T("</sheets></workbook>");

	CString sWbRels;
	sWbRels += _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n");
	sWbRels += _T("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
	sWbRels += _T("<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>");
	sWbRels += _T("<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>");
	sWbRels += _T("<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>");
	sWbRels += _T("</Relationships>");

	CArray<ZipEntry, ZipEntry&> arrZip;
	AddZipPart(arrZip, _T("[Content_Types].xml"),        sCT);
	AddZipPart(arrZip, _T("_rels/.rels"),                sRels);
	AddZipPart(arrZip, _T("xl/workbook.xml"),            sWb);
	AddZipPart(arrZip, _T("xl/_rels/workbook.xml.rels"), sWbRels);
	AddZipPart(arrZip, _T("xl/styles.xml"),              BuildStylesXml());
	AddZipPart(arrZip, _T("xl/worksheets/sheet1.xml"),   s1);
	AddZipPart(arrZip, _T("xl/worksheets/sheet2.xml"),   s2);
	return WriteZipFile(strPath, arrZip);
}

// ======================= 六、对外接口 =======================

// 记一条成绩：同一题只留最近一次，写完立刻重生成 成绩表.xlsx
inline BOOL RecordScore(int nID, const CString& strTitle, int nScore, int nTotal,
	const CString& strAnswer, CString* pstrErr = nullptr)
{
	if (nID <= 0)
		return FALSE;

	MigrateLegacyTxt();

	CArray<Rec, Rec&> arr;
	Load(arr);

	Rec r;
	r.nID = nID;
	r.nScore = nScore;
	r.nTotal = nTotal;
	r.strTime = NowString();
	r.strTitle = strTitle;
	r.strAnswer = strAnswer;
	Upsert(arr, r);

	if (!Save(arr))
	{
		if (pstrErr) *pstrErr = _T("成绩数据文件写入失败：") + DataPath();
		return FALSE;
	}
	if (!ExportXlsx(arr, XlsxPath()))
	{
		if (pstrErr) *pstrErr = _T("成绩表生成失败：") + XlsxPath();
		return FALSE;
	}
	return TRUE;
}

// 只按 score.dat 重新生成 xlsx（主界面点【查看成绩表】时用），返回记录条数
inline int RebuildXlsx(int* pnCount = nullptr, CString* pstrErr = nullptr)
{
	MigrateLegacyTxt();

	CArray<Rec, Rec&> arr;
	Load(arr);
	if (pnCount) *pnCount = (int)arr.GetSize();

	if (arr.GetSize() > 0 && !ExportXlsx(arr, XlsxPath()))
	{
		if (pstrErr) *pstrErr = _T("成绩表生成失败：") + XlsxPath();
	}
	return (int)arr.GetSize();
}

}   // namespace ScoreTable
