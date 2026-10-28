#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 把 yihuan-im-qt/build 里的运行时文件隔离成干净的发布目录 dist/，
# 然后用 Windows 自带的 iexpress 打成单一自解压安装包 YiHuanIM-Setup.exe。
import os, shutil, subprocess, sys

BUILD = r"F:\yihuan\WorkBuddy_Git_Projects\OpenIM\yihuan-im-qt\build\qtcreator_mingw"
ROOT  = r"F:\yihuan\WorkBuddy_Git_Projects\OpenIM\yihuan-im-qt"
DIST  = os.path.join(ROOT, "dist")
APP   = "YiHuanIM.exe"

# 运行时需要的 dll（build 根目录下所有 .dll）
runtime_dlls = [f for f in os.listdir(BUILD) if f.lower().endswith(".dll")]
# 运行时需要的插件目录
runtime_dirs = ["platforms", "imageformats", "styles", "tls",
                "networkinformation", "generic", "iconengines"]

# 清空并重建 dist
if os.path.isdir(DIST):
    shutil.rmtree(DIST)
os.makedirs(DIST)

# 1) 主程序
shutil.copy2(os.path.join(BUILD, APP), os.path.join(DIST, APP))
# 2) 所有 dll
for d in runtime_dlls:
    shutil.copy2(os.path.join(BUILD, d), os.path.join(DIST, d))
# 3) 插件目录
for d in runtime_dirs:
    src = os.path.join(BUILD, d)
    if os.path.isdir(src):
        shutil.copytree(src, os.path.join(DIST, d))

# 4) 说明文件
readme = """一幻IM (YiHuanIM) —— 局域网即时通讯 + 智能助手
================================================

【运行】
  直接双击 YiHuanIM.exe 即可。所有 Qt 运行库已随包内嵌，无需安装。

【AI 配置】
  第一次启动后，点右上角 ⚙ 设置，在「AI 提供商」填入你的
  DeepSeek / OpenAI 兼容接口的 apiKey（默认已填好 baseUrl 与模型）。
  填好 key 后，左侧「小幻」即为真正的智能助手；未填时小幻会回显提示。

【聊天记录】
  默认保存在：
  C:\\Users\\<你的用户名>\\AppData\\Roaming\\一幻IM\\history\\
  采用 JSONL 格式，可随时用记事本查看。

【局域网多机】
  同一局域网内的多台电脑，分别启动本程序，会话列表会自动发现
  在线成员（基于 UDP 广播），点名称即可互聊。服务端内置于每个客户端。
"""
with open(os.path.join(DIST, "README.txt"), "w", encoding="utf-8") as f:
    f.write(readme)

# 枚举 dist 下所有文件（相对路径），用于生成 iexpress 的 [SourceFiles] 段
files = []
for root, dirs, fnames in os.walk(DIST):
    for fn in fnames:
        full = os.path.join(root, fn)
        rel = os.path.relpath(full, DIST).replace("/", "\\")
        files.append(rel)

# 生成 iexpress 的 .sed 配置
sed = []
sed.append("[Version]")
sed.append("Class=IEXPRESS")
sed.append("SEDVersion=3")
sed.append("[Options]")
sed.append("PackagePurpose=1")          # 自解压并运行 AppLaunched
sed.append("ShowInstallProgramWindow=1")
sed.append("HideExtractAnimation=0")
sed.append("UseLongFileName=1")
sed.append("InsideCompressed=0")
sed.append("CAB_FixedSize=0")
sed.append("CAB_ResvCodeSigning=0")
sed.append("RebootMode=I")
sed.append("InstallPrompt=%InstallPrompt%")
sed.append("DisplayLicense=%DisplayLicense%")
sed.append("FinishMessage=%FinishMessage%")
sed.append("TargetName=%TargetName%")
sed.append("FriendlyName=%FriendlyName%")
sed.append("AppLaunched=%AppLaunched%")
sed.append("PostInstallCmd=%PostInstallCmd%")
sed.append("AdminPref=%AdminPref%")
sed.append("UserRTFloppy=")
sed.append("AuthorizedCDFPrefix=")
sed.append("[SourceFiles]")
sed.append("SourceFiles0=" + DIST)
sed.append("[SourceFiles0]")
for i, fpath in enumerate(files):
    sed.append("%%FILE%d%%=%s" % (i, fpath))
sed.append("[Strings]")
sed.append("InstallPrompt=")
sed.append("DisplayLicense=")
sed.append("FinishMessage=YiHuanIM extracted. Launching application...")
sed.append("TargetName=" + os.path.join(ROOT, "YiHuanIM-Setup.exe"))
sed.append("FriendlyName=YiHuanIM Installer")
sed.append("AppLaunched=" + APP)
sed.append("PostInstallCmd=<None>")
sed.append("AdminPref=0")

sed_path = os.path.join(ROOT, "setup.sed")
with open(sed_path, "w", encoding="utf-8") as f:
    f.write("\n".join(sed))

print("runtime dlls:", len(runtime_dlls))
print("dist files:", len(files))
print("sed ->", sed_path)

# 调用 iexpress 静默生成安装包
print("running iexpress ...")
r = subprocess.run(["iexpress", "/N", "/Q", sed_path],
                  capture_output=True, text=True)
print("iexpress rc =", r.returncode)
if r.stdout: print("stdout:", r.stdout[:500])
if r.stderr: print("stderr:", r.stderr[:500])

out = os.path.join(ROOT, "YiHuanIM-Setup.exe")
if os.path.isfile(out):
    print("OK ->", out, "size =", os.path.getsize(out), "bytes")
else:
    print("FAIL: setup.exe not generated")
    sys.exit(1)
