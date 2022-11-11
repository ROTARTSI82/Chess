#include "scacus/engine.hpp"
#include <algorithm>

#include "scacus/bitboard.hpp"

using ScoreT = int;
using DepthT = uint16_t;

namespace {
    struct Transposition {
        ScoreT score;
        DepthT depth;
        sc::Move bestMove;
    };

    Transposition transTable[4096 * 4096];
}

namespace sc {
    constexpr ScoreT PAWN_SCORE = 512;

    // We can't actually use min because -min is not max! In fact, -min is negative! 
    constexpr auto MATE_SCORE = -10000 * PAWN_SCORE;
    constexpr auto ACTUAL_MIN = std::numeric_limits<ScoreT>::min() + 2;
    constexpr auto ACTUAL_MAX = std::numeric_limits<ScoreT>::max() - 2;


    template <bool QUIESC>
    static ScoreT search(Position &pos, ScoreT alpha, ScoreT beta, DepthT depth) {
        MoveList ls = legal_moves_from(pos);

        for (const auto &mov: ls) {
            auto undo = make_move(pos, mov);



            unmake_move(pos, undo, mov);
        }
    }

    void EngineV2::start_search(int maxDepth) {

    }

    void EngineV2::stop_search() {

    }
}