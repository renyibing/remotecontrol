// Description: Windows IME state monitoring (IMM/TIS simplified), periodic polling reporting

#ifndef REMOTE_PLATFORM_WINDOWS_IME_MONITOR_WIN_H_
#define REMOTE_PLATFORM_WINDOWS_IME_MONITOR_WIN_H_

#ifdef _WIN32

#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")

#include <atomic>
#include <thread>
#include <functional>
#include <string>

#include "remote/proto/messages.h"
#include "remote/proto/serializer.h"

namespace remote {
namespace platform {
namespace windows {

class ImeMonitorWin {
 public:
  using Sender = std::function<bool(const std::vector<uint8_t>&)>;

  ImeMonitorWin() = default;
  ~ImeMonitorWin() { Stop(); }

  void SetSender(Sender s) { sender_ = std::move(s); }

  void Start() {
    if (running_.exchange(true)) return;
    th_ = std::thread([this]() { this->Loop(); });
  }
  void Stop() {
    if (!running_.exchange(false)) return;
    if (th_.joinable()) th_.join();
  }

 private:
  void Loop() {
    bool last_open = false;
    std::string last_lang;
    for (; running_.load(); ) {
      bool open = QueryOpen();
      std::string lang = QueryLang();
      if (!init_) { init_ = true; last_open = open; last_lang = lang; Send(open, lang); }
      else if (open != last_open || lang != last_lang) { last_open = open; last_lang = lang; Send(open, lang); }
      Sleep(300);
    }
  }

  bool Send(bool open, const std::string& lang) {
    if (!sender_) return false;
    remote::proto::ImeStateMsg m; m.open = open; m.lang = lang;
    return sender_(remote::proto::SerializeImeState(m));
  }

  bool QueryOpen() {
    // Get the IME status of the foreground window (simplified)
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    HIMC imc = ImmGetContext(hwnd);
    if (!imc) return false;
    BOOL st = ImmGetOpenStatus(imc);
    ImmReleaseContext(hwnd, imc);
    return st ? true : false;
  }

  std::string QueryLang() {
    HKL hkl = GetKeyboardLayout(0);
    // The lower word is the language ID
    LANGID lid = LOWORD(hkl);
    switch (PRIMARYLANGID(lid)) {
      case LANG_CHINESE: return "zh-CN"; // Simplified: no distinction between traditional and simplified
      case LANG_JAPANESE: return "ja-JP";
      case LANG_KOREAN: return "ko-KR";
      case LANG_ENGLISH: return "en-US";
      default: return "";
    }
  }

  std::atomic<bool> running_{false};
  std::thread th_;
  bool init_{false};
  Sender sender_;
};

}  // namespace windows
}  // namespace platform
}  // namespace remote

#endif  // _WIN32

#endif  // REMOTE_PLATFORM_WINDOWS_IME_MONITOR_WIN_H_
