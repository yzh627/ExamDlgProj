# -*- coding: utf-8 -*-
"""静默安装.bat：把默认安装目录统一成 C:\\对口升学练习系统（GBK + CRLF 字节级编辑）"""
import os, sys, hashlib

P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\发布包\静默安装.bat"

raw = open(P, "rb").read()
before_hash = hashlib.sha256(raw).hexdigest()
before_size = len(raw)

# 编码/换行断言
assert raw[:3] != b"\xef\xbb\xbf", "不该有 UTF-8 BOM"
assert raw.count(b"\r\n") > 100, "应该是 CRLF"
assert raw.count(b"\n") == raw.count(b"\r\n"), "不该有裸 LF"
txt = raw.decode("gbk")   # 解码失败即说明不是 GBK

edits = [
    # 1) 注释里的默认目录说明
    (
        "rem    · 都没设：有 D 盘就装 D:\\对口升学练习系统，没有就装 C:\\对口升学练习系统\r\n",
        "rem    · 都没设：装 C:\\对口升学练习系统\r\n",
    ),
    # 2) ★ 真正的逻辑：永远默认 C 盘
    (
        'if not defined TARGET (\r\n'
        '    if exist "D:\\" (set "TARGET=D:\\对口升学练习系统") else (set "TARGET=C:\\对口升学练习系统")\r\n'
        ')\r\n',
        'if not defined TARGET set "TARGET=C:\\对口升学练习系统"\r\n',
    ),
    # 3) err2 提示里的示例目录
    (
        "echo        改法：双击前先开个命令行执行  set TARGET=D:\\你要的目录  再运行本文件；\r\n",
        "echo        改法：双击前先开个命令行执行  set TARGET=C:\\你要的目录  再运行本文件；\r\n",
    ),
    # 4) suspect_end 里的 SETUP_EXE 示例
    (
        "echo              set SETUP_EXE=D:\\某处\\对口升学练习系统_安装程序.exe\r\n",
        "echo              set SETUP_EXE=C:\\某处\\对口升学练习系统_安装程序.exe\r\n",
    ),
    # 5) no_setup 里的 SETUP_EXE 示例
    (
        "echo        2) 在命令行里执行  set SETUP_EXE=D:\\某处\\对口升学练习系统_安装程序.exe  后再运行本文件；\r\n",
        "echo        2) 在命令行里执行  set SETUP_EXE=C:\\某处\\对口升学练习系统_安装程序.exe  后再运行本文件；\r\n",
    ),
]

for old, new in edits:
    n = txt.count(old)
    if n != 1:
        print("ERROR 命中 %d 次的锚点：%r" % (n, old[:60]))
        sys.exit(1)
    txt = txt.replace(old, new)

out = txt.encode("gbk")
# 保持 CRLF、无 BOM；体积应减小（if 行变成了单行）
open(P, "wb").write(out)

after = open(P, "rb").read()
print("before size=%d  after size=%d  delta=%d" % (before_size, len(after), len(after) - before_size))
print("before sha256=%s" % before_hash)
print("after  sha256=%s" % hashlib.sha256(after).hexdigest())
print("CRLF ok=%s  bare-LF=%d  BOM=%s" % (
    after.count(b"\r\n") == after.count(b"\n"),
    after.count(b"\n") - after.count(b"\r\n"),
    after[:3] == b"\xef\xbb\xbf"))
# 复核：解码回来不再含 D 盘默认目录
dec = after.decode("gbk")
for bad in ["D:\\对口升学练习系统", "TARGET=D:", "有 D 盘就装"]:
    print("residual %-24s = %d" % (bad, dec.count(bad)))
print("TARGET=C count = %d" % dec.count("TARGET=C:\\对口升学练习系统"))
