import subprocess
import csv
import time
import random

puzzles = "/home/shared/chess/lichess_db_puzzle.csv"
# puzzles = "/home/shared/chess/sample.csv"

proc = subprocess.Popen(['./build/Scacus'], stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                        bufsize=1,
                        universal_newlines=True)

# PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningFamily,OpeningVariation

proc.stdin.write("isready\n")

with open(puzzles, 'r') as fd:
    reader = sorted([i for i in filter(lambda x: len(x) == 10, csv.reader(fd))], key=lambda x: int(x[3]))

engElo = 2200

k = 24

while True:
    lo = 0
    hi = len(reader) - 1
    while lo <= hi:
        mid = (lo + hi) // 2

        val = int(reader[mid][3])
        if abs(val - engElo) < 32:
            hi = mid
            hi += random.randint(-32, 32)
            break  # good enough!

        if val > engElo:
            hi = mid + 1
        elif val < engElo:
            lo = mid - 1

    se = max(min(hi, len(reader) - random.randint(0, 128)), random.randint(0, 128))
    print(f"selected number {se} of {len(reader)}")
    row = reader[se]

    if len(row) != 10:
        continue
    print(row, flush=True)
    (ident, fen, moves, rating, ratingDev, popularity, numPlays, themes, url, opening) = row
    moves = moves.split(' ')
    it = 1
    passed = True

    # factor in ratingDev?
    scale = len(moves) / 2.0
    expected = scale / (1.0 + 10.0**((float(rating) - engElo)/400.0))

    movesDeep = 0
    while it <= len(moves):

        fenCmd = "position fen %s moves %s" % (fen, " ".join(moves[:it]))
        proc.stdin.write(fenCmd + "\ngo movetime 1000\n")
        time.sleep(3)
        print("[*]\t", end='', flush=True)
        for line in proc.stdout:
            if "bestmove" in line:
                if line[9:13] != moves[it]:
                    print(f"\nFAIL {movesDeep} moves deep - Expected {moves[it]}, got {line[9:13]} in `{fenCmd}`")
                    passed = False
                    it = 9999
                    movesDeep -= 1
                break
        it += 2
        movesDeep += 1

    if passed:
        print("\nPASS!")

    deltaElo = k*(movesDeep - expected) / scale
    # print (movesDeep, expected)

    print(f"On {rating} puzzle expected {round(expected, 2)}/{scale}, scored {movesDeep}")
    print(f"Rating: {round(engElo, 1)} + {round(deltaElo, 1)} = {round(engElo + deltaElo, 1)}\n", flush=True)
    engElo += deltaElo

    with open("train.log", 'a') as fp:
        fp.write("%s: %s\n" % (time.time(), engElo))
