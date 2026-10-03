# -*- coding: utf-8 -*-
"""
公众号封面生成
- 头图 900x383（2.35:1，公众号首图标准）
- 大图 500x400（分享卡片缩略图，可选，这里一并生成）
配色：与推文一致的绿色系
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = r"C:\Users\30601\Desktop\封面"
os.makedirs(OUT, exist_ok=True)

# ---- 配色（与 HTML 推文一致）----
C_TOP    = (46, 158, 107)     # #2e9e6b 主绿
C_BOT    = (26, 111, 74)      # #1a6f4a 深绿
C_CARD   = (255, 255, 255)
C_TEXT   = (29, 59, 44)       # #1d3b2c
C_SUB    = (74, 107, 88)      # #4a6b58
C_LINE   = (223, 232, 227)

F_REG  = r"C:\Windows\Fonts\msyh.ttc"
F_BOLD = r"C:\Windows\Fonts\msyhbd.ttc"
F_HEI  = r"C:\Windows\Fonts\simhei.ttf"


def vgrad(w, h, c1, c2):
    """竖向渐变底"""
    img = Image.new("RGB", (1, h))
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        px[0, y] = (
            int(c1[0] + (c2[0] - c1[0]) * t),
            int(c1[1] + (c2[1] - c1[1]) * t),
            int(c1[2] + (c2[2] - c1[2]) * t),
        )
    return img.resize((w, h), Image.BICUBIC)


def dgrad(w, h, c1, c2):
    """对角渐变底"""
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            t = (x / max(1, w - 1) * 0.45 + y / max(1, h - 1) * 0.55)
            px[x, y] = (
                int(c1[0] + (c2[0] - c1[0]) * t),
                int(c1[1] + (c2[1] - c1[1]) * t),
                int(c1[2] + (c2[2] - c1[2]) * t),
            )
    return img


def deco_dots(d, w, h, color=(255, 255, 255), seed_pts=None):
    """右侧装饰：点阵网格，暗示代码/矩阵"""
    pts = seed_pts or []
    for (cx, cy, r, a) in pts:
        ov = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        od = ImageDraw.Draw(ov)
        od.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color + (a,))
        return ov
    return None


def make_cover():
    W, H = 900, 383
    base = dgrad(W, H, C_TOP, C_BOT)

    # 右上角点阵装饰（两层错位，产生层次）
    dots = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(dots)
    for row in range(9):
        for col in range(14):
            cx = 636 + col * 19
            cy = 24 + row * 19
            if cx > W - 12 or cy > H - 12:
                continue
            # 越靠右下越淡
            t = (col / 14) * 0.6 + (row / 9) * 0.4
            a = int(70 * (1 - t))
            if a <= 4:
                continue
            r = 3 if (row + col) % 3 else 4
            dd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255, a))
    base = Image.alpha_composite(base.convert("RGBA"), dots)

    # 左侧大圆（半透明，制造层次）
    ov = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    od.ellipse([-150, -230, 250, 170], fill=(255, 255, 255, 16))
    od.ellipse([600, 250, 1050, 700], fill=(0, 0, 0, 22))
    base = Image.alpha_composite(base, ov)

    d = ImageDraw.Draw(base)

    # ---- 左侧白色卡片 ----
    cx0, cy0, cx1, cy1 = 58, 74, 590, 309
    card = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    cd = ImageDraw.Draw(card)
    cd.rounded_rectangle([cx0, cy0, cx1, cy1], radius=16,
                         fill=C_CARD + (242,), outline=(255, 255, 255, 90), width=1)
    # 卡片投影
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle([cx0 + 3, cy0 + 5, cx1 + 3, cy1 + 5], radius=16,
                          fill=(0, 0, 0, 46))
    sh = sh.filter(ImageFilter.GaussianBlur(7))
    base = Image.alpha_composite(base, sh)
    base = Image.alpha_composite(base, card)
    d = ImageDraw.Draw(base)

    f_ver  = ImageFont.truetype(F_BOLD, 15)
    f_ttl  = ImageFont.truetype(F_BOLD, 40)
    f_sub  = ImageFont.truetype(F_REG, 17)

    # 标签「计算机技能测试练习系统」
    tag = "计算机技能测试练习系统"
    tw = d.textlength(tag, font=f_ver)
    px0, py0, px1, py1 = cx0 + 28, cy0 + 22, cx0 + 28 + int(tw) + 22, cy0 + 50
    d.rounded_rectangle([px0, py0, px1, py1], radius=14, fill=(232, 246, 238, 255))
    d.text((px0 + 11, py0 + 7), tag, font=f_ver, fill=(31, 122, 82, 255))

    # 主标题
    d.text((cx0 + 28, cy0 + 66), "对口升学", font=f_ttl, fill=C_TEXT)
    d.text((cx0 + 28, cy0 + 118), "计算机练习系统", font=f_ttl, fill=C_TEXT)

    # 分隔线
    d.line([cx0 + 30, cy0 + 180, cx0 + 150, cy0 + 180], fill=C_LINE, width=2)

    # 副标题
    d.text((cx0 + 28, cy0 + 196), "提交代码即自动判分 · 判题环境与省考场一致",
           font=f_sub, fill=C_SUB)

    # 版本角标
    vw = d.textlength("v1.0.0", font=f_ver)
    vx1, vy0, vy1 = cx1 - 24, cy0 + 22, cy0 + 50
    vx0 = vx1 - int(vw) - 22
    d.rounded_rectangle([vx0, vy0, vx1, vy1], radius=14, fill=(29, 59, 44, 255))
    d.text((vx0 + 11, vy0 + 7), "v1.0.0", font=f_ver, fill=(255, 255, 255, 255))

    # ---- 右侧：抽象的"代码窗口"示意 ----
    wx0, wy0, wx1, wy1 = 636, 100, 838, 284
    win = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    wd = ImageDraw.Draw(win)
    wd.rounded_rectangle([wx0, wy0, wx1, wy1], radius=12,
                         fill=(255, 255, 255, 30), outline=(255, 255, 255, 70), width=1)
    # 标题栏三颗点
    for i in range(3):
        px_ = wx0 + 16 + i * 15
        py_ = wy0 + 15
        wd.ellipse([px_ - 3, py_ - 3, px_ + 3, py_ + 3], fill=(255, 255, 255, 110))
    # 代码行：宽度按窗口实际可用宽度算，绝不溢出
    pad_l, pad_r = 20, 20
    avail = (wx1 - pad_r) - (wx0 + pad_l)
    rows = [0.66, 0.40, 0.78, 0.32, 0.56]
    yy = wy0 + 40
    for k, frac in enumerate(rows):
        a = 150 if k % 2 == 0 else 96
        wd.rounded_rectangle([wx0 + pad_l, yy, wx0 + pad_l + int(avail * frac), yy + 7],
                             radius=3, fill=(255, 255, 255, a))
        yy += 22
    # 底部"通过"徽章：用矢量勾，不用字体（微软雅黑缺 ✓ 字形）
    # 内缩以免被画布裁到，圆要完整可见
    br = 26
    bcx, bcy = wx1 + 2, wy1 - 4
    if bcx + br > W - 6:
        bcx = W - 6 - br
    if bcy + br > H - 6:
        bcy = H - 6 - br
    wd.ellipse([bcx - br, bcy - br, bcx + br, bcy + br], fill=(255, 255, 255, 248))
    # 手绘勾：两段折线
    lw = max(4, br // 6)
    p1 = (bcx - 13, bcy - 1)
    p2 = (bcx - 4, bcy + 9)
    p3 = (bcx + 14, bcy - 11)
    wd.line([p1, p2], fill=(31, 122, 82), width=lw)
    wd.line([p2, p3], fill=(31, 122, 82), width=lw)
    # 勾的两端加圆头
    for pt in (p1, p2, p3):
        wd.ellipse([pt[0] - lw // 2, pt[1] - lw // 2, pt[0] + lw // 2, pt[1] + lw // 2],
                   fill=(31, 122, 82))
    base = Image.alpha_composite(base, win)

    p = os.path.join(OUT, "封面-头图900x383.png")
    base.convert("RGB").save(p, "PNG")
    return p


def make_square():
    """分享缩略图 500x400"""
    W, H = 500, 400
    base = dgrad(W, H, C_TOP, C_BOT)
    ov = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    od.ellipse([-120, -160, 200, 160], fill=(255, 255, 255, 16))
    base = Image.alpha_composite(base.convert("RGBA"), ov)
    d = ImageDraw.Draw(base)

    f_ttl = ImageFont.truetype(F_BOLD, 44)
    f_sub = ImageFont.truetype(F_REG, 19)
    f_ver = ImageFont.truetype(F_BOLD, 16)

    d.text((44, 96), "对口升学", font=f_ttl, fill=(255, 255, 255, 255))
    d.text((44, 152), "计算机练习系统", font=f_ttl, fill=(255, 255, 255, 255))

    d.line([46, 224, 130, 224], fill=(255, 255, 255, 130), width=2)
    d.text((44, 242), "提交代码即自动判分", font=f_sub, fill=(255, 255, 255, 220))

    # 版本角标：底色必须是深色，否则白字画在白底上看不见
    tw = d.textlength("v1.0.0", font=f_ver)
    bx1, by0, by1 = W - 44, 44, 44 + 32
    bx0 = bx1 - int(tw) - 24
    d.rounded_rectangle([bx0, by0, bx1, by1], radius=16, fill=(29, 59, 44, 236))
    d.text((bx0 + 12, by0 + 7), "v1.0.0", font=f_ver, fill=(255, 255, 255, 255))

    d.text((44, H - 42), "微信公众号：一个中职生", font=f_sub, fill=(255, 255, 255, 190))

    p = os.path.join(OUT, "封面-缩略500x400.png")
    base.convert("RGB").save(p, "PNG")
    return p


if __name__ == "__main__":
    a = make_cover()
    b = make_square()
    for p in (a, b):
        im = Image.open(p)
        print(f"{os.path.basename(p)}  {im.size[0]}x{im.size[1]}  {os.path.getsize(p)} bytes")
