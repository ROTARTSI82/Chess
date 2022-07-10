// DO NOT DIRECTLY INCLUDE THIS FILE
// THIS IS PART OF movegen.hpp!!

#define ACCUM_MOVES(FUN, BB, LAND, LS, POS) {                   \
    Bitboard _scacus_bb = BB;                                   \
    while (_scacus_bb) {                                        \
        Square SQ = pop_lsb(_scacus_bb);                        \
        Bitboard _scacus_it = (FUN) & (LAND);                   \
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

        const Bitboard kingBb = self & pos.by_type(KING);
        const Square kingSq = get_lsb(kingBb);

        Bitboard attk = 0; // bitboard being attacked by the opposite side
        Bitboard checkers = 0; // pieces giving check directly
        {
            Bitboard it = opponent & (pos.by_type(BISHOP) | pos.by_type(QUEEN));
            checkers |= it & lookup<BISHOP_MAGICS>(kingSq, occ);
            while (it) attk |= lookup<BISHOP_MAGICS>(pop_lsb(it), occ ^ kingBb);

            it = opponent & (pos.by_type(ROOK) | pos.by_type(QUEEN));
            checkers |= it & lookup<ROOK_MAGICS>(kingSq, occ);
            while (it) attk |= lookup<ROOK_MAGICS>(pop_lsb(it), occ ^ kingBb);

            it = opponent & pos.by_type(KNIGHT);
            checkers |= it & knight_moves(kingSq);
            while (it) attk |= knight_moves(pop_lsb(it));

            it = opponent & pos.by_type(PAWN);
            checkers |= it & pawn_attacks<SIDE>(kingSq);
            while (it) attk |= pawn_attacks<opposite_side(SIDE)>(pop_lsb(it));

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

        pos.isInCheck = false;
        Bitboard landing = ~self;  // squares we are allowed to land on
        if (checkers) {
            pos.isInCheck = true;

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

            // castling
            if (!checkers) {
                constexpr auto sideIndex = (SIDE == WHITE_SIDE ? 2 : 0);
                bool canKingside = pos.state.castlingRights & KINGSIDE_MASK << sideIndex;
                bool canQueenside = pos.state.castlingRights & QUEENSIDE_MASK << sideIndex;

                constexpr Bitboard checkMaskQueenside = CASTLING_ATTACK_MASKS[0 + sideIndex];
                constexpr Bitboard checkMaskKingside = CASTLING_ATTACK_MASKS[1 + sideIndex];
                constexpr Bitboard occMaskQueenside = CASTLING_OCCUPANCY_MASKS[0 + sideIndex];
                constexpr Bitboard occMaskKingside = CASTLING_OCCUPANCY_MASKS[1 + sideIndex];

                // these squares can't be attacked
                canKingside = canKingside && !(checkMaskKingside & attk) && !(occMaskKingside & occ);
                canQueenside = canQueenside && !(checkMaskQueenside & attk) && !(occMaskQueenside & occ);

                if (canKingside)
                    ls.push_back(make_move<CASTLE>(kingSq, kingSq + 2 * Dir::E));
                if (canQueenside)
                    ls.push_back(make_move<CASTLE>(kingSq, kingSq + 2 * Dir::W));
            }

            // pawns
            it = self & pos.by_type(PAWN);
            while (it) {
                Square SQ = pop_lsb(it);
                Bitboard destinations = ((pawn_attacks<SIDE>(SQ) & opponent) | pawn_moves<SIDE>(SQ, occ)) & landing;

                if (pinned & to_bitboard(SQ)) // restrict movement of pinned pieces
                    destinations &= pinLines[SQ];

                Bitboard promotions = destinations & (rank_bb(8) | rank_bb(1));
                destinations &= ~promotions;

                while (destinations)
                    ls.push_back(make_normal(SQ, pop_lsb(destinations)));

                while (promotions) {
                    Square dst = pop_lsb(promotions);
                    for (PromoteType to: {PROMOTE_QUEEN, PROMOTE_BISHOP, PROMOTE_KNIGHT, PROMOTE_ROOK})
                        ls.push_back(make_promotion(SQ, dst, to));
                }
            }
        }

        // movement of pinned queens, rooks, and bishops,
        // which can only move along their pin lines.
        // pinned knights can never move,
        // and pinned pawns are handled in the normal pawn logic
        {
            Bitboard it = pinned & (pos.by_type(QUEEN) | pos.by_type(ROOK) | pos.by_type(BISHOP));
            while (it) {
                Square sq = pop_lsb(it);
                Bitboard destinations = pinLines[sq] & landing;

                if (type_of(pos.pieces[sq]) == ROOK)
                    destinations &= pseudo_attacks<ROOK_MAGICS>(sq);
                else if (type_of(pos.pieces[sq]) == BISHOP)
                    destinations &= pseudo_attacks<BISHOP_MAGICS>(sq);

                while (destinations)
                    ls.push_back(make_normal(sq, pop_lsb(destinations)));
            }

        }

        // en passant
        if (pos.state.enPassantTarget != NULL_SQUARE) {
            Bitboard it = self & pos.by_type(PAWN) & pawn_attacks<opposite_side(SIDE)>(pos.state.enPassantTarget);
            Bitboard ep = to_bitboard(pos.state.enPassantTarget);

            while (it) {
                const Square sq = pop_lsb(it);
                const Bitboard bb = to_bitboard(sq);

                const Bitboard capPawn = to_bitboard(pos.state.enPassantTarget
                                                     + (SIDE == BLACK_SIDE ? Dir::N : Dir::S));

                // if we are pinned, we need to land on the pin line
                #define PIN_PASSED !(bb & pinned) || (ep & pinLines[sq])

                // if we are in check, we need to capture the checker or land between the king & checker (landings)
                #define CHECK_PASSED !checkers || (ep & landing) || (capPawn & checkers)

                // Taking en passant can be disallowed if it reveals a check to a rook or queen on the side
                // see 8/8/8/3KPp1r/8/8/8/8 w - f6 0 1
                #define SEES_KING lookup<ROOK_MAGICS>(kingSq, occ ^ bb ^ capPawn ^ ep)
                #define TARGET_PASSED !(SEES_KING & ~checkers & opponent & \
                                       (pos.by_type(QUEEN) | pos.by_type(ROOK)))

                if ((PIN_PASSED) && (CHECK_PASSED) && (TARGET_PASSED))
                    ls.push_back(make_move<EN_PASSANT>(sq, pos.state.enPassantTarget));
            }
        }
    }

    inline constexpr void legal_moves_from(MoveList &ls, Position &pos) {
        if (pos.turn == WHITE_SIDE)
            standard_moves<WHITE_SIDE>(ls, pos);
        else
            standard_moves<BLACK_SIDE>(ls, pos);
    }

    inline MoveList legal_moves_from(Position &pos) {
        MoveList legals(0);
        legal_moves_from(legals, pos);
        return legals;
    }
}