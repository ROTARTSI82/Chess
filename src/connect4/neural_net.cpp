#include <connect4/neural_net.hpp>

#include <random>


void NeuralNetwork::randomize() {
    std::random_device rd;
    std::mt19937_64 mt(rd());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (int j = 0; j < 42; j++) {
        h1.biases[j] = dist(mt);
        for (int i = 0; i < 84; i++) {
            h1.weights[i][j] = dist(mt);
        }
    }

    for (int j = 0; j < 42; j++) {
        h2.biases[j] = dist(mt);
        for (int i = 0; i < 42; i++) {
            h2.weights[i][j] = dist(mt);
        }
    }

    for (int j = 0; j < 7; j++) {
        output.biases[j] = dist(mt);
        for (int i = 0; i < 42; i++) {
            output.weights[i][j] = dist(mt);
        }
    }
}

Vector<7> NeuralNetwork::run() {
    Vector<42> h1Act = input.matMul(h1.weights) + h1.biases;
    for (int i = 0; i < 42; i++)
        h1Act[i] = squish(h1Act[i]);


    Vector<42> h2Act = h1Act.matMul(h2.weights) + h2.biases;
    for (int i = 0; i < 42; i++)
        h2Act[i] = squish(h2Act[i]);

    Vector<7> ret = h2Act.matMul(output.weights) + output.biases;
    for (int i = 0; i < 7; i++) ret[i] = squish(ret[i]);
    return ret;
}

void NeuralNetwork::backprop(const Vector<84> &inp, const Vector<7> &expectedOut) {
    Vector<42> h1Z = input.matMul(h1.weights) + h1.biases;
    Vector<42> h1Act;
    for (int i = 0; i < 42; i++) h1Act[i] = squish(h1Z[i]);


    Vector<42> h2Z = h1Act.matMul(h2.weights) + h2.biases;
    Vector<42> h2Act;
    for (int i = 0; i < 42; i++) h2Act[i] = squish(h2Z[i]);

    Vector<7> outZ = h2Act.matMul(output.weights) + output.biases;
    Vector<7> outAct;
    double cost = 0;
    for (int i = 0; i < 7; i++) {
         outAct[i] = squish(outZ[i]);
         cost += std::pow((outAct[i] - expectedOut[i]), 2);
    }

    std::cout << "COST = " << cost << std::endl;


    auto h2Nudge = output.backpropInit(expectedOut, outZ, h2Act);
    auto h1Nudge = h2.backpropCont(h2Nudge, h2Z, h1Act);
    auto inputNudge = h1.backpropCont(h1Nudge, h1Z, input);
    for (int i = 0; i < 84; i++) {
        // std::cout << "input nudge for " << i << " = " << inputNudge[i] << std::endl;
    }
}


void NeuralNetwork::applyBackprops() {
    h1.apply();
    h2.apply();
    output.apply();
}