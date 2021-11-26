import json
import numpy as np
from PIL import Image

with open("sampleNN.json", "r") as fp:
    network = json.load(fp)


def convert(object, shape, filename):
    weights = np.asarray(network[object]["weights"]).reshape(shape).transpose()
    weights = 128 + 1024 * weights / np.linalg.norm(weights)

    print (weights)

    Image.fromarray(weights).convert("L").save(filename, format="png")

convert("hidden2", (42, 42), "h2weights.png")
convert("hidden1", (84, 42), "h1weights.png")
convert("output", (42, 7), "outweights.png")
