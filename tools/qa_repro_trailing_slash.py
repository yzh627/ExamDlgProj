# -*- coding: utf-8 -*-
"""复现：注册表 InstallLocation 带结尾反斜杠(或等价非规范写法)时，
"就地升级"会把当前安装目录的 score.txt 一起删掉（数据丢失）。
对照：不带反斜杠时 score 保住。"""
import io, os, time, shutil, subprocess, hashlib, ctypes
from ctypes import wintypes

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
SETUP_SRC = PROJ + r"\发布包\对口升学练习系统_安装程序.exe"
SB = r"C:\_qatest_repro_" + time.strftime("%Y%m%d_%H%M%S")
OUT = r"C:\Users\30601\Desktop\_qa_repro_out.txt"
SUBKEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
FILES6 = ["ExamDlgProj.exe", "ExamDlgProj.exe.manifest", "questions.dat", "support.png", "说明.txt", "卸载.exe"]
advapi32 = ctypes.windll.advapi32
k32 = ctypes.windll.kernel32
out = []
def log(s):
    out.append(str(s)); io.open(OUT, "w", encoding="utf-8").write("\n".join(out))

def rm(p):
    try:
        if not os.path.exists(p): return
        if os.path.isdir(p):
            for r, ds, fs in os.walk(p, topdown=False):
                for f in fs:
                    k32.SetFileAttributesW(ctypes.c_wchar_p(os.path.join(r, f)), 0x80)
                    k32.DeleteFileW(ctypes.c_wchar_p(os.path.join(r, f)))
                k32.RemoveDirectoryW(ctypes.c_wchar_p(r))
        else:
            k32.DeleteFileW(ctypes.c_wchar_p(p))
    except BaseException:
        pass

def reg_set(name, value):
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
    """整键字节级快照（所有值，连类型和原始字节）。"""
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

def reg_set_raw(name, typ, data):
    for _ in range(20):
        h = wintypes.HKEY()
        if advapi32.RegCreateKeyExW(wintypes.HKEY(0x80000001), ctypes.c_wchar_p(SUBKEY), 0, None, 0, 0x20006, None, ctypes.byref(h), None) == 0:
            buf = ctypes.create_string_buffer(data, len(data) + 2)
            rc = advapi32.RegSetValueExW(h, ctypes.c_wchar_p(name), 0, typ, buf, len(data))
            advapi32.RegCloseKey(h)
            if rc == 0: return 0
        time.sleep(0.3)
    return rc

def reg_restore(snap):
    for _ in range(30):
        advapi32.RegDeleteTreeW(wintypes.HKEY(0x80000001), ctypes.c_wchar_p(SUBKEY))
        if snap[0] == "absent":
            return True
        ok = all(reg_set_raw(n, t, d) == 0 for n, t, d in snap[1])
        if ok:
            return True
        time.sleep(0.3)
    return False

def reg_get(name):
    h = wintypes.HKEY()
    if advapi32.RegOpenKeyExW(wintypes.HKEY(0x80000001), ctypes.c_wchar_p(SUBKEY), 0, 0x20019, ctypes.byref(h)) != 0:
        return None
    dlen = wintypes.DWORD(0); typ = wintypes.DWORD()
    if advapi32.RegQueryValueExW(h, ctypes.c_wchar_p(name), None, ctypes.byref(typ), None, ctypes.byref(dlen)) != 0:
        advapi32.RegCloseKey(h); return None
    buf = ctypes.create_string_buffer(dlen.value + 2)
    advapi32.RegQueryValueExW(h, ctypes.c_wchar_p(name), None, ctypes.byref(typ), buf, ctypes.byref(dlen))
    advapi32.RegCloseKey(h)
    return buf.raw[:dlen.value].decode("utf-16-le").rstrip("\0")

def logtext(d):
    p = os.path.join(d, "install.log")
    return open(p, "rb").read().decode("utf-8-sig", "replace") if os.path.exists(p) else ""

def case(name, regval_fn, sub):
    d = os.path.join(SB, "bin"); os.makedirs(d, exist_ok=True)
    exe = os.path.join(d, "setup.exe"); shutil.copy2(SETUP_SRC, exe)
    T = os.path.join(SB, sub, "对口升学练习系统")
    os.makedirs(T, exist_ok=True)
    shutil.copy2(SETUP_SRC, os.path.join(T, "ExamDlgProj.exe"))
    SCORE = ("成绩-张三-就地升级\n").encode("utf-8")
    open(os.path.join(T, "score.txt"), "wb").write(SCORE)
    regval = regval_fn(T)
    reg_set("InstallLocation", regval)
    rc = subprocess.run([exe, "/S", "/NODESKTOP", "/NOSTARTMENU", "/D=" + T], capture_output=True, timeout=120).returncode
    sc = os.path.join(T, "score.txt")
    kept = os.path.exists(sc) and open(sc, "rb").read() == SCORE
    lt = logtext(d)
    cleaned = [ln for ln in lt.splitlines() if "清理旧安装位置" in ln or "FINAL_DIR=" in ln]
    log("[%s] 注册表=%r" % (name, regval))
    log("     rc=%d  就地(无…1)=%s  score保住=%s" % (rc, not os.path.isdir(T + "1"), kept))
    for ln in cleaned:
        log("     LOG: " + ln.strip())
    log("")
    return kept

os.makedirs(SB, exist_ok=True)
log("沙箱=%s" % SB)
snap = reg_snapshot()
log("注册表键快照状态=%s" % snap[0])
log("")
try:
    case("对照：无反斜杠", lambda T: T, "c0")
    case("反斜杠1个", lambda T: T + "\\", "c1")
    case("反斜杠2个", lambda T: T + "\\\\", "c2")
    case("正斜杠", lambda T: T.replace("\\", "/"), "c3")
finally:
    # ★ 整键还原（不能只还原 InstallLocation：安装还会写 DisplayIcon/UninstallString 等）
    ok = reg_restore(snap)
    log("注册表整键还原=%s" % ok)
rm(SB)
log("沙箱已清理=%s" % os.path.exists(SB))
