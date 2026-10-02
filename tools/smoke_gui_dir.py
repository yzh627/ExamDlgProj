# -*- coding: utf-8 -*-
"""界面版轻量冒烟（不安装）：打开安装对话框 -> 读 IDC_EDIT_DIR 默认值 -> 关闭。"""
import io, os, time, ctypes, subprocess, shutil, traceback
from ctypes import wintypes

OUTF = r"C:\Users\30601\Desktop\_gui_test_out.txt"
PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
SETUP_SRC = PROJ + r"\发布包\对口升学练习系统_安装程序.exe"
SB = r"C:\_hbtest_gui"
u32 = ctypes.windll.user32

def log(s):
    with io.open(OUTF, "a", encoding="utf-8") as f:
        f.write(str(s) + "\n")

open(OUTF, "w").close()
proc = None
try:
    log("step0 imports ok, python=%s" % __import__("sys").version.split()[0])
    os.makedirs(SB, exist_ok=True)
    exe = os.path.join(SB, "setup_gui.exe")
    shutil.copy2(SETUP_SRC, exe)
    log("step1 copied")

    proc = subprocess.Popen([exe])
    pid = proc.pid
    log("step2 launched pid=%d" % pid)

    WM_COMMAND = 0x0111
    WM_CLOSE = 0x0010
    WM_GETTEXT = 0x000D
    IDCANCEL = 2
    IDC_EDIT_DIR = 1001
    TITLE = "河北对口升学计算机程序设计练习系统 - 安装"

    hdlg = 0
    deadline = time.time() + 15
    while time.time() < deadline:
        h = u32.FindWindowW(None, TITLE)
        if h:
            wpid = wintypes.DWORD()
            u32.GetWindowThreadProcessId(wintypes.HWND(h), ctypes.byref(wpid))
            if wpid.value == pid and u32.IsWindowVisible(wintypes.HWND(h)):
                hdlg = h
                break
        time.sleep(0.3)
    log("step3 hdlg=%d" % hdlg)

    if not hdlg:
        log("[FAIL] 没找到安装对话框")
    else:
        time.sleep(1.0)   # 等 WM_INITDIALOG 把默认目录填进去
        log("[PASS] 找到安装对话框")
        hedit = u32.GetDlgItem(wintypes.HWND(hdlg), IDC_EDIT_DIR)
        log("  IDC_EDIT_DIR hwnd=%d" % hedit)
        # 跨进程读控件文本必须用 SendMessage(WM_GETTEXT)（GetWindowText 对别的进程的
        # 子控件会返回空 —— 这是系统设计，不是控件没文本）
        buf = ctypes.create_unicode_buffer(512)
        u32.SendMessageW(wintypes.HWND(hedit), WM_GETTEXT, 512, ctypes.byref(buf))
        log("  默认安装目录=%r" % buf.value)
        log("  %s 默认目录 == C:\\对口升学练习系统" % ("[PASS]" if buf.value == r"C:\对口升学练习系统" else "[FAIL]"))
        u32.PostMessageW(wintypes.HWND(hdlg), WM_CLOSE, 0, 0)
        log("step4 posted WM_CLOSE")

    try:
        proc.wait(timeout=10)
        log("  对话框已关闭，退出码=%s" % proc.returncode)
    except Exception:
        log("  未退出，强杀")
        proc.kill()
except Exception:
    log("EXCEPTION:\n" + traceback.format_exc())
finally:
    if proc is not None:
        try:
            proc.kill()
        except Exception:
            pass
    shutil.rmtree(SB, ignore_errors=True)
    log("沙箱残留=%s  发布包 last_dir=%s" % (
        os.path.exists(SB), os.path.exists(PROJ + r"\发布包\last_dir.txt")))
