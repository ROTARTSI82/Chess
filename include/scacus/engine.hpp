#pragma once

#include "scacus/movegen.hpp"

#include <thread>
#include <atomic>
#include <mutex>

namespace sc {

    static inline int primitive_eval(Position &pos, int alpha, int beta, int depth) {
        if (depth <= 0) {
            auto count = [&](const Type t) {
                return popcnt(pos.by_side(pos.turn) & pos.by_type(t)) - popcnt(pos.by_side(opposite_side(pos.turn)) & pos.by_type(t));
            };

            return 100 * count(PAWN) + 300 * count(BISHOP) + 300 * count(KNIGHT) + 500 * count(ROOK) + 900 * count(QUEEN);
        }

        auto legalMoves = pos.turn == WHITE_SIDE ? standard_moves<WHITE_SIDE>(pos) : standard_moves<BLACK_SIDE>(pos);

        if (legalMoves.empty())
            return std::numeric_limits<int>::min() + 2; // TODO: differentiate stalemate & checkmate

        int value = std::numeric_limits<int>::min() + 2;
        for (const auto mov : legalMoves) {
            auto undoInfo = sc::make_move(pos, mov);
            auto result = primitive_eval(pos, -beta, -alpha, depth - 1);
            sc::unmake_move(pos, undoInfo, mov);

            value = std::max(value, -result);
            if (value > beta)
                return value;
            
            alpha = std::max(value, alpha);
        }
        
        return value;
    }

    #define MULTITHREAD 1

    std::pair<Move, int> primitive_search(Position &pos, int depth) {
        auto legalMoves = pos.turn == WHITE_SIDE ? standard_moves<WHITE_SIDE>(pos) : standard_moves<BLACK_SIDE>(pos);

        #if MULTITHREAD
            std::thread *workers = new std::thread[legalMoves.size()];
        #endif
        std::mutex mtx;
        std::atomic_int value = std::numeric_limits<int>::min() + 2;
        std::atomic_int alpha = std::numeric_limits<int>::min() + 2;

        auto currentMov = make_normal(0, 0);
        for (int i = 0; i < legalMoves.size(); i++) {
            auto do_work = [&, mov{legalMoves.at(i)}, cpy{pos}]() mutable {
                auto undoInfo = sc::make_move(cpy, mov);
                int result = -primitive_eval(cpy, std::numeric_limits<int>::min() + 2, -alpha, depth - 1);
                sc::unmake_move(cpy, undoInfo, mov);

                std::lock_guard lg(mtx);
                value = std::max(value.load(), result);
                if (value > alpha) {
                    currentMov = mov;
                    alpha.store(value);
                }
            };

        #if MULTITHREAD
            workers[i] = std::thread(do_work);
        #else
            do_work();
        #endif
        }

    #if MULTITHREAD
        for (int i = 0; i < legalMoves.size(); i++)
            workers[i].join();
        delete[] workers;
    #endif
        
        return std::make_pair(currentMov, value.load());
    };
}