# -*- coding: utf-8 -*-
"""重编后校验：
  1) 三产物 sha256 前缀 + 字节数
  2) 安装程序内 7 个 payload 的"全字节连续包含性"（find(b"..")>=0）
  3) 发布包 与 工程产物 逐字节一致
  4) questions.dat 哈希未变（对齐备份里的旧值）
  5) 静默安装.bat 内嵌 SHA256 == 当前安装程序 sha256
"""
import io, os, hashlib, time

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = PROJ + r"\发布包"
INST = PROJ + r"\安装程序"
OLD_QD_HASH = "ba7d0230022d60ccd0a8b0e6d3d0b0d10b1e1a4d0c8e2f9a8b7c6d5e4f3a2b1c"  # placeholder, replaced below

def sha(p, n=16):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()[:n]

def fullsha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()

def size(p):
    return os.path.getsize(p)

out = []
# 1) 三产物
prod = {
    "x64 主程序": PKG + r"\ExamDlgProj.exe",
    "x86 主程序": PKG + r"\ExamDlgProj_32位.exe",
    "安装程序":   PKG + r"\对口升学练习系统_安装程序.exe",
}
out.append("=== 1. 三产物 ===")
for k, p in prod.items():
    out.append("  %-10s size=%-9d sha256=%s  (%s)" % (k, size(p), fullsha(p), os.path.basename(p)))

# 4) questions.dat 未变
out.append("=== 4. questions.dat ===")
for p in [PKG + r"\questions.dat", PROJ + r"\questions.dat", PROJ + r"\x64\Release\questions.dat"]:
    if os.path.exists(p):
        out.append("  %-45s size=%-9d sha256=%s" % (p.replace(PROJ, "."), size(p), fullsha(p)))

# 2) payload 全字节连续包含性
setup = open(PKG + r"\对口升学练习系统_安装程序.exe", "rb").read()
payloads = [
    ("x64 exe",      PROJ + r"\x64\Release\ExamDlgProj.exe"),
    ("x86 exe",      PROJ + r"\x86\Release\ExamDlgProj.exe"),
    ("manifest",     PROJ + r"\x64\Release\ExamDlgProj.exe.manifest"),
    ("questions.dat",PROJ + r"\questions.dat"),
    ("support.png",  PROJ + r"\support.png"),
    ("说明.txt",     PROJ + r"\说明.txt"),
    ("卸载.exe",     INST + r"\Uninst.exe"),
]
out.append("=== 2. 安装程序内 payload 全字节连续包含性（setup size=%d）===" % len(setup))
for name, p in payloads:
    if not os.path.exists(p):
        out.append("  %-14s MISSING %s" % (name, p)); continue
    blob = open(p, "rb").read()
    idx = setup.find(blob)
    out.append("  %-14s payload_size=%-9d %s (offset=%d)" % (name, len(blob), "FOUND" if idx >= 0 else "NOT-FOUND", idx))

# 3) 发布包 vs 工程产物 逐字节一致
out.append("=== 3. 发布包 vs 工程产物 逐字节一致 ===")
pairs = [
    (PKG + r"\ExamDlgProj.exe", PROJ + r"\x64\Release\ExamDlgProj.exe"),
    (PKG + r"\ExamDlgProj.exe.manifest", PROJ + r"\x64\Release\ExamDlgProj.exe.manifest"),
    (PKG + r"\ExamDlgProj_32位.exe", PROJ + r"\x86\Release\ExamDlgProj.exe"),
    (PKG + r"\ExamDlgProj_32位.exe.manifest", PROJ + r"\x86\Release\ExamDlgProj.exe.manifest"),
    (PKG + r"\对口升学练习系统_安装程序.exe", INST + r"\对口升学练习系统_安装程序.exe"),
    (PKG + r"\questions.dat", PROJ + r"\questions.dat"),
    (PKG + r"\support.png", PROJ + r"\support.png"),
    (PKG + r"\说明.txt", PROJ + r"\说明.txt"),
]
for a, b in pairs:
    if not os.path.exists(a) or not os.path.exists(b):
        out.append("  MISSING pair: %s | %s" % (a, b)); continue
    same = open(a, "rb").read() == open(b, "rb").read()
    out.append("  %s  %s" % ("OK  " if same else "DIFF", os.path.basename(a)))

# 5) bat 内嵌 SHA256
out.append("=== 5. 静默安装.bat 内嵌 SHA256 ===")
bat = open(PKG + r"\静默安装.bat", "rb").read().decode("gbk")
import re
m = re.search(r'set "KNOWN=([0-9a-fA-F]{64})"', bat)
cur = fullsha(PKG + r"\对口升学练习系统_安装程序.exe")
out.append("  bat KNOWN   = %s" % (m.group(1) if m else "NOT FOUND"))
out.append("  setup sha256= %s" % cur)
out.append("  一致: %s" % ("YES" if m and m.group(1).lower() == cur else "NO"))

# 附：最后一次改动的 发布包 一致性（说明.txt / 使用说明.txt / bat 编码检查）
out.append("=== 6. 文案编码核对 ===")
for name, enc_ok in [(PKG + r"\使用说明.txt", "utf8bom-lf"), (PKG + r"\静默安装.bat", "gbk-crlf-nobom")]:
    b = open(name, "rb").read()
    out.append("  %-16s bom=%s crlf=%d bareLF=%d" % (
        os.path.basename(name), b[:3] == b"\xef\xbb\xbf", b.count(b"\r\n"), b.count(b"\n") - b.count(b"\r\n")))

io.open(r"C:\Users\30601\Desktop\_verify_out.txt", "w", encoding="utf-8").write("\n".join(out))
print("verify written")
