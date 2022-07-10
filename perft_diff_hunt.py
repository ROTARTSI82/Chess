mine = """
e7a3: 46
e7b4: 8
e7h4: 45
e7c5: 46
e7g5: 46
e7d6: 46
e7f6: 47
d8a5: 9
d8b6: 47
d8c7: 48
d8d1: 2
d8d2: 6
d8d3: 43
d8d4: 42
d8d5: 42
d8d6: 45
d8d7: 42
d8c8: 42
d8e8: 48
h8g8: 47
f2d1: 39
f2h1: 43
f2d3: 5
f2h3: 44
f2e4: 46
f2g4: 44
b8a6: 47
b8d7: 43
f8e8: 47
f8g8: 47
c6c5: 47
a7a5: 47
a7a6: 47
b7b5: 47
b7b6: 48
f7f5: 46
f7f6: 48
g7g5: 46
g7g6: 47
h7h5: 47
h7h6: 47
""".replace(" -", ":")

stock = """
c6c5: 47
a7a6: 47
b7b6: 48
f7f6: 48
g7g6: 47
h7h6: 47
a7a5: 47
b7b5: 47
f7f5: 46
g7g5: 46
h7h5: 47
f2d1: 39
f2h1: 43
f2d3: 5
f2h3: 44
f2e4: 46
f2g4: 44
b8a6: 47
b8d7: 43
e7a3: 46
e7b4: 8
e7h4: 45
e7c5: 46
e7g5: 46
e7d6: 46
e7f6: 47
h8g8: 47
d8d1: 1
d8d2: 6
d8d3: 43
d8d4: 42
d8a5: 9
d8d5: 42
d8b6: 47
d8d6: 45
d8c7: 48
d8d7: 42
d8c8: 42
d8e8: 48
f8e8: 47
f8g8: 47

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
