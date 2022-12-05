#ifndef AOC2022_LEXER_HPP_
#define AOC2022_LEXER_HPP_

#include <string_view>
#include <vector>

#include "token.hpp"

namespace aoc2022 {

std::vector<Token> Lex(const Source& source);

}  // namespace aoc2022

#endif  // AOC2022_LEXER_HPP_
