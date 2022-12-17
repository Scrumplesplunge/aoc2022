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

constexpr aoc2022::Source kPrelude = {.filename = "prelude", .contents = R"(
head xs = case xs of
  (x : xs') -> x
tail xs = case xs of
  (x : xs') -> xs'

null xs =
  case xs of
    [] -> True
    xs -> False

length xs = length' 0 xs
length' n xs =
  case xs of
    [] -> n
    (x : xs') -> length' (n + 1) xs'

delete x ys =
  case ys of
    [] -> []
    (y : ys') ->
      if x == y then
        ys'
      else
        y : delete x ys'

nub xs =
  case xs of
    [] -> []
    (x : xs') -> x : delete x (nub xs')

tails xs =
  case xs of
    [] -> [[]]
    (x : xs') -> xs : tails xs'

map f xs =
  case xs of
    [] -> []
    (x : xs') -> f x : map f xs'

filter p xs =
  case xs of
    [] -> []
    (x : xs') -> if p x then x : filter p xs' else filter p xs'

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

drop n xs =
  case xs of
    [] -> []
    (x : xs') ->
      if n == 0 then
        xs
      else
        drop (n - 1) xs'

split c = split' c []
split' c first xs =
  case xs of
    [] -> if null first then [] else [reverse first]
    (x : xs') ->
      if x == c then
        reverse first : split c xs'
      else
        split' c (x : first) xs'

lines = split '\n'
words = split ' '

intersperse j xs =
  case xs of
    [] -> []
    (x : xs') -> x : intersperse' j xs'
intersperse' j xs =
  case xs of
    [] -> []
    (x : xs') -> j : x : intersperse' j xs'

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
    [] -> (ls, rs)
    (x : xs') ->
      if p x then
        partition' p (x : ls) rs xs'
      else
        partition' p ls (x : rs) xs'

flip f a b = f b a

sortBy lt xs =
  case xs of
    [] -> []
    (x : xs') ->
      case partition (flip lt x) xs' of
        (ls, rs) -> sortBy lt ls ++ [x] ++ sortBy lt rs

lt a b = a < b
even x = x % 2 == 0
odd = not . even

sort = sortBy lt

min a b = if a < b then a else b
max a b = if a < b then b else a
minimum xs = foldl min (head xs) (tail xs)
maximum xs = foldl max (head xs) (tail xs)

all f xs =
  case xs of
    [] -> True
    (x : xs') -> f x && all f xs'
any f xs =
  case xs of
    [] -> False
    (x : xs') -> f x || any f xs'

fst x = case x of
  (a, b) -> a
snd x = case x of
  (a, b) -> b

const x y = x
id x = x

iterate f x = x : iterate f (f x)
)"};

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
