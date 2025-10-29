// Description: Full-size virtual keyboard (SDL3). Draw labels with 5x7 built-in font, and send keyboard events when clicked.
#ifndef REMOTE_OVERLAY_VIRTUAL_KEYBOARD_FULL_H_
#define REMOTE_OVERLAY_VIRTUAL_KEYBOARD_FULL_H_

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <numeric>
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
  using HideCallback = std::function<void()>;

  void SetSender(Sender s) { sender_ = std::move(s); }
  void SetHideCallback(HideCallback cb) { hide_callback_ = std::move(cb); }
  void SetOpacity(float a) {
    if (a < 0.f)
      a = 0.f;
    if (a > 1.f)
      a = 1.f;
    alpha_ = a;
  }

  SDL_FRect GetKeyboardRect() const { return kb_rect_; }

  // Lock key states
  struct LockState {
    bool caps_lock = false;
    bool num_lock = false;
    bool scroll_lock = false;
  };

  // Combo key definition
  struct ComboKey {
    std::vector<std::string> keys;
    std::string command;
  };

  // Call every frame. Draw after layout is determined.
  void Render(SDL_Renderer* r) {
    EnsureLayout(r);
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r, &w, &h);
    float margin = 10.0f;
    float kb_width = w * 0.5f;
    float kb_height = kb_width * 0.4f * (2.0f / 3.0f);
    float kb_x = (w - kb_width) * 0.5f;
    float kb_y = h - kb_height - margin;
    SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(alpha_ * 80));
    SDL_FRect bg{kb_x, kb_y, kb_width, kb_height + 8.0f};
    kb_rect_ = bg;
    SDL_RenderFillRect(r, &bg);
    for (auto& k : keys_) {
      // Lock key highlight
      bool is_locked = IsKeyLocked(k);
      
      if (k.pressed || is_locked)
        SDL_SetRenderDrawColor(r, 140, 180, 240, (Uint8)(alpha_ * 200));
      else
        SDL_SetRenderDrawColor(r, 200, 200, 200, (Uint8)(alpha_ * 150));
      SDL_RenderFillRect(r, &k.rect);
      
      // Draw lock indicator
      if (is_locked) {
        SDL_SetRenderDrawColor(r, 255, 215, 0, (Uint8)(alpha_ * 220));  // Golden indicator
        SDL_FRect indicator{k.rect.x + 2.0f, k.rect.y + 2.0f, 4.0f, 4.0f};
        SDL_RenderFillRect(r, &indicator);
      }
      
      DrawKeyLabel(r, k);
    }
  }

  // Mouse event processing
  bool OnMouseDown(float x, float y) {
    for (int i = 0; i < (int)keys_.size(); ++i) {
      auto& k = keys_[i];
      if (!Inside(k.rect, x, y))
        continue;
      
      // Special X button to hide keyboard
      if (k.is_special && k.label == "X") {
        if (hide_callback_)
          hide_callback_();
        return true;
      }
      
      // Lock keys: toggle state
      if (k.label == "CapsLock") {
        lock_state_.caps_lock = !lock_state_.caps_lock;
        SendKeyToggle(k, lock_state_.caps_lock);
        return true;
      }
      if (k.label == "NumLock") {
        lock_state_.num_lock = !lock_state_.num_lock;
        SendKeyToggle(k, lock_state_.num_lock);
        return true;
      }
      if (k.label == "ScrollLock") {
        lock_state_.scroll_lock = !lock_state_.scroll_lock;
        SendKeyToggle(k, lock_state_.scroll_lock);
        return true;
      }
      
      // Modifier keys: toggle latch state (click to lock, click again to unlock)
      if (k.is_mod) {
        k.pressed = !k.pressed;  // Toggle state
        if (k.pressed) {
          SendDown(k);
          if (std::find(active_mods_.begin(), active_mods_.end(), i) ==
              active_mods_.end())
            active_mods_.push_back(i);
        } else {
          SendUp(k);
          auto it = std::find(active_mods_.begin(), active_mods_.end(), i);
          if (it != active_mods_.end())
            active_mods_.erase(it);
        }
        return true;
      }
      
      // Normal keys: check combo and send
      SendDown(k);
      k.pressed = true;
      last_pressed_ = i;
      
      // Check if this triggers a combo
      CheckAndTriggerCombo();
      
      return true;
    }
    return false;
  }

  bool OnMouseUp(float, float) {
    bool handled = false;
    if (last_pressed_.has_value()) {
      int i = *last_pressed_;
      if (i >= 0 && i < (int)keys_.size()) {
        auto& k = keys_[i];
        // Only send Up for normal keys (not modifiers, not special)
        if (!k.is_mod && !k.is_special) {
          SendUp(k);
          k.pressed = false;
          handled = true;
        }
      }
      last_pressed_.reset();
    }
    // Note: Modifiers are NOT auto-released here anymore
    // They persist until clicked again or combo is triggered
    return handled;
  }

 private:
  struct Key {
    SDL_FRect rect;
    std::string label;
    int code;
    bool is_mod{false};
    bool pressed{false};
    bool is_special{false};
  };

  std::vector<Key> keys_;
  std::optional<int> last_pressed_{};
  int lw_{0}, lh_{0};
  float alpha_{0.85f};
  Sender sender_{};
  HideCallback hide_callback_{};
  SDL_FRect kb_rect_{};
  std::vector<int> active_mods_{};
  LockState lock_state_{};
  
  // Predefined combo keys
  std::vector<ComboKey> combo_table_ = {
      {{"Win", "R"}, "WIN_R"},
      {{"Ctrl", "Alt", "Delete"}, "CTRL_ALT_DEL"},
      {{"Ctrl", "Shift", "Escape"}, "CTRL_SHIFT_ESC"},
      {{"Ctrl", "Escape"}, "CTRL_ESC"},
      {{"Ctrl", "Shift", "Q"}, "CTRL_SHIFT_Q"},
      {{"Ctrl", "Alt", "Q"}, "CTRL_ALT_Q"},
      {{"Ctrl", "Alt", "X"}, "CTRL_ALT_X"},
  };

  static bool Inside(const SDL_FRect& r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
  }

  // Check if a key is in locked state
  bool IsKeyLocked(const Key& k) const {
    if (k.label == "CapsLock")
      return lock_state_.caps_lock;
    if (k.label == "NumLock")
      return lock_state_.num_lock;
    if (k.label == "ScrollLock")
      return lock_state_.scroll_lock;
    return false;
  }

  // Check and trigger combo keys
  void CheckAndTriggerCombo() {
    if (active_mods_.empty())
      return;
    
    // Build current pressed keys set
    std::vector<std::string> pressed_keys;
    for (int idx : active_mods_) {
      if (idx >= 0 && idx < (int)keys_.size())
        pressed_keys.push_back(keys_[idx].label);
    }
    if (last_pressed_.has_value()) {
      int i = *last_pressed_;
      if (i >= 0 && i < (int)keys_.size())
        pressed_keys.push_back(keys_[i].label);
    }
    
    // Match against combo table
    for (const auto& combo : combo_table_) {
      if (MatchCombo(pressed_keys, combo.keys)) {
        // Trigger combo action
        TriggerComboAction(combo.command);
        // Release all modifiers after combo
        ReleaseModifiers();
        return;
      }
    }
  }

  // Match if pressed keys contain all combo keys
  bool MatchCombo(const std::vector<std::string>& pressed,
                  const std::vector<std::string>& combo) const {
    if (combo.size() > pressed.size())
      return false;
    for (const auto& key : combo) {
      bool found = false;
      for (const auto& p : pressed) {
        if (NormalizeKeyName(p) == NormalizeKeyName(key)) {
          found = true;
          break;
        }
      }
      if (!found)
        return false;
    }
    return true;
  }

  // Normalize key names for comparison
  std::string NormalizeKeyName(const std::string& key) const {
    std::string normalized = key;
    if (normalized == "Control" || normalized == "CTRL")
      return "Ctrl";
    if (normalized == "GUI" || normalized == "WIN")
      return "Win";
    if (normalized == "SHFT")
      return "Shift";
    if (normalized == "ESC")
      return "Escape";
    if (normalized == "Del")
      return "Delete";
    return normalized;
  }

  // Trigger combo action (send special command or execute locally)
  void TriggerComboAction(const std::string& command) {
    // For now, just send a special keyboard message with the combo command
    // The remote end can handle these special commands
    if (!sender_)
      return;
    
    remote::proto::KeyboardMsg km{};
    km.key = "COMBO_" + command;
    km.code = 0;
    km.down = true;
    km.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(km);
    if (!pb.empty())
      sender_(pb);
    else
      sender_(remote::proto::SerializeKeyboard(km));
    
    // Send key up immediately
    km.down = false;
    pb = remote::proto::PbSerializeKeyboard(km);
    if (!pb.empty())
      sender_(pb);
    else
      sender_(remote::proto::SerializeKeyboard(km));
  }

  // Send lock key toggle
  void SendKeyToggle(const Key& k, bool state) {
    if (!sender_)
      return;
    remote::proto::KeyboardMsg km{};
    km.key = k.label;
    km.code = k.code;
    km.down = state;  // true = locked, false = unlocked
    km.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(km);
    if (!pb.empty())
      sender_(pb);
    else
      sender_(remote::proto::SerializeKeyboard(km));
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
    float kb_w = w * 0.5f;
    float kb_height = kb_w * 0.4f * (2.0f / 3.0f);
    float kb_y = static_cast<float>(h) - kb_height - margin;
    float kb_x = (w - kb_w) * 0.5f;
    float unit = kb_w / 15.0f;
    float row_h = kb_height / 6.0f;
    float y = kb_y;
    auto add = [&](float x, float y, float uw, const std::string& label,
                   int code, bool is_mod = false, bool is_special = false) {
      keys_.push_back({SDL_FRect{x, y, uw * unit - 3.0f, row_h - 3.0f}, label,
                       code, is_mod, false, is_special});
    };

    struct RowKey {
      float uw;
      std::string label;
      int code;
      bool is_mod;
      bool is_special;
    };

    auto layout_row = [&](const std::vector<RowKey>& defs) {
      if (defs.empty())
        return;
      float total_units = std::accumulate(defs.begin(), defs.end(), 0.0f,
                                          [](float sum, const RowKey& key) {
                                            return sum + key.uw;
                                          });
      const int count = static_cast<int>(defs.size());
      float spacing_units = 0.0f;
      if (count > 1)
        spacing_units = (15.0f - total_units) / (count - 1);
      float start_unit = 0.0f;
      if (spacing_units < 0.0f) {
        spacing_units = 0.0f;
        start_unit = std::max(0.0f, (15.0f - total_units) * 0.5f);
      }
      float current = start_unit;
      for (int i = 0; i < count; ++i) {
        const auto& key = defs[i];
        add(kb_x + current * unit, y, key.uw, key.label, key.code,
            key.is_mod, key.is_special);
        current += key.uw;
        if (i != count - 1)
          current += spacing_units;
      }
      y += row_h;
    };

    std::vector<RowKey> row0;
    row0.push_back({1.0f, "Escape", 0, true, false});
    for (int i = 0; i < 12; ++i)
      row0.push_back({0.9f, std::string("F") + std::to_string(i + 1), 0, false,
                      false});
    row0.push_back({0.8f, "X", 0, false, true});
    layout_row(row0);

    std::vector<RowKey> row1;
    row1.push_back({0.8f, std::string(1, '`'), '`', false, false});
    const char* nums = "1234567890";
    for (int i = 0; i < 10; ++i)
      row1.push_back({0.9f, std::string(1, nums[i]), nums[i], false, false});
    row1.push_back({0.8f, "-", '-', false, false});
    row1.push_back({0.8f, "=", '=', false, false});
    row1.push_back({1.5f, "Backspace", 0, true, false});
    row1.push_back({0.9f, "Delete", 0, true, false});
    layout_row(row1);

    std::vector<RowKey> row2;
    row2.push_back({1.6f, "Tab", 0, true, false});
    const char* rowQ = "QWERTYUIOP";
    for (int i = 0; i < 10; ++i)
      row2.push_back({1.0f, std::string(1, rowQ[i]), rowQ[i], false, false});
    row2.push_back({1.0f, "[", '[', false, false});
    row2.push_back({1.0f, "]", ']', false, false});
    row2.push_back({1.2f, "\\", '\\', false, false});
    layout_row(row2);

    std::vector<RowKey> row3;
    row3.push_back({1.6f, "CapsLock", 0, true, false});
    const char* rowA = "ASDFGHJKL";
    for (int i = 0; i < 9; ++i)
      row3.push_back({0.9f, std::string(1, rowA[i]), rowA[i], false, false});
    row3.push_back({0.8f, ";", ';', false, false});
    row3.push_back({0.8f, "'", '\'', false, false});
    row3.push_back({1.8f, "Enter", 0, true, false});
    row3.push_back({1.0f, "NumLock", 0, true, false});
    layout_row(row3);

    std::vector<RowKey> row4;
    row4.push_back({2.0f, "Shift", 0, true, false});
    const char* rowZ = "ZXCVBNM";
    for (int i = 0; i < 7; ++i)
      row4.push_back({0.9f, std::string(1, rowZ[i]), rowZ[i], false, false});
    row4.push_back({0.8f, ",", ',', false, false});
    row4.push_back({0.8f, ".", '.', false, false});
    row4.push_back({0.8f, "/", '/', false, false});
    row4.push_back({2.2f, "Shift", 0, true, false});
    row4.push_back({1.0f, "ScrollLock", 0, true, false});
    layout_row(row4);

    std::vector<RowKey> row5;
    row5.push_back({1.5f, "Ctrl", 0, true, false});
    row5.push_back({1.2f, "Win", 0, true, false});
    row5.push_back({1.2f, "Alt", 0, true, false});
    row5.push_back({5.0f, "Space", ' ', false, false});
    row5.push_back({1.2f, "Alt", 0, true, false});
    row5.push_back({1.2f, "Win", 0, true, false});
    row5.push_back({1.0f, "Menu", 0, true, false});
    row5.push_back({0.8f, "Ctrl", 0, true, false});
    layout_row(row5);
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
    else if (t == "Delete")
      t = "DEL";
    else if (t == "CapsLock")
      t = "CAPS";
    else if (t == "NumLock")
      t = "NUM";
    else if (t == "ScrollLock")
      t = "SCRL";
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
  void ReleaseModifiers() {
    for (int idx : active_mods_) {
      if (idx >= 0 && idx < (int)keys_.size()) {
        auto& mk = keys_[idx];
        if (mk.pressed) {
          SendUp(mk);
          mk.pressed = false;
        }
      }
    }
    active_mods_.clear();
  }
};

}  // namespace overlay
}  // namespace remote

#endif  // REMOTE_OVERLAY_VIRTUAL_KEYBOARD_FULL_H_
