#pragma once

#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstring>

// 0.1 before
#define GRADIENT_STEP_SCALE_FACTOR 0.3

inline double squish(double in) {
    return 1 / (1 + std::exp(-in));
    // return std::max(in, 0.0);
}

inline double squishPrime(double in) {
    return std::exp(-in) / std::pow(1 + std::exp(-in), 2);
    // return in < 0 ? 0 : 1;
}

template <int, int>
class Matrix;

template <int D>
class Vector {
public:
    double data[D];

    inline Vector<D> operator+(const Vector<D> &rhs) const {
        Vector<D> ret;
        for (int i = 0; i < D; i++)
            ret.data[i] = data[i] + rhs.data[i];
        return ret;
    }

    inline Vector<D> &operator+=(const Vector<D> &rhs) {
        for (int i = 0; i < D; i++)
            data[i] += rhs.data[i];
        return *this;
    }

    template <int OutD>
    inline Vector<OutD> matMul(Matrix<D, OutD> &rhs) const;

    inline Vector<D> operator*(double rhs) {
        Vector<D> ret;
        for (int i = 0; i < D; i++)
            ret.data[i] = data[i] * rhs;
        return ret;
    }

    inline double operator[](const int ind) const { return data[ind]; }
    inline double &operator[](const int ind) { return data[ind]; }
};

template <int W, int H>
class Matrix {
public:
    Vector<H> data[W];

    inline Vector<H> &operator[](const int ind) { return data[ind]; }
};

template <int D>
template <int OutD>
Vector<OutD> Vector<D>::matMul(Matrix<D, OutD> &rhs) const {
    Vector<OutD> ret;
    memset(ret.data, 0, OutD * sizeof(double));

    for (int i = 0; i < D; i++) {
        ret += rhs[i] * data[i];
    }
    return ret;
}


template <int PrevNo, int No>
class Layer {
public:
    Matrix<PrevNo, No> weights;
    Vector<No> biases;

    Matrix<PrevNo, No> dWeightAcc;
    Vector<No> dBiasAcc;
    double numBackProps = 0;

    Layer() {
        for (int j = 0; j < No; j++) {
            dBiasAcc[j] = 0;
            for (int i = 0; i < PrevNo; i++)
                dWeightAcc[i][j] = 0;
        }
    }

    void apply() {
        for (int j = 0; j < No; j++) {
            double nudge = GRADIENT_STEP_SCALE_FACTOR * dBiasAcc[j] / numBackProps;
            dBiasAcc[j] = 0;
            biases[j] += nudge;
            // std::cout << "Bias " << j << " nudged by " << nudge << std::endl;
            for (int i = 0; i < PrevNo; i++) {
                nudge = GRADIENT_STEP_SCALE_FACTOR * dWeightAcc[i][j] / numBackProps;
                dWeightAcc[i][j] = 0;
                weights[i][j] += nudge;
                // std::cout << "Weight " << i << j << " nudged by " << nudge << std::endl;
            }
        }

        numBackProps = 0;
    }

    // zvec is the output of this layer before being passed through the squishification function
    Vector<PrevNo> backpropCont(const Vector<No> &dCdA, const Vector<No> &zvec, const Vector<PrevNo> &prevActs) {
        Vector<PrevNo> dCdPrevA;
        memset(dCdPrevA.data, 0, PrevNo * sizeof(double));

        for (int i = 0; i < No; i++) {
            double dCdAi = dCdA[i];
            double dAidZi = squishPrime(zvec[i]);

            double dCdBias = dCdAi * dAidZi;
            // if (std::abs(dCdBias) > std::numeric_limits<double>::epsilon())
            //     std::cout << "Bias " << i << " nudged " << dCdBias << std::endl;
            dBiasAcc[i] += dCdBias;

            for (int y = 0; y < PrevNo; y++) {
                dCdPrevA[y] += weights[y][i] * dCdBias;
                double dCdWyi = dCdBias * prevActs[y];
                // if (std::abs(dCdWyi) > std::numeric_limits<double>::epsilon())
                //     std::cout << "Weight " << y << i << " nudged " << dCdWyi << std::endl;
                dWeightAcc[y][i] += dCdWyi;
            }
        }
        numBackProps++;

        return dCdPrevA;
    }

    Vector<PrevNo> backpropInit(const Vector<No> &desired, const Vector<No> &zvec, const Vector<PrevNo> &prevActs) {
        Vector<No> dCdA;
        for (int i = 0; i < No; i++)
            dCdA[i] = 2 * (desired[i] - squish(zvec[i]));
        return backpropCont(dCdA, zvec, prevActs);
    }
};

class NeuralNetwork {
public:
    Vector<84> input;
    Layer<84, 84> h1;
    Layer<84, 42> h2;
    Layer<42, 7> h3;
    Layer<7, 7> output;

    void randomize();

    void backprop(const Vector<7> &expectedOut);

    Vector<7> run();

    void toFile(const std::string &file);
    void fromFile(const std::string &file);

    void applyBackprops();
};
