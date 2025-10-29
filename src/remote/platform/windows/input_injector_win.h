// Description: Windows platform input injector implementation
// - Keyboard/mouse: SendInput/SetCursorPos
// - Gamepad: dynamically load ViGEmClient.dll to inject XInput (X360) messages

#ifndef REMOTE_PLATFORM_WINDOWS_INPUT_INJECTOR_WIN_H_
#define REMOTE_PLATFORM_WINDOWS_INPUT_INJECTOR_WIN_H_

#ifdef _WIN32

#include <windows.h>
#include <cstdint>
#include <string>
#include <algorithm>

#include "remote/input_receiver/input_injector.h"

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
  WindowsInputInjector() {}
  ~WindowsInputInjector() {}

  // Keyboard injection
  void InjectKeyboard(const remote::proto::KeyboardMsg& ev) override {
    WORD vk = MapKey(ev);
    if (vk == 0) return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = ev.down ? 0 : KEYEVENTF_KEYUP;
    ::SendInput(1, &in, sizeof(INPUT));
  }

  // Mouse absolute position
  void InjectMouseAbs(float x, float y, const remote::proto::Buttons& btns) override {
    ::SetCursorPos(static_cast<int>(x), static_cast<int>(y));
    UpdateButtons(btns.bits);
  }

  // Mouse relative position
  void InjectMouseRel(float dx, float dy, const remote::proto::Buttons& btns) override {
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

  ViGEmDyn vigem_;
  uint32_t last_btns_{0};
};

}  // namespace windows
}  // namespace platform
}  // namespace remote

#endif  // _WIN32

#endif  // REMOTE_PLATFORM_WINDOWS_INPUT_INJECTOR_WIN_H_
