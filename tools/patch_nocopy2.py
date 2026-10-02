# -*- coding: utf-8 -*-
"""左侧只读框架：把"按 Ctrl+某键才拦"改成"键盘一律不吃"。
   理由：GetKeyState 那套在合成输入 / 非前台场景下并不可靠，护栏容易形同虚设；
   而左侧本来就只是一段只读的参考代码，根本不需要键盘交互。
   改完之后 Ctrl+C/X/V/A、Shift+方向键选字……全都无从下手，也不需要运行时验证
   "Ctrl 到底按没按"——可验证性反而更强（见 verify_nocopy.py 的 E 组）。
"""
import hashlib

P = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\PracticeDlg.cpp"
raw = open(P, "rb").read()
assert raw[:3] == b"\xef\xbb\xbf" and raw.count(b"\r\n") == 0
body = raw[3:].decode("utf-8")
size0, hash0 = len(raw), hashlib.sha256(raw).hexdigest()

OLD_COMMENT = (
    "//   · WM_COPY / WM_CUT / WM_CLEAR / WM_PASTE：菜单项和快捷键最终都走这几条消息，直接吞掉；\n"
    "//   · WM_CONTEXTMENU：右键菜单里就有\"复制\"，一并吞掉；\n"
    "//   · Ctrl+C / Ctrl+X / Ctrl+V / Ctrl+A / Ctrl+Insert、Shift+Insert、Shift+Delete：\n"
    "//     按键层面再挡一道（顺手把 Ctrl+A 也挡了，全选造成的大片蓝色高亮跟着消失）；\n"
    "//   · 按住左键拖动时吞掉 WM_MOUSEMOVE：防止把选中的文字拖到别的程序里去。\n"
)
NEW_COMMENT = (
    "//   · WM_COPY / WM_CUT / WM_CLEAR / WM_PASTE：菜单项和快捷键最终都走这几条消息，直接吞掉；\n"
    "//   · WM_CONTEXTMENU：右键菜单里就有\"复制\"，一并吞掉；\n"
    "//   · 键盘消息一律不回给控件（只放行 Esc / 回车 / Tab，免得焦点万一落在框架上关不掉窗口）：\n"
    "//     没人需要跟一段只读的参考代码用键盘交互，所以干脆整条键盘通道关掉 ——\n"
    "//     Ctrl+C/X/V/A、Shift+方向键选字、Insert……全都无从下手；\n"
    "//     也不必去猜 Ctrl/Shift 到底按没按（GetKeyState 在合成输入下并不可靠，\n"
    "//     靠它做判断的护栏很容易形同虚设）；\n"
    "//   · 按住左键拖动时吞掉 WM_MOUSEMOVE：防止把选中的文字拖到别的程序里去。\n"
)
assert body.count(OLD_COMMENT) == 1, "回调注释锚点不唯一"
body = body.replace(OLD_COMMENT, NEW_COMMENT, 1)

OLD_KEYS = (
    "\tcase WM_KEYDOWN:\n"
    "\tcase WM_SYSKEYDOWN:\n"
    "\t\t{\n"
    "\t\t\tBOOL bCtrl = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);\n"
    "\t\t\tBOOL bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);\n"
    "\t\t\tif (bCtrl && (wParam == 'A' || wParam == 'C' || wParam == 'X' ||\n"
    "\t\t\t\twParam == 'V' || wParam == VK_INSERT))\n"
    "\t\t\t\treturn 0;\n"
    "\t\t\tif (bShift && (wParam == VK_INSERT || wParam == VK_DELETE))\n"
    "\t\t\t\treturn 0;\n"
    "\t\t}\n"
    "\t\tbreak;\n"
)
NEW_KEYS = (
    "\tcase WM_KEYDOWN:\n"
    "\tcase WM_SYSKEYDOWN:\n"
    "\t\t// 放行 Esc / 回车 / Tab，保住对话框的常规操作\n"
    "\t\tif (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_TAB)\n"
    "\t\t\tbreak;\n"
    "\t\treturn 0;\n"
    "\n"
    "\tcase WM_KEYUP:\n"
    "\tcase WM_SYSKEYUP:\n"
    "\tcase WM_CHAR:\n"
    "\tcase WM_SYSCHAR:\n"
    "\t\treturn 0;\n"
)
assert body.count(OLD_KEYS) == 1, "按键分支锚点不唯一"
body = body.replace(OLD_KEYS, NEW_KEYS, 1)

out = b"\xef\xbb\xbf" + body.encode("utf-8")
open(P, "wb").write(out)
chk = open(P, "rb").read()
assert chk[:3] == b"\xef\xbb\xbf" and chk.count(b"\r\n") == 0

dec = chk[3:].decode("utf-8")
log = [
    "size %d -> %d (delta %+d)" % (size0, len(chk), len(chk) - size0),
    "sha256 %s -> %s" % (hash0[:12], hashlib.sha256(chk).hexdigest()[:12]),
    "残留 GetKeyState(VK_CONTROL) = %d （应为 0）" % dec.count("GetKeyState(VK_CONTROL)"),
    "WM_CHAR 分支 = %d" % dec.count("case WM_CHAR:"),
    "安全出口 VK_ESCAPE = %d" % dec.count("wParam == VK_ESCAPE"),
]
open(r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10\tools\_nocopy2_report.txt",
     "w", encoding="utf-8").write("\n".join(log))
print("done")
