import Data.List

-- Parse the input into a list of calorie sums per elf.
totals = totals' 0 . reverse . lines
totals' x [] = [x]
totals' x ("" : ls) = x : totals' 0 ls
totals' x (l : ls) = totals' (x + read l) ls

part1 = foldr max 0
part2 = sum . take 3 . reverse . sort

main = do
  input <- fmap totals getContents
  putStrLn (show (part1 input))
  putStrLn (show (part2 input))
