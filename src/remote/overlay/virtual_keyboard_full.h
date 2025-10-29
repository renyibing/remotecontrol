// Description: Full-size virtual keyboard (SDL3). Draw labels with 5x7 built-in font, and send keyboard events when clicked.
#ifndef REMOTE_OVERLAY_VIRTUAL_KEYBOARD_FULL_H_
#define REMOTE_OVERLAY_VIRTUAL_KEYBOARD_FULL_H_

#include <SDL3/SDL.h>
#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "remote/proto/messages.h"
#include "remote/proto/protobuf_serializer.h"
#include "remote/proto/serializer.h"

namespace remote {
namespace overlay {

// Draw 5x7 glyphs (LSB is left-most). No horizontal flip.
static inline void VK_DrawGlyphFixed(SDL_Renderer* r,
                                     float x,
                                     float y,
                                     float sx,
                                     float sy,
                                     const uint8_t* g,
                                     Uint8 a) {
  if (!g)
    return;
  SDL_SetRenderDrawColor(r, 30, 30, 30, a);
  for (int row = 0; row < 7; ++row) {
    uint8_t bits = g[row];
    for (int col = 0; col < 5; ++col) {
      if (bits & (1u << col)) {
        SDL_FRect px{x + col * sx, y + row * sy, sx * 0.9f, sy * 0.9f};
        SDL_RenderFillRect(r, &px);
      }
    }
  }
}

class VirtualKeyboardFull {
 public:
  using Sender = std::function<bool(const std::vector<uint8_t>&)>;

  void SetSender(Sender s) { sender_ = std::move(s); }
  void SetOpacity(float a) {
    if (a < 0.f)
      a = 0.f;
    if (a > 1.f)
      a = 1.f;
    alpha_ = a;
  }

  // Call every frame. Draw after layout is determined.
  void Render(SDL_Renderer* r) {
    EnsureLayout(r);
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r, &w, &h);
    float kb_height = h * 0.4f;
    SDL_SetRenderDrawColor(r, 20, 20, 20, (Uint8)(alpha_ * 180));
    SDL_FRect bg{6.0f, h - kb_height - 14.0f, (float)w - 12.0f,
                 kb_height + 8.0f};
    SDL_RenderFillRect(r, &bg);
    for (auto& k : keys_) {
      if (k.pressed || (k.is_mod && mod_latched_ && (k.label == "Shift")))
        SDL_SetRenderDrawColor(r, 140, 180, 240, (Uint8)(alpha_ * 255));
      else
        SDL_SetRenderDrawColor(r, 200, 200, 200, (Uint8)(alpha_ * 255));
      SDL_RenderFillRect(r, &k.rect);
      DrawKeyLabel(r, k);
    }
  }

  // Mouse event processing
  bool OnMouseDown(float x, float y) {
    for (int i = 0; i < (int)keys_.size(); ++i) {
      auto& k = keys_[i];
      if (!Inside(k.rect, x, y))
        continue;
      // One-shot '_' and '|'
      if (!k.is_mod && (k.label == "_" || k.label == "|")) {
        if (k.label == "_")
          SendComboUnderscore();
        else
          SendComboPipe();
        return true;
      }
      if (k.is_mod) {
        if (k.label == "Shift") {
          mod_latched_ = !mod_latched_;
          k.pressed = mod_latched_;
          if (mod_latched_)
            SendDown(k);
          else
            SendUp(k);
        } else {
          k.pressed = !k.pressed;
          if (k.pressed)
            SendDown(k);
          else
            SendUp(k);
        }
        return true;
      }
      SendDown(k);
      k.pressed = true;
      last_pressed_ = i;
      return true;
    }
    return false;
  }

  bool OnMouseUp(float, float) {
    if (!last_pressed_.has_value())
      return false;
    int i = *last_pressed_;
    if (i >= 0 && i < (int)keys_.size()) {
      SendUp(keys_[i]);
      keys_[i].pressed = false;
    }
    last_pressed_.reset();
    return true;
  }

 private:
  struct Key {
    SDL_FRect rect;
    std::string label;
    int code;
    bool is_mod{false};
    bool pressed{false};
  };

  std::vector<Key> keys_;
  std::optional<int> last_pressed_{};
  int lw_{0}, lh_{0};
  float alpha_{0.85f};
  bool mod_latched_{false};
  Sender sender_{};

  static bool Inside(const SDL_FRect& r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
  }

  void EnsureLayout(SDL_Renderer* r) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r, &w, &h);
    if (w == lw_ && h == lh_ && !keys_.empty())
      return;
    lw_ = w;
    lh_ = h;
    keys_.clear();
    float margin = 10.0f;
    float kb_height = h * 0.4f;
    float kb_y = h - kb_height - margin;
    float kb_x = margin;
    float kb_w = w - margin * 2.0f;
    float unit = kb_w / 15.0f;
    float row_h = kb_height / 6.0f;
    float y = kb_y;
    auto add = [&](float x, float y, float uw, const std::string& label,
                   int code, bool is_mod = false) {
      keys_.push_back({SDL_FRect{x, y, uw * unit - 3.0f, row_h - 3.0f}, label,
                       code, is_mod, false});
    };
    // Row 0: Esc + F1..F12
    add(kb_x + 0 * unit, y, 1.2f, "Escape", 0, true);
    for (int i = 0; i < 12; i++)
      add(kb_x + (1.4f + i * 1.1f) * unit, y, 1.0f,
          std::string("F") + std::to_string(i + 1), 0, false);
    y += row_h;
    // Row 1: ` 1..0 - = Backspace
    add(kb_x + 0 * unit, y, 1.0f, "`", '`');
    const char* nums = "1234567890";
    for (int i = 0; i < 10; i++) {
      std::string lab(1, nums[i]);
      add(kb_x + (1.1f + i * 1.0f) * unit, y, 1.0f, lab, nums[i]);
    }
    add(kb_x + (11.2f) * unit, y, 1.0f, "-", '-');
    add(kb_x + (12.2f) * unit, y, 1.0f, "=", '=');
    add(kb_x + (13.3f) * unit, y, 1.7f, "Backspace", 0, true);
    y += row_h;
    // Row 2: Tab Q..P [ ] backslash
    add(kb_x + 0 * unit, y, 1.6f, "Tab", 0, true);
    const char* rowQ = "QWERTYUIOP";
    for (int i = 0; i < 10; i++) {
      std::string lab(1, rowQ[i]);
      add(kb_x + (1.7f + i * 1.0f) * unit, y, 1.0f, lab, rowQ[i]);
    }
    add(kb_x + (11.8f) * unit, y, 1.0f, "[", '[');
    add(kb_x + (12.8f) * unit, y, 1.0f, "]", ']');
    add(kb_x + (13.8f) * unit, y, 1.2f, "\\", '\\');
    y += row_h;
    // Row 3: Caps A..L ; ' Enter
    add(kb_x + 0 * unit, y, 1.8f, "CapsLock", 0, true);
    const char* rowA = "ASDFGHJKL";
    for (int i = 0; i < 9; i++) {
      std::string lab(1, rowA[i]);
      add(kb_x + (1.9f + i * 1.0f) * unit, y, 1.0f, lab, rowA[i]);
    }
    add(kb_x + (11.0f) * unit, y, 1.0f, ";", ';');
    add(kb_x + (12.0f) * unit, y, 1.0f, "'", '\'');
    add(kb_x + (13.1f) * unit, y, 2.0f, "Enter", 0, true);
    y += row_h;
    // Row 4: Shift Z..M , . / Shift
    add(kb_x + 0 * unit, y, 2.3f, "Shift", 0, true);
    const char* rowZ = "ZXCVBNM";
    for (int i = 0; i < 7; i++) {
      std::string lab(1, rowZ[i]);
      add(kb_x + (2.4f + i * 1.0f) * unit, y, 1.0f, lab, rowZ[i]);
    }
    add(kb_x + (9.4f) * unit, y, 1.0f, ",", ',');
    add(kb_x + (10.4f) * unit, y, 1.0f, ".", '.');
    add(kb_x + (11.4f) * unit, y, 1.0f, "/", '/');
    add(kb_x + (12.5f) * unit, y, 2.5f, "Shift", 0, true);
    y += row_h;
    // Row 5: Ctrl Win Alt Space Alt Win Menu Ctrl '_' '|'
    add(kb_x + 0 * unit, y, 1.5f, "Ctrl", 0, true);
    add(kb_x + (1.6f) * unit, y, 1.2f, "Win", 0, true);
    add(kb_x + (2.9f) * unit, y, 1.2f, "Alt", 0, true);
    add(kb_x + (4.2f) * unit, y, 5.0f, "Space", ' ', false);
    add(kb_x + (9.3f) * unit, y, 1.2f, "Alt", 0, true);
    add(kb_x + (10.6f) * unit, y, 1.2f, "Win", 0, true);
    add(kb_x + (11.9f) * unit, y, 1.0f, "Menu", 0, true);
    add(kb_x + (13.2f) * unit, y, 0.8f, "Ctrl", 0, true);
    add(kb_x + (14.05f) * unit, y, 0.45f, "_", '_', false);
    add(kb_x + (14.55f) * unit, y, 0.45f, "|", '|', false);
  }

  // 5x7 font (A-Z, 0-9) and major symbols
  static const uint8_t* Glyph5x7(char c) {
    static const uint8_t font[][7] = {
        {0x0E, 0x15, 0x15, 0x15, 0x15, 0x0E, 0x00},  // 0
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x0E, 0x00},  // 1
        {0x0E, 0x11, 0x08, 0x04, 0x02, 0x1F, 0x00},  // 2
        {0x0E, 0x11, 0x0E, 0x10, 0x11, 0x0E, 0x00},  // 3
        {0x08, 0x0C, 0x0A, 0x09, 0x1F, 0x08, 0x00},  // 4
        {0x1F, 0x01, 0x0F, 0x10, 0x11, 0x0E, 0x00},  // 5
        {0x0E, 0x01, 0x0F, 0x11, 0x11, 0x0E, 0x00},  // 6
        {0x1F, 0x10, 0x08, 0x04, 0x02, 0x02, 0x00},  // 7
        {0x0E, 0x11, 0x0E, 0x11, 0x11, 0x0E, 0x00},  // 8
        {0x0E, 0x11, 0x1E, 0x10, 0x08, 0x0E, 0x00},  // 9
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00},  // A
        {0x0F, 0x11, 0x0F, 0x11, 0x11, 0x0F, 0x00},  // B
        {0x0E, 0x11, 0x01, 0x01, 0x11, 0x0E, 0x00},  // C
        {0x0F, 0x11, 0x11, 0x11, 0x11, 0x0F, 0x00},  // D
        {0x1F, 0x01, 0x0F, 0x01, 0x01, 0x1F, 0x00},  // E
        {0x1F, 0x01, 0x0F, 0x01, 0x01, 0x01, 0x00},  // F
        {0x0E, 0x11, 0x01, 0x1D, 0x11, 0x0E, 0x00},  // G
        {0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00},  // H
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00},  // I
        {0x1C, 0x08, 0x08, 0x08, 0x09, 0x06, 0x00},  // J
        {0x11, 0x09, 0x05, 0x05, 0x09, 0x11, 0x00},  // K
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x1F, 0x00},  // L
        {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x00},  // M
        {0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x00},  // N
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00},  // O
        {0x0F, 0x11, 0x0F, 0x01, 0x01, 0x01, 0x00},  // P
        {0x0E, 0x11, 0x11, 0x15, 0x09, 0x16, 0x00},  // Q
        {0x0F, 0x11, 0x0F, 0x05, 0x09, 0x11, 0x00},  // R
        {0x0E, 0x11, 0x06, 0x08, 0x11, 0x0E, 0x00},  // S
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00},  // T
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00},  // U
        {0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x00},  // V
        {0x11, 0x11, 0x15, 0x15, 0x15, 0x0A, 0x00},  // W
        {0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x00},  // X
        {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x00},  // Y
        {0x1F, 0x10, 0x08, 0x04, 0x02, 0x1F, 0x00},  // Z
    };
    if (c >= '0' && c <= '9')
      return &font[c - '0'][0];
    if (c >= 'A' && c <= 'Z')
      return &font[10 + (c - 'A')][0];
    switch (c) {
      case '-': {
        static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00};
        return g;
      }
      case '_': {
        static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00};
        return g;
      }
      case '|': {
        static const uint8_t g[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00};
        return g;
      }
      case '\\': {
        static const uint8_t g[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00};
        return g;
      }
      case '=': {
        static const uint8_t g[7] = {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00};
        return g;
      }
      case '+': {
        static const uint8_t g[7] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
        return g;
      }
      case '[': {
        static const uint8_t g[7] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x0E, 0x00};
        return g;
      }
      case ']': {
        static const uint8_t g[7] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x0E, 0x00};
        return g;
      }
      case '{': {
        static const uint8_t g[7] = {0x18, 0x04, 0x04, 0x03, 0x04, 0x04, 0x18};
        return g;
      }
      case '}': {
        static const uint8_t g[7] = {0x03, 0x04, 0x04, 0x18, 0x04, 0x04, 0x03};
        return g;
      }
      case ';': {
        static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x02};
        return g;
      }
      case ':': {
        static const uint8_t g[7] = {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00};
        return g;
      }
      case '\'': {
        static const uint8_t g[7] = {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
        return g;
      }
      case '"': {
        static const uint8_t g[7] = {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
        return g;
      }
      case ',': {
        static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02};
        return g;
      }
      case '.': {
        static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00};
        return g;
      }
      case '<': {
        static const uint8_t g[7] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10};
        return g;
      }
      case '>': {
        static const uint8_t g[7] = {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01};
        return g;
      }
      case '?': {
        static const uint8_t g[7] = {0x0E, 0x11, 0x10, 0x08, 0x00, 0x08, 0x00};
        return g;
      }
      case '/': {
        static const uint8_t g[7] = {0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00};
        return g;
      }
      case '`': {
        static const uint8_t g[7] = {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
        return g;
      }
      case '~': {
        static const uint8_t g[7] = {0x00, 0x00, 0x0A, 0x15, 0x00, 0x00, 0x00};
        return g;
      }
      default:
        break;
    }
    return nullptr;
  }

  void DrawText(SDL_Renderer* r,
                float x,
                float y,
                float w,
                float h,
                const std::string& text) {
    if (text.empty())
      return;
    std::string s = text;
    for (auto& c : s)
      c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
    int n = (int)s.size();
    if (n <= 0)
      return;

    // Font scaling factor: make the font occupy 60% of the button area, leaving a margin
    float scale_factor = 0.6f;
    float available_w = w * scale_factor;
    float available_h = h * scale_factor;

    // Ensure minimum font size (reduce character count if sx is too small)
    float min_sx = 0.4f;  // Minimum character width
    float required_sx = available_w / (n * 6.0f);
    if (required_sx < min_sx && n > 1) {
      // Reduce character count and recalculate
      n = std::max(1, (int)(available_w / (6.0f * min_sx)));
      s = s.substr(0, n);
    }
    float sx = available_w / (n * 6.0f);
    float sy = available_h / 9.0f;
    if (sx < 0.3f || sy < 0.3f)
      return;  // If too small, do not draw
    float px = x + (w - sx * (n * 6.0f)) * 0.5f;
    float py = y + (h - sy * 7.0f) * 0.5f;
    Uint8 a = (Uint8)(alpha_ * 255);
    for (int i = 0; i < n; i++) {
      const uint8_t* g = Glyph5x7(s[i]);
      if (g)
        VK_DrawGlyphFixed(r, px, py, sx, sy, g, a);
      px += sx * 6.0f;
    }
  }

  void DrawKeyLabel(SDL_Renderer* r, const Key& k) {
    std::string t = k.label;
    if (t == "-")
      t = "-_";
    else if (t == "\\")
      t = "\\|";
    else if (t == "=")
      t = "=+";
    else if (t == "[")
      t = "[{";
    else if (t == "]")
      t = "]}";
    else if (t == ";")
      t = ";:";
    else if (t == "'")
      t = "'\"";
    else if (t == ",")
      t = ",<";
    else if (t == ".")
      t = ".>";
    else if (t == "/")
      t = "/?";
    else if (t == "`")
      t = "`~";
    // Simplify special key labels (allow display on narrow keys)
    if (t == "Escape")
      t = "ESC";
    else if (t == "Backspace")
      t = "BKSP";
    else if (t == "CapsLock")
      t = "CAPS";
    else if (t == "Control" || t == "Ctrl")
      t = "CTRL";
    else if (t == "GUI" || t == "Win")
      t = "WIN";
    else if (t == "Menu")
      t = "MENU";
    else if (t == "Space")
      t = " ";
    else if (t == "Shift")
      t = "SHFT";
    else if (t == "Tab")
      t = "TAB";
    else if (t == "Enter")
      t = "ENT";
    else if (t == "ArrowLeft")
      t = "LFT";
    else if (t == "ArrowRight")
      t = "RGT";
    else if (t == "ArrowUp")
      t = "UP";
    else if (t == "ArrowDown")
      t = "DWN";
    DrawText(r, k.rect.x + 4.0f, k.rect.y + 4.0f, k.rect.w - 8.0f,
             k.rect.h - 8.0f, t);
  }

  void SendDown(const Key& k) {
    if (!sender_)
      return;
    remote::proto::KeyboardMsg km{};
    km.key = k.label;
    km.code = k.code;
    km.down = true;
    km.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(km);
    if (!pb.empty())
      sender_(pb);
    else
      sender_(remote::proto::SerializeKeyboard(km));
  }
  void SendUp(const Key& k) {
    if (!sender_)
      return;
    remote::proto::KeyboardMsg km{};
    km.key = k.label;
    km.code = k.code;
    km.down = false;
    km.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(km);
    if (!pb.empty())
      sender_(pb);
    else
      sender_(remote::proto::SerializeKeyboard(km));
  }
  void SendShift(bool down) {
    if (!sender_)
      return;
    remote::proto::KeyboardMsg km{};
    km.key = "Shift";
    km.code = 0;
    km.down = down;
    km.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(km);
    if (!pb.empty())
      sender_(pb);
    else
      sender_(remote::proto::SerializeKeyboard(km));
  }
  void SendCharClick(char c) {
    if (!sender_)
      return;
    std::string lab(1, c);
    remote::proto::KeyboardMsg kd{};
    kd.key = lab;
    kd.code = (int)c;
    kd.down = true;
    kd.mods = 0;
    remote::proto::KeyboardMsg ku = kd;
    ku.down = false;
    auto pbd = remote::proto::PbSerializeKeyboard(kd);
    if (!pbd.empty())
      sender_(pbd);
    else
      sender_(remote::proto::SerializeKeyboard(kd));
    auto pbu = remote::proto::PbSerializeKeyboard(ku);
    if (!pbu.empty())
      sender_(pbu);
    else
      sender_(remote::proto::SerializeKeyboard(ku));
  }
  void SendComboUnderscore() {
    SendShift(true);
    SendCharClick('-');
    SendShift(false);
  }
  void SendComboPipe() {
    SendShift(true);
    SendCharClick((char)92);
    SendShift(false);
  }
};

}  // namespace overlay
}  // namespace remote

#endif  // REMOTE_OVERLAY_VIRTUAL_KEYBOARD_FULL_H_
