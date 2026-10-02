# -*- coding: utf-8 -*-
"""三修验证 A+B：
   A 组 产物哈希 / 安装程序完整内嵌 payload / 发布包与工程产物一致
   B 组 静默安装.bat 编码完好 + 默认目录已是 C: + 内嵌哈希等于当前安装程序
"""
import hashlib, os

BASE = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = BASE + r"\发布包"
out = []
ok = fail = 0


def chk(name, cond, extra=""):
    global ok, fail
    if cond:
        ok += 1
        out.append("[PASS] " + name + (("  " + extra) if extra else ""))
    else:
        fail += 1
        out.append("[FAIL] " + name + (("  " + extra) if extra else ""))


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def rb(p):
    return open(p, "rb").read()


# ---------------------------------------------------------------- A 组
out.append("===== A 组：产物与内嵌 =====")

inst = PKG + r"\对口升学练习系统_安装程序.exe"
inst_b = rb(inst)
out.append("安装程序 sha256 = %s  (%d 字节)" % (hashlib.sha256(inst_b).hexdigest(), len(inst_b)))

payloads = [
    ("x64 主程序", PKG + r"\ExamDlgProj.exe"),
    ("x86 主程序", PKG + r"\ExamDlgProj_32位.exe"),
    ("x64 清单", PKG + r"\ExamDlgProj.exe.manifest"),
    ("题库 questions.dat", PKG + r"\questions.dat"),
    ("support.png", PKG + r"\support.png"),
    ("说明.txt", PKG + r"\说明.txt"),
    ("卸载.exe(Uninst)", BASE + r"\安装程序\Uninst.exe"),
]
for label, p in payloads:
    b = rb(p)
    chk("安装程序完整内嵌 " + label, b in inst_b,
        "%d 字节 %s" % (len(b), hashlib.sha256(b).hexdigest()[:12]))

# 发布包 vs 工程产物逐字节一致
pairs = [
    (r"x64\Release\ExamDlgProj.exe", r"发布包\ExamDlgProj.exe"),
    (r"x64\Release\ExamDlgProj.exe.manifest", r"发布包\ExamDlgProj.exe.manifest"),
    (r"x86\Release\ExamDlgProj.exe", r"发布包\ExamDlgProj_32位.exe"),
    (r"x86\Release\ExamDlgProj.exe.manifest", r"发布包\ExamDlgProj_32位.exe.manifest"),
    (r"安装程序\对口升学练习系统_安装程序.exe", r"发布包\对口升学练习系统_安装程序.exe"),
    (r"questions.dat", r"发布包\questions.dat"),
]
for a, b in pairs:
    pa, pb = os.path.join(BASE, a), os.path.join(BASE, b)
    if not os.path.exists(pa) or not os.path.exists(pb):
        chk("发布包一致 " + b, False, "缺文件")
        continue
    chk("发布包 == 工程产物  " + b, rb(pa) == rb(pb))

# 反例：9/17 的旧安装程序不该含新 exe（防止"内嵌检查"变成永真）
old = os.path.join(BASE, r"安装程序_update_backup")
chk("旧安装程序快照不含新 exe（反例）",
    not os.path.exists(old) or not any(
        rb(os.path.join(old, d, "ExamDlgProj.exe")) in inst_b
        for d in os.listdir(old)
        if os.path.exists(os.path.join(old, d, "ExamDlgProj.exe"))
    ) if os.path.exists(old) else True)

# ---------------------------------------------------------------- B 组
out.append("")
out.append("===== B 组：静默安装.bat =====")
bat = PKG + r"\静默安装.bat"
bb = rb(bat)
chk("无 UTF-8 BOM", bb[:3] != b"\xef\xbb\xbf")
chk("CRLF 换行（无裸 LF）", bb.count(b"\n") == bb.count(b"\r\n"),
    "CRLF=%d" % bb.count(b"\r\n"))
try:
    dec = bb.decode("gbk")
    chk("GBK 可解码", True)
except UnicodeDecodeError as e:
    dec = ""
    chk("GBK 可解码", False, str(e))

chk("默认目录已是 C:\\对口升学练习系统", dec.count('set "TARGET=C:\\对口升学练习系统"') == 1)
chk("不再有 D 盘默认目录", "D:\\对口升学练习系统" not in dec)

# 内嵌哈希 == 当前安装程序 SHA256（检查三处：注释/KNOWN/SETUP_HASH）
cur = hashlib.sha256(inst_b).hexdigest()
chk("内嵌注释哈希 == 安装程序", ("rem    " + cur) in dec, cur[:16] + "…")
chk('KNOWN == 安装程序', ('set "KNOWN=%s"' % cur) in dec)
chk('SETUP_HASH == 安装程序', ('set "SETUP_HASH=%s"' % cur) in dec)

out.append("")
out.append("通过 %d 项，失败 %d 项" % (ok, fail))
open(BASE + r"\tools\_verify_ab.txt", "w", encoding="utf-8").write("\n".join(out))
print("ok=%d fail=%d" % (ok, fail))
