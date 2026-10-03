# SignPath 申请被拒：原因分析与替代路线

## 拒信原文的关键点

> After reviewing your submission, we're unable to approve your application at this time.
>
> The Foundation program is designed for projects that have already established
> a certain level of **public trust and visibility**... we look for external
> signals such as **community adoption (GitHub stars, forks, contributors),
> external articles, independent references or discussions (Reddit, Stack Overflow,
> YouTube, etc.) or institutional backing**, and evidence of sustained activity.
>
> This isn't a judgment on the quality or potential of your work, it's purely
> about the level of **public visibility**.

**拒的不是代码，是"公开可见度"。** 仓库建于今天，客观上确实没有任何外部信号。

信里明确说欢迎将来重新申请（"you're very welcome to reapply once it has gained
broader recognition"），这不是永久否决。

---

## ★ 一个重要事实：EV 证书不再有 SmartScreen 优势

这一点会直接影响你怎么选，**很多资料（包括一些 CA 的销售页）到现在还在误导**。

**2024 年 8 月，微软更新 Trusted Root Program，取消了 EV 证书的 SmartScreen 立即豁免。**

| | 2024-08 之前 | 2024-08 之后 |
|---|---|---|
| EV 签名 | 首次下载即绕过"已保护你的电脑" | 与 OV **完全相同** |
| OV 签名 | 靠下载量积累信誉 | 靠下载量积累信誉 |

微软自己的文档写得很直白：

> 仅为了避免 SmartScreen 警示而支付 EV 溢价（每年 $400+）**已不再合理** ——
> 你仍会看到与 OV 证书相同的警示。

**推论：**
- **别买 EV 证书。** 花钱多几千，SmartScreen 体验一模一样。
- **签名 ≠ 不被拦。** 新程序不管用什么证书签，初期都会被拦，靠下载量积累信誉。
- **"签名"的价值是消除"未知发布者"、证明文件未被篡改**，不是消除蓝窗。

---

## 三条可行路线

### 路线 A：等攒够可见度，再申请 SignPath（免费）

**要攒什么（任一即可，官方原话）：**

- GitHub stars / forks / contributors
- 外部文章、教程、讨论（Reddit、Stack Overflow、YouTube、B 站、博客）
- 机构背书（学校、公司、社团采用）
- 持续活跃的证据

**现实的攒法（按性价比排序）：**

| 动作 | 说明 | 成本 |
|---|---|---|
| 发到 B 站 / 小红书 / 知乎 | 录个"河北对口升学计算机练习系统怎么用"的视频或图文 | 0 |
| 发到 V2EX / 掘金 / CSDN | 技术文章，介绍实现思路（判题环境对齐省考场这个点挺有内容） | 0 |
| 给几个职教学校的老师试用 | 真实用户反馈 + 机构背书，这两条最硬 | 0 |
| 邀请同学 star / 提 issue | 别刷 star，要真实反馈 | 0 |

**时间预期：** 认真做 1–2 个月，积累 20–50 star + 几篇外部讨论 + 真实用户反馈，
再申请通过率会高很多。

⚠️ **不要做的事**：买 star、刷 star、给小号点关注。这些一经查出会被永久拒绝，
而且违背开源项目的意义。

**推荐度：★★★** —— 完全免费，且申请通过后长期受益。

---

### 路线 B：买个 IV 证书（个人可用，约 200–400 元/年）

**好消息：IV（个人验证）证书不需要公司营业执照**，只要身份证验证。
它在 Windows 上的信任等级和 OV **完全一样**（都是消除"未知发布者"）。

**主流选择与价格（2026 年，含云 HSM 交付）：**

| CA | 大致价格 | 备注 |
|---|---|---|
| **SSL.com** | $64.50–129/年 | **最低价**，eSigner 云签名，国内可付 |
| Sectigo / Comodo IV | $215–320/年 | 渠道多、资料齐 |
| DigiCert IV | $400/年 | 贵，无必要 |

**注意：**
- **有效期只有 460 天**（CA/B Forum 2026-03 新规），需要每年续
- 私钥必须放硬件（USB token 或云 HSM），不能存成 `.pfx` 文件
- 签名者是**你的姓名**（不是 SignPath Foundation）

**代码怎么接？** 我们已经预留好了：`rebuild.ps1` 里 `SIGNTOOL_HSM=keylocker` 那条路线
就是为这个准备的，拿到证书后设一个环境变量就能用。

**推荐度：★★** —— 花钱但能立刻用。适合"急着发布、等不到攒 star"的情况。

---

### 路线 C：暂不签名，只发布 + 说明

**现在就能做的：**

1. 在 Release 页面放一份**说明文件**，写清楚：
   - 这个软件是开源的，源码就在同页面上，可以自己核对
   - 出现蓝窗时点「更多信息」→「仍要运行」
   - 校验方法：给 SHA256 值，让学生自己比对
2. 在微信公众号写一篇完整的开发记录（顺便就是路线 A 需要的外部信号）

**为什么这不是"没搞定"：**

SmartScreen 拦的是"没有署名的程序"。如果学生能确认"这软件是我同学/某个 GitHub 仓库
开源出来的"，蓝窗的实际影响就小很多 —— 毕竟这是备考工具，不是需要防unknown的软件。

**推荐度：★★★** —— 零成本，立刻能发。

---

## 我的建议

**组合打法：路线 C 立刻解渴 + 路线 A 慢慢攒。**

1. **现在**：路线 C，先把软件发出去，让学生能用上
2. **同时**：路线 A，认真写几篇技术文章、发视频、找老师试用
3. **1–2 个月后**：攒够信号，重新申请 SignPath
4. **如果学生催得急**：路线 B 买 IV 证书救急

**不建议**为了"必须签名"而买 EV 证书 —— 那是纯浪费（见上面 2024 年的变更）。

---

## 顺带：现有的加固成果已经生效

不管走哪条路，这些都已经到位了（**已提交并推送**）：

- CFG（控制流保护）已开 —— 之前 4 个 exe 全都没有
- ASLR / DEP / High Entropy VA / CET shadow stack 已开
- 4 个可执行文件的一致性自检
- 私钥零暴露（本机只有 SSH 部署私钥，从未离开过本机）
- 产物来源可验证：GitHub Actions 全自动构建，Release 页可查

**这些是实打实的安全提升，和签名是两件事。** 签名解决的是"来源可信 + 不被误伤"，
加固解决的是"程序本身更难被攻击"。两者都做了最好。

---

## 外部参考

- SignPath 申请条件：<https://signpath.org/terms>
- 微软关于 EV 豁免取消的说明：
  <https://learn.microsoft.com/zh-hk/windows/apps/package-and-deploy/code-signing-options>
- 证书有效期新规（460 天）：CA/B Forum ballot CSC-31
