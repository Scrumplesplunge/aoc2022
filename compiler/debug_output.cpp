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
  std::abort();
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
  std::abort();
}

std::ostream& operator<<(std::ostream& output, Symbol x) {
  switch (x) {
    case Symbol::kAdd:
      return output << "Symbol::kAdd";
    case Symbol::kAnd:
      return output << "Symbol::kAnd";
    case Symbol::kArrow:
      return output << "Symbol::kArrow";
    case Symbol::kCloseParen:
      return output << "Symbol::kCloseParen";
    case Symbol::kCloseSquare:
      return output << "Symbol::kCloseSquare";
    case Symbol::kColon:
      return output << "Symbol::kColon";
    case Symbol::kComma:
      return output << "Symbol::kComma";
    case Symbol::kCompareEqual:
      return output << "Symbol::kCompareEqual";
    case Symbol::kCompareGreater:
      return output << "Symbol::kCompareGreater";
    case Symbol::kCompareGreaterOrEqual:
      return output << "Symbol::kCompareGreaterOrEqual";
    case Symbol::kCompareLess:
      return output << "Symbol::kCompareLess";
    case Symbol::kCompareLessOrEqual:
      return output << "Symbol::kCompareLessOrEqual";
    case Symbol::kCompareNotEqual:
      return output << "Symbol::kCompareNotEqual";
    case Symbol::kConcat:
      return output << "Symbol::kConcat";
    case Symbol::kDivide:
      return output << "Symbol::kDivide";
    case Symbol::kDot:
      return output << "Symbol::kDot";
    case Symbol::kEquals:
      return output << "Symbol::kEquals";
    case Symbol::kModulo:
      return output << "Symbol::kModulo";
    case Symbol::kMultiply:
      return output << "Symbol::kMultiply";
    case Symbol::kOpenParen:
      return output << "Symbol::kOpenParen";
    case Symbol::kOpenSquare:
      return output << "Symbol::kOpenSquare";
    case Symbol::kOr:
      return output << "Symbol::kOr";
    case Symbol::kSubtract:
      return output << "Symbol::kSubtract";
  }
  std::abort();
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

std::ostream& operator<<(std::ostream& output, const Add& x) {
  return output << "Add(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Subtract& x) {
  return output << "Subtract(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Multiply& x) {
  return output << "Multiply(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Divide& x) {
  return output << "Divide(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Modulo& x) {
  return output << "Modulo(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const LessThan& x) {
  return output << "LessThan(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const LessOrEqual& x) {
  return output << "LessOrEqual(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const GreaterThan& x) {
  return output << "GreaterThan(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const GreaterOrEqual& x) {
  return output << "GreaterOrEqual(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Equal& x) {
  return output << "Equal(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const NotEqual& x) {
  return output << "NotEqual(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const And& x) {
  return output << "And(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Or& x) {
  return output << "Or(" << x.a << ", " << x.b << ")";
}

std::ostream& operator<<(std::ostream& output, const Not& x) {
  return output << "Not(" << x.inner << ")";
}

std::ostream& operator<<(std::ostream& output, const Cons& x) {
  return output << "Cons(" << x.head << ", " << x.tail << ")";
}

std::ostream& operator<<(std::ostream& output, const Concat& x) {
  return output << "Concat(" << x.a << ", " << x.b << ")";
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

std::ostream& operator<<(std::ostream& output, const Binding& x) {
  if (x.parameters.empty()) {
    return output << "Binding(" << x.name << ", {}, " << x.value << ")";
  } else {
    output << "Binding(" << x.name << ", {" << x.parameters[0];
    for (int i = 1, n = x.parameters.size(); i < n; i++) {
      output << ", " << x.parameters[i];
    }
    return output << "}, " << x.value << ")";
  }
}

std::ostream& operator<<(std::ostream& output, const Let& x) {
  if (x.bindings.empty()) {
    return output << "Let({}, " << x.value << ")";
  } else {
    output << "Let({" << x.bindings[0];
    for (int i = 1, n = x.bindings.size(); i < n; i++) {
      output << ", " << x.bindings[i];
    }
    return output << "}, " << x.value << ")";
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

namespace core {

std::ostream& operator<<(std::ostream& output, const Identifier& x) {
  return output << "Identifier(" << (int)x << ")";
}

std::ostream& operator<<(std::ostream& output, const Decons& x) {
  return output << "Decons(" << x.head << ", " << x.tail << ")";
}

std::ostream& operator<<(std::ostream& output, const Pattern& x) {
  std::visit([&output](const auto& x) { output << x; }, x->value);
  return output;
}

std::ostream& operator<<(std::ostream& output, const Builtin& x) {
  switch (x) {
    case Builtin::kAdd:
      return output << "Builtin::kAdd";
    case Builtin::kAnd:
      return output << "Builtin::kAnd";
    case Builtin::kConcat:
      return output << "Builtin::kConcat";
    case Builtin::kDivide:
      return output << "Builtin::kDivide";
    case Builtin::kEqual:
      return output << "Builtin::kEqual";
    case Builtin::kFalse:
      return output << "Builtin::kFalse";
    case Builtin::kLessThan:
      return output << "Builtin::kLessThan";
    case Builtin::kModulo:
      return output << "Builtin::kModulo";
    case Builtin::kMultiply:
      return output << "Builtin::kMultiply";
    case Builtin::kNil:
      return output << "Builtin::kNil";
    case Builtin::kNot:
      return output << "Builtin::kNot";
    case Builtin::kOr:
      return output << "Builtin::kOr";
    case Builtin::kReadInt:
      return output << "Builtin::kReadInt";
    case Builtin::kShowInt:
      return output << "Builtin::kShowInt";
    case Builtin::kSubtract:
      return output << "Builtin::kSubtract";
    case Builtin::kTrue:
      return output << "Builtin::kTrue";
  }
  std::abort();
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

std::ostream& operator<<(std::ostream& output, const Cons& x) {
  return output << "Cons(" << x.head << ", " << x.tail << ")";
}

std::ostream& operator<<(std::ostream& output, const Apply& x) {
  return output << "Apply(" << x.f << ", " << x.x << ")";
}

std::ostream& operator<<(std::ostream& output, const Lambda& x) {
  return output << "Lambda(" << x.parameter << ", " << x.result << ")";
}

std::ostream& operator<<(std::ostream& output, const Binding& x) {
  return output << "Binding(" << x.name << ", " << x.result << ")";
}

std::ostream& operator<<(std::ostream& output, const Let& x) {
  return output << "Let(" << x.binding << ", " << x.result << ")";
}

std::ostream& operator<<(std::ostream& output, const LetRecursive& x) {
  output << "LetRecursive({";
  if (!x.bindings.empty()) {
    output << x.bindings[0];
    for (int i = 1, n = x.bindings.size(); i < n; i++) {
      output << ", " << x.bindings[i];
    }
  }
  return output << "}, " << x.result << ")";
}

std::ostream& operator<<(std::ostream& output, const Case::Alternative& x) {
  return output << "Alternative(" << x.pattern << ", " << x.value << ")";
}

std::ostream& operator<<(std::ostream& output, const Case& x) {
  output << "Case(" << x.value << ", {";
  if (x.alternatives.empty()) {
    return output << "})";
  } else {
    output << x.alternatives[0];
    for (int i = 1, n = x.alternatives.size(); i < n; i++) {
      output << ", " << x.alternatives[i];
    }
    return output << "})";
  }
}

std::ostream& operator<<(std::ostream& output, const Expression& x) {
  std::visit([&output](const auto& x) { output << x; }, x->value);
  return output;
}

}  // namespace core
}  // namespace aoc2022
