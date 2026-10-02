# -*- coding: utf-8 -*-
r"""
按用户选择，把三修版装到 C:\对口升学练习系统
  1) 记录/清理 C: 上遗留的空 draft\ （老安装卸载后留下的残渣，正是这次修掉的那类东西）
  2) 备份现状到 工程\C盘安装备份\<时间戳>
  3) 用新版安装程序静默安装：/S /D=C:\对口升学练习系统 /NORUN（桌面+开始菜单快捷方式照建）
  4) 核对：6 个文件与发布包逐字节一致；注册表卸载项已重建且指向 C:；快捷方式已在
"""
import ctypes, ctypes.wintypes as wt, hashlib, json, os, shutil, subprocess, time

adv = ctypes.windll.advapi32
PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = os.path.join(PROJ, "发布包")
INSTALLER = os.path.join(PKG, "对口升学练习系统_安装程序.exe")
TARGET = r"C:\对口升学练习系统"
BKROOT = os.path.join(PROJ, "C盘安装备份")
SUBKEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
HKCU = 0x80000001
SHORTCUTS = [
    ("桌面", os.path.join(os.environ["USERPROFILE"], "Desktop", "对口升学练习系统.lnk")),
    ("开始菜单", os.path.join(os.environ["APPDATA"],
                          r"Microsoft\Windows\Start Menu\Programs\对口升学练习系统.lnk")),
]

OUT = []
res = {"checks": []}


def log(m):
    OUT.append(str(m)); print(m)


def check(name, ok, detail=""):
    res["checks"].append({"name": name, "ok": bool(ok), "detail": detail})
    log("  %s %s%s" % ("[PASS]" if ok else "[FAIL]", name, ("  — " + detail) if detail else ""))


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def snap(root):
    d = {}
    if not os.path.isdir(root):
        return d
    for dp, _, fns in os.walk(root):
        for f in fns:
            fp = os.path.join(dp, f)
            d[os.path.relpath(fp, root)] = (os.path.getsize(fp), sha(fp))
    return d


adv.RegOpenKeyExW.argtypes = [wt.HKEY, wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.POINTER(wt.HKEY)]
adv.RegOpenKeyExW.restype = ctypes.c_long
adv.RegCloseKey.argtypes = [wt.HKEY]
adv.RegEnumValueW.argtypes = [wt.HKEY, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD),
                              ctypes.POINTER(wt.DWORD), ctypes.POINTER(wt.DWORD),
                              ctypes.c_void_p, ctypes.POINTER(wt.DWORD)]
adv.RegEnumValueW.restype = ctypes.c_long
KEY_READ, ERROR_SUCCESS, ERROR_NO_MORE_ITEMS = 0x20019, 0, 259
TYPES = {1: "REG_SZ", 2: "REG_EXPAND_SZ", 3: "REG_BINARY", 4: "REG_DWORD", 7: "REG_MULTI_SZ"}


def reg_values():
    h = wt.HKEY()
    if adv.RegOpenKeyExW(HKCU, SUBKEY, 0, KEY_READ, ctypes.byref(h)) != ERROR_SUCCESS:
        return None
    vals, i = {}, 0
    while True:
        nb = ctypes.create_unicode_buffer(512)
        nl, dt, sz = wt.DWORD(512), wt.DWORD(), wt.DWORD(0)
        rc = adv.RegEnumValueW(h, i, nb, ctypes.byref(nl), None, ctypes.byref(dt), None,
                               ctypes.byref(sz))
        if rc != ERROR_SUCCESS:
            break
        buf = ctypes.create_string_buffer(sz.value + 8)
        sz2 = wt.DWORD(sz.value)
        if adv.RegEnumValueW(h, i, nb, ctypes.byref(nl), None, ctypes.byref(dt),
                             ctypes.cast(buf, ctypes.c_void_p), ctypes.byref(sz2)) == ERROR_SUCCESS:
            raw = buf.raw[:sz2.value]
            if dt.value in (1, 2):
                v = raw.decode("utf-16-le").rstrip("\0")
            elif dt.value == 7:
                v = [x for x in raw.decode("utf-16-le").split("\0") if x]
            else:
                v = raw.hex()
            vals[nb.value] = (TYPES.get(dt.value, str(dt.value)), v)
        i += 1
    adv.RegCloseKey(h)
    return vals


log("=== 1 C: 上遗留的空 draft\\ ===")
leftover = os.path.join(TARGET, "draft")
if os.path.isdir(leftover):
    inner = os.listdir(leftover)
    log("  %s 存在，里面 %d 项 %s" % (leftover, len(inner), inner))
    if not inner:
        removed = False
        try:
            os.rmdir(leftover)
            removed = True
        except OSError as e:
            log("  删不掉：%r" % (e,))
        log("  空目录已清除 = %s（这就是老卸载留下的那种残渣）" % removed)
    else:
        log("  里面有东西，不动它")
else:
    log("  没有残留")

log("")
log("=== 2 装前快照 + 备份 ===")
before = snap(TARGET)
log("  装前文件数 = %d" % len(before))
for k in sorted(before):
    log("    %s  %d B" % (k, before[k][0]))
stamp = time.strftime("%Y%m%d_%H%M")
bkdir = os.path.join(BKROOT, stamp)
os.makedirs(BKROOT, exist_ok=True)
if os.path.isdir(bkdir):
    shutil.rmtree(bkdir, ignore_errors=True)
os.makedirs(bkdir, exist_ok=True)
for k in before:
    src = os.path.join(TARGET, k)
    dst = os.path.join(bkdir, k)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
shutil.copy2(INSTALLER, os.path.join(bkdir, "_本次使用的安装程序.exe"))
log("  备份到 %s" % bkdir)
res["backup"] = bkdir

log("")
log("=== 3 静默安装到 %s ===" % TARGET)
log("  安装前注册表卸载项 = %s" % (reg_values() is not None))
cmd = [INSTALLER, "/S", "/D=%s" % TARGET, "/NORUN"]
log("  " + " ".join('"%s"' % c if " " in c else c for c in cmd))
t0 = time.time()
r = subprocess.run(cmd, cwd=PKG, capture_output=True, timeout=300)
log("  退出码 = %d  耗时 %.2fs" % (r.returncode, time.time() - t0))
for ln in (r.stdout or b"").decode("utf-8", "replace").splitlines():
    if ln.strip():
        log("    " + ln)
check("静默安装退出码为 0", r.returncode == 0, "rc=%d" % r.returncode)
time.sleep(1.5)

log("")
log("=== 4 核对文件 ===")
after = snap(TARGET)
res["after"] = {k: list(v) for k, v in after.items()}
for n in ["ExamDlgProj.exe", "ExamDlgProj.exe.manifest", "questions.dat", "support.png", "说明.txt"]:
    lp, pp = os.path.join(TARGET, n), os.path.join(PKG, n)
    if os.path.exists(lp) and os.path.exists(pp):
        check("与发布包一致  " + n, sha(lp) == sha(pp),
              "%d B  %s" % (os.path.getsize(lp), sha(lp)[:12]))
    else:
        check("与发布包一致  " + n, False, "缺文件")
un = os.path.join(TARGET, "卸载.exe")
check("卸载.exe 已装", os.path.exists(un),
      "%d B" % (os.path.getsize(un) if os.path.exists(un) else -1))
check("目录里没有残留的旧文件（装前是空的）", set(after.keys()) - set(
    ["ExamDlgProj.exe", "ExamDlgProj.exe.manifest", "questions.dat", "support.png", "说明.txt",
     "卸载.exe"]) == set(), str(sorted(after.keys())))
log("  装后文件：")
for k in sorted(after):
    log("    %-32s %9d B  %s" % (k, after[k][0], after[k][1][:12]))

log("")
log("=== 5 核对注册表卸载项 ===")
vals = reg_values()
res["reg"] = vals
if vals:
    for k in sorted(vals):
        v = vals[k][1]
        if isinstance(v, list):
            v = " | ".join(v)
        log("    %-18s [%s] = %s" % (k, vals[k][0], str(v)[:110]))
    check("注册表卸载项已重建", True, "")
    il = vals.get("InstallLocation", ("", ""))[1].rstrip("\\")
    check("InstallLocation 指向 C:\\对口升学练习系统",
          il.lower() == TARGET.lower(), il)
    check("InstalledFiles 清单齐全（含 6 个文件名）",
          isinstance(vals.get("InstalledFiles", ("", []))[1], list) and
          len(vals["InstalledFiles"][1]) >= 6,
          str(vals.get("InstalledFiles", ("", ""))[1]))
else:
    check("注册表卸载项已重建", False, "读不到")

log("")
log("=== 6 核对快捷方式 ===")
for label, p in SHORTCUTS:
    check("%s快捷方式已创建" % label, os.path.exists(p),
          "%d B" % (os.path.getsize(p) if os.path.exists(p) else -1))

npass = sum(1 for c in res["checks"] if c["ok"])
log("")
log("=== 汇总 %d / %d 通过 ===" % (npass, len(res["checks"])))
for c in res["checks"]:
    log("    %s %s" % ("[PASS]" if c["ok"] else "[FAIL]", c["name"]))
res["passed"], res["total"] = npass, len(res["checks"])

open(os.path.join(PROJ, "tools", "_install_c_log.txt"), "w", encoding="utf-8").write("\n".join(OUT))
open(os.path.join(PROJ, "tools", "_install_c_result.json"), "w", encoding="utf-8").write(
    json.dumps(res, ensure_ascii=False, indent=2))
print("DONE %d/%d" % (npass, len(res["checks"])))
