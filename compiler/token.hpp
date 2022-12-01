#ifndef AOC2022_TOKEN_HPP_
#define AOC2022_TOKEN_HPP_

#include <cstdint>
#include <string>
#include <variant>

namespace aoc2022 {

struct Identifier { std::string value; };
struct Integer { std::int64_t value; };
struct Character { char value; };
struct String { std::string value; };

enum class Space {
  kIndent,
  kDedent,
  kNewline,
};

enum class Keyword {
  kCase, kOf,
  kLet, kIn,
  kIf, kThen, kElse,
};

enum class Symbol {
  kOpenParen,    // "("
  kCloseParen,   // ")"
  kOpenSquare,   // "["
  kCloseSquare,  // "]"
  kEquals,       // "="
  kDot,          // "."
  kColon,        // ":"
  kPlus,         // "+"
  kArrow,        // "->"
  kLess,         // "<"
};

struct Token {
  template <typename... Args>
  Token(Args&&... args) : value(std::forward<Args>(args)...) {}

  std::variant<Identifier, Integer, Character, String, Space, Keyword, Symbol>
      value;
};

}  // namespace aoc2022

#endif  // AOC2022_TOKEN_HPP_
