mine = """
h4g3: 1
h4h3: 1
h4g4: 1
h4g5: 1
h5b5: 1
h5c5: 1
h5d5: 1
h5e5: 1
h5f5: 1
h5g5: 1
h5h6: 1
h5h7: 1
h5h8: 1
d6d5: 1
c7c5: 1
c7c6: 1
""".replace(" -", ":")

stock = """
f4f3: 1
d6d5: 1
c7c6: 1
c7c5: 1
h4g5: 1
h5b5: 1
h5c5: 1
h5d5: 1
h5e5: 1
h5f5: 1
h5g5: 1
h5h6: 1
h5h7: 1
h5h8: 1
h4g3: 1
h4h3: 1
h4g4: 1
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
