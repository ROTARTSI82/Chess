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
        uint64_t hash = 0;
        ScoreT score = 0;
        DepthT depth = 0;
//        sc::Move bestMove{};
        bool isQuiesc = true; // if false, it's a quiesc search
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

    inline static unsigned tt_strength(DepthT depth, bool quiesc) {
        return (quiesc ? 16 : 256) + depth;
    }

    class SearchThread {
    private:
        Position *pos;
        DepthT startDepth;
        uint64_t ttHits = 0;

    public:

        inline uint64_t getTTHits() const {
            return ttHits;
        }

        SearchThread(Position *p, DepthT d) : pos(p), startDepth(d) {}

        inline ScoreT mateScore(DepthT depth) {
            return pos->in_check() ? MATE_SCORE + (startDepth - depth) * MATE_STEP : 0;
        }

        inline ScoreT eval(DepthT depth) {
            // need to double check if it's mate! TODO: we don't need to full list of legal
            // moves, so you can implement a more efficient way to check this
            MoveList ls = legal_moves_from<false>(*pos);
            if (ls.empty())
                return mateScore(depth);

            MoveList opp(0);
            if (pos->get_turn() == WHITE_SIDE)
                standard_moves<BLACK_SIDE, false>(opp, *pos);
            else
                standard_moves<WHITE_SIDE, false>(opp, *pos);

            return eval_material(*pos) + (ls.size() - opp.size());
        }

        template <bool QUIESC>
        inline ScoreT search(ScoreT alpha, ScoreT beta, DepthT depth) {
            auto &tt = transTable[pos->get_state()->hash % TABLE_SIZE];
            // either the tt is in a higher mode OR (higher depth and same mode)
            if (tt.hash == pos->get_state()->hash && tt_strength(depth, QUIESC) <= tt_strength(tt.depth, tt.isQuiesc)) {
                ttHits++;
                return tt.score;
            }
            
            MoveList ls = legal_moves_from<QUIESC>(*pos);

            const bool canForceDraw = pos->get_state()->halfmoves >= 50 || pos->get_state()->reps;

            if (QUIESC) {
                ScoreT ev = canForceDraw ? 0 : eval(depth);
                if (ev > beta || ls.empty() || depth <= 0) // depth limit is bad but whatever
                    return ev;
                alpha = std::max(alpha, ev);
            } else {
                if (depth <= 0)
                    return search<true>(alpha, beta, 20);
                
                if (ls.empty())
                    return mateScore(depth);

                if (canForceDraw) return 0;
            }

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
            if (tt_strength(depth, QUIESC) >= tt_strength(tt.depth, tt.isQuiesc)) {
                tt.hash = 0;
                tt.depth = depth;
                tt.isQuiesc = QUIESC;
                tt.score = value;
                tt.hash = pos->get_state()->hash;
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
        std::cout << "info string ttHits = " << me.getTTHits() << '\n';
    }

    void EngineV2::stop_search() {

    }
}