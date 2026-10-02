#include "pch.h"
#include "framework.h"
#include "PracticeDlg.h"
#include "ExamDlgProjDlg.h"
#include "ScoreTable.h"       // 成绩记录：score.dat（内部数据）+ 成绩表.xlsx（展示）
#include "afxdialogex.h"
#include <shellapi.h>
#include <objbase.h>
#include <commctrl.h>   // SetWindowSubclass：左侧只读框架禁止复制用

IMPLEMENT_DYNAMIC(CPracticeDlg, CDialogEx)

// ================= CScoreDlg 判题结果对话框 =================
IMPLEMENT_DYNAMIC(CScoreDlg, CDialogEx)

CScoreDlg::CScoreDlg(int nScore, int nTotal, const CString& strDetail, CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SCORE_DLG, pParent)
{
	m_nScore = nScore;
	m_nTotal = nTotal;
	m_strDetail = strDetail;
}

CScoreDlg::~CScoreDlg()
{
}

void CScoreDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_SCORE, m_staticScore);
	DDX_Control(pDX, IDC_STATIC_DETAIL, m_staticDetail);
}

BEGIN_MESSAGE_MAP(CScoreDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_BACK_FROM_SCORE, &CScoreDlg::OnBnClickedBtnBackFromScore)
END_MESSAGE_MAP()

BOOL CScoreDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 大号加粗字体显示分数
	m_fontScore.CreatePointFont(160, _T("微软雅黑"));
	m_staticScore.SetFont(&m_fontScore);

	CString str;
	str.Format(_T("本题得分：%d / %d"), m_nScore, m_nTotal);
	m_staticScore.SetWindowText(str);

	// 显示用例明细（检查逻辑）
	m_staticDetail.SetWindowText(m_strDetail);
	return TRUE;
}

// 返回主界面按钮：EndDialog(3)，DoModal返回3（1=继续做题 2=点X关闭 3=返回主界面）
void CScoreDlg::OnBnClickedBtnBackFromScore()
{
	EndDialog(3);
}
// ================= CScoreDlg 结束 =================

// ANSI字节串转Unicode CString（用于读取控制台输出）
static CString A2TStr(const char* pA)
{
	if (pA == nullptr)
		return _T("");
	int nLen = MultiByteToWideChar(CP_ACP, 0, pA, -1, nullptr, 0);
	CString str;
	if (nLen > 1)
	{
		MultiByteToWideChar(CP_ACP, 0, pA, -1, str.GetBuffer(nLen), nLen);
		str.ReleaseBuffer();
	}
	return str;
}

// 构造函数：接收题目参数
CPracticeDlg::CPracticeDlg(CWnd* pParent /*=nullptr*/, QuestionInfo info)
	: CDialogEx(IDD_PRACTICE_DLG, pParent)
{
	m_qInfo = info;
	m_nRemainSec = info.nTimeMin * 60; // 分钟转秒
	m_strOrigFrame = info.strFrameCode;
	m_strSavePath.Empty();
	m_strCsPath.Empty();
	m_strJudgeCsPath.Empty();
	m_strExePath.Empty();
	m_strJudgeDir.Empty();
	m_strProjDir.Empty();
	m_strCsprojPath.Empty();
	m_strProgramCsPath.Empty();
	m_strSlnPath.Empty();
	m_strDraftDir.Empty();
	m_strDraftPath.Empty();
	m_strLastSubmitted.Empty();
	m_nCtrlCount = 0;
	m_bLayoutInit = FALSE;
	m_bJudging = FALSE;

	// 判题线程状态
	m_pJudgeThread = nullptr;
	m_bJudgeCancel = 0;
	m_nJudgeDone = 0;
	m_bJudgeCompileOk = FALSE;
	m_nJudgeScore = 0;
	m_strJudgeDetail.Empty();
	m_strJudgeErr.Empty();
	m_strSubmittedCode.Empty();
	m_bFrameSubclassed = FALSE;
}

CPracticeDlg::~CPracticeDlg()
{
	// 兜底：万一判题线程还在跑就销毁窗口（比如整个程序被关掉），
	// 这里必须先把线程收干净再让对象消失 —— 线程体里还在用 this 的题目信息
	// 和编译目录，对象一没就是野指针。正常情况下走不到这里（返回按钮会先拦住）。
	if (m_pJudgeThread != nullptr)
	{
		m_bJudgeCancel = 1;   // 让它尽快退出（最坏等一个用例的 5 秒超时）
		::WaitForSingleObject(m_pJudgeThread->m_hThread, 10000);
		delete m_pJudgeThread;
		m_pJudgeThread = nullptr;
	}
}

void CPracticeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RICH_FRAME, m_richFrame);
	DDX_Control(pDX, IDC_RICH_CODE, m_richCode);
	DDX_Control(pDX, IDC_STATIC_TIMER, m_staticTimer);
}

BEGIN_MESSAGE_MAP(CPracticeDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SUBMIT, &CPracticeDlg::OnBnClickedBtnSubmit)
	ON_BN_CLICKED(IDC_BTN_SAVE, &CPracticeDlg::OnBnClickedBtnSave)
	ON_BN_CLICKED(IDC_BTN_RESET, &CPracticeDlg::OnBnClickedBtnReset)
	ON_BN_CLICKED(IDC_BTN_RESTORE, &CPracticeDlg::OnBnClickedBtnRestore)
	ON_BN_CLICKED(IDC_BTN_DEBUG, &CPracticeDlg::OnBnClickedBtnDebug)
	ON_BN_CLICKED(IDC_BTN_BACK, &CPracticeDlg::OnBnClickedBtnBack)
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_EN_SETFOCUS(IDC_RICH_FRAME, &CPracticeDlg::OnEnSetfocusRichFrame)
	ON_MESSAGE(WM_JUDGE_PROGRESS, &CPracticeDlg::OnJudgeProgress)
	ON_MESSAGE(WM_JUDGE_DONE, &CPracticeDlg::OnJudgeDone)
	ON_MESSAGE(WM_COLLAPSE_FRAME_SEL, &CPracticeDlg::OnCollapseFrameSel)
END_MESSAGE_MAP()

// 初始化文件路径
// 保存目录的规则统一放在 ScoreTable::ResolveSaveDir() 里（主界面的【查看成绩表】
// 也用同一个函数），保证做题界面和主界面算出来的路径永远一致。
void CPracticeDlg::InitSavePath()
{
	CString strDir = ScoreTable::ResolveSaveDir();

	// ===== 纯临时文件（判题用）固定放 %LOCALAPPDATA% =====
	// 判题要写一份 .cs 再编出一个 .exe，这类"编译产物"放在程序目录里既容易被杀软盯上，
	// 装在 C:\Program Files 时又可能触发 UAC 相关的怪问题。放到用户自己的
	// LocalAppData 里不需要任何提权，程序目录只留用户看得见的成绩和草稿。
	CString strTempRoot;
	TCHAR szLocal[MAX_PATH] = { 0 };
	if (GetEnvironmentVariable(_T("LOCALAPPDATA"), szLocal, MAX_PATH) > 0)
	{
		strTempRoot.Format(_T("%s\\对口升学练习系统\\"), szLocal);
		CreateDirectory(strTempRoot, nullptr);   // 先建父目录，再建 temp 子目录
		strTempRoot += _T("temp\\");
	}
	else
	{
		strTempRoot = strDir;   // 极端情况（取不到 LOCALAPPDATA）退回原目录，保证功能可用
	}
	CreateDirectory(strTempRoot, nullptr);

	m_strSavePath = strDir + _T("answer.txt");       // 纯答案（用户可见，退出时清理）
	m_strCsPath = strDir + _T("answer.cs");          // 完整代码（供VS打开）
	// 成绩记录：数据在 score.dat（见 ScoreTable.h），展示用的 成绩表.xlsx 由它生成
	m_strJudgeCsPath = strTempRoot + _T("answer_judge.cs"); // 判题用代码（临时）
	m_strExePath = strTempRoot + _T("answer_judge.exe");    // 判题编译结果（临时）
	m_strJudgeDir = strTempRoot + _T("judge_tmp\\");        // 判题运行时隔离目录

	// VS调试工程目录：和保存目录同级。必须在这里就算出来——CleanupTempFiles() 在
	// OnInitDialog() 里就会被调用，那时若还没赋值，上一轮留下的 exam\ 目录永远清不掉。
	m_strProjDir = strDir + _T("exam\\");

	// 草稿：每道题一个文件，程序退出也不删（CleanupTempFiles 只清答案/判题临时文件）；
	// 唯一会清掉本题草稿的时机是"提交判分成功"（见 OnJudgeDone）
	m_strDraftDir = strDir + _T("draft\\");
	CreateDirectory(m_strDraftDir, nullptr);
	CString strNo;
	strNo.Format(_T("%d"), m_qInfo.nID);
	m_strDraftPath = m_strDraftDir + strNo + _T(".txt");
}

// ===== 递归删除目录（含只读文件） =====
void CPracticeDlg::DeleteDirRecursive(const CString& strDir)
{
	if (strDir.IsEmpty())
		return;
	if (GetFileAttributes(strDir) == INVALID_FILE_ATTRIBUTES)
		return;

	CString strPattern = strDir + _T("*");
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(strPattern, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			CString strName = fd.cFileName;
			if (strName == _T(".") || strName == _T(".."))
				continue;
			CString strFull = strDir + strName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				DeleteDirRecursive(strFull + _T("\\"));
				RemoveDirectory(strFull);
			}
			else
			{
				SetFileAttributes(strFull, FILE_ATTRIBUTE_NORMAL); // 去掉只读属性
				DeleteFile(strFull);
			}
		} while (FindNextFile(hFind, &fd));
		FindClose(hFind);
	}
	RemoveDirectory(strDir);
}

// ===== 清理判题/保存产生的临时文件（保留questions.txt和score.txt） =====
void CPracticeDlg::CleanupTempFiles()
{
	DeleteFile(m_strSavePath);     // answer.txt
	DeleteFile(m_strCsPath);       // answer.cs
	DeleteFile(m_strJudgeCsPath);  // answer_judge.cs
	DeleteFile(m_strExePath);      // answer_judge.exe
	DeleteDirRecursive(m_strJudgeDir); // judge_tmp 目录
	DeleteDirRecursive(m_strProjDir);  // exam 目录
}

BOOL CPracticeDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置窗口标题
	SetWindowText(_T("河北对口升学计算机程序设计练习系统"));

	// 右上角题目描述（题目→示例→注意）
	SetDlgItemText(IDC_STATIC_QUESTION, m_qInfo.strContent);

	// 左侧只读代码框架
	m_richFrame.SetWindowText(m_qInfo.strFrameCode);
	m_richFrame.SetReadOnly(TRUE); // 只读，不能改

	// 美化代码区：浅灰背景
	m_richFrame.SetBackgroundColor(FALSE, RGB(250, 250, 250));

	// 高亮 begin/end 标记（蓝色粗体，像IDE一样醒目）
	HighlightMarker(_T("/************begin************/"));
	HighlightMarker(_T("/************end************/"));
	m_richFrame.SetSel(0, 0); // 取消选中

	// 右侧答题区域
	m_richCode.SetWindowText(m_qInfo.strCode);

	// 设置代码字体（等宽字体）
	// 字体必须是成员变量：原来用局部 CFont + Detach() 相当于"把 HFONT 甩给控件"，
	// 而控件并不负责释放，每打开一道题就漏一个 GDI 对象，做几十道题后句柄会吃紧。
	m_fontCode.CreatePointFont(100, _T("Consolas"));
	m_richFrame.SetFont(&m_fontCode);
	m_richCode.SetFont(&m_fontCode);

	// 初始化文件路径
	InitSavePath();

	// 载入本题草稿（上次退出/离开时自动保存的答案）
	LoadDraft();

	// 清理上次做题遗留的临时文件（answer*.cs/exe/txt、exam\、judge_tmp\）
	CleanupTempFiles();

	// 采集控件初始布局（用于窗口拉伸时动态缩放）
	InitLayout();

	// 启动倒计时定时器：每1000毫秒（1秒）触发一次
	SetTimer(1, 1000, nullptr);

	// 初始显示倒计时
	UpdateTimerText();

	// 焦点直接给答题区：学生一进来就能开始写。
	// 顺带这也是"左侧代码被整段自动选中"的修法——原先左侧只读框架在 .rc 里带 WS_TABSTOP
	// 且排在答题区前面，系统会把焦点给它，而 RichEdit 首次获得焦点会全选自己全部文本，
	// 于是 OnInitDialog 里那句 SetSel(0,0) 被覆盖，表现为整段代码蓝色高亮。
	m_richCode.SetFocus();

	// 左侧只读框架：禁止复制（拦截剪贴板消息 / 快捷键 / 右键菜单 / 拖拽）
	if (::IsWindow(m_richFrame.GetSafeHwnd()))
		m_bFrameSubclassed = SetWindowSubclass(m_richFrame.GetSafeHwnd(),
			s_FrameSubclassProc, 1, 0) ? TRUE : FALSE;

	return TRUE;
}

// ===== 采集控件初始布局 =====
void CPracticeDlg::InitLayout()
{
	GetClientRect(&m_rcInit);

	struct TmpCtrl { UINT nID; BOOL bFixedY; };
	TmpCtrl tmp[] = {
		{ IDC_STATIC_TIMER,     TRUE  }, // 顶部固定
		{ IDC_GROUP_LEFT,       FALSE },
		{ IDC_RICH_FRAME,       FALSE },
		{ IDC_GROUP_RIGHT,      FALSE },
		{ IDC_STATIC_QUESTION,  FALSE },
		{ IDC_STATIC_LABEL,     FALSE },
		{ IDC_RICH_CODE,        FALSE },
		{ IDC_BTN_SAVE,         FALSE },
		{ IDC_BTN_RESET,        FALSE },
		{ IDC_BTN_RESTORE,      FALSE },
		{ IDC_BTN_DEBUG,        FALSE },
		{ IDC_BTN_SUBMIT,       FALSE },
		{ IDC_BTN_BACK,         FALSE },
	};

	m_nCtrlCount = 0;
	for (int i = 0; i < (int)(sizeof(tmp) / sizeof(tmp[0])); i++)
	{
		CWnd* pWnd = GetDlgItem(tmp[i].nID);
		if (pWnd == nullptr)
			continue;

		CRect rc;
		pWnd->GetWindowRect(&rc);
		ScreenToClient(&rc);

		m_ctrlRects[m_nCtrlCount].nID = tmp[i].nID;
		m_ctrlRects[m_nCtrlCount].x = rc.left;
		m_ctrlRects[m_nCtrlCount].y = rc.top;
		m_ctrlRects[m_nCtrlCount].w = rc.Width();
		m_ctrlRects[m_nCtrlCount].h = rc.Height();
		m_ctrlRects[m_nCtrlCount].bFixedY = tmp[i].bFixedY;
		m_nCtrlCount++;
	}

	m_bLayoutInit = TRUE;
}

// ===== 窗口拉伸时动态缩放所有控件 =====
void CPracticeDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (!m_bLayoutInit || cx <= 0 || cy <= 0)
		return;

	double fx = (double)cx / (double)m_rcInit.Width();
	double fy = (double)cy / (double)m_rcInit.Height();

	for (int i = 0; i < m_nCtrlCount; i++)
	{
		CWnd* pWnd = GetDlgItem(m_ctrlRects[i].nID);
		if (pWnd == nullptr)
			continue;

		int x = (int)(m_ctrlRects[i].x * fx);
		int y = (int)(m_ctrlRects[i].y * fy);
		int w = (int)(m_ctrlRects[i].w * fx);
		int h = (int)(m_ctrlRects[i].h * fy);

		// 顶部固定控件：纵向不缩放，防止文字变形
		if (m_ctrlRects[i].bFixedY)
		{
			y = m_ctrlRects[i].y;
			h = m_ctrlRects[i].h;
		}

		pWnd->MoveWindow(x, y, w, h);
	}

	Invalidate();
}

// 高亮begin/end标记：蓝色粗体
void CPracticeDlg::HighlightMarker(LPCTSTR lpszMarker)
{
	CString strText;
	m_richFrame.GetWindowText(strText);

	int nPos = 0;
	while (true)
	{
		int nFind = strText.Find(lpszMarker, nPos);
		if (nFind < 0)
			break;

		long nStart = nFind;
		long nEnd = nFind + (long)_tcslen(lpszMarker);

		m_richFrame.SetSel(nStart, nEnd);
		CHARFORMAT2 cf;
		ZeroMemory(&cf, sizeof(cf));
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_COLOR | CFM_BOLD;
		cf.dwEffects = CFE_BOLD;
		cf.crTextColor = RGB(0, 102, 204); // 蓝色
		m_richFrame.SetSelectionCharFormat(cf);

		nPos = nEnd;
	}
}

// 左侧只读框架获得焦点时，RichEdit 的默认行为是"选中自己全部文本"并整段蓝底高亮，
// 学生看着像这块能改、以为要在这里写代码。
// 这里把选择折叠到开头。护栏放在这里而不是只放 OnInitDialog：
// 只写 OnInitDialog 是不够的——SetSel(0,0) 执行时焦点还没分配，系统随后把焦点给
// 第一个 Tab 控件，RichEdit 一拿到焦点又全选，把前面的清选动作覆盖掉。
// （顺带把左侧框架的 WS_TABSTOP 从 .rc 里去掉了，让焦点默认落在答题区。）
void CPracticeDlg::OnEnSetfocusRichFrame()
{
	// 用 Post 而不是直接调：等本轮焦点处理（含 RichEdit 内部的全选）走完再折叠，
	// 否则可能被后面的全选动作盖掉。
	PostMessage(WM_COLLAPSE_FRAME_SEL, 0, 0);
}

// 把左侧框架的选择折叠到开头（真正干活的）
LRESULT CPracticeDlg::OnCollapseFrameSel(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if (::IsWindow(m_richFrame.GetSafeHwnd()))
		m_richFrame.SetSel(0, 0);
	return 0;
}

// ===== 左侧只读框架：禁止复制 =====
// 左侧是题目给的固定代码框架，学生要写的是右侧答题区，它不该被复制走。
// 用子类化来拦，比只挂 ES_READONLY 彻底 —— ES_READONLY 挡得住"改"，挡不住"复制"：
//   · WM_COPY / WM_CUT / WM_CLEAR / WM_PASTE：菜单项和快捷键最终都走这几条消息，直接吞掉；
//   · WM_CONTEXTMENU：右键菜单里就有"复制"，一并吞掉；
//   · 键盘消息一律不回给控件（只放行 Esc / 回车 / Tab，免得焦点万一落在框架上关不掉窗口）：
//     没人需要跟一段只读的参考代码用键盘交互，所以干脆整条键盘通道关掉 ——
//     Ctrl+C/X/V/A、Shift+方向键选字、Insert……全都无从下手；
//     也不必去猜 Ctrl/Shift 到底按没按（GetKeyState 在合成输入下并不可靠，
//     靠它做判断的护栏很容易形同虚设）；
//   · 按住左键拖动时吞掉 WM_MOUSEMOVE：防止把选中的文字拖到别的程序里去。
// 注意：只挂在 m_richFrame 上，右侧答题区的复制/粘贴完全不受影响。
LRESULT CALLBACK CPracticeDlg::s_FrameSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
	switch (uMsg)
	{
	case WM_COPY:
	case WM_CUT:
	case WM_CLEAR:
	case WM_PASTE:
		return 0;                       // 剪贴板操作：一律不理

	case WM_CONTEXTMENU:
		return 0;                       // 不给右键菜单

	case WM_MOUSEMOVE:
		if ((wParam & MK_LBUTTON) != 0)
			return 0;                   // 正按着左键拖：不转发，文字就拖不出去
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		// 放行 Esc / 回车 / Tab，保住对话框的常规操作
		if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_TAB)
			break;
		return 0;

	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
		return 0;

	case WM_NCDESTROY:
		// 控件销毁前必须把子类化摘掉，否则回调会留在已经释放的窗口上
		RemoveWindowSubclass(hWnd, s_FrameSubclassProc, uIdSubclass);
		break;
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// 更新倒计时显示文字（HH:MM:SS格式）
void CPracticeDlg::UpdateTimerText()
{
	int nHour = m_nRemainSec / 3600;
	int nMin = (m_nRemainSec % 3600) / 60;
	int nSec = m_nRemainSec % 60;
	CString str;
	str.Format(_T("系统剩余时间: %02d:%02d:%02d"), nHour, nMin, nSec);
	m_staticTimer.SetWindowText(str);
}

// 定时器：倒计时
void CPracticeDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		// 判题中/成绩窗口打开时不倒计时，避免"交卷时间到"把还开着成绩窗口的父窗口销毁
		if (m_bJudging)
		{
			CDialogEx::OnTimer(nIDEvent);
			return;
		}

		m_nRemainSec--;
		if (m_nRemainSec <= 0)
		{
			m_nRemainSec = 0;
			UpdateTimerText();
			KillTimer(1);
			// 时间到：先存草稿，再自动提交判题并记成绩（不让学生白做）
			SaveDraft();
			{
				CString strAns = GetAnswerText();
				strAns.Trim();
				if (strAns.IsEmpty())
				{
					// 一个字都没写，没有可判的内容，直接回主界面
					// （否则窗口会卡在"计时器已停、窗口还在"的死状态里）
					MessageBox(_T("考试时间到！\r\n\r\n答题区是空的，本次不判分，将返回主界面。"),
						_T("时间到"), MB_OK | MB_ICONINFORMATION);
					DoLeavePractice(FALSE);
					return;
				}
			}
			MessageBox(_T("考试时间到！\r\n\r\n系统将自动提交你的答案并判分。"),
				_T("时间到"), MB_OK | MB_ICONINFORMATION);
			OnBnClickedBtnSubmit(); // 走正常判题流程：判分 + 写成绩记录 + 弹得分窗口
			return;
		}
		UpdateTimerText();
	}

	CDialogEx::OnTimer(nIDEvent);
}

// 获取答题区内容
CString CPracticeDlg::GetAnswerText()
{
	CString str;
	m_richCode.GetWindowText(str);
	return str;
}

// 把答案插入框架的begin/end之间，组成完整代码
CString CPracticeDlg::ComposeFullCode(const CString& answer)
{
	CString frame = m_qInfo.strFrameCode;
	CString beginMark = _T("/************begin************/");
	CString endMark = _T("/************end************/");
	int nBegin = frame.Find(beginMark);
	int nEnd = frame.Find(endMark);
	if (nBegin < 0 || nEnd < 0 || nEnd < nBegin)
		return frame;

	int nStart = nBegin + beginMark.GetLength();
	CString full = frame.Left(nStart) + _T("\r\n") + answer + frame.Mid(nEnd);
	return full;
}

// ===== 判题/自检通用：统一换行 + 去掉每行首尾空白后再比较 =====
// 避免学生程序多输出一个换行、行尾多个空格就被判错
static CString NormalizeJudgeText(const CString& str)
{
	CString s = str;
	s.Replace(_T("\r\n"), _T("\n"));
	s.Replace(_T("\r"), _T("\n"));

	CString strOut, strLine;
	int nPos = 0;
	while (nPos <= s.GetLength())
	{
		int nFind = s.Find(_T('\n'), nPos);
		if (nFind < 0)
		{
			strLine = s.Mid(nPos);
			strLine.Trim();
			if (!strLine.IsEmpty())
			{
				if (!strOut.IsEmpty())
					strOut += _T("\n");
				strOut += strLine;
			}
			break;
		}
		strLine = s.Mid(nPos, nFind - nPos);
		strLine.Trim();
		if (!strLine.IsEmpty())
		{
			if (!strOut.IsEmpty())
				strOut += _T("\n");
			strOut += strLine;
		}
		nPos = nFind + 1;
	}
	return strOut;
}

// ===== 去掉注释和字符串/字符常量，只保留真正的代码 =====
// 原来直接在整个答案里搜关键字，代码/注释里出现"Delete"之类的字眼会误判
static CString StripCommentsAndLiterals(const CString& src)
{
	CString out;
	int n = src.GetLength();
	int i = 0;
	while (i < n)
	{
		TCHAR c = src[i];

		// 单行注释
		if (c == _T('/') && i + 1 < n && src[i + 1] == _T('/'))
		{
			while (i < n && src[i] != _T('\n'))
				i++;
			continue;
		}
		// 块注释
		if (c == _T('/') && i + 1 < n && src[i + 1] == _T('*'))
		{
			i += 2;
			while (i + 1 < n && !(src[i] == _T('*') && src[i + 1] == _T('/')))
				i++;
			i += 2;
			continue;
		}
		// 字符串常量
		if (c == _T('"'))
		{
			i++;
			while (i < n && src[i] != _T('"'))
			{
				if (src[i] == _T('\\') && i + 1 < n)
					i++;
				i++;
			}
			i++;
			out += _T("\"\"");
			continue;
		}
		// 字符常量
		if (c == _T('\''))
		{
			i++;
			while (i < n && src[i] != _T('\''))
			{
				if (src[i] == _T('\\') && i + 1 < n)
					i++;
				i++;
			}
			i++;
			out += _T("''");
			continue;
		}

		out += c;
		i++;
	}
	return out;
}

// ===== 按"单词"匹配关键字，避免 ProcessData、Substring 这类词被误判 =====
static BOOL ContainsWord(const CString& code, LPCTSTR lpszWord)
{
	int nWord = (int)_tcslen(lpszWord);
	if (nWord <= 0)
		return FALSE;

	int nPos = 0;
	while (true)
	{
		int nFind = code.Find(lpszWord, nPos);
		if (nFind < 0)
			return FALSE;

		BOOL bLeft = (nFind == 0) || !(_istalnum(code[nFind - 1]) || code[nFind - 1] == _T('_'));
		int nEnd = nFind + nWord;
		BOOL bRight = (nEnd >= code.GetLength()) || !(_istalnum(code[nEnd]) || code[nEnd] == _T('_'));
		if (bLeft && bRight)
			return TRUE;

		nPos = nFind + 1;
	}
}

// ===== 括号是否配对 =====
static BOOL IsBalanced(const CString& code, TCHAR cOpen, TCHAR cClose)
{
	int n = 0;
	for (int i = 0; i < code.GetLength(); i++)
	{
		if (code[i] == cOpen)
			n++;
		else if (code[i] == cClose)
		{
			n--;
			if (n < 0)
				return FALSE;
		}
	}
	return (n == 0);
}

// ===== 只去掉注释（保留字符串内容），反硬编码检查用 =====
static CString StripCommentsOnly(const CString& src)
{
	CString out;
	int n = src.GetLength();
	int i = 0;
	while (i < n)
	{
		TCHAR c = src[i];
		if (c == _T('/') && i + 1 < n && src[i + 1] == _T('/'))
		{
			while (i < n && src[i] != _T('\n'))
				i++;
			continue;
		}
		if (c == _T('/') && i + 1 < n && src[i + 1] == _T('*'))
		{
			i += 2;
			while (i + 1 < n && !(src[i] == _T('*') && src[i + 1] == _T('/')))
				i++;
			i += 2;
			continue;
		}
		out += c;
		i++;
	}
	return out;
}

// ===== 反硬编码：答案里直接写死了用例结果就拦下来 =====
// 只检查"有区分度"的期望值（长度≥3），避免把公式里的常量误判成硬编码。
// 实测：题库158道标准答案全部零误判（最多命中1个），写死多组答案的会被抓住。
// ===== 取出"紧跟在 return 后面"的数字字面量 =====
// 为什么只看 return 后面的：写死答案的特征是 return 一个字面量（return 8.00;），
// 而正常答案 return 的是变量或表达式。实测（159 道标准答案）：
//   拿全部数字字面量去比 → 误拦 4 题（标准答案里本来就有 0.50 / 60.0 这类常量）
//   只看 return 后的字面量 → 误拦 0 题，且照样抓住"去掉小数位""整数强转"两种绕过
static void CollectReturnedNumbers(const CString& code, CArray<double, double>& arrNum)
{
	int n = code.GetLength();
	int i = 0;
	while (i < n)
	{
		if ((code[i] == _T('r') || code[i] == _T('R'))
			&& i + 6 <= n
			&& code.Mid(i, 6).CompareNoCase(_T("return")) == 0
			&& (i == 0 || !(_istalnum(code[i - 1]) || code[i - 1] == _T('_'))))
		{
			int k = i + 6;
			while (k < n && (code[k] == _T(' ') || code[k] == _T('\t')))
				k++;

			// 跳过强制类型转换：(int) / (double) / (long) 之类
			if (k < n && code[k] == _T('('))
			{
				int nClose = code.Find(_T(')'), k);
				if (nClose > k)
				{
					CString strCast = code.Mid(k + 1, nClose - k - 1);
					strCast.Trim();
					BOOL bIsCast = !strCast.IsEmpty();
					for (int t = 0; bIsCast && t < strCast.GetLength(); t++)
					{
						TCHAR c = strCast[t];
						if (!(_istalnum(c) || c == _T('_') || c == _T('[')
							|| c == _T(']') || c == _T('.')))
							bIsCast = FALSE;
					}
					if (bIsCast)
					{
						k = nClose + 1;
						while (k < n && (code[k] == _T(' ') || code[k] == _T('\t')))
							k++;
					}
				}
			}

			if (k < n && code[k] == _T('+'))
				k++;
			if (k < n && code[k] == _T('('))   // 形如 return (8.00);
				k++;
			while (k < n && (code[k] == _T(' ') || code[k] == _T('\t')))
				k++;

			// 数字字面量
			int nStart = k;
			BOOL bDot = FALSE;
			while (k < n && (_istdigit(code[k]) || (!bDot && code[k] == _T('.'))))
			{
				if (code[k] == _T('.'))
					bDot = TRUE;
				k++;
			}
			if (k > nStart)
			{
				// 科学计数法
				if (k < n && (code[k] == _T('e') || code[k] == _T('E')))
				{
					int nSave = k;
					int e = k + 1;
					if (e < n && (code[e] == _T('+') || code[e] == _T('-')))
						e++;
					if (e < n && _istdigit(code[e]))
					{
						while (e < n && _istdigit(code[e]))
							e++;
						k = e;
					}
					else
						k = nSave;
				}
				CString strNum = code.Mid(nStart, k - nStart);
				arrNum.Add(_tcstod(strNum, nullptr));
			}
			i = (k > i) ? k : i + 1;
			continue;
		}
		i++;
	}
}

// 期望值是否"整串就是一个数字"
static BOOL TryParseWholeNumber(const CString& str, double& dOut)
{
	CString s = str;
	s.Trim();
	if (s.IsEmpty())
		return FALSE;
	LPTSTR pEnd = nullptr;
	double d = _tcstod(s, &pEnd);
	if (pEnd == nullptr || *pEnd != _T('\0') || pEnd == (LPCTSTR)s)
		return FALSE;
	dOut = d;
	return TRUE;
}

// ===== 反硬编码：答案里直接写死了用例结果就拦下来 =====
// 只检查"有区分度"的期望值（长度≥3），避免把公式里的常量误判成硬编码。
// 两道防线：
//   ① 字符串精确匹配（原逻辑，覆盖非数值期望值）
//   ② 数值比对：期望值是数字时，和代码里 return 后面的数字字面量按数值比较
//      —— 这样 "8.00 的答案在代码里写成 8" 也能扫到（实测可拦住该绕过）
// 阈值仍然是"命中≥2 才拦"，保证正常答案不被误伤。
static int FindHardcodedOutputs(const CString& code, const CStringArray& arrExpected, CStringArray& arrHit)
{
	CStringArray arrKeys;
	for (int i = 0; i < arrExpected.GetSize(); i++)
	{
		CString strExp = arrExpected.GetAt(i);
		strExp.Trim();
		if (strExp.GetLength() < 3)
			continue;
		BOOL bDup = FALSE;
		for (int j = 0; j < arrKeys.GetSize(); j++)
			if (arrKeys.GetAt(j) == strExp)
				bDup = TRUE;
		if (!bDup)
			arrKeys.Add(strExp);
	}

	// 代码里 return 后面的数字字面量（数值比对用）
	CArray<double, double> arrRetNum;
	CollectReturnedNumbers(code, arrRetNum);

	for (int k = 0; k < arrKeys.GetSize(); k++)
	{
		CString strKey = arrKeys.GetAt(k);
		BOOL bHit = FALSE;

		// ① 字符串精确匹配
		int nPos = 0;
		while (!bHit)
		{
			int nFind = code.Find(strKey, nPos);
			if (nFind < 0)
				break;

			BOOL bLeft = (nFind == 0)
				|| !(_istalnum(code[nFind - 1]) || code[nFind - 1] == _T('_') || code[nFind - 1] == _T('.'));
			int nEnd = nFind + strKey.GetLength();
			BOOL bRight = (nEnd >= code.GetLength())
				|| !(_istalnum(code[nEnd]) || code[nEnd] == _T('_') || code[nEnd] == _T('.'));
			if (bLeft && bRight)
			{
				bHit = TRUE;
				break;
			}
			nPos = nFind + 1;
		}

		// ② 数值比对（只对 |期望值| >= 1 的做，避免 0.00 这类太泛的数字误伤）
		if (!bHit && arrRetNum.GetSize() > 0)
		{
			double dExp = 0.0;
			if (TryParseWholeNumber(strKey, dExp) && (dExp >= 1.0 || dExp <= -1.0))
			{
				for (int m = 0; m < arrRetNum.GetSize(); m++)
				{
					double dCode = arrRetNum.GetAt(m);
					double dDiff = dCode - dExp;
					if (dDiff < 0)
						dDiff = -dDiff;
					double dScale = (dExp < 0) ? -dExp : dExp;
					if (dDiff <= 1e-9 * dScale)
					{
						bHit = TRUE;
						break;
					}
				}
			}
		}

		if (bHit)
			arrHit.Add(strKey);
	}
	return (int)arrHit.GetSize();
}

// ===== 检查学生代码逻辑（危险操作/框架完整性/常见错误） =====
BOOL CPracticeDlg::CheckAnswerLogic(const CString& answer, CString& errMsg)
{
	// 1. 检查答案非空
	CString strAnswer = answer;
	if (strAnswer.Trim().IsEmpty())
	{
		errMsg = _T("答题区域为空！请在右侧答题区填写函数的方法体（代码框架已在左侧，无需重复书写）。");
		return FALSE;
	}

	// 2. 检查 begin/end 标记是否被破坏（框架完整性）
	CString strFrame = m_qInfo.strFrameCode;
	if (strFrame.Find(_T("/************begin************/")) < 0
		|| strFrame.Find(_T("/************end************/")) < 0)
	{
		errMsg = _T("代码框架异常（begin/end标记丢失）！请点击【重置】恢复原始框架。");
		return FALSE;
	}

	// 3. 危险操作检查：先剔除注释和字符串，再匹配（避免误判）
	CString strCode = StripCommentsAndLiterals(answer);

	if (strCode.Find(_T("/************begin************/")) >= 0
		|| strCode.Find(_T("/************end************/")) >= 0)
	{
		errMsg = _T("答题区里不需要写 begin/end 标记，只填写方法体代码即可（代码框架已在左侧，程序会自动拼接）。");
		return FALSE;
	}

	// 危险接口（带命名空间或带点号的写法，直接整串匹配）
	static const LPCTSTR arrDanger[] = {
		_T("System.IO"), _T("System.Diagnostics"), _T("System.Net"), _T("System.Threading"),
		_T("System.Reflection"), _T("File."), _T("Directory."), _T("Registry"),
		_T("WebClient"), _T("Socket"), _T("Http"), _T("ShellExecute"), _T("Clipboard"),
		_T("Environment.Exit"), _T("DeleteFile"), _T("RemoveDirectory"),
		_T("powershell"), _T("cmd.exe"), _T("DllImport"), _T("Marshal.")
	};
	for (int i = 0; i < (int)(sizeof(arrDanger) / sizeof(arrDanger[0])); i++)
	{
		if (strCode.Find(arrDanger[i]) >= 0)
		{
			errMsg.Format(_T("检测到不允许使用的代码：%s\r\n\r\n请只填写题目的算法逻辑，不要使用文件、网络、进程等系统操作。"),
				arrDanger[i]);
			return FALSE;
		}
	}

	// 危险标识符（按单词匹配，Substring、ProcessData 这类正常写法不会被误判）
	static const LPCTSTR arrDangerWord[] = {
		_T("Process"), _T("ProcessStartInfo"), _T("Thread"), _T("Registry"),
		_T("WebClient"), _T("Socket"), _T("Sleep"), _T("Environment")
	};
	for (int i = 0; i < (int)(sizeof(arrDangerWord) / sizeof(arrDangerWord[0])); i++)
	{
		if (ContainsWord(strCode, arrDangerWord[i]))
		{
			errMsg.Format(_T("检测到不允许使用的代码：%s\r\n\r\n判题只考察算法逻辑，请不要使用系统相关的类。"),
				arrDangerWord[i]);
			return FALSE;
		}
	}

	// 4. 常见错误提前提示（比直接抛编译错误更友好）
	if (strCode.Find(_T("Console.Read")) >= 0)
	{
		errMsg = _T("请不要在方法里再读取键盘输入！\r\n\r\n题目的输入由主函数 Main 负责读取，方法只需要处理参数并 return 结果。");
		return FALSE;
	}

	if (!IsBalanced(strCode, _T('{'), _T('}')))
	{
		errMsg = _T("花括号 { } 不配对，请检查是否少写或多写了 }。");
		return FALSE;
	}

	if (!IsBalanced(strCode, _T('('), _T(')')))
	{
		errMsg = _T("小括号 ( ) 不配对，请检查代码。");
		return FALSE;
	}

	// 5. 需要返回值的方法必须有 return，否则直接提示（省得等编译报错）
	if (m_qInfo.strSignature.Find(_T("void")) < 0 && strCode.Find(_T("return")) < 0)
	{
		errMsg = _T("这道题的方法需要返回结果，但代码里没有 return 语句。\r\n\r\n请把计算结果 return 出去（不要只打印到控制台）。");
		return FALSE;
	}

	// 6. 反硬编码：把用例答案直接写死在代码里不算通过
	{
		CString strNoComment = StripCommentsOnly(answer);
		CStringArray arrHit;
		if (FindHardcodedOutputs(strNoComment, m_qInfo.arrExpected, arrHit) >= 2)
		{
			CString strList;
			for (int i = 0; i < arrHit.GetSize(); i++)
			{
				if (i > 0)
					strList += _T("、");
				strList += arrHit.GetAt(i);
			}
			errMsg.Format(_T("检测到答案里直接写死了多组用例结果（%s）。\r\n\r\n")
				_T("判题会用多组输入运行程序，写死某几组答案得不到分；\r\n")
				_T("请改成用参数计算的通用算法（把答案算出来，而不是背出来）。"),
				(LPCTSTR)strList);
			return FALSE;
		}
	}

	return TRUE;
}

// ===== 判题期间处理界面消息（窗口不假死） =====
static void PumpMessages()
{
	MSG msg;
	while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			break;
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
}

// 读ANSI文本文件（和 WriteFileANSI 配对）
// 注意：不能用 CStdioFile 的文本模式读 —— 宽字符构建下 CRT 会把每个字节
//       直接映射成一个 wchar_t，GBK 中文会变成「ÎÒµÄ´ð°¸」这种乱码。
BOOL CPracticeDlg::ReadFileAnsi(const CString& path, CString& strOut)
{
	strOut.Empty();

	CFile f;
	if (!f.Open(path, CFile::modeRead))
		return FALSE;

	ULONGLONG nLen64 = f.GetLength();
	if (nLen64 == 0)
	{
		f.Close();
		return TRUE;
	}
	if (nLen64 > 1024 * 1024)   // 答案文件最大1MB，防异常文件
		nLen64 = 1024 * 1024;

	int nLen = (int)nLen64;
	char* pBuf = new char[nLen + 1];
	f.Read(pBuf, nLen);
	f.Close();
	pBuf[nLen] = 0;

	int nWide = MultiByteToWideChar(CP_ACP, 0, pBuf, nLen, nullptr, 0);
	if (nWide > 0)
	{
		MultiByteToWideChar(CP_ACP, 0, pBuf, nLen, strOut.GetBuffer(nWide), nWide);
		strOut.ReleaseBuffer();
	}
	delete[] pBuf;
	return TRUE;
}

// 写ANSI文本文件（csc按系统代码页读取源码）
BOOL CPracticeDlg::WriteFileANSI(const CString& path, const CString& content)
{
	CFile f;
	if (!f.Open(path, CFile::modeCreate | CFile::modeWrite))
		return FALSE;

	int nLen = WideCharToMultiByte(CP_ACP, 0, content, -1, nullptr, 0, nullptr, nullptr);
	if (nLen > 1)
	{
		char* pBuf = new char[nLen];
		WideCharToMultiByte(CP_ACP, 0, content, -1, pBuf, nLen, nullptr, nullptr);
		f.Write(pBuf, nLen - 1); // 去掉结尾的\0
		delete[] pBuf;
	}
	f.Close();
	return TRUE;
}

// 保存答案：写文件 + 更新左侧代码显示
void CPracticeDlg::SaveAnswerFiles(const CString& answer)
{
	// 1. 保存纯答案 answer.txt
	WriteFileANSI(m_strSavePath, answer);

	// 2. 保存完整代码 answer.cs（框架+答案）
	CString full = ComposeFullCode(answer);
	WriteFileANSI(m_strCsPath, full);

	// 3. 更新左侧代码框架显示（合并后的完整代码）
	m_richFrame.SetWindowText(full);
	m_richFrame.SetSel(0, 0);   // SetWindowText 会把选择整段留下，这里折叠掉，避免残留高亮
}

// ===== 草稿：按题号存一个文件，退出程序也不删，下次打开这道题自动带出来 =====
// 例外：这道题已经提交判分成功过 —— 那时草稿会被清掉，下次打开是干净的空白框架，
//       可以当新题重做（提交的答案已经写进成绩表 成绩表.xlsx，不会丢）
void CPracticeDlg::LoadDraft()
{
	if (m_strDraftPath.IsEmpty())
		return;
	if (GetFileAttributes(m_strDraftPath) == INVALID_FILE_ATTRIBUTES)
		return;

	CString strAll;
	if (!ReadFileAnsi(m_strDraftPath, strAll))
		return;

	if (!strAll.IsEmpty())
	{
		m_richCode.SetWindowText(strAll);
		SetDlgItemText(IDC_STATIC_LABEL, _T("答题区域（已自动恢复上次的答案）"));
	}
}

// 保存本题草稿（保存/离开/时间到时调用）
// 返回 TRUE=真的写了草稿文件；FALSE=没写（答题区为空，或这道题已经提交过判分）
BOOL CPracticeDlg::SaveDraft()
{
	if (m_strDraftPath.IsEmpty())
		return FALSE;

	CString strCode = GetAnswerText();
	CString strCheck = strCode;
	strCheck.Trim();
	if (strCheck.IsEmpty())
	{
		DeleteDraft();   // 答题区是空的就不留草稿文件，免得 draft 目录堆一堆空文件
		return FALSE;
	}

	// 已经提交判分过的答案不再留草稿：提交即视为本题练完，
	// 下次打开应当是干净的空白框架，可以当新题重做。
	// 这里必须挡一道——否则离开做题界面时 DoLeavePractice() 又会把刚清掉的草稿写回来。
	CString strLast = m_strLastSubmitted;
	strLast.Trim();
	if (!strLast.IsEmpty() && strCheck == strLast)
	{
		DeleteDraft();
		return FALSE;
	}

	return WriteFileANSI(m_strDraftPath, strCode);
}

void CPracticeDlg::DeleteDraft()
{
	if (!m_strDraftPath.IsEmpty())
		DeleteFile(m_strDraftPath);
}

// 答题区是否有"还没提交过"的内容（决定返回主界面时要不要弹确认）
BOOL CPracticeDlg::HasUnsavedContent()
{
	CString strNow = GetAnswerText();
	strNow.Trim();
	if (strNow.IsEmpty())
		return FALSE;                  // 空的，没什么可丢，不用打扰

	CString strLast = m_strLastSubmitted;
	strLast.Trim();
	if (!strLast.IsEmpty() && strNow == strLast)
		return FALSE;                  // 和刚提交的一模一样，也没必要再问

	return TRUE;
}

// 写UTF-8 BOM文本文件（Excel可直接打开不乱码）
BOOL CPracticeDlg::WriteFileUtf8Bom(const CString& path, const CString& content)
{
	CFile f;
	if (!f.Open(path, CFile::modeCreate | CFile::modeWrite))
		return FALSE;

	BYTE bom[] = { 0xEF, 0xBB, 0xBF };
	f.Write(bom, 3);

	int nLen = WideCharToMultiByte(CP_UTF8, 0, content, -1, nullptr, 0, nullptr, nullptr);
	if (nLen > 1)
	{
		char* pBuf = new char[nLen];
		WideCharToMultiByte(CP_UTF8, 0, content, -1, pBuf, nLen, nullptr, nullptr);
		f.Write(pBuf, nLen - 1);
		delete[] pBuf;
	}
	f.Close();
	return TRUE;
}

// ===== 写成绩记录 =====
// 数据落在 score.dat：一行一题，同一道题只保留【最近一次】，整体按【题号】升序。
// 每次写完立刻由 score.dat 重新生成一份 成绩表.xlsx（主界面【查看成绩表】看的就是它）。
// 具体实现（含 xlsx 的拼装）都在 ScoreTable.h。
void CPracticeDlg::WriteScoreRecord(const CString& answer, int nScore, int nTotal)
{
	CString strErr;
	if (!ScoreTable::RecordScore(m_qInfo.nID, m_qInfo.strTitle, nScore, nTotal, answer, &strErr))
	{
		// 成绩没写进去不该影响本题的判题结果展示，如实提示一句即可
		AfxMessageBox(_T("成绩记录保存失败：\r\n") + strErr +
			_T("\r\n\r\n本题得分依然有效，只是没能写进成绩表。"),
			MB_OK | MB_ICONWARNING);
	}
}

// 按行拆分字符串
void CPracticeDlg::SplitLines(const CString& str, CStringArray& arr)
{
	CString s = str;
	s.Replace(_T("\r\n"), _T("\n"));
	int pos = 0;
	while (pos < s.GetLength())
	{
		int nPos = s.Find(_T('\n'), pos);
		if (nPos < 0)
		{
			arr.Add(s.Mid(pos));
			break;
		}
		arr.Add(s.Mid(pos, nPos - pos));
		pos = nPos + 1;
	}
}

// 非阻塞地把管道里"已经到达"的数据读出来（先用 PeekNamedPipe 探长度再读）
// 返回值 = 本次读到的字节数
static DWORD DrainPipe(HANDLE hPipe, CStringA& strOut, int nMaxKeep)
{
	DWORD dwTotal = 0;
	for (;;)
	{
		DWORD dwAvail = 0;
		if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &dwAvail, nullptr) || dwAvail == 0)
			break;
		char buf[8192];
		DWORD dwWant = (dwAvail < (DWORD)sizeof(buf)) ? dwAvail : (DWORD)sizeof(buf);
		DWORD dwRead = 0;
		if (!ReadFile(hPipe, buf, dwWant, &dwRead, nullptr) || dwRead == 0)
			break;
		buf[dwRead] = 0;
		if (strOut.GetLength() < nMaxKeep)   // 防止学生程序狂刷输出把内存吃爆
			strOut += buf;
		dwTotal += dwRead;
	}
	return dwTotal;
}

// 运行进程并捕获输出（可写入stdin，可指定工作目录隔离）
// input：写入子进程标准输入的内容（判题时喂样例输入）
BOOL CPracticeDlg::RunProcessWithInput(const CString& exe, const CString& args, const CString& input, CString& output, CString& errMsg, const CString& workDir, DWORD dwTimeoutMs)
{
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE hInRead = nullptr, hInWrite = nullptr, hOutRead = nullptr, hOutWrite = nullptr;
	if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 64 * 1024))
	{
		errMsg = _T("创建输出管道失败");
		return FALSE;
	}
	if (!CreatePipe(&hInRead, &hInWrite, &sa, 64 * 1024))
	{
		CloseHandle(hOutRead);
		CloseHandle(hOutWrite);
		errMsg = _T("创建输入管道失败");
		return FALSE;
	}
	SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = hOutWrite;
	si.hStdError = hOutWrite;   // 错误输出合并到标准输出
	si.hStdInput = hInRead;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	CString cmdLine = _T("\"") + exe + _T("\" ") + args;
	TCHAR* pCmd = cmdLine.GetBuffer(cmdLine.GetLength() + 1);

	// 工作目录：空则继承父进程；非空则使用隔离目录
	LPCTSTR lpszWorkDir = (workDir.IsEmpty()) ? nullptr : (LPCTSTR)workDir;

	BOOL bOK = CreateProcess(nullptr, pCmd, nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr, lpszWorkDir, &si, &pi);
	cmdLine.ReleaseBuffer();

	if (!bOK)
	{
		errMsg = _T("启动进程失败: ") + exe;
		CloseHandle(hInRead); CloseHandle(hInWrite);
		CloseHandle(hOutRead); CloseHandle(hOutWrite);
		return FALSE;
	}

	// 关闭父进程的写端（子进程仍持有，读到EOF靠它）
	CloseHandle(hOutWrite);

	// 写入标准输入（样例输入），然后关闭写端 → 子进程读到EOF
	if (!input.IsEmpty())
	{
		// Unicode→ANSI写入（csc编译的程序按系统代码页读输入）
		int nLen = WideCharToMultiByte(CP_ACP, 0, input, -1, nullptr, 0, nullptr, nullptr);
		if (nLen > 1)
		{
			char* pBuf = new char[nLen];
			WideCharToMultiByte(CP_ACP, 0, input, -1, pBuf, nLen, nullptr, nullptr);
			DWORD dwWritten = 0;
			WriteFile(hInWrite, pBuf, nLen - 1, &dwWritten, nullptr);
			delete[] pBuf;
		}
	}
	CloseHandle(hInWrite);

	// ===== 边等边收 =====
	// 原实现是"先死等进程结束，再读管道"。管道缓冲只有 64KB，
	// 学生程序一旦输出超过这个量就会阻塞在 WriteFile 上永远不退出，
	// 结果明明是能跑对的程序被判成"超时"。改成轮询：一边等进程、一边把输出收走。
	// 顺带在这里响应【取消判题】，不用等满整个超时时间。
	const int nMaxKeep = 4 * 1024 * 1024;
	CStringA strOutA;
	ULONGLONG dwRunStart = GetTickCount64();
	for (;;)
	{
		DWORD dwWait = WaitForSingleObject(pi.hProcess, 20);
		DrainPipe(hOutRead, strOutA, nMaxKeep);

		if (dwWait != WAIT_TIMEOUT)
			break;   // 进程已结束

		if (GetTickCount64() - dwRunStart >= (ULONGLONG)dwTimeoutMs)
		{
			TerminateProcess(pi.hProcess, 1);
			WaitForSingleObject(pi.hProcess, 1000);
			errMsg = _T("运行超时");
			break;
		}
		if (m_bJudgeCancel)
		{
			TerminateProcess(pi.hProcess, 1);
			WaitForSingleObject(pi.hProcess, 1000);
			errMsg = _T("运行已取消");
			break;
		}
	}
	// 收尾：进程已退出/被终止，把管道里剩下的输出也读干净
	while (DrainPipe(hOutRead, strOutA, nMaxKeep) > 0)
		;

	// 清理句柄
	CloseHandle(hInRead);
	CloseHandle(hOutRead);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	output = A2TStr(strOutA);
	return TRUE;
}

// 编译C#代码：调用系统csc.exe编译（黑盒判题：只编译，运行由JudgeByCases逐用例执行）
BOOL CPracticeDlg::CompileAndRun(const CString& csPath, CString& errMsg)
{
	// 1. 查找csc.exe（.NET Framework自带）
	TCHAR szWin[MAX_PATH] = { 0 };
	GetWindowsDirectory(szWin, MAX_PATH);
	CString winDir = szWin;

	CString csc64 = winDir + _T("\\Microsoft.NET\\Framework64\\v4.0.30319\\csc.exe");
	CString csc32 = winDir + _T("\\Microsoft.NET\\Framework\\v4.0.30319\\csc.exe");

	CString cscPath;
	if (GetFileAttributes(csc64) != INVALID_FILE_ATTRIBUTES)
		cscPath = csc64;
	else if (GetFileAttributes(csc32) != INVALID_FILE_ATTRIBUTES)
		cscPath = csc32;
	else
	{
		errMsg = _T("未找到.NET编译环境(csc.exe)，无法判题！");
		return FALSE;
	}

	// 2. 编译前删除旧exe（防止编译失败时误用上一次的判题结果）
	DeleteFile(m_strExePath);

	// 编译（工作目录用exe目录）
	CString args;
	// /langversion:4：和学生的 VS2010（C# 4）保持一致，避免"练习能过、考试机编不过"
	args.Format(_T("/nologo /langversion:4 /out:\"%s\" \"%s\""), (LPCTSTR)m_strExePath, (LPCTSTR)csPath);
	CString compOut;
	if (!RunProcessWithInput(cscPath, args, _T(""), compOut, errMsg, _T(""), 60000))
		return FALSE;

	// 3. 检查exe是否生成成功
	if (GetFileAttributes(m_strExePath) == INVALID_FILE_ATTRIBUTES)
	{
		errMsg = _T("编译失败！\r\n") + compOut; // 编译错误信息
		return FALSE;
	}

	return TRUE;
}

// 黑盒判题：逐用例运行程序，喂标准输入，比对标准输出（每题满分12分，按通过用例比例折算）
// 判题原理：完整程序（框架+学生答案）编译成exe后，像OJ一样用样例输入运行、比对输出，
// 不关心方法名/签名/返回类型——什么代码都能判。
int CPracticeDlg::JudgeByCases(CString& strDetail, CString& errMsg)
{
	// 公开用例 + 隐藏用例一起判，一起折算分数
	int nPublic = (int)m_qInfo.arrInput.GetSize();
	int nHidden = (int)m_qInfo.arrHiddenInput.GetSize();
	int nCases = nPublic + nHidden;
	if (nPublic <= 0 || nPublic != (int)m_qInfo.arrExpected.GetSize()
		|| nHidden != (int)m_qInfo.arrHiddenExpected.GetSize())
	{
		errMsg = _T("题目用例配置错误（用例输入与期望数量不一致）！");
		return -1;
	}

	int nScore = 0;
	strDetail.Empty();
	int nPassed = 0;        // 通过的用例数（满分固定12分，最后按比例折算）
	int nPublicPassed = 0;  // 公开用例通过数（用于识别"写死答案"）

	// 判题在隔离目录运行，防止学生代码乱写文件
	CreateDirectory(m_strJudgeDir, nullptr);

	// 整题判题总时限：11 个用例正常 3~4 秒跑完，20 秒留了 5 倍余量。
	// 原来没有这个限制，死循环最坏要 8×5=40 秒，全程界面无响应。
	const ULONGLONG kJudgeTotalLimitMs = 20000;
	ULONGLONG dwJudgeStart = GetTickCount64();

	for (int i = 0; i < nCases; i++)
	{
		// 先把"正在跑第几个"报上去，再执行；这样界面上的进度是真的在动
		m_nJudgeDone = i;
		::PostMessage(m_hWnd, WM_JUDGE_PROGRESS, (WPARAM)(i + 1), (LPARAM)nCases);

		// 取消 / 整题超时：单用例 5 秒封顶，所以最多等 5 秒就能响应取消
		if (m_bJudgeCancel)
		{
			strDetail += _T("\r\n（已按你的要求中止判题，剩余用例未运行）\r\n");
			break;
		}
		if (GetTickCount64() - dwJudgeStart > kJudgeTotalLimitMs)
		{
			strDetail += _T("\r\n（整题判题超过 20 秒已自动中止，剩余用例按未通过计算）\r\n");
			break;
		}

		BOOL bHidden = (i >= nPublic);
		int nIdx = bHidden ? (i - nPublic) : i;

		// 构造标准输入：用例行内容 + 换行（程序ReadLine即可读到）
		CString strInput = bHidden ? m_qInfo.arrHiddenInput.GetAt(nIdx)
			: m_qInfo.arrInput.GetAt(nIdx);
		strInput.TrimRight();
		strInput += _T("\r\n");

		CString strOutput;
		CString strErr;
		// 单个用例最多跑5秒，防止死循环把判题卡住
		if (!RunProcessWithInput(m_strExePath, _T(""), strInput, strOutput, strErr, m_strJudgeDir, 5000))
		{
			CString strOne;
			strOne.Format(_T("%s%d  ✗ 运行失败：%s\r\n"),
				bHidden ? _T("隐藏用例") : _T("用例"), nIdx + 1, (LPCTSTR)strErr);
			strDetail += strOne;
			continue;
		}

		// 期望与实际都做规范化（统一换行、去掉每行首尾空白）后比较
		CString strExp = NormalizeJudgeText(
			bHidden ? m_qInfo.arrHiddenExpected.GetAt(nIdx) : m_qInfo.arrExpected.GetAt(nIdx));
		CString strActual = NormalizeJudgeText(strOutput);

		BOOL bPass = (strActual == strExp);
		if (bPass)
		{
			nPassed++;
			if (!bHidden)
				nPublicPassed++;
		}

		LPCTSTR lpszState = bPass ? _T("✓ 通过")
			: (strErr.IsEmpty() ? _T("✗ 错误") : _T("✗ 超时（超过5秒，可能是死循环）"));

		CString strOne;
		if (bHidden)
		{
			// 隐藏用例刻意不显示输入与期望值：显示了就等于没藏，
			// 学生照样能把这几组答案写死。只报通过与否。
			strOne.Format(_T("隐藏用例%d  %s\r\n"), nIdx + 1, lpszState);
			strDetail += strOne;
			continue;
		}

		// 把用例输入也显示出来，方便学生对照排查
		CString strIn = m_qInfo.arrInput.GetAt(nIdx);
		strIn.Replace(_T("\r\n"), _T("\n"));
		strIn.Replace(_T("\n"), _T(" → "));

		strOne.Format(_T("用例%d  %s\r\n    输入：%s\r\n    期望：%s\r\n    实际：%s\r\n"),
			nIdx + 1,
			lpszState,
			(LPCTSTR)strIn,
			strExp.IsEmpty() ? _T("(空)") : (LPCTSTR)strExp,
			strActual.IsEmpty() ? _T("(无输出)") : (LPCTSTR)strActual);
		strDetail += strOne;
	}

	// 每题满分固定 12 分：按通过用例比例折算，四舍五入
	nScore = (int)(nPassed * 12.0 / nCases + 0.5);

	CString strSum;
	if (nHidden > 0 && nPublicPassed == nPublic && nPassed < nCases)
	{
		// 公开用例全对、隐藏用例没全对 —— 这正是"照着用例写死答案"的特征
		strSum += _T("\r\n⚠ 公开用例全部通过，但隐藏用例未通过：这说明答案很可能是针对")
			_T("已知用例写死的，而不是按参数计算的通用算法。请改成真正的通用解法。\r\n");
	}
	{
		CString strTail;
		strTail.Format(_T("\r\n得分：%d / 12（共 %d 个用例，按通过比例折算）"), nScore, nCases);
		strSum += strTail;
	}
	strDetail += strSum;

	return nScore;
}

// 保存按钮：保存答案并更新左侧代码
void CPracticeDlg::OnBnClickedBtnSave()
{
	CString strCode = GetAnswerText();
	if (strCode.IsEmpty())
	{
		MessageBox(_T("答题区域为空，请先输入答案！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	SaveAnswerFiles(strCode);
	BOOL bDraftSaved = SaveDraft();   // 同时写入本题草稿（关窗口/时间到都不会丢）
	if (bDraftSaved)
	{
		MessageBox(_T("代码已保存并更新！\r\n\r\n（已存为本题草稿，下次打开这道题会自动带出来）"),
			_T("提示"), MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		// 这道题已经提交判分过了：按设定不再留草稿，下次打开是空白框架
		MessageBox(_T("代码已保存并更新！\r\n\r\n")
			_T("（这道题已经提交过判分，不再保留草稿；下次打开是空白框架，可以重新做一遍）"),
			_T("提示"), MB_OK | MB_ICONINFORMATION);
	}
}

// 重置按钮：恢复左侧原始框架 + 清空答题区
void CPracticeDlg::OnBnClickedBtnReset()
{
	if (MessageBox(_T("确定要重置吗？将恢复原始代码框架并清空答题区域！"),
		_T("确认"), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		m_richFrame.SetWindowText(m_strOrigFrame); // 恢复原始框架
		m_richFrame.SetSel(0, 0);                  // 折叠选择，别留整段高亮
		m_richCode.SetWindowText(_T(""));          // 清空答题区
		DeleteDraft();                             // 本题草稿一并清掉，免得下次又冒出来
	}
}

// 恢复已保存答案按钮：从文件读取答案并恢复到答题区
void CPracticeDlg::OnBnClickedBtnRestore()
{
	// 优先读本题草稿（草稿不会被清理，跨次有效）；老版本留下的 answer.txt 也兼容
	CString strPath = m_strDraftPath;
	if (GetFileAttributes(strPath) == INVALID_FILE_ATTRIBUTES)
		strPath = m_strSavePath;

	CString strAll;
	if (!ReadFileAnsi(strPath, strAll))
	{
		MessageBox(_T("这道题还没有保存过答案！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	if (!strAll.IsEmpty())
	{
		m_richCode.SetWindowText(strAll);
		MessageBox(_T("已恢复上次保存的答案！"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		MessageBox(_T("这道题还没有保存过答案！"), _T("提示"), MB_OK | MB_ICONWARNING);
	}
}

// ===== 生成可运行的VS工程（csproj + Program.cs），供学生在VS里F5运行 =====
BOOL CPracticeDlg::CreateDebugProject(const CString& answer)
{
	// 工程目录：保存目录下的 exam 文件夹（InitSavePath 里已算好，这里只做兜底）
	if (m_strProjDir.IsEmpty())
	{
		m_strProjDir = m_strSavePath;
		int nPos = m_strProjDir.ReverseFind(_T('\\'));
		if (nPos >= 0)
			m_strProjDir = m_strProjDir.Left(nPos + 1);
		m_strProjDir += _T("exam\\");
	}
	CreateDirectory(m_strProjDir, nullptr);

	m_strProgramCsPath = m_strProjDir + _T("Program.cs");
	m_strCsprojPath = m_strProjDir + _T("exam.csproj");
	m_strSlnPath = m_strProjDir + _T("exam.sln");

	// 1. 写入完整代码 Program.cs
	CString full = ComposeFullCode(answer);
	// 在 Main 的最后加一句"按回车退出"，否则在 VS 里按 F5 运行完控制台会一闪而过，看不到结果
	// 注意：只加在给 VS 的调试工程里，判题用的代码不加（判题比对输出，加了会判错）
	{
		CString strEnd = _T("\r\n        }\r\n    }\r\n}");
		int nInsert = -1;
		for (int i = full.GetLength() - strEnd.GetLength(); i >= 0; i--)
		{
			if (full.Mid(i, strEnd.GetLength()) == strEnd)
			{
				nInsert = i;
				break;
			}
		}
		if (nInsert >= 0)
		{
			CString strPause;
			strPause += _T("\r\n");
			strPause += _T("            Console.WriteLine();\r\n");
			strPause += _T("            Console.WriteLine(\"── 程序运行结束，按回车键退出 ──\");\r\n");
			strPause += _T("            Console.ReadLine();");
			full = full.Left(nInsert) + strPause + full.Mid(nInsert);
		}
	}
	if (!WriteFileANSI(m_strProgramCsPath, full))
		return FALSE;

	// 2. 生成随机GUID
	GUID guid;
	CoCreateGuid(&guid);
	CString strGuid;
	strGuid.Format(_T("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	// 3. 生成 csproj（VS2010+兼容的传统格式，.NET 4.0控制台程序）
	CString csproj;
	csproj.Format(_T("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n")
		_T("<Project ToolsVersion=\"4.0\" DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n")
		_T("  <PropertyGroup>\r\n")
		_T("    <Configuration Condition=\" '$(Configuration)' == '' \">Debug</Configuration>\r\n")
		_T("    <Platform Condition=\" '$(Platform)' == '' \">AnyCPU</Platform>\r\n")
		_T("    <ProjectGuid>{%s}</ProjectGuid>\r\n")
		_T("    <OutputType>Exe</OutputType>\r\n")
		_T("    <RootNamespace>prog</RootNamespace>\r\n")
		_T("    <AssemblyName>exam</AssemblyName>\r\n")
		_T("    <TargetFrameworkVersion>v4.0</TargetFrameworkVersion>\r\n")
		_T("    <FileAlignment>512</FileAlignment>\r\n")
		_T("  </PropertyGroup>\r\n")
		_T("  <PropertyGroup Condition=\" '$(Configuration)|$(Platform)' == 'Debug|AnyCPU' \">\r\n")
		_T("    <DebugSymbols>true</DebugSymbols>\r\n")
		_T("    <DebugType>full</DebugType>\r\n")
		_T("    <Optimize>false</Optimize>\r\n")
		_T("    <OutputPath>bin\\Debug\\</OutputPath>\r\n")
		_T("    <DefineConstants>DEBUG;TRACE</DefineConstants>\r\n")
		_T("    <ErrorReport>prompt</ErrorReport>\r\n")
		_T("    <WarningLevel>4</WarningLevel>\r\n")
		_T("  </PropertyGroup>\r\n")
		_T("  <PropertyGroup Condition=\" '$(Configuration)|$(Platform)' == 'Release|AnyCPU' \">\r\n")
		_T("    <DebugType>pdbonly</DebugType>\r\n")
		_T("    <Optimize>true</Optimize>\r\n")
		_T("    <OutputPath>bin\\Release\\</OutputPath>\r\n")
		_T("    <DefineConstants>TRACE</DefineConstants>\r\n")
		_T("    <ErrorReport>prompt</ErrorReport>\r\n")
		_T("    <WarningLevel>4</WarningLevel>\r\n")
		_T("  </PropertyGroup>\r\n")
		_T("  <ItemGroup>\r\n")
		_T("    <Reference Include=\"System\" />\r\n")
		_T("    <Reference Include=\"System.Core\" />\r\n")
		_T("    <Reference Include=\"System.Data\" />\r\n")
		_T("  </ItemGroup>\r\n")
		_T("  <ItemGroup>\r\n")
		_T("    <Compile Include=\"Program.cs\" />\r\n")
		_T("  </ItemGroup>\r\n")
		_T("  <Import Project=\"$(MSBuildToolsPath)\\Microsoft.CSharp.targets\" />\r\n")
		_T("</Project>\r\n"),
		(LPCTSTR)strGuid);

	if (!WriteFileANSI(m_strCsprojPath, csproj))
		return FALSE;

	// 4. 生成 sln（VS2010-2026通用，双击sln即可打开并F5运行）
	CString sln;
	sln.Format(_T("Microsoft Visual Studio Solution File, Format Version 11.00\r\n")
		_T("# Visual Studio 2010\r\n")
		_T("Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"exam\", \"exam.csproj\", \"{%s}\"\r\n")
		_T("EndProject\r\n")
		_T("Global\r\n")
		_T("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n")
		_T("\t\tDebug|Any CPU = Debug|Any CPU\r\n")
		_T("\t\tRelease|Any CPU = Release|Any CPU\r\n")
		_T("\tEndGlobalSection\r\n")
		_T("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n")
		_T("\t\t{%s}.Debug|Any CPU.ActiveCfg = Debug|Any CPU\r\n")
		_T("\t\t{%s}.Debug|Any CPU.Build.0 = Debug|Any CPU\r\n")
		_T("\t\t{%s}.Release|Any CPU.ActiveCfg = Release|Any CPU\r\n")
		_T("\t\t{%s}.Release|Any CPU.Build.0 = Release|Any CPU\r\n")
		_T("\tEndGlobalSection\r\n")
		_T("EndGlobal\r\n"),
		(LPCTSTR)strGuid,
		(LPCTSTR)strGuid, (LPCTSTR)strGuid, (LPCTSTR)strGuid, (LPCTSTR)strGuid);

	return WriteFileANSI(m_strSlnPath, sln);
}

// ===== 查找 Visual Studio（devenv.exe）=====
// 思路：优先 VS2010（多数考场装的就是它）→ 其它旧版标准目录 → VS2017+ 安装目录扫描
//       → 都没有则返回空（由调用方退回 .sln 文件关联并给出提示）
static BOOL PathIsFile(const CString& strPath)
{
	DWORD dwAttr = GetFileAttributes(strPath);
	return (dwAttr != INVALID_FILE_ATTRIBUTES) && !(dwAttr & FILE_ATTRIBUTE_DIRECTORY);
}

// 在 C:\Program Files (x86)\Microsoft Visual Studio\*\*\Common7\IDE\devenv.exe 这类目录里找
static CString ScanNewVsRoot(const CString& strRoot)
{
	CString strPattern = strRoot + _T("*");
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(strPattern, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return _T("");
	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			&& _tcscmp(fd.cFileName, _T(".")) != 0 && _tcscmp(fd.cFileName, _T("..")) != 0)
		{
			// 一级子目录（如 2019 / 2022 / 18），再往下是版本（Community/Professional/Insiders）
			CString strSub = strRoot + fd.cFileName + _T("\\");
			CString strPattern2 = strSub + _T("*");
			WIN32_FIND_DATA fd2;
			HANDLE hFind2 = FindFirstFile(strPattern2, &fd2);
			if (hFind2 != INVALID_HANDLE_VALUE)
			{
				do
				{
					if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						CString strExe = strSub + fd2.cFileName + _T("\\Common7\\IDE\\devenv.exe");
						if (PathIsFile(strExe))
						{
							FindClose(hFind2);
							FindClose(hFind);
							return strExe;
						}
					}
				} while (FindNextFile(hFind2, &fd2));
				FindClose(hFind2);
			}
		}
	} while (FindNextFile(hFind, &fd));
	FindClose(hFind);
	return _T("");
}

static CString FindDevenvPath()
{
	// 1) 注册表 App Paths（VS 安装时一般会写）
	static const HKEY arrRoots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
	static const LPCTSTR arrKeys[] = {
		_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\devenv.exe"),
		_T("SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\devenv.exe")
	};
	for (int r = 0; r < 2; r++)
	{
		for (int k = 0; k < 2; k++)
		{
			TCHAR szPath[MAX_PATH] = { 0 };
			DWORD cbData = sizeof(szPath), dwType = 0;
			if (RegGetValue(arrRoots[r], arrKeys[k], nullptr, RRF_RT_REG_SZ,
				&dwType, szPath, &cbData) == ERROR_SUCCESS)
			{
				CString strPath = szPath;
				strPath.Trim();
				if (PathIsFile(strPath))
					return strPath;
			}
		}
	}

	// 2) 旧版 VS 标准安装位置：VS2010 排在最前（和考场一致）
	static const LPCTSTR arrStd[] = {
		_T("C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files\\Microsoft Visual Studio 10.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files (x86)\\Microsoft Visual Studio 11.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files\\Microsoft Visual Studio 11.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files\\Microsoft Visual Studio 12.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\Common7\\IDE\\devenv.exe"),
		_T("C:\\Program Files\\Microsoft Visual Studio 14.0\\Common7\\IDE\\devenv.exe"),
	};
	for (int i = 0; i < (int)(sizeof(arrStd) / sizeof(arrStd[0])); i++)
	{
		if (PathIsFile(arrStd[i]))
			return arrStd[i];
	}

	// 3) VS2017 及以上：扫描安装目录（如 ...\Microsoft Visual Studio\2022\Community\...）
	CString strFound = ScanNewVsRoot(_T("C:\\Program Files\\Microsoft Visual Studio\\"));
	if (!strFound.IsEmpty())
		return strFound;
	strFound = ScanNewVsRoot(_T("C:\\Program Files (x86)\\Microsoft Visual Studio\\"));
	if (!strFound.IsEmpty())
		return strFound;

	return _T("");
}

// 调用VS调试运行按钮：生成可运行的VS工程并打开，学生可F5直接运行
void CPracticeDlg::OnBnClickedBtnDebug()
{
	CString strCode = GetAnswerText();
	if (strCode.IsEmpty())
	{
		MessageBox(_T("答题区域为空，请先输入答案！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	// 先保存（写answer.cs + 更新左侧）
	SaveAnswerFiles(strCode);

	// 生成可运行的VS工程
	if (!CreateDebugProject(strCode))
	{
		MessageBox(_T("生成VS工程失败！"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	// 用Visual Studio打开解决方案（双击sln，F5即可运行）
	// 先显式查找 VS（优先 VS2010，和考场一致），找到就用它打开；
	// 找不到再用 .sln 的默认关联；再失败就明确提示，避免"点了没反应"
	CString strDevenv = FindDevenvPath();
	BOOL bOpened = FALSE;
	if (!strDevenv.IsEmpty())
	{
		HINSTANCE hRet = ShellExecute(GetSafeHwnd(), _T("open"), strDevenv,
			_T("\"") + m_strSlnPath + _T("\""), nullptr, SW_SHOWNORMAL);
		bOpened = ((INT_PTR)hRet > 32);
	}
	if (!bOpened)
	{
		HINSTANCE hRet = ShellExecute(GetSafeHwnd(), _T("open"), m_strSlnPath, nullptr, nullptr, SW_SHOWNORMAL);
		bOpened = ((INT_PTR)hRet > 32);
	}
	if (!bOpened)
	{
		CString strMsg;
		strMsg.Format(_T("没有检测到 Visual Studio，无法自动打开调试工程。\r\n\r\n")
			_T("工程已经生成好了，你可以手动打开这个文件：\r\n%s\r\n\r\n")
			_T("（如果这台机器没装 Visual Studio，可以把 exam 文件夹拷到装了 VS 的电脑上运行。）"),
			(LPCTSTR)m_strSlnPath);
		MessageBox(strMsg, _T("提示"), MB_OK | MB_ICONINFORMATION);
	}
}

// 提交判题按钮：黑盒判题
// 流程：检查逻辑 → 组装完整程序 → 编译 → 逐用例喂标准输入、比对输出（每用例3分）
void CPracticeDlg::OnBnClickedBtnSubmit()
{
	// 判题进行中再点这个按钮 = 取消判题（按钮文案此时已经是"取消判题"）
	if (m_bJudging)
	{
		RequestCancelJudge();
		return;
	}

	CString strCode = GetAnswerText();

	// 1. 检查逻辑（危险代码、框架完整性、非空、括号/return等常见错误）
	CString errMsg;
	if (!CheckAnswerLogic(strCode, errMsg))
	{
		MessageBox(_T("检查未通过：\r\n\r\n") + errMsg, _T("检查结果"), MB_OK | MB_ICONWARNING);
		return;
	}

	// 2. 先保存当前答案
	SaveAnswerFiles(strCode);

	// 3. 组装完整程序（框架 + 学生答案），写入 answer.cs
	CString fullCode = ComposeFullCode(strCode);
	WriteFileANSI(m_strCsPath, fullCode);

	// 4. 交给工作线程去编译 + 判题，主线程照常跑消息循环，界面不假死
	StartJudgeThread(strCode);
}

// ===== 启动判题线程 =====
void CPracticeDlg::StartJudgeThread(const CString& strCode)
{
	m_strSubmittedCode = strCode;
	m_bJudgeCancel = 0;
	m_nJudgeDone = 0;
	m_bJudgeCompileOk = FALSE;
	m_nJudgeScore = 0;
	m_strJudgeDetail.Empty();
	m_strJudgeErr.Empty();

	m_bJudging = TRUE;

	// 判题期间：【提交判题】变成【取消判题】并保持可用，其余按钮禁掉
	SetDlgItemText(IDC_BTN_SUBMIT, _T("取消判题"));
	GetDlgItem(IDC_BTN_SUBMIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_BACK)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_DEBUG)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_SAVE)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_RESET)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_RESTORE)->EnableWindow(FALSE);
	m_staticTimer.SetWindowText(_T("正在编译并判题…"));

	// CREATE_SUSPENDED + m_bAutoDelete=FALSE：线程对象由我们自己回收，
	// 否则线程一结束 CWinThread 自删、句柄失效，收尾时就没法安全等待了
	m_pJudgeThread = AfxBeginThread(JudgeThreadProc, this,
		THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (m_pJudgeThread == nullptr)
	{
		m_bJudging = FALSE;
		SetDlgItemText(IDC_BTN_SUBMIT, _T("提交判题"));
		GetDlgItem(IDC_BTN_BACK)->EnableWindow(TRUE);
		GetDlgItem(IDC_BTN_DEBUG)->EnableWindow(TRUE);
		GetDlgItem(IDC_BTN_SAVE)->EnableWindow(TRUE);
		GetDlgItem(IDC_BTN_RESET)->EnableWindow(TRUE);
		GetDlgItem(IDC_BTN_RESTORE)->EnableWindow(TRUE);
		UpdateTimerText();
		MessageBox(_T("无法启动判题线程，请重试。"), _T("判题失败"), MB_OK | MB_ICONERROR);
		return;
	}
	m_pJudgeThread->m_bAutoDelete = FALSE;
	m_pJudgeThread->ResumeThread();
}

// 线程入口
UINT AFX_CDECL CPracticeDlg::JudgeThreadProc(LPVOID pParam)
{
	CPracticeDlg* pThis = (CPracticeDlg*)pParam;
	if (pThis != nullptr)
		pThis->DoJudgeInThread();
	return 0;
}

// 线程体：编译 + 黑盒判题（只碰数据，不碰界面；界面更新一律用 PostMessage 交回主线程）
void CPracticeDlg::DoJudgeInThread()
{
	CString errMsg;
	m_bJudgeCompileOk = CompileAndRun(m_strCsPath, errMsg);
	if (!m_bJudgeCompileOk)
	{
		m_strJudgeErr = errMsg;
		::PostMessage(m_hWnd, WM_JUDGE_DONE, 0, 0);
		return;
	}

	m_nJudgeScore = JudgeByCases(m_strJudgeDetail, errMsg);
	if (m_nJudgeScore < 0)
		m_strJudgeErr = errMsg;

	::PostMessage(m_hWnd, WM_JUDGE_DONE, 0, 0);
}

// 请求取消判题
void CPracticeDlg::RequestCancelJudge()
{
	m_bJudgeCancel = 1;
	GetDlgItem(IDC_BTN_SUBMIT)->EnableWindow(FALSE);   // 防止连点
	m_staticTimer.SetWindowText(_T("正在中止判题…"));
}

// 进度回报（主线程）
LRESULT CPracticeDlg::OnJudgeProgress(WPARAM wParam, LPARAM lParam)
{
	CString str;
	str.Format(_T("正在判题 %d/%d（可点取消判题）"), (int)wParam, (int)lParam);
	m_staticTimer.SetWindowText(str);
	return 0;
}

// 判题结束（主线程）：回收线程 → 恢复界面 → 弹分数
LRESULT CPracticeDlg::OnJudgeDone(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	// 线程刚 PostMessage 完就快退出了，这里等一下再回收；
	// 必须等它真的退出，否则后面删窗口时线程可能还在用 this
	if (m_pJudgeThread != nullptr)
	{
		::WaitForSingleObject(m_pJudgeThread->m_hThread, 10000);
		delete m_pJudgeThread;
		m_pJudgeThread = nullptr;
	}

	m_bJudging = FALSE;
	SetDlgItemText(IDC_BTN_SUBMIT, _T("提交判题"));
	GetDlgItem(IDC_BTN_SUBMIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_BACK)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_DEBUG)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_SAVE)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_RESET)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_RESTORE)->EnableWindow(TRUE);
	UpdateTimerText();

	// 用户主动取消：不判分、不记成绩，只提示一下
	if (m_bJudgeCancel)
	{
		MessageBox(_T("已取消判题，本次不计分。\r\n\r\n你的答案还在答题区里，可以改完再提交。"),
			_T("已取消"), MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	if (!m_bJudgeCompileOk)
	{
		CString strShow = m_strJudgeErr;
		strShow.Replace(_T("\n"), _T("\r\n"));
		if (strShow.GetLength() > 1500)
			strShow = strShow.Left(1500) + _T("\r\n……（错误信息过长已截断）");
		MessageBox(_T("判题失败（编译错误）：\r\n\r\n") + strShow, _T("判题结果"), MB_OK | MB_ICONERROR);
		return 0;
	}

	if (m_nJudgeScore < 0)
	{
		MessageBox(m_strJudgeErr, _T("判题失败"), MB_OK | MB_ICONERROR);
		return 0;
	}

	int nTotal = 12; // 每题满分固定12分

	// 写成绩记录
	WriteScoreRecord(m_strSubmittedCode, m_nJudgeScore, nTotal);
	m_strLastSubmitted = m_strSubmittedCode;  // 返回主界面时不再重复确认
	DeleteDraft();                            // 本题已提交判分：清掉草稿，下次打开是干净的空白框架

	// 弹分数窗口期间把 m_bJudging 置回 TRUE：计时到点也不会销毁父窗口
	m_bJudging = TRUE;
	CScoreDlg dlg(m_nJudgeScore, nTotal, m_strJudgeDetail, this);
	INT_PTR nRet = dlg.DoModal();
	m_bJudging = FALSE;
	if (nRet == 3)
	{
		// 用户选择"返回主界面"
		OnBnClickedBtnBack();
	}
	return 0;
}

// 返回主界面按钮
void CPracticeDlg::OnBnClickedBtnBack()
{
	DoLeavePractice(TRUE);
}

// 点右上角X关闭时，也走同一套逻辑
void CPracticeDlg::OnCancel()
{
	DoLeavePractice(TRUE);
}

// ===== 离开做题界面的统一出口 =====
//   bAskConfirm=TRUE：答题区有未提交内容时先弹确认，防止误点一下就把写的全丢了
//   无论是否确认，离开前都会把答案存成草稿，下次打开这道题自动带出来
void CPracticeDlg::DoLeavePractice(BOOL bAskConfirm)
{
	// 判题进行中不能直接关窗口：判题线程还在用 this 里的题目信息和编译目录。
	// 但也不能像原来那样"点了没反应"——明确告诉学生先点【取消判题】。
	if (m_bJudging)
	{
		MessageBox(_T("正在判题中，现在还不能关闭窗口。\r\n\r\n")
			_T("想停下来的话，请先点【取消判题】，等判题中止后再返回。"),
			_T("判题进行中"), MB_OK | MB_ICONINFORMATION);
		return;
	}

	if (bAskConfirm && HasUnsavedContent())
	{
		CString strMsg;
		strMsg = _T("答题区里还有没提交的内容，返回主界面会中断本次练习。\r\n\r\n")
			_T("你的答案会自动存为本题草稿，下次打开这道题时会自动带出来。\r\n\r\n")
			_T("确定返回主界面吗？");
		if (MessageBox(strMsg, _T("确认返回"),
			MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
			return;
	}

	// 离开前保存草稿（已经提交判分过的题，会在这里被 SaveDraft 改成清掉草稿）
	SaveDraft();

	// 停止定时器
	KillTimer(1);

	// 清理本次做题产生的临时文件（answer*.cs/exe/txt、exam\、judge_tmp\）
	CleanupTempFiles();

	// 拿到父窗口（主对话框）
	CExamDlgProjDlg* pMain = (CExamDlgProjDlg*)GetParent();

	// 非模态对话框必须 DestroyWindow + delete
	this->DestroyWindow();
	delete this;
	pMain->m_pPracticeDlg = nullptr;

	// 重新显示主对话框
	pMain->ShowWindow(SW_SHOW);
	pMain->SetForegroundWindow();
}
