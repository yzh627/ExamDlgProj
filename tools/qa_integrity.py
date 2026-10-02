# -*- coding: utf-8 -*-
"""V9 产物完整性：
  A) 发布包 各文件 vs 工程产物 逐字节一致
  B) 安装程序内"全字节连续包含" 7 个 payload
  C) 静默安装.bat 内嵌 SHA256 == 当前安装程序
"""
import io, os, hashlib, re

PROJ = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
PKG = PROJ + r"\发布包"
SETUP = PROJ + r"\安装程序"
OUT = r"C:\Users\30601\Desktop\_qa_integrity_out.txt"
out = []
def log(s): out.append(str(s)); io.open(OUT, "w", encoding="utf-8").write("\n".join(out))

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

INSTALLER = PKG + r"\对口升学练习系统_安装程序.exe"
inst_hash = sha256(INSTALLER)
log("安装程序 SHA256 = %s" % inst_hash)
log("")

# ---- A) 发布包 vs 工程产物 ----
pairs = [
    ("x64 主程序", PKG + r"\ExamDlgProj.exe", PROJ + r"\x64\Release\ExamDlgProj.exe"),
    ("x86 主程序", PKG + r"\ExamDlgProj_32位.exe", PROJ + r"\x86\Release\ExamDlgProj.exe"),
    ("manifest", PKG + r"\ExamDlgProj.exe.manifest", PROJ + r"\x64\Release\ExamDlgProj.exe.manifest"),
    ("questions.dat", PKG + r"\questions.dat", PROJ + r"\questions.dat"),
    ("support.png", PKG + r"\support.png", PROJ + r"\support.png"),
    ("说明.txt", PKG + r"\说明.txt", PROJ + r"\说明.txt"),
]
log("== A) 发布包 vs 工程产物 ==")
for name, a, b in pairs:
    ea, eb = os.path.exists(a), os.path.exists(b)
    if not (ea and eb):
        log("  [FAIL] %s  存在性 pkg=%s proj=%s" % (name, ea, eb)); continue
    ha, hb = sha256(a), sha256(b)
    log("  [%s] %-14s %s  pkg=%d proj=%d" % ("PASS" if ha == hb else "FAIL", name, ha[:16], os.path.getsize(a), os.path.getsize(b)))
    if ha != hb:
        log("        pkg=%s\n        proj=%s" % (ha, hb))

# ---- B) 安装程序内嵌 payload 全字节连续包含 ----
log("")
log("== B) 安装程序内连续包含 payload ==")
ip = open(INSTALLER, "rb").read()
payloads = [
    ("x64 exe", PROJ + r"\x64\Release\ExamDlgProj.exe"),
    ("x86 exe", PROJ + r"\x86\Release\ExamDlgProj.exe"),
    ("manifest", PROJ + r"\x64\Release\ExamDlgProj.exe.manifest"),
    ("questions.dat", PROJ + r"\questions.dat"),
    ("support.png", PROJ + r"\support.png"),
    ("说明.txt", PROJ + r"\说明.txt"),
    ("卸载.exe(Uninst)", SETUP + r"\Uninst.exe"),
]
for name, p in payloads:
    if not os.path.exists(p):
        log("  [FAIL] %-18s 源文件不存在 %s" % (name, p)); continue
    data = open(p, "rb").read()
    idx = ip.find(data)
    log("  [%s] %-18s size=%-8d 连续包含于偏移=%s" % ("PASS" if idx >= 0 else "FAIL", name, len(data), idx))

# ---- C) bat 内嵌 SHA256 == 安装程序 ----
log("")
log("== C) 静默安装.bat 内嵌 SHA256 ==")
bat = PKG + r"\静默安装.bat"
raw = open(bat, "rb").read()
try:
    text = raw.decode("gbk", "replace")
except Exception:
    text = raw.decode("latin-1")
hashes = re.findall(r"[0-9a-fA-F]{64}", text)
log("  bat 中 64 位十六进制串：%r" % hashes)
log("  [%s] bat 含安装程序完整 SHA256" % ("PASS" if inst_hash.lower() in [h.lower() for h in hashes] else "FAIL"))
log("  安装程序 SHA256=%s" % inst_hash)
# bat 编码检查
log("  bat 编码检查：BOM=%s  含CRLF=%s" % (raw[:3] == b"\xef\xbb\xbf", b"\r\n" in raw))
io.open(OUT, "w", encoding="utf-8").write("\n".join(out))
