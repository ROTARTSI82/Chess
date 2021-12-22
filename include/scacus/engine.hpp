#pragma once

#include "scacus/movegen.hpp"

#include <thread>
#include <atomic>
#include <mutex>

namespace sc {
    std::pair<Move, int> primitive_search(Position &pos, int depth);
}