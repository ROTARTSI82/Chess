mine = """
f8h6: 1
f8g7: 1
b8a6: 1
b8c6: 1
g8f6: 1
g8h6: 1
g4g3: 1
a7a5: 1
a7a6: 1
b7b5: 1
b7b6: 1
c7c5: 1
c7c6: 1
d7d5: 1
d7d6: 1
e7e5: 1
e7e6: 1
f7f5: 1
f7f6: 1
h7h5: 1
h7h6: 1

""".replace(" -", ":")

stock = """
g4g3: 1
a7a6: 1
b7b6: 1
c7c6: 1
d7d6: 1
e7e6: 1
f7f6: 1
h7h6: 1
a7a5: 1
b7b5: 1
c7c5: 1
d7d5: 1
e7e5: 1
f7f5: 1
h7h5: 1
g4f3: 1
b8a6: 1
b8c6: 1
g8f6: 1
g8h6: 1
f8h6: 1
f8g7: 1
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
