# -*- coding: utf-8 -*-
"""转储真实机卸载键全部值，确认 QA 修复后 DisplayIcon/UninstallString 指向真实路径。"""
import winreg, io, os

KEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
out = []
def log(s): out.append(s)

def dump(hive, hive_name, subkey):
    log("== %s\\%s ==" % (hive_name, subkey))
    try:
        k = winreg.OpenKey(hive, subkey)
    except FileNotFoundError:
        log("  <键不存在>")
        return
    n_sub = winreg.QueryInfoKey(k)[0]
    i = 0
    while True:
        try:
            name, val, typ = winreg.EnumValue(k, i); i += 1
            log("  %-18s = %r" % (name or "(Default)", val))
        except OSError:
            break
    if n_sub:
        try:
            j = 0
            while True:
                sn = winreg.EnumKey(k, j); j += 1
                log("  [子键] %s" % sn)
        except OSError:
            pass
    winreg.CloseKey(k)

dump(winreg.HKEY_CURRENT_USER, "HKCU", KEY)

# 真实安装目录是否与 InstallLocation 一致
log("")
try:
    k = winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY)
    loc, _ = winreg.QueryValueEx(k, "InstallLocation")
    winreg.CloseKey(k)
    log("InstallLocation=%r  exists=%s" % (loc, os.path.isdir(loc)))
except Exception as e:
    log("InstallLocation 读取失败: %r" % e)

io.open(r"C:\Users\30601\Desktop\_reg_dump.txt", "w", encoding="utf-8").write("\n".join(out))
print("done")
