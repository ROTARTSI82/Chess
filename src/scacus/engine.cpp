#include "scacus/engine.hpp"
#include <algorithm>

#include "scacus/bitboard.hpp"

using ScoreT = int;
using DepthT = uint16_t;

namespace {
    enum class TransposeType: uint_fast8_t {
        Full, BetaCut
    };

    struct Transposition {
        ScoreT score = 0;
        DepthT depth = 0;
//        sc::Move bestMove{};
        bool isFull; // if false, it's a quiesc search
    };

    constexpr auto TABLE_SIZE = 4096 * 4096;
    Transposition transTable[TABLE_SIZE] = {{}};
}

namespace sc {
    constexpr ScoreT PAWN_SCORE = 512;

    // We can't actually use min because -min is not max! In fact, -min is negative! 
    constexpr auto MATE_SCORE = -10000 * PAWN_SCORE;
    constexpr auto MATE_STEP = 200 * PAWN_SCORE;
    constexpr auto MIN_SCORE = std::numeric_limits<ScoreT>::min() + 2;
    constexpr auto MAX_SCORE = std::numeric_limits<ScoreT>::max() - 2;


    inline static ScoreT eval_material(Position &pos) {
        const Side turn = pos.get_turn();

        const Bitboard bishops = pos.by_type(BISHOP);
        const Bitboard pawns = pos.by_type(PAWN);
        const Bitboard knights = pos.by_type(KNIGHT);
        const Bitboard rooks = pos.by_type(ROOK);
        const Bitboard queens = pos.by_type(QUEEN);

        const Bitboard me = pos.by_side(turn);
        const Bitboard them = pos.by_side(opposite_side(turn));
        return (popcnt(me & bishops) - popcnt(them & bishops)) * 330
                + (popcnt(me & knights) - popcnt(them & knights)) * 320
                + (popcnt(me & rooks) - popcnt(them & rooks)) * 500
                + (popcnt(me & queens) - popcnt(them & queens)) * 900
                + (popcnt(me & pawns) - popcnt(them & pawns)) * 100;
    }

    inline static ScoreT eval(Position &pos) {
        return eval_material(pos);
    }

    class SearchThread {
    private:
        Position *pos;
        DepthT startDepth;

    public:

        SearchThread(Position *p, DepthT d) : pos(p), startDepth(d) {}

        template <bool QUIESC>
        inline ScoreT search(ScoreT alpha, ScoreT beta, DepthT depth) {
            MoveList ls = legal_moves_from<QUIESC>(*pos);

            if (depth <= 0)
                // return eval_material(*pos);
                return QUIESC ? eval_material(*pos) : search<true>(alpha, beta, 20);

            if (ls.empty())
                return pos->in_check() ? MATE_SCORE + (startDepth - depth) * MATE_STEP : 0;
            if (pos->get_state()->halfmoves >= 50 || pos->get_state()->reps) return 0;

            auto &tt = transTable[pos->get_state()->hash % TABLE_SIZE];
            // either the tt is in a higher mode OR (higher depth and same mode)
            if ((QUIESC && tt.isFull) || (tt.depth > depth && (QUIESC ^ tt.isFull)))
                return tt.score;

            ScoreT value = MIN_SCORE;
            for (const auto &mov: ls) {
                make_move(*pos, mov);

                value = std::max(-search<QUIESC>(-beta, -alpha, depth - 1), value);
                alpha = std::max(value, alpha);

                unmake_move(*pos, mov);

                if (value >= beta) {
                    return value;
                }
            }

            // either we are in a higher mode OR we have higher depth in the same mode
            if ((!QUIESC && !tt.isFull) || (depth > tt.depth && (QUIESC ^ tt.isFull))) {
                tt.depth = depth;
                tt.isFull = !QUIESC;
                tt.score = value;
            }

            return value;
        }
    };


    void EngineV2::start_search(int maxDepth) {
        // return;
        maxDepth = 6;
        SearchThread me{pos, static_cast<DepthT>(maxDepth)};
        MoveList ls = legal_moves_from<false>(*pos);

        ScoreT best = MIN_SCORE;
        for (const auto &mov : ls) {
            make_move(*pos, mov);
            const auto score = -me.search<false>(MIN_SCORE, MAX_SCORE, maxDepth - 1);

            std::cout << "info string mov " << mov.long_alg_notation() << " score " << (double) score / PAWN_SCORE << '\n';
            
            if (score > best) {
                best = score;
                best_mov = mov;
            }

            unmake_move(*pos, mov);
        }

        std::cout << "info string eval " << (double) best / PAWN_SCORE << '\n';
    }

    void EngineV2::stop_search() {

    }
}