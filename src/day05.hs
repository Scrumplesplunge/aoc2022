import Data.Maybe
import Data.List
import Data.Char
import Text.ParserCombinators.ReadP

type Box = Char
type Stack = String
data Move = Move Int Int Int deriving Show
data Puzzle = Puzzle [Stack] [Move] deriving Show

box = do
  string "["
  c <- get
  string "]"
  return (Just c)
gap = string "   " >> return Nothing
maybeBox = choice [box, gap]
row = do
  first <- maybeBox
  rest <- sequence $ replicate 8 $ string " " >> maybeBox
  string "\n"
  return (first : rest)
stackLabels = string " 1   2   3   4   5   6   7   8   9 \n\n"
stacks = do
  raw <- manyTill row stackLabels
  return $ map catMaybes $ transpose $ raw

int :: ReadP Int
int = fmap read (munch isDigit)
move = do
  string "move "
  count <- int
  string " from "
  from <- int
  string " to "
  to <- int
  string "\n"
  return (Move count from to)
puzzle = do
  s <- stacks
  m <- manyTill move eof
  return (Puzzle s m)
retrieve xs =
  case xs of
    [x] -> x
    _ -> error ("not exactly one: " ++ show xs)
parseInput = fst . retrieve . readP_to_S puzzle

apply :: (Stack -> Stack) -> [Stack] -> Move -> [Stack]
apply f stacks (Move count from to) =
  let (moved, newFrom) = splitAt count (stacks !! (from - 1))
      newTo = f moved ++ (stacks !! (to - 1))
      results i [] = []
      results i (stack : stacks')
        | i == to = newTo : results (i + 1) stacks'
        | i == from = newFrom : results (i + 1) stacks'
        | otherwise = stack : results (i + 1) stacks'
  in results 1 stacks

run f (Puzzle stacks moves) = putStrLn $ map head $ foldl (apply f) stacks moves

main = do
  input <- fmap parseInput getContents
  run reverse input
  run id input
