#ifndef AOC2022_SYNTAX_HPP_
#define AOC2022_SYNTAX_HPP_

#include "token.hpp"
#include "variant.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aoc2022::syntax {

struct ExpressionVariant;
class Expression {
 public:
  template <HoldableBy<ExpressionVariant> T>
  Expression(T value);
  const ExpressionVariant& operator*() const;
  const ExpressionVariant* operator->() const { return &(**this); }
  const Location& location() const;
  bool operator==(const Expression&) const;
 private:
  std::shared_ptr<const ExpressionVariant> value_;
};

struct Identifier {
  bool operator==(const Identifier&) const = default;
  Location location;
  std::string value;
};

struct Integer {
  bool operator==(const Integer&) const = default;
  Location location;
  std::int64_t value;
};

struct Character {
  bool operator==(const Character&) const = default;
  Location location;
  char value;
};

struct String {
  bool operator==(const String&) const = default;
  Location location;
  std::string value;
};

struct List {
  bool operator==(const List&) const = default;
  Location location;
  std::vector<Expression> elements;
};

struct Tuple {
  bool operator==(const Tuple&) const = default;
  Location location;
  std::vector<Expression> elements;
};

struct Add {
  bool operator==(const Add&) const = default;
  Location location;
  Expression a, b;
};

struct Subtract {
  bool operator==(const Subtract&) const = default;
  Location location;
  Expression a, b;
};

struct Multiply {
  bool operator==(const Multiply&) const = default;
  Location location;
  Expression a, b;
};

struct Divide {
  bool operator==(const Divide&) const = default;
  Location location;
  Expression a, b;
};

struct Modulo {
  bool operator==(const Modulo&) const = default;
  Location location;
  Expression a, b;
};

struct LessThan {
  bool operator==(const LessThan&) const = default;
  Location location;
  Expression a, b;
};

struct LessOrEqual {
  bool operator==(const LessOrEqual&) const = default;
  Location location;
  Expression a, b;
};

struct GreaterThan {
  bool operator==(const GreaterThan&) const = default;
  Location location;
  Expression a, b;
};

struct GreaterOrEqual {
  bool operator==(const GreaterOrEqual&) const = default;
  Location location;
  Expression a, b;
};

struct Equal {
  bool operator==(const Equal&) const = default;
  Location location;
  Expression a, b;
};

struct NotEqual {
  bool operator==(const NotEqual&) const = default;
  Location location;
  Expression a, b;
};

struct And {
  bool operator==(const And&) const = default;
  Location location;
  Expression a, b;
};

struct Or {
  bool operator==(const Or&) const = default;
  Location location;
  Expression a, b;
};

struct Not {
  bool operator==(const Not&) const = default;
  Location location;
  Expression inner;
};

struct Cons {
  bool operator==(const Cons&) const = default;
  Location location;
  Expression head, tail;
};

struct Concat {
  bool operator==(const Concat&) const = default;
  Location location;
  Expression a, b;
};

struct Apply {
  bool operator==(const Apply&) const = default;
  Location location;
  Expression f, x;
};

struct Compose {
  bool operator==(const Compose&) const = default;
  Location location;
  Expression f, g;
};

struct Alternative {
  bool operator==(const Alternative&) const = default;
  Location location;
  Expression pattern, value;
};

struct Case {
  bool operator==(const Case&) const = default;
  Location location;
  Expression value;
  std::vector<Alternative> alternatives;
};

struct Binding {
  Location location;
  Identifier name;
  std::vector<Identifier> parameters;
  Expression value;
};

struct Let {
  bool operator==(const Let&) const = default;
  Location location;
  std::vector<Binding> bindings;
  Expression value;
};

struct If {
  bool operator==(const If&) const = default;
  Location location;
  Expression condition, then_branch, else_branch;
};

struct ExpressionVariant {
  bool operator==(const ExpressionVariant&) const = default;
  std::variant<Identifier, Integer, Character, String, List, Tuple, Add,
               Subtract, Multiply, Divide, Modulo, LessThan, LessOrEqual,
               GreaterThan, GreaterOrEqual, Equal, NotEqual, And, Or, Not, Cons,
               Concat, Apply, Compose, Case, Let, If>
      value;
};

template <HoldableBy<ExpressionVariant> T>
Expression::Expression(T value)
    : value_(std::make_shared<ExpressionVariant>(
          ExpressionVariant{.value = std::move(value)})) {}

inline const ExpressionVariant& Expression::operator*() const {
  return *value_;
}

inline const Location& Expression::location() const {
  return std::visit([](const auto& x) -> const Location& { return x.location; },
                    value_->value);
}

struct DataDefinition {
  struct Alternative {
    bool operator==(const Alternative&) const = default;
    Location location;
    Identifier name;
    std::vector<Expression> members;
  };

  bool operator==(const DataDefinition&) const = default;
  Location location;
  Identifier name;
  std::vector<Identifier> parameters;
  std::vector<Alternative> alternatives;
};

struct Program {
  bool operator==(const Program&) const = default;
  std::vector<DataDefinition> data_definitions;
  std::vector<Binding> definitions;
  Location end;
};

}  // namespace aoc2022::syntax

#endif  // AOC2022_SYNTAX_HPP_
