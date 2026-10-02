# -*- coding: utf-8 -*-
"""对外文案补丁（第三修）：
  A. 发布包\\使用说明.txt  (UTF-8 BOM + LF) 补"盘符回退 + 同名加序号"说明
  B. 发布包\\静默安装.bat  (GBK + CRLF, 无 BOM) 让"安装位置"显示改为读安装程序写的
     last_dir.txt（ANSI），从而反映回退/加序号后的真实目录
"""
import hashlib

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
DOC = PROJ + r"\发布包\使用说明.txt"
BAT = PROJ + r"\发布包\静默安装.bat"
log = []

# ============================================================ A. 使用说明.txt
raw = open(DOC, "rb").read()
assert raw[:3] == b"\xef\xbb\xbf" and raw.count(b"\r\n") == 0
dt = raw[3:].decode("utf-8")
b_size = len(raw)

o1 = ("    · 装到盘根（C:\\）或 Windows 目录里会被直接拒绝，会提示你换一个文件夹；\n"
      "      装到非空文件夹会先问一句（默认建议换空文件夹），确认后才装。")
n1 = ("    · 装到盘根（C:\\）或 Windows 目录里会被直接拒绝，会提示你换一个文件夹。\n"
      "    · 目标盘不可用（比如这台机器根本没有 D 盘、或盘读不出来）：会自动改用第一个能用的固定盘，\n"
      "      目标变成 <该盘>:\\对口升学练习系统；光驱、网络盘、U 盘都不会被选中，最终装到哪会明确告诉你。\n"
      "    · 目标文件夹已经被占用（又不是本程序上次装的地方）：会自动改用带序号的文件夹\n"
      "      （…1、…2 …… 最多 …99），并把最终目录回显给你，免得你以为装到了自己填的那个路径；\n"
      "      只有 1~99 都被占时，才会保留原目录、照旧问一句（默认建议换空文件夹）。")
assert dt.count(o1) == 1, "doc D1 锚点不唯一"
dt = dt.replace(o1, n1, 1)

o2 = ("  日志：安装程序旁边会生成 install.log（旁边写不进去就放 %TEMP%），\n"
      "        每一步都记在里面，装失败先看它。")
n2 = (o2 + "\n"
      "  自动调整：目标盘不可用会自动改用其它可用固定盘；同名文件夹被占用会自动加序号（如 …1）。\n"
      "            最终目录记在 install.log 的「FINAL_DIR=」那一行；双击「静默安装.bat」时它会直接显示出来。")
assert dt.count(o2) == 1, "doc D2 锚点不唯一"
dt = dt.replace(o2, n2, 1)

open(DOC, "wb").write(b"\xef\xbb\xbf" + dt.encode("utf-8"))
a = open(DOC, "rb").read()
assert a[:3] == b"\xef\xbb\xbf" and a.count(b"\r\n") == 0
log.append("DOC (使用说明.txt)  size %d -> %d  delta %+d" % (b_size, len(a), len(a) - b_size))

# ============================================================ B. 静默安装.bat
raw = open(BAT, "rb").read()
assert raw[:3] != b"\xef\xbb\xbf", "bat 不该有 BOM"
assert raw.count(b"\n") == raw.count(b"\r\n"), "bat 应为纯 CRLF"
bt = raw.decode("gbk")
b_size = len(raw)

def CR(s):
    return s.replace("\n", "\r\n")

# B1 注释里补"装到哪"的新规则
o = CR("rem    · 都没设：装 C:\\对口升学练习系统\n")
n = o + CR("rem    · 目标盘不可用会自动改用其它固定盘；同名文件夹被占用会自动加序号（…1），实际目录看日志\n")
assert bt.count(o) == 1
bt = bt.replace(o, n, 1)

# B2 装之前那行：说明这是"计划"目录，可能会被自动调整
o = CR("echo   安装位置：%TARGET%\n")
n = CR("echo   计划安装到：%TARGET%（如该盘不可用或同名文件夹被占用，装的时候会自动调整）\n")
assert bt.count(o) == 1, "bat B2 锚点不唯一"
bt = bt.replace(o, n, 1)

# B3 :ok 区块改为读 last_dir.txt 显示真实目录
o = CR(
    ":ok\n"
    "echo [成功] 已安装到：%TARGET%\n"
    'set "N=0"\n'
    "for %%F in (ExamDlgProj.exe ExamDlgProj.exe.manifest questions.dat support.png \u8bf4\u660e.txt \u5378\u8f7d.exe) do if exist \"%TARGET%\\%%F\" set /a N+=1\n"
    "echo        \u6838\u5fc3\u6587\u4ef6\u6838\u5bf9\uff1a!N! / 6 \u4e2a\u90fd\u5728\n"
    "if exist \"%TARGET%\\score.txt\" echo        \uff08score.txt \u662f\u5b66\u751f\u505a\u9898\u7559\u4e0b\u7684\u6210\u7ee9\u8bb0\u5f55\uff0c\u6709\u8fd9\u4e2a\u5c5e\u6b63\u5e38\uff09\n"
    "goto :tail\n"
)
n = CR(
    ":ok\n"
    "rem \u5b9e\u9645\u88c5\u5230\u54ea\uff0c\u4ee5\u5b89\u88c5\u7a0b\u5e8f\u5199\u7684 last_dir.txt \u4e3a\u51c6\uff08\u5b83\u6309 ANSI \u5199\uff0c\u548c\u672c bat \u540c\u7f16\u7801\uff09\u3002\n"
    "rem \u5b89\u88c5\u7a0b\u5e8f\u53ef\u80fd\u505a\u4e86\"\u76d8\u7b26\u56de\u9000\"\u6216\"\u540c\u540d\u52a0\u5e8f\u53f7\"\uff0c\u4e0d\u80fd\u518d\u60f3\u5f53\u7136\u5730\u7528 %TARGET%\u3002\n"
    "rem \u8be5\u6587\u4ef6\u5728\u5b89\u88c5\u7a0b\u5e8f\u65c1\u8fb9\uff1b\u5199\u4e0d\u8fdb\u53bb\u65f6\u5728 %TEMP%\\\u5bf9\u53e3\u5347\u5b66\u7ec3\u4e60\u7cfb\u7edf_last_dir.txt\u3002\n"
    'set "ACTUAL="\n'
    'for %%P in ("%SETUP%") do set "SETUPDIR=%%~dpP"\n'
    'set "LDIR=%SETUPDIR%last_dir.txt"\n'
    'if not exist "%LDIR%" set "LDIR=%TEMP%\\\u5bf9\u53e3\u5347\u5b66\u7ec3\u4e60\u7cfb\u7edf_last_dir.txt"\n'
    'if exist "%LDIR%" set /p "ACTUAL="<"%LDIR%"\n'
    'if not defined ACTUAL set "ACTUAL=%TARGET%"\n'
    "echo [\u6210\u529f] \u5b89\u88c5\u5b8c\u6210\uff0c\u4f4d\u7f6e\uff1a\n"
    "echo        !ACTUAL!\n"
    'set "N=0"\n'
    "for %%F in (ExamDlgProj.exe ExamDlgProj.exe.manifest questions.dat support.png \u8bf4\u660e.txt \u5378\u8f7d.exe) do if exist \"!ACTUAL!\\%%F\" set /a N+=1\n"
    "echo        \u6838\u5fc3\u6587\u4ef6\u6838\u5bf9\uff1a!N! / 6 \u4e2a\u90fd\u5728\n"
    "if /i not \"!ACTUAL!\"==\"%TARGET%\" echo        \uff08\u6ce8\u610f\uff1a\u4e0d\u662f\u539f\u5b9a\u7684 %TARGET%\uff0c\u5b89\u88c5\u7a0b\u5e8f\u5df2\u81ea\u52a8\u8c03\u6574\uff09\n"
    "if exist \"!ACTUAL!\\score.txt\" echo        \uff08score.txt \u662f\u5b66\u751f\u505a\u9898\u7559\u4e0b\u7684\u6210\u7ee9\u8bb0\u5f55\uff0c\u6709\u8fd9\u4e2a\u5c5e\u6b63\u5e38\uff09\n"
    "goto :tail\n"
)
assert bt.count(o) == 1, "bat :ok 区块锚点不唯一"
bt = bt.replace(o, n, 1)

out = bt.encode("gbk")
open(BAT, "wb").write(out)
a = open(BAT, "rb").read()
assert a[:3] != b"\xef\xbb\xbf"
assert a.count(b"\n") == a.count(b"\r\n"), "bat 写回后出现裸 LF"
log.append("BAT (静默安装.bat) size %d -> %d  delta %+d" % (b_size, len(a), len(a) - b_size))

log.append("")
log.append("DOC sha256=%s" % hashlib.sha256(open(DOC, "rb").read()).hexdigest())
log.append("BAT sha256=%s" % hashlib.sha256(a).hexdigest())
open(PROJ + r"\tools\_dir_policy_text_report.txt", "w", encoding="utf-8").write("\n".join(log))
print("OK text patch done")
