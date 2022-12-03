#include "debug_output.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "checker.hpp"

#include <fstream>
#include <iostream>
#include <string>

std::string GetContents(const char* filename) {
  std::ifstream file(filename);
  std::string contents = std::string(std::istreambuf_iterator<char>(file), {});
  if (!file.good()) throw std::runtime_error("can't read input");
  return contents;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: compiler <filename>\n";
    return 1;
  }
  const std::string source = GetContents(argv[1]);
  const std::vector<aoc2022::Token> tokens = aoc2022::Lex(source);
  for (const auto& token : tokens) {
    std::cout << token << '\n';
  }
  std::cout << '\n';
  const aoc2022::syntax::Program program = aoc2022::Parse(tokens);
  std::cout << program << "\n\n";
  const aoc2022::core::Expression ir = aoc2022::Check(program);
  std::cout << ir << '\n';
}
