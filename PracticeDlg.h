#pragma once
#include "PublicDef.h"

// ===== 判题线程 → 主窗口 的自定义消息 =====
// 判题放在工作线程里跑，主线程继续处理消息，界面不会假死
// （原来在主线程里逐个用例 WaitForSingleObject，8 个用例 × 5 秒最坏 40 秒全卡住）
#define WM_JUDGE_PROGRESS   (WM_USER + 100)   // wParam=正在跑第几个用例  lParam=总用例数
#define WM_JUDGE_DONE       (WM_USER + 101)   // 判题结束
#define WM_COLLAPSE_FRAME_SEL (WM_USER + 102) // 折叠左侧只读框架的选择（RichEdit 拿到焦点会全选）

// ============ 判题结果对话框（显示分数 + 用例明细 + 继续做题/返回主界面）============
class CScoreDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CScoreDlg)

public:
	CScoreDlg(int nScore, int nTotal, const CString& strDetail, CWnd* pParent = nullptr);
	virtual ~CScoreDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SCORE_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

public:
	int m_nScore;          // 本题得分
	int m_nTotal;          // 满分
	CString m_strDetail;   // 用例明细
	CStatic m_staticScore; // 分数显示
	CStatic m_staticDetail;// 用例明细显示
	CFont m_fontScore;     // 大号分数字体

	afx_msg void OnBnClickedBtnBackFromScore();
};

// ============ 做题对话框 ============
class CPracticeDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CPracticeDlg)

public:
	// 自定义构造函数：接收题目参数
	CPracticeDlg(CWnd* pParent = nullptr, QuestionInfo info = {});
	virtual ~CPracticeDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PRACTICE_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	// ===== 动态布局（窗口可拉伸）=====
	struct CtrlRect
	{
		UINT nID;    // 控件ID
		int x, y, w, h; // 初始像素位置（客户区）
		BOOL bFixedY;   // 顶部固定控件：纵向不缩放
	};
	CRect m_rcInit;              // 初始客户区
	CtrlRect m_ctrlRects[16];    // 各控件初始位置
	int m_nCtrlCount;            // 控件数量
	BOOL m_bLayoutInit;          // 布局是否已采集

	void InitLayout();           // 采集控件初始布局

	QuestionInfo m_qInfo;        // 题目信息
	int m_nRemainSec;            // 剩余秒数
	BOOL m_bJudging;             // 是否正在提交判题（判题时禁止关闭窗口，防止对象被销毁）
	CString m_strOrigFrame;      // 原始代码框架（重置用）
	CString m_strSavePath;       // answer.txt（纯答案）
	CString m_strCsPath;         // answer.cs（完整代码，供VS打开）
	CString m_strJudgeCsPath;    // answer_judge.cs（判题用）
	CString m_strExePath;        // answer_judge.exe（判题编译结果）
	CString m_strJudgeDir;       // 判题临时目录（隔离工作目录）
	CString m_strProjDir;        // VS调试工程目录（exam\）
	CString m_strCsprojPath;     // exam.csproj
	CString m_strProgramCsPath;  // Program.cs
	CString m_strSlnPath;        // exam.sln
	CString m_strDraftDir;       // 草稿目录（每道题一个文件，退出程序也不删）
	CString m_strDraftPath;      // 本题草稿文件：draft\题号.txt
	CString m_strLastSubmitted;  // 最近一次提交判题时的答案（判断返回时要不要二次确认）
	CRichEditCtrl m_richFrame;   // 左侧只读代码框架
	CRichEditCtrl m_richCode;    // 右侧答题区域

	// 左侧只读框架禁止复制的子类化（实现见 PracticeDlg.cpp 的 s_FrameSubclassProc）
	static LRESULT CALLBACK s_FrameSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
		LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	BOOL m_bFrameSubclassed;     // 子类化是否已挂上
	CStatic m_staticTimer;       // 倒计时显示
	CFont m_fontCode;            // 代码区等宽字体（成员变量，随对话框销毁自动释放）

	// ===== 判题线程状态 =====
	CWinThread* m_pJudgeThread;      // 判题线程（m_bAutoDelete=FALSE，由我们自己回收）
	volatile LONG m_bJudgeCancel;    // 用户点了【取消判题】
	volatile LONG m_nJudgeDone;      // 已开始的用例序号（只用于显示进度）
	BOOL m_bJudgeCompileOk;          // 编译是否成功
	int m_nJudgeScore;               // 工作线程算出的分数
	CString m_strJudgeDetail;        // 工作线程填的用例明细
	CString m_strJudgeErr;           // 工作线程产生的错误信息
	CString m_strSubmittedCode;      // 本次提交的答案原文（写成绩记录用）

	void StartJudgeThread(const CString& strCode); // 启动判题线程
	void DoJudgeInThread();                        // 线程体：编译 + 黑盒判题
	void RequestCancelJudge();                     // 请求取消判题
	static UINT AFX_CDECL JudgeThreadProc(LPVOID pParam);
	afx_msg LRESULT OnJudgeProgress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnJudgeDone(WPARAM wParam, LPARAM lParam);

	afx_msg void OnEnSetfocusRichFrame();             // 左侧只读框架拿到焦点时折叠选择
	afx_msg LRESULT OnCollapseFrameSel(WPARAM wParam, LPARAM lParam);

	void UpdateTimerText();                  // 更新倒计时（HH:MM:SS）
	void HighlightMarker(LPCTSTR lpszMarker); // 高亮begin/end标记（蓝色粗体）
	void InitSavePath();                     // 初始化文件路径
	void CleanupTempFiles();                 // 清理判题/保存产生的临时文件
	void DeleteDirRecursive(const CString& strDir); // 递归删除目录
	CString GetAnswerText();                 // 获取答题区内容
	CString ComposeFullCode(const CString& answer);  // 框架+答案合并成完整代码
	BOOL CheckAnswerLogic(const CString& answer, CString& errMsg);    // 检查逻辑（危险代码/框架完整性）
	BOOL WriteFileANSI(const CString& path, const CString& content); // 写ANSI文本文件
	BOOL ReadFileAnsi(const CString& path, CString& strOut);         // 读ANSI文本文件（和 WriteFileANSI 配对）
	BOOL WriteFileUtf8Bom(const CString& path, const CString& content); // 写UTF-8 BOM文本文件
	void SaveAnswerFiles(const CString& answer);     // 保存答案并更新左侧
	void LoadDraft();                                // 载入本题草稿（若有，自动填回答题区）
	BOOL SaveDraft();                                // 保存本题草稿；返回是否真写了草稿（已提交判分的题会清掉草稿）
	void DeleteDraft();                              // 删除本题草稿
	BOOL HasUnsavedContent();                        // 答题区是否有"还没提交过"的内容
	void DoLeavePractice(BOOL bAskConfirm);          // 离开做题界面的统一出口（二次确认+自动存草稿）
	void WriteScoreRecord(const CString& answer, int nScore, int nTotal); // 写成绩记录
	BOOL CreateDebugProject(const CString& answer);  // 生成可运行的VS工程(csproj+sln+Program.cs)
	BOOL CompileAndRun(const CString& csPath, CString& errMsg); // csc编译（黑盒判题：只编译不运行）
	int JudgeByCases(CString& strDetail, CString& errMsg);  // 黑盒判题：逐用例喂输入比对输出（每用例3分）
	BOOL RunProcessWithInput(const CString& exe, const CString& args, const CString& input, CString& output, CString& errMsg, const CString& workDir, DWORD dwTimeoutMs = 5000); // 创建进程：写stdin+捕获输出
	void SplitLines(const CString& str, CStringArray& arr); // 按行拆分

	afx_msg void OnBnClickedBtnSubmit();
	afx_msg void OnBnClickedBtnSave();
	afx_msg void OnBnClickedBtnReset();
	afx_msg void OnBnClickedBtnRestore();
	afx_msg void OnBnClickedBtnDebug();
	afx_msg void OnBnClickedBtnBack();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual void OnCancel();
};
