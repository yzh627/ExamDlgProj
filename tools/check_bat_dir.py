# -*- coding: utf-8 -*-
"""校验 静默安装.bat 的默认目录改动结果，产出 UTF-8 报告"""
import hashlib, io

P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\发布包\静默安装.bat"
raw = open(P, "rb").read()
dec = raw.decode("gbk")

lines = []
lines.append("size=%d" % len(raw))
lines.append("sha256=%s" % hashlib.sha256(raw).hexdigest())
lines.append("BOM=%s" % (raw[:3] == b"\xef\xbb\xbf"))
lines.append("CRLF=%d  bare_LF=%d" % (raw.count(b"\r\n"), raw.count(b"\n") - raw.count(b"\r\n")))
lines.append("")
lines.append("--- 残留 D 盘默认目录检查（都应为 0）---")
for bad in ["D:\\对口升学练习系统", 'TARGET=D:', "有 D 盘就装", "set TARGET=D"]:
    lines.append("  %-26s = %d" % (bad, dec.count(bad)))
lines.append("")
lines.append("--- 应存在的 C 盘默认（都应 >=1）---")
for good in ["set \"TARGET=C:\\对口升学练习系统\"", "set TARGET=C:\\你要的目录",
             "SETUP_EXE=C:\\某处\\对口升学练习系统_安装程序.exe",
             "rem    · 都没设：装 C:\\对口升学练习系统"]:
    lines.append("  %-50s = %d" % (good, dec.count(good)))
lines.append("")
lines.append("--- 默认目录那几行原文 ---")
for i, ln in enumerate(dec.split("\r\n"), 1):
    if "TARGET" in ln and ("C:\\对口升学" in ln or "D:\\对口升学" in ln or "你要的目录" in ln):
        lines.append("  %4d| %s" % (i, ln))

open(r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\tools\_bat_dir_report.txt", "w", encoding="utf-8").write("\n".join(lines))
print("ok")
