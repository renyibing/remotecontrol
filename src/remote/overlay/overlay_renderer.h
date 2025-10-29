// Overlay rendering (sending side)
// Policy: readability priority, SDL3 support, optional SDL_ttf tooltip text
#ifndef REMOTE_OVERLAY_OVERLAY_RENDERER_H_
#define REMOTE_OVERLAY_OVERLAY_RENDERER_H_

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "remote/overlay/virtual_keyboard_full.h"
#include "remote/proto/messages.h"
#include "remote/proto/protobuf_serializer.h"
#include "remote/proto/serializer.h"

#ifdef REMOTE_WITH_SDL_TTF
#include <SDL3_ttf/SDL_ttf.h>
#endif

namespace remote {
namespace overlay {

class OverlayRenderer {
 public:
  using ReliableSender = std::function<bool(const std::vector<uint8_t>&)>;
  using RtSender = std::function<bool(const std::vector<uint8_t>&)>;
  using UiCommand = std::function<void(const std::string& cmd, bool value)>;

  // Render (call after video frame)
  void Render(SDL_Renderer* r) {
    const bool has_image =
        cursor_.visible && cursor_.w > 0 && cursor_.h > 0 &&
        cursor_.rgba.size() >= static_cast<size_t>(cursor_.w) * cursor_.h * 4;
    // Hide OS cursor only when there is a valid remote cursor image
    if (has_image)
      SDL_HideCursor();
    else
      SDL_ShowCursor();
    if (has_image) {
      float mx = 0.0f, my = 0.0f;
      SDL_GetMouseState(&mx, &my);
      // Windows side CursorMonitor is BGRA due to DIB, treat as BGRA32
      // If fmt is transported via message in the future, switch here
      SDL_Surface* surface = SDL_CreateSurfaceFrom(
          cursor_.w, cursor_.h, SDL_PIXELFORMAT_BGRA32,
          static_cast<void*>(const_cast<uint8_t*>(cursor_.rgba.data())),
          cursor_.w * 4);
      if (surface) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surface);
        if (tex) {
          // Enable blend to correctly synthesize transparent cursor
          SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
          SDL_FRect src{0, 0, static_cast<float>(cursor_.w),
                        static_cast<float>(cursor_.h)};
          SDL_FRect dst{mx - static_cast<float>(cursor_.hotspotX),
                        my - static_cast<float>(cursor_.hotspotY),
                        static_cast<float>(cursor_.w),
                        static_cast<float>(cursor_.h)};
          SDL_RenderTexture(r, tex, &src, &dst);
          SDL_DestroyTexture(tex);
        }
        SDL_DestroySurface(surface);
      }
    }

    DrawToolbar(r);
    if (keyboard_visible_) {
      vk_full_.SetSender(reliable_);
      vk_full_.Render(r);
    }
    if (gamepad_visible_) {
      DrawGamepad(r);
    }
  }

  // State update
  void SetCursorImage(const remote::proto::CursorImageMsg& img) {
    cursor_ = img;
  }
  void SetImeState(const remote::proto::ImeStateMsg& st) { ime_state_ = st; }
  void SetSenders(ReliableSender reliable, RtSender rt) {
    reliable_ = std::move(reliable);
    rt_ = std::move(rt);
  }
  void SetUiCommand(UiCommand cb) { ui_cmd_ = std::move(cb); }
  void SetKeyboardOpacity(float a) { vk_full_.SetOpacity(a); }

#ifdef REMOTE_WITH_SDL_TTF
  bool ConfigureTooltipFont(const std::string& font_path, int pt_size) {
    tip_font_path_ = font_path;
    tip_font_px_ = pt_size;
    if (!InitTtf())
      return false;
    if (tip_font_) {
      TTF_CloseFont(tip_font_);
      tip_font_ = nullptr;
    }
    tip_font_ = TTF_OpenFont(font_path.c_str(), pt_size);
    return tip_font_ != nullptr;
  }
#endif

  // Event processing: return true indicates consumption
  bool OnEvent(const SDL_Event& e) {
    switch (e.type) {
      case SDL_EVENT_MOUSE_MOTION: {
        auto inside = [](const SDL_FRect& rc, float px, float py) {
          if (rc.w <= 0.0f || rc.h <= 0.0f)
            return false;
          return px >= rc.x && px <= rc.x + rc.w && py >= rc.y &&
                 py <= rc.y + rc.h;
        };
        const float mx = static_cast<float>(e.motion.x);
        const float my = static_cast<float>(e.motion.y);
        const bool over_arrow = inside(toolbar_trigger_area_, mx, my);
        const bool over_buttons = inside(toolbar_buttons_area_, mx, my);
        if (toolbar_pinned_) {
          toolbar_hover_ = true;
        } else {
          toolbar_hover_ = over_arrow || over_buttons;
        }
        break;
      }
      case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (HitToolbar(static_cast<float>(e.button.x),
                       static_cast<float>(e.button.y)))
          return true;
        if (keyboard_visible_) {
          if (vk_full_.OnMouseDown(static_cast<float>(e.button.x),
                                   static_cast<float>(e.button.y)))
            return true;
        }
        break;
      }
      case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (keyboard_visible_) {
          if (vk_full_.OnMouseUp(static_cast<float>(e.button.x),
                                 static_cast<float>(e.button.y)))
            return true;
        }
        if (gamepad_visible_ && gp_a_down_) {
          gp_a_down_ = false;
          uint16_t mask = 0;
          auto pb =
              remote::proto::PbSerializeGamepadXInput(mask, 0, 0, 0, 0, 0, 0);
          if (!pb.empty()) {
            if (rt_)
              rt_(pb);
          } else {
            auto js =
                remote::proto::SerializeGamepadXInput(mask, 0, 0, 0, 0, 0, 0);
            if (rt_)
              rt_(js);
          }
          return true;
        }
        break;
      }
      default:
        break;
    }
    return false;
  }

 private:
  // Send
  remote::proto::CursorImageMsg cursor_{};
  ReliableSender reliable_{};
  RtSender rt_{};
  UiCommand ui_cmd_{};
  remote::proto::ImeStateMsg ime_state_{};

  // Keyboard
  VirtualKeyboardFull vk_full_{};

  // Top toolbar
  bool toolbar_pinned_{false};
  bool toolbar_hover_{false};
  float toolbar_height_{36.0f};          // Increase height to accommodate larger buttons
  float toolbar_trigger_height_{20.0f};  // Trigger area height (display arrow)
  SDL_FRect toolbar_btn_pin_{};
  SDL_FRect toolbar_btn_minimize_{};  // Minimize button
  SDL_FRect toolbar_btn_full_{};
  SDL_FRect toolbar_btn_close_{};
  SDL_FRect toolbar_btn_kb_{};
  SDL_FRect toolbar_btn_pad_{};
  SDL_FRect toolbar_btn_copy_{};
  SDL_FRect toolbar_btn_paste_{};
  SDL_FRect toolbar_btn_cad_{};
  SDL_FRect toolbar_trigger_area_{};
  SDL_FRect toolbar_buttons_area_{};

  // Visible state
  bool keyboard_visible_{false};
  bool gamepad_visible_{false};

  // Gamepad (example button A)
  SDL_FRect gp_a_{};
  bool gp_a_down_{false};

#ifdef REMOTE_WITH_SDL_TTF
  // Optional: SDL_ttf
  bool ttf_ready_{false};
  TTF_Font* tip_font_{nullptr};
  int tip_font_px_{16};
  std::string tip_font_path_{};
  bool InitTtf() {
    if (ttf_ready_)
      return true;
    if (TTF_Init() == 0) {
      ttf_ready_ = true;
      return true;
    }
    return false;
  }
#endif

  void DrawToolbar(SDL_Renderer* r) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r, &w, &h);
    const float center_x = static_cast<float>(w) / 2.0f;

    const bool show_full = toolbar_pinned_ || toolbar_hover_;

    // State to display only downward arrow
    if (!show_full) {
      const float arrow_btn_size = 32.0f;
      const float arrow_y = 6.0f;
      SDL_FRect arrow_btn{center_x - arrow_btn_size / 2.0f, arrow_y,
                          arrow_btn_size, arrow_btn_size};
      toolbar_trigger_area_ = arrow_btn;
      toolbar_buttons_area_ = SDL_FRect{};
      toolbar_btn_pin_ = {};
      toolbar_btn_minimize_ = {};
      toolbar_btn_full_ = {};
      toolbar_btn_close_ = {};
      toolbar_btn_kb_ = {};
      toolbar_btn_pad_ = {};
      toolbar_btn_copy_ = {};
      toolbar_btn_paste_ = {};
      toolbar_btn_cad_ = {};

      // Button background (semi-transparent gradient)
      SDL_SetRenderDrawColor(r, 82, 122, 168, 230);
      SDL_RenderFillRect(r, &arrow_btn);
      SDL_SetRenderDrawColor(r, 130, 170, 210, 210);
      SDL_RenderLine(r, arrow_btn.x + 2, arrow_btn.y + 1,
                     arrow_btn.x + arrow_btn.w - 2, arrow_btn.y + 1);
      SDL_SetRenderDrawColor(r, 48, 74, 106, 210);
      SDL_RenderLine(r, arrow_btn.x + 2, arrow_btn.y + arrow_btn.h - 2,
                     arrow_btn.x + arrow_btn.w - 2,
                     arrow_btn.y + arrow_btn.h - 2);
      SDL_SetRenderDrawColor(r, 96, 100, 116, 255);
      SDL_RenderRect(r, &arrow_btn);

      // Arrow icon (make it look 3D white)
      SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
      const float cx = arrow_btn.x + arrow_btn.w / 2.0f;
      const float cy = arrow_btn.y + arrow_btn.h / 2.0f;
      for (int i = 0; i < 7; ++i) {
        const float offset = static_cast<float>(i);
        SDL_RenderLine(r, cx - 8.0f + offset, cy - 3.0f + offset * 0.5f,
                       cx + 8.0f - offset, cy - 3.0f + offset * 0.5f);
      }
      SDL_SetRenderDrawColor(r, 210, 230, 250, 255);
      SDL_RenderLine(r, cx - 5.0f, cy - 1.5f, cx + 5.0f, cy - 1.5f);
      return;
    }

    // Button group (including background)
    const float btn_size = 26.0f;        // Button size
    const float btn_spacing = 6.0f;      // Button spacing
    const float total_btn_count = 9.0f;  // Button count
    const float total_width =
        total_btn_count * btn_size + (total_btn_count - 1) * btn_spacing;
    const float group_padding = 8.0f;
    const float ime_gap = 12.0f;
    const float ime_width = 48.0f;
    const float group_height = btn_size + group_padding * 2.0f;
    const float group_width =
        total_width + group_padding * 2.0f + ime_gap + ime_width;
    const float max_group_x =
        std::max(0.0f, static_cast<float>(w) - group_width);
    const float group_x =
        std::clamp(center_x - group_width / 2.0f, 0.0f, max_group_x);
    const float group_y = 6.0f;
    const float btn_y = group_y + group_padding;

    SDL_FRect group_rect{group_x, group_y, group_width, group_height};
    toolbar_buttons_area_ = group_rect;
    toolbar_trigger_area_ = SDL_FRect{};

    // Background (semi-transparent plate centrally positioned)
    SDL_SetRenderDrawColor(r, 28, 30, 40, 235);
    SDL_RenderFillRect(r, &group_rect);
    SDL_SetRenderDrawColor(r, 58, 60, 76, 220);
    SDL_RenderLine(r, group_rect.x + 2, group_rect.y + 1,
                   group_rect.x + group_rect.w - 2, group_rect.y + 1);
    SDL_SetRenderDrawColor(r, 12, 14, 20, 220);
    SDL_RenderLine(r, group_rect.x + 2, group_rect.y + group_rect.h - 2,
                   group_rect.x + group_rect.w - 2,
                   group_rect.y + group_rect.h - 2);
    SDL_SetRenderDrawColor(r, 70, 72, 90, 255);
    SDL_RenderRect(r, &group_rect);

    float btn_x = group_x + group_padding;
    SDL_FRect btn_pin{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_minimize{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_full{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_close{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_kb{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_pad{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_copy{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_paste{btn_x, btn_y, btn_size, btn_size};
    btn_x += btn_size + btn_spacing;
    SDL_FRect btn_cad{btn_x, btn_y, btn_size, btn_size};
    const float ime_x = group_rect.x + group_rect.w - group_padding - ime_width;

    // Draw button background (with gradient effect)
    auto draw_button_bg = [&](const SDL_FRect& btn, int r_val, int g_val,
                              int b_val) {
      // Main background
      SDL_SetRenderDrawColor(r, r_val, g_val, b_val, 255);
      SDL_RenderFillRect(r, &btn);
      // Highlight effect (top edge)
      SDL_SetRenderDrawColor(r, std::min(255, r_val + 30),
                             std::min(255, g_val + 30),
                             std::min(255, b_val + 30), 200);
      SDL_RenderLine(r, btn.x + 2, btn.y + 1, btn.x + btn.w - 2, btn.y + 1);
      // Shadow effect (bottom edge)
      SDL_SetRenderDrawColor(r, std::max(0, r_val - 30),
                             std::max(0, g_val - 30), std::max(0, b_val - 30),
                             200);
      SDL_RenderLine(r, btn.x + 2, btn.y + btn.h - 2, btn.x + btn.w - 2,
                     btn.y + btn.h - 2);
      // Border
      SDL_SetRenderDrawColor(r, 80, 80, 90, 255);
      SDL_RenderRect(r, &btn);
    };

    draw_button_bg(btn_pin, 100, 150, 200);       // Blue
    draw_button_bg(btn_minimize, 150, 150, 150);  // Gray
    draw_button_bg(btn_full, 100, 180, 100);      // Green
    draw_button_bg(btn_close, 200, 80, 80);       // Red
    draw_button_bg(btn_kb, 140, 120, 180);        // Purple
    draw_button_bg(btn_pad, 180, 140, 100);       // Orange
    draw_button_bg(btn_copy, 120, 160, 140);      // Cyan green
    draw_button_bg(btn_paste, 160, 130, 160);     // Lavender
    draw_button_bg(btn_cad, 180, 100, 100);       // Crimson

    // Simple icons (white, clearly visible)
    auto draw_line = [&](float x1, float y1, float x2, float y2) {
      SDL_RenderLine(r, x1, y1, x2, y2);
    };
    auto draw_rect = [&](const SDL_FRect& rc) { SDL_RenderRect(r, &rc); };
    auto fill_rect = [&](const SDL_FRect& rc) { SDL_RenderFillRect(r, &rc); };

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);  // White icons

    // Fixed button: Pin icon
    float cx = btn_pin.x + btn_size / 2.0f;
    float cy = btn_pin.y + btn_size / 2.0f;
    draw_line(cx - 3, cy + 4, cx + 3, cy - 4);   // Diagonal line
    fill_rect(SDL_FRect{cx - 1, cy - 6, 2, 5});  // Needle head
    fill_rect(SDL_FRect{cx - 4, cy + 4, 8, 2});  // Needle bottom

    // Minimize: Horizontal line
    cx = btn_minimize.x + btn_size / 2.0f;
    cy = btn_minimize.y + btn_size / 2.0f;
    fill_rect(SDL_FRect{cx - 6, cy, 12, 2});

    // Fullscreen: Extended arrow
    cx = btn_full.x + btn_size / 2.0f;
    cy = btn_full.y + btn_size / 2.0f;
    // Top left corner arrow
    draw_line(cx - 6, cy - 2, cx - 6, cy - 6);
    draw_line(cx - 6, cy - 6, cx - 2, cy - 6);
    draw_line(cx - 6, cy - 6, cx - 3, cy - 3);
    // Bottom right corner arrow
    draw_line(cx + 6, cy + 2, cx + 6, cy + 6);
    draw_line(cx + 6, cy + 6, cx + 2, cy + 6);
    draw_line(cx + 6, cy + 6, cx + 3, cy + 3);

    // Close: X
    cx = btn_close.x + btn_size / 2.0f;
    cy = btn_close.y + btn_size / 2.0f;
    draw_line(cx - 5, cy - 5, cx + 5, cy + 5);
    draw_line(cx - 5, cy + 5, cx + 5, cy - 5);

    // Keyboard: Small square
    SDL_FRect keyrc{btn_kb.x + 5, btn_kb.y + 8, 16, 10};
    draw_rect(keyrc);
    draw_line(btn_kb.x + 9, btn_kb.y + 8, btn_kb.x + 9, btn_kb.y + 18);
    draw_line(btn_kb.x + 13, btn_kb.y + 8, btn_kb.x + 13, btn_kb.y + 18);
    draw_line(btn_kb.x + 17, btn_kb.y + 8, btn_kb.x + 17, btn_kb.y + 18);
    draw_line(btn_kb.x + 5, btn_kb.y + 13, btn_kb.x + 21, btn_kb.y + 13);

    // Gamepad: Game controller
    cx = btn_pad.x + btn_size / 2.0f;
    cy = btn_pad.y + btn_size / 2.0f;
    SDL_FRect padrc{cx - 7, cy - 4, 14, 8};
    draw_rect(padrc);
    // Cross key
    draw_line(cx - 3, cy, cx + 1, cy);
    draw_line(cx - 1, cy - 2, cx - 1, cy + 2);
    // Button
    fill_rect(SDL_FRect{cx + 3, cy - 1, 2, 2});
    fill_rect(SDL_FRect{cx + 5, cy - 1, 2, 2});

    // Copy: Two overlapping rectangles
    SDL_FRect cp1{btn_copy.x + 7, btn_copy.y + 7, 8, 10};
    draw_rect(cp1);
    SDL_FRect cp2{btn_copy.x + 11, btn_copy.y + 9, 8, 10};
    draw_rect(cp2);

    // Paste: Clipboard
    cx = btn_paste.x + btn_size / 2.0f;
    cy = btn_paste.y + btn_size / 2.0f;
    SDL_FRect pst{cx - 7, cy - 3, 14, 10};
    draw_rect(pst);
    fill_rect(SDL_FRect{cx - 3, cy - 6, 6, 3});  // Clamp

    // CAD: Ctrl+Alt+Del letter identifier
    cx = btn_cad.x + btn_size / 2.0f;
    cy = btn_cad.y + btn_size / 2.0f;
    // Draw simplified version of "CAD" three letters
    draw_line(cx - 8, cy - 4, cx - 8, cy + 4);  // C left
    draw_line(cx - 8, cy - 4, cx - 4, cy - 4);  // C top
    draw_line(cx - 8, cy + 4, cx - 4, cy + 4);  // C bottom
    draw_line(cx - 2, cy - 4, cx - 2, cy + 4);  // A left
    draw_line(cx + 2, cy - 4, cx + 2, cy + 4);  // A right
    draw_line(cx - 2, cy - 4, cx + 2, cy - 4);  // A top
    draw_line(cx - 2, cy, cx + 2, cy);          // A middle
    draw_line(cx + 4, cy - 4, cx + 4, cy + 4);  // D left
    draw_line(cx + 4, cy - 4, cx + 7, cy - 2);  // D right top
    draw_line(cx + 7, cy - 2, cx + 7, cy + 2);  // D right middle
    draw_line(cx + 7, cy + 2, cx + 4, cy + 4);  // D right bottom

    toolbar_btn_pin_ = btn_pin;
    toolbar_btn_minimize_ = btn_minimize;
    toolbar_btn_full_ = btn_full;
    toolbar_btn_close_ = btn_close;
    toolbar_btn_kb_ = btn_kb;
    toolbar_btn_pad_ = btn_pad;
    toolbar_btn_copy_ = btn_copy;
    toolbar_btn_paste_ = btn_paste;
    toolbar_btn_cad_ = btn_cad;

    // IME indicator (placed on the right side of the menu bar, not blocking the buttons)
    SDL_FRect ime{ime_x, btn_y, ime_width, btn_size};
    if (ime_state_.open)
      SDL_SetRenderDrawColor(r, 100, 180, 255, 255);
    else
      SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
    SDL_RenderFillRect(r, &ime);
    SDL_SetRenderDrawColor(r, 60, 60, 70, 255);
    SDL_RenderRect(r, &ime);

    // Tooltip removed due to user request
  }

  bool HitToolbar(float x, float y) {
    auto inside = [](const SDL_FRect& rc, float px, float py) {
      if (rc.w <= 0.0f || rc.h <= 0.0f)
        return false;
      return px >= rc.x && px <= rc.x + rc.w && py >= rc.y && py <= rc.y + rc.h;
    };

    // When not expanded, only the arrow button reacts
    if (!(toolbar_pinned_ || toolbar_hover_)) {
      if (inside(toolbar_trigger_area_, x, y)) {
        // When the arrow button is pressed, the menu is expanded
        toolbar_hover_ = true;
        return true;
      }
      return false;
    }

    // When expanded, only the valid button area is processed
    if (inside(toolbar_btn_pin_, x, y)) {
      toolbar_pinned_ = !toolbar_pinned_;
      return true;
    }
    if (inside(toolbar_btn_minimize_, x, y)) {
      if (ui_cmd_)
        ui_cmd_("minimize", true);
      return true;
    }
    if (inside(toolbar_btn_full_, x, y)) {
      if (ui_cmd_)
        ui_cmd_("fullscreen", true);
      return true;
    }
    if (inside(toolbar_btn_close_, x, y)) {
      if (ui_cmd_)
        ui_cmd_("close", true);
      return true;
    }
    if (inside(toolbar_btn_kb_, x, y)) {
      keyboard_visible_ = !keyboard_visible_;
      return true;
    }
    if (inside(toolbar_btn_pad_, x, y)) {
      gamepad_visible_ = !gamepad_visible_;
      return true;
    }
    if (inside(toolbar_btn_copy_, x, y)) {
      SendComboCopy();
      return true;
    }
    if (inside(toolbar_btn_paste_, x, y)) {
      SendComboPaste();
      return true;
    }
    if (inside(toolbar_btn_cad_, x, y)) {
      SendComboCtrlAltDel();
      return true;
    }
    // Click outside the menu is transparent
    return false;
  }

  void DrawGamepad(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 30, 30, 30, 160);
    SDL_FRect base{190.0f, 40.0f, 50.0f, 50.0f};
    SDL_RenderFillRect(r, &base);
    SDL_FRect a{200.0f, 50.0f, 28.0f, 28.0f};
    gp_a_ = a;
    SDL_SetRenderDrawColor(r, 0, 200, 0, 255);
    SDL_RenderFillRect(r, &a);
  }

  // Send combination keys
  void SendKeyDown(const std::string& name, int code) {
    if (!reliable_)
      return;
    remote::proto::KeyboardMsg k{};
    k.key = name;
    k.code = code;
    k.down = true;
    k.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(k);
    if (!pb.empty())
      reliable_(pb);
    else
      reliable_(remote::proto::SerializeKeyboard(k));
  }
  void SendKeyUp(const std::string& name, int code) {
    if (!reliable_)
      return;
    remote::proto::KeyboardMsg k{};
    k.key = name;
    k.code = code;
    k.down = false;
    k.mods = 0;
    auto pb = remote::proto::PbSerializeKeyboard(k);
    if (!pb.empty())
      reliable_(pb);
    else
      reliable_(remote::proto::SerializeKeyboard(k));
  }
  void SendKeyClick(const std::string& name, int code) {
    SendKeyDown(name, code);
    SendKeyUp(name, code);
  }
  void SendComboCopy() {
    SendKeyDown("Ctrl", 0);
    SendKeyClick("C", 'C');
    SendKeyUp("Ctrl", 0);
  }
  void SendComboPaste() {
    SendKeyDown("Ctrl", 0);
    SendKeyClick("V", 'V');
    SendKeyUp("Ctrl", 0);
  }
  void SendComboCtrlAltDel() {
    SendKeyDown("Ctrl", 0);
    SendKeyDown("Alt", 0);
    SendKeyClick("Delete", 0);
    SendKeyUp("Alt", 0);
    SendKeyUp("Ctrl", 0);
  }

  void DrawTooltip(SDL_Renderer* r, int mx, int my, const char* text_utf8) {
    float w = 120.0f, h = 26.0f;
#ifdef REMOTE_WITH_SDL_TTF
    if (InitTtf() && tip_font_) {
      int tw = 0, th = 0;
      if (TTF_SizeUTF8(tip_font_, text_utf8, &tw, &th) == 0) {
        w = static_cast<float>(tw) + 20.0f;
        h = static_cast<float>(th) + 12.0f;
      }
    }
#endif
    // Offset position (avoid blocking the mouse)
    SDL_FRect bg{static_cast<float>(mx + 16), static_cast<float>(my + 16), w,
                 h};

    // Dark background + high contrast border
    SDL_SetRenderDrawColor(r, 40, 40, 40, 240);
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 255, 200, 80, 255);  // Golden border, more visible
    SDL_RenderRect(r, &bg);

#ifdef REMOTE_WITH_SDL_TTF
    if (ttf_ready_ && tip_font_) {
      SDL_Color bright_text{255, 255, 255, 255};  // Pure white text
      SDL_Surface* ts =
          TTF_RenderUTF8_Blended(tip_font_, text_utf8, bright_text);
      if (ts) {
        SDL_Texture* tt = SDL_CreateTextureFromSurface(r, ts);
        if (tt) {
          SDL_FRect dst{bg.x + 10.0f, bg.y + 6.0f, static_cast<float>(ts->w),
                        static_cast<float>(ts->h)};
          SDL_RenderTexture(r, tt, nullptr, &dst);
          SDL_DestroyTexture(tt);
        }
        SDL_DestroySurface(ts);
      }
    }
#else
    // When there is no SDL_ttf, draw simple text indication
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for (int i = 0; i < 3; i++) {
      SDL_RenderLine(r, bg.x + 10 + i * 8, bg.y + h / 2, bg.x + 14 + i * 8,
                     bg.y + h / 2);
    }
#endif
  }
};

}  // namespace overlay
}  // namespace remote

#endif  // REMOTE_OVERLAY_OVERLAY_RENDERER_H_
