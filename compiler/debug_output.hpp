#ifndef AOC2022_DEBUG_OUTPUT_HPP_
#define AOC2022_DEBUG_OUTPUT_HPP_

#include "core.hpp"
#include "token.hpp"
#include "syntax.hpp"

#include <iostream>

namespace aoc2022 {

std::ostream& operator<<(std::ostream&, const Identifier&);
std::ostream& operator<<(std::ostream&, Integer);
std::ostream& operator<<(std::ostream&, Character);
std::ostream& operator<<(std::ostream&, const String&);
std::ostream& operator<<(std::ostream&, Space);
std::ostream& operator<<(std::ostream&, Keyword);
std::ostream& operator<<(std::ostream&, Symbol);
std::ostream& operator<<(std::ostream&, const Token&);

namespace syntax {

std::ostream& operator<<(std::ostream&, const Identifier&);
std::ostream& operator<<(std::ostream&, const Integer&);
std::ostream& operator<<(std::ostream&, const Character&);
std::ostream& operator<<(std::ostream&, const String&);
std::ostream& operator<<(std::ostream&, const List&);
std::ostream& operator<<(std::ostream&, const Tuple&);
std::ostream& operator<<(std::ostream&, const BitwiseAnd&);
std::ostream& operator<<(std::ostream&, const BitwiseOr&);
std::ostream& operator<<(std::ostream&, const Add&);
std::ostream& operator<<(std::ostream&, const Subtract&);
std::ostream& operator<<(std::ostream&, const Multiply&);
std::ostream& operator<<(std::ostream&, const Divide&);
std::ostream& operator<<(std::ostream&, const Modulo&);
std::ostream& operator<<(std::ostream&, const LessThan&);
std::ostream& operator<<(std::ostream&, const LessOrEqual&);
std::ostream& operator<<(std::ostream&, const GreaterThan&);
std::ostream& operator<<(std::ostream&, const GreaterOrEqual&);
std::ostream& operator<<(std::ostream&, const Equal&);
std::ostream& operator<<(std::ostream&, const NotEqual&);
std::ostream& operator<<(std::ostream&, const And&);
std::ostream& operator<<(std::ostream&, const Or&);
std::ostream& operator<<(std::ostream&, const Not&);
std::ostream& operator<<(std::ostream&, const Cons&);
std::ostream& operator<<(std::ostream&, const Concat&);
std::ostream& operator<<(std::ostream&, const Apply&);
std::ostream& operator<<(std::ostream&, const Compose&);
std::ostream& operator<<(std::ostream&, const Alternative&);
std::ostream& operator<<(std::ostream&, const Case&);
std::ostream& operator<<(std::ostream&, const Binding&);
std::ostream& operator<<(std::ostream&, const Let&);
std::ostream& operator<<(std::ostream&, const If&);
std::ostream& operator<<(std::ostream&, const Expression&);
std::ostream& operator<<(std::ostream&, const DataDefinition::Alternative&);
std::ostream& operator<<(std::ostream&, const DataDefinition&);
std::ostream& operator<<(std::ostream&, const Program&);

}  // namespace syntax

namespace core {

std::ostream& operator<<(std::ostream&, const Identifier&);

std::ostream& operator<<(std::ostream&, const TupleType&);
std::ostream& operator<<(std::ostream&, const UnionType::Id&);
std::ostream& operator<<(std::ostream&, const UnionType&);

std::ostream& operator<<(std::ostream&, const MatchTuple&);
std::ostream& operator<<(std::ostream&, const MatchUnion&);
std::ostream& operator<<(std::ostream&, const Integer&);
std::ostream& operator<<(std::ostream&, const Character&);
std::ostream& operator<<(std::ostream&, const Pattern&);

std::ostream& operator<<(std::ostream&, const Builtin&);
std::ostream& operator<<(std::ostream&, const Tuple&);
std::ostream& operator<<(std::ostream&, const UnionConstructor&);
std::ostream& operator<<(std::ostream&, const Apply&);
std::ostream& operator<<(std::ostream&, const Lambda&);
std::ostream& operator<<(std::ostream&, const Binding&);
std::ostream& operator<<(std::ostream&, const Let&);
std::ostream& operator<<(std::ostream&, const LetRecursive&);
std::ostream& operator<<(std::ostream&, const Case::Alternative&);
std::ostream& operator<<(std::ostream&, const Case&);
std::ostream& operator<<(std::ostream&, const Expression&);

}  // namespace core
}  // namespace aoc2022

#endif  // AOC2022_DEBUG_OUTPUT_HPP_
