#include "pch.h"
#include "framework.h"
#include "ExamDlgProj.h"
#include "ExamDlgProjDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CExamDlgProjApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CExamDlgProjApp::CExamDlgProjApp()
{
	m_nExitCode = 0;
}

CExamDlgProjApp theApp;

// ===== 打包题库：ExamDlgProj.exe /pack =====
// 把 exe 目录下的 questions.txt（明文，开发/加题用）加密成 questions.dat（发布用）
// 学生机上只放 questions.dat，打开也是乱码，看不到用例和标准答案
static int PackQuestionBank()
{
	CString strDir = GetExeDir();
	CString strTxt = strDir + _T("questions.txt");
	CString strDat = strDir + _T("questions.dat");

	CFile fIn;
	if (!fIn.Open(strTxt, CFile::modeRead))
	{
		_tprintf(_T("找不到明文题库：%s\r\n"), (LPCTSTR)strTxt);
		return 1;
	}
	ULONGLONG n64 = fIn.GetLength();
	if (n64 == 0 || n64 > 4 * 1024 * 1024)
	{
		fIn.Close();
		_tprintf(_T("题库文件大小异常（%llu 字节），已停止。\r\n"), n64);
		return 1;
	}
	int nLen = (int)n64;
	BYTE* pBuf = new BYTE[nLen];
	fIn.Read(pBuf, nLen);
	fIn.Close();

	int nOff = 0;
	if (nLen >= 3 && pBuf[0] == 0xEF && pBuf[1] == 0xBB && pBuf[2] == 0xBF)
		nOff = 3; // 去掉UTF-8 BOM

	int nBody = nLen - nOff;
	BYTE* pBody = new BYTE[nBody];
	memcpy(pBody, pBuf + nOff, nBody);
	BankXorCrypt(pBody, nBody);

	CFile fOut;
	if (!fOut.Open(strDat, CFile::modeCreate | CFile::modeWrite))
	{
		delete[] pBuf;
		delete[] pBody;
		_tprintf(_T("无法写入加密题库：%s\r\n"), (LPCTSTR)strDat);
		return 1;
	}
	fOut.Write(BANK_MAGIC, 8);
	fOut.Write(pBody, nBody);
	fOut.Close();

	delete[] pBuf;
	delete[] pBody;
	_tprintf(_T("加密题库已生成：%s\r\n（明文 %d 字节 → 加密 %d 字节）\r\n"),
		(LPCTSTR)strDat, nLen, nBody + 8);
	return 0;
}

// 退出码统一收口：正常关闭 0，自检失败给非 0（详见 ExamDlgProj.h 里 m_nExitCode 的说明）
int CExamDlgProjApp::ExitInstance()
{
	CWinApp::ExitInstance();   // 先让 MFC 正常收尾（保存设置、释放全局量）
	return m_nExitCode;
}

BOOL CExamDlgProjApp::InitInstance()
{
	// 初始化RichEdit控件（必须，否则使用RichEdit的对话框会崩溃）
	AfxInitRichEdit2();

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	// 标准初始化
	SetRegistryKey(_T("考试练习系统"));

	// ===== 命令行自检模式：ExamDlgProj.exe /verify =====
	// 不打开界面，直接编译全部标准答案并跑用例，结果打印到控制台并写入 verify_report.txt
	CString strCmd = m_lpCmdLine;
	strCmd.MakeLower();
	if (strCmd.Find(_T("/verify")) >= 0 || strCmd.Find(_T("--verify")) >= 0)
	{
		if (AttachConsole(ATTACH_PARENT_PROCESS))
		{
			FILE* fp = nullptr;
			freopen_s(&fp, "CONOUT$", "w", stdout);
			freopen_s(&fp, "CONOUT$", "w", stderr);
		}
		CExamDlgProjDlg dlgVerify;
		int nFailed = dlgVerify.HeadlessVerify();
		// 退出码：0=全部通过；1=题库加载失败；其余=失败题数（上限 255，太大没意义）
		if (nFailed < 0)
			m_nExitCode = 1;
		else if (nFailed > 255)
			m_nExitCode = 255;
		else
			m_nExitCode = nFailed;
		return FALSE; // 自检完直接退出，不进入消息循环
	}

	// ===== 命令行打包模式：ExamDlgProj.exe /pack =====
	if (strCmd.Find(_T("/pack")) >= 0)
	{
		if (AttachConsole(ATTACH_PARENT_PROCESS))
		{
			FILE* fp = nullptr;
			freopen_s(&fp, "CONOUT$", "w", stdout);
			freopen_s(&fp, "CONOUT$", "w", stderr);
		}
		PackQuestionBank();
		return FALSE;
	}

	CExamDlgProjDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  对话框来关闭应用程序的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  对话框来关闭应用程序的代码
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
		m_nExitCode = 3;   // 对话框创建失败才是真异常，批处理应当能察觉
	}

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}
