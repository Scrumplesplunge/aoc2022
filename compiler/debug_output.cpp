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
    case Space::kEnd:
      return output << "Space::kEnd";
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
    case Symbol::kComma:
      return output << "Symbol::kComma";
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

namespace syntax {

std::ostream& operator<<(std::ostream& output, const Identifier& x) {
  return output << "Identifier(" << std::quoted(x.value) << ")";
}

std::ostream& operator<<(std::ostream& output, const Integer& x) {
  return output << "Integer(" << x.value << ")";
}

std::ostream& operator<<(std::ostream& output, const Character& x) {
  const std::string_view value(&x.value, 1);
  return output << "Character(" << std::quoted(value, '\'') << ")";
}

std::ostream& operator<<(std::ostream& output, const String& x) {
  return output << "String(" << std::quoted(x.value) << ")";
}

std::ostream& operator<<(std::ostream& output, const List& x) {
  if (x.elements.empty()) {
    return output << "List({})";
  } else {
    output << "List({" << x.elements[0];
    for (int i = 1, n = x.elements.size(); i < n; i++) {
      output << ", " << x.elements[i];
    }
    return output << "})";
  }
}

std::ostream& operator<<(std::ostream& output, const LessThan& x) {
  return output << "LessThan(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Add& x) {
  return output << "Add(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Cons& x) {
  return output << "Cons(" << x.head << ", " << x.tail << ")";
}

std::ostream& operator<<(std::ostream& output, const Apply& x) {
  return output << "Apply(" << x.f << ", " << x.x << ")";
}

std::ostream& operator<<(std::ostream& output, const Compose& x) {
  return output << "Compose(" << x.f << ", " << x.g << ")";
}

std::ostream& operator<<(std::ostream& output, const Alternative& x) {
  return output << "Alternative(" << x.pattern << ", " << x.value << ")";
}

std::ostream& operator<<(std::ostream& output, const Case& x) {
  if (x.alternatives.empty()) {
    return output << "Case(" << x.value << ", {})";
  } else {
    output << "Case(" << x.value << ", {" << x.alternatives[0];
    for (int i = 1, n = x.alternatives.size(); i < n; i++) {
      output << ", " << x.alternatives[i];
    }
    return output << "})";
  }
}

std::ostream& operator<<(std::ostream& output, const If& x) {
  return output << "If(" << x.condition << ", " << x.then_branch << ", "
                << x.else_branch << ")";
}

std::ostream& operator<<(std::ostream& output, const Expression& x) {
  std::visit([&output](const auto& x) { output << x; }, x->value);
  return output;
}

std::ostream& operator<<(std::ostream& output, const Definition& x) {
  if (x.parameters.empty()) {
    return output << "Definition(" << x.name << ", {}, " << x.value << ")";
  } else {
    output << "Definition(" << x.name << ", {" << x.parameters[0];
    for (int i = 1, n = x.parameters.size(); i < n; i++) {
      output << ", " << x.parameters[i];
    }
    return output << "}, " << x.value << ")";
  }
}

std::ostream& operator<<(std::ostream& output, const Program& x) {
  if (x.definitions.empty()) {
    return output << "Program({})";
  } else {
    output << "Program({" << x.definitions[0];
    for (int i = 1, n = x.definitions.size(); i < n; i++) {
      output << ", " << x.definitions[i];
    }
    return output << "})";
  }
}

}  // namespace syntax
}  // namespace aoc2022
