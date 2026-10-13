# FFmpeg 影像魔方

这是一个基于 Qt Widgets 与 FFmpeg 的桌面音视频播放器工程，包含播放器源码、界面资源和 Windows 运行包。

## 目录

- `../Core/VisonCode/vision_player/`：Qt Creator 工程源码，入口工程为 `vision.pro`。
- `../App/st_vplayer_v1.1/`：整理后的 Windows 应用目录。
- `../App/st_vplayer_v1.1.zip`：本地打包归档。该归档约 79 MB，目前按 `.gitignore` 规则保留在本地。

## 构建

使用 Qt Creator 打开 `../Core/VisonCode/vision_player/vision.pro`。工程依赖 Qt（Core、Gui、Widgets、Multimedia、Network）和 FFmpeg 开发库；需要按本机 Qt、编译器和 FFmpeg 路径调整 `.pro` 中的 include/lib 配置。项目使用 C++11。

## 说明

已提交源码、界面资源及必要头文件；生成的二进制、构建缓存和大型压缩包按仓库 `.gitignore` 规则排除。FFmpeg、Qt 等第三方组件仍需遵守各自许可证。
