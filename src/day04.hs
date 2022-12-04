import Data.Char
import Text.ParserCombinators.ReadP (ReadP, munch, string, readP_to_S)

int :: ReadP Int
int = fmap read (munch isDigit)
range = do
  l <- int
  string "-"
  r <- int
  return (l, r)
pair = do
  l <- range
  string ","
  r <- range
  return (l, r)
retrieve xs =
  case xs of
    [x] -> x
    _ -> error ("is bad: " ++ show xs)
parsePair = fst . retrieve . readP_to_S pair

count = count' 0
count' n f [] = n
count' n f (x : xs) = count' (if f x then n + 1 else n) f xs
includes (a, b) (c, d) = a <= c && d <= b
overlaps (a, b) (c, d) = not (b < c || d < a)
eitherWay f (a, b) = f a b || f b a

part1 = count (eitherWay includes)
part2 = count (uncurry overlaps)

main = do
  input <- fmap (map parsePair . lines) getContents
  putStrLn $ show $ part1 input
  putStrLn $ show $ part2 input
