# -*- coding: utf-8 -*-
import os, shutil, sys
t = r"C:\_qtmp_deltest_py"
shutil.rmtree(t, ignore_errors=True)
os.makedirs(t, exist_ok=True)
for i in range(60):
    open(os.path.join(t, "f%d.txt" % i), "w").write("x")
r = ["created=%d" % len(os.listdir(t))]
try:
    shutil.rmtree(t)
    r.append("shutil-ok exists=%s" % os.path.exists(t))
except Exception as e:
    r.append("shutil-ERR %r" % e)
# 再测 ctypes 原生删除
t2 = r"C:\_qtmp_deltest_ct"
os.makedirs(t2, exist_ok=True)
for i in range(60):
    open(os.path.join(t2, "f%d.txt" % i), "w").write("x")
try:
    import ctypes
    k = ctypes.windll.kernel32
    for i in range(60):
        k.DeleteFileW(os.path.join(t2, "f%d.txt" % i))
    k.RemoveDirectoryW(t2)
    r.append("ctypes-ok exists=%s" % os.path.exists(t2))
except Exception as e:
    r.append("ctypes-ERR %r" % e)
open(r"C:\Users\30601\Desktop\_qa_deltest_py.txt", "w").write("\n".join(r))
