#ifndef AOC2022_TOKEN_HPP_
#define AOC2022_TOKEN_HPP_

#include <cstdint>
#include <string>
#include <variant>

namespace aoc2022 {

struct Source {
  std::string_view filename;
  std::string_view contents;
};

struct Location {
  bool operator==(const Location&) const = default;
  const Source* source;
  int line, column;
};

struct Identifier {
  bool operator==(const Identifier&) const = default;
  std::string value;
};

struct Integer {
  bool operator==(const Integer&) const = default;
  std::int64_t value;
};

struct Character {
  bool operator==(const Character&) const = default;
  char value;
};

struct String {
  bool operator==(const String&) const = default;
  std::string value;
};

enum class Space {
  kIndent,
  kDedent,
  kNewline,
  kEnd,
};

enum class Keyword {
  kData,
  kCase, kOf,
  kLet, kIn,
  kIf, kThen, kElse,
};

enum class Symbol {
  kAdd,                    // "+"
  kAnd,                    // "&&"
  kArrow,                  // "->"
  kCloseParen,             // ")"
  kCloseSquare,            // "]"
  kColon,                  // ":"
  kComma,                  // ","
  kCompareEqual,           // "=="
  kCompareGreater,         // ">"
  kCompareGreaterOrEqual,  // ">="
  kCompareLess,            // "<"
  kCompareLessOrEqual,     // "<="
  kCompareNotEqual,        // "!="
  kConcat,                 // "++"
  kDivide,                 // "/"
  kDot,                    // "."
  kEquals,                 // "="
  kModulo,                 // "%"
  kMultiply,               // "*"
  kOpenParen,              // "("
  kOpenSquare,             // "["
  kOr,                     // "||"
  kPipe,                   // "|"
  kSubtract,               // "-"
};

struct Token {
  // These values match the index for the corresponding type in the value
  // variant.
  enum Type {
    kIdentifier,
    kInteger,
    kCharacter,
    kString,
    kSpace,
    kKeyword,
    kSymbol,
  };

  template <typename... Args>
  Token(Location location, Args&&... args)
      : location(location), value(std::forward<Args>(args)...) {}

  Location location;
  std::variant<Identifier, Integer, Character, String, Space, Keyword, Symbol>
      value;
};

}  // namespace aoc2022

#endif  // AOC2022_TOKEN_HPP_
