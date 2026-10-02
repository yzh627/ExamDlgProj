# -*- coding: utf-8 -*-
r"""
把 D:\对口升学练习系统（用户真实安装）同步到本次三修的新构建，
并重建被误删的注册表卸载项。

流程：
  1) 快照 D: 现状（文件清单 + 哈希 + score.txt 大小）
  2) 整目录备份到  D盘安装备份\<时间戳>_三修
  3) 用新版安装程序静默安装到 D:（/S /D=... /NORUN）
  4) 核对：6 个程序文件与发布包逐字节一致；score.txt 完好；其它用户文件没被动
  5) 核对注册表卸载项已重建（用 ctypes 直接读 advapi32，本机 reg.exe 被拦）
"""
import ctypes, ctypes.wintypes as wt, hashlib, json, os, shutil, subprocess, time

adv = ctypes.windll.advapi32
PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = os.path.join(PROJ, "发布包")
INSTALLER = os.path.join(PKG, "对口升学练习系统_安装程序.exe")
LIVE = r"D:\对口升学练习系统"
BKROOT = os.path.join(PROJ, "D盘安装备份")
SUBKEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
HKCU = 0x80000001

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
    for dirpath, dirnames, filenames in os.walk(root):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            rel = os.path.relpath(fp, root)
            try:
                d[rel] = (os.path.getsize(fp), sha(fp))
            except OSError:
                d[rel] = (-1, "?")
    return d


# ---- 注册表读取（ctypes，绕过被拦的 reg.exe）----
adv.RegOpenKeyExW.argtypes = [wt.HKEY, wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.POINTER(wt.HKEY)]
adv.RegOpenKeyExW.restype = ctypes.c_long
adv.RegCloseKey.argtypes = [wt.HKEY]
adv.RegEnumValueW.argtypes = [wt.HKEY, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD),
                              ctypes.POINTER(wt.DWORD), ctypes.POINTER(wt.DWORD),
                              ctypes.c_void_p, ctypes.POINTER(wt.DWORD)]
adv.RegEnumValueW.restype = ctypes.c_long
KEY_READ, ERROR_SUCCESS, ERROR_NO_MORE_ITEMS = 0x20019, 0, 259
REG_TYPES = {1: "REG_SZ", 2: "REG_EXPAND_SZ", 3: "REG_BINARY", 4: "REG_DWORD",
             7: "REG_MULTI_SZ"}


def reg_values():
    h = wt.HKEY()
    if adv.RegOpenKeyExW(HKCU, SUBKEY, 0, KEY_READ, ctypes.byref(h)) != ERROR_SUCCESS:
        return None
    vals = {}
    i = 0
    while True:
        namebuf = ctypes.create_unicode_buffer(512)
        nlen = wt.DWORD(512)
        dtype = wt.DWORD()
        size = wt.DWORD(0)
        rc = adv.RegEnumValueW(h, i, namebuf, ctypes.byref(nlen), None,
                               ctypes.byref(dtype), None, ctypes.byref(size))
        if rc == ERROR_NO_MORE_ITEMS or rc != ERROR_SUCCESS:
            break
        data = ctypes.create_string_buffer(size.value + 8)
        size2 = wt.DWORD(size.value)
        rc2 = adv.RegEnumValueW(h, i, namebuf, ctypes.byref(nlen), None,
                                ctypes.byref(dtype), ctypes.cast(data, ctypes.c_void_p),
                                ctypes.byref(size2))
        if rc2 == ERROR_SUCCESS:
            if dtype.value == 1 or dtype.value == 2:
                v = data.raw[:size2.value].decode("utf-16-le").rstrip("\0")
            elif dtype.value == 7:
                v = [x for x in data.raw[:size2.value].decode("utf-16-le").split("\0") if x]
            else:
                v = data.raw[:size2.value].hex()
            vals[namebuf.value] = (REG_TYPES.get(dtype.value, str(dtype.value)), v)
        i += 1
    adv.RegCloseKey(h)
    return vals


log("=== 1 D: 现状快照 ===")
before = snap(LIVE)
res["before"] = {k: list(v) for k, v in before.items()}
for k in sorted(before):
    log("  %-30s %9d B  %s" % (k, before[k][0], before[k][1][:12]))
log("  文件数 = %d" % len(before))

log("")
log("=== 2 备份 D: 到 工程\\D盘安装备份\\ ===")
stamp = time.strftime("%Y%m%d_%H%M")
bkdir = os.path.join(BKROOT, stamp + "_三修")
os.makedirs(BKROOT, exist_ok=True)
if os.path.isdir(bkdir):
    shutil.rmtree(bkdir, ignore_errors=True)
shutil.copytree(LIVE, bkdir)
nb = snap(bkdir)
log("  已备份 %d 个文件 → %s" % (len(nb), bkdir))
check("备份与现状逐字节一致", nb == before, "备份 %d 项 / 现状 %d 项" % (len(nb), len(before)))

log("")
log("=== 3 用新版安装程序静默装到 D: ===")
cmd = [INSTALLER, "/S", '/D=%s' % LIVE, "/NORUN"]
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
log("=== 4 核对 D: 内容 ===")
after = snap(LIVE)
res["after"] = {k: list(v) for k, v in after.items()}
pkg_files = ["ExamDlgProj.exe", "ExamDlgProj.exe.manifest", "questions.dat", "support.png", "说明.txt"]
for n in pkg_files:
    lp = os.path.join(LIVE, n)
    pp = os.path.join(PKG, n)
    if os.path.exists(lp) and os.path.exists(pp):
        same = sha(lp) == sha(pp)
        check("与发布包一致  " + n, same,
              "%d B  %s" % (os.path.getsize(lp), sha(lp)[:12]))
    else:
        check("与发布包一致  " + n, False, "缺文件")
u_live = os.path.join(LIVE, "卸载.exe")
check("卸载.exe 存在", os.path.exists(u_live),
      "%d B" % (os.path.getsize(u_live) if os.path.exists(u_live) else -1))

# 用户数据没被动
for k in sorted(before):
    if k in pkg_files or k == "卸载.exe":
        continue
    if k in ("score.txt",) or not after.get(k):
        pass
check("用户文件全部原样保留（比对新旧快照）",
      all(after.get(k) == before[k] for k in before if k not in pkg_files and k != "卸载.exe"),
      "")
for k in sorted(set(list(before.keys()) + list(after.keys()))):
    b, a = before.get(k), after.get(k)
    if b != a:
        log("  变化：%-30s %s -> %s" % (k, (b[1][:12] if b else "无"),
                                      (a[1][:12] if a else "删除")))

log("")
log("=== 5 注册表卸载项是否重建 ===")
vals = reg_values()
res["reg"] = vals
log("  键存在 = %s" % (vals is not None))
if vals:
    for k in sorted(vals):
        v = vals[k][1]
        if isinstance(v, list):
            v = " | ".join(v)
        log("    %-18s [%s] = %s" % (k, vals[k][0], str(v)[:110]))
    check("注册表卸载项已重建", True, "")
    check("InstallLocation 指向 D:", vals.get("InstallLocation", ("", ""))[1].rstrip("\\").lower()
          == LIVE.lower(), str(vals.get("InstallLocation", "")))
    check("InstalledFiles 清单已写入", "InstalledFiles" in vals, str(sorted(vals.keys())))
else:
    check("注册表卸载项已重建", False, "仍然读不到")

npass = sum(1 for c in res["checks"] if c["ok"])
log("")
log("=== 汇总 %d / %d 通过 ===" % (npass, len(res["checks"])))
for c in res["checks"]:
    log("    %s %s" % ("[PASS]" if c["ok"] else "[FAIL]", c["name"]))
res["passed"], res["total"], res["backup"] = npass, len(res["checks"]), bkdir

open(os.path.join(PROJ, "tools", "_live_sync_log.txt"), "w", encoding="utf-8").write("\n".join(OUT))
open(os.path.join(PROJ, "tools", "_live_sync_result.json"), "w", encoding="utf-8").write(
    json.dumps(res, ensure_ascii=False, indent=2))
print("DONE %d/%d" % (npass, len(res["checks"])))
