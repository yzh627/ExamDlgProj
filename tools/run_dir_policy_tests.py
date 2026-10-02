# -*- coding: utf-8 -*-
"""沙箱功能测试（不碰真实安装）：
   T1 同名加序号：目标被占 -> 装到 ...1；不删"别人"的目录；旧位置(合成)被清，含 score
   T2 盘符回退：/D 在不存在的盘 -> 回退到第一个可用固定盘；撞到真实目录则加序号
   T3 就地升级：注册表旧位置 == 目标 -> 不改名、保住 score
  全程快照并恢复 HKCU 卸载键；快照真实安装目录，测试后逐项核对未被改动。
"""
import io, os, sys, shutil, subprocess, hashlib, winreg, traceback

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = PROJ + r"\发布包"
SETUP_SRC = PKG + r"\对口升学练习系统_安装程序.exe"
REAL = r"C:\对口升学练习系统"
SB = r"C:\_hbtest"
KEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
HKCU = winreg.HKEY_CURRENT_USER
FILES6 = ["ExamDlgProj.exe", "ExamDlgProj.exe.manifest", "questions.dat",
          "support.png", "说明.txt", "卸载.exe"]
out = []
def log(s): out.append(s)

# ---------- registry helpers ----------
def reg_snap():
    k = winreg.OpenKey(HKCU, KEY)
    vals = []
    i = 0
    while True:
        try:
            vals.append(winreg.EnumValue(k, i)); i += 1
        except OSError:
            break
    winreg.CloseKey(k)
    return vals

def reg_del():
    try:
        winreg.DeleteKey(HKCU, KEY)
    except FileNotFoundError:
        pass

def reg_restore(vals):
    import time
    time.sleep(3.0)   # 卸载刚删过键，等 Windows 把"标记为删除"消化掉
    last = None
    for _ in range(120):
        try:
            reg_del()
            k = winreg.CreateKey(HKCU, KEY)
            for name, val, typ in vals:
                winreg.SetValueEx(k, name, 0, typ, val)
            winreg.CloseKey(k)
            return
        except OSError as e:
            last = e
            time.sleep(0.5)
    raise last

def reg_set_loc(path):
    import time
    last = None
    for _ in range(20):
        try:
            k = winreg.CreateKey(HKCU, KEY)   # 键可能被上一步卸载删掉了，缺了就建
            winreg.SetValueEx(k, "InstallLocation", 0, winreg.REG_SZ, path)
            winreg.CloseKey(k)
            return
        except OSError as e:
            last = e
            time.sleep(0.3)   # 卸载把键删了，可能短暂处于"标记为删除"状态
    raise last

def reg_get_loc():
    try:
        k = winreg.OpenKey(HKCU, KEY)
        v, _ = winreg.QueryValueEx(k, "InstallLocation")
        winreg.CloseKey(k)
        return v
    except FileNotFoundError:
        return ""

# 子进程模式：本进程可能残留注册表句柄，导致"标记为删除"一直消不掉；
# 独立进程没有这个包袱，恢复最稳。主流程在 finally 里调用它兜底。
if len(sys.argv) > 2 and sys.argv[1] == "--restore":
    import json, time
    vals = json.load(open(sys.argv[2], encoding="utf-8"))
    okr = False
    lastr = None
    for _ in range(240):
        try:
            reg_del()
            k = winreg.CreateKey(HKCU, KEY)
            for name, val, typ in vals:
                winreg.SetValueEx(k, name, 0, typ, val)
            winreg.CloseKey(k)
            okr = True
            break
        except OSError as e:
            lastr = e
            time.sleep(0.5)
    sys.stdout.write("RESTORE_OK" if okr else ("RESTORE_FAIL %r" % lastr))
    sys.exit(0)

# ---------- fs helpers ----------
def direntries(path):
    res = {}
    try:
        for r, ds, fs in os.walk(path):
            for f in fs:
                p = os.path.join(r, f)
                res[os.path.relpath(p, path)] = os.path.getsize(p)
    except Exception:
        pass
    return res

def rm(path):
    try:
        if os.path.isdir(path):
            shutil.rmtree(path, ignore_errors=True)
        elif os.path.exists(path):
            os.remove(path)
    except Exception:
        pass

def run_setup(args, setup_exe):
    p = subprocess.run([setup_exe] + args, capture_output=True, timeout=180)
    return p.returncode

def last_dir_file(setup_dir):
    p = os.path.join(setup_dir, "last_dir.txt")
    if not os.path.exists(p):
        return None
    return open(p, "rb").read().decode("gbk").strip()

def log_final_dir(setup_dir):
    p = os.path.join(setup_dir, "install.log")
    if not os.path.exists(p):
        return None
    t = open(p, "rb").read().decode("utf-8-sig", "replace")
    for ln in t.splitlines():
        if "FINAL_DIR=" in ln:
            return ln.split("FINAL_DIR=", 1)[1].strip()
    return None

def core_count(d):
    return sum(1 for f in FILES6 if os.path.exists(os.path.join(d, f)))

def ok(cond, msg):
    log(("  [PASS] " if cond else "  [FAIL] ") + msg)
    return cond

# ---------- main ----------
fails = 0
real_before = direntries(REAL)
pkg_before = sorted(os.listdir(PKG))
reg_backup = reg_snap()
log("== 环境 ==")
log("  REAL=%s entries=%d" % (REAL, len(real_before)))
log("  REG InstallLocation=%r" % reg_get_loc())

setup_exe = None
cleanup_paths = []   # 只清理测试自己创建的目录，绝不广撒网
try:
    rm(SB)
    os.makedirs(SB, exist_ok=True)
    setup_exe = os.path.join(SB, "setup.exe")
    shutil.copy2(SETUP_SRC, setup_exe)

    # ---------------- T1 ----------------
    log("== T1 同名加序号 + 不误删他人目录 + 旧位置(合成)清理 ==")
    old1 = os.path.join(SB, "old")
    os.makedirs(old1, exist_ok=True)
    shutil.copy2(PKG + r"\ExamDlgProj.exe", os.path.join(old1, "ExamDlgProj.exe"))
    open(os.path.join(old1, "score.txt"), "w", encoding="utf-8").write("OLD SCORE 1\n")
    reg_set_loc(old1)
    occ = os.path.join(SB, "target", "对口升学练习系统")
    os.makedirs(occ, exist_ok=True)
    open(os.path.join(occ, "别人的文件.txt"), "w", encoding="utf-8").write("not ours\n")
    rc = run_setup(["/S", "/NODESKTOP", "/NOSTARTMENU", "/D=" + occ], setup_exe)
    fin1 = os.path.join(SB, "target", "对口升学练习系统1")
    log("  rc=%d" % rc)
    fails += not ok(rc == 0, "退出码 0")
    fails += not ok(os.path.isdir(fin1), "已装到 ...1：%s" % fin1)
    fails += not ok(core_count(fin1) == 6, "最终目录 6 个核心文件齐（%d/6）" % core_count(fin1))
    fails += not ok(os.path.exists(os.path.join(occ, "别人的文件.txt")), "『别人』目录里的文件没被删")
    fails += not ok(os.path.isdir(old1 + "") is False or not os.path.exists(old1), "旧位置(合成)已被清理")
    fails += not ok(not os.path.exists(os.path.join(old1, "score.txt")), "旧位置 score.txt 随旧位置清掉")
    fails += not ok(log_final_dir(SB) == fin1, "install.log FINAL_DIR == %s" % fin1)
    fails += not ok(last_dir_file(SB) == fin1, "last_dir.txt == %s" % fin1)
    # 卸载清理
    run_setup(["/S"], os.path.join(fin1, "卸载.exe"))
    rm(fin1); rm(os.path.join(SB, "target"))

    # ---------------- T2 ----------------
    log("== T2 盘符回退（Q: 不存在）==")
    old2 = os.path.join(SB, "old2")
    os.makedirs(old2, exist_ok=True)
    shutil.copy2(PKG + r"\ExamDlgProj.exe", os.path.join(old2, "ExamDlgProj.exe"))
    open(os.path.join(old2, "score.txt"), "w", encoding="utf-8").write("OLD SCORE 2\n")
    reg_set_loc(old2)
    rc = run_setup(["/S", "/NODESKTOP", "/NOSTARTMENU", "/D=" + r"Q:\对口升学练习系统"], setup_exe)
    expT2 = r"C:\对口升学练习系统1"
    cleanup_paths.append(expT2)
    log("  rc=%d" % rc)
    fails += not ok(rc == 0, "退出码 0（回退成功）")
    lf = log_final_dir(SB); ld = last_dir_file(SB)
    fails += not ok(lf == expT2, "FINAL_DIR 回退+加序号 == %s（实际 %s）" % (expT2, lf))
    fails += not ok(ld == expT2, "last_dir.txt == %s（实际 %s）" % (expT2, ld))
    fails += not ok(os.path.isdir(expT2) and core_count(expT2) == 6, "回退目录 6 个文件齐")
    fails += not ok(last_dir_file(SB) == expT2, "last_dir 一致")
    run_setup(["/S"], os.path.join(expT2, "卸载.exe"))
    rm(expT2)

    # ---------------- T3 ----------------
    log("== T3 就地升级（注册表旧位置==目标，不改名，保 score）==")
    t3 = os.path.join(SB, "t3", "对口升学练习系统")
    os.makedirs(t3, exist_ok=True)
    shutil.copy2(PKG + r"\ExamDlgProj.exe", os.path.join(t3, "ExamDlgProj.exe"))
    open(os.path.join(t3, "score.txt"), "w", encoding="utf-8").write("KEEP ME\n")
    reg_set_loc(t3)
    rc = run_setup(["/S", "/NODESKTOP", "/NOSTARTMENU", "/D=" + t3], setup_exe)
    log("  rc=%d" % rc)
    fails += not ok(rc == 0, "退出码 0")
    fails += not ok(last_dir_file(SB) == t3, "没加序号，仍装到原地：%s" % t3)
    fails += not ok(os.path.exists(os.path.join(t3, "score.txt")), "score.txt 保住了")
    fails += not ok(core_count(t3) == 6, "原地升级 6 个文件齐")
    run_setup(["/S"], os.path.join(t3, "卸载.exe"))
    rm(os.path.join(SB, "t3"))

except Exception:
    log("EXCEPTION:\n" + traceback.format_exc())
    fails += 1
finally:
    # 恢复注册表（先在本进程试；不行就用独立子进程恢复，绕开本进程残留句柄）
    try:
        reg_restore(reg_backup)
    except Exception as e:
        log("REG RESTORE(本进程) 失败 %r -> 改用独立进程" % e)
        try:
            import json
            snapf = r"C:\Users\30601\Desktop\_hb_reg_snap.json"
            io.open(snapf, "w", encoding="utf-8").write(json.dumps(reg_backup, ensure_ascii=False))
            r = subprocess.run([sys.executable, os.path.abspath(__file__), "--restore", snapf],
                               capture_output=True, text=True, timeout=180)
            log("REG RESTORE(独立进程): %s" % (r.stdout or "").strip())
        except Exception as e2:
            log("REG RESTORE(独立进程) 失败 %r" % e2)
    # 清理沙箱
    rm(SB)
    # 只清理测试自己可能建出来的目录（精确清单，不做通配删除）
    for p in cleanup_paths:
        rm(p)

# ---------- 事后核对 ----------
log("== 事后核对 ==")
real_after = direntries(REAL)
same = real_before == real_after
fails += not ok(same, "真实安装目录 %s 逐文件未变（before=%d after=%d）" % (REAL, len(real_before), len(real_after)))
if not same:
    added = set(real_after) - set(real_before)
    removed = set(real_before) - set(real_after)
    log("    added=%r removed=%r" % (list(added)[:10], list(removed)[:10]))
pkg_after = sorted(os.listdir(PKG))
fails += not ok(pkg_before == pkg_after, "发布包 文件清单未变（测试未污染发布包）")
loc_now = reg_get_loc()
orig_loc = ""
for nm, vv, tt in reg_backup:
    if nm == "InstallLocation":
        orig_loc = vv
fails += not ok(loc_now == orig_loc, "注册表 InstallLocation 已恢复为 %r（实际 %r）" % (orig_loc, loc_now))

log("")
log("TOTAL FAILS = %d" % fails)
io.open(r"C:\Users\30601\Desktop\_func_test_out.txt", "w", encoding="utf-8").write("\n".join(out))
print("done fails=%d" % fails)
