# Pendant Revision Code

这是一个基于 ESP32 的小屏 MJPEG 播放项目。

程序启动后会初始化 ST7789 240x240 屏幕和 SD 卡，自动扫描 SD 卡中的 `.mjpeg` 文件并进行播放。  
通过按键可以切换到下一个视频文件，并且会记住上一次手动切换到的文件，下次上电后继续从该文件开始播放。

## 主要功能

- 自动扫描 SD 卡中的 `.mjpeg` 文件
- 在 240x240 的 ST7789 屏幕上播放 MJPEG
- `IO23` 按键切换到下一个文件
- 按键采用“按下后松开”的上升沿触发
- 开机后前 1.5 秒屏蔽按键误触
- 使用 ESP32 NVS 记住上次手动选择的文件
- 背光支持 PWM 亮度控制

## 硬件信息

- 主控：ESP32
- 屏幕驱动：ST7789
- 分辨率：240x240
- 存储：SD 卡

## 当前引脚定义

- `TFT_BL`：GPIO22
- `NEXT_FILE_PIN`：GPIO23
- `TFT_DC`：GPIO27
- `TFT_RST`：GPIO33
- `TFT_CS`：GPIO5
- `SD_CS`：GPIO13
- `SCK`：GPIO14
- `MOSI`：GPIO15
- `MISO`：GPIO2

## 使用方法

1. 将一个或多个 `.mjpeg` 文件拷贝到 SD 卡
2. 将 SD 卡插入设备
3. 上电后设备会自动播放扫描到的文件
4. 按下并松开 `IO23` 对应按键，可以切换到下一个文件
5. 断电重启后，会恢复到上次手动切换到的文件

## 开发说明

- 当前 PlatformIO 环境在 [platformio.ini:10](C:\Users\EYCO\Documents\PlatformIO\Projects\240713-145244-upesy_wroom\platformio.ini:10) 使用 `esp32dev`
- 显示与播放主逻辑位于 [SD_MJPEG_video.cpp](C:\Users\EYCO\Documents\PlatformIO\Projects\240713-145244-upesy_wroom\src\SD_MJPEG_video.cpp)
- MJPEG 解码与输出封装位于 [MjpegClass.h](C:\Users\EYCO\Documents\PlatformIO\Projects\240713-145244-upesy_wroom\src\MjpegClass.h)
