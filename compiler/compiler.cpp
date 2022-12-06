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

constexpr aoc2022::Source kPrelude = {
  .filename = "prelude",
  .contents = R"(
map f xs =
  case xs of
    [] -> []
    (x : xs') -> f x : map f xs'

reverse = reverse' []
reverse' sx xs =
  case xs of
    [] -> sx
    (x : xs') -> reverse' (x : sx) xs'

concat xs =
  case xs of
    [] -> []
    (x : xs') -> x ++ concat xs'

take n xs =
  case xs of
    [] -> []
    (x : xs') ->
      if n == 0 then
        []
      else
        x : take (n - 1) xs'

lines = lines' []
lines' first xs =
  case xs of
    [] -> reverse first
    (x : xs') ->
      case x of
        '\n' -> reverse first : lines xs'
        x -> lines' (x : first) xs'

foldr f e xs =
  case xs of
    [] -> e
    (x : xs') -> f x (foldr f e xs')

foldl f e xs =
  case xs of
    [] -> e
    (x : xs') -> foldl f (f e x) xs'

sum xs = sum' 0 xs
sum' n xs =
  case xs of
    [] -> n
    (x : xs') -> sum' (n + x) xs'

partition p = partition' p [] []
partition' p ls rs xs =
  case xs of
    [] -> [ls, rs]
    (x : xs') ->
      if x < p then
        partition' p (x : ls) rs xs'
      else
        partition' p ls (x : rs) xs'
quicksort xs =
  case xs of
    [] -> []
    (x : xs') ->
      case partition x xs' of
        (ls : tmp) -> case tmp of
          (rs : tmp2) -> quicksort ls ++ [x] ++ quicksort rs
)",
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: compiler <filename>\n";
    return 1;
  }

  // Load the prelude.
  const std::vector<aoc2022::Token> prelude_tokens = aoc2022::Lex(kPrelude);
  const aoc2022::syntax::Program prelude = aoc2022::Parse(prelude_tokens);

  const std::string contents = GetContents(argv[1]);
  const aoc2022::Source source = {.filename = argv[1], .contents = contents};
  const std::vector<aoc2022::Token> tokens = aoc2022::Lex(source);
  aoc2022::syntax::Program program = aoc2022::Parse(tokens);
  program.definitions.insert(program.definitions.end(),
                             prelude.definitions.begin(),
                             prelude.definitions.end());
  const aoc2022::core::Expression ir = aoc2022::Check(program);
  aoc2022::Run(ir);
}
