#pragma once

#include "chess.hpp"
#include <unordered_map>
#include <mutex>
#include <functional>

double defaultEvaluator(Board &b, bool side);
double swarmEvaluator(Board &b, bool side);
double rushEvaluator(Board &b, bool side);
double huddleEvaluator(Board &b, bool side);
double enPassantEvaluator(Board &b, bool side);
double pawnGenocideEvaluator(Board &b, bool side);

class Searcher {
public:
    

public:
    Board *b;
    bool isWhite;
    int depth;

    std::function<double(Board &, bool)> evaluator;

    // recursive, with parameters as part of the class to prevent
    // the call stack from growing excessively.
    double scoreOf(double alpha, double beta);

    std::unordered_map<uint64_t, double> *transpositionTable;
    std::mutex *tableMtx;
};

class MyEngine : public Engine {
public:
    MyEngine(const std::function<double(Board &, bool)> &eval) : evaluator(eval) {}
    ~MyEngine() = default;
    Move search(Board &b, bool sideIsWhite) override;

private:
    std::function<double(Board &, bool)> evaluator;

    // std::unordered_map<uint64_t, double> whiteTable;
};
