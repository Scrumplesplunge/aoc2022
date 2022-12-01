#ifndef AOC2022_DEBUG_OUTPUT_HPP_
#define AOC2022_DEBUG_OUTPUT_HPP_

#include "token.hpp"

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

}  // namespace aoc2022

#endif  // AOC2022_DEBUG_OUTPUT_HPP_
