# Summary of libuiohook Keyboard Hook Integration

## Project Goals

Integrate libuiohook into the momo project to automatically intercept all keyboard input when the mouse focus moves into the SDL window and automatically release it when the mouse moves away.

## Completed Work

### 1. Creating the Keyboard Hook Manager

#### File: `src/sdl_renderer/keyboard_hook.h`

Defines the `KeyboardHookManager` class, encapsulating the functionality of libuiohook:

- `Initialize()`: Initializes the keyboard hook and sets the callback function

- `Shutdown()`: Cleans up keyboard hook resources

- `EnableInterception()`: Enables keyboard interception

- `DisableInterception()`: Disables keyboard interception

- `SetSDLWindow()`: Sets the SDL window pointer

- `UpdateMouseTracking()`: Updates the mouse tracking state and automatically determines whether interception is needed

- `IsIntercepting()`: Checks if interception is currently in progress

#### File: `src/sdl_renderer/keyboard_hook.cpp`

Implements the core logic of the keyboard hook manager:

- **HookEventProc**: libuiohook Event callbacks determine whether to consume keyboard events based on interception flags.

- **LoggerProc**: libuiohook log callback, outputting debug information.

- **UpdateMouseTracking**: Intelligently tracks mouse position and window focus, automatically switching interception states.

### 2. Integration with SDLRenderer

#### Modification: `src/sdl_renderer/sdl_renderer.h`

- Added `keyboard_hook.h` header file reference

- Added `std::unique_ptr<sdl_hook::KeyboardHookManager> keyboard_hook_` member variable

#### Modification: `src/sdl_renderer/sdl_renderer.cpp`

- **Constructor**: Initializes the `keyboard_hook_` member, calling `Initialize()` and `SetSDLWindow()`.

- **Destructor**: Calls `Shutdown()`. Clean up keyboard hook resources

- **PollEvent()**:

- Listen for the `SDL_EVENT_MOUSE_MOTION` event to update mouse tracking in real time

- Listen for the `SDL_EVENT_WINDOW_FOCUS_GAINED/LOST` event to handle window focus changes

### 3. Update CMake Configuration

#### Modify: `CMakeLists.txt`

- Add libuiohook subdirectory: `add_subdirectory(third_party/libuiohook)`

- Add keyboard hook source files to the build target:

``cmake

target_sources(momo

PRIVATE

src/sdl_renderer/keyboard_hook.cpp

)

```

- Link libuiohook library: `target_link_libraries(momo PRIVATE uiohook)`

### 4. Documentation

#### Creation: `doc/KEYBOARD_HOOK.md`

Detailed keyboard hook integration documentation, including:

- Feature description

- Implementation principle

- Architecture design

- Compilation configuration

- Platform compatibility

- Usage examples

- Debugging information

- Notes

- Extension suggestions

- Troubleshooting

References

#### Update: `README.md`

An introduction to keyboard hook integration and documentation links have been added to the project's README.

## Technical Implementation Details

### Interception Logic

```
if (mouse inside window && window has focus) {

Enable keyboard interception

} else {

Disable keyboard interception

}
```

### Event Flow

1. The SDL event loop detects mouse movement or window focus change.

2. Call `keyboard_hook_->UpdateMouseTracking()`.

3. `UpdateMouseTracking()` calculates whether the mouse is inside the window and the window has focus.

4. Call `EnableInterception()` or `DisableInterception()` based on the state change.

5. The libuiohook's `HookEventProc` callback consumes or allows keyboard events based on the interception flag.

### Thread Safety

- Use `std::atomic<bool>` to ensure thread-safe access to the interception flag.

- The SDL event loop runs on the main thread.

- libuiohook callbacks may execute on different threads; state is synchronized using atomic variables.

## Platform Support

### Windows
- Using the `SetWindowsHookEx` API

- No additional dependencies

- May require administrator privileges

### macOS

- Using the `CGEventTap` API

- Requires accessibility permissions

- System Preferences → Security & Privacy → Privacy → Accessibility

### Linux

- Using the X11 `XRecord` extension

- Requires the following development libraries to be installed:

``bash

sudo apt-get install libx11-dev libxtst-dev libxt-dev libxinerama-dev \
libx11-xcb-dev libxkbcommon-dev libxkbcommon-x11-dev \
libxkbfile-dev

```

## Compiling and Testing

### Compilation Commands

```bash

# Windows
python run.py build windows_x86_64

# macOS
python run.py build macos_arm64

# Linux
python run.py build ubuntu-22.04_x86_64

```

### Testing Method

1. Launch the momo application

2. Move the mouse into the SDL window

3. Try pressing a keyboard key to confirm that it is intercepted (not passed to other applications)

4. Move the mouse outside the SDL window

5. Try pressing a keyboard key to confirm that it works normally (passed to other applications)

### Verification Points

- [ ] Keyboard input is intercepted when the mouse enters the window

- [ ] Keyboard input returns to normal when the mouse leaves the window

- [ ] Keyboard input returns to normal when the window loses focus

- [ ] Keyboard input is intercepted when the window gains focus and the mouse is inside the window

- [ ] Works normally when switching mouse position and window focus state multiple times

- [ ] No memory leak

- [ ] No crashes

## Notes

1. **Performance**: Keyboard hooks are low-level system operations with slight performance overhead, but this is almost negligible on modern systems.

2. **Security**: Some operating systems may prompt users to grant keyboard hook permissions.

3. **System Key Combinations:** System-level key combinations (such as Ctrl+Alt+Del) cannot be intercepted on some platforms.

4. **Event Consumption:** Intercepted keyboard events will be completely consumed and will not be passed to other applications.

5. **Debugging:** If the keyboard hook intercepts debugger breakpoint shortcuts, it may be necessary to temporarily disable the hook for debugging.

## Future Improvement Suggestions

1. **Whitelist/Blacklist:** Implement a key whitelist or blacklist to selectively intercept specific keys.

2. **Hotkey Support:** Add global hotkey functionality in interception mode.

3. **Key Recording:** Implement key recording and replay functionality.

4. **Configurability:** Control interception behavior through configuration files or command-line arguments.

5. **Performance Optimization:** Reduce the frequency of `UpdateMouseTracking()` calls.

6. **Error Recovery:** Enhance error handling and automatic recovery mechanisms.

## Dependency Library Versions

- **libuiohook:** Used in the project Versions in the `third_party/libuiohook` directory:

- **SDL3**: Project already has this dependency

- **WebRTC**: Project already has this dependency

## References

- [libuiohook GitHub](https://github.com/kwhat/libuiohook)

- [SDL3 Documentation](https://wiki.libsdl.org/SDL3)

- [Windows Hooks](https://docs.microsoft.com/windows/win32/winmsg/hooks)

- [macOS Event Taps](https://developer.apple.com/documentation/coregraphics/cgeventtap)

- [X11 XRecord](https://www.x.org/releases/X11R7.7/doc/libXtst/recordlib.html)

## Contributors

- Implementation: Claude (Anthropic AI Assistant)

- Review: [Your Name]

- Testing: [Tester] [Names]

## License

This integration code is licensed under the project's original Apache License 2.0.

The libuiohook library is licensed under the LGPL v3 license.