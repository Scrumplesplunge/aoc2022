#ifndef AOC2022_CORE_HPP_
#define AOC2022_CORE_HPP_

#include "token.hpp"
#include "variant.hpp"

#include <memory>
#include <vector>

namespace aoc2022::core {

enum class Identifier : int {};

struct TupleType {
  bool operator==(const TupleType&) const = default;
  int num_members;
};

struct UnionType {
  enum class Id : int {
    kBool,
    kList,
    kFirstUserType,
  };
  bool operator==(const UnionType&) const = default;
  Id id;
  std::vector<TupleType> alternatives;
};

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

struct MatchTuple {
  bool operator==(const MatchTuple&) const = default;
  std::vector<Identifier> elements;
};

struct MatchUnion {
  bool operator==(const MatchUnion&) const = default;
  std::shared_ptr<const UnionType> type;
  int index;
  std::vector<Identifier> elements;
};

struct Integer {
  bool operator==(const Integer&) const = default;
  std::int64_t value;
};

struct Character {
  bool operator==(const Character&) const = default;
  char value;
};

struct PatternVariant {
  bool operator==(const PatternVariant&) const = default;
  std::variant<Identifier, MatchTuple, MatchUnion, Integer, Character> value;
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

enum class Builtin {
  kAdd,
  kAnd,
  kBitShift,
  kBitwiseAnd,
  kBitwiseOr,
  kChr,
  kConcat,
  kDivide,
  kError,
  kEqual,
  kLessThan,
  kModulo,
  kMultiply,
  kNot,
  kOr,
  kOrd,
  kReadInt,
  kShowInt,
  kSubtract,
};

struct Tuple {
  bool operator==(const Tuple&) const = default;
  std::vector<Expression> elements;
};

struct UnionConstructor {
  bool operator==(const UnionConstructor&) const = default;
  std::shared_ptr<const UnionType> type;
  int index;
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
  std::variant<Builtin, Identifier, Integer, Character, Tuple, UnionConstructor,
               Apply, Lambda, Let, LetRecursive, Case>
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
