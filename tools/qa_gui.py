# -*- coding: utf-8 -*-
"""V6 界面版完整流程（真实点【开始安装】，PID 过滤枚举窗口 + PostMessage(BM_CLICK)）。
子场景：
  B  目标目录不存在 -> 不应弹「已调整」，装到该目录
  A  目标目录被外人占用 -> 应弹「安装位置已调整」，回显最终目录 ...1，装到 ...1
  F  目标盘 Q: 不存在 -> 应弹「已调整(回退)」，装到可用固定盘
安全：全程注册表 InstallLocation 指向不存在的沙箱路径，避免触发清旧位置；
      勾掉桌面/开始菜单/立即运行，避免污染真实桌面。
"""
import io, os, sys, time, shutil, hashlib, ctypes, traceback
from ctypes import wintypes

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = PROJ + r"\发布包"
SETUP_SRC = PKG + r"\对口升学练习系统_安装程序.exe"
TS = time.strftime("%Y%m%d_%H%M%S")
SB = r"C:\_qatest_gui_" + TS
OUT = r"C:\Users\30601\Desktop\_qa_gui_out.txt"
REAL = r"C:\对口升学练习系统"
FILES6 = ["ExamDlgProj.exe", "ExamDlgProj.exe.manifest", "questions.dat",
          "support.png", "说明.txt", "卸载.exe"]

u32 = ctypes.windll.user32
k32 = ctypes.windll.kernel32
advapi32 = ctypes.windll.advapi32
HKCU = 0x80000001
KEY_READ = 0x20019
KEY_WRITE = 0x20006
SUBKEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"

u32.SendMessageTimeoutW.restype = ctypes.c_ssize_t
u32.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, ctypes.c_size_t,
                                     ctypes.c_ssize_t, wintypes.UINT, wintypes.UINT,
                                     ctypes.POINTER(ctypes.c_size_t)]
u32.PostMessageW.restype = wintypes.BOOL
u32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t]
u32.GetDlgItem.restype = wintypes.HWND
u32.GetDlgItem.argtypes = [wintypes.HWND, ctypes.c_int]
u32.GetWindowThreadProcessId.restype = wintypes.DWORD
u32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
u32.EnumWindows.argtypes = [ctypes.c_void_p, wintypes.LPARAM]
u32.EnumChildWindows.argtypes = [wintypes.HWND, ctypes.c_void_p, wintypes.LPARAM]

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
WM_SETTEXT, WM_GETTEXT, WM_COMMAND = 0x000C, 0x000D, 0x0111
BM_CLICK, BM_GETCHECK = 0x00F5, 0x00F0
SMTO_ABORTIFHUNG = 0x0002
MAIN_TITLE = "河北对口升学计算机程序设计练习系统 - 安装"

out = []
def log(s):
    out.append(str(s))
    try:
        io.open(OUT, "w", encoding="utf-8").write("\n".join(out))
    except Exception:
        pass

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def tree(root):
    res = {}
    for r, ds, fs in os.walk(root):
        for f in fs:
            p = os.path.join(r, f)
            try: res[os.path.relpath(p, root)] = (os.path.getsize(p), sha256(p))
            except Exception: res[os.path.relpath(p, root)] = (-1, "ERR")
    return res

def rm(path):
    try:
        if not os.path.exists(path): return
        if os.path.isdir(path):
            for r, ds, fs in os.walk(path, topdown=False):
                for f in fs:
                    fp = os.path.join(r, f)
                    k32.SetFileAttributesW(ctypes.c_wchar_p(fp), 0x80)
                    k32.DeleteFileW(ctypes.c_wchar_p(fp))
                k32.RemoveDirectoryW(ctypes.c_wchar_p(r))
        else:
            k32.DeleteFileW(ctypes.c_wchar_p(path))
    except BaseException:
        pass

# ---- registry ----
def reg_set_str(name, value):
    data = (value + "\0").encode("utf-16-le")
    for _ in range(20):
        h = wintypes.HKEY()
        rc = advapi32.RegCreateKeyExW(wintypes.HKEY(HKCU), ctypes.c_wchar_p(SUBKEY), 0, None,
                                      0, KEY_WRITE, None, ctypes.byref(h), None)
        if rc == 0:
            buf = ctypes.create_string_buffer(data, len(data) + 2)
            rc2 = advapi32.RegSetValueExW(h, ctypes.c_wchar_p(name), 0, 1, buf, len(data))
            advapi32.RegCloseKey(h)
            if rc2 == 0: return 0
        time.sleep(0.3)
    return rc

def reg_deltree():
    advapi32.RegDeleteTreeW(wintypes.HKEY(HKCU), ctypes.c_wchar_p(SUBKEY))

NO_MORE_ITEMS = 259
def reg_snapshot():
    h = wintypes.HKEY()
    if advapi32.RegOpenKeyExW(wintypes.HKEY(HKCU), ctypes.c_wchar_p(SUBKEY), 0, KEY_READ, ctypes.byref(h)) != 0:
        return ("absent",)
    vals = []; i = 0; nb = ctypes.create_unicode_buffer(4096)
    while True:
        nl = wintypes.DWORD(4096); ty = wintypes.DWORD()
        rc = advapi32.RegEnumValueW(h, i, nb, ctypes.byref(nl), None, ctypes.byref(ty), None, None)
        if rc == NO_MORE_ITEMS: break
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
        if advapi32.RegCreateKeyExW(wintypes.HKEY(HKCU), ctypes.c_wchar_p(SUBKEY), 0, None, 0, KEY_WRITE, None, ctypes.byref(h), None) == 0:
            buf = ctypes.create_string_buffer(data, len(data) + 2)
            rc = advapi32.RegSetValueExW(h, ctypes.c_wchar_p(name), 0, typ, buf, len(data))
            advapi32.RegCloseKey(h)
            if rc == 0: return 0
        time.sleep(0.3)
    return rc

def reg_restore(snap):
    for _ in range(30):
        reg_deltree()
        if snap[0] == "absent":
            return True
        ok = True
        for name, typ, data in snap[1]:
            if reg_set_raw(name, typ, data) != 0:
                ok = False
        if ok:
            return True
        time.sleep(0.3)
    return False

# ---- win helpers ----
def get_text(h):
    buf = ctypes.create_unicode_buffer(4096)
    r = ctypes.c_size_t()
    ok = u32.SendMessageTimeoutW(wintypes.HWND(h), WM_GETTEXT, 4096,
                                 ctypes.addressof(buf), SMTO_ABORTIFHUNG, 1500, ctypes.byref(r))
    return buf.value if ok else ""

def set_text(h, s):
    sp = ctypes.c_wchar_p(s)
    lp = ctypes.cast(sp, ctypes.c_void_p).value
    r = ctypes.c_size_t()
    u32.SendMessageTimeoutW(wintypes.HWND(h), WM_SETTEXT, 0, lp, SMTO_ABORTIFHUNG, 2000, ctypes.byref(r))

def win_text(h):
    buf = ctypes.create_unicode_buffer(512)
    u32.GetWindowTextW(wintypes.HWND(h), buf, 512)
    return buf.value

def cls_name(h):
    buf = ctypes.create_unicode_buffer(256)
    u32.GetClassNameW(wintypes.HWND(h), buf, 256)
    return buf.value

def top_windows(pid):
    res = []
    def cb(h, l):
        p = wintypes.DWORD()
        u32.GetWindowThreadProcessId(wintypes.HWND(h), ctypes.byref(p))
        if p.value == pid:
            res.append(h)
        return True
    u32.EnumWindows(WNDENUMPROC(cb), 0)
    return res

def child_texts(h):
    res = []
    def cb(c, l):
        t = win_text(c)
        if t:
            res.append(t)
        return True
    u32.EnumChildWindows(wintypes.HWND(h), WNDENUMPROC(cb), 0)
    return res

def core6(d):
    return sum(1 for f in FILES6 if os.path.exists(os.path.join(d, f)))

def child_windows(h):
    res = []
    def cb(c, l):
        res.append((c, cls_name(c), win_text(c)))
        return True
    u32.EnumChildWindows(wintypes.HWND(h), WNDENUMPROC(cb), 0)
    return res

def dismiss(h):
    """点确定/是/OK 按钮关弹窗；找不到就 WM_COMMAND IDOK。"""
    btns = [(c, t) for (c, cl, t) in child_windows(h) if cl == "Button"]
    for c, t in btns:
        tt = t.replace("&", "").strip()
        if tt in ("确定", "OK", "是", "Yes", "是(Y)", "是（Y）"):
            u32.PostMessageW(wintypes.HWND(c), BM_CLICK, 0, 0)
            return "btn:%s" % tt
    if btns:
        u32.PostMessageW(wintypes.HWND(btns[0][0]), BM_CLICK, 0, 0)
        return "btn0:%s" % btns[0][1]
    u32.PostMessageW(wintypes.HWND(h), WM_COMMAND, 1, 0)
    return "wm_command_idok"

# ---- one GUI run ----
def run_gui(exe, edit_value, timeout=45):
    import subprocess
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    pid = proc.pid
    main = 0
    deadline = time.time() + 20
    while time.time() < deadline:
        for h in top_windows(pid):
            if win_text(h) == MAIN_TITLE:
                main = h; break
        if main: break
        time.sleep(0.25)
    result = {"pid": pid, "main": main, "seen": {}, "edits": [], "default_edit": None,
              "exited": False, "rc": None, "timeline": [], "dismissed": []}
    if not main:
        try: proc.kill()
        except Exception: pass
        return result
    time.sleep(0.8)
    hedit = u32.GetDlgItem(wintypes.HWND(main), 1001)
    result["default_edit"] = get_text(hedit)
    # 勾掉桌面/开始菜单/立即运行（避免污染真机）
    for cid in (1003, 1004, 1005):
        hc = u32.GetDlgItem(wintypes.HWND(main), cid)
        if hc:
            u32.PostMessageW(wintypes.HWND(hc), BM_CLICK, 0, 0)
    time.sleep(0.3)
    set_text(hedit, edit_value)
    time.sleep(0.3)
    result["set_edit_readback"] = get_text(hedit)
    # 点【开始安装】
    hok = u32.GetDlgItem(wintypes.HWND(main), 1)
    u32.PostMessageW(wintypes.HWND(hok), BM_CLICK, 0, 0)
    # 处理弹出的对话框
    seen_local = {}
    deadline = time.time() + timeout
    last_sig = None
    while time.time() < deadline:
        ws = top_windows(pid)
        if not ws:
            break
        titles = []
        for h in ws:
            if h == main:
                t = get_text(u32.GetDlgItem(wintypes.HWND(main), 1001))
                if t:
                    result["edits"].append(t)
                titles.append("<main>")
                continue
            title = win_text(h)
            titles.append(title)
            if h in seen_local:
                continue
            ctexts = child_texts(h)
            seen_local[h] = (title, ctexts)
            result["seen"][h] = (title, ctexts)
            result["dismissed"].append((title, dismiss(h)))
        sig = tuple(sorted(titles))
        if sig != last_sig:
            result["timeline"].append("%.1fs %r" % (time.time() - (deadline - timeout), titles))
            last_sig = sig
        time.sleep(0.2)
    try:
        proc.wait(timeout=10)
        result["exited"] = True
        result["rc"] = proc.returncode
    except Exception:
        try: proc.kill()
        except Exception: pass
        result["rc"] = "killed"
    return result

def has_adjust_prompt(res):
    for h, (t, c) in res["seen"].items():
        if "已调整" in t:
            return True, t, c
    return False, None, None

def completion_text(res):
    for h, (t, c) in res["seen"].items():
        if "安装完成" in t:
            return " | ".join(c)
    for h, (t, c) in res["seen"].items():
        if "失败" in t:
            return "FAILBOX: " + " | ".join(c)
    return None

results = {}
def check(name, cond, ev):
    results[name] = bool(cond)
    log(("  [PASS] " if cond else "  [FAIL] ") + name + "  :: " + str(ev))
    return bool(cond)

def logrun(tag, r):
    log("  [%s] rc=%r exited=%s" % (tag, r["rc"], r["exited"]))
    log("  [%s] timeline:" % tag)
    for ln in r["timeline"]:
        log("      " + ln)
    log("  [%s] dismissed=%r" % (tag, r["dismissed"]))

def main():
    case = sys.argv[1] if len(sys.argv) > 1 else "all"
    os.makedirs(SB, exist_ok=True)
    real_before = tree(REAL)
    reg_before = reg_snapshot()          # ★ 关键：GUI 里的卸载会 RegDeleteTree，必须快照并还原
    log("沙箱 = %s" % SB)
    log("真实目录文件数=%d  注册表键=%s" % (len(real_before), reg_before[0]))
    exe = os.path.join(SB, "bin", "setup.exe")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    shutil.copy2(SETUP_SRC, exe)
    reg_set_str("InstallLocation", os.path.join(SB, "noop_nonexistent"))
    try:
        # ---- 子场景 B：目标不存在，不应乱弹 ----
        log("")
        log("===== V6-B 目标不存在（期望：不弹「已调整」）=====")
        fresh = os.path.join(SB, "b", "对口升学练习系统")
        r = run_gui(exe, fresh)
        logrun("B", r)
        log("  默认输入框=%r" % r["default_edit"])
        log("  设值回读=%r" % r.get("set_edit_readback"))
        log("  见到窗口=%r" % {v[0]: v[1] for v in r["seen"].values()})
        log("  输入框历史=%r" % r["edits"][:6])
        adj, at, ac = has_adjust_prompt(r)
        check("V6-B 未乱弹「已调整」", not adj, "adj=%s title=%r" % (adj, at))
        ct = completion_text(r)
        check("V6-B 弹出安装完成且含真实目录", bool(ct) and fresh in ct, "ctext=%r" % ct)
        check("V6-B 装到目标 6 文件", os.path.isdir(fresh) and core6(fresh) == 6,
              "core6=%s" % (core6(fresh) if os.path.isdir(fresh) else "NA"))
        # 卸载清理
        un = os.path.join(fresh, "卸载.exe")
        if os.path.exists(un):
            import subprocess; subprocess.run([un, "/S"], timeout=120)
            for _ in range(40):
                if not os.path.exists(os.path.join(fresh, "ExamDlgProj.exe")): break
                time.sleep(0.5)

        # ---- 子场景 A：目标被占 -> 加序号 + 弹提示 ----
        log("")
        log("===== V6-A 目标被外人占用（期望：弹「已调整」，装到 …1）=====")
        occ = os.path.join(SB, "a", "对口升学练习系统")
        os.makedirs(occ, exist_ok=True)
        open(os.path.join(occ, "外人文件.txt"), "w", encoding="utf-8").write("KEEP\n")
        occ1 = occ + "1"
        occ_before = tree(occ)
        reg_set_str("InstallLocation", os.path.join(SB, "noop_nonexistent"))
        r = run_gui(exe, occ)
        logrun("A", r)
        adj, at, ac = has_adjust_prompt(r)
        log("  见到窗口=%r" % {v[0]: v[1] for v in r["seen"].values()})
        log("  输入框历史=%r" % r["edits"][:8])
        check("V6-A 弹出「安装位置已调整」", adj, "title=%r text=%r" % (at, ac))
        check("V6-A 提示文本含 …1 真值", adj and any(occ1 in td for td in (ac or [])), "ac=%r" % ac)
        ct = completion_text(r)
        check("V6-A 完成框含真实目录 …1", bool(ct) and occ1 in ct, "ctext=%r" % ct)
        check("V6-A 装到 …1 6 文件", os.path.isdir(occ1) and core6(occ1) == 6,
              "core6=%s" % (core6(occ1) if os.path.isdir(occ1) else "NA"))
        check("V6-A 原同名目录零改动", tree(occ) == occ_before, "equal=%s" % (tree(occ) == occ_before))
        un = os.path.join(occ1, "卸载.exe")
        if os.path.exists(un):
            import subprocess; subprocess.run([un, "/S"], timeout=120)
            for _ in range(40):
                if not os.path.exists(os.path.join(occ1, "ExamDlgProj.exe")): break
                time.sleep(0.5)

        # ---- 子场景 F：Q: 不存在 -> 回退 ----
        log("")
        log("===== V6-F 目标盘 Q: 不存在（期望：弹「已调整(回退)」）=====")
        reg_set_str("InstallLocation", os.path.join(SB, "noop_nonexistent"))
        r = run_gui(exe, r"Q:\对口升学练习系统")
        logrun("F", r)
        adj, at, ac = has_adjust_prompt(r)
        log("  见到窗口=%r" % {v[0]: v[1] for v in r["seen"].values()})
        check("V6-F 弹出「已调整」", adj, "title=%r text=%r" % (at, ac))
        check("V6-F 提示为盘不可用回退", adj and any("盘" in td for td in (ac or [])), "ac=%r" % ac)
        ct = completion_text(r)
        check("V6-F 完成框落在可用固定盘", bool(ct) and ("C:\\对口升学练习系统" in ct or "D:\\对口升学练习系统" in ct),
              "ctext=%r" % ct)
        # 清理回退目录（只删带序号/或 D 盘的，绝不碰无后缀的 C:\）
        for extra in [r"C:\对口升学练习系统1", r"C:\对口升学练习系统2",
                      r"D:\对口升学练习系统", r"D:\对口升学练习系统1"]:
            if os.path.isdir(extra) and os.path.exists(os.path.join(extra, "卸载.exe")):
                import subprocess; subprocess.run([os.path.join(extra, "卸载.exe"), "/S"], timeout=120)
            rm(extra)

    except Exception:
        log("EXCEPTION:\n" + traceback.format_exc())
    finally:
        rm(SB)
        # 还原注册表卸载项（GUI 卸载动作会把它删掉）
        restored = reg_restore(reg_before)
        real_after = tree(REAL)
        log("")
        log("注册表还原=%s  当前项=%s" % (restored, reg_snapshot()[0]))
        check("事后 真实安装目录未变", real_before == real_after,
              "before=%d after=%d" % (len(real_before), len(real_after)))
        log("")
        log("==== 失败 %d ====" % sum(1 for v in results.values() if v is False))
    log("完成")

if __name__ == "__main__":
    main()
