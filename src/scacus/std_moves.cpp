#include "scacus/movegen.hpp"

// this is some cryptic macro usage that probably isn't ideal
#define ACCUM_MOVES(FUN, BB, LAND, LS, POS) {                       \
    Bitboard _scacus_bb = BB;                                       \
    while (_scacus_bb) {                                            \
        Square SQ = pop_lsb(_scacus_bb);                            \
        Bitboard _scacus_it = (FUN) & (LAND);                       \
        while (_scacus_it)                                          \
            LS.push_back(new_move_normal(SQ, pop_lsb(_scacus_it))); \
    }                                                               \
}

namespace sc {

    /**
     * Calculate the pieces on the board that are currently pinned
     * @param pos Chess position
     * @param pinnable Candidates for pieces which might be pinned
     * @param blocking Pieces that can block a pin (i.e. opponent's own pieces breaking the pin)
     * @param pinnerCandidates Pieces that are candidates to be pinning something
     * @param kingSq King square, or the square to which pieces are being binned
     * @param pinLines Array of 64 Bitboards to store the lines pieces are pinned to into.
     * @return Pieces that are pinned
     */
    [[nodiscard]] static inline Bitboard calc_pinned(Position &pos, Bitboard pinnable, Bitboard blocking,
                                                     Bitboard pinnerCandidates, Square kingSq, Bitboard *pinLines) {
        Bitboard pinned = 0;
        {
            // opponent's sliders which can be pinning
            Bitboard sliders = (lookup<BISHOP_MAGICS>(kingSq, blocking) & (pos.by_type(BISHOP) | pos.by_type(QUEEN)))
                               | (lookup<ROOK_MAGICS>(kingSq, blocking) & (pos.by_type(ROOK) | pos.by_type(QUEEN)));
            sliders &= pinnerCandidates;

            while (sliders) {
                Square sq = pop_lsb(sliders);
                Bitboard pinLine = pin_line(kingSq, sq);

                // we remove sq when testing for the case when sq appears in both pinnerCandidates and pinnable
                // we only want to check for `pinnable` pieces appearing between the two squares, exclusive.
                Bitboard mine = pinnable & (pinLine & ~to_bitboard(sq));

                if (mine && (mine & (mine - 1)) == 0) {
                    // `mine` has exactly 1 bit set, the piece is pinned!
                    pinned |= mine;
                    pinLines[get_lsb(mine)] = pinLine;
                }
            }
        }

        return pinned;
    }

    template <Side SIDE, bool QUIESC>
    void standard_moves(MoveList &ls, Position &pos) {
        pos.isInCheck = false;

        const Bitboard opponent = pos.by_side(opposite_side(SIDE));
        const Bitboard self = pos.by_side(SIDE);
        const Bitboard occ = opponent | self;

        const Bitboard kingBb = self & pos.by_type(KING);
        const Square kingSq = get_lsb(kingBb);
        const Square opponentKing = get_lsb(opponent & pos.by_type(KING));

        if (QUIESC && (kingBb == 0 || (opponent & pos.by_type(KING)) == 0)) {
            dbg_dump_position(pos);
        }

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
                ls.push_back(new_move_normal(kingSq, pop_lsb(it)));

            pos.isInCheck = true;
            return;
        }

        // pieces that are pinned
        Bitboard pinLines[64];
        Bitboard pinned = calc_pinned(pos, self, opponent, opponent & ~checkers, kingSq, pinLines);

        const bool DO_QUIESC = QUIESC && !checkers;
        Bitboard discoveryLines[64];
        Bitboard discoveredChecks = 0;
        if (DO_QUIESC) {
            for (Bitboard &discoveryLine : discoveryLines)
                discoveryLine = 0;
            discoveredChecks = calc_pinned(pos, self, 0ULL, self, opponentKing, discoveryLines);
        }

        pos.isInCheck = false;
        Bitboard landing = ~self; // squares we are allowed to land on
        if (checkers) {
            pos.isInCheck = true;

            // single check: We are being checked by a single piece. Either block or move a piece
            Square sq = get_lsb(checkers);

            // we can only land on the pin line to block checks of sliding pieces
            // this produces the correct result even for knights and pawns
            // see information about PIN_LINES
            landing &= pin_line(kingSq, sq);
        }

        // discovered checks is only set for quiescence, which handle them specially
        Bitboard normals = self & ~pinned;
        if (DO_QUIESC) normals &= ~discoveredChecks;
        {
            // note: in quiescence searches only look for moves giving check or capturing a piece
            Bitboard it = normals & (pos.by_type(BISHOP) | pos.by_type(QUEEN));
            Bitboard quiescTerm = DO_QUIESC ? occ | lookup<BISHOP_MAGICS>(opponentKing, occ) : ~0ULL;
            ACCUM_MOVES(lookup<BISHOP_MAGICS>(SQ, occ), it, landing & quiescTerm, ls, pos);

            it = normals & (pos.by_type(ROOK) | pos.by_type(QUEEN));
            quiescTerm = DO_QUIESC ? occ | lookup<ROOK_MAGICS>(opponentKing, occ) : ~0ULL;
            ACCUM_MOVES(lookup<ROOK_MAGICS>(SQ, occ), it, landing & quiescTerm, ls, pos);

            it = normals & pos.by_type(KNIGHT);
            quiescTerm = DO_QUIESC ? occ | knight_moves(opponentKing) : ~0ULL;
            ACCUM_MOVES(knight_moves(SQ), it, landing & quiescTerm, ls, pos);

            // king movement
            it = king_moves(kingSq) & ~attk & ~self;

            // allow king to either capture or create discovered check
            if (DO_QUIESC)
                it &= occ | ((kingBb & discoveredChecks) != 0 ? ~discoveryLines[kingSq] : 0ULL);

            while (it)
                ls.push_back(new_move_normal(kingSq, pop_lsb(it)));

            // castling
            if (!checkers) {
                constexpr auto sideIndex = (SIDE == WHITE_SIDE ? 2 : 0);
                bool canKingside = pos.state->castlingRights & KINGSIDE_MASK << sideIndex;
                bool canQueenside = pos.state->castlingRights & QUEENSIDE_MASK << sideIndex;

                constexpr Bitboard checkMaskQueenside = CASTLING_ATTACK_MASKS[0 + sideIndex];
                constexpr Bitboard checkMaskKingside = CASTLING_ATTACK_MASKS[1 + sideIndex];
                constexpr Bitboard occMaskQueenside = CASTLING_OCCUPANCY_MASKS[0 + sideIndex];
                constexpr Bitboard occMaskKingside = CASTLING_OCCUPANCY_MASKS[1 + sideIndex];

                // these squares can't be attacked
                canKingside = canKingside && !(checkMaskKingside & attk) && !(occMaskKingside & occ);
                canQueenside = canQueenside && !(checkMaskQueenside & attk) && !(occMaskQueenside & occ);

                if (DO_QUIESC) {
                    // only allow castling if it gives check. pin_line includes the second square but not the first.
                    canKingside = canKingside && ((occ & pin_line(opponentKing, kingSq + Dir::E)) != 0);
                    canQueenside = canQueenside && ((occ & pin_line(opponentKing, kingSq + Dir::W)) != 0);
                }

                if (canKingside)
                    ls.push_back(new_move<CASTLE>(kingSq, kingSq + 2 * Dir::E));
                if (canQueenside)
                    ls.push_back(new_move<CASTLE>(kingSq, kingSq + 2 * Dir::W));
            }

            // pawns
            it = self & pos.by_type(PAWN);
            while (it) {
                Square SQ = pop_lsb(it);
                Bitboard destinations = ((pawn_attacks<SIDE>(SQ) & opponent) | pawn_moves<SIDE>(SQ, occ)) & landing;

                if (pinned & to_bitboard(SQ)) // restrict movement of pinned pieces
                    destinations &= pinLines[SQ];

                // in quiescence search, limit to discovered checks and captures
                if (DO_QUIESC) {
                    Bitboard checkSqs = pawn_attacks<opposite_side(SIDE)>(opponentKing);
                    Bitboard discoveryTerm = ((to_bitboard(SQ) & discoveredChecks) != 0 ? ~discoveryLines[SQ] : 0ULL);
                    destinations &= checkSqs | occ | discoveryTerm;
                }

                Bitboard promotions = destinations & (rank_bb(8) | rank_bb(1));
                while (promotions) { // always look at promotions
                    Square dst = pop_lsb(promotions);
                    // bishop and rook desirable for stalemate tricks
                    for (PromoteType to: {PROMOTE_QUEEN, PROMOTE_BISHOP, PROMOTE_KNIGHT, PROMOTE_ROOK})
                        ls.push_back(new_promotion(SQ, dst, to));
                }

                destinations &= ~(rank_bb(8) | rank_bb(1)); // these are promotion squares
                while (destinations)
                    ls.push_back(new_move_normal(SQ, pop_lsb(destinations)));
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
                
                if (DO_QUIESC) {
                    Bitboard quiescAllowed = (to_bitboard(sq) & discoveredChecks) != 0 ? ~discoveryLines[sq] : 0ULL;
                    quiescAllowed |= occ;
                    
                    // TODO: These moves are probably bad, so do we even bother searching for them???
                    Type type = type_of(pos.pieces[sq]);
                    if (type == ROOK || type == QUEEN)
                        quiescAllowed |= pseudo_attacks<ROOK_MAGICS>(opponentKing);
                    if (type == BISHOP || type == QUEEN)
                        quiescAllowed |= pseudo_attacks<BISHOP_MAGICS>(opponentKing);
                    
                    destinations &= quiescAllowed;
                }

                while (destinations)
                    ls.push_back(new_move_normal(sq, pop_lsb(destinations)));
            }
        }

        // special handling of discovered checks
        if (DO_QUIESC) {
            // kings, pawns, and pinned pieces are handled already seperately.
            discoveredChecks &= pos.by_type(BISHOP) | pos.by_type(QUEEN) | pos.by_type(ROOK) | pos.by_type(KNIGHT);
            discoveredChecks &= ~pinned;

            // todo;
        }

        // en passant: always allowed even in quiescence
        if (pos.state->enPassantTarget != NULL_SQUARE) {
            Bitboard it = self & pos.by_type(PAWN) & pawn_attacks<opposite_side(SIDE)>(pos.state->enPassantTarget);
            Bitboard ep = to_bitboard(pos.state->enPassantTarget);

            while (it) {
                const Square sq = pop_lsb(it);
                const Bitboard bb = to_bitboard(sq);

                const Bitboard capPawn = to_bitboard(pos.state->enPassantTarget
                                                     + (SIDE == BLACK_SIDE ? Dir::N : Dir::S));

                // if we are pinned, we need to land on the pin line
                const bool PIN_PASSED = !(bb & pinned) || (ep & pinLines[sq]);

                // if we are in check, we need to capture the checker or land between the king & checker (landings)
                const bool CHECK_PASSED = !checkers || (ep & landing) || (capPawn & checkers);

                // Taking en passant can be disallowed if it reveals a check to a rook or queen on the side
                // see 8/8/8/3KPp1r/8/8/8/8 w - f6 0 1
                const Bitboard SEES_KING = lookup<ROOK_MAGICS>(kingSq, occ ^ bb ^ capPawn ^ ep);
                const bool TARGET_PASSED = !(SEES_KING & ~checkers & opponent &
                        (pos.by_type(QUEEN) | pos.by_type(ROOK)));

                if ((PIN_PASSED) && (CHECK_PASSED) && (TARGET_PASSED))
                    ls.push_back(new_move<EN_PASSANT>(sq, pos.get_state()->enPassantTarget));
            }
        }
    }

    template void standard_moves<BLACK_SIDE, false>(MoveList &, Position &);
    template void standard_moves<WHITE_SIDE, false>(MoveList &, Position &);

    template void standard_moves<BLACK_SIDE, true>(MoveList &, Position &);
    template void standard_moves<WHITE_SIDE, true>(MoveList &, Position &);

}