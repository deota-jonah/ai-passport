[English](deotaland-agent-link.md)

# Deotaland Agent Link 固件

本应用分支将 AI Passport 变为 Wi-Fi 语音终端，连接在
[Deotaland 控制台](https://bot.deotaland.cn/#/devices)绑定的 Agent。
官方硬件测试示例源码仍可作为参考，实际启动入口是
`main/agent_link_app/passport_main.cc`。

## 配网与操作

1. 首次开机，用手机连接 `DeotaPassport-XXXX`，浏览器打开
   `http://192.168.4.1`，填写 2.4 GHz Wi-Fi 名称和密码。
2. 设备连接路由器后关闭配网热点，屏幕显示六位设备识别码（激活码）和有效时间。
3. 打开控制台的设备页，用识别码添加设备，再绑定支持语音的 Agent。
   固件不需要预置用户账号 API Key。
4. 按 OK 继续最近会话或创建首个会话，再按 OK 结束通话；上下键调节音量。
5. 长按 OK 打开会话与设置，上下键选择，OK 确认。更换 Wi-Fi 需要再次确认，
   随后只清除网络设置并重启配网，保留平台归属与 Agent 绑定。
6. 在设置中打开语言选项，选择中文或英文。所有固定界面文案统一切换，
   语言偏好保存至 NVS；新打开的配网页面使用同一语言。网络名称、地址与
   智能体返回的内容保持原始值。

取得 IP 后才保存 Wi-Fi 凭据。连接尝试最长 20 秒，失败后回到配网。
识别码到期后重新申请。设备凭据、设备标识、会话选择和音量保存在 NVS，
正常重启会自动连接。凭据被撤销后重新激活；已归属其他账号的设备必须先在
控制台解绑。更换 Agent 后会结束旧通话并清除旧会话选择。

屏幕右上角显示电量，读取失败时显示 `--`。应用使用新的配网卡片、识别码、
状态和会话界面，通过官方 BSP 面板以一个 1,920 字节 DMA 条带绘制，
不启动 LVGL，字体常驻 Flash。回复使用 8 KB PCM 缓冲，语音为半双工，
扬声器播放及回声尾部期间屏蔽麦克风。为节省内存，关闭 MQTT 和 BLE；
本版本提供 HTTP 激活、Agent/会话接口及 Opus 实时 WSS 语音，
不宣称支持 MQTT 远程控制或 OTA。TLS 证书校验保持开启。

## 构建与验证

官方基线要求 ESP-IDF 5.5.3；用户已同意本次复用本机的 ESP-IDF 5.5.4。
本机 SDK 报告 `v5.5.4-dirty`，本应用未改变其原有修改。
添加音频编解码依赖时，将 BSP 的 LVGL 固定为原有 9.5.0，避免无关升级。

在已激活的 ESP-IDF 终端中配置一次，并以最多两个任务编译：

```text
idf.py -B build reconfigure
ninja -C build -j 2
idf.py -B build merge-bin -o FoloToy-AI-Passport-full.bin
python tools/verify_firmware.py build
python tools/archive_firmware.py create build --archive-root build/firmware
python tools/test_agent_link.py
```

在具备主机 C 编译器和 actionlint 的兼容 shell 中运行 `tools/validate.sh`，
要求见[官方验证流程](engineering/build-and-test.md)。新增屏幕测试执行实际应用
渲染代码，检查 UI 源码的每个非 ASCII 字符是否有字形，验证 UTF-8 截断、语言存储值解码、翻译表完整性、语言隔离和播放
缓冲，并输出中英文两套配网、绑定、过期、就绪及语言设置页面。字体支持导入的 GB2312 字符集，
不覆盖全部 Unicode；不支持的字形保留可见占位框。

合并固件用于本款 ESP32-C3/8 MB 设备的 `0x0` 地址。写入合并固件可能重置
NVS，需要重新配网、激活；使用兼容分区表的分段烧录可以保留 NVS。
编译不代表允许烧录或整片擦除，参见[数据策略](engineering/firmware-layout.md#flashing-and-stored-data)。

提交前需要实测：开机、手机配网、错误密码和路由器离线恢复、识别码过期、
认领绑定、重启持久化、语音收发、Agent 换绑、更换 Wi-Fi、电量及屏幕显示。
记录板型版本、构建标识和实际结果；主机预览与编译通过不等于实机通过。
ESP-IDF 5.5.3 的官方 CI 验证仍需单独完成。

## 来源与官方提交

官方基线为 FoloToy/ai-passport 提交
`006c2279852e352a8433b2c21e5e09be2ca36f77`。
MIT 协议的 Agent Link SDK 来自
[Deotaland/Agent_link](https://github.com/Deotaland/Agent_link/tree/9f664fc028a664a47789506802aecfb0b69b7e18/firmware/components/agent_link)
的 `9f664fc028a664a47789506802aecfb0b69b7e18`，保留原文档和
[许可证](../../components/agent_link/LICENSE)。SDK 的本地修改包括取消 NVS 初始化失败时自动擦除整个 NVS 的行为，
以及添加有界的配网页面语言钩子和本地化页面模板。应用的语音、会话、条带绘制和播放逻辑参考同一
提交；设备驱动改用官方 BSP 适配，配网引导和导航在本分支实现。

Montserrat 14/20/28 条带字体保留 LVGL MIT 声明，其可选 sparse-base 字段
显式初始化为零。14 px CJK 条带字体从该提交
原样导入，原生成工具为 `firmware/boards/common/fonts/make_raster_cjk.py`，
输入为其 `cjk_font.bin`；保留 [SIL 字体许可证](../../assets/fonts/agent_link/OFL.txt)。
所有字形源码从 `assets/fonts/agent_link/` 单独编译一次。

官方的[社区市场发布流程](release/project-completion.md#action-a)和
[贡献规则](../../.github/CONTRIBUTING.md)与本地开发是不同步骤。
实机验收后可按应用渠道提交已验证固件；向上游提交应用源码 PR 仍需维护者确认
接收范围。提交材料应包括中英文说明、封面和设备截图、配网及控制台地址、能力与
限制、固件哈希、测试结果。构建本分支不会自动执行提交、推送、创建 PR、
上传固件或发布市场作品。

## 本机验证限制

本机 Windows 不允许创建目录符号链接，官方技能安装测试因此有三项
`WinError 1314` 失败。保留这些失败记录，它们与固件功能无关。此前的
BSP、运行时、仓库及固件归档测试已通过；双语应用测试单独运行。
完整静态检查仍需在允许符号链接的环境中重新运行。

## 本次构建记录

- 版本：`0.1.0`，ESP-IDF `v5.5.4-dirty`，ESP32-C3，8 MB Flash。
- Build：PASS，官方固件验证与归档复核通过。
- Host tests：双语应用、字形、语言隔离、播放缓冲、仓库与工作流检查通过；
  完整静态检查仍有前述三项 Windows 权限失败。
- Device tests：NOT RUN，仅只读发现 Espressif USB 候选端口 `COM113`。
- Unverified：实机配网、认领、语音、重启与语言持久化，以及 ESP-IDF 5.5.3 CI。

```text
Archive: build/firmware/1125fd166dbad8ffebc91ff5064b69ba795d6c2560c5c4036291cb96cbe945b8
Full image SHA-256: 1125fd166dbad8ffebc91ff5064b69ba795d6c2560c5c4036291cb96cbe945b8
Matching ELF SHA-256: aad82f1d944288e2f02905a87d8034cf04e00e02c34a1ac06d5ab2b1b960f6d0
```

## 已知限制：已有会话的语音连接恢复

后续用户提供的设备日志与本次 ELF 标识匹配，确认了启动、网络连接、平台认证，
以及新建会话后的语音收发。已有会话可能被服务端标记为仍有实时连接；平台将
`realtime_connection_live` 冲突包装在 HTTP 200、业务码 1002 的响应中，
当前固件只对实际 HTTP 409 触发恢复，因此该场景会请求失败。
暂时可选择未占用的会话或新建会话；恢复修复仍待用户确认，未包含在本次提交。
原构建记录的设备测试状态保留为当时状态，其余实机验收仍需完成。
