# -*- coding: utf-8 -*-
import hashlib
P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\安装程序\Setup.cpp"
raw = open(P, "rb").read()
assert raw[:3] == b"\xef\xbb\xbf" and raw.count(b"\r\n") == 0
body = raw[3:].decode("utf-8")
before = body.count("\n\n\n")
while "\n\n\n" in body:
    body = body.replace("\n\n\n", "\n\n")
out = b"\xef\xbb\xbf" + body.encode("utf-8")
open(P, "wb").write(out)
a = open(P, "rb").read()
assert a[:3] == b"\xef\xbb\xbf" and a.count(b"\r\n") == 0
print("collapsed triple-newlines=%d size=%d sha=%s" % (before, len(a), hashlib.sha256(a).hexdigest()[:16]))
