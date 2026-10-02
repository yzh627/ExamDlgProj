# -*- coding: utf-8 -*-
"""使用说明.txt：静默安装默认目录 D: -> C:；卸 clean 说明补上 draft\exam\临时答案"""
import hashlib

P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\发布包\使用说明.txt"
raw = open(P, "rb").read()
assert raw[:3] == b"\xef\xbb\xbf", "应该是 UTF-8 BOM"
assert raw.count(b"\r\n") == 0, "应该是纯 LF"
body = raw[3:].decode("utf-8")
size0, hash0 = len(raw), hashlib.sha256(raw).hexdigest()
log = []


def rep(old, new, tag):
    global body
    n = body.count(old)
    assert n == 1, "%s 锚点命中 %d 次" % (tag, n)
    body = body.replace(old, new, 1)
    log.append("%s ok (+%d 字符)" % (tag, len(new) - len(old)))


# 1) 方式一：卸载说明
rep(
    "      —— 卸载只删本程序装进去的那 6 个文件（主程序、清单、题库、图片、说明、卸载程序）；\n"
    "         你自己放进那个文件夹的东西不会被删。它俩互不干扰：\n"
    "         如果那文件夹里还有你放的文件，目录会保留下来并提示你自己删。\n",
    "      —— 卸载只删本程序自己的东西：装进去的那 6 个文件（主程序、清单、题库、图片、\n"
    "         说明、卸载程序），再加上程序运行时产生的草稿 draft\\、VS 调试工程 exam\\\n"
    "         和临时答案 answer.txt / answer.cs；\n"
    "         你自己放进那个文件夹的东西不会被删。\n"
    "         如果那文件夹里还有你放的文件，目录会保留下来并提示你自己删。\n",
    "D1 方式一·卸载说明")

# 2) 静默安装命令行示例
rep(
    "  对口升学练习系统_安装程序.exe /S /D=D:\\对口升学练习系统\n",
    "  对口升学练习系统_安装程序.exe /S /D=C:\\对口升学练习系统\n",
    "D2 静默安装示例")

# 3) /D 说明里的空格路径示例
rep(
    '                    例如 /D="D:\\我的 目录"）。不加就装到默认位置\n',
    '                    例如 /D="C:\\我的 目录"）。不加就装到 C:\\对口升学练习系统\n',
    "D3 /D 示例与默认值")

# 4) 共享目录示例
rep(
    "      \\\\服务器\\共享\\对口升学练习系统_安装程序.exe /S /D=D:\\对口升学练习系统\n",
    "      \\\\服务器\\共享\\对口升学练习系统_安装程序.exe /S /D=C:\\对口升学练习系统\n",
    "D4 共享目录示例")

# 5) 【注意】块 —— 默认目录 + 旧位置清理范围
rep(
    "    所以机房部署前先定好一个目录，之后每台机器都用它（建议 D:\\对口升学练习系统）。\n"
    "    （草稿目录 draft 不在删除范围内，会原地保留下来。）\n",
    "    所以机房部署前先定好一个目录，之后每台机器都用它（默认 C:\\对口升学练习系统）。\n"
    "    （旧位置里程序自己产生的草稿 draft\\、调试工程 exam\\、临时答案也会一并清掉，\n"
    "    　不会在旧目录里留残渣。）\n",
    "D5 注意·默认目录与清理范围")

# 6) 静默安装.bat 段落里也提一句默认目录
rep(
    "    想固定装到某个目录：用记事本打开它，把 FIXED_DIR= 后面写成你要的路径，保存即可。\n",
    "    不带参数双击就是装到默认目录 C:\\对口升学练习系统；想固定装到别处：\n"
    "    用记事本打开它，把 FIXED_DIR= 后面写成你要的路径，保存即可。\n",
    "D6 bat 段落·默认目录")

out = b"\xef\xbb\xbf" + body.encode("utf-8")
open(P, "wb").write(out)
chk = open(P, "rb").read()
assert chk[:3] == b"\xef\xbb\xbf" and chk.count(b"\r\n") == 0

log.append("")
log.append("size %d -> %d (delta %+d)" % (size0, len(chk), len(chk) - size0))
log.append("sha256 %s -> %s" % (hash0[:16], hashlib.sha256(chk).hexdigest()[:16]))
dec = chk[3:].decode("utf-8")
log.append("残留 'D:\\对口升学练习系统' = %d" % dec.count("D:\\对口升学练习系统"))
log.append("残留 '/D=\"D:' = %d" % dec.count('/D="D:'))
log.append("C:\\对口升学练习系统 出现 %d 次" % dec.count("C:\\对口升学练习系统"))
log.append("'默认 C:\\对口升学练习系统' = %d" % dec.count("默认 C:\\对口升学练习系统"))

open(r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\tools\_doc_patch_report.txt",
     "w", encoding="utf-8").write("\n".join(log))
print("done")
