#ifndef AOC2022_PARSER_HPP_
#define AOC2022_PARSER_HPP_

#include "token.hpp"
#include "syntax.hpp"

#include <span>

namespace aoc2022 {

syntax::Program Parse(std::span<const Token> tokens);

}  // namespace aoc2022

#endif  // AOC2022_PARSER_HPP_
