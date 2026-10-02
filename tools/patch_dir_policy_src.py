# -*- coding: utf-8 -*-
"""Setup.cpp 补丁（第三修）：
  A. 新增公共 helper：盘符回退(GetDirDriveRoot/IsDriveUsable) + 同名加序号(ResolveInstallDir)
     + 旧位置清理守卫(ShouldCleanupOldDir) + 给静默 bat 用的小文件(WriteLastDirFile)
  B. DoSilentInstall：校验后调 ResolveInstallDir，写 FINAL_DIR 日志 + last_dir.txt
  C. 界面版 IDOK：校验后调同一套 ResolveInstallDir，调整了就回显 + 弹提示
  D. 两处"换目录删旧位置"改用 ShouldCleanupOldDir（数据安全红线）
不改 Setup.rc / resource.h。保持 UTF-8 BOM + LF。
"""
import hashlib

P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\安装程序\Setup.cpp"

def L(level, s):
    return "\t" * level + s

def to_tabs(s):
    res = []
    for ln in s.split("\n"):
        i = 0
        while ln[i:i + 4] == "    ":
            i += 4
        res.append("\t" * (i // 4) + ln[i:])
    return "\n".join(res)

raw = open(P, "rb").read()
assert raw[:3] == b"\xef\xbb\xbf", "Setup.cpp 应为 UTF-8 BOM"
assert raw.count(b"\r\n") == 0, "Setup.cpp 应为纯 LF"
body = raw[3:].decode("utf-8")
before_size = len(raw)
before_hash = hashlib.sha256(raw).hexdigest()
log = []

# ---------------------------------------------------------------- A. helper
HEADER = "// ================= 题库信息（让界面上的题数永远跟实际题库一致）================="
assert body.count(HEADER) == 1
log.append("A anchor 题库信息 header 命中 1 次")

HELPER = to_tabs(r"""// ================= 安装目录解析（盘符回退 + 同名加序号，界面版/静默版共用）=================

// 取目录所在的盘根，如 "C:\对口升学练习系统" -> "C:\"。不是 "X:\..." 形式时返回 FALSE。
static BOOL GetDirDriveRoot(const WCHAR* pszDir, WCHAR* pszRoot, int cchRoot)
{
    if (pszDir == NULL || pszDir[0] == L'\0' || pszRoot == NULL || cchRoot < 4)
        return FALSE;
    WCHAR c = pszDir[0];
    if (((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')) && pszDir[1] == L':')
    {
        swprintf_s(pszRoot, cchRoot, L"%c:\\", c);
        return TRUE;
    }
    return FALSE;
}

// 一个盘是不是"存在且可写"。bRequireFixed=TRUE 时只认固定盘（回退选盘用：
// 光驱、网络盘、U 盘一律跳过）。只看 GetDriveTypeW 还不够 —— 网络盘在线返回
// DRIVE_REMOTE、U 盘也能写，所以这里再在盘根上实打实建一个临时目录再删掉，
// 确认"真的能写"（用户要求的"实打实校验"）。
static BOOL IsDriveUsable(const WCHAR* pszRoot, BOOL bRequireFixed)
{
    if (pszRoot == NULL || pszRoot[0] == L'\0')
        return FALSE;
    UINT t = GetDriveTypeW(pszRoot);
    if (t == DRIVE_NO_ROOT_DIR || t == DRIVE_UNKNOWN)
        return FALSE;
    if (bRequireFixed && t != DRIVE_FIXED)
        return FALSE;
    WCHAR szProbe[MAX_PATH];
    swprintf_s(szProbe, MAX_PATH, L"%s__hebei_write_probe_%u__", pszRoot, GetCurrentProcessId());
    if (CreateDirectoryW(szProbe, NULL))
    {
        RemoveDirectoryW(szProbe);
        return TRUE;
    }
    return (GetLastError() == ERROR_ALREADY_EXISTS);
}

// 旧安装位置值不值得"换位置后清理"：必须是有效目录、和最终目录确实不同，
// 而且里面确实有本程序的痕迹（免得注册表指向别处时误删人家的东西）。
// ★ 数据安全红线：宁可少清一次，也绝不多删一个。
static BOOL ShouldCleanupOldDir(const WCHAR* pszOld, const WCHAR* pszFinal)
{
    if (pszOld == NULL || pszOld[0] == L'\0')
        return FALSE;
    if (GetFileAttributesW(pszOld) == INVALID_FILE_ATTRIBUTES)
        return FALSE;
    if (pszFinal != NULL && pszFinal[0] != L'\0' && _wcsicmp(pszOld, pszFinal) == 0)
        return FALSE;
    WCHAR szProbe[MAX_PATH];
    swprintf_s(szProbe, MAX_PATH, L"%s\\ExamDlgProj.exe", pszOld);
    if (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES)
        return TRUE;
    swprintf_s(szProbe, MAX_PATH, L"%s\\卸载.exe", pszOld);
    if (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES)
        return TRUE;
    swprintf_s(szProbe, MAX_PATH, L"%s\\questions.dat", pszOld);
    if (GetFileAttributesW(szProbe) != INVALID_FILE_ATTRIBUTES)
        return TRUE;
    return FALSE;
}

// 把"原定目录"解析成"最终目录"，界面版和静默版走同一套：
//   1) 盘符回退：原定目录所在盘不可用 -> 从 C: 到 Z: 找第一个能用的固定盘，
//      目标改成 <盘符>:\对口升学练习系统（只认固定盘，U盘/光驱/网络盘跳过）
//   2) 同名加序号：目标已存在且不是"本程序上次装的地方" -> 依次试 <目录>1..99，
//      取第一个不存在的（例如 C:\对口升学练习系统 被别人占了 -> C:\对口升学练习系统1）
// 返回 TRUE 正常；FALSE 表示原定目录为空（调用方报错退出）。
// pbFallback / pbNumbered（可空）：告诉调用方发生了哪种调整，便于回显给用户。
static BOOL ResolveInstallDir(const WCHAR* pszWant, WCHAR* pszOut, int cchOut,
    BOOL* pbFallback, BOOL* pbNumbered)
{
    if (pbFallback != NULL) *pbFallback = FALSE;
    if (pbNumbered != NULL) *pbNumbered = FALSE;
    if (pszOut == NULL || cchOut <= 0)
        return FALSE;

    WCHAR szCur[MAX_PATH];
    wcsncpy_s(szCur, MAX_PATH, (pszWant != NULL) ? pszWant : L"", _TRUNCATE);
    size_t n = wcslen(szCur);
    while (n > 3 && szCur[n - 1] == L'\\')
        szCur[--n] = L'\0';
    if (szCur[0] == L'\0')
        return FALSE;

    // ---- 1) 盘符回退 ----
    WCHAR szRoot[8] = { 0 };
    BOOL bDriveOK = TRUE;
    if (GetDirDriveRoot(szCur, szRoot, 8))
        bDriveOK = IsDriveUsable(szRoot, FALSE);

    if (!bDriveOK)
    {
        WCHAR szNew[MAX_PATH] = { 0 };
        for (WCHAR c = L'C'; c <= L'Z'; c++)
        {
            WCHAR szR[8];
            swprintf_s(szR, 8, L"%c:\\", c);
            if (IsDriveUsable(szR, TRUE))
            {
                swprintf_s(szNew, MAX_PATH, L"%c:\\%s", c, APP_SHORTNAME);
                break;
            }
        }
        if (szNew[0] != L'\0')
        {
            LogLine(L"原定目录 %s 所在盘不可用 -> 回退到 %s", szCur, szNew);
            wcsncpy_s(szCur, MAX_PATH, szNew, _TRUNCATE);
            if (pbFallback != NULL) *pbFallback = TRUE;
        }
        else
        {
            LogLine(L"原定目录 %s 所在盘不可用，且 C:~Z: 都没有可用的固定盘，仍按原目录处理。", szCur);
        }
    }

    // ---- 2) 同名加序号 ----
    if (GetFileAttributesW(szCur) != INVALID_FILE_ATTRIBUTES)
    {
        WCHAR szOld[MAX_PATH] = { 0 };
        GetInstalledDir(szOld, MAX_PATH);
        size_t no = wcslen(szOld);
        while (no > 3 && szOld[no - 1] == L'\\')
            szOld[--no] = L'\0';

        if (szOld[0] != L'\0' && _wcsicmp(szOld, szCur) == 0)
        {
            LogLine(L"目标目录已存在，且就是本程序上次的安装位置 -> 就地升级，不改名：%s", szCur);
        }
        else
        {
            WCHAR szTry[MAX_PATH] = { 0 };
            BOOL bFound = FALSE;
            for (int i = 1; i <= 99; i++)
            {
                swprintf_s(szTry, MAX_PATH, L"%s%d", szCur, i);
                if (GetFileAttributesW(szTry) == INVALID_FILE_ATTRIBUTES)
                {
                    bFound = TRUE;
                    break;
                }
            }
            if (bFound)
            {
                LogLine(L"目标目录已被占用（且不是本程序上次的安装位置）-> 改用：%s", szTry);
                wcsncpy_s(szCur, MAX_PATH, szTry, _TRUNCATE);
                if (pbNumbered != NULL) *pbNumbered = TRUE;
            }
            else
            {
                LogLine(L"目标目录已被占用，且 目录1~目录99 都被占用，仍按原目录处理：%s", szCur);
            }
        }
    }

    wcsncpy_s(pszOut, cchOut, szCur, _TRUNCATE);
    return TRUE;
}

// 把最终安装目录写进一个 ANSI（本机代码页）小文件，专供 静默安装.bat 读取：
// bat/cmd 是 GBK，而 install.log 是 UTF-8，从 log 里取中文路径会乱码、if exist 也会对不上。
// 优先放自己旁边（和 install.log 一致），写不进去就放 %TEMP%。
static void WriteLastDirFile(const WCHAR* pszDir)
{
    if (pszDir == NULL || pszDir[0] == L'\0')
        return;
    WCHAR szSelf[MAX_PATH] = { 0 };
    WCHAR szPath[MAX_PATH] = { 0 };
    GetSelfDir(szSelf, MAX_PATH);
    swprintf_s(szPath, MAX_PATH, L"%slast_dir.txt", szSelf);

    FILE* fp = NULL;
    _wfopen_s(&fp, szPath, L"wb");
    if (fp == NULL)
    {
        WCHAR szTmp[MAX_PATH] = { 0 };
        DWORD dw = GetTempPathW(MAX_PATH, szTmp);
        if (dw == 0 || dw >= MAX_PATH)
            return;
        swprintf_s(szPath, MAX_PATH, L"%s对口升学练习系统_last_dir.txt", szTmp);
        _wfopen_s(&fp, szPath, L"wb");
        if (fp == NULL)
            return;
    }
    int cb = WideCharToMultiByte(CP_ACP, 0, pszDir, -1, NULL, 0, NULL, NULL);
    if (cb > 1)
    {
        char* p = (char*)malloc(cb);
        if (p != NULL)
        {
            WideCharToMultiByte(CP_ACP, 0, pszDir, -1, p, cb, NULL, NULL);
            fwrite(p, 1, cb - 1, fp);
            free(p);
        }
    }
    fflush(fp);
    fclose(fp);
}
""")

body = body.replace(HEADER, HELPER + "\n\n" + HEADER, 1)

# ---------------------------------------------------------------- B silent 插入
S2a = "\n".join([
    L(1, '// 目录合法性：和界面版同一套规则。静默模式没人在旁边看，更要拦住。'),
    L(1, 'WCHAR szWhy[MAX_PATH + 300] = { 0 };'),
    L(1, 'if (!IsSafeInstallDir(szDir, szWhy, MAX_PATH + 300))'),
    L(1, '{'),
    L(2, 'LogLine(L"安装目录不合适：%s", szWhy);'),
    L(2, 'LogLine(L"静默安装结束（退出码 2）。");'),
    L(2, 'CloseLog();'),
    L(2, 'return 2;'),
    L(1, '}'),
])
assert body.count(S2a) == 1, "silent 校验块锚点不唯一"
INSERT_SILENT = to_tabs(r"""    // 盘符回退 + 同名加序号（和界面版 ResolveInstallDir 同一套逻辑，别再各写一份）
    {
        WCHAR szResolved[MAX_PATH] = { 0 };
        BOOL bFallback = FALSE, bNumbered = FALSE;
        if (!ResolveInstallDir(szDir, szResolved, MAX_PATH, &bFallback, &bNumbered))
        {
            LogLine(L"安装目录解析失败（目录为空），静默安装结束（退出码 2）。");
            CloseLog();
            return 2;
        }
        if (bFallback || bNumbered)
            LogLine(L"最终安装目录：%s（盘符回退=%s，同名加序号=%s）",
                szResolved, bFallback ? L"是" : L"否", bNumbered ? L"是" : L"否");
        wcsncpy_s(szDir, MAX_PATH, szResolved, _TRUNCATE);
        // 回退/加序号后的目录再校验一次（正常必然通过；万一不通过就按住不动）
        if (!IsSafeInstallDir(szDir, szWhy, MAX_PATH + 300))
        {
            LogLine(L"回退/加序号后的目录不合适：%s", szWhy);
            LogLine(L"静默安装结束（退出码 2）。");
            CloseLog();
            return 2;
        }
    }

    // 最终装到哪儿：日志里留一行 FINAL_DIR=（给人看/可解析），
    // 另写一个 ANSI 小文件 last_dir.txt 给「静默安装.bat」直接读（bat 是 GBK，读 UTF-8 的 log 会乱码）
    LogLine(L"FINAL_DIR=%s", szDir);
    WriteLastDirFile(szDir);
""")
body = body.replace(S2a, S2a + "\n\n" + INSERT_SILENT, 1)
log.append("B DoSilentInstall 已插入 ResolveInstallDir + FINAL_DIR + last_dir.txt")

# ---------------------------------------------------------------- C 界面版插入
S3 = "\n".join([
    L(3, '// 【修复】安装位置安全校验：盘根 / Windows 目录 / 系统特殊目录一律拦住。'),
    L(3, '// 以前完全不校验，用户真把路径填成 C:\\，6 个文件就散在盘根上了。'),
    L(3, '{'),
    L(4, 'WCHAR szWhy[MAX_PATH + 300] = { 0 };'),
    L(4, 'if (!IsSafeInstallDir(szDir, szWhy, MAX_PATH + 300))'),
    L(4, '{'),
    L(5, 'MessageBoxW(hDlg, szWhy, L"安装位置不合适", MB_OK | MB_ICONWARNING | MB_TOPMOST);'),
    L(5, 'return TRUE;'),
    L(4, '}'),
    L(3, '}'),
])
assert body.count(S3) == 1, "界面版校验块锚点不唯一"
INSERT_UI = to_tabs(r"""            // 【新增】盘符回退 + 同名加序号（和静默版共用 ResolveInstallDir）。
            // 用户填的目录可能"所在盘没了"或"同名文件夹被占了"，统一在这里定下最终目录。
            {
                WCHAR szTyped[MAX_PATH];
                wcsncpy_s(szTyped, MAX_PATH, szDir, _TRUNCATE);
                WCHAR szResolved[MAX_PATH] = { 0 };
                BOOL bFallback = FALSE, bNumbered = FALSE;
                if (!ResolveInstallDir(szDir, szResolved, MAX_PATH, &bFallback, &bNumbered))
                {
                    MessageBoxW(hDlg, L"请先填写安装位置。", L"提示", MB_OK | MB_ICONWARNING | MB_TOPMOST);
                    return TRUE;
                }
                wcsncpy_s(szDir, MAX_PATH, szResolved, _TRUNCATE);
                if (bFallback || bNumbered)
                {
                    // 把最终目录回显到输入框，并明确告知，别让用户以为装到了自己填的那个路径
                    SetDlgItemTextW(hDlg, IDC_EDIT_DIR, szDir);
                    WCHAR szWhy2[160];
                    if (bFallback)
                        wcsncpy_s(szWhy2, 160, L"（原定目录所在盘不可用，已改用其它可用固定盘）", _TRUNCATE);
                    else
                        wcsncpy_s(szWhy2, 160, L"（原定同名文件夹已被占用，已自动加序号）", _TRUNCATE);
                    WCHAR szNote2[MAX_PATH * 3];
                    swprintf_s(szNote2, MAX_PATH * 3,
                        L"安装位置已自动调整。\r\n\r\n原定：%s\r\n实际：%s\r\n\r\n%s",
                        szTyped, szDir, szWhy2);
                    MessageBoxW(hDlg, szNote2, L"安装位置已调整", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                }
            }
""")
body = body.replace(S3, S3 + "\n\n" + INSERT_UI, 1)
log.append("C 界面版 IDOK 已插入 ResolveInstallDir + 回显/提示")

# ---------------------------------------------------------------- D 两处清理守卫
S2b = "\n".join([
    L(2, "if (szOld[0] != L'\\0' && _wcsicmp(szOld, szDir) != 0"),
    L(3, '&& GetFileAttributesW(szOld) != INVALID_FILE_ATTRIBUTES)'),
])
assert body.count(S2b) == 1, "silent 旧位置清理条件锚点不唯一"
body = body.replace(S2b, L(2, "if (ShouldCleanupOldDir(szOld, szDir))"), 1)

S4 = "\n".join([
    L(4, "if (szOld[0] != L'\\0' && _wcsicmp(szOld, szDir) != 0 &&"),
    L(5, 'GetFileAttributesW(szOld) != INVALID_FILE_ATTRIBUTES)'),
])
assert body.count(S4) == 1, "界面版旧位置清理条件锚点不唯一"
body = body.replace(S4, L(4, "if (ShouldCleanupOldDir(szOld, szDir))"), 1)
log.append("D 两处旧位置清理条件已改用 ShouldCleanupOldDir")

# ---------------------------------------------------------------- 写回
out_raw = b"\xef\xbb\xbf" + body.encode("utf-8")
open(P, "wb").write(out_raw)
after = open(P, "rb").read()
assert after[:3] == b"\xef\xbb\xbf"
assert after.count(b"\r\n") == 0, "写回后出现 CRLF"
assert after.count(b"\n") == after.count(b"\n")

dec = after[3:].decode("utf-8")
log.append("")
log.append("before size=%d  after size=%d  delta=%+d" % (before_size, len(after), len(after) - before_size))
log.append("before sha256=%s" % before_hash)
log.append("after  sha256=%s" % hashlib.sha256(after).hexdigest())
for k in ["GetDirDriveRoot", "IsDriveUsable", "ShouldCleanupOldDir", "ResolveInstallDir",
          "WriteLastDirFile", "FINAL_DIR=", "ShouldCleanupOldDir(szOld, szDir)"]:
    log.append("  count %-34s = %d" % (k, dec.count(k)))

open(r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\tools\_dir_policy_src_report.txt",
     "w", encoding="utf-8").write("\n".join(log))
print("OK src patch done")
