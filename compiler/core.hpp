#ifndef AOC2022_CORE_HPP_
#define AOC2022_CORE_HPP_

#include "token.hpp"
#include "variant.hpp"

#include <memory>
#include <vector>

namespace aoc2022::core {

enum class Identifier : int {};

struct PatternVariant;
class Pattern {
 public:
  template <HoldableBy<PatternVariant> T>
  Pattern(T value);
  const PatternVariant& operator*() const;
  const PatternVariant* operator->() const { return &(**this); }
  bool operator==(const Pattern&) const;
 private:
  std::shared_ptr<const PatternVariant> value_;
};

enum class Builtin {
  kNil,
  kTrue,
  kFalse,
  kReadInt,
  kShowInt,
  kAdd,
  kLessThan,
};

struct Decons {
  bool operator==(const Decons&) const = default;
  Identifier head, tail;
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

struct PatternVariant {
  bool operator==(const PatternVariant&) const = default;
  std::variant<Builtin, Identifier, Decons, Integer, Character> value;
};

template <HoldableBy<PatternVariant> T>
Pattern::Pattern(T value)
    : value_(std::make_shared<PatternVariant>(
          PatternVariant{.value = std::move(value)})) {}

inline const PatternVariant& Pattern::operator*() const {
  return *value_;
}

struct ExpressionVariant;
class Expression {
 public:
  template <HoldableBy<ExpressionVariant> T>
  Expression(T value);
  const ExpressionVariant& operator*() const;
  const ExpressionVariant* operator->() const { return &(**this); }
  bool operator==(const Expression&) const;
 private:
  std::shared_ptr<const ExpressionVariant> value_;
};

struct Cons {
  bool operator==(const Cons&) const = default;
  Expression head, tail;
};

struct Apply {
  bool operator==(const Apply&) const = default;
  Expression f, x;
};

struct Lambda {
  bool operator==(const Lambda&) const = default;
  Identifier parameter;
  Expression result;
};

struct Binding {
  bool operator==(const Binding&) const = default;
  Identifier name;
  Expression result;
};

struct Let {
  bool operator==(const Let&) const = default;
  Binding binding;
  Expression result;
};

struct LetRecursive {
  bool operator==(const LetRecursive&) const = default;
  std::vector<Binding> bindings;
  Expression result;
};

struct Case {
  struct Alternative {
    bool operator==(const Alternative&) const = default;
    Pattern pattern;
    Expression value;
  };

  bool operator==(const Case&) const = default;
  Expression value;
  std::vector<Alternative> alternatives;
};

struct ExpressionVariant {
  bool operator==(const ExpressionVariant&) const = default;
  std::variant<Builtin, Identifier, Integer, Character, String, Cons, Apply,
               Lambda, Let, LetRecursive, Case>
      value;
};

template <HoldableBy<ExpressionVariant> T>
Expression::Expression(T value)
    : value_(std::make_shared<ExpressionVariant>(
          ExpressionVariant{.value = std::move(value)})) {}

inline const ExpressionVariant& Expression::operator*() const {
  return *value_;
}

}  // namespace aoc2022::core

#endif  // AOC2022_CORE_HPP_
