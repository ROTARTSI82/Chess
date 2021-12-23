#include "scacus/movegen.hpp"

namespace sc {
    template <Side SIDE>
    MoveList antichess_moves(const Position &pos);

    StateInfo antichess_make_move(Position &pos, Move mov);
    void antichess_unmake_move(Position &pos, const StateInfo &info, Move mov);

    std::pair<Move, int> antichess_search(Position &pos, int depth);
}