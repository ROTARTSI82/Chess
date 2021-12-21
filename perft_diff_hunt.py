mine = """
f2d1 - 1
f2h1 - 1
f2d3 - 1
f2h3 - 1
f2e4 - 1
f2g4 - 1
c6c5 - 1
a7a5 - 1
a7a6 - 1
b7b5 - 1
b7b6 - 1
e7a3 - 1
e7b4 - 1
e7h4 - 1
e7c5 - 1
e7g5 - 1
e7d6 - 1
e7f6 - 1
f7f5 - 1
f7f6 - 1
g7g5 - 1
g7g6 - 1
h7h5 - 1
h7h6 - 1
b8a6 - 1
b8d7 - 1
d8c8 - 1
f8e8 - 1
f8g8 - 1
h8g8 - 1
""".replace(" -", ":")

stock = """
c6c5: 1
a7a6: 1
b7b6: 1
f7f6: 1
g7g6: 1
h7h6: 1
a7a5: 1
b7b5: 1
f7f5: 1
g7g5: 1
h7h5: 1
f2d1: 1
f2h1: 1
f2d3: 1
f2h3: 1
f2e4: 1
f2g4: 1
b8a6: 1
b8d7: 1
e7a3: 1
e7b4: 1
e7h4: 1
e7c5: 1
e7g5: 1
e7d6: 1
e7f6: 1
h8g8: 1
f8g8: 1
f8e8: 1
d8e8: 1
d8c8: 1


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
