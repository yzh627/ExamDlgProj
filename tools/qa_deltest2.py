# -*- coding: utf-8 -*-
"""只测原生 ctypes 删除是否被 safe-delete 钩子拦截（不用 os/shutil 删）。"""
import os, sys, ctypes
k = ctypes.windll.kernel32
r = []
t = r"C:\_qtmp_deltest_ct2"
os.makedirs(t, exist_ok=True)
for i in range(60):
    open(os.path.join(t, "f%d.txt" % i), "w").write("x")
r.append("created=%d" % len(os.listdir(t)))
try:
    for i in range(60):
        ok = k.DeleteFileW(os.path.join(t, "f%d.txt" % i))
    ok2 = k.RemoveDirectoryW(t)
    r.append("ctypes-delete-ok exists=%s rm=%s" % (os.path.exists(t), ok2))
except BaseException as e:
    r.append("ctypes-ERR %r" % e)
# 单个 os.remove 是否 OK
t2 = r"C:\_qtmp_deltest_one"
os.makedirs(t2, exist_ok=True)
open(os.path.join(t2, "a.txt"), "w").write("x")
try:
    os.remove(os.path.join(t2, "a.txt"))
    os.rmdir(t2)
    r.append("os-single-ok exists=%s" % os.path.exists(t2))
except BaseException as e:
    r.append("os-single-ERR %r" % e)
open(r"C:\Users\30601\Desktop\_qa_deltest_py2.txt", "w").write("\n".join(r))
