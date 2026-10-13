# Engineering Project Warehouse

工程项目与配套资料集合。保留各主题的原有目录层级，通过下方入口查看代码与文档。

## 项目导航

| 目录 | 内容 | 文档 |
| --- | --- | --- |
| [FFmpeg 影像魔方](FFmpeg%20%E5%BD%B1%E5%83%8F%E9%AD%94%E6%96%B9) | Qt Widgets 与 FFmpeg 桌面音视频播放器，包含源码、界面资源和本地应用包。 | [README](FFmpeg%20%E5%BD%B1%E5%83%8F%E9%AD%94%E6%96%B9/Doc/README.md) |
| [LabTrace 实验室试剂追溯系统](LabTrace%20%E5%AE%9E%E9%AA%8C%E5%AE%A4%E8%AF%95%E5%89%82%E8%BF%BD%E6%BA%AF%E7%B3%BB%E7%BB%9F) | STM32 + ESP8266 数据采集、Qt 服务端与客户端，以及 MQTT/TCP/SQLite 通信和存储工程。 | [README](LabTrace%20%E5%AE%9E%E9%AA%8C%E5%AE%A4%E8%AF%95%E5%89%82%E8%BF%BD%E6%BA%AF%E7%B3%BB%E7%BB%9F/Doc/README.md) |

## 使用方式

各子目录是独立项目或学习示例；仓库没有统一构建命令。进入所需主题，按文档中的工程入口选择 Qt Creator、Keil MDK、STM32CubeMX、Vivado 或 C/C++ 编译工具。

## 本地资料与上传范围

源码、工程配置和可上传文档按原目录保存。当前 `.gitignore` 排除了构建产物、IDE 缓存、运行二进制以及部分本地媒体/归档；这些文件仍保留在本地。学习仓库的 PDF 和视频当前不在 Git 中。

README 导航和文件清单已核对；尚未在目标硬件或全部开发工具链中编译、运行所有示例。

## 维护

新增代码或文档后，使用 GitHub Desktop 提交更改，再执行 Push origin。涉及第三方代码或资料时，请遵守对应组件的许可证和使用要求。
