# -*- coding: utf-8 -*-
"""热修：注册表 InstallLocation 带结尾反斜杠/正斜杠时，就地升级误删 score.txt。
新增 NormalizeDirForCompare / SameDir；ResolveInstallDir 与 ShouldCleanupOldDir
的目录比较都改用 SameDir（归一化后比较）。保持 UTF-8 BOM + LF。"""
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
assert raw[:3] == b"\xef\xbb\xbf" and raw.count(b"\r\n") == 0
body = raw[3:].decode("utf-8")
before_size = len(raw)
before_hash = hashlib.sha256(raw).hexdigest()
log = []

# ---- 1) 插入归一化 helper（放在“安装目录解析”段注释之后、GetDirDriveRoot 之前）
HDR = "// ================= 安装目录解析（盘符回退 + 同名加序号，界面版/静默版共用）================="
assert body.count(HDR) == 1
NEW = to_tabs(r"""// 目录比较用的归一化：统一正反斜杠、去掉结尾的分隔符（保留盘根那种）。
// 注册表里的 InstallLocation 常带结尾反斜杠，甚至写成正斜杠；不归一化就会把
// "同一个目录"误判成"另一个目录"——那会导致就地升级时反而去"清理旧位置"、把成绩删了。
static void NormalizeDirForCompare(const WCHAR* pszIn, WCHAR* pszOut, int cchOut)
{
    if (pszOut == NULL || cchOut <= 0)
        return;
    pszOut[0] = L'\0';
    if (pszIn == NULL)
        return;
    wcsncpy_s(pszOut, cchOut, pszIn, _TRUNCATE);
    size_t n = wcslen(pszOut);
    for (size_t i = 0; i < n; i++)
    {
        if (pszOut[i] == L'/')
            pszOut[i] = L'\\';
    }
    while (n > 3 && pszOut[n - 1] == L'\\')
        pszOut[--n] = L'\0';
}

// 两个目录是不是"同一个地方"（忽略大小写、正反斜杠、结尾反斜杠）
static BOOL SameDir(const WCHAR* pszA, const WCHAR* pszB)
{
    if (pszA == NULL || pszB == NULL)
        return FALSE;
    WCHAR szA[MAX_PATH] = { 0 };
    WCHAR szB[MAX_PATH] = { 0 };
    NormalizeDirForCompare(pszA, szA, MAX_PATH);
    NormalizeDirForCompare(pszB, szB, MAX_PATH);
    return (_wcsicmp(szA, szB) == 0);
}
""")
body = body.replace(HDR, HDR + "\n\n" + NEW, 1)
log.append("1 NormalizeDirForCompare/SameDir 已插入")

# ---- 2) ShouldCleanupOldDir：把“同一个目录”判定改为归一化比较
A2 = "\n".join([
    L(1, "if (pszFinal != NULL && pszFinal[0] != L'\\0' && _wcsicmp(pszOld, pszFinal) == 0)"),
    L(2, "return FALSE;"),
])
assert body.count(A2) == 1, "ShouldCleanupOldDir 比较锚点不唯一"
B2 = "\n".join([
    L(1, "// ★ 归一化后再比：注册表里的旧位置可能带结尾反斜杠/正斜杠，原样比较会把"),
    L(1, "//   同一个目录当成\"另一个目录\"，于是\"就地升级\"反而去清理旧位置、把成绩删了。"),
    L(1, "if (pszFinal != NULL && pszFinal[0] != L'\\0' && SameDir(pszOld, pszFinal))"),
    L(2, "return FALSE;"),
])
body = body.replace(A2, B2, 1)
log.append("2 ShouldCleanupOldDir 改用 SameDir")

# ---- 3) ResolveInstallDir：判“是否我方旧位置”也改用 SameDir
A3 = "\n".join([
    L(2, "WCHAR szOld[MAX_PATH] = { 0 };"),
    L(2, "GetInstalledDir(szOld, MAX_PATH);"),
    L(2, "size_t no = wcslen(szOld);"),
    L(2, "while (no > 3 && szOld[no - 1] == L'\\\\')"),
    L(3, "szOld[--no] = L'\\0';"),
    "",
    L(2, "if (szOld[0] != L'\\0' && _wcsicmp(szOld, szCur) == 0)"),
])
assert body.count(A3) == 1, "ResolveInstallDir 比较锚点不唯一"
B3 = "\n".join([
    L(2, "WCHAR szOld[MAX_PATH] = { 0 };"),
    L(2, "GetInstalledDir(szOld, MAX_PATH);"),
    "",
    L(2, "// 用 SameDir 归一化比较：旧位置可能带结尾反斜杠/正斜杠（见 NormalizeDirForCompare）"),
    L(2, "if (szOld[0] != L'\\0' && SameDir(szOld, szCur))"),
])
body = body.replace(A3, B3, 1)
log.append("3 ResolveInstallDir 改用 SameDir")

out = b"\xef\xbb\xbf" + body.encode("utf-8")
open(P, "wb").write(out)
a = open(P, "rb").read()
assert a[:3] == b"\xef\xbb\xbf" and a.count(b"\r\n") == 0 and a.count(b"\n") == a.count(b"\n")
dec = a[3:].decode("utf-8")
log.append("")
log.append("size %d -> %d  delta %+d" % (before_size, len(a), len(a) - before_size))
log.append("sha %s -> %s" % (before_hash[:16], hashlib.sha256(a).hexdigest()[:16]))
for k in ["static void NormalizeDirForCompare", "static BOOL SameDir(",
          "SameDir(pszOld, pszFinal)", "SameDir(szOld, szCur)"]:
    log.append("  count %-38s = %d" % (k, dec.count(k)))
open(r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\tools\_trailing_slash_fix_report.txt",
     "w", encoding="utf-8").write("\n".join(log))
print("OK trailing-slash fix done")
