#pragma once
#include "PublicDef.h"
#include "QuestionBank.h"

class CPracticeDlg; // 前向声明

class CExamDlgProjDlg : public CDialogEx
{
public:
	CExamDlgProjDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_EXAMDLGPROJ_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	CListBox m_lstQuestion;     // 题库列表
	CButton m_chkSimple;         // 难度勾选：简单
	CButton m_chkMedium;         // 难度勾选：中等
	CButton m_chkHard;           // 难度勾选：困难
	CButton m_chkReal;           // 真题筛选：只看真题
	CButton m_btnAllLevel;       // 难度全选
	CStatic m_staticCount;       // 题目数量提示
	CStatic m_staticSel;         // 当前选中题目提示
	int m_nTime;                 // 考试时长
	CPracticeDlg* m_pPracticeDlg; // 做题对话框指针（非模态）

	CQuestionBank m_bank;          // 题库
	CArray<int, int> m_mapListToBank; // 列表index → 题库index（难度筛选后映射，同时写入item data）

	// ===== 窗口自适应布局 =====
	struct LayoutItem
	{
		UINT nID;        // 控件ID
		CRect rc;        // 初始位置（客户区坐标）
		BOOL bStretchW;  // 宽度跟随窗口
		BOOL bStretchH;  // 高度跟随窗口
		BOOL bMoveX;     // 水平跟随窗口移动（右对齐/居中控件）
		BOOL bMoveY;     // 整体跟随窗口下移
	};
	LayoutItem m_layout[20];
	int m_nLayoutCount;
	CRect m_rcInit;                // 初始客户区
	CRect m_rcMin;                 // 最小窗口尺寸（像素）
	BOOL m_bLayoutInit;

	BOOL m_bVerifying;             // 是否正在自检（FALSE=用户点了取消）
	BOOL m_bSilent;                // 命令行自检模式：不显示任何对话框
	BOOL m_bBankError;             // 题库文件损坏/打不开（停用开始练习）
	int m_nVerifyTotal;            // 自检总题数
	int m_nVerifyDone;             // 已完成题数

	void InitLayout();             // 采集控件初始位置
	void LoadQuestionBank();       // 加载题库（文件优先，失败用内置）
	void RefreshList();            // 按难度/真题勾选筛选刷新列表
	void UpdateCountText();        // 更新"共 X 题，当前显示 Y 题"
	void UpdateSelText();          // 更新"当前选中：xxx"
	BOOL IsLevelChecked(LPCTSTR lpszLevel); // 该难度是否被勾选（全不勾=全部）
	BOOL IsRealOnly();             // 是否勾选了"只看真题"
	void PumpUi();                 // 自检/判题期间处理界面消息，避免"卡死"
	void LaunchPractice(int nSel, int nTime); // 启动做题界面
	BOOL VerifyBankCore(CString& strReport, int& nChecked, int& nPassed, int& nSkipped); // 自检核心
	CString VerifyOneQuestion(const QuestionInfo& q); // 自检单题，返回错误描述（空=通过）
	int HeadlessVerify();          // 命令行 /verify 无界面自检，返回失败题数
	int GetBankIndexFromList(int nListIdx); // 列表行 → 题库下标

	afx_msg void OnBnClickedBtnStart();
	afx_msg void OnBnClickedBtnRandom();
	afx_msg void OnBnClickedBtnSupport();
	afx_msg void OnBnClickedBtnAbout();
	afx_msg void OnBnClickedBtnScoreTable();
	afx_msg void OnBnClickedChkLevel();
	afx_msg void OnBnClickedChkReal();
	afx_msg void OnBnClickedBtnAllLevel();
	afx_msg void OnLbnDblclkListQuestion();
	afx_msg void OnLbnSelchangeListQuestion();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	virtual void OnCancel();
};
