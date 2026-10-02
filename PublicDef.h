#pragma once
#include <afxwin.h>
#include <afxtempl.h>

// 题目信息结构体：主对话框传给做题对话框
struct QuestionInfo
{
    int nID;               // 题目编号（题库文件里的[编号]，唯一）
    CString strTitle;      // 题目标题
    CString strLevel;      // 难度：简单/中等/困难
    CString strContent;    // 题目描述（题目/示例/注意）
    CString strFrameCode;  // 左侧只读代码框架（含using/Main等固定代码）
    CString strCode;       // 答题区初始内容（一般留空，考生自己填）
    CString strSignature;  // fun函数签名，如 public int fun(int[] a)（黑盒判题已不依赖）
    CString strParamType;  // 参数类型：array=传入整个数组  params=传入多个数值（黑盒判题已不依赖）
    CString strStdAnswer;  // 标准答案（begin/end之间的代码，供"题库自检"验证用，可选）
    int nTimeMin;          // 考试时长（分钟）
    BOOL bTimeFromBank;    // 时长是否由题库的「时长」字段指定（否则用界面上填的值）
    CStringArray arrInput;     // 用例输入（每行一个用例，空格分隔）
    CStringArray arrExpected;  // 用例期望输出（每行一个用例）
    // 隐藏用例：判分时一并跑，但结果里不显示输入与期望值。
    // 写死公开用例答案的代码在隐藏用例上必然穿帮，是反"写死答案"最有效的手段。
    CStringArray arrHiddenInput;
    CStringArray arrHiddenExpected;
    BOOL bReal;            // 是否真题（题库里写了"真题=是"，或标题里带"真题"二字）

    QuestionInfo() : nID(0), nTimeMin(15), bTimeFromBank(FALSE), bReal(FALSE) {}

    // 判分要跑的用例总数（公开用例 + 隐藏用例）
    int GetTotalCaseCount() const
    {
        return (int)arrInput.GetSize() + (int)arrHiddenInput.GetSize();
    }

    // 显式拷贝构造/赋值（VS2026对含CStringArray成员的隐式拷贝判定为删除）
    QuestionInfo(const QuestionInfo& other)
    {
        CopyFrom(other);
    }
    QuestionInfo& operator=(const QuestionInfo& other)
    {
        if (this != &other)
            CopyFrom(other);
        return *this;
    }

private:
    void CopyFrom(const QuestionInfo& other)
    {
        nID = other.nID;
        strTitle = other.strTitle;
        strLevel = other.strLevel;
        strContent = other.strContent;
        strFrameCode = other.strFrameCode;
        strCode = other.strCode;
        strSignature = other.strSignature;
        strParamType = other.strParamType;
        strStdAnswer = other.strStdAnswer;
        nTimeMin = other.nTimeMin;
        bTimeFromBank = other.bTimeFromBank;
        arrInput.Copy(other.arrInput);
        arrExpected.Copy(other.arrExpected);
        arrHiddenInput.Copy(other.arrHiddenInput);
        arrHiddenExpected.Copy(other.arrHiddenExpected);
        bReal = other.bReal;
    }
};

// 获取exe所在目录（带结尾反斜杠）
inline CString GetExeDir()
{
	TCHAR szPath[MAX_PATH] = { 0 };
	GetModuleFileName(nullptr, szPath, MAX_PATH);
	CString strDir = szPath;
	int nPos = strDir.ReverseFind(_T('\\'));
	if (nPos >= 0)
		strDir = strDir.Left(nPos + 1);
	return strDir;
}

// ===== 题库文件加密（异或混淆，防止学生直接打开题库看到答案和全部用例）=====
// 发布版用 questions.dat：格式为 8 字节魔数 + 异或加密后的 UTF-8 文本；
// 开发时继续用 questions.txt 明文，方便加题、改题、跑题库自检。
#define BANK_MAGIC "EXBANK1"

inline void BankXorCrypt(BYTE* pData, int nLen)
{
	static const BYTE s_key[16] = {
		0x5A, 0x37, 0xC1, 0x9E, 0x24, 0x8B, 0x6D, 0xF0,
		0x13, 0xAA, 0x4C, 0x77, 0xE5, 0x08, 0x9B, 0x31
	};
	for (int i = 0; i < nLen; i++)
		pData[i] ^= s_key[i % 16];
}
