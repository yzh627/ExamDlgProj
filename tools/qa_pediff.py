# -*- coding: utf-8 -*-
"""独立核验"主程序 exe 哈希变化 = PE 时间戳效应，而非源码变化"。
对比两个同尺寸(2804736)的 x64 构建，看差异字节是否只落在 PE 头时间戳/校验和等元数据区。"""
import io, os, struct, hashlib

A = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\C盘安装备份\20260918_1516\ExamDlgProj.exe"
B = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\发布包\ExamDlgProj.exe"
OUT = r"C:\Users\30601\Desktop\_qa_pediff.txt"
adj = {"x86": r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\发布包\ExamDlgProj_32位.exe",
       "安装程序": r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\发布包\对口升学练习系统_安装程序.exe",
       "x64": B}

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def pe_stamp(p):
    with open(p, "rb") as f:
        d = f.read(0x400)
    e_lfanew = struct.unpack_from("<I", d, 0x3C)[0]
    ts = struct.unpack_from("<I", d, e_lfanew + 8)[0]
    import datetime
    try:
        s = datetime.datetime.utcfromtimestamp(ts).strftime("%Y-%m-%d %H:%M:%S UTC")
    except Exception:
        s = "?"
    return e_lfanew, ts, s

out = []
out.append("A=%s\n  sha=%s" % (A, sha(A)))
out.append("B=%s\n  sha=%s" % (B, sha(B)))
out.append("")
for nm, p in adj.items():
    e, ts, s = pe_stamp(p)
    out.append("PE TimeDateStamp %-8s e_lfanew=0x%X ts=%d (%s)  %s" % (nm, e, ts, s, os.path.basename(p)))

da = open(A, "rb").read()
db = open(B, "rb").read()
out.append("")
out.append("尺寸 A=%d B=%d" % (len(da), len(db)))
if len(da) == len(db):
    diff = [i for i in range(len(da)) if da[i] != db[i]]
    out.append("差异字节数 = %d" % len(diff))
    # 归组连续区间
    groups = []
    for i in diff:
        if groups and i == groups[-1][1] + 1:
            groups[-1][1] = i
        else:
            groups.append([i, i])
    out.append("差异区间数 = %d（各区间起止偏移）:" % len(groups))
    for g in groups[:40]:
        ctx_a = da[g[0]:g[1] + 1].hex()
        ctx_b = db[g[0]:g[1] + 1].hex()
        out.append("  [0x%X..0x%X] len=%d  A=%s  B=%s" % (g[0], g[1], g[1] - g[0] + 1, ctx_a, ctx_b))
    # 判断是否只在 PE 头(前 0x400)与调试目录/校验和
    outside_header = [g for g in groups if g[0] >= 0x400]
    out.append("")
    out.append("落在 PE 头(前0x400)之外的差异区间数 = %d" % len(outside_header))
    for g in outside_header[:20]:
        out.append("  outside [0x%X..0x%X]" % (g[0], g[1]))
io.open(OUT, "w", encoding="utf-8").write("\n".join(out))
print("done")
