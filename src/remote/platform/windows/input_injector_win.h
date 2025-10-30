// Description: Windows platform input injector implementation
// - Keyboard/mouse: vmulti (fallback to SendInput/SetCursorPos if initialization fails)
// - Gamepad: dynamically load ViGEmClient.dll to inject XInput (X360) messages

#ifndef REMOTE_PLATFORM_WINDOWS_INPUT_INJECTOR_WIN_H_
#define REMOTE_PLATFORM_WINDOWS_INPUT_INJECTOR_WIN_H_

#ifdef _WIN32

#include <windows.h>
#include <cstdint>
#include <string>
#include <algorithm>
#include <iostream>
#include <vector>
#include <SDL3/SDL_keycode.h>

#include "remote/input_receiver/input_injector.h"

// vmulti client API
extern "C" {
#include <vmulticlient.h>
}

namespace remote {
namespace platform {
namespace windows {

// XUSB message definition (minimum set)
typedef struct _XUSB_REPORT {
  uint16_t wButtons;
  uint8_t bLeftTrigger;
  uint8_t bRightTrigger;
  int16_t sThumbLX;
  int16_t sThumbLY;
  int16_t sThumbRX;
  int16_t sThumbRY;
} XUSB_REPORT, *PXUSB_REPORT;

// ViGEm dynamic loading wrapper
class ViGEmDyn {
 public:
  ViGEmDyn() : hmod_(nullptr), client_(nullptr), target_(nullptr) {}
  ~ViGEmDyn() { Cleanup(); }

  bool Init() {
    if (hmod_) return true;
    hmod_ = ::LoadLibraryA("ViGEmClient.dll");
    if (!hmod_) return false;
    p_vigem_alloc_ = reinterpret_cast<pf_vigem_alloc>(::GetProcAddress(hmod_, "vigem_alloc")); if (!p_vigem_alloc_) return false;
    p_vigem_connect_ = reinterpret_cast<pf_vigem_connect>(::GetProcAddress(hmod_, "vigem_connect")); if (!p_vigem_connect_) return false;
    p_vigem_free_ = reinterpret_cast<pf_vigem_free>(::GetProcAddress(hmod_, "vigem_free")); if (!p_vigem_free_) return false;
    p_vigem_target_x360_alloc_ = reinterpret_cast<pf_vigem_target_x360_alloc>(::GetProcAddress(hmod_, "vigem_target_x360_alloc")); if (!p_vigem_target_x360_alloc_) return false;
    p_vigem_target_add_ = reinterpret_cast<pf_vigem_target_add>(::GetProcAddress(hmod_, "vigem_target_add")); if (!p_vigem_target_add_) return false;
    p_vigem_target_remove_ = reinterpret_cast<pf_vigem_target_remove>(::GetProcAddress(hmod_, "vigem_target_remove")); if (!p_vigem_target_remove_) return false;
    p_vigem_target_free_ = reinterpret_cast<pf_vigem_target_free>(::GetProcAddress(hmod_, "vigem_target_free")); if (!p_vigem_target_free_) return false;
    p_vigem_target_x360_update_ = reinterpret_cast<pf_vigem_target_x360_update>(::GetProcAddress(hmod_, "vigem_target_x360_update")); if (!p_vigem_target_x360_update_) return false;
    client_ = p_vigem_alloc_();
    if (!client_) return false;
    if (p_vigem_connect_(client_) != 0) return false;  // VIGEM_ERROR_NONE == 0
    target_ = p_vigem_target_x360_alloc_();
    if (!target_) return false;
    if (p_vigem_target_add_(client_, target_) != 0) return false;
    return true;
  }

  void UpdateX360(const XUSB_REPORT& rep) {
    if (!client_ || !target_) return;
    p_vigem_target_x360_update_(client_, target_, rep);
  }

 private:
  void Cleanup() {
    if (client_ && target_) {
      p_vigem_target_remove_(client_, target_);
      p_vigem_target_free_(target_);
      target_ = nullptr;
    }
    if (client_) {
      p_vigem_free_(client_);
      client_ = nullptr;
    }
    if (hmod_) {
      ::FreeLibrary(hmod_);
      hmod_ = nullptr;
    }
  }

  // ViGEmClient API prototype (approximated with void*/int to avoid dependency on header files)
  typedef void* (*pf_vigem_alloc)();
  typedef int (*pf_vigem_connect)(void*);
  typedef void (*pf_vigem_free)(void*);
  typedef void* (*pf_vigem_target_x360_alloc)();
  typedef int (*pf_vigem_target_add)(void*, void*);
  typedef void (*pf_vigem_target_remove)(void*, void*);
  typedef void (*pf_vigem_target_free)(void*);
  typedef void (*pf_vigem_target_x360_update)(void*, void*, XUSB_REPORT);

  HMODULE hmod_;
  void* client_;
  void* target_;
  pf_vigem_alloc p_vigem_alloc_{nullptr};
  pf_vigem_connect p_vigem_connect_{nullptr};
  pf_vigem_free p_vigem_free_{nullptr};
  pf_vigem_target_x360_alloc p_vigem_target_x360_alloc_{nullptr};
  pf_vigem_target_add p_vigem_target_add_{nullptr};
  pf_vigem_target_remove p_vigem_target_remove_{nullptr};
  pf_vigem_target_free p_vigem_target_free_{nullptr};
  pf_vigem_target_x360_update p_vigem_target_x360_update_{nullptr};
};

class WindowsInputInjector : public remote::input_receiver::IInputInjector {
 public:
  WindowsInputInjector() {
    // Try to initialize hiddriver (requires hiddriver to be installed)
    hiddriver_ = vmulti_alloc();  // API name kept for compatibility
    if (hiddriver_) {
      use_hiddriver_ = vmulti_connect(hiddriver_);
      if (!use_hiddriver_) {
        vmulti_free(hiddriver_);
        hiddriver_ = nullptr;
        std::cout << "[input_injector] hiddriver not found or access denied" << std::endl;
        std::cout << "[input_injector] Possible reasons:" << std::endl;
        std::cout << "[input_injector]   1. hiddriver not installed (xrcloud\\hiddriver)" << std::endl;
        std::cout << "[input_injector]   2. Insufficient permissions (run as Administrator)" << std::endl;
        std::cout << "[input_injector]   3. Device not accessible" << std::endl;
        std::cout << "[input_injector] Falling back to SendInput method" << std::endl;
      } else {
        std::cout << "[input_injector] hiddriver connected successfully" << std::endl;
      }
    } else {
      std::cout << "[input_injector] Failed to allocate hiddriver client, using SendInput" << std::endl;
    }
  }
  
  ~WindowsInputInjector() {
    if (hiddriver_) {
      vmulti_disconnect(hiddriver_);
      vmulti_free(hiddriver_);
      hiddriver_ = nullptr;
    }
  }

  // Keyboard injection
  void InjectKeyboard(const remote::proto::KeyboardMsg& ev) override {
    // Try hiddriver first
    if (use_hiddriver_ && hiddriver_) {
      bool is_modifier = IsModifierKey(ev.code);
      
      if (is_modifier) {
        // For modifier keys, update HID modifier state based on key identity
        BYTE hid_bit = GetHidModifierBit(ev.code);
        if (hid_bit == 0) {
          goto use_sendinput;  // Unknown modifier, fallback
        }
        if (ev.down) {
          hid_modifiers_ |= hid_bit;
        } else {
          hid_modifiers_ &= static_cast<BYTE>(~hid_bit);
        }
      } else {
        // For non-modifier keys, update key state normally
        BYTE hid_code = MapKeyToHIDScancode(ev);
        if (hid_code != 0) {
          if (ev.down) {
            UpdateKeyState(hid_code, true);
          } else {
            UpdateKeyState(hid_code, false);
          }
        } else {
          // Unknown key, fall through to SendInput
          goto use_sendinput;
        }
      }
      
      // Always send HID report when using hiddriver (for both modifiers and regular keys)
      BYTE shift_flags = hid_modifiers_;
      
      BYTE key_codes[KBD_KEY_CODES] = {0};
      size_t idx = 0;
      for (BYTE code : pressed_keys_) {
        if (idx >= KBD_KEY_CODES) break;
        key_codes[idx++] = code;
      }
      
      if (vmulti_update_keyboard(hiddriver_, shift_flags, key_codes)) {
        return;  // Success with hiddriver
      }
      // Fall through to SendInput if hiddriver fails
    }
    
use_sendinput:
    
    // Fallback to SendInput
    WORD vk = MapKey(ev);
    if (vk == 0) return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = ev.down ? 0 : KEYEVENTF_KEYUP;
    if (IsExtendedKey(vk)) {
      in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    ::SendInput(1, &in, sizeof(INPUT));

    if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT) {
      std::cout << "[input] InjectKeyboard vk=" << vk
                << " down=" << (ev.down ? "true" : "false")
                << " key=" << ev.key << " code=" << ev.code
                << " mods=" << ev.mods << std::endl;
    }
  }

  // Mouse absolute position
  void InjectMouseAbs(float x, float y, const remote::proto::Buttons& btns) override {
    if (use_hiddriver_ && hiddriver_) {
      // Map screen coordinates to HID coordinates (0-32767)
      USHORT hid_x = static_cast<USHORT>(std::clamp(x / static_cast<float>(GetSystemMetrics(SM_CXSCREEN)) * MOUSE_MAX_COORDINATE, 0.0f, static_cast<float>(MOUSE_MAX_COORDINATE)));
      USHORT hid_y = static_cast<USHORT>(std::clamp(y / static_cast<float>(GetSystemMetrics(SM_CYSCREEN)) * MOUSE_MAX_COORDINATE, 0.0f, static_cast<float>(MOUSE_MAX_COORDINATE)));
      BYTE button_flags = ConvertMouseButtons(btns.bits);
      
      if (vmulti_update_mouse(hiddriver_, button_flags, hid_x, hid_y, 0)) {
        last_btns_ = btns.bits;
        return;
      }
    }
    
    // Fallback to SendInput
    ::SetCursorPos(static_cast<int>(x), static_cast<int>(y));
    UpdateButtons(btns.bits);
  }

  // Mouse relative position
  void InjectMouseRel(float dx, float dy, const remote::proto::Buttons& btns) override {
    if (use_hiddriver_ && hiddriver_) {
      BYTE button_flags = ConvertMouseButtons(btns.bits);
      BYTE rel_x = static_cast<BYTE>(std::clamp(dx, static_cast<float>(RELATIVE_MOUSE_MIN_COORDINATE), static_cast<float>(RELATIVE_MOUSE_MAX_COORDINATE)));
      BYTE rel_y = static_cast<BYTE>(std::clamp(dy, static_cast<float>(RELATIVE_MOUSE_MIN_COORDINATE), static_cast<float>(RELATIVE_MOUSE_MAX_COORDINATE)));
      
      if (vmulti_update_relative_mouse(hiddriver_, button_flags, rel_x, rel_y, 0)) {
        last_btns_ = btns.bits;
        return;
      }
    }
    
    // Fallback to SendInput
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = static_cast<LONG>(dx);
    in.mi.dy = static_cast<LONG>(dy);
    ::SendInput(1, &in, sizeof(INPUT));
    UpdateButtons(btns.bits);
  }

  // Wheel
  void InjectWheel(float dx, float dy) override {
    if (dy != 0.0f) {
      INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_WHEEL; in.mi.mouseData = static_cast<DWORD>(dy * WHEEL_DELTA);
      ::SendInput(1, &in, sizeof(INPUT));
    }
    if (dx != 0.0f) {
      INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = 0x0800; in.mi.mouseData = static_cast<DWORD>(dx * WHEEL_DELTA);
      ::SendInput(1, &in, sizeof(INPUT));
    }
  }

  void SetIme(const remote::proto::ImeStateMsg&) override {}
  void InjectGamepad(const remote::proto::GamepadMsg&) override {}

  // XInput injection (ViGEm)
  void InjectGamepadXInput(uint16_t buttons, float lx, float ly, float rx, float ry, float lt, float rt) override {
    if (!vigem_.Init()) return;
    XUSB_REPORT rep{};
    rep.wButtons = buttons;
    rep.bLeftTrigger = static_cast<uint8_t>(std::clamp(lt, 0.0f, 1.0f) * 255.0f);
    rep.bRightTrigger = static_cast<uint8_t>(std::clamp(rt, 0.0f, 1.0f) * 255.0f);
    rep.sThumbLX = static_cast<int16_t>(std::clamp(lx, -1.0f, 1.0f) * 32767.0f);
    rep.sThumbLY = static_cast<int16_t>(std::clamp(ly, -1.0f, 1.0f) * 32767.0f);
    rep.sThumbRX = static_cast<int16_t>(std::clamp(rx, -1.0f, 1.0f) * 32767.0f);
    rep.sThumbRY = static_cast<int16_t>(std::clamp(ry, -1.0f, 1.0f) * 32767.0f);
    vigem_.UpdateX360(rep);
  }

 private:
  // Key name mapping (loose matching)
  WORD MapKey(const remote::proto::KeyboardMsg& ev) {
    auto upper_no_space = [](std::string s) {
      std::string t; t.reserve(s.size());
      for (char c : s) { if (c!=' ' && c!='-' && c!='_') t.push_back(static_cast<char>(::toupper(static_cast<unsigned char>(c)))); }
      return t;
    };

    if (!ev.key.empty()) {
      std::string k = upper_no_space(ev.key);
      if (k.size()==1) {
        char c=k[0];
        if (c>='A'&&c<='Z') return static_cast<WORD>(c);
        if (c>='0'&&c<='9') return static_cast<WORD>(c);
        // Punctuation and OEM keys
        switch (c) {
          case '`': case '~': return VK_OEM_3;     // ` ~
          case '-': case '_': return VK_OEM_MINUS; // - _
          case '=': case '+': return VK_OEM_PLUS;  // = +
          case '[': case '{': return VK_OEM_4;     // [ {
          case ']': case '}': return VK_OEM_6;     // ] }
          case '\\': case '|': return VK_OEM_5;   // \ |
          case ';': case ':': return VK_OEM_1;     // ; :
          case '\'': case '"': return VK_OEM_7;   // ' "
          case ',': case '<': return VK_OEM_COMMA; // , <
          case '.': case '>': return VK_OEM_PERIOD;// . >
          case '/': case '?': return VK_OEM_2;     // / ?
          default: break;
        }
      }
      if (k.find("CTRL")!=std::string::npos || k=="CONTROL") return VK_CONTROL;
      if (k.find("SHIFT")!=std::string::npos) return VK_SHIFT;
      if (k.find("ALT")!=std::string::npos) return VK_MENU;
      if (k.find("GUI")!=std::string::npos || k.find("WIN")!=std::string::npos) return VK_LWIN;
      if (k=="LEFT"||k=="ARROWLEFT") return VK_LEFT; if (k=="RIGHT"||k=="ARROWRIGHT") return VK_RIGHT;
      if (k=="UP"||k=="ARROWUP") return VK_UP; if (k=="DOWN"||k=="ARROWDOWN") return VK_DOWN;
      if (k=="ENTER"||k=="RETURN") return VK_RETURN; if (k=="ESC"||k=="ESCAPE") return VK_ESCAPE;
      if (k=="SPACE"||k=="SPACEBAR") return VK_SPACE; if (k=="TAB") return VK_TAB;
      if (k=="BACKSPACE"||k=="BACK") return VK_BACK; if (k=="DELETE"||k=="DEL") return VK_DELETE;
      if (k=="INSERT"||k=="INS") return VK_INSERT; if (k=="HOME") return VK_HOME; if (k=="END") return VK_END;
      if (k=="PAGEUP") return VK_PRIOR; if (k=="PAGEDOWN") return VK_NEXT;
      if (k=="CAPSLOCK") return VK_CAPITAL; if (k=="NUMLOCK") return VK_NUMLOCK; if (k=="SCROLLLOCK") return VK_SCROLL;
      if (k=="PRINTSCREEN"||k=="PRTSC") return VK_SNAPSHOT; if (k=="PAUSE"||k=="BREAK") return VK_PAUSE;
      // SDL name (remove spaces and uppercase) to OEM key
      if (k=="GRAVE") return VK_OEM_3;
      if (k=="MINUS") return VK_OEM_MINUS;
      if (k=="EQUALS"||k=="EQUAL") return VK_OEM_PLUS;
      if (k=="LEFTBRACKET") return VK_OEM_4;
      if (k=="RIGHTBRACKET") return VK_OEM_6;
      if (k=="BACKSLASH") return VK_OEM_5;
      if (k=="SEMICOLON") return VK_OEM_1;
      if (k=="APOSTROPHE"||k=="QUOTE") return VK_OEM_7;
      if (k=="COMMA") return VK_OEM_COMMA;
      if (k=="PERIOD"||k=="DOT") return VK_OEM_PERIOD;
      if (k=="SLASH") return VK_OEM_2;
      if (k=="NONUSBACKSLASH"||k=="OEM102"||k=="LESS"||k=="GREATER") return VK_OEM_102;
      if (k.size()>=2 && k[0]=='F') { int num=0; try{ num=std::stoi(k.substr(1)); }catch(...){} if (num>=1&&num<=24) return static_cast<WORD>(VK_F1+(num-1)); }
    }
    if (ev.code>='A'&&ev.code<='Z') return static_cast<WORD>(ev.code);
    if (ev.code>='0'&&ev.code<='9') return static_cast<WORD>(ev.code);

    // Map common SDL virtual keycodes (>= 0x40000000) to Windows VK codes
    switch (ev.code) {
      case SDLK_UP: return VK_UP;
      case SDLK_DOWN: return VK_DOWN;
      case SDLK_LEFT: return VK_LEFT;
      case SDLK_RIGHT: return VK_RIGHT;
      case SDLK_HOME: return VK_HOME;
      case SDLK_END: return VK_END;
      case SDLK_PAGEUP: return VK_PRIOR;
      case SDLK_PAGEDOWN: return VK_NEXT;
      case SDLK_INSERT: return VK_INSERT;
      case SDLK_DELETE: return VK_DELETE;
      case SDLK_CAPSLOCK: return VK_CAPITAL;
      case SDLK_NUMLOCKCLEAR: return VK_NUMLOCK;
      case SDLK_SCROLLLOCK: return VK_SCROLL;
      case SDLK_PRINTSCREEN: return VK_SNAPSHOT;
      case SDLK_PAUSE: return VK_PAUSE;
      case SDLK_LSHIFT: case SDLK_RSHIFT: return VK_SHIFT;
      case SDLK_LCTRL: case SDLK_RCTRL: return VK_CONTROL;
      case SDLK_LALT: case SDLK_RALT: return VK_MENU;
      case SDLK_LGUI: case SDLK_RGUI: return VK_LWIN;
      default:
        break;
    }

    // Backward mapping of SDL keycode ASCII value to punctuation
    switch (ev.code) {
      case '`': case '~': return VK_OEM_3;     // ` ~
      case '-': case '_': return VK_OEM_MINUS; // - _
      case '=': case '+': return VK_OEM_PLUS;  // = +
      case '[': case '{': return VK_OEM_4;     // [ {
      case ']': case '}': return VK_OEM_6;     // ] }
      case '\\': case '|': return VK_OEM_5;   // \ |
      case ';': case ':': return VK_OEM_1;     // ; :
      case '\'': case '"': return VK_OEM_7;   // ' "
      case ',': case '<': return VK_OEM_COMMA; // , <
      case '.': case '>': return VK_OEM_PERIOD;// . >
      case '/': case '?': return VK_OEM_2;     // / ?
      default: break;
    }
    return 0;
  }

  bool IsExtendedKey(WORD vk) const {
    switch (vk) {
      case VK_UP:
      case VK_DOWN:
      case VK_LEFT:
      case VK_RIGHT:
      case VK_HOME:
      case VK_END:
      case VK_PRIOR:  // Page Up
      case VK_NEXT:   // Page Down
      case VK_INSERT:
      case VK_DELETE:
      case VK_DIVIDE: // Numpad divide, if using scan codes
      case VK_NUMLOCK:
      case VK_RCONTROL:
      case VK_RMENU:  // Right Alt
      case VK_LWIN:
      case VK_RWIN:
      case VK_APPS:
        return true;
      default:
        return false;
    }
  }

  void UpdateButtons(uint32_t btns) {
    struct M { uint32_t bit; DWORD down; DWORD up; } map[] = {
        {1u<<0, MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP},
        {1u<<1, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP},
        {1u<<2, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP},
        {1u<<3, MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP},
        {1u<<4, MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP},
    };
    for (auto& m : map) {
      uint32_t was = (last_btns_ & m.bit);
      uint32_t now = (btns & m.bit);
      if (was == now) continue;
      INPUT in{}; in.type = INPUT_MOUSE;
      if (now) { in.mi.dwFlags = m.down; if (m.down==MOUSEEVENTF_XDOWN) in.mi.mouseData = (m.bit==(1u<<3)?XBUTTON1:XBUTTON2); }
      else { in.mi.dwFlags = m.up; if (m.up==MOUSEEVENTF_XUP) in.mi.mouseData = (m.bit==(1u<<3)?XBUTTON1:XBUTTON2); }
      ::SendInput(1, &in, sizeof(INPUT));
    }
    last_btns_ = btns;
  }

  // Convert button bits to vmulti format
  BYTE ConvertMouseButtons(uint32_t btns) const {
    BYTE result = 0;
    if (btns & (1u << 0)) result |= MOUSE_BUTTON_1;  // Left
    if (btns & (1u << 2)) result |= MOUSE_BUTTON_2;  // Right
    if (btns & (1u << 1)) result |= MOUSE_BUTTON_3;  // Middle
    return result;
  }

  // Map SDL keycode to USB HID scancode
  // Reference: http://www.usb.org/developers/devclass_docs/Hut1_11.pdf
  BYTE MapKeyToHIDScancode(const remote::proto::KeyboardMsg& ev) {
    // Handle modifiers separately (these are tracked in shift flags, not key codes)
    switch (ev.code) {
      case SDLK_LCTRL: case SDLK_RCTRL:
      case SDLK_LSHIFT: case SDLK_RSHIFT:
      case SDLK_LALT: case SDLK_RALT:
      case SDLK_LGUI: case SDLK_RGUI:
        return 0;  // Handled by modifier flags
    }
    
    // Letters A-Z
    if (ev.code >= 'a' && ev.code <= 'z') return 0x04 + (ev.code - 'a');
    if (ev.code >= 'A' && ev.code <= 'Z') return 0x04 + (ev.code - 'A');
    
    // Numbers 1-9, 0
    if (ev.code >= '1' && ev.code <= '9') return 0x1E + (ev.code - '1');
    if (ev.code == '0') return 0x27;
    
    // Special keys
    switch (ev.code) {
      case SDLK_RETURN: return 0x28;
      case SDLK_ESCAPE: return 0x29;
      case SDLK_BACKSPACE: return 0x2A;
      case SDLK_TAB: return 0x2B;
      case SDLK_SPACE: return 0x2C;
      case SDLK_MINUS: return 0x2D;
      case SDLK_EQUALS: return 0x2E;
      case SDLK_LEFTBRACKET: return 0x2F;
      case SDLK_RIGHTBRACKET: return 0x30;
      case SDLK_BACKSLASH: return 0x31;
      case SDLK_SEMICOLON: return 0x33;
      case SDLK_APOSTROPHE: return 0x34;
      case SDLK_GRAVE: return 0x35;
      case SDLK_COMMA: return 0x36;
      case SDLK_PERIOD: return 0x37;
      case SDLK_SLASH: return 0x38;
      case SDLK_CAPSLOCK: return 0x39;
      
      // Function keys F1-F12
      case SDLK_F1: return 0x3A;
      case SDLK_F2: return 0x3B;
      case SDLK_F3: return 0x3C;
      case SDLK_F4: return 0x3D;
      case SDLK_F5: return 0x3E;
      case SDLK_F6: return 0x3F;
      case SDLK_F7: return 0x40;
      case SDLK_F8: return 0x41;
      case SDLK_F9: return 0x42;
      case SDLK_F10: return 0x43;
      case SDLK_F11: return 0x44;
      case SDLK_F12: return 0x45;
      
      // Navigation keys
      case SDLK_PRINTSCREEN: return 0x46;
      case SDLK_SCROLLLOCK: return 0x47;
      case SDLK_PAUSE: return 0x48;
      case SDLK_INSERT: return 0x49;
      case SDLK_HOME: return 0x4A;
      case SDLK_PAGEUP: return 0x4B;
      case SDLK_DELETE: return 0x4C;
      case SDLK_END: return 0x4D;
      case SDLK_PAGEDOWN: return 0x4E;
      case SDLK_RIGHT: return 0x4F;
      case SDLK_LEFT: return 0x50;
      case SDLK_DOWN: return 0x51;
      case SDLK_UP: return 0x52;
      
      // Numpad
      case SDLK_NUMLOCKCLEAR: return 0x53;
      case SDLK_KP_DIVIDE: return 0x54;
      case SDLK_KP_MULTIPLY: return 0x55;
      case SDLK_KP_MINUS: return 0x56;
      case SDLK_KP_PLUS: return 0x57;
      case SDLK_KP_ENTER: return 0x58;
      case SDLK_KP_1: return 0x59;
      case SDLK_KP_2: return 0x5A;
      case SDLK_KP_3: return 0x5B;
      case SDLK_KP_4: return 0x5C;
      case SDLK_KP_5: return 0x5D;
      case SDLK_KP_6: return 0x5E;
      case SDLK_KP_7: return 0x5F;
      case SDLK_KP_8: return 0x60;
      case SDLK_KP_9: return 0x61;
      case SDLK_KP_0: return 0x62;
      case SDLK_KP_PERIOD: return 0x63;
      
      // Additional keys
      case SDLK_APPLICATION: return 0x65;
      
      default: return 0;
    }
  }

  // Check if a key is a modifier key
  bool IsModifierKey(int code) const {
    switch (code) {
      case SDLK_LCTRL: case SDLK_RCTRL:
      case SDLK_LSHIFT: case SDLK_RSHIFT:
      case SDLK_LALT: case SDLK_RALT:
      case SDLK_LGUI: case SDLK_RGUI:
        return true;
      default:
        return false;
    }
  }
  
  // Get the HID modifier bit for a specific modifier key
  BYTE GetHidModifierBit(int code) const {
    switch (code) {
      case SDLK_LCTRL:  return KBD_LCONTROL_BIT;
      case SDLK_RCTRL:  return KBD_RCONTROL_BIT;
      case SDLK_LSHIFT: return KBD_LSHIFT_BIT;
      case SDLK_RSHIFT: return KBD_RSHIFT_BIT;
      case SDLK_LALT:   return KBD_LALT_BIT;
      case SDLK_RALT:   return KBD_RALT_BIT;
      case SDLK_LGUI:   return KBD_LGUI_BIT;
      case SDLK_RGUI:   return KBD_RGUI_BIT;
      default:
        return 0;
    }
  }

  // Update key state for vmulti keyboard reports (non-modifier keys only)
  void UpdateKeyState(BYTE hid_code, bool is_down) {
    if (is_down) {
      // Add to pressed keys if not already present
      if (std::find(pressed_keys_.begin(), pressed_keys_.end(), hid_code) == pressed_keys_.end()) {
        if (pressed_keys_.size() < KBD_KEY_CODES) {
          pressed_keys_.push_back(hid_code);
        }
      }
    } else {
      // Remove from pressed keys
      pressed_keys_.erase(
        std::remove(pressed_keys_.begin(), pressed_keys_.end(), hid_code),
        pressed_keys_.end()
      );
    }
  }

  // Member variables
  ViGEmDyn vigem_;
  uint32_t last_btns_{0};
  
  // hiddriver support
  pvmulti_client hiddriver_{nullptr};
  bool use_hiddriver_{false};
  std::vector<BYTE> pressed_keys_;
  BYTE hid_modifiers_{0}; // New member for HID modifier bits
};

}  // namespace windows
}  // namespace platform
}  // namespace remote

#endif  // _WIN32

#endif  // REMOTE_PLATFORM_WINDOWS_INPUT_INJECTOR_WIN_H_
