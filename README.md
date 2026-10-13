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

源码、工程配置、文档、PDF、视频、应用包和依赖库按原目录保存。视频及较大的文件使用 Git LFS 管理；克隆后运行 `git lfs pull` 下载完整内容。GitHub Desktop 支持 Git LFS。`.gitignore` 仅排除构建中间产物、IDE 缓存和本地日志等文件。

README 导航和文件清单已核对；尚未在目标硬件或全部开发工具链中编译、运行所有示例。

## 维护

新增代码或文档后，使用 GitHub Desktop 提交更改，再执行 Push origin。涉及第三方代码或资料时，请遵守对应组件的许可证和使用要求。
