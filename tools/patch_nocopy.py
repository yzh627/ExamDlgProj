# -*- coding: utf-8 -*-
"""左侧只读代码框架：禁止复制（子类化拦截剪贴板相关消息）
   改 PracticeDlg.h（成员 + 回调声明）与 PracticeDlg.cpp（安装子类化 + 回调实现）
"""
import hashlib

BASE = r"C:\Users\30601\Desktop\ExamDlgProj_开发版VS2026_v6.10"
H = BASE + r"\PracticeDlg.h"
C = BASE + r"\PracticeDlg.cpp"
log = []


def load(p):
    raw = open(p, "rb").read()
    assert raw[:3] == b"\xef\xbb\xbf", p + " 应该是 UTF-8 BOM"
    assert raw.count(b"\r\n") == 0, p + " 应该是纯 LF"
    return raw, raw[3:].decode("utf-8")


def save(p, text):
    out = b"\xef\xbb\xbf" + text.encode("utf-8")
    open(p, "wb").write(out)
    chk = open(p, "rb").read()
    assert chk[:3] == b"\xef\xbb\xbf" and chk.count(b"\r\n") == 0
    return chk


# ============================================================ PracticeDlg.h
raw, h = load(H)
h_size0, h_hash0 = len(raw), hashlib.sha256(raw).hexdigest()

anchor_h = (
    "\tCRichEditCtrl m_richFrame;   // 左侧只读代码框架\n"
    "\tCRichEditCtrl m_richCode;    // 右侧答题区域\n"
)
assert h.count(anchor_h) == 1, "PracticeDlg.h 锚点不唯一"
new_h = anchor_h + (
    "\n"
    "\t// 左侧只读框架禁止复制的子类化（实现见 PracticeDlg.cpp 的 s_FrameSubclassProc）\n"
    "\tstatic LRESULT CALLBACK s_FrameSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,\n"
    "\t\tLPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);\n"
    "\tBOOL m_bFrameSubclassed;     // 子类化是否已挂上\n"
)
h = h.replace(anchor_h, new_h, 1)
chk = save(H, h)
log.append("H  before=%d after=%d delta=%+d" % (h_size0, len(chk), len(chk) - h_size0))
log.append("H  sha256 %s -> %s" % (h_hash0[:12], hashlib.sha256(chk).hexdigest()[:12]))

# ============================================================ PracticeDlg.cpp
raw, c = load(C)
c_size0, c_hash0 = len(raw), hashlib.sha256(raw).hexdigest()

# --- C1. 构造函数里初始化成员
a = "\tm_strJudgeErr.Empty();\n\tm_strSubmittedCode.Empty();\n}\n"
assert c.count(a) == 1, "构造函数尾部锚点不唯一"
c = c.replace(a, "\tm_strJudgeErr.Empty();\n\tm_strSubmittedCode.Empty();\n"
                "\tm_bFrameSubclassed = FALSE;\n}\n", 1)
log.append("C1 构造函数已初始化 m_bFrameSubclassed")

# --- C2. 子类化回调实现（放在 OnCollapseFrameSel 之后）
a = "// 更新倒计时显示文字（HH:MM:SS格式）\n"
assert c.count(a) == 1, "UpdateTimerText 锚点不唯一"

PROC = r'''// ===== 左侧只读框架：禁止复制 =====
// 左侧是题目给的固定代码框架，学生要写的是右侧答题区，它不该被复制走。
// 用子类化来拦，比只挂 ES_READONLY 彻底 —— ES_READONLY 挡得住"改"，挡不住"复制"：
//   · WM_COPY / WM_CUT / WM_CLEAR / WM_PASTE：菜单项和快捷键最终都走这几条消息，直接吞掉；
//   · WM_CONTEXTMENU：右键菜单里就有"复制"，一并吞掉；
//   · Ctrl+C / Ctrl+X / Ctrl+V / Ctrl+A / Ctrl+Insert、Shift+Insert、Shift+Delete：
//     按键层面再挡一道（顺手把 Ctrl+A 也挡了，全选造成的大片蓝色高亮跟着消失）；
//   · 按住左键拖动时吞掉 WM_MOUSEMOVE：防止把选中的文字拖到别的程序里去。
// 注意：只挂在 m_richFrame 上，右侧答题区的复制/粘贴完全不受影响。
LRESULT CALLBACK CPracticeDlg::s_FrameSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
	switch (uMsg)
	{
	case WM_COPY:
	case WM_CUT:
	case WM_CLEAR:
	case WM_PASTE:
		return 0;                       // 剪贴板操作：一律不理

	case WM_CONTEXTMENU:
		return 0;                       // 不给右键菜单

	case WM_MOUSEMOVE:
		if ((wParam & MK_LBUTTON) != 0)
			return 0;                   // 正按着左键拖：不转发，文字就拖不出去
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			BOOL bCtrl = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);
			BOOL bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);
			if (bCtrl && (wParam == 'A' || wParam == 'C' || wParam == 'X' ||
				wParam == 'V' || wParam == VK_INSERT))
				return 0;
			if (bShift && (wParam == VK_INSERT || wParam == VK_DELETE))
				return 0;
		}
		break;

	case WM_NCDESTROY:
		// 控件销毁前必须把子类化摘掉，否则回调会留在已经释放的窗口上
		RemoveWindowSubclass(hWnd, s_FrameSubclassProc, uIdSubclass);
		break;
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

'''
c = c.replace(a, PROC + a, 1)
log.append("C2 子类化回调已插入")

# --- C3. OnInitDialog 里挂上子类化
a = "\tm_richCode.SetFocus();\n\n\treturn TRUE;\n}\n"
assert c.count(a) == 1, "OnInitDialog 尾部锚点不唯一"
c = c.replace(a,
              "\tm_richCode.SetFocus();\n"
              "\n"
              "\t// 左侧只读框架：禁止复制（拦截剪贴板消息 / 快捷键 / 右键菜单 / 拖拽）\n"
              "\tif (::IsWindow(m_richFrame.GetSafeHwnd()))\n"
              "\t\tm_bFrameSubclassed = SetWindowSubclass(m_richFrame.GetSafeHwnd(),\n"
              "\t\t\ts_FrameSubclassProc, 1, 0) ? TRUE : FALSE;\n"
              "\n"
              "\treturn TRUE;\n}\n", 1)
log.append("C3 OnInitDialog 已安装子类化")

# --- C4. 显式包含 commctrl.h（虽然 afxcmn.h 已经带进来，写明更清楚）
a = '#include <objbase.h>\n'
assert c.count(a) == 1, "objbase 头锚点不唯一"
c = c.replace(a, '#include <objbase.h>\n#include <commctrl.h>   // SetWindowSubclass：左侧只读框架禁止复制用\n', 1)
log.append("C4 已显式包含 commctrl.h")

chk = save(C, c)
log.append("C  before=%d after=%d delta=%+d" % (c_size0, len(chk), len(chk) - c_size0))
log.append("C  sha256 %s -> %s" % (c_hash0[:12], hashlib.sha256(chk).hexdigest()[:12]))

dec = chk[3:].decode("utf-8")
for k in ["s_FrameSubclassProc", "SetWindowSubclass", "RemoveWindowSubclass",
          "m_bFrameSubclassed", "DefSubclassProc", "commctrl.h"]:
    log.append("  cpp count %-22s = %d" % (k, dec.count(k)))

open(BASE + r"\tools\_nocopy_patch_report.txt", "w", encoding="utf-8").write("\n".join(log))
print("done")
