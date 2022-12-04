data Hand = Rock | Paper | Scissors

Rock `beats` Scissors = True
Scissors `beats` Paper = True
Paper `beats` Rock = True
_ `beats` _ = False

outcome yours theirs
  | yours `beats` theirs = 6
  | theirs `beats` yours = 0
  | otherwise = 3

value Rock = 1
value Paper = 2
value Scissors = 3

score (theirs, yours) = value yours + outcome yours theirs

parseRow [l, ' ', r] = (l, r)
parseInput = map parseRow . lines

decode 'A' = Rock
decode 'B' = Paper
decode 'C' = Scissors

part1 _ 'X' = Rock
part1 _ 'Y' = Paper
part1 _ 'Z' = Scissors

part2 Rock 'X' = Scissors
part2 Rock 'Y' = Rock
part2 Rock 'Z' = Paper
part2 Paper 'X' = Rock
part2 Paper 'Y' = Paper
part2 Paper 'Z' = Scissors
part2 Scissors 'X' = Paper
part2 Scissors 'Y' = Scissors
part2 Scissors 'Z' = Rock

play strategy (l, r) = score (theirs, yours)
  where theirs = decode l
        yours = strategy theirs r

total strategy = sum . map (play strategy)

main = do
  input <- fmap parseInput getContents
  putStrLn $ show $ total part1 $ input
  putStrLn $ show $ total part2 $ input
