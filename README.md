# 河北对口升学计算机程序设计练习系统

一个用于河北对口升学计算机类考试备考的练习工具。提交代码即自动判分，运行环境与省考场同环境。

作者：[一个中职生](https://github.com/) ｜ 版本：1.0.0 ｜ 许可证：[MIT](LICENSE)

## 功能

- **题库练习**：按难度筛选、随机抽题、计时训练
- **提交即判分**：写入 `.cs` 并调用编译器编译，与判题逻辑对答案给出判定
- **环境一致**：判题用的编译与运行参数按省考场口径配置，避免"在家能做、考场做不出"

## 界面

主界面提供题库列表、难度筛选与练习设置；练习界面为左右布局，左侧代码编辑区、右侧题目与答题区，两个界面之间以非模态对话框切换。

## 运行环境

| 项目 | 要求 |
|---|---|
| 操作系统 | Windows 10 / 11（x64 或 x86） |
| 运行时依赖 | 无需安装其它组件，静态链接 |
| 权限 | 普通用户权限即可（`asInvoker`） |

## 从源码构建

需要 Visual Studio 2022（含 C++ 桌面开发工作负载）与 Windows SDK。

```bat
双击 一键重建.bat
```

或在 VS 中打开 `ExamDlgProj.sln` → 重新生成解决方案。

构建脚本 `tools\rebuild.ps1` 会依次产出：x64 与 x86 主程序 → 加密题库 `questions.dat` → 安装程序与卸载程序 → 更新 `发布包\`。

## 发布

打 `v*` 标签触发 GitHub Actions 自动构建。配置 SignPath 后产物会获得免费的
OV 代码签名（详见 `发布说明\签名与发布.md`），未配置则产出未签名版本。

## 目录结构

```
ExamDlgProj.cpp / .h        程序入口与主对话框
ExamDlgProjDlg.cpp / .h     主界面
PracticeDlg.cpp / .h        练习界面
QuestionBank.cpp / .h       题库加载与打包
PublicDef.h                公共数据结构
app.manifest               DPI 感知 / supportedOS / 长路径
安装程序\Setup.cpp          自研安装器（含内嵌 payload）
tools\                     构建与自检脚本
```

## 安全说明

发布前可运行自检脚本，它会检查私钥是否误入仓库、产物是否已签名、
关键加固标志（ASLR / DEP / CFG）是否开启、以及打包产物是否被篡改：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_signing.ps1
```

## 免责声明

本项目为学习与备考用途，不保证完全无缺陷。
欢迎通过微信公众号「一个中职生」反馈问题与建议。
