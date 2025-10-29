// Description: Minimal JSON parser (only extracting required fields)
// Comments in Chinese, emphasizing readability

#ifndef REMOTE_PROTO_PARSER_H_
#define REMOTE_PROTO_PARSER_H_

#include <string>
#include <string_view>
#include <optional>
#include <cctype>

#include "remote/proto/messages.h"
#include "remote/proto/base64.h"

namespace remote {
namespace proto {

// Extract the string value of the specified key from the JSON text (does not handle general escape, suitable for simple cases)
inline std::optional<std::string> JsonGetString(std::string_view s, std::string_view key) {
  std::string pattern = std::string("\"") + std::string(key) + std::string("\":\"");
  size_t p = s.find(pattern);
  if (p == std::string::npos) return std::nullopt;
  p += pattern.size();
  size_t q = s.find('"', p);
  if (q == std::string::npos) return std::nullopt;
  return std::string(s.substr(p, q - p));
}

// Extract the integer value of the specified key from the JSON text
inline std::optional<int> JsonGetInt(std::string_view s, std::string_view key) {
  std::string pattern = std::string("\"") + std::string(key) + std::string("\":");
  size_t p = s.find(pattern);
  if (p == std::string::npos) return std::nullopt;
  p += pattern.size();
  size_t q = p;
  while (q < s.size() && (isdigit(static_cast<unsigned char>(s[q])) || s[q]=='-')) q++;
  if (q == p) return std::nullopt;
  return std::stoi(std::string(s.substr(p, q - p)));
}

// Extract the boolean value of the specified key from the JSON text
inline std::optional<bool> JsonGetBool(std::string_view s, std::string_view key) {
  std::string pattern = std::string("\"") + std::string(key) + std::string("\":");
  size_t p = s.find(pattern);
  if (p == std::string::npos) return std::nullopt;
  p += pattern.size();
  if (s.compare(p, 4, "true") == 0) return true;
  if (s.compare(p, 5, "false") == 0) return false;
  return std::nullopt;
}

// Determine the type field
inline std::optional<std::string> JsonGetType(std::string_view s) {
  return JsonGetString(s, "type");
}

inline bool ParseCursorImage(std::string_view s, CursorImageMsg& out) {
  auto w = JsonGetInt(s, "w");
  auto h = JsonGetInt(s, "h");
  if (!w || !h) return false;
  out.w = *w;
  out.h = *h;
  if (auto hsx = JsonGetInt(s, "hotspotX")) out.hotspotX = *hsx;
  if (auto hsy = JsonGetInt(s, "hotspotY")) out.hotspotY = *hsy;
  if (auto vis = JsonGetBool(s, "visible")) out.visible = *vis;
  auto fmt = JsonGetString(s, "fmt");
  auto data_b64 = JsonGetString(s, "data");
  if (!data_b64) return false;
  out.rgba = Base64Decode(*data_b64);
  // Assume RGBA layout
  const size_t need = static_cast<size_t>(out.w) * static_cast<size_t>(out.h) * 4;
  if (out.rgba.size() < need) return false;
  return true;
}

inline bool ParseImeState(std::string_view s, ImeStateMsg& out) {
  auto open = JsonGetBool(s, "open");
  auto lang = JsonGetString(s, "lang");
  if (open) out.open = *open;
  if (lang) out.lang = *lang;
  return open.has_value() || lang.has_value();
}

}  // namespace proto
}  // namespace remote

#endif  // REMOTE_PROTO_PARSER_H_
