# -*- coding: utf-8 -*-
"""把真实机的卸载注册表项里被测试残留污染的 DisplayIcon/UninstallString 修正回真实目录。"""
import io, time, ctypes
from ctypes import wintypes
advapi32 = ctypes.windll.advapi32
SUBKEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\HebeiExamPractice"
REAL = r"C:\对口升学练习系统"
OUT = r"C:\Users\30601\Desktop\_qa_regfix.txt"

def set_str(name, value):
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

def get(name):
    h = wintypes.HKEY()
    if advapi32.RegOpenKeyExW(wintypes.HKEY(0x80000001), ctypes.c_wchar_p(SUBKEY), 0, 0x20019, ctypes.byref(h)) != 0:
        return None
    dl = wintypes.DWORD(0); ty = wintypes.DWORD()
    if advapi32.RegQueryValueExW(h, ctypes.c_wchar_p(name), None, ctypes.byref(ty), None, ctypes.byref(dl)) != 0:
        advapi32.RegCloseKey(h); return None
    buf = ctypes.create_string_buffer(dl.value + 2)
    advapi32.RegQueryValueExW(h, ctypes.c_wchar_p(name), None, ctypes.byref(ty), buf, ctypes.byref(dl))
    advapi32.RegCloseKey(h)
    return buf.raw[:dl.value].decode("utf-16-le").rstrip("\0")

out = []
out.append("改前 DisplayIcon=%r" % get("DisplayIcon"))
out.append("改前 UninstallString=%r" % get("UninstallString"))
set_str("DisplayIcon", REAL + r"\ExamDlgProj.exe")
set_str("UninstallString", '"' + REAL + r'\卸载.exe" /uninstall')
set_str("InstallLocation", REAL)
out.append("改后 DisplayIcon=%r" % get("DisplayIcon"))
out.append("改后 UninstallString=%r" % get("UninstallString"))
out.append("改后 InstallLocation=%r" % get("InstallLocation"))
io.open(OUT, "w", encoding="utf-8").write("\n".join(out))
