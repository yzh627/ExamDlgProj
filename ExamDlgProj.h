#pragma once
#include "resource.h"

class CExamDlgProjApp : public CWinApp
{
public:
	CExamDlgProjApp();

// 重写
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 实现
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()

public:
	// 进程退出码：正常关闭固定 0；命令行自检 /verify 返回失败题数。
	// MFC 默认的 CWinApp::ExitInstance() 返回的是"上一次消息泵的 wParam"，
	// 正常关窗时 DoModal 的 IDCANCEL(2) 会被带成退出码，批处理 if errorlevel 1 会误判失败。
	int m_nExitCode;
};

extern CExamDlgProjApp theApp;
