#include "scacus/engine.hpp"

namespace sc {
    static inline int eval(Position &pos, const MoveList &legalMoves) {
        if (legalMoves.empty()) return pos.isInCheck ? std::numeric_limits<int>::min() + 2 : 0;

        auto count = [&](const Type t) {
            return popcnt(pos.by_side(pos.turn) & pos.by_type(t)) - popcnt(pos.by_side(opposite_side(pos.turn)) & pos.by_type(t));
        };

        auto opposingMoves = pos.turn == WHITE_SIDE ? standard_moves<BLACK_SIDE>(pos) : standard_moves<WHITE_SIDE>(pos);

        return (legalMoves.size() - opposingMoves.size()) + 100 * count(PAWN) + 300 * count(BISHOP) + 300 * count(KNIGHT) + 500 * count(ROOK) + 900 * count(QUEEN);
    }

    static inline int quiescence_search(Position &pos, int alpha, int beta) {
        auto legalMoves = pos.turn == WHITE_SIDE ? standard_moves<WHITE_SIDE>(pos) : standard_moves<BLACK_SIDE>(pos);
        int stand_pat = eval(pos, legalMoves);
        if (stand_pat >= beta)
            return beta;
        if (alpha < stand_pat)
            alpha = stand_pat;

        for (const auto mov : legalMoves) {
            // mov.typeFlags == EN_PASSANT ||
            // TODO: not searching en passants right now bc they are buggy
            if ((pos.by_side(opposite_side(pos.turn)) & to_bitboard(mov.dst)) && mov.typeFlags != EN_PASSANT) {
                auto undo = sc::make_move(pos, mov);
                int score = -quiescence_search(pos, -beta, -alpha);
                sc::unmake_move(pos, undo, mov);

                if (score >= beta)
                    return beta;
                alpha = std::max(alpha, score);
            }
        }

        return alpha;
    }

    static inline int primitive_eval(Position &pos, int alpha, int beta, int depth) {

        if (depth <= 0) {
            return quiescence_search(pos, alpha, beta);
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
    }

}