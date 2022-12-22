#ifndef AOC2022_CORE_HPP_
#define AOC2022_CORE_HPP_

#include "token.hpp"
#include "variant.hpp"

#include <memory>
#include <vector>

namespace aoc2022::core {

enum class Identifier : int {};

struct TypeVariant;
class Type {
 public:
  template <HoldableBy<TypeVariant> T>
  Type(T value);
  const TypeVariant& operator*() const;
  const TypeVariant* operator->() const { return &(**this); }
  bool operator==(const Type&) const;
 private:
  std::shared_ptr<const TypeVariant> value_;
};

enum class BuiltinType {
  kChar,
  kInt64,
};

struct TypeVariable {
  bool operator==(const TypeVariable&) const = default;
  Identifier id;
};

struct TupleType {
  bool operator==(const TupleType&) const = default;
  std::vector<Type> elements;
};

struct TypeConstructor {
  enum class Id : int {
    kBool,
    kList,
    kFirstUserType,
  };
  bool operator==(const TypeConstructor&) const = default;
  Id id;
  std::vector<TypeVariable> parameters;
};

struct ApplyType {
  bool operator==(const ApplyType&) const = default;
  TypeConstructor constructor;
  std::vector<Type> arguments;
};

struct ArrowType {
  bool operator==(const ArrowType&) const = default;
  Type a, b;
};

struct TypeVariant {
  bool operator==(const TypeVariant&) const = default;
  std::variant<BuiltinType, TypeVariable, TupleType, TypeConstructor, ApplyType,
               ArrowType>
      value;
};

template <HoldableBy<TypeVariant> T>
Type::Type(T value)
    : value_(std::make_shared<TypeVariant>(
          TypeVariant{.value = std::move(value)})) {}

inline const TypeVariant& Type::operator*() const {
  return *value_;
}

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

struct Variable {
  bool operator==(const Variable&) const = default;
  Type type;
  Identifier id;
};

struct MatchTuple {
  bool operator==(const MatchTuple&) const = default;
  std::vector<Variable> elements;
};

struct MatchTypeConstructor {
  bool operator==(const MatchTypeConstructor&) const = default;
  Type type;
  int index;
  std::vector<Variable> elements;
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
  std::variant<Identifier, MatchTuple, MatchTypeConstructor, Integer, Character>
      value;
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
  Type GetType() const;
 private:
  std::shared_ptr<const ExpressionVariant> value_;
};

enum class Builtin {
  kAdd,         // Int -> Int -> Int
  kAnd,         // Bool -> Bool -> Bool
  kBitShift,    // Int -> Int -> Int
  kBitwiseAnd,  // Int -> Int -> Int
  kBitwiseOr,   // Int -> Int -> Int
  kChr,         // Int -> Char
  kConcat,      // [a] -> [a] -> [a]
  kDivide,      // Int -> Int -> Int
  kEqual,       // a -> a -> Bool
  kError,       // String -> a
  kLessThan,    // a -> a -> Bool
  kModulo,      // Int -> Int -> Int
  kMultiply,    // Int -> Int -> Int
  kNot,         // Bool -> Bool
  kOr,          // Bool -> Bool -> Bool
  kOrd,         // Char -> Int
  kReadInt,     // String -> Int
  kShowInt,     // Int -> String
  kSubtract,    // Int -> Int -> Int
};

struct Tuple {
  bool operator==(const Tuple&) const = default;
  Type type;
  std::vector<Expression> elements;
};

struct DataConstructor {
  bool operator==(const DataConstructor&) const = default;
  Type type;
  int index;
};

struct Apply {
  bool operator==(const Apply&) const = default;
  Type type;
  Expression f, x;
};

struct Lambda {
  bool operator==(const Lambda&) const = default;
  Type type;
  Variable parameter;
  Expression result;
};

struct Binding {
  bool operator==(const Binding&) const = default;
  Variable variable;
  Expression value;
};

struct Let {
  bool operator==(const Let&) const = default;
  Binding binding;
  Expression value;
};

struct LetRecursive {
  bool operator==(const LetRecursive&) const = default;
  std::vector<Binding> bindings;
  Expression value;
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
  std::variant<Builtin, Variable, Integer, Character, Tuple, DataConstructor,
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
