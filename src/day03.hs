import Data.Char
import Data.List

halve xs = splitAt (length xs `div` 2) xs
common xs = let (as, bs) = halve xs in intersect (nub as) (nub bs)
retrieve [x] = x
retrieve _ = error "not exactly one"
priority x
  | isLower x = ord x - ord 'a' + 1
  | isUpper x = ord x - ord 'A' + 27
part1 = sum . map (priority . retrieve . common)

chunks [] = []
chunks (a : b : c : xs) = (a, b, c) : chunks xs
badge (a, b, c) = priority $ retrieve $ nub $ intersect a $ intersect b c
part2 = sum . map badge . chunks

main = do
  rucksacks <- fmap lines getContents
  putStrLn $ show $ part1 rucksacks
  putStrLn $ show $ part2 rucksacks
