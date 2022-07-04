mine = """
b1d2: 40560
b1a3: 44378
b1c3: 50303
c1d2: 46881
c1e3: 53637
c1f4: 52350
c1g5: 45601
c1h6: 40913
d1d2: 48843
d1d3: 44393
d1d4: 41628
d1d5: 42818
d1d6: 35444
e1f1: 44331
e1d2: 30494
e1f2: 31727
e1g1: 41765
h1f1: 42128
h1g1: 40695
a2a3: 42860
a2a4: 44909
b2b3: 42523
b2b4: 42756
c2c3: 41766
e2g1: 40761
e2c3: 46098
e2g3: 43823
e2d4: 46955
e2f4: 43093
g2g3: 40542
g2g4: 41537
h2h3: 42789
h2h4: 43838
c4b3: 39484
c4d3: 43537
c4b5: 41581
c4d5: 45733
c4a6: 38146
c4e6: 45681
c4f7: 39801
d7c8q: 40306
d7c8b: 59254
d7c8n: 56256
d7c8r: 34078
""".replace(" -", ":")

stock = """
a2a3: 46833
b2b3: 46497
c2c3: 49406
g2g3: 44509
h2h3: 46762
a2a4: 48882
b2b4: 46696
g2g4: 45506
h2h4: 47811
d7c8q: 44226
d7c8r: 38077
d7c8b: 65053
d7c8n: 62009
b1d2: 40560
b1a3: 44378
b1c3: 50303
e2g1: 48844
e2c3: 54792
e2g3: 51892
e2d4: 52109
e2f4: 51127
c1d2: 46881
c1e3: 53637
c1f4: 52350
c1g5: 45601
c1h6: 40913
c4b3: 43453
c4d3: 43565
c4b5: 45559
c4d5: 48002
c4a6: 41884
c4e6: 49872
c4f7: 43289
h1f1: 46101
h1g1: 44668
d1d2: 48843
d1d3: 57153
d1d4: 57744
d1d5: 56899
d1d6: 43766
e1f1: 49775
e1d2: 33423
e1f2: 36783
e1g1: 47054
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
