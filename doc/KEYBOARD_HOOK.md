# Keyboard Hook Integration

## 概述

本项目已集成 libuiohook 库，实现了当鼠标焦点在 SDL 窗体内时自动拦截所有键盘输入，鼠标移出 SDL 窗体后自动释放键盘输入的功能。

## 功能特性

### 自动拦截机制

- **鼠标进入 SDL 窗口**: 当鼠标光标移动到 SDL 窗口内部且窗口获得焦点时，键盘钩子自动激活，所有键盘输入将被拦截
- **鼠标离开 SDL 窗口**: 当鼠标光标移出 SDL 窗口边界或窗口失去焦点时，键盘钩子自动释放，键盘输入恢复正常

### 实现原理

1. **libuiohook 集成**: 使用 libuiohook 提供跨平台的低级键盘钩子功能
2. **鼠标追踪**: 通过 SDL3 事件系统实时追踪鼠标位置和窗口焦点状态
3. **动态切换**: 根据鼠标位置和窗口焦点状态动态启用/禁用键盘拦截

## 架构设计

### 核心组件

#### KeyboardHookManager (`src/sdl_renderer/keyboard_hook.h/cpp`)

键盘钩子管理器，封装了 libuiohook 的功能：

- `Initialize()`: 初始化键盘钩子
- `Shutdown()`: 关闭键盘钩子
- `EnableInterception()`: 启用键盘拦截
- `DisableInterception()`: 禁用键盘拦截
- `UpdateMouseTracking()`: 更新鼠标追踪状态，自动判断是否需要拦截

#### SDLRenderer 集成

在 `SDLRenderer` 类中集成了 `KeyboardHookManager`：

- 构造函数中创建键盘钩子管理器
- 初始化时调用 `Initialize()` 并设置 SDL 窗口指针
- 析构函数中调用 `Shutdown()` 清理资源
- 在事件循环中的 `PollEvent()` 方法中：
  - 监听 `SDL_EVENT_MOUSE_MOTION` 事件，实时更新鼠标追踪
  - 监听 `SDL_EVENT_WINDOW_FOCUS_GAINED/LOST` 事件，处理窗口焦点变化

### 拦截逻辑

```
鼠标在窗口内 && 窗口有焦点 → 启用键盘拦截
鼠标在窗口外 || 窗口失去焦点 → 禁用键盘拦截
```

## 编译配置

### CMakeLists.txt 修改

```cmake
# 添加 libuiohook 子目录
add_subdirectory(third_party/libuiohook)

# 添加键盘钩子源文件
target_sources(momo
  PRIVATE
    src/sdl_renderer/keyboard_hook.cpp
)

# 链接 libuiohook 库
target_link_libraries(momo
  PRIVATE
    uiohook
)
```

## 平台兼容性

libuiohook 支持以下平台：

- **Windows**: 使用 `SetWindowsHookEx` API
- **macOS**: 使用 `CGEventTap` API
- **Linux**: 使用 X11 `XRecord` 扩展

### 平台特定依赖

#### Linux

需要安装以下开发库：

```bash
sudo apt-get install libx11-dev libxtst-dev libxt-dev libxinerama-dev \
                     libx11-xcb-dev libxkbcommon-dev libxkbcommon-x11-dev \
                     libxkbfile-dev
```

#### macOS

需要启用辅助功能权限：

系统偏好设置 → 安全性与隐私 → 隐私 → 辅助功能 → 添加应用

#### Windows

无需额外依赖，但可能需要管理员权限以安装全局键盘钩子。

## 使用示例

键盘钩子在 `SDLRenderer` 初始化时自动启动，无需手动调用：

```cpp
// 创建 SDLRenderer 实例（自动初始化键盘钩子）
auto renderer = std::make_unique<SDLRenderer>(1280, 720, false);

// 键盘钩子会自动根据鼠标位置和窗口焦点状态工作
// 无需手动管理

// 析构时自动清理
renderer.reset();
```

## 调试信息

KeyboardHookManager 使用 libuiohook 的日志系统，日志会输出到：

- `LOG_LEVEL_INFO`: stdout
- `LOG_LEVEL_WARN`: stderr
- `LOG_LEVEL_ERROR`: stderr

可以通过修改 `LoggerProc` 函数来调整日志行为。

## 注意事项

1. **性能影响**: 键盘钩子是低级系统操作，会有轻微的性能开销，但在现代系统上几乎可以忽略不计

2. **安全性**: 某些操作系统（如 Windows）可能会提示用户授权键盘钩子权限

3. **线程安全**: `KeyboardHookManager` 使用原子变量确保线程安全，可以在多线程环境中使用

4. **事件消费**: 当前实现中，被拦截的键盘事件会被消费，不会传递给其他应用程序

5. **组合键**: 系统级组合键（如 Ctrl+Alt+Del）在某些平台上无法被拦截

## 扩展建议

如果需要进一步定制拦截行为，可以：

1. 修改 `KeyboardHookManager::HookEventProc` 中的拦截逻辑
2. 添加白名单/黑名单功能，选择性拦截特定按键
3. 添加热键功能，在拦截模式下触发特定操作
4. 实现按键记录和重放功能

## 故障排除

### 键盘拦截不工作

1. 检查 libuiohook 是否正确编译和链接
2. 确认平台特定依赖已安装（Linux）
3. 检查辅助功能权限（macOS）
4. 查看日志输出确认钩子是否成功初始化

### 键盘输入延迟

1. 确认 `UpdateMouseTracking()` 调用频率合理
2. 检查 `HookEventProc` 回调是否执行耗时操作
3. 考虑使用更高优先级的线程（Windows）

### 编译错误

1. 确认 `third_party/libuiohook` 目录存在且完整
2. 检查 CMakeLists.txt 配置是否正确
3. 确认平台特定的编译选项已设置

## 参考资料

- [libuiohook GitHub](https://github.com/kwhat/libuiohook)
- [SDL3 Documentation](https://wiki.libsdl.org/SDL3)
- [Windows SetWindowsHookEx](https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
- [macOS CGEventTap](https://developer.apple.com/documentation/coregraphics/cgeventtap)
- [X11 XRecord Extension](https://www.x.org/releases/X11R7.7/doc/libXtst/recordlib.html)

