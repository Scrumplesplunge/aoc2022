#include "debug_output.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "checker.hpp"
#include "interpreter.hpp"

#include <fstream>
#include <iostream>
#include <string>

std::string GetContents(const char* filename) {
  std::ifstream file(filename);
  std::string contents = std::string(std::istreambuf_iterator<char>(file), {});
  if (!file.good()) throw std::runtime_error("can't read input");
  return contents;
}

constexpr char kPrelude[] = R"(
map f xs =
  case xs of
    [] -> []
    (x : xs') -> f x : map f xs'

reverse xs = reverse' [] xs
reverse' sx xs =
  case xs of
    [] -> sx
    (x : xs) -> reverse' (x : sx) xs

lines = lines' []
lines' first xs =
  case xs of
    [] -> reverse first
    (x : xs') ->
      case x of
        '\n' -> reverse first : lines xs'
        x -> lines' (x : first) xs'
)";

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: compiler <filename>\n";
    return 1;
  }

  // Load the prelude.
  const std::vector<aoc2022::Token> prelude_tokens = aoc2022::Lex(kPrelude);
  const aoc2022::syntax::Program prelude = aoc2022::Parse(prelude_tokens);

  const std::string source = GetContents(argv[1]);
  const std::vector<aoc2022::Token> tokens = aoc2022::Lex(source);
  aoc2022::syntax::Program program = aoc2022::Parse(tokens);
  program.definitions.insert(program.definitions.end(),
                             prelude.definitions.begin(),
                             prelude.definitions.end());
  const aoc2022::core::Expression ir = aoc2022::Check(program);
  aoc2022::Run(ir);
}
