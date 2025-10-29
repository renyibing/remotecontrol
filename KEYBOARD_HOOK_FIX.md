# System Keyboard Hook Fix - Dynamic Pass-Through

## Problem
The `libuiohook` keyboard hook was intercepting **all** keyboard input system-wide, even when the SDL window lost focus or the mouse moved outside. This prevented users from typing on the local system (sending end).

## Root Cause
**No dynamic control**: Events were being consumed system-wide regardless of window focus/mouse position, preventing local keyboard input when the SDL window was not active.

## Solution

### Understanding libuiohook's Reserved Field Logic

**File**: `third_party/libuiohook/src/windows/input_hook.c` (line 277-278)

```c
if (nCode < 0 || event.reserved ^ 0x01) {
    hook_result = 1;  // Pass through
} else {
    // Consume event (return -1)
}
```

**The XOR Logic**:
- `event.reserved = 0x00` (default): `0x00 ^ 0x01 = 0x01` (true) → **PASS THROUGH** to OS
- `event.reserved = 0x01`: `0x01 ^ 0x01 = 0x00` (false) → **CONSUME** (block from OS)

### Dynamic Event Control
**File**: `src/sdl_renderer/keyboard_hook.cpp`

The dispatch callback now sets `event->reserved` based on interception state:

```cpp
case EVENT_KEY_PRESSED:
case EVENT_KEY_RELEASED:
  if (instance_->is_intercepting_.load() && instance_->window_) {
    // Intercept: consume event and forward to SDL
    event->reserved = 0x01;  // XOR: 0x01 ^ 0x01 = 0x00 → CONSUME
    SDL_PushEvent(&sdl_event);  // Send to remote via SDL
  }
  // else: Keep event->reserved = 0x00 (default)
  // XOR: 0x00 ^ 0x01 = 0x01 → PASS THROUGH to OS
  break;
```

### 3. State Management
**File**: `src/sdl_renderer/keyboard_hook.cpp`

Interception is enabled only when **BOTH** conditions are true:
- Mouse is inside SDL window
- Window has input focus

State is updated on these SDL events:
- `SDL_EVENT_MOUSE_MOTION`
- `SDL_EVENT_WINDOW_FOCUS_GAINED`
- `SDL_EVENT_WINDOW_FOCUS_LOST`
- `SDL_EVENT_WINDOW_MOUSE_ENTER`
- `SDL_EVENT_WINDOW_MOUSE_LEAVE`

## Behavior

### ✅ Mouse Inside + Window Focused
- `event->reserved = 0x01` → XOR result = 0x00 → Event **CONSUMED** by hook
- Event forwarded to SDL → Sent to remote end
- **Local system**: Key doesn't work (blocked from OS)
- **Remote system**: Key press received

### ✅ Mouse Outside OR Window Unfocused
- `event->reserved = 0x00` (default) → XOR result = 0x01 → Event **PASSES THROUGH**
- Hook returns 1 → OS handles normally
- **Local system**: Key works normally
- **Remote system**: No key press (not forwarded)

## Testing

1. **Start momo** with SDL window visible
2. **Mouse inside + Window focused**: Press Win key → should be sent to remote
3. **Mouse outside**: Press Win key → should open Start menu locally
4. **Window unfocused**: Press Ctrl+Esc → should work locally
5. **Alt+Tab away**: Keyboard should work in other apps

## Technical Notes

- The hook runs continuously in a background thread (`hook_run()`)
- We **cannot** dynamically start/stop the hook (libuiohook limitation)
- Instead, we control event consumption via the `reserved` field
- This is more efficient than stop/restart anyway

## Files Modified

1. `src/sdl_renderer/keyboard_hook.cpp` - Dynamic event control via `event->reserved`
2. `src/sdl_renderer/keyboard_hook.h` - Added EnableInterception/DisableInterception
3. `src/sdl_renderer/sdl_renderer.cpp` - Event tracking updates
4. `CMakeLists.txt` - Restored libuiohook integration with notes

## Credits

This fix addresses the fundamental issue with low-level Windows keyboard hooks:
they intercept events **before** they reach applications, so proper pass-through
logic is critical for system usability.

