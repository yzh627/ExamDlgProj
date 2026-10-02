#pragma once
// ===== 品牌 / 版本：只在这里写一次 =====
// 以前同一个版本号散在 4 处（rc 的 FILEVERSION、rc 的 FileVersion、
// Setup.cpp 里的 DisplayVersion、说明.txt 的〔1.0〕），改一处忘一处就会对不上；
// 控制面板的"发布者"字段又跟产品名重复。现在统一从这里取。
#define APP_VER_D0          1
#define APP_VER_D1          0
#define APP_VER_D2          0
#define APP_VER_D3          0
#define APP_VERSION_DOTS_A  "1.0.0.0"
#define APP_VERSION_STR_W   L"1.0.0"
#define APP_AUTHOR_STR      "一个中职生"
#define APP_AUTHOR_STR_W    L"一个中职生"
#define APP_FEEDBACK_STR_W  L"微信公众号「一个中职生」"
#define APP_PRODUCT_STR     "河北对口升学计算机程序设计练习系统"

#define IDD_SETUP               101
#define IDC_STATIC              (-1)
#define IDR_PAYLOAD_EXE         201
#define IDR_PAYLOAD_MANIFEST    202
#define IDR_PAYLOAD_BANK        203
#define IDR_PAYLOAD_EXE32       204
#define IDR_PAYLOAD_SUPPORT     205
#define IDR_PAYLOAD_ABOUT       206
#define IDR_PAYLOAD_UNINST      207
#define IDI_SETUP               129

#define IDC_EDIT_DIR            1001
#define IDC_BTN_BROWSE          1002
#define IDC_CHK_DESKTOP         1003
#define IDC_CHK_STARTMENU       1004
#define IDC_CHK_RUN             1005
#define IDC_STATIC_STATUS       1006
#define IDC_STATIC_BRIEF        1007
#define IDC_STATIC_INFO         1008
#define IDC_STATIC_AUTHOR       1009
