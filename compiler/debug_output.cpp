#include "debug_output.hpp"

#include <iomanip>

namespace aoc2022 {

std::ostream& operator<<(std::ostream& output, const Identifier& x) {
  return output << "Identifier(" << std::quoted(x.value) << ")";
}

std::ostream& operator<<(std::ostream& output, Integer x) {
  return output << "Integer(" << x.value << ")";
}

std::ostream& operator<<(std::ostream& output, Character x) {
  const std::string_view value(&x.value, 1);
  return output << "Character(" << std::quoted(value, '\'') << ")";
}

std::ostream& operator<<(std::ostream& output, const String& x) {
  return output << "String(" << std::quoted(x.value) << ")";
}

std::ostream& operator<<(std::ostream& output, Space x) {
  switch (x) {
    case Space::kIndent:
      return output << "Space::kIndent";
    case Space::kDedent:
      return output << "Space::kDedent";
    case Space::kNewline:
      return output << "Space::kNewline";
  }
  abort();
}

std::ostream& operator<<(std::ostream& output, Keyword x) {
  switch (x) {
    case Keyword::kCase:
      return output << "Keyword::kCase";
    case Keyword::kOf:
      return output << "Keyword::kOf";
    case Keyword::kLet:
      return output << "Keyword::kLet";
    case Keyword::kIn:
      return output << "Keyword::kIn";
    case Keyword::kIf:
      return output << "Keyword::kIf";
    case Keyword::kThen:
      return output << "Keyword::kThen";
    case Keyword::kElse:
      return output << "Keyword::kElse";
  }
  abort();
}

std::ostream& operator<<(std::ostream& output, Symbol x) {
  switch (x) {
    case Symbol::kOpenParen:
      return output << "Symbol::kOpenParen";
    case Symbol::kCloseParen:
      return output << "Symbol::kCloseParen";
    case Symbol::kOpenSquare:
      return output << "Symbol::kOpenSquare";
    case Symbol::kCloseSquare:
      return output << "Symbol::kCloseSquare";
    case Symbol::kEquals:
      return output << "Symbol::kEquals";
    case Symbol::kDot:
      return output << "Symbol::kDot";
    case Symbol::kColon:
      return output << "Symbol::kColon";
    case Symbol::kPlus:
      return output << "Symbol::kPlus";
    case Symbol::kArrow:
      return output << "Symbol::kArrow";
    case Symbol::kLess:
      return output << "Symbol::kLess";
  }
  abort();
}

std::ostream& operator<<(std::ostream& output, const Token& t) {
  std::visit([&output](const auto& x) { output << x; }, t.value);
  return output;
}

}  // namespace aoc2022
