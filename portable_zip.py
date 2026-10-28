#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 dist/ 打包成绿色免安装版 zip（解压即用，无需安装）。"""
import os
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, "dist")
OUT = os.path.join(ROOT, "YiHuanIM-Portable.zip")

if not os.path.isdir(SRC):
    raise SystemExit("dist/ 不存在，请先运行 dist_build.py 收集依赖")

# 若输出文件已存在则先删除，避免写内部
if os.path.exists(OUT):
    os.remove(OUT)

count = 0
with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(SRC):
        for name in files:
            full = os.path.join(base, name)
            # arcname 取 dist 之后的相对路径，使解压后直接是 exe+dll+插件目录
            arc = os.path.relpath(full, SRC)
            z.write(full, arcname=arc)
            count += 1

print("done: %s  files=%d  size=%.2f MB" %
      (OUT, count, os.path.getsize(OUT) / 1024.0 / 1024.0))
