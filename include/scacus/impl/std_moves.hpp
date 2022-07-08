// DO NOT DIRECTLY INCLUDE THIS FILE
// THIS IS PART OF movegen.hpp!!

namespace sc {

    template <Side SIDE>
    struct CheckersInfo {
        Bitboard kingBB;
        Square kingSq;

        Bitboard otherOcc;
        Bitboard occ;

        Bitboard knight, bishopQueen, rookQueen, pawn;
        Bitboard all, lines;

        explicit inline constexpr CheckersInfo(Position &pos) : kingBB(pos.by_side(SIDE) & pos.by_type(KING)),
            kingSq(get_lsb(kingBB)),
            otherOcc(pos.by_side(opposite_side(SIDE))),
            occ(otherOcc | pos.by_side(SIDE)),
            knight(knight_moves(kingSq) & otherOcc & pos.by_type(KNIGHT)),
            bishopQueen(lookup<BISHOP_MAGICS>(kingSq, occ) & otherOcc & (pos.by_type(QUEEN) | pos.by_type(BISHOP))),
            rookQueen(lookup<ROOK_MAGICS>(kingSq, occ) & otherOcc & (pos.by_type(QUEEN) | pos.by_type(ROOK))),
            pawn(pawn_attacks(SIDE, kingSq) & otherOcc & pos.by_type(PAWN)),
            all(knight | bishopQueen | rookQueen | pawn),
            lines()
        {

        }
    };

    template <Side SIDE>
    MoveList standard_moves(Position &pos);

    inline MoveList legal_moves_from(Position &pos) {
        return pos.turn == WHITE_SIDE ? standard_moves<WHITE_SIDE>(pos) : standard_moves<BLACK_SIDE>(pos);
    }
}