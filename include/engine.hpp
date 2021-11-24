#pragma once

#include "chess.hpp"
#include <unordered_map>

double defaultEvaluator(Board &b, bool side);

class MyEngine : public Engine {
public:
    ~MyEngine() = default;
    Move search(Board &b, bool sideIsWhite) override;

private:
    Board *b;
    bool isWhite;
    int depth;

    // recursive, with parameters as part of the class to prevent
    // the call stack from growing excessively.
    double scoreOf(double alpha, double beta);

    std::unordered_map<uint64_t, double> transpositionTable;
    // std::unordered_map<uint64_t, double> whiteTable;
};
