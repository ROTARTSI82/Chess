#pragma once

#include "scacus/movegen.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

#include <cstring> // memset
#include <unordered_map>
#include <condition_variable>

namespace sc {
    using DepthT = int;
    using ScoreT = int;

    constexpr auto QUIESC_DEPTH = 0;
    constexpr ScoreT PAWN_SCORE = 512;

    // We can't actually use min because -min is not max! In fact, -min is negative! 
    constexpr auto MATE_SCORE = -10000 * PAWN_SCORE;
    constexpr auto MATE_STEP = 200 * PAWN_SCORE;
    constexpr auto MIN_SCORE = std::numeric_limits<ScoreT>::min() + 2;
    constexpr auto MAX_SCORE = std::numeric_limits<ScoreT>::max() - 2;

    constexpr auto MOBILITY_VALUE = PAWN_SCORE / 512;


    // transposition tables are global and defined in engine.cpp

    struct SearchTask {
        Move mov{};
        DepthT depth = QUIESC_DEPTH;
        ScoreT score = 0;

        bool operator<(const SearchTask &rhs) const {
            if (depth == rhs.depth)
                return score < rhs.score;
            return depth > rhs.depth;
            // return (10.0 + score) / (1.0 + 10.0 * depth) < (10.0 + score) / (1.0 + 10.0 * rhs.depth);
        }
    };

    class EngineV2 {
    private:
        Position *pos;

        // why did i name some with camelCase and some with snake_case?
        // i don't even know
        std::vector<std::thread> workers;
        std::priority_queue<SearchTask> tasks;
        std::mutex taskMtx, bestMtx;
        std::condition_variable taskCv;

        struct EngineLine {
            Move best_mov{};
            ScoreT best_score = MIN_SCORE;
        };

        EngineLine prelim_line, true_line;
        DepthT search_depth = 0; // the depth level we are currently searching

        std::atomic<bool> running = true;

        friend void workerFunc(EngineV2 *);

    public:
        EngineV2() = default;

        EngineV2(const EngineV2 &) = delete;
        EngineV2 &operator=(const EngineV2 &) = delete;

        void start_search(int maxDepth = 99);
        void stop_search();

        inline void set_pos(Position *p) {
            pos = p;
        }

        [[nodiscard]] inline Move best_move() const {
            return true_line.best_mov;
        }

        // can be an estimate. Search is optimized for best
        [[nodiscard]] inline Move worst_move() const {
            return Move{};
        }

        inline bool is_running() const {
            return running;
        }
    };
}