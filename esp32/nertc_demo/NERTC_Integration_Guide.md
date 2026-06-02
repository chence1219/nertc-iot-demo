# NERtc ESP32 集成指南

本文档面向在 **xiaozhi-esp32** 项目基础上集成网易云信 NERtc IoT SDK 的开发者，说明如何将 NERtc 能力引入现有工程。

> **SDK 版本**：1.2.7
> **文档更新时间**：2026-05-19
> **ESP-IDF 版本**：release-v5.5（兼容，见注意事项）
> **Demo 工程路径**：`demo/esp32/`

---

## 目录

1. [依赖库说明](#1-依赖库说明)
2. [资源文件](#2-资源文件)
3. [分区配置](#3-分区配置)
4. [代码集成](#4-代码集成)
5. [Kconfig 配置项说明](#5-kconfig-配置项说明)
6. [本地配置文件（config.bin）](#6-本地配置文件configbin)
7. [构建与烧录](#7-构建与烧录)
8. [进阶功能](#8-进阶功能)

---

## 1. 依赖库说明

### 1.1 组件库一览

| 组件名 | 说明 | 是否必须 |
|--------|------|----------|
| `nertc_sdk` | 网易云信 IoT SDK 核心库，提供音频流传输和 AI 对话能力 | **必须** |
| `nertc_wake_up` | 云信自定义唤醒词 SDK | 使用自定义唤醒词时必须 |
| `esp-ml307` | 4G 模组（ML307/EC801E/NT26K）的 AT 命令网络库，位于 `components/esp-ml307`，由本项目独立维护 | 使用 4G 模组时按需参考 |
| `esp-hosted`（仆从芯片固件） | ESP32-P4 无内置 WiFi，需外挂一颗 ESP32（如 ESP32-C6）作为仆从芯片提供网络，仆从芯片运行 esp-hosted slave 固件。参考仓库：https://github.com/espressif/esp-hosted | 使用 P4 + 仆从芯片方案时必须 |

### 1.2 CMakeLists.txt 配置

上述组件需在 `main/CMakeLists.txt` 的 `idf_component_register` 的 `PRIV_REQUIRES` 中声明，否则链接阶段会报符号缺失错误。

```cmake
idf_component_register(SRCS ${SOURCES}
    ...
    PRIV_REQUIRES
        ...
        nertc_sdk
        nertc_wake_up       # 使用自定义唤醒词时
        ...
)
```

> **IDF 版本注意**：
> - **IDF 5.4**：组件会自动被 CMake 检测到，无需额外处理。
> - **IDF 5.5+**：`touch_element` 组件在 IDF 5.5.2 中被移除，已通过 `idf_component.yml` 条件依赖处理。若遇到编译错误，请确认 `main/idf_component.yml` 中相关版本约束与你的 IDF 版本匹配。

---

## 2. 资源文件

### 2.1 本地化语音文件

路径：`main/assets/locales/zh-CN/`

此目录存放设备本地播放的提示音（如连接中、等待、错误提示等）。可参考示例工程中的音频文件，按需加载到自己的工程中。若不需要中文提示音，可跳过。

### 2.2 本地配置文件（config.bin）

路径：`create_local_config/`

该目录提供生成配置文件 `config.bin` 所需的模板和工具脚本。`config.bin` 以 SPIFFS 格式烧入设备的 `custom` 分区，主要用于配置 `appkey` 等运行参数。

> 如果 appkey 直接硬编码在固件中，可不使用此配置文件。详细说明见第 [6 节](#6-本地配置文件configbin)。

### 2.3 蓝牙配网二进制文件

路径：`third_party/blufi_app/`

用于蓝牙配网（BluFi）和小程序操作的独立可执行文件，烧入设备的 `blufi` 分区。示例仓库当前预置 ESP32-S3 产物；若目标工程自行编译 C3 版本，可按 `blufi_app_c3.bin` 命名后由 `config.py --target esp32-c3 --blufi` 烧录。

> **注意**：部分特殊版本或带 display 的硬件可能无法正常使用，遇到兼容问题请联系技术支持。

---

## 3. 分区配置

示例工程使用 `partitions/v2/` 下的分区表，针对不同芯片型号分别提供。

### 3.1 ESP32-S3（16MB Flash）

文件：`partitions/v2/16m.csv`

| 分区名 | 类型 | 用途 | 偏移 | 大小 |
|--------|------|------|------|------|
| `nvs` | data/nvs | 系统 NVS 存储 | 0x9000 | 16KB |
| `otadata` | data/ota | OTA 状态数据 | 0xD000 | 8KB |
| `phy_init` | data/phy | PHY 初始化数据 | 0xF000 | 4KB |
| `custom` | data/spiffs | **存放 config.bin**（本地配置） | 0x10000 | 128KB |
| `ota_0` | app/ota_0 | 主应用程序 | 0x30000 | 4928KB |
| `blufi` | app/ota_1 | **存放 blufi_app.bin** | 0x520000 | 2944KB |
| `assets` | data/spiffs | 资源文件（音频、字体等） | 0x810000 | 8128KB |

### 3.2 ESP32-C3（16MB Flash）

文件：`partitions/v2/16m_c3.csv`

| 分区名 | 类型 | 用途 | 偏移 | 大小 |
|--------|------|------|------|------|
| `nvs` | data/nvs | 系统 NVS 存储 | 0x9000 | 16KB |
| `otadata` | data/ota | OTA 状态数据 | 0xD000 | 8KB |
| `phy_init` | data/phy | PHY 初始化数据 | 0xF000 | 4KB |
| `custom` | data/spiffs | **存放 config.bin** | 0x10000 | 64KB |
| `ota_0` | app/ota_0 | 主应用程序 | 0x20000 | 4000KB |
| `ota_1` | app/ota_1 | 备用 OTA | （接续） | 4000KB |
| `assets` | data/spiffs | 资源文件 | 0x800000 | 4000KB |

---

## 4. 代码集成

### 4.1 闹钟模块（`main/alarm/`）

| 文件 | 说明 |
|------|------|
| `alarm_manager.cc/h` | 闹钟调度与管理核心逻辑 |
| `ccronexpr.c/h` | Cron 表达式解析库（第三方，用于定时触发） |

当 `CONFIG_CONNECTION_TYPE_NERTC` 开启时，CMake 自动编译上述文件。如不需要闹钟功能，可从 CMakeLists.txt 中移除对应条目，并屏蔽 `application.cc` / `application_nertc.cc` 中相关调用。

### 4.2 Board 公共模块（`main/boards/common/`）

#### 4.2.1 `board.cc`

云信扩展了以下接口：

- **`Board::GetSystemInfoJson()`**：追加 OTA 鉴权流程，上报设备信息（型号、固件版本）及云音乐功能开关等字段。
- **`Board::GetBoardName()`**：返回设备型号字符串，用于 OTA 鉴权和系统信息上报。
- **`Board::StartBlufiMode()`**：启动蓝牙配网流程（BluFi），支持小程序操作设备入网。
- **`Board::StartBlufiOtaMode()`**：蓝牙配网模块内的 OTA 升级流程。

#### 4.2.2 `dual_network_board.cc`

添加了 `Board::GetBoardName()` 的实现，适用于同时支持 WiFi 和 4G 的双网络板型。非双网络板型可不添加。

#### 4.2.3 `wifi_board.cc`

新增进入蓝牙配网模式的入口逻辑，当用户触发配网操作时调用 `StartBlufiMode()`。

### 4.3 显示模块（`main/display/`）

#### 4.3.1 `main/display/display.h`

示例在 `main/protocols/nertc_protocol.cc` 中使用 `SetEmotionForce()` 强制更新表情。目标工程有两种接入方式：
- 保留该能力：在 `Display` 基类和本地 display 实现中增加 `SetEmotionForce()`。
- 不需要强制表情：将协议层调用改为已有的 `SetEmotion()`，并跳过 `display.h` 的接口扩展。

### 4.4 云音乐播放器（`main/music_player/`）

| 文件 | 说明 |
|------|------|
| `music_player.cc/h` | 云音乐播放器逻辑 |
| `mp3_online_player.cc/h` | MP3 在线流播放实现 |

本节只列出代码移植涉及的文件。云音乐的开通条件、OTA 字段和播放打断要求见[第 8.3 节](#83-云音乐播放)。

### 4.5 协议层（`main/protocols/`）

#### 4.5.1 `protocol.h`

云信扩展了 Protocol 基类接口，新增 `SetAISleep()`、`SendTTSText()` 等空实现，便于 `NeRtcProtocol` 在 NERtc 通道上实现这些扩展功能。

#### 4.5.2 `nertc_protocol.cc`

**核心文件**。封装了云信 AI 通道的完整通信逻辑，包括：
- NERtc 引擎的创建、初始化、加入房间、AI 启动与销毁流程。
- 音频帧推送（PCM / OPUS）与接收回调。
- 从 SPIFFS 分区读取 `config.json`，加载 appkey、音频参数，以及 `ext_net_handle` / `ext_osal_handle` 等本地开关。
- 服务器下发 JSON 消息的解析（如 `system.sleep`、`updateSongList`、`alarm`、`app` 等扩展指令）。

当前 SDK 推荐使用 device_id 激活鉴权方式：`nertc_sdk_configuration_init()` 会默认将 `force_unsafe_mode` 初始化为 `true`，`licence_cfg.license` 初始化为空；业务侧只需要设置 `app_key` 和 `device_id`。`device_id` 需要在后台按对应 `app_key` 激活，并绑定智能体，该激活绑定用于替代旧版 demo 中的 license 鉴权。旧用户升级时不需要再在 `config.json` 中配置 `license_config.license`。

当 `licence_cfg.license` 保持默认空值时，SDK 会跳过本地 license 解码和验签流程，并把当前 `device_id` 作为后续服务鉴权身份。此模式要求设备已经在云信后台完成激活和智能体绑定；如果后台未激活或绑定关系不正确，SDK 初始化本身可能成功，但后续 AI 服务启动、入会或信令交互会在服务端鉴权阶段失败。

```cpp
nertc_sdk_configuration_t sdk_config = { 0 };
nertc_sdk_configuration_init(&sdk_config);
sdk_config.app_key = local_config_appkey_.c_str();
sdk_config.device_id = device_id.c_str();

nertc_sdk_engine_t engine = nertc_create_engine_with_config(&sdk_config);
```

如果存量产品仍需沿用旧版 license 鉴权，可以继续给 `sdk_config.licence_cfg.license` 传入旧 license 字符串。此时 SDK 会执行本地 license 解码、签名校验和有效期检查，并继续使用 license 中的 `licenseKey` 参与后续鉴权。旧 license 模式下仍建议同时设置 `app_key` 和稳定的 `device_id`，但不要再把空字符串当作有效 license 传入；空值会进入前述 device_id 激活模式。

`nertc_join` 接口仍保留 `token` 参数以兼容 SDK API，但在当前推荐模式下默认不需要 token。ESP32 demo 传入 `nullptr`：

```cpp
nertc_join(engine_, cname_.c_str(), nullptr, uid);
```

当前 demo 在调用 `nertc_init_engine` 前，会先读取 `config.json` 中的 `ext_net_handle` 与 `ext_osal_handle`：

```cpp
if (use_ext_net_handle) {
    engine_config.ext_net_handle = NeRtcExternalNetwork::GetInstance()->GetHandle();
} else {
    engine_config.ext_net_handle = nullptr;
}

if (use_ext_osal_handle) {
    engine_config.ext_osal_handle = NeRtcExternalOsal::GetInstance()->GetHandle();
} else {
    engine_config.ext_osal_handle = nullptr;
}
```

也就是说，示例工程中是否启用外部网络 / 外部 OSAL，不再由示例代码写死，而是由 `create_local_config/config.json.s3`（或 `config.json.c3`）里的布尔开关决定。

```cpp
engine_config.engine_mode = lite_mode_ ? NERTC_SDK_ENGINE_MODE_LITE : NERTC_SDK_ENGINE_MODE_NORMAL;
```
engine_mode用于指定 SDK 引擎的模式，可选值：NERTC_SDK_ENGINE_MODE_LITE, NERTC_SDK_ENGINE_MODE_NORMAL
NERTC_SDK_ENGINE_MODE_NORMAL 模式下，SDK 具备 AI 对话与音视频通话的全部能力，能实现打电话及 RTC 通话。
NERTC_SDK_ENGINE_MODE_LITE 模式下，需要在协议初始化前使用云信 OTA 返回的 `mqtt` 参数完成相关初始化，主要用于低延迟 AI 对话场景，不支持打电话及 RTC 通话。

#### 4.5.3 `nertc_external_network.cc`

`NeRtcExternalNetwork` 是外部网络抽象层，封装了 HTTP、TCP、UDP、MQTT 的具体实现，并通过函数指针表（`ext_net_handle`）暴露给 NERtc SDK。SDK 收到该 handle 后，会将所有网络 I/O 委托给应用侧提供的实现，**完全绕过 SDK 内置的网络栈**。

**需要或建议设置 `ext_net_handle` 的场景：**

| 场景 | 说明 |
|------|------|
| 4G 模块（ML307/EC801E 等） | SDK 内置网络栈依赖 lwIP socket API，4G 模块有独立的网络接口，必须通过外部 handle 桥接。参考实现见 `components/esp-ml307`（详见[第 8.5 节](#85-4g-模组接入esp-ml307)） |
| **ESP32-P4 + ESP-Hosted 仆从芯片** | **P4 无内置 WiFi**，通过 SPI/SDIO 连接仆从 ESP32 提供网络。当前示例建议显式设置外部 handle，让 SDK 使用应用侧同一工程编译的网络实现；底层仍可复用 esp-hosted host 注册后的 lwIP socket。仆从芯片固件参考 https://github.com/espressif/esp-hosted（详见[第 8.6 节](#86-esp32-p4--esp-hosted-仆从芯片网络接入)） |
| **IDF 版本不一致** | SDK 预编译库的 IDF 版本与用户工程不同时，SDK 内置网络栈可能存在 ABI 不兼容（如 `esp_http_client`、socket 结构体布局变化），导致运行时崩溃或连接异常。此时需要设置 `ext_net_handle`，让 SDK 调用用户工程编译的网络实现 |

**WiFi + IDF 版本一致**时，`ext_net_handle` 可设为 `nullptr`，SDK 使用内置网络栈。当前 `create_local_config/config.json.s3` 的默认值也是：

```json
"ext_net_handle": false
```

**在本示例中的设置方式**（由 `config.json` 开关控制，在 `nertc_protocol.cc` 的引擎初始化处生效）：

```cpp
if (use_ext_net_handle) {
    engine_config.ext_net_handle = NeRtcExternalNetwork::GetInstance()->GetHandle();
} else {
    engine_config.ext_net_handle = nullptr;
}
```

> **配置建议**：若使用 4G、ESP-Hosted，或 WiFi 方案下出现 SDK 连接失败、崩溃在网络相关调用栈中，可将 `config.json` 中的 `ext_net_handle` 改为 `true`，让 SDK 强制走应用侧编译的网络实现。

#### 4.5.4 `nertc_external_osal.cc`

`NeRtcExternalOsal` 是外部 OSAL 抽象层，通过 `ext_osal_handle` 向 NERtc SDK 提供线程、定时器、睡眠、时间、日志、互斥锁、条件变量等系统能力。

当前示例中的 `nertc_external_osal.cc` 主要基于 ESP-IDF / FreeRTOS 适配了以下能力：

- 线程创建与销毁：`xTaskCreate` / `xTaskCreatePinnedToCoreWithCaps`
- 定时器：`esp_timer`
- 睡眠与时间：`vTaskDelay`、`esp_timer_get_time`
- 日志：转发到 `ESP_LOGx`
- 同步原语：`Semaphore` 形式的 mutex / condition variable

`create_local_config/config.json.s3` 默认启用了该开关：

```json
"ext_osal_handle": true
```

在本示例中的设置方式如下：

```cpp
if (use_ext_osal_handle) {
    engine_config.ext_osal_handle = NeRtcExternalOsal::GetInstance()->GetHandle();
} else {
    engine_config.ext_osal_handle = nullptr;
}
```

> **配置建议**：当前 ESP32 示例默认建议保持 `ext_osal_handle = true`。如果手动关闭，SDK 会退回内部 OSAL 路径；仅在你已经确认目标构建不依赖外部 OSAL 适配时再关闭。

### 4.6 Application 层

#### 4.6.1 `application.h`

云信扩展需要在头文件中添加：
- 包含 `music_player/music_player.h`、`alarm/alarm_manager.h` 等模块头文件。
- `AecMode` 枚举新增 `kAecOnNertc` 类型（对应云信服务器端 AEC）。
- `CONFIG_CONNECTION_TYPE_NERTC` 宏保护下的成员变量和接口声明。

#### 4.6.2 `application.cc`

主要集成点，涉及以下扩展逻辑（均通过编译宏隔离）：

| 宏 | 扩展内容 |
|----|----------|
| `CONFIG_CONNECTION_TYPE_NERTC` | 创建 `NeRtcProtocol` 实例，初始化云信通道 |
| `CONFIG_USE_MUSIC_PLAYER` | 初始化云音乐播放器，处理 `updateSongList` 指令 |
| `CONFIG_USE_NERTC_PTT_MODE` | 启用按键对讲（PTT）模式逻辑 |

其他关键扩展：
- `protocol_->OnAudioChannelClosed`：关闭音频通道后异步设置表情为 `sleepy`。
- `protocol_->OnIncomingJson`：解析 `system->sleep`、`app`、`alarm` 等服务器下发的扩展消息。
- `ai_sleep_`、`current_pedding_speaking_`、`ResetDecoder()` 等状态和播放链路处理，用于对齐 AI 休眠、TTS 起播和用户打断行为。
- 云音乐、PTT、Server AEC 均为条件能力：未开启对应 Kconfig 或 `config.bin` 配置时，不需要合入对应分支。

#### 4.6.3 `application_nertc.cc`

云信功能在 Application 中的具体实现文件。包含 NERtc 协议初始化、设备信息上报、OTA 同步时的云信扩展逻辑等，与 `application.cc` 解耦，便于维护。

使用内置唤醒词或不使用唤醒词时，无需修改此文件。

### 4.7 main/CMakeLists.txt

以下是 NERtc 集成相关的 CMake 配置要点：

```cmake
# 1. 当 CONFIG_CONNECTION_TYPE_NERTC 开启时，自动添加 NERtc 相关源文件
if(CONFIG_CONNECTION_TYPE_NERTC)
    list(APPEND INCLUDE_DIRS "alarm")
    list(APPEND SOURCES "application_nertc.cc")
    list(APPEND SOURCES "protocols/nertc_protocol.cc")
    list(APPEND SOURCES "protocols/nertc_external_network.cc")
    list(APPEND SOURCES "alarm/alarm_manager.cc")
    list(APPEND SOURCES "alarm/ccronexpr.c")
endif()

# 2. 云音乐播放器（可选）
if(CONFIG_USE_MUSIC_PLAYER)
    list(APPEND INCLUDE_DIRS "music_player")
    list(APPEND SOURCES "music_player/mp3_online_player.cc")
    list(APPEND SOURCES "music_player/music_player.cc")
endif()

# 3. PRIV_REQUIRES 中添加 NERtc 库
idf_component_register(SRCS ${SOURCES}
    ...
    PRIV_REQUIRES
        ...
        nertc_sdk
        nertc_wake_up
        ...
)
```

### 4.8 main/idf_component.yml

`nertc_sdk`、`nertc_wake_up` 均作为私有组件存在于工程本地，无需在 `idf_component.yml` 中声明。该文件主要管理来自 ESP Component Registry 的第三方依赖，IDF 5.4 / 5.5 版本差异部分已通过 `matches` 条件处理。

### 4.9 main/Kconfig.projbuild

见下节 [Kconfig 配置项说明](#5-kconfig-配置项说明)。

### 4.10 main/mcp_server.cc

云信相关 MCP 扩展可以按目标产品能力选择接入，不要求所有工具一次性合入：
- **`self.good_bye`** MCP Tool：让设备主动退出 AI 对话。
- **`self.photo_explain`** MCP Tool：触发摄像头拍照并上传识别。
- **`self.cancel_alarm`** MCP Tool：取消正在响铃的闹钟。
- **服务器动态禁用 Tool**：解析 `capabilities.disableTools` 字段，配合 `IsToolDisabled()` 接口，支持服务器动态下发不支持的 Tool 列表。
- **`GetToolsList()`**：`max_payload_size` 扩大至 `8000`，避免 Tool 列表过长时被截断。

如果目标工程不需要对应能力，可以不注册相关 MCP Tool；如果注册了 Tool，需要同时确认 Application 层已经实现对应处理逻辑。例如 `self.photo_explain` 不只是新增工具名，还需要目标板提供摄像头拍照、JPEG 获取和图片上行链路。

### 4.11 main/ota.cc

云信 OTA 扩展：
- **`Ota::GetCheckVersionUrl()`**：当 `CONFIG_CONNECTION_TYPE_NERTC` 开启时，正式服 OTA primary 优先使用 `CONFIG_OTA_URL`；仅当该配置为空时，才回退到源码内默认地址 `https://nrtc.netease.im/v1/ota`。正式服 IP fallback 的 URL 与 `Host` 仍使用本地固定配置。
- **OTA 返回解析**：从 OTA 响应中解析 `agent` 字段，用于更新智能体配置；同时解析 `mqtt` 字段并写入本地 Settings，供后续协议初始化和 lite 模式直连 AI 服务使用；如需通过独立 BluFi App 执行 OTA，`firmware` 字段还必须包含 `version`、`url` 和 `md5`。
- **BluFi OTA 交接**：当 `CONFIG_CONNECTION_TYPE_NERTC` 开启、OTA 返回 `firmware.md5` 且分区表中存在 `blufi` 分区时，主固件调用 `Board::StartBlufiOtaMode(url, version, md5)`，由独立 BluFi App 下载、校验 MD5 并写回主固件分区。
- **`Ota::Upgrade()`**：使用 `GetNextSafePartition()` 替代原生的 `esp_ota_get_next_update_partition()`，确保在有 `blufi` 分区的情况下 OTA 不会写入独立 BluFi App 分区。

`agent` 字段中与 NERTC 扩展相关的内容需要继续传递给 Application 层：
- `agent.pipeline.interrupt_mode`：用于决定当前对话是否允许打断，并影响最终 AEC 模式。
- `agent.netease_cloud_music.support_music`：用于判断是否初始化云音乐播放器。
- `agent.netease_cloud_music.support_play_in_4g`：用于 4G 板型是否允许云音乐播放。
- `agent.device_sdk_config`：需要合并到 NERTC wakeup SDK 的 `custom_config.device_sdk_config`，用于在线唤醒配置。

相关字段默认值应保持保守，例如云音乐支持字段在解析前默认为 `false`，避免 OTA 失败或字段缺失时误初始化可选功能。

### 4.12 分区表文件

详见[第 3 节](#3-分区配置)。

---

## 5. Kconfig 配置项说明

在 `idf.py menuconfig` → **Xiaozhi Assistant** 菜单下可找到以下云信相关配置项。

### 5.1 核心开关

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `CONFIG_CONNECTION_TYPE_NERTC` | `y` | **NERtc 能力总开关**，开启后编译所有 NERtc 相关代码 |
| `CONFIG_OTA_URL` | `https://nrtc.netease.im/v1/ota` | OTA 检查地址，NERTC 正式服 OTA primary 优先使用该配置；为空时才回退到源码内默认云信 OTA 地址 |

### 5.2 音频模式

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `CONFIG_USE_NERTC_PTT_MODE` | `n` | 开启按键对讲模式（PTT），按住说话松开停止，与 Server AEC 互斥 |

> `server_aec` 已从 `menuconfig` 移至 `config.bin` 中的 `audio_config.server_aec` 字段配置。
>
> 三种 AEC 模式说明：
> - **服务器 AEC**（`audio_config.server_aec = true` 且 `lite_mode = false`）：推送 PCM 帧，服务端处理回声，需同时推送参考帧；与 PTT 模式互斥。当前代码会将 `server_aec && !lite_mode` 作为最终生效值。
> - **本地设备 AEC**（`CONFIG_USE_DEVICE_AEC`）：设备本地处理，推送 OPUS 编码帧。
> - **无 AEC**（`audio_config.server_aec = false` 且本地设备 AEC 关闭）：推送 OPUS 编码帧，无自动打断功能。

### 5.3 唤醒词

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `CONFIG_USE_CUSTOM_WAKE_WORD` | 视芯片而定 | 使用云信自定义唤醒词（Multinet 模型），需要 ESP32-S3/P4 + PSRAM |
| `CONFIG_SR_MN_CN_MULTINET7_QUANT` | S3 默认 y | ESP32-S3 自定义唤醒词使用的 `mn7_cn` 模型配置，见 `sdkconfig.defaults.esp32s3` |
| `CONFIG_SR_MN_CN_MULTINET7_AC_QUANT` | P4 默认 y | ESP32-P4 自定义唤醒词使用的 `mn7_cn` AC 模型配置，见 `sdkconfig.defaults.esp32p4` |

## 6. 本地配置文件（config.bin）

设备从 `custom` SPIFFS 分区读取 `config.json`，在运行时加载 appkey 等参数，无需重新编译固件即可更换 appkey。以下示例以 `create_local_config/config.json.s3` 为例。

### 6.1 config.json 格式说明

```json
{
    "appkey": "your_appkey_here",
    "audio_config": {
        "frame_size": 60,
        "afe_agc": {
            "enabled": false,
            "mode": 0,
            "compression_gain_db": 9,
            "target_level_dbfs": 3
        },
        "server_aec": false
    },
    "ext_net_handle": false,
    "ext_osal_handle": true,
    "lite_mode": true
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `appkey` | string | **必填**，云信控制台分配的 App Key |
| `audio_config.frame_size` | number | 音频帧时长（ms），推荐值 `20` 或 `60` |
| `audio_config.afe_agc.enabled` | boolean | 是否开启 AFE AGC，默认 `false` |
| `audio_config.afe_agc.mode` | number | AFE AGC 模式，当前示例默认 `0`，对应 `AFE_AGC_MODE_WEBRTC` |
| `audio_config.afe_agc.compression_gain_db` | number | AFE AGC 压缩增益，默认 `9` |
| `audio_config.afe_agc.target_level_dbfs` | number | AFE AGC 目标电平，默认 `3`，表示目标约为 `-3 dBFS` |
| `audio_config.server_aec` | boolean | 是否开启云信服务器端 AEC。该配置已从 `menuconfig` 移至 `config.bin`，默认 `false`；仅在 `lite_mode=false` 时生效 |
| `ext_net_handle` | boolean | 是否在 `nertc_init_engine` 前注入 `NeRtcExternalNetwork::GetInstance()->GetHandle()`。`config.json.s3` 默认值为 `false`；4G、ESP-Hosted、或 WiFi ABI 不兼容排查场景建议设为 `true` |
| `ext_osal_handle` | boolean | 是否在 `nertc_init_engine` 前注入 `NeRtcExternalOsal::GetInstance()->GetHandle()`。`config.json.s3` 默认值为 `true`；当前 ESP32 示例建议保持开启 |
| `lite_mode` | boolean | 是否开启 lite 模式。开启后设备不走 RTC 入会流程，而是基于 OTA 返回的 MQTT 参数直连 AI 服务，默认 `true` |
| `blufi_wifi` | boolean | 是否在重新配网入口跳转到独立 BluFi App。为 `true` 时需要提前烧入 `blufi_app.bin`；缺失或为 `false` 时走目标工程原有配网兜底路径 |

> `audio_config.afe_agc` 主要用于使用 AFE 音频处理链路的板型做本地调参。仓库默认保持关闭；如需改善实际硬件的拾音效果，可在目标板上自行调试并重新生成、烧录 `config.bin`。
>
> 当前 demo 不再读取 `license_config.license`。老用户从旧版接入迁移时，推荐在后台按 `appkey` 激活设备 `device_id` 并绑定智能体，作为 `licence_cfg` 默认填空时的鉴权方式。如果仍要继续使用旧版 license，需要在业务代码中显式给 `sdk_config.licence_cfg.license` 赋值，示例默认配置文件不会自动读取该字段。
>
> 当 `lite_mode = true` 时，设备不走传统 RTC 入会流程，而是使用 OTA 返回的 MQTT 连接参数建立信令通道，再根据 AI 服务下发的信息建立 RTP 音频链路。
>
> 修改 `create_local_config/config.json.*` 后，只重新执行 `idf.py flash` 不会更新 `custom` 分区中的 `config.bin`。需要重新生成并烧录 `config.bin`，设备重启后才会读取到新的 appkey、`lite_mode`、`blufi_wifi` 等本地配置。

### 6.2 使用 config.py 生成并烧录 config.bin（推荐）

工程提供了 `config.py` 脚本，能自动读取分区表确定烧录地址和镜像大小，无需手动计算偏移量。

**Step 1：填入 appkey**

根据目标芯片编辑对应的模板文件：

- **ESP32-S3 / P4**：编辑 `create_local_config/config.json.s3`
- **ESP32-C3 及其他**：编辑 `create_local_config/config.json.c3`

将 `appkey` 字段替换为从云信控制台获取的真实 App Key。

如需调整音频参数或 SDK 外部适配开关，也可同时检查以下字段：

- `audio_config.afe_agc.enabled`：默认 `false`
- `audio_config.afe_agc.mode`：当前示例默认 `0`
- `audio_config.afe_agc.compression_gain_db`：当前示例默认 `9`
- `audio_config.afe_agc.target_level_dbfs`：当前示例默认 `3`
- `audio_config.server_aec`：默认 `false`

- `ext_net_handle`：`config.json.s3` 默认是 `false`
- `ext_osal_handle`：`config.json.s3` 默认是 `true`
- `lite_mode`：`config.json.s3` 默认是 `true`
- `blufi_wifi`：是否使用独立 BluFi App；S3 使用 `blufi_app.bin`，C3 可使用自行编译的 `blufi_app_c3.bin`

通常建议：

- WiFi + IDF 版本一致时，保持 `ext_net_handle = false`
- 4G、ESP-Hosted，或怀疑网络 ABI 不兼容时，改为 `ext_net_handle = true`
- `ext_osal_handle` 保持 `true`，与当前示例中的 FreeRTOS / `esp_timer` 适配保持一致
- `audio_config.afe_agc` 默认保持关闭；如需优化不同硬件板型的拾音表现，可在目标板上自行调参后重新烧录 `config.bin`
- 如需使用云信服务器端 AEC，请将 `audio_config.server_aec` 设为 `true`，并关闭 `lite_mode`
- 如需使用 lite 直连 AI 服务，请将 `lite_mode` 设为 `true`，并确保 OTA 返回中包含可用的 `mqtt` 参数
- 如需使用独立 BluFi App 配网或 BluFi OTA，请将 `blufi_wifi` 设为 `true`，并确保已烧入当前 target 对应的 BluFi App 固件

**Step 2：生成并烧录**

```bash
# 仅生成 config.bin，不烧录（用于检查生成结果）
python3 config.py --build --target esp32-s3

# ESP32-S3（默认 target，可省略 --target）
python3 config.py -p COM6 --build --flash --target esp32-s3

# ESP32-C3
python3 config.py -p COM6 --build --flash --target esp32-c3

# ESP32-P4（复用 config.json.s3 模板）
python3 config.py -p COM6 --build --flash --target esp32-p4

# ESP32-S3 同时烧录蓝牙配网固件（blufi）
python3 config.py -p COM6 --build --flash --blufi --target esp32-s3

# ESP32-C3 如已自行编译 third_party/blufi_app/bin/blufi_app_c3.bin，也可烧录
python3 config.py -p COM6 --build --flash --blufi --target esp32-c3
```

脚本执行流程：
1. 根据 `--target` 自动选择分区表（`partitions/v2/16m.csv` 或 `16m_c3.csv`）
2. 从分区表解析 `custom` 分区的偏移地址和大小
3. 将模板配置文件复制到 `local_config/config.json`
4. 调用 `create_local_config/spiffsgen.py` 生成 `config.bin`
5. 调用 esptool 将 `config.bin` 烧入 `custom` 分区
6. 若指定 `--blufi`，额外将 target 对应的 BluFi App 固件烧入 `0x520000`；S3 使用 `blufi_app.bin`，C3 使用 `blufi_app_c3.bin`

> **参数说明**：
> - `-p`：串口号（Windows 示例：`COM6`；Linux 示例：`/dev/ttyUSB0`）
> - `--build`：生成 config.bin
> - `--flash`：烧录 config.bin（需 `-p`）
> - `--blufi`：同时烧录蓝牙配网固件（需 `-p`）；C3 需要先自行编译并放置 `third_party/blufi_app/bin/blufi_app_c3.bin`
> - `--target`：目标芯片，默认 `esp32-s3`，可选 `esp32-c3`、`esp32-p4`
> - `-o`：自定义输出文件路径，默认 `config.bin`
> - `-i`：自定义输入目录，默认自动从 `create_local_config/` 复制模板

---

## 7. 构建与烧录

### 7.1 固件构建与烧录

```bash
cd demo/esp32

# 配置目标芯片（idf.py 使用下划线格式，如 esp32s3）
idf.py set-target esp32s3

# 打开菜单配置
idf.py menuconfig

# 构建
idf.py build

# 烧录并监视日志
idf.py -p /dev/ttyUSB0 flash monitor
```

> 如果分区表包含独立 `blufi` app 分区，构建时可能出现主固件大于 `blufi` 分区的 warning。该提示表示主固件不能写入 `blufi` 分区，不代表 `ota_0` 主固件烧录失败。NERTC 示例使用 `GetNextSafePartition()` 避免主固件 OTA 写入 `blufi`；如果目标工程保留独立 BluFi App，也需要确认 OTA 逻辑不会选择 `blufi` 作为主固件升级分区。
>
> OTA 验证时可关注日志：升级选择分区应打印 `Writing to partition ota_0 ...` 或其他主固件 OTA 分区；如果轮到 `blufi`，应出现 `ota_2(blufi) skipped, try next...`。若最终打印 `No available OTA slot`，应先调整分区表，不要让主固件写入 `blufi` 分区。

### 7.2 本地配置与蓝牙固件烧录

`config.bin` 和 BluFi 固件不属于主固件 `idf.py flash` 的默认烧录范围，统一使用 `config.py` 处理，具体命令见[第 6.2 节](#62-使用-configpy-生成并烧录-configbin推荐)。如果修改了 `create_local_config/config.json.*`、`lite_mode`、`blufi_wifi` 或 `appkey`，需要重新生成并烧录 `config.bin`。

---

## 8. 进阶功能

### 8.1 自定义唤醒词

自定义唤醒词基于 ESP-SR Multinet 模型，支持在 ESP32-S3 / P4 + PSRAM 硬件上运行。

**配置步骤**：

1. 在 `menuconfig` 中选择 **Wake Word Implementation Type** → `Multinet model (Custom Wake Word)`。
2. 在 `menuconfig` 中选择 **ESP Speech Recognition** → `Chinese Speech Commands Model（general chinese recognition (mn7_cn)）`。
3. 若 `assets.bin` 是自定义生成的，请确认其中包含 `mn7_cn` 模型。当前小智提供的 `xiaozhi-assets-generator` 网页工具不会默认选中该模型，如需生成对应资源，可参考我们 fork 的项目：https://github.com/netease-im/xiaozhi-assets-generator。

> 若沿用当前 demo 默认配置与资源，通常无需额外调整唤醒词模型。

**接入检查点**：
- 音频输入按 `wake_word_->GetFeedSize()` 喂给唤醒词，不要固定为 160 samples；`NertcAfeWakeWord::Feed()` 内部再调用 `nertc_wakeup_feed()` 和 `nertc_wakeup_detect()`。
- S3/P4 上 `AudioService::SetModelsList()` 会优先检查 MultiNet 模型并创建 `NertcAfeWakeWord`；`assets.bin` 缺少 `mn7_cn` 时无法创建自定义唤醒词。
- 如果启用了 listening/speaking 状态下的唤醒检测，`AudioService::IsAfeWakeWord()` 需要把 NERTC wakeup 也按 AFE 类唤醒词处理。
- 传给 NERTC wakeup SDK 的 `appkey`、`deviceId`、`custom_config` 字符串需要在 SDK 使用期间保持有效。

`create_local_config/config.json.*` 不需要预置顶层 `wake_words` 字段。唤醒词在线配置来自 OTA 返回的 `agent.device_sdk_config`，示例会将其合并到传给 wakeup SDK 的 `custom_config.device_sdk_config`。排查时重点看 assets 是否加载模型、是否出现 `wake word custom_config=...` / `NertcAfeWakeWord Start` 日志，以及 OTA 是否通过 `awakensEnable=false` 禁用了唤醒词创建。

### 8.2 蓝牙配网（BluFi）

蓝牙配网依赖独立固件文件，烧入 `blufi` 分区（偏移 `0x520000`）：
- **ESP32-S3**：`third_party/blufi_app/bin/blufi_app.bin`
- **ESP32-C3**：`third_party/blufi_app/bin/blufi_app_c3.bin`（目标工程自行编译后放置）

运行时是否跳转独立 BluFi App 由 `config.json` 中的 `blufi_wifi` 字段控制。`blufi_wifi=true` 时，重新配网入口会调用 `Board::StartBlufiMode()` 跳转到 `blufi` 分区；缺失或为 `false` 时应保留目标工程原有配网流程作为兜底。

当目标工程保留 `blufi` 独立 App 分区时，NERTC OTA 后台需要在固件响应中同时返回 `version`、`url` 和 `md5`：

```json
{
  "firmware": {
    "version": "1.2.7",
    "url": "https://example.com/firmware.bin",
    "md5": "32-char-lowercase-md5",
    "force": 1
  }
}
```

主固件检测到新版本后会把 `ota_v`、`ota_url`、`ota_md5` 写入 `board` 命名空间 NVS，并切换到 `blufi` 分区；BluFi App 进入 OTA 模式后下载目标固件，按 `ota_md5` 校验后写回主固件分区。

配置与 BluFi 固件可以在生成 `config.bin` 时通过 `--blufi` 一并烧录，命令见[第 6.2 节](#62-使用-configpy-生成并烧录-configbin推荐)。若只需单独烧录蓝牙固件（不更新 config.bin）：

```bash
python3 config.py -p COM6 --blufi --target esp32-s3
```

> **注意**：部分特殊版本或带 display 的硬件可能存在兼容问题，遇到问题请联系技术支持。

### 8.3 云音乐播放

云音乐功能需要同时满足三类条件：后台智能体已配置并启用云音乐 MCP 服务；`menuconfig` 开启 `CONFIG_USE_MUSIC_PLAYER`（仅 ESP32-S3/P4 + PSRAM）；OTA 返回允许该设备使用云音乐。示例中的初始化条件如下，4G 板型需要额外允许蜂窝网络播放：

```cpp
ota_->GetSupportAirMusicPlayer() &&
(Board::GetInstance().GetBoardType() != "ml307" || ota_->GetSupportAirMusicIn4G())
```

对应 OTA 字段为 `agent.netease_cloud_music.support_music` 和 `agent.netease_cloud_music.support_play_in_4g`。如果 OTA 请求失败、字段缺失或字段不是 bool，示例保持默认 `false`，不会初始化云音乐播放器。

服务器通过 `updateSongList` JSON 指令下发歌单，设备收到后更新本地播放列表并开始播放。参考接入官方文档：https://doc.yunxin.163.com/emotional-ai/guide/jE5NDExNzc?platform=client，注意当前版本已经废弃 `music_player_api.h` 和 `music_player_api.c` 文件。

云音乐播放中需要能被用户交互打断。示例在按键进入对话和唤醒词命中入口都会调用 `MusicPlayer::GetInstance().InterruptPlay()`；目标工程如果改写了按键或唤醒入口，需要保留等价打断逻辑。当前示例还包含连续 MP3 解码失败保护，并将流式播放相关线程栈提高到 `16KB`；裁剪或移植播放器逻辑时建议保留等价保护。

### 8.4 拍照识别

拍照识别依赖 `self.photo_explain` MCP Tool、Application 图片上行逻辑和板级 Camera 实现共同完成。只注册 MCP Tool 不足以让该能力工作。

参考流程：

1. `self.photo_explain` 触发 `Application::PhotoExplain()`。
2. 本地摄像头路径会调用 `Camera::Capture()`，再通过 `Camera::GetCapturedJpeg(uint8_t*& data, size_t& len)` 取得 JPEG 数据。
3. Application 将 JPEG 编码成 `data:image/jpeg;base64,...`，再调用 `protocol_->SendLlmImage(..., img_type=0)` 发送给 NERTC AI。
4. 网络图片路径调用 `SendLlmImage(..., img_type=1)`，用于 URL 图片示例，不代表本地摄像头拍照。

如果目标板使用 `esp32-camera` 旧驱动，可参考示例中的 `Esp32CameraLegacy` 封装；如果目标工程已有自己的 `Camera` 实现，需要确认它已经实现 `GetCapturedJpeg()`。接入后还需要关注 PSRAM 占用，JPEG buffer 和 base64 buffer 会额外占用内存，必要时降低分辨率或 JPEG 质量。

### 8.5 4G 模组接入（esp-ml307）

#### 8.5.1 概述

`components/esp-ml307` 是本项目独立维护的 4G 模组网络库，支持通过 AT 命令驱动以下模组：

| 模组 | 说明 |
|------|------|
| ML307R / ML307A | 移远 Cat.1 模组（主要支持目标） |
| EC801E | 移远 Cat.1 模组，需确认固件是否支持 SSL TCP |
| NT26K | 需确认固件是否支持 SSL TCP |

提供的协议支持：HTTP/HTTPS、MQTT/MQTTS、TCP/SSL TCP、UDP、WebSocket。

#### 8.5.2 两种引入方式

**方式一：使用本项目本地维护版本（推荐）**

直接使用工程中的 `components/esp-ml307`，已在本地针对实际使用场景做了若干稳定性修复（见下节）。

**方式二：通过 ESP Component Registry 引入上游版本**

`main/idf_component.yml` 中预留了上游组件的注释入口，如需使用可取消注释：

```yaml
# main/idf_component.yml
dependencies:
  # 78/esp-ml307: ~3.6.4   ← 取消注释即可通过 idf.py update-dependencies 拉取
```

> 上游版本（`78/esp-ml307`）是社区公开版本，本项目本地修复不一定已合并回上游，使用上游版本时需自行评估稳定性。

#### 8.5.3 使用建议

> **用户有自己稳定的网络库版本时，应优先使用自己工程的网络库**，无需引入 `components/esp-ml307`。

`components/esp-ml307` 主要作为**参考实现**，适用于以下场景：

| 场景 | 说明 |
|------|------|
| 自研网络库遇到问题 | 可对照 `components/esp-ml307` 的 AT 命令交互逻辑排查 |
| 接入新模组或新功能 | 参考已有模组的实现模式（ML307/EC801E 并列实现） |
| 还没有自己的网络库 | 可直接使用 `components/esp-ml307` 作为起点，按需修改 |

#### 8.5.4 本地版本的重要修复

本地维护版本相对上游做了以下关键修复，自研网络库遇到类似问题可参考对应文件：

| 修复内容 | 涉及文件 |
|----------|----------|
| HTTP 死锁：`OnTcpData` 在 `write_cv_` 等待前持有 `mutex_`，TCP 断开路径同时尝试加锁，触发看门狗 | `src/http_client.cc` |
| `cv_` 在 `Read()` 和 `ReadAll()` 中使用了不同的锁，导致潜在的等待无法被唤醒 | `src/http_client.cc` |
| TCP 断开回调重入及 use-after-free：用原子 `callback_called_` 保证回调只触发一次；`HttpClient::Close()` 始终调用 `Disconnect()` 等待 `ReceiveTask` 退出；重新连接前清空旧连接的回调 | `src/esp/esp_tcp.cc`、`src/http_client.cc` |
| ML307 HTTP 连接关闭时 connect ID 未重置，影响连接复用 | `src/ml307/ml307_http.cc` |

#### 8.5.5 与 NERtc 的衔接

使用 4G 模组时，需通过 `nertc_external_network.cc` 将 `esp-ml307` 的网络能力桥接给 NERtc SDK（详见[第 4.5.3 节](#453-nertc_external_networkcc)）。`components/esp-ml307` 中的 `NetworkInterface` 抽象基类与 `NeRtcExternalNetwork` 的外部 handle 设计一致，可直接对接。对当前示例工程而言，推荐直接在 `config.json` 中将 `ext_net_handle` 设为 `true`。

```cpp
// nertc_protocol.cc 引擎初始化处（4G 模组方案）
engine_config.ext_net_handle = NeRtcExternalNetwork::GetInstance()->GetHandle();
```

`NeRtcExternalNetwork` 内部的 HTTP、TCP、UDP、MQTT 实现，可参照 `components/esp-ml307/src/ml307/` 下对应文件（`ml307_http.cc`、`ml307_tcp.cc` 等）进行适配。

### 8.6 ESP32-P4 + ESP-Hosted 仆从芯片网络接入

#### 8.6.1 适用场景

**ESP32-P4 无内置 WiFi/BT**，需要外挂一颗 ESP32 模组（如 ESP32-C6、ESP32-S3）作为仆从芯片（Slave）提供 WiFi 网络能力。仆从芯片通过 SPI 或 SDIO 与 P4 通信，运行 esp-hosted slave 固件后，P4 侧通过 esp-hosted host 驱动访问网络。

> 与 4G 模组方案的区别：同为"外部网络"，4G 模组走 AT 命令 + 蜂窝网，esp-hosted 走 SPI/SDIO + WiFi（仆从 ESP32）。NERtc 侧的桥接方式完全相同，均通过 `ext_net_handle`。

#### 8.6.2 仆从芯片固件

仆从 ESP32 需烧录 esp-hosted slave 固件，固件源码及烧录说明见官方仓库：

- 仓库地址：https://github.com/espressif/esp-hosted
- 路径：`esp_hosted_fg/esp/esp_driver/`（FreeRTOS 版）或 `esp_hosted_ng/esp/`（Linux NG 版，推荐用 FreeRTOS 版）
- 支持的传输接口：**SPI**（推荐）、SDIO、UART

选择与 P4 硬件连接方式对应的 slave 目标进行编译烧录，例如：

```bash
cd esp-hosted/esp_hosted_fg/esp/esp_driver
idf.py set-target esp32c6
idf.py -DCONFIG_ESP_SPI_HOST_INTERFACE=y build flash
```

#### 8.6.3 主控（P4）侧适配

P4 侧需在工程中集成 esp-hosted host 驱动，并将其网络能力桥接给 NERtc SDK，方式与 4G 模组完全一致。对当前示例工程而言，推荐直接在 `config.json` 中将 `ext_net_handle` 设为 `true`：

```cpp
// nertc_protocol.cc 引擎初始化处（P4 + esp-hosted 方案）
engine_config.ext_net_handle = NeRtcExternalNetwork::GetInstance()->GetHandle();
```

`nertc_external_network.cc` 中 HTTP、TCP、UDP、MQTT 的底层实现需要走 P4 工程可用的网络接口。esp-hosted host 驱动会在 P4 上注册标准 netif，因此通常可以继续使用 lwIP socket 实现，不需要像 4G AT 模组那样重写一套 HTTP/TCP/UDP/MQTT。

> **与 4G 模组的关键区别**：4G 模组的 AT 命令接口无法直接使用 lwIP socket，因此 `esp-ml307` 需要完整实现 HTTP/TCP/UDP/MQTT；esp-hosted host 则提供标准 netif，外部网络 handle 的实现可复用 lwIP socket 路径。设置 `ext_net_handle=true` 的重点不是更换网络协议，而是避免 SDK 预编译内置网络栈与 P4 工程运行环境不一致。

#### 8.6.4 与其他方案对比

| 维度 | WiFi（S3 内置） | 4G 模组（ML307） | ESP-Hosted（P4 专用） |
|------|----------------|-----------------|----------------------|
| 典型主控 | ESP32-S3 | ESP32-S3 / C3 | **ESP32-P4** |
| 网络介质 | 内置 WiFi | 蜂窝 Cat.1 | WiFi（仆从 ESP32） |
| 主控↔网络接口 | 内置 | UART AT 命令 | SPI / SDIO |
| 仆从固件 | 无 | 模组出厂固件 | esp-hosted slave |
| NERtc 桥接 | 可不设置 handle | 必须设置 handle | 建议设置 handle；实现通常可复用 lwIP socket |
| 参考实现 | — | `components/esp-ml307` | https://github.com/espressif/esp-hosted |

## 附录：常见问题

**Q：IDF 5.5 编译报 `touch_element` 相关错误？**
A：先确认 `main/idf_component.yml` 的 IDF 版本条件与当前工程匹配，并运行 `idf.py update-dependencies`。如果目标板自己的代码直接包含 `touch_element/touch_button.h`，还需要按目标 IDF 版本评估板级触摸实现是否要替换或条件编译。

**Q：OTA 升级后设备进入 blufi 分区而非主程序？**
A：检查 `ota.cc` 是否使用 `GetNextSafePartition()`，并确认 OTA 日志写入的是 `ota_0` 或其他主固件 OTA 分区。`blufi` 只用于独立 BluFi App，不应承载主固件 OTA；分区和日志检查点见[第 7.1 节](#71-固件构建与烧录)。

**Q：config.bin 烧录后 appkey 未生效？**
A：检查 `config.json` 格式是否合法（标准 JSON，无注释），以及烧录地址是否与分区表中 `custom` 分区的 Offset 一致。可通过串口日志中的 `local config set appkey to ...` 确认是否读取成功。

**Q：`license_config.license` 为空时 SDK 会怎么鉴权？**
A：当前 SDK 初始化默认让 `licence_cfg.license` 为空，并设置 `force_unsafe_mode=true`。SDK 检测到 license 为空后会跳过本地 license 校验，用 `device_id` 作为后续服务鉴权身份。此时必须保证该 `device_id` 已在云信后台按同一个 `appkey` 激活并绑定智能体；否则可能出现 SDK 创建成功但 AI 服务启动或信令阶段鉴权失败。

**Q：存量设备还想继续使用旧版 license，应该怎么做？**
A：可以继续使用，但需要业务代码显式设置 `sdk_config.licence_cfg.license = "<old_license>"`，不要依赖当前 demo 的 `config.json` 自动读取。传入非空 license 后，SDK 会走旧版本地 license 解码、签名校验和有效期检查，并使用 license 中的 `licenseKey` 参与后续鉴权。迁移期间建议二选一：要么改为 device_id 后台激活模式并保持 license 为空，要么明确传入旧 license，不要把空字符串误当成旧 license。

**Q：自定义唤醒词不生效但编译通过？**
A：先确认 `sdkconfig.defaults.esp32s3` / `sdkconfig.defaults.esp32p4` 中启用了对应 MultiNet7 配置，并确认 `assets.bin` 包含模型资源。运行时重点看 `NertcAfeWakeWord Start`、`wake word custom_config=...` 以及 OTA 是否下发 `agent.device_sdk_config.awakensEnable=false`；更多检查点见[第 8.1 节](#81-自定义唤醒词)。

**Q：Server AEC 和本地 AEC 能同时开启吗？**
A：不能。本地 AEC 由 `CONFIG_USE_DEVICE_AEC` 控制，Server AEC 由 `config.bin` 中的 `audio_config.server_aec` 控制；两者应只选择一种。PTT 模式开启时不能使用 AEC；`lite_mode=true` 时 `audio_config.server_aec` 不生效。

**Q：WiFi 方案下 SDK 连接失败或崩溃在网络调用栈中？**
A：大概率是 SDK 预编译库的 IDF 版本与用户工程不一致，导致内置网络栈 ABI 不兼容。解决方法：将 `config.json` 中的 `ext_net_handle` 改为 `true`，让 SDK 使用应用侧编译的网络实现：

```json
"ext_net_handle": true
```

详见[第 4.5.3 节](#453-nertc_external_networkcc)。
