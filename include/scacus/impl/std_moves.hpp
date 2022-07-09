// DO NOT DIRECTLY INCLUDE THIS FILE
// THIS IS PART OF movegen.hpp!!

#define ACCUM_MOVES(FUN, BB, LAND, LS, POS) {                   \
    Bitboard _scacus_bb = BB;                                   \
    while (_scacus_bb) {                                        \
        Square SQ = pop_lsb(_scacus_bb);                        \
        Bitboard _scacus_it = FUN & (LAND);                     \
        while (_scacus_it)                                      \
            LS.push_back(make_normal(SQ, pop_lsb(_scacus_it))); \
    }                                                           \
}

namespace sc {

    template <Side SIDE>
    inline constexpr void standard_moves(MoveList &ls, Position &pos) {
        pos.isInCheck = false;

        const Bitboard opponent = pos.by_side(opposite_side(SIDE));
        const Bitboard self = pos.by_side(SIDE);
        const Bitboard occ = opponent | self;

        const Square kingSq = get_lsb(self & pos.by_type(KING));

        Bitboard attk = 0; // bitboard being attacked by the opposite side
        Bitboard checkers = 0; // pieces giving check directly
        {
            Bitboard it = opponent & (pos.by_type(BISHOP) | pos.by_type(QUEEN));
            checkers |= it & lookup<BISHOP_MAGICS>(kingSq, occ);
            while (it) attk |= lookup<BISHOP_MAGICS>(pop_lsb(it), occ);

            it = opponent & (pos.by_type(ROOK) | pos.by_type(QUEEN));
            checkers |= it & lookup<ROOK_MAGICS>(kingSq, occ);
            while (it) attk |= lookup<ROOK_MAGICS>(pop_lsb(it), occ);

            it = opponent & pos.by_type(KNIGHT);
            checkers |= it & knight_moves(kingSq);
            while (it) attk |= knight_moves(pop_lsb(it));

            it = opponent & pos.by_type(PAWN);
            checkers |= it & pawn_attacks(SIDE, kingSq);
            while (it) attk |= pawn_attacks(opposite_side(SIDE), pop_lsb(it));

            attk |= king_moves(get_lsb(opponent & pos.by_type(KING)));
        }

        if (checkers && (checkers & (checkers - 1)) != 0) {
            // checkers more than 1 bit set: multiple pieces are giving check
            // we MUST move the king to a safe square
            Bitboard it = king_moves(kingSq) & ~self & ~attk;
            while (it)
                ls.push_back(make_normal(kingSq, pop_lsb(it)));

            pos.isInCheck = true;
            return;
        }

        Bitboard pinned = 0;
        Bitboard pinLines[BOARD_SIZE];
        {
            // opponent's sliders which can be pinning
            Bitboard sliders = (lookup<BISHOP_MAGICS>(kingSq, opponent) & (pos.by_type(BISHOP) | pos.by_type(QUEEN)))
                    | (lookup<ROOK_MAGICS>(kingSq, opponent) & (pos.by_type(ROOK) | pos.by_type(QUEEN)));
            sliders &= opponent & ~checkers;

            while (sliders) {
                Square sq = pop_lsb(sliders);
                Bitboard pinLine = pin_line(kingSq, sq);
                Bitboard mine = self & pinLine;

                if (mine && (mine & (mine - 1)) == 0) {
                    // `mine` has exactly 1 bit set, the piece is pinned!
                    pinned |= mine;
                    pinLines[get_lsb(mine)] = pinLine;
                }
            }
        }

        Bitboard landing = ~self;  // squares we are allowed to land on
        if (checkers) {
            // single check: We are being checked by a single piece. Either block or move a piece
            Square sq = get_lsb(checkers);

            // we can only land on the pin line to block checks of sliding pieces
            // this produces the correct result even for knights and pawns
            // see information about PIN_LINES
            landing &= pin_line(kingSq, sq);
        }

        Bitboard normals = self & ~pinned;
        {
            Bitboard it = normals & (pos.by_type(BISHOP) | pos.by_type(QUEEN));
            ACCUM_MOVES(lookup<BISHOP_MAGICS>(SQ, occ), it, landing, ls, pos);

            it = normals & (pos.by_type(ROOK) | pos.by_type(QUEEN));
            ACCUM_MOVES(lookup<ROOK_MAGICS>(SQ, occ), it, landing, ls, pos);

            it = normals & pos.by_type(KNIGHT);
            ACCUM_MOVES(knight_moves(SQ), it, landing, ls, pos);

            it = king_moves(kingSq) & ~attk & ~self;
            while (it)
                ls.push_back(make_normal(kingSq, pop_lsb(it)));

            // todo: castling

            // todo: pawns
        }

        // todo: movement of pinned pieces
        {

        }
    }

    inline constexpr void legal_moves_from(MoveList &ls, Position &pos) {
        if (pos.turn == WHITE_SIDE)
            standard_moves<WHITE_SIDE>(ls, pos);
        else
            standard_moves<BLACK_SIDE>(ls, pos);
    }
}