# -*- coding: utf-8 -*-
"""诊断：主程序 exe 哈希为何相对基线变化。
证据链：
 1) 主程序源文件最后修改时间（若都是很久以前，说明源码没动过）
 2) 当前 x64/x86 exe 的 PE 头 TimeDateStamp（MSVC 默认写入链接时刻，每次重编必变）
 3) 是否存在确定性构建开关 /TIMESTAMP（rebuild.ps1 里没有）
 4) 当前各产物哈希
"""
import os, struct, time, io, hashlib

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
out = []
def log(s): out.append(s)

# ---- 1) 主程序源码文件 mtime ----
main_src = ["ExamDlgProj.cpp", "ExamDlgProj.h", "ExamDlgProjDlg.cpp", "ExamDlgProjDlg.h",
            "PracticeDlg.cpp", "PracticeDlg.h", "QuestionBank.cpp", "QuestionBank.h",
            "pch.cpp", "pch.h", "PublicDef.h", "resource.h", "framework.h", "targetver.h",
            "app.manifest", "ExamDlgProj.rc", "questions.txt"]
log("== 主程序源码 mtime（本地时间）==")
now = time.time()
for f in main_src:
    p = os.path.join(PROJ, f)
    if os.path.exists(p):
        mt = os.path.getmtime(p)
        age_h = (now - mt) / 3600.0
        log("  %-24s %s  (%.1f 小时前)" % (f, time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(mt)), age_h))
    else:
        log("  %-24s <缺失>" % f)

# ---- 2) PE TimeDateStamp ----
def pe_timestamp(path):
    with open(path, "rb") as fh:
        data = fh.read(0x400)
    if data[:2] != b"MZ":
        return None
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        return None
    tds = struct.unpack_from("<I", data, e_lfanew + 8)[0]
    return tds

def fmt_ts(ts):
    if not ts:
        return "None"
    return "%s (0x%08X)" % (time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(ts)), ts)

log("")
log("== 当前 exe 的 PE TimeDateStamp ==")
for label, rel in [("x64\\Release\\ExamDlgProj.exe", r"x64\Release\ExamDlgProj.exe"),
                   ("x86\\Release\\ExamDlgProj.exe", r"x86\Release\ExamDlgProj.exe"),
                   ("发布包\\ExamDlgProj.exe", r"发布包\ExamDlgProj.exe"),
                   ("发布包\\ExamDlgProj_32位.exe", r"发布包\ExamDlgProj_32位.exe"),
                   ("发布包\\对口升学练习系统_安装程序.exe", r"发布包\对口升学练习系统_安装程序.exe"),
                   ("安装程序\\Uninst.exe", r"安装程序\Uninst.exe")]:
    p = os.path.join(PROJ, rel)
    if os.path.exists(p):
        log("  %-40s %s" % (label, fmt_ts(pe_timestamp(p))))
    else:
        log("  %-40s <缺失>" % label)

# ---- 3) 确定性开关 ----
rb = os.path.join(PROJ, "tools", "rebuild.ps1")
txt = io.open(rb, encoding="utf-8-sig").read()
log("")
log("== 确定性构建检查 ==")
log("  rebuild.ps1 含 /TIMESTAMP : %s" % ("/TIMESTAMP" in txt))
log("  rebuild.ps1 含 /Brepro    : %s" % ("/Brepro" in txt))
log("  rebuild.ps1 含 /GL        : %s  (全程序优化，每次重编)" % ("/GL" in txt))
log("  rebuild.ps1 含 /DEBUG /Zi : %s" % ("/DEBUG" in txt or "/Zi" in txt))

# ---- 4) 产物哈希 ----
def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for c in iter(lambda: fh.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()

log("")
log("== 当前产物 SHA256 ==")
for rel in [r"x64\Release\ExamDlgProj.exe", r"x86\Release\ExamDlgProj.exe",
            r"发布包\ExamDlgProj.exe", r"发布包\ExamDlgProj_32位.exe",
            r"发布包\对口升学练习系统_安装程序.exe", r"questions.dat"]:
    p = os.path.join(PROJ, rel)
    if os.path.exists(p):
        log("  %s  %s  %d" % (sha(p), rel, os.path.getsize(p)))

io.open(r"C:\Users\30601\Desktop\_diag_payload.txt", "w", encoding="utf-8").write("\n".join(out))
print("done")
