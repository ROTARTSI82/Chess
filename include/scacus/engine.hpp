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
    using DepthT = unsigned;
    using ScoreT = int;

    constexpr auto QUIESC_DEPTH = 32;

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

        std::vector<std::thread> workers;
        std::priority_queue<SearchTask> tasks;
        std::mutex taskMtx;
        std::condition_variable taskCv;

        Move best_mov;
        std::atomic<ScoreT> best_score;

        std::atomic<bool> running = true;

        friend void workerFunc(EngineV2 *);

    public:
        EngineV2() = default;

        void start_search(int maxDepth = 99);
        void stop_search();

        inline void set_pos(Position *p) {
            pos = p;
        }

        [[nodiscard]] inline Move best_move() const {
            return best_mov;
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