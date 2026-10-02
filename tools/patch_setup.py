# -*- coding: utf-8 -*-
"""Setup.cpp 补丁：
   A. 默认/示例目录 D: -> C:（和界面版 DEFAULT_DIR 统一）
   B. 卸载时一并清掉程序自己产生的过程文件（draft\\ exam\\ answer.txt answer.cs
      以及 %LOCALAPPDATA%\\对口升学练习系统\\temp\\）
"""
import sys, hashlib

P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\安装程序\Setup.cpp"
raw = open(P, "rb").read()
assert raw[:3] == b"\xef\xbb\xbf", "Setup.cpp 应该是 UTF-8 BOM"
assert raw.count(b"\r\n") == 0, "Setup.cpp 应该是纯 LF（不该有 CRLF）"
body = raw[3:].decode("utf-8")
before_size = len(raw)
before_hash = hashlib.sha256(raw).hexdigest()

log = []

# ---------------------------------------------------------------- A. D: -> C:
n = body.count("D:\\\\对口升学练习系统")
assert n == 6, "示例目录 D:\\\\对口升学练习系统 期望 6 处，实际 %d" % n
body = body.replace("D:\\\\对口升学练习系统", "C:\\\\对口升学练习系统")
log.append("A1 示例目录 D:->C: 替换 %d 处" % n)

n = body.count('/D="D:\\\\我的 目录"')
assert n == 1, "注释里的 /D 示例期望 1 处，实际 %d" % n
body = body.replace('/D="D:\\\\我的 目录"', '/D="C:\\\\我的 目录"')
log.append("A2 /D 注释示例 D:->C: 替换 1 处")

assert body.count("D:\\\\对口升学练习系统") == 0 and body.count("D:\\\\我的 目录") == 0

# ------------------------------------------------- B1. 插入 helper（放在 DeleteInstalledOnly 之前）
anchor = '// 按清单删"本程序自己的文件"。\n'
assert body.count(anchor) == 1, "helper 插入锚点不唯一"

HELPER = r'''// ============ 程序"运行时自己产生"的过程文件（卸载要一并清掉）============
// draft\                 草稿目录（每道题一个 <题号>.txt）
// exam\                  【VS调试运行】生成的工程目录（exam.sln / exam.csproj / obj / bin …）
// answer.txt / answer.cs 保存答案时写在程序目录里的临时文件
// 这些不是安装时装进去的，却是本程序运行时生成的。以前卸载不管它们，
// 用户卸完发现目录里还留着 draft\ 、exam\ ，会以为没卸干净。
// ★ 只认这几个确定的名字，目录里其它任何东西一律不碰（和"绝不删用户文件"一个原则）。
static const WCHAR* const k_pOwnedArtifacts[] = {
	L"draft",
	L"exam",
	L"answer.txt",
	L"answer.cs",
};
#define OWNED_ARTIFACT_COUNT (int)(sizeof(k_pOwnedArtifacts) / sizeof(k_pOwnedArtifacts[0]))

// 往"删不掉的清单"里追加一个名字（重复的不再加；放不下就不写，绝不越界）
static void AppendLeftName(WCHAR* pszLeftList, int cchLeft, const WCHAR* pszName)
{
	if (pszLeftList == NULL || cchLeft <= 0 || pszName == NULL)
		return;
	if (wcsstr(pszLeftList, pszName) != NULL)
		return;
	size_t nCur = wcslen(pszLeftList);
	size_t nAdd = wcslen(pszName) + ((nCur > 0) ? 1 : 0);
	if (nCur + nAdd + 1 >= (size_t)cchLeft)
		return;
	if (nCur > 0)
		wcscat_s(pszLeftList, cchLeft, L"、");
	wcscat_s(pszLeftList, cchLeft, pszName);
}

// 递归删一个目录（先清空、再删目录本身）。删不掉的登记"下次开机自动删除"。
// 返回删不掉的文件个数；目录里还有东西时 RemoveDirectoryW 自然失败，目录会保留。
static int DeleteDirRecursiveW(const WCHAR* pszDir)
{
	WCHAR szPat[MAX_PATH];
	swprintf_s(szPat, MAX_PATH, L"%s\\*", pszDir);
	int nLeft = 0;
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(szPat, &fd);
	if (h != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			WCHAR szChild[MAX_PATH];
			swprintf_s(szChild, MAX_PATH, L"%s\\%s", pszDir, fd.cFileName);
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				nLeft += DeleteDirRecursiveW(szChild);
			}
			else
			{
				SetFileAttributesW(szChild, FILE_ATTRIBUTE_NORMAL);
				if (!DeleteFileRetry(szChild))
				{
					MoveFileExW(szChild, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
					nLeft++;
				}
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}
	SetFileAttributesW(pszDir, FILE_ATTRIBUTE_NORMAL);
	RemoveDirectoryW(pszDir);
	return nLeft;
}

// 清掉本程序运行时留下的过程文件。
// 除了安装目录里的 draft\ / exam\ / answer.txt / answer.cs，
// 还有 %LOCALAPPDATA%\对口升学练习系统\ 下的判题编译产物（那个目录整个是本程序建的）。
// 删不掉的把（顶层）名字追加进 pszLeftList，让调用方如实汇报，不谎报成功。
static void DeleteOwnedArtifacts(const WCHAR* pszDir, int* pnLeft,
	WCHAR* pszLeftList, int cchLeft)
{
	int nLeft = 0;

	// 1) 安装目录里的过程文件
	for (int i = 0; i < OWNED_ARTIFACT_COUNT; i++)
	{
		WCHAR szFull[MAX_PATH];
		swprintf_s(szFull, MAX_PATH, L"%s\\%s", pszDir, k_pOwnedArtifacts[i]);
		DWORD dwAttr = GetFileAttributesW(szFull);
		if (dwAttr == INVALID_FILE_ATTRIBUTES)
			continue;
		int nThis = 0;
		if ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			nThis = DeleteDirRecursiveW(szFull);
		}
		else
		{
			SetFileAttributesW(szFull, FILE_ATTRIBUTE_NORMAL);
			if (!DeleteFileRetry(szFull))
			{
				MoveFileExW(szFull, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
				nThis = 1;
			}
		}
		if (nThis > 0)
		{
			nLeft += nThis;
			AppendLeftName(pszLeftList, cchLeft, k_pOwnedArtifacts[i]);
		}
	}

	// 2) %LOCALAPPDATA%\对口升学练习系统\ —— 判题要写 .cs 再编 .exe，产物都在这。
	//    只删确定是自己建的 temp\ 子目录；父目录空了才顺手删掉，绝不递归清人家的目录。
	WCHAR szLocal[MAX_PATH] = { 0 };
	if (GetEnvironmentVariableW(L"LOCALAPPDATA", szLocal, MAX_PATH) > 0)
	{
		WCHAR szTempDir[MAX_PATH], szRoot[MAX_PATH];
		swprintf_s(szRoot, MAX_PATH, L"%s\\对口升学练习系统", szLocal);
		swprintf_s(szTempDir, MAX_PATH, L"%s\\temp", szRoot);
		if (GetFileAttributesW(szTempDir) != INVALID_FILE_ATTRIBUTES)
		{
			int nThis = DeleteDirRecursiveW(szTempDir);
			if (nThis > 0)
			{
				nLeft += nThis;
				AppendLeftName(pszLeftList, cchLeft, L"%LOCALAPPDATA%\\对口升学练习系统\\temp");
			}
		}
		RemoveDirectoryW(szRoot);   // 空了才删得掉；里面还有别的东西就保留
	}

	if (pnLeft != NULL)
		*pnLeft += nLeft;
}

'''
body = body.replace(anchor, HELPER + anchor, 1)
log.append("B1 helper 已插入")

# ------------------------------------------------- B2. 四处调用点后面挂上清理
lines = body.split("\n")
out = []
hits_szOld = 0
hits_szDir = 0
for ln in lines:
    out.append(ln)
    s = ln.strip()
    if s == "DeleteInstalledOnly(szOld, TRUE, &nLeft, szLeft, MAX_PATH * 3);":
        indent = ln[:len(ln) - len(ln.lstrip("\t"))]
        out.append(indent + "// 程序运行时产生的草稿 draft\\、VS调试工程 exam\\、临时答案一并清掉")
        out.append(indent + "DeleteOwnedArtifacts(szOld, &nLeft, szLeft, MAX_PATH * 3);")
        hits_szOld += 1
    elif s == "DeleteInstalledOnly(szDir, TRUE, &nLeft, szLeft, MAX_PATH * 3);":
        indent = ln[:len(ln) - len(ln.lstrip("\t"))]
        out.append(indent + "// 程序运行时产生的草稿 draft\\、VS调试工程 exam\\、临时答案一并清掉")
        out.append(indent + "DeleteOwnedArtifacts(szDir, &nLeft, szLeft, MAX_PATH * 3);")
        hits_szDir += 1
assert hits_szOld == 2, "szOld 调用点期望 2 处，实际 %d" % hits_szOld
assert hits_szDir == 2, "szDir 调用点期望 2 处，实际 %d" % hits_szDir
body = "\n".join(out)
log.append("B2 4 处调用点已挂上 DeleteOwnedArtifacts")

# ------------------------------------------------- C. 界面文案跟上实际行为
old_msg = (
    '\t\t\tL"确定要卸载【%s】吗？\\r\\n\\r\\n"\n'
    '\t\t\tL"安装目录：%s\\r\\n"\n'
    '\t\t\tL"这里只删本程序装进去的文件；\\r\\n"\n'
    '\t\t\tL"你自己放进这个文件夹的其它文件会原样保留。\\r\\n%s",\n'
)
new_msg = (
    '\t\t\tL"确定要卸载【%s】吗？\\r\\n\\r\\n"\n'
    '\t\t\tL"安装目录：%s\\r\\n"\n'
    '\t\t\tL"只删本程序自己的东西：装进去的 6 个文件，以及程序运行时\\r\\n"\n'
    '\t\t\tL"产生的草稿（draft）、VS调试工程（exam）和临时答案文件；\\r\\n"\n'
    '\t\t\tL"你自己放进这个文件夹的其它文件会原样保留。\\r\\n%s",\n'
)
assert body.count(old_msg) == 1, "卸载确认文案锚点不唯一"
body = body.replace(old_msg, new_msg, 1)
log.append("C1 卸载确认文案已更新")

old_ask = (
    '\t\t\t\t\t\tL"（装在这里也能正常卸载：卸载只删本程序装进去的 6 个文件，\\r\\n"\n'
    '\t\t\t\t\t\tL"　这个文件夹里原有的东西不会被删掉。）",\n'
)
new_ask = (
    '\t\t\t\t\t\tL"（装在这里也能正常卸载：只删本程序自己的文件，以及程序\\r\\n"\n'
    '\t\t\t\t\t\tL"　运行时产生的草稿（draft）和调试工程（exam）文件夹，\\r\\n"\n'
    '\t\t\t\t\t\tL"　这个文件夹里原有的东西不会被删掉。）",\n'
)
assert body.count(old_ask) == 1, "非空目录提示文案锚点不唯一"
body = body.replace(old_ask, new_ask, 1)
log.append("C2 非空目录提示文案已更新")

# ------------------------------------------------- 写回
out_raw = b"\xef\xbb\xbf" + body.encode("utf-8")
open(P, "wb").write(out_raw)
after = open(P, "rb").read()
assert after[:3] == b"\xef\xbb\xbf"
assert after.count(b"\r\n") == 0, "写回后出现了 CRLF"

log.append("")
log.append("before size=%d  after size=%d  delta=%+d" % (before_size, len(after), len(after) - before_size))
log.append("before sha256=%s" % before_hash)
log.append("after  sha256=%s" % hashlib.sha256(after).hexdigest())
dec = after[3:].decode("utf-8")
for k in ["DeleteOwnedArtifacts", "DeleteDirRecursiveW", "AppendLeftName", "OWNED_ARTIFACT_COUNT"]:
    log.append("  count %-22s = %d" % (k, dec.count(k)))
log.append("  residual 'D:\\\\对口升学练习系统' = %d" % dec.count("D:\\\\对口升学练习系统"))

open(r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\tools\_setup_patch_report.txt",
     "w", encoding="utf-8").write("\n".join(log))
print("done")
