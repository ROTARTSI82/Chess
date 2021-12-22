mine = """
a1b1: 41
a1c1: 41
a1d1: 41
e1d1: 41
e1f1: 41
e1g1: 41
e1c1: 41
h1f1: 41
h1g1: 41
a2a3: 42
a2a4: 42
b2b3: 40
d2c1: 41
d2e3: 41
d2f4: 41
d2g5: 39
d2h6: 37
e2d1: 42
e2f1: 42
e2d3: 40
e2c4: 39
e2b5: 38
e2a6: 35
g2h3: 41
g2g3: 40
g2g4: 40
c3b1: 40
c3d1: 40
c3a4: 40
c3b5: 37
c3d5: 39
f3d3: 40
f3e3: 41
f3g3: 41
f3h3: 41
f3f4: 41
f3g4: 41
f3f5: 42
f3h5: 41
f3f6: 38
e5d3: 43
e5c4: 42
e5g4: 44
e5c6: 43
e5g6: 0
e5d7: 2
f7g8q: 2
f7g8b: 5
f7g8n: 5
f7g8r: 2
""".replace(" -", ":")

stock = """
a2a3: 42
b2b3: 40
g2g3: 40
a2a4: 42
g2g4: 40
f7g8q: 1
f7g8r: 1
f7g8b: 5
f7g8n: 5
g2h3: 41
c3b1: 40
c3d1: 40
c3a4: 40
c3b5: 37
c3d5: 39
e5d3: 43
e5c4: 42
e5g4: 44
e5c6: 43
e5g6: 0
e5d7: 2
d2c1: 41
d2e3: 41
d2f4: 41
d2g5: 39
d2h6: 37
e2d1: 42
e2f1: 42
e2d3: 40
e2c4: 39
e2b5: 38
e2a6: 35
a1b1: 41
a1c1: 41
a1d1: 41
h1f1: 41
h1g1: 41
f3d3: 40
f3e3: 41
f3g3: 41
f3h3: 41
f3f4: 41
f3g4: 41
f3f5: 42
f3h5: 41
f3f6: 38
e1d1: 41
e1f1: 41
e1g1: 41
e1c1: 41


"""

stockset = set(stock.split("\n"))
myset = set(mine.split("\n"))
print ("Missing moves: ", stockset - myset)
print ("Extra moves:", myset - stockset)

rm_dup = lambda x : list(set(x))

me = sorted(rm_dup(mine.split("\n")))
them = sorted(rm_dup(stock.split("\n")))

get = lambda arr, i : arr[i] if i < len(arr) else None

print ("my output\tcorrect output")
for i in range(max(len(me), len(them))):
    if (get(me, i) != get(them, i)):
        print(get(me, i), "\t", get(them, i))
