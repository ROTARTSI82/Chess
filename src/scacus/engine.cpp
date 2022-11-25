#include "scacus/engine.hpp"
#include <algorithm>
#include <thread>
#include <vector>

#include "scacus/bitboard.hpp"

namespace {
    enum class TransposeType: uint_fast8_t {
        Full, BetaCut
    };

    struct Transposition {
        uint64_t hash = 0;
        sc::ScoreT score = 0;
        sc::DepthT depth = 0;
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

    constexpr auto MOBILITY_VALUE = PAWN_SCORE / 512;


    inline static ScoreT eval_material(Position &pos) {
        const Side turn = pos.get_turn();

        const Bitboard bishops = pos.by_type(BISHOP);
        const Bitboard pawns = pos.by_type(PAWN);
        const Bitboard knights = pos.by_type(KNIGHT);
        const Bitboard rooks = pos.by_type(ROOK);
        const Bitboard queens = pos.by_type(QUEEN);

        const Bitboard me = pos.by_side(turn);
        const Bitboard them = pos.by_side(opposite_side(turn));
        return (popcnt(me & bishops) - popcnt(them & bishops)) * 33 * PAWN_SCORE / 10
                + (popcnt(me & knights) - popcnt(them & knights)) * 32 * PAWN_SCORE / 10
                + (popcnt(me & rooks) - popcnt(them & rooks)) * 5 * PAWN_SCORE
                + (popcnt(me & queens) - popcnt(them & queens)) * 9 * PAWN_SCORE
                + (popcnt(me & pawns) - popcnt(them & pawns)) * PAWN_SCORE;
    }

    inline static uint64_t tt_strength(DepthT depth, bool quiesc) {
        return (quiesc ? 0ULL : 4096ULL) + depth;
    }

    class SearchThread {
    private:
        Position *pos;
        DepthT startDepth;
        uint64_t ttHits = 0;
        EngineV2 *eng;

    public:

        inline uint64_t getTTHits() const {
            return ttHits;
        }

        SearchThread(Position *p, DepthT d, EngineV2 *e) : pos(p), startDepth(d), eng(e) {}

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

            return eval_material(*pos) + (ls.size() - opp.size()) * MOBILITY_VALUE;
        }

        template <bool QUIESC>
        ScoreT search(ScoreT alpha, ScoreT beta, DepthT depth) {
            auto &tt = transTable[pos->get_state()->hash % TABLE_SIZE];
            // // either the tt is in a higher mode OR (higher depth and same mode)
            if (tt.hash == pos->get_state()->hash && tt_strength(depth, QUIESC) <= tt_strength(tt.depth, tt.isQuiesc)) {
                ttHits++;
                return tt.score;
            }
            
            MoveList ls = legal_moves_from<QUIESC>(*pos);

            const bool canForceDraw = pos->get_state()->halfmoves >= 50 || pos->get_state()->reps;

            if (QUIESC) {
                ScoreT ev = canForceDraw ? 0 : eval(depth);
                if (ev > beta || ls.empty() || depth <= 0 || !eng->is_running()) // depth limit is bad but whatever
                    return ev;
                alpha = std::max(alpha, ev);
            } else {
                if (ls.empty())
                    return mateScore(depth);

                if (canForceDraw) return 0;

                if (depth <= QUIESC_DEPTH || !eng->is_running())
                    return search<true>(alpha, beta, depth - 1);
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
            if (tt.hash != pos->get_state()->hash || tt_strength(depth, QUIESC) >= tt_strength(tt.depth, tt.isQuiesc)) {
                tt.hash = 0;
                tt.depth = depth;
                tt.isQuiesc = QUIESC;
                tt.score = value;
                tt.hash = pos->get_state()->hash;
            }

            return value;
        }
    };


    void workerFunc(EngineV2 *eng) {
        Position cpos;
        eng->pos->copy_into(&cpos);

        while (eng->is_running()) {
            SearchTask task;
            {
                std::unique_lock<std::mutex> lg(eng->taskMtx);
                eng->taskCv.wait(lg, [&]() { return !eng->is_running() || !eng->tasks.empty(); });
                if (!eng->is_running()) return;

                task = eng->tasks.top();
                eng->tasks.pop();
            }
            std::cout << "info string exec " << task.mov.long_alg_notation() << " rank " << task.score / (double) PAWN_SCORE << " depth " << task.depth << '\n';

            SearchThread me{&cpos, static_cast<DepthT>(task.depth), eng};

            make_move(cpos, task.mov);
            const ScoreT score = -me.search<false>(MIN_SCORE, MAX_SCORE, task.depth - 1);

            std::cout << "info string mov " << task.mov.long_alg_notation() << " score " 
                      << (double) score / PAWN_SCORE << " ttHits = " << me.getTTHits() 
                      << " depth " << task.depth << '\n';

            unmake_move(cpos, task.mov);

            if (!eng->running)
                return;

            if (score > eng->best_score) {
                eng->best_score = score;
                eng->best_mov = task.mov;
            }

            task.depth++;
            task.score = score;

            {
                std::unique_lock<std::mutex> lg(eng->taskMtx);
                eng->tasks.push(task);
            }

            eng->taskCv.notify_one();
        }
    }

    void EngineV2::start_search(int maxDepth) {
        MoveList ls = legal_moves_from<false>(*pos);

        while (!tasks.empty())
            tasks.pop();

        workers.clear();
        running = true;
        best_score = MIN_SCORE;

        for (const auto &mov : ls) {
            SearchTask task{};
            task.mov = mov;
            task.score = 0;
            task.depth = QUIESC_DEPTH + 2;

            tasks.push(task);
        }

        for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++)
            workers.push_back(std::thread(workerFunc, this));
    }

    void EngineV2::stop_search() {
        running = false;
        taskCv.notify_all();
        for (auto &thread : workers)
            thread.join();
        workers.clear();

        while (!tasks.empty())
            tasks.pop();
    }
}