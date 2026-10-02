#pragma once
#include "PublicDef.h"

// ===== 题库管理类 =====
// 题库来源：exe目录下的 questions.txt（UTF-8编码，加题只需改文件，不用重新编译）
// 文件格式说明：
//   [编号]          每题一个块，编号唯一
//   标题=xxx
//   难度=简单|中等|困难
//   内容=>>> ... <<<    （多行字段用 >>> 开头、<<< 结尾）
//   框架=>>> ... <<<
//   初始答案=（可留空）
//   参数类型=array|params   （array=fun接收整个数组；params=fun接收多个数值）
//   签名=public int fun(int[] a)
//   用例输入=>>> ... <<<    （每行一个用例，逗号分隔）
//   用例期望=>>> ... <<<
//   时长=15                 （可选，单位分钟；题库写了就用题库的，没写用界面填的）
//   真题=是                 （可选，标记为真题；老题库在标题里用括号写"真题"也认）
//   隐藏用例输入=>>> ... <<< （可选，判分时一并跑，但不把内容显示给学生，用于反"写死答案"）
//   隐藏用例期望=>>> ... <<<
class CQuestionBank
{
public:
	CQuestionBank();
	virtual ~CQuestionBank();

	// 从题库文件加载（UTF-8）。成功返回TRUE；文件不存在/格式错误返回FALSE
	BOOL LoadFromFile(const CString& strPath);

	// 加载内置默认题库（题库文件缺失时的兜底）
	void SetDefault();

	int GetCount() const { return (int)m_arrQuestions.GetSize(); }
	QuestionInfo GetAt(int nIndex) const { return m_arrQuestions.GetAt(nIndex); }

private:
	CArray<QuestionInfo, QuestionInfo&> m_arrQuestions;

	// UTF-8文件读取为Unicode文本
	static BOOL ReadFileUtf8(const CString& strPath, CString& strOut);
	// 按行拆分
	static void SplitLines(const CString& str, CStringArray& arr);
	// 把单个多行块解析成QuestionInfo
	static BOOL ParseBlock(const CStringArray& arrLines, int& nPos, QuestionInfo& qInfo);
};
