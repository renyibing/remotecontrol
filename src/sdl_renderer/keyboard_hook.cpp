#include "keyboard_hook.h"

#include <cstdarg>
#include <cstdio>
#include <iostream>

namespace sdl_hook {

// Static instance pointer for callbacks
KeyboardHookManager* KeyboardHookManager::instance_ = nullptr;

KeyboardHookManager::KeyboardHookManager() {
  instance_ = this;
  
  // Create synchronization primitives
  hook_running_mutex_ = SDL_CreateMutex();
  hook_control_mutex_ = SDL_CreateMutex();
  hook_control_cond_ = SDL_CreateCondition();
  
  if (!hook_running_mutex_ || !hook_control_mutex_ || !hook_control_cond_) {
    std::cerr << "KeyboardHookManager: Failed to create synchronization primitives" << std::endl;
  }
}

KeyboardHookManager::~KeyboardHookManager() {
  Shutdown();
  
  // Destroy synchronization primitives
  if (hook_control_cond_) {
    SDL_DestroyCondition(hook_control_cond_);
    hook_control_cond_ = nullptr;
  }
  if (hook_control_mutex_) {
    SDL_DestroyMutex(hook_control_mutex_);
    hook_control_mutex_ = nullptr;
  }
  if (hook_running_mutex_) {
    SDL_DestroyMutex(hook_running_mutex_);
    hook_running_mutex_ = nullptr;
  }
  
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

bool KeyboardHookManager::Initialize() {
  if (initialized_.exchange(true)) {
    std::cout << "KeyboardHookManager: Already initialized" << std::endl;
    return true;
  }

  if (!hook_running_mutex_ || !hook_control_mutex_ || !hook_control_cond_) {
    std::cerr << "KeyboardHookManager: Synchronization primitives not available" << std::endl;
    initialized_ = false;
    return false;
  }

  // Lock the control mutex
  SDL_LockMutex(hook_control_mutex_);

  // Set logger callback
  hook_set_logger_proc(&LoggerProc);

  // Set dispatch callback
  hook_set_dispatch_proc(&HookEventProc);

  // Create background thread to run hook_run()
  hook_thread_ = SDL_CreateThread(HookThreadFunc, "UIOHookThread", this);
  
  if (!hook_thread_) {
    std::cerr << "KeyboardHookManager: Failed to create hook thread: " 
              << SDL_GetError() << std::endl;
    SDL_UnlockMutex(hook_control_mutex_);
    initialized_ = false;
    return false;
  }

  // Wait for EVENT_HOOK_ENABLED signal
  SDL_WaitCondition(hook_control_cond_, hook_control_mutex_);

  // Check if hook started successfully
  bool try_lock_result = SDL_TryLockMutex(hook_running_mutex_);
  
  if (try_lock_result) {
    // Hook failed to start
    std::cerr << "KeyboardHookManager: Hook failed to start" << std::endl;
    
    int thread_status = 0;
    SDL_WaitThread(hook_thread_, &thread_status);
    hook_thread_ = nullptr;
    
    SDL_UnlockMutex(hook_running_mutex_);
    SDL_UnlockMutex(hook_control_mutex_);
    initialized_ = false;
    return false;
  }
  
  // Success!
  SDL_UnlockMutex(hook_control_mutex_);
  
  std::cout << "KeyboardHookManager: Successfully initialized" << std::endl;
  return true;
}

void KeyboardHookManager::Shutdown() {
  if (!initialized_.exchange(false)) {
    return;
  }

  std::cout << "KeyboardHookManager: Shutting down..." << std::endl;

  // Stop the hook
  int status = hook_stop();
  if (status != UIOHOOK_SUCCESS) {
    std::cerr << "KeyboardHookManager: hook_stop() failed with status " 
              << status << std::endl;
  }

  // Wait for thread to finish
  if (hook_thread_) {
    SDL_WaitThread(hook_thread_, nullptr);
    hook_thread_ = nullptr;
  }

  std::cout << "KeyboardHookManager: Shutdown complete" << std::endl;
}

void KeyboardHookManager::UpdateMouseTracking() {
  if (!window_ || !initialized_) {
    return;
  }

  // Get mouse position
  float mx = 0.0f, my = 0.0f;
  SDL_GetMouseState(&mx, &my);

  // Get window size
  int w = 0, h = 0;
  SDL_GetWindowSize(window_, &w, &h);

  // Check if mouse is inside window
  bool inside = (mx >= 0.0f && mx <= static_cast<float>(w) && 
                 my >= 0.0f && my <= static_cast<float>(h));

  // Check if window has focus
  SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
  bool focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;

  // Update state
  mouse_inside_window_ = inside;
  window_has_focus_ = focused;

  // Determine if we should be intercepting
  bool should_intercept = (mouse_inside_window_ && window_has_focus_);
  bool currently_intercepting = is_intercepting_.load();

  // If state changed, enable or disable the hook
  if (should_intercept != currently_intercepting) {
    std::cout << "KeyboardHookManager: should_intercept=" << (should_intercept ? "true" : "false")
              << " mouse_inside=" << (mouse_inside_window_ ? "true" : "false")
              << " focus=" << (window_has_focus_ ? "true" : "false") << std::endl;
    if (should_intercept) {
      EnableInterception();
    } else {
      DisableInterception();
    }
  }
}

void KeyboardHookManager::EnableInterception() {
  if (is_intercepting_.exchange(true)) {
    return;  // Already enabled
  }
  
  std::cout << "KeyboardHookManager: Interception ENABLED (re-hooking)" << std::endl;
  
  // Note: libuiohook doesn't support dynamic enable/disable after hook_run() starts
  // The hook is always active, we just control whether to forward events
  // This is a limitation of the library design
}

void KeyboardHookManager::DisableInterception() {
  if (!is_intercepting_.exchange(false)) {
    return;  // Already disabled
  }
  
  std::cout << "KeyboardHookManager: Interception DISABLED (releasing hook)" << std::endl;
  
  // Note: libuiohook doesn't support dynamic enable/disable after hook_run() starts
  // The hook is always active, we just control whether to forward events
  // This is a limitation of the library design
}

// Hook thread function
int KeyboardHookManager::HookThreadFunc(void* data) {
  KeyboardHookManager* self = static_cast<KeyboardHookManager*>(data);
  if (!self) {
    return UIOHOOK_FAILURE;
  }

  std::cout << "HookThreadFunc: Starting hook_run()..." << std::endl;

  // Run the hook (blocks until hook_stop() is called)
  int status = hook_run();

  std::cout << "HookThreadFunc: hook_run() returned with status " << status << std::endl;

  return status;
}

// Hook event dispatcher callback
void KeyboardHookManager::HookEventProc(uiohook_event* const event) {
  if (!instance_) {
    return;
  }

  switch (event->type) {
    case EVENT_HOOK_ENABLED:
      // Lock running mutex to indicate hook is active
      SDL_LockMutex(instance_->hook_running_mutex_);
      
      // Signal Initialize() that we're ready
      SDL_SignalCondition(instance_->hook_control_cond_);
      SDL_UnlockMutex(instance_->hook_control_mutex_);
      break;

    case EVENT_HOOK_DISABLED:
      // Lock control mutex and unlock running mutex
      SDL_LockMutex(instance_->hook_control_mutex_);
      SDL_UnlockMutex(instance_->hook_running_mutex_);
      break;

    case EVENT_KEY_PRESSED:
    case EVENT_KEY_RELEASED: {
      // Debug: Log ALL keyboard events at hook level
      std::cout << "[Hook] Keycode=" << event->data.keyboard.keycode 
                << " (" << (event->type == EVENT_KEY_PRESSED ? "DOWN" : "UP") << ")"
                << " intercepting=" << instance_->is_intercepting_.load()
                << " has_window=" << (instance_->window_ != nullptr) << std::endl;
      
      // Check if we should intercept
      if (instance_->is_intercepting_.load() && instance_->window_) {
        // Intercept: forward to SDL and block from OS
        // Set event.reserved = 0x01 to CONSUME the event (prevent OS from seeing it)
        // Logic: if (event.reserved ^ 0x01) → if (0x01 ^ 0x01) → if (0x00) → false → consume
        event->reserved = 0x01;
        
        // Convert libuiohook event to SDL event and push it
        SDL_Event sdl_event;
        SDL_zero(sdl_event);
        
        sdl_event.type = (event->type == EVENT_KEY_PRESSED) ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        sdl_event.key.windowID = SDL_GetWindowID(instance_->window_);
        sdl_event.key.timestamp = SDL_GetTicks();
        sdl_event.key.down = (event->type == EVENT_KEY_PRESSED);
        sdl_event.key.repeat = false;
        
        // Convert keycode
        sdl_event.key.key = ConvertKeycodeToSDL(event->data.keyboard.keycode);
        sdl_event.key.scancode = SDL_GetScancodeFromKey(sdl_event.key.key, nullptr);
        
        std::cout << "[Hook] Converted to SDL keycode=" << sdl_event.key.key 
                  << " name=" << (SDL_GetKeyName(sdl_event.key.key) ? SDL_GetKeyName(sdl_event.key.key) : "NULL") 
                  << std::endl;
        
        // Set modifier state
        sdl_event.key.mod = SDL_KMOD_NONE;
        if (event->mask & MASK_SHIFT) sdl_event.key.mod = static_cast<SDL_Keymod>(sdl_event.key.mod | SDL_KMOD_SHIFT);
        if (event->mask & MASK_CTRL) sdl_event.key.mod = static_cast<SDL_Keymod>(sdl_event.key.mod | SDL_KMOD_CTRL);
        if (event->mask & MASK_ALT) sdl_event.key.mod = static_cast<SDL_Keymod>(sdl_event.key.mod | SDL_KMOD_ALT);
        if (event->mask & MASK_META) sdl_event.key.mod = static_cast<SDL_Keymod>(sdl_event.key.mod | SDL_KMOD_GUI);
        
        // Push event to SDL queue
        int result = SDL_PushEvent(&sdl_event);
        std::cout << "[Hook] SDL_PushEvent result=" << result << std::endl;
      }
      break;
    }
      // else: Keep event.reserved = 0x00 (default) to PASS THROUGH to OS
      // Logic: if (event.reserved ^ 0x01) → if (0x00 ^ 0x01) → if (0x01) → true → pass through
      break;

    default:
      break;
  }
}

// Convert libuiohook keycode to SDL keycode
SDL_Keycode KeyboardHookManager::ConvertKeycodeToSDL(uint16_t uiohook_keycode) {
  // Extended keys (with 0xEE00 prefix) need to be handled separately from numpad keys
  // because they share the same lower byte values
  // For example: 0xEE48 is Extended UP, but 0x0048 is Numpad 8
  
  switch (uiohook_keycode) {
    // Extended navigation keys (with 0xEE00 prefix to distinguish from numpad)
    case 0xEE48: return SDLK_UP;       // Extended UP (not Numpad 8)
    case 0xEE50: return SDLK_DOWN;     // Extended DOWN (not Numpad 2)
    case 0xEE4B: return SDLK_LEFT;     // Extended LEFT (not Numpad 4)
    case 0xEE4D: return SDLK_RIGHT;    // Extended RIGHT (not Numpad 6)
    case 0xEE47: return SDLK_HOME;     // Extended HOME (not Numpad 7)
    case 0xEE4F: return SDLK_END;      // Extended END (not Numpad 1)
    case 0xEE49: return SDLK_PAGEUP;   // Extended PAGE UP (not Numpad 9)
    case 0xEE51: return SDLK_PAGEDOWN; // Extended PAGE DOWN (not Numpad 3)
    case 0xEE52: return SDLK_INSERT;   // Extended INSERT (not Numpad 0)
    case 0xEE53: return SDLK_DELETE;   // Extended DELETE (not Numpad .)
    
    // Special keys
    case VC_ESCAPE: return SDLK_ESCAPE;
    
    // Function keys (F1-F24)
    case VC_F1: return SDLK_F1;
    case VC_F2: return SDLK_F2;
    case VC_F3: return SDLK_F3;
    case VC_F4: return SDLK_F4;
    case VC_F5: return SDLK_F5;
    case VC_F6: return SDLK_F6;
    case VC_F7: return SDLK_F7;
    case VC_F8: return SDLK_F8;
    case VC_F9: return SDLK_F9;
    case VC_F10: return SDLK_F10;
    case VC_F11: return SDLK_F11;
    case VC_F12: return SDLK_F12;
    case VC_F13: return SDLK_F13;
    case VC_F14: return SDLK_F14;
    case VC_F15: return SDLK_F15;
    case VC_F16: return SDLK_F16;
    case VC_F17: return SDLK_F17;
    case VC_F18: return SDLK_F18;
    case VC_F19: return SDLK_F19;
    case VC_F20: return SDLK_F20;
    case VC_F21: return SDLK_F21;
    case VC_F22: return SDLK_F22;
    case VC_F23: return SDLK_F23;
    case VC_F24: return SDLK_F24;
    
    // Number row keys (with special characters: ~!@#$%^&*()_+)
    case VC_BACKQUOTE: return SDLK_GRAVE;     // ` ~
    case VC_1: return SDLK_1;                 // 1 !
    case VC_2: return SDLK_2;                 // 2 @
    case VC_3: return SDLK_3;                 // 3 #
    case VC_4: return SDLK_4;                 // 4 $
    case VC_5: return SDLK_5;                 // 5 %
    case VC_6: return SDLK_6;                 // 6 ^
    case VC_7: return SDLK_7;                 // 7 &
    case VC_8: return SDLK_8;                 // 8 *
    case VC_9: return SDLK_9;                 // 9 (
    case VC_0: return SDLK_0;                 // 0 )
    case VC_MINUS: return SDLK_MINUS;         // - _
    case VC_EQUALS: return SDLK_EQUALS;       // = +
    
    // Letter keys (A-Z)
    case VC_A: return SDLK_A;
    case VC_B: return SDLK_B;
    case VC_C: return SDLK_C;
    case VC_D: return SDLK_D;
    case VC_E: return SDLK_E;
    case VC_F: return SDLK_F;
    case VC_G: return SDLK_G;
    case VC_H: return SDLK_H;
    case VC_I: return SDLK_I;
    case VC_J: return SDLK_J;
    case VC_K: return SDLK_K;
    case VC_L: return SDLK_L;
    case VC_M: return SDLK_M;
    case VC_N: return SDLK_N;
    case VC_O: return SDLK_O;
    case VC_P: return SDLK_P;
    case VC_Q: return SDLK_Q;
    case VC_R: return SDLK_R;
    case VC_S: return SDLK_S;
    case VC_T: return SDLK_T;
    case VC_U: return SDLK_U;
    case VC_V: return SDLK_V;
    case VC_W: return SDLK_W;
    case VC_X: return SDLK_X;
    case VC_Y: return SDLK_Y;
    case VC_Z: return SDLK_Z;
    
    // Special character keys
    case VC_OPEN_BRACKET: return SDLK_LEFTBRACKET;    // [ {
    case VC_CLOSE_BRACKET: return SDLK_RIGHTBRACKET;  // ] }
    case VC_BACK_SLASH: return SDLK_BACKSLASH;        // \ |
    case VC_SEMICOLON: return SDLK_SEMICOLON;         // ; :
    case VC_QUOTE: return SDLK_APOSTROPHE;            // ' "
    case VC_COMMA: return SDLK_COMMA;                 // , <
    case VC_PERIOD: return SDLK_PERIOD;               // . >
    case VC_SLASH: return SDLK_SLASH;                 // / ?
    
    // Modifier keys
    case VC_SHIFT_L: return SDLK_LSHIFT;
    case VC_SHIFT_R: return SDLK_RSHIFT;
    case VC_CONTROL_L: return SDLK_LCTRL;
    case VC_CONTROL_R: return SDLK_RCTRL;
    case VC_ALT_L: return SDLK_LALT;
    case VC_ALT_R: return SDLK_RALT;
    case VC_META_L: return SDLK_LGUI;        // Windows/Command key (left)
    case VC_META_R: return SDLK_RGUI;        // Windows/Command key (right)
    
    // Control keys
    case VC_SPACE: return SDLK_SPACE;
    case VC_ENTER: return SDLK_RETURN;
    case VC_BACKSPACE: return SDLK_BACKSPACE;
    case VC_TAB: return SDLK_TAB;
    case VC_CAPS_LOCK: return SDLK_CAPSLOCK;
    case VC_NUM_LOCK: return SDLK_NUMLOCKCLEAR;
    case VC_SCROLL_LOCK: return SDLK_SCROLLLOCK;
    case VC_PAUSE: return SDLK_PAUSE;
    case VC_PRINTSCREEN: return SDLK_PRINTSCREEN;
    case VC_CONTEXT_MENU: return SDLK_APPLICATION;  // Context menu key
    
    // Navigation keys (Arrow keys) - These are the non-extended versions (rarely used)
    case VC_UP: return SDLK_UP;
    case VC_DOWN: return SDLK_DOWN;
    case VC_LEFT: return SDLK_LEFT;
    case VC_RIGHT: return SDLK_RIGHT;
    
    // Navigation keys (6-key cluster) - These are the non-extended versions (rarely used)
    case VC_PAGE_UP: return SDLK_PAGEUP;
    case VC_PAGE_DOWN: return SDLK_PAGEDOWN;
    case VC_HOME: return SDLK_HOME;
    case VC_END: return SDLK_END;
    case VC_INSERT: return SDLK_INSERT;
    case VC_DELETE: return SDLK_DELETE;
    
    // Numpad keys
    case VC_KP_DIVIDE: return SDLK_KP_DIVIDE;      // / (numpad)
    case VC_KP_MULTIPLY: return SDLK_KP_MULTIPLY;  // * (numpad)
    case VC_KP_SUBTRACT: return SDLK_KP_MINUS;     // - (numpad)
    case VC_KP_ADD: return SDLK_KP_PLUS;           // + (numpad)
    case VC_KP_ENTER: return SDLK_KP_ENTER;        // Enter (numpad)
    case VC_KP_SEPARATOR: return SDLK_KP_PERIOD;   // . (numpad)
    case VC_KP_0: return SDLK_KP_0;
    case VC_KP_1: return SDLK_KP_1;
    case VC_KP_2: return SDLK_KP_2;
    case VC_KP_3: return SDLK_KP_3;
    case VC_KP_4: return SDLK_KP_4;
    case VC_KP_5: return SDLK_KP_5;
    case VC_KP_6: return SDLK_KP_6;
    case VC_KP_7: return SDLK_KP_7;
    case VC_KP_8: return SDLK_KP_8;
    case VC_KP_9: return SDLK_KP_9;
    
    // Media keys (SDL3 naming)
    case VC_MEDIA_PLAY: return SDLK_MEDIA_PLAY;
    case VC_MEDIA_STOP: return SDLK_MEDIA_STOP;
    case VC_MEDIA_PREVIOUS: return SDLK_MEDIA_PREVIOUS_TRACK;
    case VC_MEDIA_NEXT: return SDLK_MEDIA_NEXT_TRACK;
    case VC_MEDIA_SELECT: return SDLK_MEDIA_SELECT;
    case VC_MEDIA_EJECT: return SDLK_MEDIA_EJECT;
    case VC_VOLUME_MUTE: return SDLK_MUTE;
    case VC_VOLUME_UP: return SDLK_VOLUMEUP;
    case VC_VOLUME_DOWN: return SDLK_VOLUMEDOWN;
    
    // Browser/App keys
    case VC_APP_MAIL: return SDLK_UNKNOWN;        // SDL3 removed SDLK_MAIL
    case VC_APP_CALCULATOR: return SDLK_UNKNOWN;  // SDL3 removed SDLK_CALCULATOR
    case VC_APP_MUSIC: return SDLK_MEDIA_PLAY;
    case VC_APP_PICTURES: return SDLK_UNKNOWN;    // No direct SDL equivalent
    
    // Power keys
    case VC_POWER: return SDLK_POWER;
    case VC_SLEEP: return SDLK_SLEEP;
    case VC_WAKE: return SDLK_UNKNOWN;  // No direct SDL equivalent
    
    default:
      return SDLK_UNKNOWN;
  }
}

// Logger callback for libuiohook
bool KeyboardHookManager::LoggerProc(unsigned int level,
                                     const char* format,
                                     ...) {
  bool status = false;

  va_list args;
  switch (level) {
    case LOG_LEVEL_INFO:
      va_start(args, format);
      status = vfprintf(stdout, format, args) >= 0;
      va_end(args);
      break;

    case LOG_LEVEL_WARN:
    case LOG_LEVEL_ERROR:
      va_start(args, format);
      status = vfprintf(stderr, format, args) >= 0;
      va_end(args);
      break;

    default:
      break;
  }

  return status;
}

}  // namespace sdl_hook
