#ifndef SDL_RENDERER_KEYBOARD_HOOK_H_
#define SDL_RENDERER_KEYBOARD_HOOK_H_

#include <atomic>
#include <memory>

// SDL
#include <SDL3/SDL.h>

// libuiohook
#include <uiohook.h>

namespace sdl_hook {

// Keyboard hook manager using libuiohook
// Captures system-level keyboard events (Win, Ctrl+Esc, etc.) that SDL normally doesn't receive
// and forwards them to SDL as synthetic events so they can be sent to the remote end
class KeyboardHookManager {
 public:
  KeyboardHookManager();
  ~KeyboardHookManager();

  // Initialize the hook and start the background thread (call once at startup)
  bool Initialize();

  // Cleanup the hook and stop the background thread (call once at shutdown)
  void Shutdown();

  // Set the SDL window pointer for event forwarding
  void SetSDLWindow(SDL_Window* window) { window_ = window; }

  // Update interception state based on window focus
  void UpdateMouseTracking();

 private:
  // Enable/disable keyboard interception
  void EnableInterception();
  void DisableInterception();
  // Hook thread entry point (runs hook_run() in background)
  static int HookThreadFunc(void* data);

  // Hook event dispatcher callback (runs on hook thread)
  static void HookEventProc(uiohook_event* const event);

  // Logger callback for libuiohook
  static bool LoggerProc(unsigned int level, const char* format, ...);

  // Convert libuiohook keycode to SDL keycode
  static SDL_Keycode ConvertKeycodeToSDL(uint16_t uiohook_keycode);

  // Thread and synchronization (following demo_hook_async.c pattern)
  SDL_Thread* hook_thread_{nullptr};

  // Mutexes for thread synchronization
  SDL_Mutex* hook_running_mutex_{nullptr};   // Indicates if hook is running
  SDL_Mutex* hook_control_mutex_{nullptr};   // Controls startup/shutdown
  SDL_Condition* hook_control_cond_{nullptr}; // Condition variable for signaling

  // State flags
  std::atomic<bool> is_intercepting_{false};  // Should we forward events to SDL?
  std::atomic<bool> initialized_{false};      // Has Initialize() been called?
  
  SDL_Window* window_{nullptr};
  bool mouse_inside_window_{false};
  bool window_has_focus_{false};

  // Singleton instance for callbacks
  static KeyboardHookManager* instance_;
};

}  // namespace sdl_hook

#endif  // SDL_RENDERER_KEYBOARD_HOOK_H_
