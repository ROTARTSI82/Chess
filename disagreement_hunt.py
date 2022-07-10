import subprocess
import time

# fails 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -  at depth 6

# fails position fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1 moves b4b1 c7c6 b1g1 h4g5 g2g4
# missing f4g3

positions = [
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
]


eng = subprocess.Popen(["./build/Scacus"], stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                        bufsize=1, universal_newlines=True)
ref = subprocess.Popen(["stockfish"], stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                       bufsize=1, universal_newlines=True)

time.sleep(1)

for line in eng.stdout:
    print(line)
    if "Scacus" in line:
        break
for line in ref.stdout:
    print(line)
    if "Stockfish" in line:
        break

to_fix = []

for pos in positions:
    for depth in range(5, 6):
        movs = []

        for i in range(depth):
            cmd = "position fen %s%s\ngo perft %s\n" % (pos, " moves " + " ".join(movs) if len(movs) != 0 else "",
                                                        depth - i)
            print(cmd)

            eng.stdin.write(cmd)
            ref.stdin.write(cmd)

            time.sleep((depth - i)**2 / 2)

            mine = set()
            stock = set()

            myMovs = set()
            stockMovs = set()

            def add_to(src, dst, mvs):
                for ln in src:
                    # print(ln)
                    if "Nodes" in ln:
                        break
                    elif ": " in ln:
                        elem = ln.split(": ")
                        dst.add((elem[0], int(elem[1])))
                        mvs.add(elem[0])

            add_to(ref.stdout, stock, stockMovs)
            add_to(eng.stdout, mine, myMovs)

            if len(stock) != len(mine) or myMovs != stockMovs:
                print("Incorrect generation for %s" % cmd)
                print("\tstock:\t", sorted(stockMovs))
                print("\tmine:\t", sorted(myMovs))

                print("\n\tMoves MISSING:\t", stockMovs.difference(myMovs))
                print("\tMoves EXTRA:\t", myMovs.difference(stockMovs))
                to_fix.append(cmd)
                break

            wrong = stock.difference(mine)
            if len(wrong) == 0:
                print("Depth %s-%s All Correct" % (depth, i))
                break

            movs.append(wrong.pop()[0])


print("==== FINAL REPORT ====")
print("Positions to fix:", "\n\t".join(to_fix))
