// DO NOT DIRECTLY INCLUDE THIS FILE
// THIS IS PART OF movegen.hpp!!

namespace sc {
    template <Side SIDE>
    MoveList standard_moves(Position &pos);

    inline MoveList legal_moves_from(Position &pos) {
        return pos.turn == WHITE_SIDE ? standard_moves<WHITE_SIDE>(pos) : standard_moves<BLACK_SIDE>(pos);
    }
}