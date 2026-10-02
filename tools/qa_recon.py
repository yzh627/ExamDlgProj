# -*- coding: utf-8 -*-
"""QA 侦察：环境事实采集。只读，不改任何东西。"""
import io, os, sys, hashlib, ctypes, string

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = PROJ + r"\发布包"
REAL = r"C:\对口升学练习系统"
OUT = r"C:\Users\30601\Desktop\_qa_recon2.txt"
out = []
def log(s): out.append(str(s))

def sha256(p, n=None):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while True:
            b = f.read(1 << 20)
            if not b: break
            h.update(b)
    return h.hexdigest()

log("python=%s" % sys.version.replace("\n", " "))

# drives
k32 = ctypes.windll.kernel32
for c in string.ascii_uppercase:
    root = c + ":\\"
    if os.path.exists(root):
        t = k32.GetDriveTypeW(ctypes.c_wchar_p(root))
        typ = {2:"REMOVABLE",3:"FIXED",4:"REMOTE",5:"CDROM",6:"RAMDISK"}.get(t, str(t))
        free = ctypes.c_ulonglong(0); tot = ctypes.c_ulonglong(0); tf = ctypes.c_ulonglong(0)
        k32.GetDiskFreeSpaceExW(ctypes.c_wchar_p(root), ctypes.byref(free), ctypes.byref(tot), ctypes.byref(tf))
        log("DRIVE %s type=%s free=%dMB" % (root, typ, free.value // (1<<20)))

log("")
log("== REAL %s exists=%s ==" % (REAL, os.path.isdir(REAL)))
if os.path.isdir(REAL):
    for f in sorted(os.listdir(REAL)):
        fp = os.path.join(REAL, f)
        if os.path.isfile(fp):
            log("  %s  %d" % (f, os.path.getsize(fp)))
        else:
            log("  [D] %s" % f)

log("")
log("== 发布包 ==")
for f in sorted(os.listdir(PKG)):
    fp = os.path.join(PKG, f)
    log("  %-40s %d" % (f, os.path.getsize(fp)))

log("")
log("== hashes ==")
for f, rel in [("installer", r"\对口升学练习系统_安装程序.exe"),
               ("bat", r"\静默安装.bat"),
               ("manual", r"\使用说明.txt"),
               ("x64", r"\ExamDlgProj.exe"),
               ("x86", r"\ExamDlgProj_32位.exe"),
               ("questions_pkg", r"\questions.dat"),
               ("questions_proj", r"\..\questions.dat")]:
    p = os.path.normpath(PKG + rel)
    if os.path.exists(p):
        log("  %-16s %s  (%d B)  %s" % (f, sha256(p)[:16], os.path.getsize(p), p))
    else:
        log("  %-16s MISSING %s" % (f, p))

log("")
log("== 关键过程文件是否已残留在发布包 ==")
for nm in ["last_dir.txt", "install.log"]:
    log("  发布包\\%s exists=%s" % (nm, os.path.exists(os.path.join(PKG, nm))))
log("  安装程序目录\\last_dir.txt exists=%s" % os.path.exists(PROJ + r"\安装程序\last_dir.txt"))

io.open(OUT, "w", encoding="utf-8").write("\n".join(out))
print("done")
