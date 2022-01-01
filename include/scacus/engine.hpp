#pragma once

#include "scacus/movegen.hpp"

#include <thread>
#include <atomic>
#include <mutex>

#include <cstring> // memset
#include <unordered_map>

namespace sc {

    struct Transposition {
        int depth;
        int score;
        // int alpha, beta;
        Move bestMove;
    };

    class DefaultEngine {
    public:

        std::thread *workers;
        int numWorkers;

        std::mutex ttMtx;
        std::unordered_map<uint64_t, Transposition> tt; // transposition table
        int ttHits = 0;

        std::mutex mtx;
        std::atomic_int runningAlpha;

        Position *pos;
        int history[NUM_SIDES][BOARD_SIZE][BOARD_SIZE]; // [side_to_move][square_from][square_to]
        Move bestMove;
        int evaluation;
        bool continueSearch = true;

        DefaultEngine() {
            memset(history, 0, sizeof(history));
        }

        void start_search(int maxDepth = 99);
        void stop_search();
        void order_moves(MoveList &list, Move best = Move{0, 0});
    };
}