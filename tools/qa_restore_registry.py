# -*- coding: utf-8 -*-
"""恢复真实机上的"卸载"注册表项（我在 GUI 测试里漏了快照/还原，被卸载动作删掉了）。
做法：注册表 InstallLocation 先指回真实目录(触发就地升级) -> 静默重装 -> 程序把 6 个文件
(字节与现有一致) 重写一遍 + 重新写入完整注册表项。重装前后核对真实目录逐字节未变。
"""
import io, os, time, shutil, subprocess, hashlib, ctypes
from ctypes import wintypes

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = PROJ + r"\发布包"
SETUP_SRC = PKG + r"\对口升学练习系统_安装程序.exe"
REAL = r"C:\对口升学练习系统"
SB = r"C:\_qareg_" + time.strftime("%Y%m%d_%H%M%S")
OUT = r"C:\Users\30601\Desktop\_qa_restore_reg.txt"
SUBKEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
advapi32 = ctypes.windll.advapi32
k32 = ctypes.windll.kernel32
out = []
def log(s):
    out.append(str(s)); io.open(OUT, "w", encoding="utf-8").write("\n".join(out))

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def tree(root):
    r = {}
    for a, ds, fs in os.walk(root):
        for f in fs:
            p = os.path.join(a, f)
            r[os.path.relpath(p, root)] = (os.path.getsize(p), sha256(p))
    return r

def rm(p):
    try:
        if not os.path.exists(p): return
        if os.path.isdir(p):
            for r, ds, fs in os.walk(p, topdown=False):
                for f in fs:
                    k32.SetFileAttributesW(ctypes.c_wchar_p(os.path.join(r, f)), 0x80)
                    k32.DeleteFileW(ctypes.c_wchar_p(os.path.join(r, f)))
                k32.RemoveDirectoryW(ctypes.c_wchar_p(r))
        else: k32.DeleteFileW(ctypes.c_wchar_p(p))
    except BaseException: pass

def reg_set_str(name, value):
    data = (value + "\0").encode("utf-16-le")
    for _ in range(20):
        h = wintypes.HKEY()
        if advapi32.RegCreateKeyExW(wintypes.HKEY(0x80000001), ctypes.c_wchar_p(SUBKEY), 0, None, 0, 0x20006, None, ctypes.byref(h), None) == 0:
            buf = ctypes.create_string_buffer(data, len(data) + 2)
            rc = advapi32.RegSetValueExW(h, ctypes.c_wchar_p(name), 0, 1, buf, len(data))
            advapi32.RegCloseKey(h)
            if rc == 0: return True
        time.sleep(0.3)
    return False

def reg_snapshot():
    h = wintypes.HKEY()
    if advapi32.RegOpenKeyExW(wintypes.HKEY(0x80000001), ctypes.c_wchar_p(SUBKEY), 0, 0x20019, ctypes.byref(h)) != 0:
        return ("absent",)
    vals = []; i = 0; nb = ctypes.create_unicode_buffer(4096)
    while True:
        nl = wintypes.DWORD(4096); ty = wintypes.DWORD()
        rc = advapi32.RegEnumValueW(h, i, nb, ctypes.byref(nl), None, ctypes.byref(ty), None, None)
        if rc == 259: break
        if rc != 0: break
        dl = wintypes.DWORD(0)
        advapi32.RegQueryValueExW(h, nb, None, ctypes.byref(ty), None, ctypes.byref(dl))
        db = ctypes.create_string_buffer(dl.value + 2)
        advapi32.RegQueryValueExW(h, nb, None, ctypes.byref(ty), db, ctypes.byref(dl))
        vals.append((nb.value, ty.value, bytes(db.raw[:dl.value])))
        i += 1
    advapi32.RegCloseKey(h)
    return ("present", vals)

before = tree(REAL) if os.path.isdir(REAL) else {}
log("== 恢复前 ==")
log("真实目录文件=%r" % {k: v[0] for k, v in before.items()})
snap0 = reg_snapshot()
log("注册表键状态=%s" % snap0[0])

os.makedirs(SB, exist_ok=True)
exe = os.path.join(SB, "setup.exe")
shutil.copy2(SETUP_SRC, exe)
# 先让 InstallLocation 指回真实目录 => 触发"就地升级"而不是加序号
reg_set_str("InstallLocation", REAL)
log("已设 InstallLocation=%r（就地升级）" % REAL)
rc = subprocess.run([exe, "/S", "/NODESKTOP", "/NOSTARTMENU", "/D=" + REAL],
                    capture_output=True, timeout=180).returncode
log("重装 rc=%d" % rc)
time.sleep(1.5)

after = tree(REAL)
log("")
log("== 恢复后 ==")
log("真实目录文件=%r" % {k: v[0] for k, v in after.items()})
same = (before == after)
log("真实目录逐字节未变 = %s" % same)
if not same:
    log("  added=%r" % (set(after) - set(before)))
    log("  removed=%r" % (set(before) - set(after)))
    log("  changed=%r" % [k for k in before if k in after and before[k] != after[k]])
snap1 = reg_snapshot()
log("注册表键状态=%s  值：" % snap1[0])
if snap1[0] == "present":
    for nm, ty, da in snap1[1]:
        s = ""
        if ty == 1:
            try: s = da.decode("utf-16-le").rstrip("\0")
            except Exception: s = repr(da)
        elif ty == 7:
            s = da.decode("utf-16-le", "replace").replace("\0", "|")
        else:
            s = da.hex()
        log("   %-16s type=%d len=%d  val=%s" % (nm, ty, len(da), s))
    # 校验 InstallLocation 指回真实目录
    loc = ""
    for nm, ty, da in snap1[1]:
        if nm == "InstallLocation":
            loc = da.decode("utf-16-le").rstrip("\0")
    log("InstallLocation 是否== 真实目录：%s" % (loc == REAL))

# 与发布包逐文件核对真实目录内容
log("")
log("== 真实目录 vs 发布包 payload ==")
for real_name, pkg_name in [("ExamDlgProj.exe", r"\ExamDlgProj.exe"),
                            ("ExamDlgProj.exe.manifest", r"\ExamDlgProj.exe.manifest"),
                            ("questions.dat", r"\questions.dat"),
                            ("support.png", r"\support.png"),
                            ("说明.txt", r"\说明.txt")]:
    a = os.path.join(REAL, real_name); b = PKG + pkg_name
    if os.path.exists(a) and os.path.exists(b):
        log("   %-28s %s" % (real_name, sha256(a)[:16] == sha256(b)[:16]))
rm(SB)
log("")
log("沙箱已清理=%s" % os.path.exists(SB))
