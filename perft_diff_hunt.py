mine = """

b1a3 - 120142144
b1c3 - 148527161
g1f3 - 147678554
g1h3 - 120669525
a2a3 - 106743106
a2a4 - 137077337
b2b3 - 133233975
b2b4 - 134087476
c2c3 - 144074944
c2c4 - 157756443
d2d3 - 227598692
d2d4 - 269605599
e2e3 - 306138410
e2e4 - 309478263
f2f3 - 102021008
f2f4 - 119614841
g2g3 - 135987651
g2g4 - 130293018
h2h3 - 106678423
h2h4 - 138495290
""".replace(" -", ":")

stock = """
a2a3: 106743106
b2b3: 133233975
c2c3: 144074944
d2d3: 227598692
e2e3: 306138410
f2f3: 102021008
g2g3: 135987651
h2h3: 106678423
a2a4: 137077337
b2b4: 134087476
c2c4: 157756443
d2d4: 269605599
e2e4: 309478263
f2f4: 119614841
g2g4: 130293018
h2h4: 138495290
b1a3: 120142144
b1c3: 148527161
g1f3: 147678554
g1h3: 120669525

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
