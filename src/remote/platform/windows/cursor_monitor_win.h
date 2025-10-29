// Description: Windows cursor image monitoring, periodic polling, sending cursorImage JSON when changed

#ifndef REMOTE_PLATFORM_WINDOWS_CURSOR_MONITOR_WIN_H_
#define REMOTE_PLATFORM_WINDOWS_CURSOR_MONITOR_WIN_H_

#ifdef _WIN32

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "remote/proto/base64.h"
#include "remote/proto/messages.h"
#include "remote/proto/protobuf_serializer.h"

namespace remote {
namespace platform {
namespace windows {

class CursorMonitorWin {
 public:
  using Sender = std::function<bool(const std::vector<uint8_t>&)>;

  CursorMonitorWin() = default;
  ~CursorMonitorWin() { Stop(); }

  void SetSender(Sender s) { sender_ = std::move(s); }

  void Start() {
    if (running_.exchange(true))
      return;
    th_ = std::thread([this]() { this->Loop(); });
  }
  void Stop() {
    if (!running_.exchange(false))
      return;
    if (th_.joinable())
      th_.join();
  }

 private:
  void Loop() {
    std::string last_sig;
    for (; running_.load();) {
      remote::proto::CursorImageMsg msg;
      if (Capture(msg)) {
        std::string sig = Signature(msg);
        if (sig != last_sig) {
          last_sig = sig;
          Send(msg);
        }
      }
      Sleep(300);
    }
  }

  bool Send(const remote::proto::CursorImageMsg& msg) {
    if (!sender_)
      return false;
#ifdef REMOTE_USE_PROTOBUF
    auto pb = remote::proto::PbSerializeCursorImage(msg);
    if (!pb.empty())
      return sender_(pb);
#endif
    // Fallback JSON: {"type":"cursorImage",...}
    std::vector<uint8_t> bytes;
    bytes.reserve(128 + msg.rgba.size() * 4 / 3);
    auto b64 = remote::proto::Base64Encode(msg.rgba);
    std::string s =
        "{\"type\":\"cursorImage\",\"w\":" + std::to_string(msg.w) +
        ",\"h\":" + std::to_string(msg.h) +
        ",\"hotspotX\":" + std::to_string(msg.hotspotX) +
        ",\"hotspotY\":" + std::to_string(msg.hotspotY) +
        ",\"fmt\":\"BGRA\",\"visible\":" + (msg.visible ? "true" : "false") +
        ",\"data\":\"" + b64 + "\"}";
    bytes.assign(s.begin(), s.end());
    return sender_(bytes);
  }

  // Capture current system cursor as RGBA (BGRA32) + hotspot
  bool Capture(remote::proto::CursorImageMsg& out) {
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci))
      return false;
    out.visible = (ci.flags == CURSOR_SHOWING);

    HICON hIcon = (HICON)CopyIcon(ci.hCursor);
    if (!hIcon)
      return false;

    ICONINFO ii{};
    if (!GetIconInfo(hIcon, &ii)) {
      DestroyIcon(hIcon);
      return false;
    }
    out.hotspotX = static_cast<int>(ii.xHotspot);
    out.hotspotY = static_cast<int>(ii.yHotspot);

    // Calculate width and height: color bitmap preferred; monochrome cursor uses half the mask height
    int width = 0, height = 0;
    BITMAP bm{};
    if (ii.hbmColor) {
      if (GetObject(ii.hbmColor, sizeof(bm), &bm)) {
        width = bm.bmWidth;
        height = bm.bmHeight;
      }
    } else if (ii.hbmMask) {
      if (GetObject(ii.hbmMask, sizeof(bm), &bm)) {
        width = bm.bmWidth;
        height = bm.bmHeight / 2;
      }
    }
    if (width <= 0 || height <= 0) {
      CleanupIcon(ii, hIcon);
      return false;
    }

    out.w = width;
    out.h = height;
    out.rgba.resize(static_cast<size_t>(out.w) * out.h * 4);

    // Use DrawIconEx to render to 32-bit DIB (BGRA), handle both color and monochrome double mask cursors uniformly
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = out.w;
    bmi.bmiHeader.biHeight = -out.h;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hdc || !dib || !bits) {
      if (dib)
        DeleteObject(dib);
      if (hdc)
        DeleteDC(hdc);
      CleanupIcon(ii, hIcon);
      return false;
    }
    HGDIOBJ old = SelectObject(hdc, dib);
    // Clear background to fully transparent
    memset(bits, 0x00, static_cast<size_t>(out.w) * out.h * 4);
    // Draw system cursor to DIB
    DrawIconEx(hdc, 0, 0, hIcon, out.w, out.h, 0, NULL, DI_NORMAL);
    // Copy BGRA pixels (first copy RGB, then correct alpha)
    memcpy(out.rgba.data(), bits, static_cast<size_t>(out.w) * out.h * 4);

    // If there is a mask (double-color or GDI does not write alpha), use AND/XOR mask to complete alpha;
    // Visible condition: (AND==0) or (XOR==1). Transparent: (AND==1 and XOR==0).
    if (ii.hbmMask) {
      const int mask_stride =
          ((out.w + 31) / 32) * 4;  // Bytes per row (1bpp, 32-aligned)
      const int rows = out.h * 2;   // Top AND, bottom XOR
      std::vector<uint8_t> mb(static_cast<size_t>(mask_stride) * rows);

      BITMAPINFO bmim{};
      bmim.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmim.bmiHeader.biWidth = out.w;
      bmim.bmiHeader.biHeight = -rows;  // top-down
      bmim.bmiHeader.biPlanes = 1;
      bmim.bmiHeader.biBitCount = 1;
      bmim.bmiHeader.biCompression = BI_RGB;

      HDC hdc_mask = GetDC(NULL);
      GetDIBits(hdc_mask, ii.hbmMask, 0, rows, mb.data(), &bmim,
                DIB_RGB_COLORS);
      ReleaseDC(NULL, hdc_mask);

      auto bit_at = [&](int row, int x) -> int {
        const uint8_t* rp = mb.data() + row * mask_stride;
        int byte_index = x >> 3;
        int bit_index = 7 - (x & 7);
        return (rp[byte_index] >> bit_index) & 1;
      };

      // Note: Monochrome cursor drawing rules
      // D' = (D AND A) XOR X (D is background pixel, A is AND mask, X is XOR mask)
      // Combination:
      //  A=1,X=0 -> transparent, A=0,X=0 -> black, A=0,X=1 -> white, A=1,X=1 -> inverted
      // Background is unknown, so strict reproduction in one pass is not possible. The following two stages prioritize "both black and white pixels":
      //  1) Pixel classification: transparent / black / white / inverted
      //  2) Invert is colored according to the neighborhood of Black/White, otherwise assign black and white in a checkerboard pattern

      enum class PK : uint8_t { T, K, W, I };
      std::vector<PK> kinds(static_cast<size_t>(out.w) * out.h, PK::T);

      // First stage: classification
      for (int y = 0; y < out.h; ++y) {
        int r_and = y;
        int r_xor = y + out.h;
        for (int x = 0; x < out.w; ++x) {
          int andb = bit_at(r_and, x);
          int xorb = bit_at(r_xor, x);
          PK v = PK::T;
          if (andb == 1 && xorb == 0)
            v = PK::T;  // transparent
          else if (andb == 0 && xorb == 0)
            v = PK::K;  // black
          else if (andb == 0 && xorb == 1)
            v = PK::W;  // white
          else          /* (andb == 1 && xorb == 1) */
            v = PK::I;  // inverted
          kinds[static_cast<size_t>(y) * out.w + x] = v;
        }
      }

      auto has_neighbor = [&](int x, int y, PK target) {
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0)
              continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= out.w || ny >= out.h)
              continue;
            if (kinds[static_cast<size_t>(ny) * out.w + nx] == target)
              return true;
          }
        }
        return false;
      };

      // Second stage: color output
      for (int y = 0; y < out.h; ++y) {
        for (int x = 0; x < out.w; ++x) {
          PK v = kinds[static_cast<size_t>(y) * out.w + x];
          size_t p =
              static_cast<size_t>(y) * out.w * 4 + static_cast<size_t>(x) * 4;
          uint8_t* B = &out.rgba[p + 0];
          uint8_t* G = &out.rgba[p + 1];
          uint8_t* R = &out.rgba[p + 2];
          uint8_t* A = &out.rgba[p + 3];

          if (v == PK::T) {
            *A = 0;
            continue;
          }

          if (v == PK::K) {
            if (ii.hbmColor == NULL) {
              *R = 0;
              *G = 0;
              *B = 0;
            }
            *A = 255;
            continue;
          }
          if (v == PK::W) {
            if (ii.hbmColor == NULL) {
              *R = 255;
              *G = 255;
              *B = 255;
            }
            *A = 255;
            continue;
          }

          // v == PK::I (inverted): if there is black in the neighborhood, white, if there is white in the neighborhood, black, if neither, a checkerboard
          bool nb_black = has_neighbor(x, y, PK::K);
          bool nb_white = has_neighbor(x, y, PK::W);
          bool use_white = false;
          if (nb_black && !nb_white)
            use_white = true;
          else if (!nb_black && nb_white)
            use_white = false;
          else
            use_white = (((x ^ y) & 1) == 0);  // checkerboard alternating white/black

          if (use_white) {
            if (ii.hbmColor == NULL) {
              *R = 255;
              *G = 255;
              *B = 255;
            }
            *A = 255;
          } else {
            if (ii.hbmColor == NULL) {
              *R = 0;
              *G = 0;
              *B = 0;
            }
            *A = 255;
          }
        }
      }
    } else {
      // When there is no mask but all alpha is 0, use RGB non-zero as the opaque condition
      bool any_alpha = false;
      for (size_t i = 3; i < out.rgba.size(); i += 4) {
        if (out.rgba[i]) {
          any_alpha = true;
          break;
        }
      }
      if (!any_alpha) {
        for (size_t p = 0; p < out.rgba.size(); p += 4) {
          uint8_t b = out.rgba[p + 0], g = out.rgba[p + 1], r = out.rgba[p + 2];
          out.rgba[p + 3] = (r | g | b) ? 255 : 0;
        }
      }
    }
    // Clean up GDI resources
    SelectObject(hdc, old);
    DeleteObject(dib);
    DeleteDC(hdc);

    CleanupIcon(ii, hIcon);
    return true;
  }

  void CleanupIcon(ICONINFO& ii, HICON hIcon) {
    if (ii.hbmColor)
      DeleteObject(ii.hbmColor);
    if (ii.hbmMask)
      DeleteObject(ii.hbmMask);
    if (hIcon)
      DestroyIcon(hIcon);
  }

  std::string Signature(const remote::proto::CursorImageMsg& m) {
    // Simple signature: size + hotspot + visible + first 64 bytes hash
    uint32_t h = 2166136261u;
    auto mix = [&](uint32_t v) {
      h ^= v;
      h *= 16777619u;
    };
    mix(static_cast<uint32_t>(m.w));
    mix(static_cast<uint32_t>(m.h));
    mix(static_cast<uint32_t>(m.hotspotX));
    mix(static_cast<uint32_t>(m.hotspotY));
    mix(static_cast<uint32_t>(m.visible));
    size_t n = std::min<size_t>(64, m.rgba.size());
    for (size_t i = 0; i < n; ++i)
      mix(m.rgba[i]);
    return std::to_string(h);
  }

  std::atomic<bool> running_{false};
  std::thread th_;
  Sender sender_;
};

}  // namespace windows
}  // namespace platform
}  // namespace remote

#endif  // _WIN32

#endif  // REMOTE_PLATFORM_WINDOWS_CURSOR_MONITOR_WIN_H_
