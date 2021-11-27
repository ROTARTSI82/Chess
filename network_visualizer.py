import json
import numpy as np
from PIL import Image


def convert(object, shape, filename, network):
    weights = np.asarray(network[object]["weights"]).reshape(shape).transpose()
    weights = 128 + 1024 * weights / np.linalg.norm(weights)

    print (weights)

    Image.fromarray(weights).convert("L").save(filename, format="png")


for i in ['nn-0.2.json', 'nn-0.16.json', 'nn-0.26-final-noMiniMax.json', "randomNet.json"]:
    with open(i, "r") as fp:
        network = json.load(fp)

    convert("hidden3", (42, 7),  "./visuals/" + i + "_h3weights.png", network)
    convert("hidden1", (84, 84), "./visuals/" + i + "_h1weights.png", network)
    convert("hidden2", (84, 42), "./visuals/" + i + "_h2weights.png", network)
    convert("output", (7, 7), "./visuals/" + i + "_outweights.png", network)
