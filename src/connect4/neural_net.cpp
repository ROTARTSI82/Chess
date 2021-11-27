#include <connect4/neural_net.hpp>

#include <random>


void NeuralNetwork::randomize() {
    std::random_device rd;
    std::mt19937_64 mt(rd());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (int j = 0; j < 84; j++) {
        h1.biases[j] = dist(mt);
        for (int i = 0; i < 84; i++) {
            h1.weights[i][j] = dist(mt);
        }
    }

    for (int j = 0; j < 42; j++) {
        h2.biases[j] = dist(mt);
        for (int i = 0; i < 84; i++) {
            h2.weights[i][j] = dist(mt);
        }
    }

    for (int j = 0; j < 7; j++) {
        h3.biases[j] = dist(mt);
        for (int i = 0; i < 42; i++) {
            h3.weights[i][j] = dist(mt);
        }
    }

    for (int j = 0; j < 7; j++) {
        output.biases[j] = dist(mt);
        for (int i = 0; i < 7; i++) {
            output.weights[i][j] = dist(mt);
        }
    }
}

Vector<7> NeuralNetwork::run() {
    Vector<84> h1Act = input.matMul(h1.weights) + h1.biases;
    for (int i = 0; i < 84; i++)
        h1Act[i] = squish(h1Act[i]);


    Vector<42> h2Act = h1Act.matMul(h2.weights) + h2.biases;
    for (int i = 0; i < 42; i++)
        h2Act[i] = squish(h2Act[i]);

    Vector<7> h3Act = h2Act.matMul(h3.weights) + h3.biases;
    for (int i = 0; i < 7; i++) h3Act[i] = squish(h3Act[i]);

    Vector<7> ret = h3Act.matMul(output.weights) + output.biases;
    for (int i = 0; i < 7; i++) ret[i] = squish(ret[i]);
    return ret;
}

void NeuralNetwork::backprop(const Vector<7> &expectedOut) {
    Vector<84> h1Z = input.matMul(h1.weights) + h1.biases;
    Vector<84> h1Act;
    for (int i = 0; i < 84; i++) h1Act[i] = squish(h1Z[i]);


    Vector<42> h2Z = h1Act.matMul(h2.weights) + h2.biases;
    Vector<42> h2Act;
    for (int i = 0; i < 42; i++) h2Act[i] = squish(h2Z[i]);

    Vector<7> h3Z = h2Act.matMul(h3.weights) + h3.biases;
    Vector<7> h3Act;
    for (int i = 0; i < 7; i++) h3Act[i] = squish(h3Act[i]);

    Vector<7> outZ = h3Act.matMul(output.weights) + output.biases;
    Vector<7> outAct;
    double cost = 0;
    for (int i = 0; i < 7; i++) {
         outAct[i] = squish(outZ[i]);
         cost += std::pow((outAct[i] - expectedOut[i]), 2);
    }

    std::cout << "COST = " << cost << std::endl;


    auto h3Nudge = output.backpropInit(expectedOut, outZ, h3Act);
    auto h2Nudge = h3.backpropCont(h3Nudge, h3Z, h2Act);
    auto h1Nudge = h2.backpropCont(h2Nudge, h2Z, h1Act);
    auto inputNudge = h1.backpropCont(h1Nudge, h1Z, input);
}


void NeuralNetwork::applyBackprops() {
    if (h1.numBackProps == 0) {
        std::cout << "No backprops to apply!" << std::endl; 
        return; 
    }
    h1.apply();
    h2.apply();
    h3.apply();
    output.apply();
    std::cout << "Batch complete" << std::endl;
}


#include <fstream>

void NeuralNetwork::fromFile(const std::string &file) {
    std::ifstream in(file);
    std::string read;
    in >> read;
    std::cout << "READ " << read << std::endl;
    for (int i = 0; i < 84; i++) {
        in >> h1.biases[i];
        in >> read; // consume stuff in between numbers (commas & spaces, sometimes JSON stuff)
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 84; i++) {
        for (int j = 0; j < 84; j++) {
            in >> h1.weights[i][j];
            in >> read;
        }
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 42; i++) {
        in >> h2.biases[i];
        in >> read;
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 42; i++) {
        for (int j = 0; j < 84; j++) {
            in >> h2.weights[i][j];
            in >> read;
        }
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 7; i++) {
        in >> h3.biases[i];
        in >> read;
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 42; j++) {
            in >> h3.weights[i][j];
            in >> read;
        }
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 7; i++) {
        in >> output.biases[i];
        in >> read;
    }

    std::cout << "READ " << read << std::endl;

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            in >> output.weights[i][j];
            in >> read;
        }
    }

    std::cout << "READ " << read << std::endl;
}

void NeuralNetwork::toFile(const std::string &file) {
    std::ofstream out(file);
    out << "{\"hidden1\":{\"biases\":[\n";
    for (int i = 0; i < 84; i++) {
        if (i) out << ", ";
        out << h1.biases[i];
    }

    out << "\n],\"weights\":[\n";
    for (int i = 0; i < 84; i++) {
        for (int j = 0; j < 84; j++) {
            if (i || j) out << ", ";
            out << h1.weights[i][j];
        }
    }
    out << "\n]},\"hidden2\":{\"biases\":[\n";
    for (int i = 0; i < 42; i++) {
        if (i) out << ", ";
        out << h2.biases[i];
    }

    out << "\n],\"weights\":[\n";
    for (int i = 0; i < 42; i++) {
        for (int j = 0; j < 84; j++) {
            if (i || j) out << ", ";
            out << h2.weights[i][j];
        }
    }



    out << "\n]},\"hidden3\":{\"biases\":[\n";
    for (int i = 0; i < 7; i++) {
        if (i) out << ", ";
        out << h3.biases[i];
    }

    out << "\n],\"weights\":[\n";
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 42; j++) {
            if (i || j) out << ", ";
            out << h3.weights[i][j];
        }
    }


    out << "\n]},\"output\":{\"biases\":[\n";
    for (int i = 0; i < 7; i++) {
        if (i) out << ", ";
        out << output.biases[i];
    }

    out << "\n],\"weights\":[\n";
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (i || j) out << ", ";
            out << output.weights[i][j];
        }
    }

    out << "\n]}}";
}

