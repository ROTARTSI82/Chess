#pragma once

#include "scacus/movegen.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

#include <cstring> // memset
#include <unordered_map>

namespace sc {

    // transposition tables are global and defined in engine.cpp

    class EngineV2 {
    private:
        struct SearchTask {

        };

        Position *pos;

        std::vector<std::thread> workers;
        std::queue<SearchTask> tasks;

    public:
        EngineV2() = default;

        void start_search(int maxDepth = 99);
        void stop_search();

        inline void set_pos(Position *p) {
            pos = p;
        }

        [[nodiscard]] inline Move best_move() const {
            return Move{};
        }

        // can be an estimate. Search is optimized for best
        [[nodiscard]] inline Move worst_move() const {
            return Move{};
        }
    };
}