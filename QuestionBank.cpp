#include "pch.h"
#include "framework.h"
#include "QuestionBank.h"

CQuestionBank::CQuestionBank()
{
}

CQuestionBank::~CQuestionBank()
{
}

// 读取UTF-8文本文件（支持BOM）
BOOL CQuestionBank::ReadFileUtf8(const CString& strPath, CString& strOut)
{
	CFile f;
	if (!f.Open(strPath, CFile::modeRead))
		return FALSE;

	ULONGLONG nLen64 = f.GetLength();
	if (nLen64 == 0)
	{
		f.Close();
		strOut.Empty();
		return TRUE;
	}
	if (nLen64 > 8 * 1024 * 1024) // 题库最大8MB，防异常文件
		nLen64 = 8 * 1024 * 1024;

	int nLen = (int)nLen64;
	char* pBuf = new char[nLen + 1];
	f.Read(pBuf, nLen);
	f.Close();
	pBuf[nLen] = 0;

	// 题库文件：加密版(.dat，带魔数)和明文版(.txt，带BOM)都支持
	int nOffset = 0;
	if (nLen >= 8 && memcmp(pBuf, BANK_MAGIC, 8) == 0)
	{
		BankXorCrypt((BYTE*)pBuf + 8, nLen - 8); // 解密
		nOffset = 8;
	}
	else if (nLen >= 3 && (unsigned char)pBuf[0] == 0xEF
		&& (unsigned char)pBuf[1] == 0xBB && (unsigned char)pBuf[2] == 0xBF)
	{
		nOffset = 3; // 跳过UTF-8 BOM
	}

	// UTF-8转Unicode
	int nWide = MultiByteToWideChar(CP_UTF8, 0, pBuf + nOffset, nLen - nOffset, nullptr, 0);
	if (nWide > 0)
	{
		MultiByteToWideChar(CP_UTF8, 0, pBuf + nOffset, nLen - nOffset,
			strOut.GetBuffer(nWide), nWide);
		strOut.ReleaseBuffer();
		// 去掉解密后可能残留的BOM字符
		if (!strOut.IsEmpty() && strOut[0] == 0xFEFF)
			strOut = strOut.Mid(1);
	}
	delete[] pBuf;
	return TRUE;
}

void CQuestionBank::SplitLines(const CString& str, CStringArray& arr)
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

// 解析一个题块：从 [编号] 行开始，直到下一个 [ 或文件结束
BOOL CQuestionBank::ParseBlock(const CStringArray& arrLines, int& nPos, QuestionInfo& qInfo)
{
	// 当前行应该是 [编号]
	CString strLine = arrLines[nPos];
	strLine.Trim();
	if (strLine.GetLength() < 3 || strLine[0] != _T('['))
		return FALSE;
	qInfo.nID = _ttoi(strLine.Mid(1));

	nPos++; // 跳过 [编号] 行

	CString strKey, strValue;
	BOOL bInMulti = FALSE; // 是否在多行字段中

	for (; nPos < (int)arrLines.GetSize(); nPos++)
	{
		CString line = arrLines[nPos];
		line.TrimRight(_T('\r'));

		// 多行字段结束标记
		if (bInMulti)
		{
			// 注意：CString::Trim() 会原地修改，这里必须先复制再判断，
			// 否则收集到的代码会丢失行首缩进（生成的框架会变成没缩进的）
			CString strMarker = line;
			strMarker.Trim();
			if (strMarker == _T("<<<"))
			{
				bInMulti = FALSE;
				continue;
			}
			// 收集多行内容
			CString* pDest = nullptr;
			if (strKey == _T("内容")) pDest = &qInfo.strContent;
			else if (strKey == _T("框架")) pDest = &qInfo.strFrameCode;
			else if (strKey == _T("标准答案")) pDest = &qInfo.strStdAnswer;
			else if (strKey == _T("用例输入")) pDest = nullptr; // 单独处理
			else if (strKey == _T("用例期望")) pDest = nullptr;

			if (strKey == _T("用例输入"))
				qInfo.arrInput.Add(line);
			else if (strKey == _T("用例期望"))
				qInfo.arrExpected.Add(line);
			else if (pDest)
			{
				if (!pDest->IsEmpty())
					*pDest += _T("\r\n");
				*pDest += line;
			}
			continue;
		}

		// 下一个题块开始
		CString lineTrim = line.Trim();
		if (!lineTrim.IsEmpty() && lineTrim[0] == _T('['))
		{
			// 回退一行：让外层循环的 i++ 后正好停在 [ 行，继续解析下一题
			nPos--;
			return TRUE;
		}

		// 注释或空行
		if (lineTrim.IsEmpty() || lineTrim[0] == _T('#'))
			continue;

		// 键=值 或 键=>>>
		int nEq = line.Find(_T('='));
		if (nEq <= 0)
			continue;

		strKey = line.Left(nEq);
		strKey.Trim();
		strValue = line.Mid(nEq + 1);
		strValue.Trim();

		if (strValue == _T(">>>"))
		{
			// 用例输入/期望（含隐藏用例）：支持空行分隔的多行用例；
			// 无空行则每行一个用例（老格式，自动兼容）
			if (strKey == _T("用例输入") || strKey == _T("用例期望")
				|| strKey == _T("隐藏用例输入") || strKey == _T("隐藏用例期望"))
			{
				BOOL bIsInput = (strKey == _T("用例输入") || strKey == _T("隐藏用例输入"));
				BOOL bHidden = (strKey == _T("隐藏用例输入") || strKey == _T("隐藏用例期望"));
				CStringArray* pArr = bHidden
					? (bIsInput ? &qInfo.arrHiddenInput : &qInfo.arrHiddenExpected)
					: (bIsInput ? &qInfo.arrInput : &qInfo.arrExpected);

				// 先收集块内所有行（到 <<< 为止），判断是否含空行
				CStringArray arrBlock;
				BOOL bHasBlank = FALSE;
				int nScan = nPos + 1;
				for (; nScan < (int)arrLines.GetSize(); nScan++)
				{
					CString l2 = arrLines[nScan];
					l2.TrimRight(_T('\r'));
					if (l2.Trim() == _T("<<<"))
						break;
					arrBlock.Add(l2);
					if (l2.Trim().IsEmpty())
						bHasBlank = TRUE;
				}
				if (nScan < (int)arrLines.GetSize())
					nPos = nScan; // 让外层 nPos++ 正好跳过 <<<

				if (!bHasBlank)
				{
					// 老格式：每行一个用例
							for (int k = 0; k < (int)arrBlock.GetSize(); k++)
							{
								CString t2 = arrBlock.GetAt(k);
								t2.Trim();
								if (!t2.IsEmpty())
									pArr->Add(arrBlock.GetAt(k));
							}
				}
				else
				{
					// 新格式：空行分隔用例，一个用例的多行用 \n 连接（判题时一次喂入标准输入）
					CString strCase;
					for (int k = 0; k < (int)arrBlock.GetSize(); k++)
					{
						CString l2 = arrBlock.GetAt(k);
						if (l2.Trim().IsEmpty())
						{
							if (!strCase.IsEmpty()) { pArr->Add(strCase); strCase.Empty(); }
							continue;
						}
						if (!strCase.IsEmpty()) strCase += _T("\n");
						strCase += l2;
					}
					if (!strCase.IsEmpty())
						pArr->Add(strCase);
				}
			}
			else
			{
				bInMulti = TRUE; // 内容/框架/标准答案：逐行收集
			}
			continue;
		}

		// 单行字段
		if (strKey == _T("标题"))
		{
			qInfo.strTitle = strValue;
			// 老题库兼容：标题里用括号标注的「真题」也认（例如"秒数转换时分秒（简单·真题）"）。
			// 收紧成"必须落在全角括号内"，避免"真题改编"这类标题被误判成真题。
			// 新加题请统一写 真题=是，识别路径只有一条最不容易乱。
			int nRealPos = strValue.Find(_T("真题"));
			if (nRealPos >= 0)
			{
				int nLeftParen = strValue.ReverseFind(_T('（'));
				int nRightParen = strValue.Find(_T('）'));
				if (nLeftParen >= 0 && nLeftParen < nRealPos
					&& nRightParen > nRealPos)
					qInfo.bReal = TRUE;
			}
		}
		else if (strKey == _T("难度")) qInfo.strLevel = strValue;
		else if (strKey == _T("初始答案")) qInfo.strCode = strValue;
		else if (strKey == _T("参数类型")) qInfo.strParamType = strValue;
		else if (strKey == _T("时长"))
		{
			// 题目自带时长（分钟）。原实现根本没解析这个键，字段等于摆设；
			// 现在解析出来并置 bTimeFromBank，LaunchPractice 会优先用题库的。
			int nMin = _ttoi(strValue);
			if (nMin > 0 && nMin <= 600)
			{
				qInfo.nTimeMin = nMin;
				qInfo.bTimeFromBank = TRUE;
			}
		}
		else if (strKey == _T("签名")) qInfo.strSignature = strValue;
		else if (strKey == _T("真题"))
		{
			// 显式标记：真题=是 或 真题=1 都算真题（大小写不敏感）
			CString strFlag = strValue;
			strFlag.Trim();
			strFlag.MakeLower();
			if (strFlag == _T("是") || strFlag == _T("1") || strFlag == _T("true") || strFlag == _T("y"))
				qInfo.bReal = TRUE;
		}
	}

	return TRUE;
}

BOOL CQuestionBank::LoadFromFile(const CString& strPath)
{
	CString strText;
	if (!ReadFileUtf8(strPath, strText))
		return FALSE;

	CStringArray arrLines;
	SplitLines(strText, arrLines);

	m_arrQuestions.RemoveAll();

	for (int i = 0; i < (int)arrLines.GetSize(); i++)
	{
		CString line = arrLines[i];
		line.Trim();
		if (line.GetLength() >= 3 && line[0] == _T('['))
		{
			QuestionInfo qInfo;
			if (ParseBlock(arrLines, i, qInfo) && !qInfo.strTitle.IsEmpty())
			{
				// 公开用例数量必须一致且>0，否则跳过
				// 隐藏用例是可选的，但一旦写了就必须成对，否则把隐藏用例整组丢掉
				// （宁可少一层反作弊，也不能因为题库写错就判分错乱）
				if (qInfo.arrHiddenInput.GetSize()
					!= qInfo.arrHiddenExpected.GetSize())
				{
					qInfo.arrHiddenInput.RemoveAll();
					qInfo.arrHiddenExpected.RemoveAll();
				}
				if (qInfo.arrInput.GetSize() > 0
					&& qInfo.arrInput.GetSize() == qInfo.arrExpected.GetSize())
					m_arrQuestions.Add(qInfo);
			}
		}
	}

	return m_arrQuestions.GetSize() > 0;
}

// 内置默认题库（题库文件缺失时的兜底，与示例questions.txt一致）
void CQuestionBank::SetDefault()
{
	m_arrQuestions.RemoveAll();

	QuestionInfo q1;
	q1.nID = 1;
	q1.strTitle = _T("统计有序数组不重复元素个数");
	q1.strLevel = _T("简单");
	q1.strContent = _T("七、程序设计题（共12分）\r\n\r\n题目：编写函数fun，统计数组中数值的个数，重复元素只按1个计数。\r\n\r\n示例：数组 0 0 0 2 2 2 5 9 10，共有五个不同数值，输出 5。\r\n\r\n注意：\r\n1. 请在答题区填写函数fun的方法体（代码框架已在左侧，无需重复书写）。\r\n2. 请勿改动主函数Main和其它函数中的任何内容。\r\n3. 输入的一行中多个数用空格分隔。");
	q1.strFrameCode = _T("using System;\r\nusing System.Collections.Generic;\r\nusing System.Linq;\r\nusing System.Text;\r\nusing System.IO;\r\n\r\nnamespace prog\r\n{\r\n    public class Program\r\n    {\r\n        public int fun(int[] a)\r\n        {\r\n            /************begin************/\r\n\r\n            /************end************/\r\n        }\r\n\r\n        static void Main(string[] args)\r\n        {\r\n            Program p = new Program();\r\n            string[] s = Console.ReadLine().Split(' ');\r\n            int[] a = new int[s.Length];\r\n            for (int i = 0; i < s.Length; i++)\r\n                a[i] = int.Parse(s[i]);\r\n            Console.WriteLine(p.fun(a));\r\n        }\r\n    }\r\n}");
	q1.arrInput.Add(_T("0 0 0 2 2 2 5 9 10"));
	q1.arrInput.Add(_T("1 1 2 3 3 3"));
	q1.arrInput.Add(_T("5 5 5"));
	q1.arrInput.Add(_T("1 2 3 4 5"));
	q1.arrInput.Add(_T("7 7 7 7 7 7 7 7 7 7"));
	q1.arrExpected.Add(_T("5"));
	q1.arrExpected.Add(_T("3"));
	q1.arrExpected.Add(_T("1"));
	q1.arrExpected.Add(_T("5"));
	q1.arrExpected.Add(_T("1"));
	q1.strStdAnswer = _T("int count = 0;\r\nfor (int i = 0; i < a.Length; i++)\r\n{\r\n    bool dup = false;\r\n    for (int j = 0; j < i; j++)\r\n        if (a[i] == a[j]) { dup = true; break; }\r\n    if (!dup) count++;\r\n}\r\nreturn count;");
	m_arrQuestions.Add(q1);

	QuestionInfo q2;
	q2.nID = 2;
	q2.strTitle = _T("计算 a+aa+aaa+... 的和");
	q2.strLevel = _T("中等");
	q2.strContent = _T("七、程序设计题（共12分）\r\n\r\n题目：编写函数fun，计算出a+aa+aaa+…+aa…a（n个a）的和，a和n由键盘输入。\r\n\r\n示例：输入 a=3 n=4，输出 3702（即3+33+333+3333）。\r\n\r\n注意：\r\n1. 请在答题区填写函数fun的方法体（代码框架已在左侧，无需重复书写）。\r\n2. 请勿改动主函数Main和其它函数中的任何内容。\r\n3. 输入的一行中 a 和 n 用空格分隔。");
	q2.strFrameCode = _T("using System;\r\nusing System.Collections.Generic;\r\nusing System.Linq;\r\nusing System.Text;\r\nusing System.IO;\r\n\r\nnamespace prog\r\n{\r\n    public class Program\r\n    {\r\n        public long fun(int a, int n)\r\n        {\r\n            /************begin************/\r\n\r\n            /************end************/\r\n        }\r\n\r\n        static void Main(string[] args)\r\n        {\r\n            Program p = new Program();\r\n            string[] s = Console.ReadLine().Split(' ');\r\n            int a = int.Parse(s[0]);\r\n            int n = int.Parse(s[1]);\r\n            Console.WriteLine(p.fun(a, n));\r\n        }\r\n    }\r\n}");
	q2.arrInput.Add(_T("3 4"));
	q2.arrInput.Add(_T("2 3"));
	q2.arrInput.Add(_T("5 2"));
	q2.arrInput.Add(_T("1 5"));
	q2.arrInput.Add(_T("9 3"));
	q2.arrExpected.Add(_T("3702"));
	q2.arrExpected.Add(_T("246"));
	q2.arrExpected.Add(_T("60"));
	q2.arrExpected.Add(_T("12345"));
	q2.arrExpected.Add(_T("1107"));
	q2.strStdAnswer = _T("long sum = 0;\r\nlong term = 0;\r\nfor (int i = 0; i < n; i++)\r\n{\r\n    term = term * 10 + a;\r\n    sum += term;\r\n}\r\nreturn sum;");
	m_arrQuestions.Add(q2);

	QuestionInfo q3;
	q3.nID = 3;
	q3.strTitle = _T("判断一个数是否为素数");
	q3.strLevel = _T("困难");
	q3.strContent = _T("七、程序设计题（共12分）\r\n\r\n题目：编写函数fun，判断一个数是否为素数，是返回1，不是返回0。\r\n\r\n示例：输入 7，输出 1；输入 10，输出 0。\r\n\r\n注意：\r\n1. 请在答题区填写函数fun的方法体（代码框架已在左侧，无需重复书写）。\r\n2. 请勿改动主函数Main和其它函数中的任何内容。");
	q3.strFrameCode = _T("using System;\r\nusing System.Collections.Generic;\r\nusing System.Linq;\r\nusing System.Text;\r\nusing System.IO;\r\n\r\nnamespace prog\r\n{\r\n    public class Program\r\n    {\r\n        public int fun(int n)\r\n        {\r\n            /************begin************/\r\n\r\n            /************end************/\r\n        }\r\n\r\n        static void Main(string[] args)\r\n        {\r\n            Program p = new Program();\r\n            int n = int.Parse(Console.ReadLine());\r\n            Console.WriteLine(p.fun(n));\r\n        }\r\n    }\r\n}");
	q3.arrInput.Add(_T("7"));
	q3.arrInput.Add(_T("10"));
	q3.arrInput.Add(_T("2"));
	q3.arrInput.Add(_T("97"));
	q3.arrInput.Add(_T("1"));
	q3.arrExpected.Add(_T("1"));
	q3.arrExpected.Add(_T("0"));
	q3.arrExpected.Add(_T("1"));
	q3.arrExpected.Add(_T("1"));
	q3.arrExpected.Add(_T("0"));
	q3.strStdAnswer = _T("if (n < 2) return 0;\r\nfor (int i = 2; i * i <= n; i++)\r\n    if (n % i == 0) return 0;\r\nreturn 1;");
	m_arrQuestions.Add(q3);
}
