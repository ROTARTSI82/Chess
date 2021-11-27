#pragma once

#include <vector>
#include <cstdint>
#include <random>

#define BWIDTH 7
#define BHEIGHT 6

enum class C4State : uint8_t {
    NIL, RED, YELLOW, DRAW
};

typedef uint8_t C4Move;


template <typename T>
inline T randOf(const std::vector<T> &vec) {
    static std::random_device dev;
    static std::mt19937_64 eng = std::mt19937_64(dev());

    auto dist = std::uniform_int_distribution<int>(0, vec.size() - 1);
    return vec.at(dist(eng));
}


class C4Board {
public:

    C4Board() = default;
    ~C4Board() = default;


    inline void make(C4Move mov) {
        columns[mov].push_back(turn);
        update(mov);
        // this can be branchless if we cast and add 1
        turn = turn == C4State::RED ? C4State::YELLOW : C4State::RED;
    }

    inline void unmake(C4Move mov) {
        columns[mov].pop_back();
        gameState = C4State::NIL;
        turn = turn == C4State::RED ? C4State::YELLOW : C4State::RED;
    }

    inline bool legal(C4Move mov) {
        return columns[mov].size() < BHEIGHT;
    }

    void randMove();

    int score(int alpha, int beta, int depth);

    std::vector<C4Move> searchAll(int depth);
    C4Move search(int depth);

    // 7 columns, 6 rows
    std::vector<C4State> columns[BWIDTH];
    C4State turn = C4State::RED;
    C4State gameState = C4State::NIL;

private:
    int countDiag(int8_t col, int8_t row, int8_t dcol, int8_t drow);

    void update(C4Move changed);
};


