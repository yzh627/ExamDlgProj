#include "pch.h"
#include "framework.h"
#include "ExamDlgProj.h"
#include "ExamDlgProjDlg.h"
#include "afxdialogex.h"
#include "PublicDef.h"
#include "PracticeDlg.h"
#include "ScoreTable.h"       // 成绩记录：score.dat（内部数据）+ 成绩表.xlsx（展示）
#include <time.h>
#include <stdlib.h>
#include <atlimage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static CString FindCscPath(); // 查找判题用 csc.exe（定义在文件后面）

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// ============ "说明" 对话框（纯文本介绍 + 关闭按钮）============
// 说明文字：优先读程序目录下的 说明.txt（改文字不用重新编译程序），
//           文件不存在时用下面内置的默认文字。
static CString LoadAboutText()
{
	CString strPath = GetExeDir() + _T("说明.txt");
	CFile f;
	if (f.Open(strPath, CFile::modeRead))
	{
		ULONGLONG n64 = f.GetLength();
		if (n64 > 0 && n64 <= 64 * 1024)
		{
			int n = (int)n64;
			char* p = new char[n + 1];
			f.Read(p, n);
			p[n] = 0;
			f.Close();

			int nOff = 0;
			UINT nCodePage = CP_ACP;   // 记事本默认存的 ANSI(GBK)
			if (n >= 3 && (unsigned char)p[0] == 0xEF
				&& (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
			{
				nOff = 3;
				nCodePage = CP_UTF8;   // 带 BOM 的 UTF-8
			}

			CString str;
			int nWide = MultiByteToWideChar(nCodePage, 0, p + nOff, n - nOff, nullptr, 0);
			if (nWide > 0)
			{
				MultiByteToWideChar(nCodePage, 0, p + nOff, n - nOff,
					str.GetBuffer(nWide), nWide);
				str.ReleaseBuffer();
				// 统一成 Windows 换行，多行文本框才显示正常
				str.Replace(_T("\r\n"), _T("\n"));
				str.Replace(_T("\r"), _T("\n"));
				str.Replace(_T("\n"), _T("\r\n"));
			}
			delete[] p;
			if (!str.Trim().IsEmpty())
				return str;
		}
		else
		{
			f.Close();
		}
	}

	// 内置默认说明文字（想改这里的话，改完要重新编译）
	CString strText;
	strText += _T("〔河北对口升学计算机程序设计练习系统〕\r\n");
	strText += _T("\r\n");
	strText += _T("〔河北对口升学计算机程序设计练习系统〕是我在学习 AI 开发的过程中做的一个小工具，初衷是练手，也希望能帮到正在准备河北对口升学计算机类考试的同学们。\r\n");
	strText += _T("\r\n");
	strText += _T("我不敢说这个软件没有漏洞。如果你在使用中发现任何不足之处，或者有任何意见、建议，欢迎到微信公众号「一个中职生」私信我，我都会认真看。\r\n");
	strText += _T("\r\n");
	strText += _T("祝你备考顺利。\r\n");
	strText += _T("────────────────────────\r\n");
	strText += _T("版本：〔1.0.0〕\r\n");
	strText += _T("作者：一个中职生\r\n");
	strText += _T("反馈：微信公众号「一个中职生」\r\n");
	strText += _T("────────────────────────\r\n");
	return strText;
}

class CAboutInfoDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CAboutInfoDlg)

public:
	CAboutInfoDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_ABOUT_INFO, pParent) {}
	virtual ~CAboutInfoDlg() {}

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUT_INFO };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};

IMPLEMENT_DYNAMIC(CAboutInfoDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CAboutInfoDlg, CDialogEx)
END_MESSAGE_MAP()

BOOL CAboutInfoDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetWindowText(_T("说明 - 河北对口升学计算机程序设计练习系统"));

	SetDlgItemText(IDC_STATIC_ABOUT_TEXT, LoadAboutText());
	return TRUE;
}

// ============ "给作者回血" 对话框 ============
// 【图片不编译进exe】，而是运行时从程序目录读取，方便你随时更换：
// 依次尝试 support.png / support.jpg / support.jpeg / support.bmp /
//          收款码.png / 打赏.png / zanshang.png
// 找不到图片时不会报错，只在窗口里提示怎么放图片。
class CSupportDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CSupportDlg)

public:
	CSupportDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_SUPPORT_DLG, pParent)
	{
		m_bLoaded = FALSE;
	}
	virtual ~CSupportDlg() {}

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SUPPORT_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	void DrawSupportImage(CDC& dc);   // 实际绘制（供双缓冲复用）
	DECLARE_MESSAGE_MAP()

public:
	CImage m_img;      // 收款码/图片（支持 png/jpg/bmp，png 的透明通道也支持）
	BOOL m_bLoaded;    // 是否成功加载到图片
	CRect m_rcPic;     // 图片显示区域（客户区坐标）
};

IMPLEMENT_DYNAMIC(CSupportDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CSupportDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CSupportDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_bLoaded = FALSE;

	// 计算图片显示区域：上边让开提示文字，下边让开【关闭】按钮
	// （不用静态框占位，避免子控件把画上去的图片盖住）
	CRect rcClient;
	GetClientRect(&rcClient);
	int nTop = 20;
	int nBottom = rcClient.bottom - 20;

	CWnd* pTip = GetDlgItem(IDC_STATIC_SUPPORT_TIP);
	if (pTip != nullptr)
	{
		CRect rc;
		pTip->GetWindowRect(&rc);
		ScreenToClient(&rc);
		nTop = rc.bottom + 10;
	}
	CWnd* pBtn = GetDlgItem(IDCANCEL);
	if (pBtn != nullptr)
	{
		CRect rc;
		pBtn->GetWindowRect(&rc);
		ScreenToClient(&rc);
		nBottom = rc.top - 10;
	}
	m_rcPic.SetRect(20, nTop, rcClient.right - 20, nBottom);
	if (m_rcPic.bottom <= m_rcPic.top + 20)
		m_rcPic.bottom = m_rcPic.top + 20;

	// 依次尝试程序目录下的图片文件
	static const LPCTSTR arrNames[] = {
		_T("support.png"), _T("support.jpg"), _T("support.jpeg"), _T("support.bmp"),
		_T("收款码.png"), _T("打赏.png"), _T("zanshang.png")
	};
	CString strDir = GetExeDir();
	for (int i = 0; i < (int)(sizeof(arrNames) / sizeof(arrNames[0])); i++)
	{
		CString strFile = strDir + arrNames[i];
		if (GetFileAttributes(strFile) == INVALID_FILE_ATTRIBUTES)
			continue;
		if (SUCCEEDED(m_img.Load(strFile))) // CImage 支持 png/jpg/bmp
		{
			m_bLoaded = TRUE;
			break;
		}
	}

	if (m_bLoaded)
	{
		SetDlgItemText(IDC_STATIC_SUPPORT_TIP,
			_T("感谢支持！扫码请作者喝杯奶茶，谢谢鼓励～"));
	}
	else
	{
		SetDlgItemText(IDC_STATIC_SUPPORT_TIP,
			_T("还没有放图片哦～\r\n把收款码图片放到程序所在目录，命名为 support.png（也支持 jpg/bmp），\r\n再点这个按钮就能看到。"));
	}
	return TRUE;
}

// 背景交给 OnPaint 一次画完，避免先擦后画造成的闪烁
BOOL CSupportDlg::OnEraseBkgnd(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
	return TRUE;
}

// 双缓冲：先画到内存 DC，再一次性贴到屏幕。原实现直接往窗口 DC 上画，
// 缩放图片（二维码）时每一帧都看得见擦除+重绘，会闪。
void CSupportDlg::OnPaint()
{
	CPaintDC dc(this);

	CRect rcClient;
	GetClientRect(&rcClient);
	if (rcClient.IsRectEmpty())
		return;

	CDC dcMem;
	CBitmap bmp;
	if (!dcMem.CreateCompatibleDC(&dc)
		|| !bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height()))
	{
		// 内存 DC / 位图建不出来时退回直接画，保证功能不丢
		DrawSupportImage(dc);
		return;
	}

	CBitmap* pOldBmp = dcMem.SelectObject(&bmp);
	dcMem.FillSolidRect(&rcClient, ::GetSysColor(COLOR_3DFACE));
	DrawSupportImage(dcMem);
	dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &dcMem, 0, 0, SRCCOPY);
	dcMem.SelectObject(pOldBmp);
}

void CSupportDlg::DrawSupportImage(CDC& dc)
{
	if (m_bLoaded && m_img.GetWidth() > 0 && m_img.GetHeight() > 0)
	{
		// 等比缩放居中显示（相邻像素法，二维码放大/缩小后依然清晰可扫）
		int nW = m_img.GetWidth();
		int nH = m_img.GetHeight();
		double f = min((double)m_rcPic.Width() / nW, (double)m_rcPic.Height() / nH);
		int w = (int)(nW * f + 0.5);
		int h = (int)(nH * f + 0.5);
		int x = m_rcPic.left + (m_rcPic.Width() - w) / 2;
		int y = m_rcPic.top + (m_rcPic.Height() - h) / 2;

		dc.Draw3dRect(&m_rcPic, RGB(210, 210, 210), RGB(210, 210, 210));
		dc.SetStretchBltMode(COLORONCOLOR);
		m_img.Draw(dc.m_hDC, x, y, w, h);
	}
	else
	{
		CRect rc = m_rcPic;
		dc.Draw3dRect(&rc, RGB(210, 210, 210), RGB(210, 210, 210));
		dc.DrawText(_T("（未找到图片文件）\r\n\r\n把二维码图片放到程序目录，\r\n命名为 support.png 即可"),
			&rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
	}
}

CExamDlgProjDlg::CExamDlgProjDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EXAMDLGPROJ_DIALOG, pParent)
	, m_nTime(15)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pPracticeDlg = nullptr;
	// 布局成员必须先初始化：窗口创建时 WM_SIZE 可能先于 OnInitDialog 到达，
	// 否则 OnSize 会读到未初始化的 m_nLayoutCount/m_layout 而越界崩溃。
	m_nLayoutCount = 0;
	m_bLayoutInit = FALSE;
	m_rcInit.SetRectEmpty();
	m_rcMin.SetRectEmpty();
}

void CExamDlgProjDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_QUESTION, m_lstQuestion);
	DDX_Control(pDX, IDC_CHK_SIMPLE, m_chkSimple);
	DDX_Control(pDX, IDC_CHK_MEDIUM, m_chkMedium);
	DDX_Control(pDX, IDC_CHK_HARD, m_chkHard);
	DDX_Control(pDX, IDC_CHK_REAL, m_chkReal);
	DDX_Control(pDX, IDC_BTN_ALL_LEVEL, m_btnAllLevel);
	DDX_Control(pDX, IDC_STATIC_COUNT, m_staticCount);
	DDX_Control(pDX, IDC_STATIC_SEL, m_staticSel);
	DDX_Text(pDX, IDC_EDIT_TIME, m_nTime);
}

BEGIN_MESSAGE_MAP(CExamDlgProjDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_START, &CExamDlgProjDlg::OnBnClickedBtnStart)
	ON_BN_CLICKED(IDC_BTN_RANDOM, &CExamDlgProjDlg::OnBnClickedBtnRandom)
	ON_BN_CLICKED(IDC_BTN_SUPPORT, &CExamDlgProjDlg::OnBnClickedBtnSupport)
	ON_BN_CLICKED(IDC_BTN_ABOUT, &CExamDlgProjDlg::OnBnClickedBtnAbout)
	ON_BN_CLICKED(IDC_CHK_SIMPLE, &CExamDlgProjDlg::OnBnClickedChkLevel)
	ON_BN_CLICKED(IDC_CHK_MEDIUM, &CExamDlgProjDlg::OnBnClickedChkLevel)
	ON_BN_CLICKED(IDC_CHK_HARD, &CExamDlgProjDlg::OnBnClickedChkLevel)
	ON_BN_CLICKED(IDC_CHK_REAL, &CExamDlgProjDlg::OnBnClickedChkReal)
	ON_BN_CLICKED(IDC_BTN_ALL_LEVEL, &CExamDlgProjDlg::OnBnClickedBtnAllLevel)
	ON_BN_CLICKED(IDC_BTN_SCORETABLE, &CExamDlgProjDlg::OnBnClickedBtnScoreTable)
	ON_LBN_DBLCLK(IDC_LIST_QUESTION, &CExamDlgProjDlg::OnLbnDblclkListQuestion)
	ON_LBN_SELCHANGE(IDC_LIST_QUESTION, &CExamDlgProjDlg::OnLbnSelchangeListQuestion)
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

BOOL CExamDlgProjDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// ===== 难度勾选框：默认三个难度全部勾选 =====
	m_chkSimple.SetCheck(BST_CHECKED);
	m_chkMedium.SetCheck(BST_CHECKED);
	m_chkHard.SetCheck(BST_CHECKED);
	// ===== 真题筛选：默认不勾选（=显示全部题目） =====
	m_chkReal.SetCheck(BST_UNCHECKED);
	m_bVerifying = FALSE;
	m_bSilent = FALSE;
	m_nVerifyTotal = 0;
	m_nVerifyDone = 0;

	// ===== 加载题库（优先读外部文件，失败用内置）=====
	LoadQuestionBank();
	RefreshList();
	InitLayout();   // 采集控件位置，窗口拉伸时自适应
	GetWindowRect(&m_rcMin); // 记录初始尺寸作为最小尺寸

	// 题库损坏时停用开始练习，只保留界面提示
	if (m_bBankError)
	{
		GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
		GetDlgItem(IDC_BTN_RANDOM)->EnableWindow(FALSE);
		m_staticCount.SetWindowText(_T("题库加载失败：请检查程序目录下的 questions.dat（或 questions.txt）"));
		m_staticSel.SetWindowText(_T("当前选中：--"));
	}

	// ===== 判题环境自查：找不到 csc.exe 时提前提醒，别等学生提交才发现 =====
	if (!m_bSilent && FindCscPath().IsEmpty())
	{
		MessageBox(_T("未找到 .NET 编译环境（csc.exe），【提交判题】功能将无法使用！\r\n\r\n")
			_T("请安装 .NET Framework 4.x（Windows 10/11 一般自带），\r\n")
			_T("或确认下面这个文件存在：\r\nC:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\csc.exe"),
			_T("环境检查"), MB_OK | MB_ICONWARNING);
	}

	m_nTime = 15;
	UpdateData(FALSE);

	return TRUE;
}

// ===== 加载题库 =====
void CExamDlgProjDlg::LoadQuestionBank()
{
	m_bBankError = FALSE;

	// 依次查找：exe目录 → exe上一级 → exe上两级（兼容从项目根目录运行）
	// 每一层都优先找加密题库 questions.dat，再找明文 questions.txt
	CString strExeDir = GetExeDir();
	CStringArray arrDirs;
	arrDirs.Add(strExeDir);

	CString strParent = strExeDir;
	int nPos = strParent.ReverseFind(_T('\\'));
	if (nPos >= 0)
	{
		strParent = strParent.Left(nPos + 1);
		arrDirs.Add(strParent);

		CString strGrand = strParent;
		nPos = strGrand.ReverseFind(_T('\\'));
		if (nPos >= 0)
		{
			strGrand = strGrand.Left(nPos + 1);
			arrDirs.Add(strGrand);
		}
	}

	CStringArray arrCandidates;
	for (int i = 0; i < (int)arrDirs.GetSize(); i++)
	{
		CString strDat = arrDirs.GetAt(i) + _T("questions.dat");
		CString strTxt = arrDirs.GetAt(i) + _T("questions.txt");
		BOOL bDat = (GetFileAttributes(strDat) != INVALID_FILE_ATTRIBUTES);
		BOOL bTxt = (GetFileAttributes(strTxt) != INVALID_FILE_ATTRIBUTES);

		// 同一目录下两种题库都有时，取修改时间较新的那个：
		// 开发时改完 questions.txt 忘了执行 /pack，也不会读到旧的加密题库
		BOOL bTxtFirst = FALSE;
		if (bDat && bTxt)
		{
			WIN32_FILE_ATTRIBUTE_DATA fd, ft;
			if (GetFileAttributesEx(strDat, GetFileExInfoStandard, &fd)
				&& GetFileAttributesEx(strTxt, GetFileExInfoStandard, &ft))
				bTxtFirst = (CompareFileTime(&ft.ftLastWriteTime, &fd.ftLastWriteTime) > 0);
		}

		if (bTxtFirst)
		{
			arrCandidates.Add(strTxt);
			arrCandidates.Add(strDat);
		}
		else
		{
			arrCandidates.Add(strDat); // 发布版：只有加密题库，优先加载
			arrCandidates.Add(strTxt);
		}
	}

	BOOL bFileExists = FALSE;
	CString strBadFile;
	for (int i = 0; i < (int)arrCandidates.GetSize(); i++)
	{
		if (GetFileAttributes(arrCandidates.GetAt(i)) != INVALID_FILE_ATTRIBUTES)
		{
			bFileExists = TRUE;
			if (m_bank.LoadFromFile(arrCandidates.GetAt(i)))
				return; // 找到并成功加载
			strBadFile = arrCandidates.GetAt(i);
		}
	}

	// 情况一：题库文件在，但打不开或格式错误
	// 明确提示并停用【开始练习】，避免静默退回"只有3道题的内置题库"被误认为题目变少
	if (bFileExists)
	{
		m_bBankError = TRUE;
		if (!m_bSilent)
		{
			CString strMsg;
			strMsg.Format(_T("题库文件打开失败或格式不正确！\r\n\r\n文件：%s\r\n\r\n")
				_T("请确认题库文件完整（发布版为 questions.dat，开发版为 questions.txt），\r\n")
				_T("或联系监考老师。为避免题目变少造成误判，【开始练习】已停用。"),
				(LPCTSTR)strBadFile);
			MessageBox(strMsg, _T("题库加载失败"), MB_OK | MB_ICONERROR);
		}
		return;
	}

	// 情况二：完全找不到题库文件 → 退回内置示例题库（仅3题，供开发调试）
	if (!m_bSilent)
	{
		MessageBox(_T("没有找到题库文件！\r\n\r\n程序目录下应有 questions.dat（发布版）或 questions.txt（开发版）。\r\n")
			_T("现在载入的是内置示例题库，只有3道题，请检查题库文件是否随程序一起拷贝。"),
			_T("题库提示"), MB_OK | MB_ICONWARNING);
	}
	m_bank.SetDefault();
}

// ===== 难度勾选状态：只要有一个勾选就按勾选的筛选，一个都没勾=显示全部 =====
BOOL CExamDlgProjDlg::IsLevelChecked(LPCTSTR lpszLevel)
{
	BOOL bSimple = (m_chkSimple.GetCheck() == BST_CHECKED);
	BOOL bMedium = (m_chkMedium.GetCheck() == BST_CHECKED);
	BOOL bHard = (m_chkHard.GetCheck() == BST_CHECKED);

	if (!bSimple && !bMedium && !bHard)
		return TRUE; // 一个都没勾，视为"全部"

	CString strLevel = lpszLevel;
	if (strLevel == _T("简单"))
		return bSimple;
	if (strLevel == _T("中等"))
		return bMedium;
	if (strLevel == _T("困难"))
		return bHard;
	return TRUE;
}

// ===== 列表行 → 题库下标（优先用item data，向前兼容映射数组） =====
int CExamDlgProjDlg::GetBankIndexFromList(int nListIdx)
{
	if (nListIdx < 0 || nListIdx >= m_lstQuestion.GetCount())
		return -1;

	DWORD_PTR dwData = m_lstQuestion.GetItemData(nListIdx);
	if (dwData != (DWORD_PTR)-1 && dwData < (DWORD_PTR)m_bank.GetCount())
		return (int)dwData;

	if (nListIdx < m_mapListToBank.GetSize())
		return m_mapListToBank.GetAt(nListIdx);
	return -1;
}

// ===== 是否勾选了"只看真题" =====
BOOL CExamDlgProjDlg::IsRealOnly()
{
	return (m_chkReal.GetCheck() == BST_CHECKED);
}

// ===== 更新题目数量提示 =====
void CExamDlgProjDlg::UpdateCountText()
{
	// 统计题库里的真题数量（标题带"真题"或写了"真题=是"的题目）
	int nReal = 0;
	for (int i = 0; i < m_bank.GetCount(); i++)
	{
		if (m_bank.GetAt(i).bReal)
			nReal++;
	}

	CString str;
	str.Format(_T("题库共 %d 题（含真题 %d 题），当前显示 %d 题"),
		m_bank.GetCount(), nReal, m_lstQuestion.GetCount());
	if (m_lstQuestion.GetCount() == 0)
		str += _T("（没有符合当前筛选条件的题目，请重新勾选难度或真题）");
	else if (m_chkSimple.GetCheck() != BST_CHECKED
		&& m_chkMedium.GetCheck() != BST_CHECKED
		&& m_chkHard.GetCheck() != BST_CHECKED)
	{
		// 三个难度都没勾时程序按"全部"处理（避免勾空了看到白板），
		// 但界面上必须写明白，否则使用者会以为筛选坏了
		str += _T("（未勾选难度，当前按全部显示）");
	}
	m_staticCount.SetWindowText(str);
}

// ===== 按难度/真题勾选刷新题库列表 =====
// 注意：列表框不能带LBS_SORT，否则顺序会被按字符串打乱（1、10、100…），
//       而且列表行号和题库下标会错位，导致选中的题和打开的题不一致。
void CExamDlgProjDlg::RefreshList()
{
	// 记住刷新前选中的题库下标，刷新后尽量恢复到同一题
	int nOldBank = GetBankIndexFromList(m_lstQuestion.GetCurSel());

	BOOL bRealOnly = IsRealOnly();   // 勾了"只看真题"就只显示真题

	m_lstQuestion.ResetContent();
	m_mapListToBank.RemoveAll();

	for (int i = 0; i < m_bank.GetCount(); i++)
	{
		QuestionInfo q = m_bank.GetAt(i);
		if (!IsLevelChecked(q.strLevel))
			continue;
		if (bRealOnly && !q.bReal)
			continue;

		CString strItem;
		strItem.Format(_T("%d. %s"), q.nID, (LPCTSTR)q.strTitle);
		int nIdx = m_lstQuestion.AddString(strItem);
		if (nIdx < 0)
			continue;

		// item data 里存题库下标，双击/选中时直接取，不受排序影响
		m_lstQuestion.SetItemData(nIdx, (DWORD_PTR)i);
		if (nIdx < m_mapListToBank.GetSize())
			m_mapListToBank.SetAt(nIdx, i);
		else
			m_mapListToBank.Add(i);
	}

	// 恢复选中项
	int nRestore = -1;
	for (int i = 0; i < m_lstQuestion.GetCount(); i++)
	{
		if (GetBankIndexFromList(i) == nOldBank)
		{
			nRestore = i;
			break;
		}
	}
	if (nRestore < 0 && m_lstQuestion.GetCount() > 0)
		nRestore = 0;
	if (nRestore >= 0)
	{
		m_lstQuestion.SetCurSel(nRestore);
		m_lstQuestion.SetTopIndex(nRestore);
	}

	UpdateCountText();
	UpdateSelText();
}

// ===== 难度勾选变化：立即刷新列表 =====
void CExamDlgProjDlg::OnBnClickedChkLevel()
{
	RefreshList();
}

// ===== 真题筛选变化：立即刷新列表 =====
void CExamDlgProjDlg::OnBnClickedChkReal()
{
	RefreshList();
}

// ===== 处理界面消息：自检时保持窗口可响应（不"卡死"） =====
void CExamDlgProjDlg::PumpUi()
{
	if (m_bSilent)
		return;
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			PostQuitMessage((int)msg.wParam);
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

// ===== 双击列表中的题目，直接开始练习 =====
void CExamDlgProjDlg::OnLbnDblclkListQuestion()
{
	OnBnClickedBtnStart();
}

// ===== "给作者回血"：弹出显示图片的小窗口（图片从程序目录读取） =====
void CExamDlgProjDlg::OnBnClickedBtnSupport()
{
	CSupportDlg dlg(this);
	dlg.DoModal();
}

// ===== "说明"：弹出软件说明窗口（纯文本 + 关闭按钮） =====
void CExamDlgProjDlg::OnBnClickedBtnAbout()
{
	CAboutInfoDlg dlg(this);
	dlg.DoModal();
}

// ===== "查看成绩表"：按 score.dat 重新生成 成绩表.xlsx，然后弹出"选择一个应用"让学生自己挑软件 =====
// 每次点都重生成一遍，保证看到的一定是最新的（生成只要几毫秒）。
void CExamDlgProjDlg::OnBnClickedBtnScoreTable()
{
	CString strErr;
	int nCount = ScoreTable::RebuildXlsx(nullptr, &strErr);

	if (!strErr.IsEmpty())
	{
		AfxMessageBox(_T("生成成绩表失败：\r\n") + strErr, MB_OK | MB_ICONWARNING);
		return;
	}

	CString strXlsx = ScoreTable::XlsxPath();

	if (nCount <= 0)
	{
		AfxMessageBox(_T("还没有成绩记录。\r\n\r\n先挑一道题练一练，点了【提交判题】之后，")
			_T("这里就会自动生成一张成绩表。"), MB_OK | MB_ICONINFORMATION);
		return;
	}

	if (ScoreTable::ShellOpen(strXlsx))
		return;

	// 走到这里，说明连"打开方式"选择器都没能调起来、默认程序也开不动 ——
	// 基本就是这台机器没装 Excel/WPS 这类表格软件。如实说清楚，并给出文件位置
	CString strMsg;
	strMsg.Format(_T("成绩表已经生成好了（共 %d 条记录），但这台电脑上没能打开它——")
		_T("多半是没装 Excel 或 WPS 这类表格软件。\r\n\r\n")
		_T("文件位置：\r\n%s\r\n\r\n")
		_T("要帮你打开它所在的文件夹吗？（在那里把这个文件拷到有表格软件的电脑上就能看）"),
		nCount, (LPCTSTR)strXlsx);
	if (AfxMessageBox(strMsg, MB_YESNO | MB_ICONINFORMATION) == IDYES)
		ScoreTable::ShellRevealInFolder(strXlsx);
}

// ===== 鼠标滚轮滚动题库列表（158题时很需要） =====
BOOL CExamDlgProjDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (m_lstQuestion.GetSafeHwnd() != nullptr)
	{
		CRect rc;
		m_lstQuestion.GetWindowRect(&rc);
		if (rc.PtInRect(pt))
		{
			int nLines = 3; // 每滚一格滚3行
			int nCode = (zDelta > 0) ? SB_LINEUP : SB_LINEDOWN;
			for (int i = 0; i < nLines; i++)
				m_lstQuestion.SendMessage(WM_VSCROLL, MAKEWPARAM(nCode, 0), 0);
			return TRUE;
		}
	}
	return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

// ===== 更新"当前选中：xxx"提示 =====
void CExamDlgProjDlg::UpdateSelText()
{
	int nBank = GetBankIndexFromList(m_lstQuestion.GetCurSel());
	CString str;
	if (nBank < 0)
	{
		str = _T("当前选中：--");
	}
	else
	{
		QuestionInfo q = m_bank.GetAt(nBank);
		// 标题里若已带难度（如"（简单·真题）"），就不再重复追加标签
		if (q.strTitle.Find(q.strLevel) >= 0)
			str.Format(_T("当前选中：%d. %s"), q.nID, (LPCTSTR)q.strTitle);
		else if (q.bReal)
			str.Format(_T("当前选中：%d. %s  【%s·真题】"), q.nID, (LPCTSTR)q.strTitle, (LPCTSTR)q.strLevel);
		else
			str.Format(_T("当前选中：%d. %s  【%s】"), q.nID, (LPCTSTR)q.strTitle, (LPCTSTR)q.strLevel);
	}
	m_staticSel.SetWindowText(str);
}

// ===== 列表选中项变化 =====
void CExamDlgProjDlg::OnLbnSelchangeListQuestion()
{
	UpdateSelText();
}

// ===== "全选"：难度三个全部勾上，并取消"只看真题"（恢复显示全部题目） =====
void CExamDlgProjDlg::OnBnClickedBtnAllLevel()
{
	m_chkSimple.SetCheck(BST_CHECKED);
	m_chkMedium.SetCheck(BST_CHECKED);
	m_chkHard.SetCheck(BST_CHECKED);
	m_chkReal.SetCheck(BST_UNCHECKED);
	RefreshList();
}

// ===== 采集控件初始位置（窗口拉伸时用锚点调整） =====
void CExamDlgProjDlg::InitLayout()
{
	GetClientRect(&m_rcInit);

	struct TmpItem { UINT nID; BOOL bStretchW, bStretchH, bMoveX, bMoveY; };
	TmpItem arr[] = {
		{ IDC_GROUP_LIST,    TRUE,  TRUE,  FALSE, FALSE }, // 分组框
		{ IDC_LIST_QUESTION, TRUE,  TRUE,  FALSE, FALSE }, // 题目列表
		{ IDC_STATIC_COUNT,  TRUE,  FALSE, FALSE, TRUE  }, // 数量提示（跟在列表下面）
		{ IDC_LABEL_LEVEL,   FALSE, FALSE, FALSE, TRUE  },
		{ IDC_CHK_SIMPLE,    FALSE, FALSE, FALSE, TRUE  },
		{ IDC_CHK_MEDIUM,    FALSE, FALSE, FALSE, TRUE  },
		{ IDC_CHK_HARD,      FALSE, FALSE, FALSE, TRUE  },
		{ IDC_CHK_REAL,      FALSE, FALSE, FALSE, TRUE  }, // 只看真题
		{ IDC_BTN_ALL_LEVEL, FALSE, FALSE, FALSE, TRUE  },
		{ IDC_BTN_RANDOM,    FALSE, FALSE, FALSE, TRUE  },
		{ IDC_LABEL_TIME,    FALSE, FALSE, FALSE, TRUE  },
		{ IDC_EDIT_TIME,     FALSE, FALSE, FALSE, TRUE  },
		{ IDC_STATIC_SEL,    TRUE,  FALSE, FALSE, TRUE  },
		{ IDC_BTN_START,     TRUE,  FALSE, FALSE, TRUE  },
		{ IDC_BTN_ABOUT,     FALSE, FALSE, TRUE,  TRUE  }, // 右下角小按钮（右对齐）
		{ IDC_BTN_SUPPORT,   FALSE, FALSE, TRUE,  TRUE  }, // 右下角小按钮（右对齐）
		{ IDC_BTN_SCORETABLE,FALSE, FALSE, FALSE, TRUE  }, // 左下角小按钮（只跟着窗口下移）
	};

	m_nLayoutCount = 0;
	for (int i = 0; i < (int)(sizeof(arr) / sizeof(arr[0])); i++)
	{
		CWnd* pWnd = GetDlgItem(arr[i].nID);
		if (pWnd == nullptr)
			continue;
		CRect rc;
		pWnd->GetWindowRect(&rc);
		ScreenToClient(&rc);
		m_layout[m_nLayoutCount].nID = arr[i].nID;
		m_layout[m_nLayoutCount].rc = rc;
		m_layout[m_nLayoutCount].bStretchW = arr[i].bStretchW;
		m_layout[m_nLayoutCount].bStretchH = arr[i].bStretchH;
		m_layout[m_nLayoutCount].bMoveX = arr[i].bMoveX;
		m_layout[m_nLayoutCount].bMoveY = arr[i].bMoveY;
		m_nLayoutCount++;
	}
	m_bLayoutInit = TRUE;
}

// ===== 窗口大小变化：列表跟着变大，底部按钮跟着下移 =====
void CExamDlgProjDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (!m_bLayoutInit || cx <= 0 || cy <= 0)
		return;

	int dx = cx - m_rcInit.Width();
	int dy = cy - m_rcInit.Height();

	for (int i = 0; i < m_nLayoutCount; i++)
	{
		CWnd* pWnd = GetDlgItem(m_layout[i].nID);
		if (pWnd == nullptr)
			continue;
		CRect rc = m_layout[i].rc;
		if (m_layout[i].bStretchW)
			rc.right += dx;
		if (m_layout[i].bStretchH)
			rc.bottom += dy;
		if (m_layout[i].bMoveX)
		{
			rc.left += dx;
			rc.right += dx;
		}
		if (m_layout[i].bMoveY)
		{
			rc.top += dy;
			rc.bottom += dy;
		}
		pWnd->MoveWindow(rc);
	}
	Invalidate();
}

// ===== 限制最小窗口尺寸，避免把控件压变形 =====
void CExamDlgProjDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CDialogEx::OnGetMinMaxInfo(lpMMI);
	if (m_rcMin.Width() > 0 && m_rcMin.Height() > 0)
	{
		lpMMI->ptMinTrackSize.x = m_rcMin.Width();
		lpMMI->ptMinTrackSize.y = m_rcMin.Height();
	}
}

void CExamDlgProjDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		// 原来这里弹的是 CAboutDlg，它用的 IDD_ABOUTBOX 对话框模板在 .rc 里
		// 根本不存在（只有 IDD_ABOUT_INFO），DoModal 会抛资源异常后被吞掉，
		// 表现就是"系统菜单里点了『关于』一点反应都没有"。
		// 统一走程序自己那份说明窗口（和界面上的【说明】按钮同一个）。
		CAboutInfoDlg dlgAbout(this);
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

void CExamDlgProjDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CExamDlgProjDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// ===== 随机抽题按钮：随机选题并直接进入做题界面 =====
void CExamDlgProjDlg::OnBnClickedBtnRandom()
{
	UpdateData(TRUE);
	if (m_nTime <= 0)
	{
		MessageBox(_T("请输入有效的考试时长（分钟）！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	srand((unsigned)time(nullptr));
	int nCount = m_lstQuestion.GetCount();
	if (nCount <= 0)
	{
		MessageBox(_T("当前没有可练习的题目，请检查难度勾选！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}
	int nRand = rand() % nCount; // 随机选题
	LaunchPractice(nRand, m_nTime);
}

// ===== 开始练习按钮（核心：创建非模态做题对话框） =====
void CExamDlgProjDlg::OnBnClickedBtnStart()
{
	UpdateData(TRUE);

	if (m_nTime <= 0)
	{
		MessageBox(_T("请输入有效的考试时长（分钟）！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	int nSel = m_lstQuestion.GetCurSel();
	LaunchPractice(nSel, m_nTime);
}

// ===== 公共启动函数：组装题目数据并创建做题对话框 =====
void CExamDlgProjDlg::LaunchPractice(int nSel, int nTime)
{
	// 通过列表行号映射到题库下标
	int nBankIdx = GetBankIndexFromList(nSel);
	if (nBankIdx < 0)
	{
		MessageBox(_T("请先在列表中选择一道题目！"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	QuestionInfo qInfo = m_bank.GetAt(nBankIdx);
	// 题库里明确写了「时长」的题以题库为准；没写才用界面上填的时长
	if (!qInfo.bTimeFromBank)
		qInfo.nTimeMin = nTime;

	// 创建非模态对话框（必须new，不能局部变量）
	if (m_pPracticeDlg == nullptr)
	{
		m_pPracticeDlg = new CPracticeDlg(this, qInfo);
		if (!m_pPracticeDlg->Create(IDD_PRACTICE_DLG, this))
		{
			MessageBox(_T("创建做题窗口失败！"), _T("错误"), MB_OK | MB_ICONERROR);
			delete m_pPracticeDlg;
			m_pPracticeDlg = nullptr;
			return;
		}
	}

	// 隐藏主对话框，显示做题对话框
	ShowWindow(SW_HIDE);
	m_pPracticeDlg->ShowWindow(SW_SHOW);
	m_pPracticeDlg->SetForegroundWindow();
}

// ============================================================
// 题库自检：用"标准答案"自动验证每道题
// 加题后点主界面"题库自检"按钮：系统自动编译+跑全部用例，报告问题
// ============================================================

// 查找系统csc.exe
// ANSI字节串转Unicode CString
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

static CString FindCscPath()
{
	TCHAR szWin[MAX_PATH] = { 0 };
	GetWindowsDirectory(szWin, MAX_PATH);
	CString winDir = szWin;
	CString csc64 = winDir + _T("\\Microsoft.NET\\Framework64\\v4.0.30319\\csc.exe");
	CString csc32 = winDir + _T("\\Microsoft.NET\\Framework\\v4.0.30319\\csc.exe");
	if (GetFileAttributes(csc64) != INVALID_FILE_ATTRIBUTES)
		return csc64;
	if (GetFileAttributes(csc32) != INVALID_FILE_ATTRIBUTES)
		return csc32;
	return _T("");
}

// 写ANSI文本文件
static BOOL WriteAnsiFile(const CString& path, const CString& content)
{
	CFile f;
	if (!f.Open(path, CFile::modeCreate | CFile::modeWrite))
		return FALSE;
	int nLen = WideCharToMultiByte(CP_ACP, 0, content, -1, nullptr, 0, nullptr, nullptr);
	if (nLen > 1)
	{
		char* pBuf = new char[nLen];
		WideCharToMultiByte(CP_ACP, 0, content, -1, pBuf, nLen, nullptr, nullptr);
		f.Write(pBuf, nLen - 1);
		delete[] pBuf;
	}
	f.Close();
	return TRUE;
}

// 写UTF-8 BOM文本文件（自检报告，可直接用记事本/VS打开）
static BOOL WriteUtf8BomFile(const CString& path, const CString& content)
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

// 统一换行、去掉每行首尾空白后再比较（避免CRLF/行尾空格造成误判）
static CString NormalizeOutputText(const CString& str)
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

// 运行进程：可带参数、可写stdin、可指定超时（毫秒）
// 【重要】原实现在编译标准答案时没有传源文件参数，导致自检永远编译失败，
//         这里补上 args 参数，编译/运行都走同一个函数。
static BOOL RunVerifyProcess(const CString& exe, const CString& args, const CString& input,
	DWORD dwTimeoutMs, CString& output, CString& errMsg, const CString& workDir)
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
	si.hStdError = hOutWrite;
	si.hStdInput = hInRead;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	CString cmdLine = _T("\"") + exe + _T("\"");
	if (!args.IsEmpty())
		cmdLine += _T(" ") + args;
	TCHAR* pCmd = cmdLine.GetBuffer(cmdLine.GetLength() + 1);
	BOOL bOK = CreateProcess(nullptr, pCmd, nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr,
		workDir.IsEmpty() ? nullptr : (LPCTSTR)workDir, &si, &pi);
	cmdLine.ReleaseBuffer();

	if (!bOK)
	{
		errMsg = _T("启动进程失败");
		CloseHandle(hInRead); CloseHandle(hInWrite);
		CloseHandle(hOutRead); CloseHandle(hOutWrite);
		return FALSE;
	}

	CloseHandle(hOutWrite);

	// 写标准输入
	if (!input.IsEmpty())
	{
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

	if (WaitForSingleObject(pi.hProcess, dwTimeoutMs) == WAIT_TIMEOUT)
	{
		TerminateProcess(pi.hProcess, 1);
		WaitForSingleObject(pi.hProcess, 1000);
		errMsg = _T("运行超时");
	}

	CStringA strOutA;
	CHAR buf[4096];
	DWORD bytesRead = 0;
	while (ReadFile(hOutRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0)
	{
		buf[bytesRead] = 0;
		strOutA += buf;
	}

	CloseHandle(hInRead);
	CloseHandle(hOutRead);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	output = A2TStr(strOutA);
	return TRUE;
}

// 自检单题：框架+标准答案 → 编译 → 逐用例喂输入比对输出
// 返回空表示全部通过，否则返回错误描述
CString CExamDlgProjDlg::VerifyOneQuestion(const QuestionInfo& q)
{
	CString strErr;

	// 1. 组装完整代码（标准答案填入begin/end之间）
	CString frame = q.strFrameCode;
	CString beginMark = _T("/************begin************/");
	CString endMark = _T("/************end************/");
	int nBegin = frame.Find(beginMark);
	int nEnd = frame.Find(endMark);
	if (nBegin < 0 || nEnd < 0 || nEnd < nBegin)
		return _T("框架缺少begin/end标记");

	int nStart = nBegin + beginMark.GetLength();
	CString full = frame.Left(nStart) + _T("\r\n") + q.strStdAnswer + frame.Mid(nEnd);

	// 2. 写临时文件 → 调用csc编译（必须把源文件作为参数传给csc！）
	CString strDir = GetExeDir();
	CString strCs = strDir + _T("_verify_tmp.cs");
	CString strExe = strDir + _T("_verify_tmp.exe");
	if (!WriteAnsiFile(strCs, full))
		return _T("写入临时文件失败（目录可能只读）");

	CString cscPath = FindCscPath();
	if (cscPath.IsEmpty())
	{
		DeleteFile(strCs);
		return _T("未找到.NET编译环境(csc.exe)");
	}

	DeleteFile(strExe);
	CString strArgs;
	strArgs.Format(_T("/nologo /langversion:4 /out:\"%s\" \"%s\""), (LPCTSTR)strExe, (LPCTSTR)strCs);
	CString compOut, errMsg;
	RunVerifyProcess(cscPath, strArgs, _T(""), 60000, compOut, errMsg, _T(""));
	if (GetFileAttributes(strExe) == INVALID_FILE_ATTRIBUTES)
	{
		CString strOne;
		strOne.Format(_T("标准答案编译失败：%s"), (LPCTSTR)compOut);
		strOne.Replace(_T("\r\n"), _T(" "));
		strOne.Replace(_T("\n"), _T(" "));
		DeleteFile(strCs);
		return strOne;
	}

	// 3. 逐用例运行比对
	// 公开用例 + 隐藏用例都要自检，否则隐藏用例错了根本发现不了
	int nTotalCases = q.GetTotalCaseCount();
	if (nTotalCases <= 0
		|| (int)q.arrInput.GetSize() != (int)q.arrExpected.GetSize()
		|| (int)q.arrHiddenInput.GetSize() != (int)q.arrHiddenExpected.GetSize())
	{
		DeleteFile(strCs);
		DeleteFile(strExe);
		return _T("用例输入与期望数量不一致");
	}

	for (int i = 0; i < nTotalCases; i++)
	{
		BOOL bHidden = (i >= (int)q.arrInput.GetSize());
		int nIdx = bHidden ? (i - (int)q.arrInput.GetSize()) : i;

		CString strInput = bHidden ? q.arrHiddenInput.GetAt(nIdx) : q.arrInput.GetAt(nIdx);
		strInput.TrimRight();
		strInput += _T("\r\n");

		CString strOutput, strErr2;
		if (!RunVerifyProcess(strExe, _T(""), strInput, 5000, strOutput, strErr2, strDir))
		{
			CString strOne;
			strOne.Format(_T("%s%d运行失败:%s; "),
				bHidden ? _T("隐藏用例") : _T("用例"), nIdx + 1, (LPCTSTR)strErr2);
			strErr += strOne;
			continue;
		}

		CString strExp = NormalizeOutputText(
			bHidden ? q.arrHiddenExpected.GetAt(nIdx) : q.arrExpected.GetAt(nIdx));
		CString strAct = NormalizeOutputText(strOutput);
		if (strAct != strExp)
		{
			CString strOne;
			if (!strErr2.IsEmpty())
				strOne.Format(_T("%s%d 超时(可能死循环) 期望[%s]; "),
					bHidden ? _T("隐藏用例") : _T("用例"), nIdx + 1, (LPCTSTR)strExp);
			else
				strOne.Format(_T("%s%d 期望[%s] 实际[%s]; "),
					bHidden ? _T("隐藏用例") : _T("用例"), nIdx + 1,
					(LPCTSTR)strExp, (LPCTSTR)strAct);
			strErr += strOne;
		}
	}

	DeleteFile(strCs);
	DeleteFile(strExe);
	return strErr;
}

// ===== 自检核心：逐题验证，更新进度、支持取消 =====
BOOL CExamDlgProjDlg::VerifyBankCore(CString& strReport, int& nChecked, int& nPassed, int& nSkipped)
{
	nChecked = 0;
	nPassed = 0;
	nSkipped = 0;
	int nTotal = m_bank.GetCount();
	m_nVerifyTotal = nTotal;
	m_nVerifyDone = 0;

	for (int i = 0; i < nTotal; i++)
	{
		if (!m_bVerifying)
			break; // 用户点了取消

		QuestionInfo q = m_bank.GetAt(i);
		if (q.strStdAnswer.IsEmpty())
		{
			nSkipped++;
			m_nVerifyDone++;
			continue;
		}

		// 题库自检按钮已去掉，自检只在命令行模式(/verify)下使用：进度直接打到控制台
		_tprintf(_T("[%d/%d] %s ... "), i + 1, nTotal, (LPCTSTR)q.strTitle);

		nChecked++;
		CString strResult = VerifyOneQuestion(q);
		if (strResult.IsEmpty())
		{
			nPassed++;
			_tprintf(_T("通过\r\n"));
		}
		else
		{
			CString strOne;
			strOne.Format(_T("[%d] %s：%s\r\n"), q.nID, (LPCTSTR)q.strTitle, (LPCTSTR)strResult);
			strReport += strOne;
			_tprintf(_T("失败：%s\r\n"), (LPCTSTR)strResult);
		}
		m_nVerifyDone++;
	}
	return TRUE;
}

// ===== 命令行自检模式：ExamDlgProj.exe /verify =====
// 不打开界面，直接把结果打印到控制台并写入 verify_report.txt（开发时最快）
int CExamDlgProjDlg::HeadlessVerify()
{
	m_bSilent = TRUE;
	LoadQuestionBank();

	if (m_bank.GetCount() <= 0)
	{
		_tprintf(_T("题库为空或加载失败，请检查 exe 目录下的 questions.txt\r\n"));
		return -1;
	}

	CString strReport;
	int nChecked = 0, nPassed = 0, nSkipped = 0;
	m_bVerifying = TRUE;
	_tprintf(_T("开始自检，共 %d 道题……\r\n"), m_bank.GetCount());
	VerifyBankCore(strReport, nChecked, nPassed, nSkipped);

	CString strOut;
	strOut.Format(_T("题库自检报告\r\n题目总数：%d\r\n参与自检：%d（无标准答案跳过 %d）\r\n通过：%d\r\n失败：%d\r\n\r\n"),
		m_bank.GetCount(), nChecked, nSkipped, nPassed, nChecked - nPassed);
	strOut += strReport.IsEmpty() ? CString(_T("全部用例验证正确。\r\n")) : strReport;
	CString strReportPath = GetExeDir() + _T("verify_report.txt");
	WriteUtf8BomFile(strReportPath, strOut);

	_tprintf(_T("自检结束：通过 %d / %d，失败 %d 题\r\n报告文件：%s\r\n"),
		nPassed, nChecked, nChecked - nPassed, (LPCTSTR)strReportPath);
	return nChecked - nPassed;
}

// ===== 自检过程中禁止关闭窗口（防止界面对象被销毁后自检还在用） =====
void CExamDlgProjDlg::OnCancel()
{
	if (m_bVerifying)
		return;
	CDialogEx::OnCancel();
}
