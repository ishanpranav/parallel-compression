# generate_random_test.py
# Copyright (c) 2024 Ishan Pranav
# Licensed under the MIT license.

from random import randrange

count = randrange(1, 100)

for i in range(count):
    with open(f"{i}.txt", "w") as output:
        length = randrange(1, 100000)
        for j in range(length):
            symbol = randrange(0, 256)
            output.write(chr(symbol))

print("./nyuenc -j 3", end="")

for i in range(count):
    print(f"../tools/{i}.txt ", end="")

print()
