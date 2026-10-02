# -*- coding: utf-8 -*-
import io, re, os
OUT = r"C:\Users\30601\Desktop\_qa_origdate.txt"
found = []
for fn in [r"C:\Users\30601\.workbuddy\audit-log\2026-09-18.jsonl",
           r"C:\Users\30601\.workbuddy\audit-log\2026-09-17.jsonl"]:
    if not os.path.exists(fn):
        continue
    t = io.open(fn, "r", encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"InstallDate", t):
        s = max(0, m.start() - 200); e = min(len(t), m.end() + 200)
        found.append(t[s:e])
io.open(OUT, "w", encoding="utf-8").write("\n\n----\n\n".join(found[:20]) if found else "no InstallDate in audit logs")
print("done", len(found))
