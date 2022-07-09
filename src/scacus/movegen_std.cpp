/*#include "scacus/movegen.hpp"


namespace sc {
    template <Side SIDE>
    static constexpr inline uint64_t en_passant_is_legal(const Position &pos, const Square sq);

    static constexpr inline bool check_resolved_sq(const Position &pos, const Bitboard occ, const Bitboard kingBB, const Square sq) {
        switch (type_of(pos.pieces[sq])) {
        case QUEEN:
            return !((lookup<BISHOP_MAGICS>(sq, occ) | lookup<ROOK_MAGICS>(sq, occ)) & kingBB);
        case ROOK:
            return !(lookup<ROOK_MAGICS>(sq, occ) & kingBB);
        case BISHOP:
            return !(lookup<BISHOP_MAGICS>(sq, occ) & kingBB);
        case PAWN:
            return !(pawn_attacks(side_of(pos.pieces[sq]), sq) & kingBB);
        case KNIGHT:
            return !(knight_moves(sq) & kingBB);
        default:
            UNDEFINED();
        }
    }

    // checks if there is no check given by checker with the given occupancy bits
    static constexpr inline bool check_resolved(const Position &pos, const Bitboard occ, const Bitboard checker, const Bitboard kingBB) {
        return check_resolved_sq(pos, occ, kingBB, std::countr_zero(checker));
    }

    // Assumption: You can't en passant and promote at the same time.
    template <Side SIDE>
    MoveList standard_moves(Position &pos) {
        static_assert(SIDE == WHITE_SIDE || SIDE == BLACK_SIDE);

        MoveList ret(0);
        auto check = CheckersInfo<SIDE>(pos);

//        const Bitboard occ = pos.by_side(WHITE_SIDE) | pos.by_side(BLACK_SIDE);
//
//        // now we generate a list of pinned pieces
//        Bitboard kingBB = pos.by_side(SIDE) & pos.by_type(KING);
//        Square king = std::countr_zero<uint64_t>(kingBB);
//        // is seperating these into two passes of rooks/queens + bishops/queens worth it?
//
        Bitboard opposingIter = check.otherOcc;
//        Bitboard checkers = 0;
//        Bitboard checkerAttk = 0; // squares attacked by checker (set to the last checker found)
        Bitboard underFire = 0; // squares attacked by other side. pinned pieces and king are considered to attack normal squares.
        while (opposingIter) {
            Square sq = pop_lsb(opposingIter);
            Type type = type_of(pos.pieces[sq]);

            switch (type) {
            case PAWN:
                underFire |= pawn_attacks(opposite_side(SIDE), sq);
                break;
            case KING:
                underFire |= king_moves(sq);
                break;
            case QUEEN:
                underFire |= lookup<BISHOP_MAGICS>(sq, check.occ) | lookup<ROOK_MAGICS>(sq, check.occ);
                break;
            case BISHOP:
                underFire |= lookup<BISHOP_MAGICS>(sq, check.occ);
                break;
            case ROOK:
                underFire |= lookup<ROOK_MAGICS>(sq, check.occ);
                break;
            case KNIGHT:
                underFire |= knight_moves(sq);
                break;
            default:
                UNDEFINED();
            }
        }


        // QUEEN_MOVES + BISHOP_MOVES covers pawns and opposing king
        // is computing the checkers mask even worth it? Would just looping through all opponent pieces be faster?
        opposingIter = pos.by_side(opposite_side(SIDE));
        // mask of possible pieces that can be checking the king
        Bitboard possiblePinners = opposingIter & (pos.by_type(QUEEN) | pos.by_type(ROOK) | pos.by_type(BISHOP));
        // blocked by opposing pieces, ignoring own pieces.
        Bitboard pinLines = lookup<ROOK_MAGICS>(check.kingSq, opposingIter) | lookup<BISHOP_MAGICS>(check.kingSq, opposingIter);
        possiblePinners &= pinLines;
        possiblePinners &= ~check.all; // cannot be already giving check!
        Bitboard maybePinned = pos.by_side(SIDE) & pinLines;
        Bitboard maybePinnedIter = maybePinned;
        Bitboard pinned = 0;

        // the pinner pinning the piece at a specific sqaure
        Square pinnerOf[64];

        while (maybePinnedIter) {
            Square sq = pop_lsb(maybePinnedIter);
            Bitboard bb = to_bitboard(sq);

            // test if removing this piece will reveal an attack on the king
            maybePinned ^= bb; // remove this piece from the occupancy

            Bitboard pinnerIter = possiblePinners;

            while (pinnerIter) {
                Square pinnerSq = pop_lsb(pinnerIter);
                Bitboard pinnerAttk = 0;

                switch (type_of(pos.pieces[pinnerSq])) {
                case BISHOP:
                    pinnerAttk = lookup<BISHOP_MAGICS>(pinnerSq, maybePinned);
                    break;
                case ROOK:
                    pinnerAttk = lookup<ROOK_MAGICS>(pinnerSq, maybePinned) & check.kingBB;
                    break;
                case QUEEN:
                    pinnerAttk = (lookup<ROOK_MAGICS>(pinnerSq, maybePinned) | lookup<BISHOP_MAGICS>(pinnerSq, maybePinned));
                    break;
                default:
                    UNDEFINED();
                }

                if (pinnerAttk & check.kingBB) {
                    pinned |= bb;
                    pinnerOf[sq] = pinnerSq;
                    break;
                }
            }

            maybePinned ^= bb; // add it back
        }

        Bitboard iter = pos.by_side(SIDE);
        Bitboard attk = 0;

        if (check.all) { // i.e: in check
            pos.isInCheck = true;

            Square sq;

            auto plain_adder = [&](const Square _dstsq) { ret.push_back(make_normal(sq, _dstsq)); };
            auto if_resolves_check = [&](const std::function<void(const Square)> &action) {
                while (attk) {
                    Square _dstsq = pop_lsb(attk);

                    // in this case we capture the piece giving check!
                    if (to_bitboard(_dstsq) & check.all) {
                        action(_dstsq);
                    } else {
                        Bitboard testOcc = check.occ;
                        Bitboard fromBb = to_bitboard(sq);
                        Bitboard toBb = to_bitboard(_dstsq);

                        testOcc ^= fromBb;
                        testOcc |= toBb;

                        if (check_resolved(pos, testOcc, check.all, check.kingBB)) {
                            action(_dstsq);
                        }
                    }
                }
            };


            if (popcnt(check.all) == 1) { // only 1 checker. we can block by moving a piece.


                iter &= ~pinned; // pinned pieces cannot be moved while in check, i hope! TODO: find if this assumption is correct

                // taking to checker, or squares the checker attacks (so that we might block the check)
                const Bitboard checkerAttk = lookup<BISHOP_MAGICS>(check.kingSq, check.occ) |
                                             lookup<ROOK_MAGICS>(check.kingSq, check.occ);

                const Bitboard landingSquares = check.all | (checkerAttk & ~check.occ);

                while (iter) {
                    sq = pop_lsb(iter);


                    switch (type_of(pos.pieces[sq])) {
                    case KNIGHT:
                        attk = knight_moves(sq) & landingSquares;
                        if_resolves_check(plain_adder);
                        break;
                    case QUEEN:
                        attk = (lookup<ROOK_MAGICS>(sq, check.occ) | lookup<BISHOP_MAGICS>(sq, check.occ)) & landingSquares;
                        if_resolves_check(plain_adder);
                        break;
                    case BISHOP:
                        attk = lookup<BISHOP_MAGICS>(sq, check.occ) & landingSquares;
                        if_resolves_check(plain_adder);
                        break;
                    case ROOK:
                        attk = lookup<ROOK_MAGICS>(sq, check.occ) & landingSquares;
                        if_resolves_check(plain_adder);
                        break;
                    case KING:
                        // ignore the king. it's handled down below
                        // it's not handled here because we need that code
                        // in the case of double checks, and this code only handles single checks.
                        break;
                    case PAWN: {
                        attk = pawn_attacks(SIDE, sq);

                        Bitboard enPassant = to_bitboard(pos.state.enPassantTarget); // will be 0 if enPassantTarget is NULL_SQUARE
                        if ((enPassant & attk) && pos.state.enPassantTarget != NULL_SQUARE) {
                            if (en_passant_is_legal<SIDE>(pos, sq)) {
                                // now check if this en passant resolves the check!
                                Bitboard testOcc = check.occ;
                                testOcc ^= to_bitboard(sq); // this piece has moved
                                testOcc |= enPassant; // move to the target square
                                testOcc ^= to_bitboard(pos.state.enPassantTarget + (SIDE == WHITE_SIDE ? Dir::S : Dir::N)); // other has been captured

                                if (get_lsb(check.all) + (SIDE == WHITE_SIDE ? Dir::N : Dir::S) == pos.state.enPassantTarget // we just captured the checker! we're out of check now
                                    || check_resolved(pos, testOcc, check.all, check.kingBB))
                                    ret.push_back(make_move<EN_PASSANT>(sq, pos.state.enPassantTarget));
                            }
                        }

                        auto add_and_consider_promote = [&](const Square _dstsq) {
                            if ((0 <= _dstsq && _dstsq <= 7) || (56 <= _dstsq && _dstsq <= 63)) {
                                for (PromoteType to : {PROMOTE_QUEEN, PROMOTE_BISHOP, PROMOTE_KNIGHT, PROMOTE_ROOK})
                                    ret.push_back(make_promotion(sq, _dstsq, to));
                            } else {
                                ret.push_back(make_normal(sq, _dstsq));
                            }
                        };

                        attk &= pos.by_side(opposite_side(SIDE));
                        
                        // add moves
                        Bitboard normMoves = PAWN_MOVES[SIDE][sq];

                        // hacky hack to prevent double advancing over a piece. TODO: Remove PAWN_MOVES table completely?
                        if (normMoves & check.occ) normMoves &= ~to_bitboard(sq + 2 * (SIDE == WHITE_SIDE ? Dir::N : Dir::S));
                        normMoves &= landingSquares;
                        normMoves &= ~check.occ; // pawn advances cannot capture pieces. we must do this since landingSquares includes an or of the checkers.

                        attk |= normMoves;
                        if_resolves_check(add_and_consider_promote);
                        break;
                    }
                    default:
                        UNDEFINED();
                    }
                }

            }

            attk = king_moves(check.kingSq) & ~underFire & ~pos.by_side(SIDE);

            Bitboard fromBb = check.kingBB;
            while (attk) {
                Square _dstsq = pop_lsb(attk);
                Bitboard testOcc = check.occ;
                Bitboard toBb = to_bitboard(_dstsq);

                testOcc ^= fromBb;
                testOcc |= toBb;
    
                // we need to update the king bitboard if the king is the one being moved
                // also, the king needs special handling for double checks!
                // thus, we use toBb instead of kingBB, and we entertain the possibility of
                // more than one checker.
                // see r4kQr/p1ppq1b1/bn4p1/4N3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQ - 0 3
                Bitboard checkersIter = check.all & ~to_bitboard(_dstsq); // remove the checker we capture
                while (checkersIter) {
                    if (!check_resolved_sq(pos, testOcc, toBb, pop_lsb(checkersIter)))
                        goto skipBcFail; // we are still in check! just return
                }

                ret.push_back(make_normal(check.kingSq, _dstsq));

                skipBcFail:;
            }

            // castling is illegal in check anyways; don't consider it
            return ret;
        }

        pos.isInCheck = false;
        while (iter) {
            Square sq = pop_lsb(iter);
            Type type = type_of(pos.pieces[sq]);

            Bitboard srcBb = to_bitboard(sq);

            auto plain_adder = [&](const Square to) {
                ret.push_back(make_normal(sq, to));
            };

            // allow_taking_pinner is used by the pawn code to add pawn advances without adding advances that CAPTURE PIECES! 
            // (fuck pawns, they're so bad)
            auto add_moves = [&](const std::function<void(const Square to)> &action) {
                if (srcBb & pinned) { // this piece is pinned. The king can never be pinned (otherwise we go to the check code)
                    // we MUST move onto a square that our pinner attacks or capture it
                    Bitboard pinnerBb = to_bitboard(pinnerOf[sq]);
                    attk &= pinLines; // step into a pin line (which includes pinners)
                    while (attk) {
                        Square candidateDst = pop_lsb(attk);
                        Bitboard testOcc = check.occ;
                        testOcc ^= srcBb; // unset the square we left
                        testOcc |= to_bitboard(candidateDst); // set the square we move to
                        if (candidateDst == pinnerOf[sq] || check_resolved(pos, testOcc, pinnerBb, check.kingBB)) {
                            action(candidateDst);
                        }
                    }
                } else {
                    while (attk) {
                        action(pop_lsb(attk));
                    }
                }
            };

            switch (type) {
            case KNIGHT:
                attk = knight_moves(sq) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case QUEEN:
                attk = (lookup<ROOK_MAGICS>(sq, check.occ) | lookup<BISHOP_MAGICS>(sq, check.occ)) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case BISHOP:
                attk = lookup<BISHOP_MAGICS>(sq, check.occ) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case ROOK:
                attk = lookup<ROOK_MAGICS>(sq, check.occ) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case KING: {
                attk = king_moves(sq) & ~pos.by_side(SIDE) & ~underFire;
                add_moves(plain_adder);

                static_assert(SIDE == WHITE_SIDE || SIDE == BLACK_SIDE);
                constexpr auto sideIndex = (SIDE == WHITE_SIDE ? 2 : 0);
                bool canKingside = pos.state.castlingRights & KINGSIDE_MASK << sideIndex;
                bool canQueenside = pos.state.castlingRights & QUEENSIDE_MASK << sideIndex;

                constexpr Bitboard checkMaskQueenside = CASTLING_ATTACK_MASKS[0 + sideIndex];
                constexpr Bitboard checkMaskKingside = CASTLING_ATTACK_MASKS[1 + sideIndex];
                constexpr Bitboard occMaskQueenside = CASTLING_OCCUPANCY_MASKS[0 + sideIndex];
                constexpr Bitboard occMaskKingside = CASTLING_OCCUPANCY_MASKS[1 + sideIndex];

                // these squares can't be attacked
                canKingside = canKingside && !(checkMaskKingside & underFire) && !(occMaskKingside & check.occ);
                canQueenside = canQueenside && !(checkMaskQueenside & underFire) && !(occMaskQueenside & check.occ);

                if (canKingside)
                    ret.push_back(make_move<CASTLE>(sq, sq + 2 * Dir::E));
                if (canQueenside)
                    ret.push_back(make_move<CASTLE>(sq, sq + 2 * Dir::W));

                break;
            }
            case PAWN: {
                attk = pawn_attacks(SIDE, sq);

                Bitboard enPassant = to_bitboard(pos.state.enPassantTarget); // will be 0 if enPassantTarget is NULL_SQUARE
                if ((enPassant & attk) && pos.state.enPassantTarget != NULL_SQUARE) {
                    if (en_passant_is_legal<SIDE>(pos, sq)) // en_passant_is_legal implicitly handles pinned pawns.
                        ret.push_back(make_move<EN_PASSANT>(sq, pos.state.enPassantTarget));
                }

                // TODO: is this lambda compiled/optimized efficiently?
                auto add_moves_check_promotions = [&](const Square dstSq) {
                    if ((0 <= dstSq && dstSq <= 7) || (56 <= dstSq && dstSq <= 63)) {
                        for (PromoteType to : {PROMOTE_QUEEN, PROMOTE_BISHOP, PROMOTE_KNIGHT, PROMOTE_ROOK})
                            ret.push_back(make_promotion(sq, dstSq, to));
                    } else {
                        ret.push_back(make_normal(sq, dstSq));
                    }
                };

                attk &= pos.by_side(opposite_side(SIDE));
                
                // add moves
                // TODO: Remove PAWN_MOVES table and calculate this properly
                Bitboard normMoves = PAWN_MOVES[SIDE][sq];
                if (normMoves & check.occ) normMoves &= ~to_bitboard(sq + 2 * (SIDE == WHITE_SIDE ? Dir::N : Dir::S)); // disallow double advance if single is blocked.
                normMoves &= ~check.occ;

                attk |= normMoves;
                add_moves(add_moves_check_promotions); // false -> do not allow_taking_pinner with a pawn advance
                break;
            }
            default:
                UNDEFINED();
            }
        }

        return ret;
    }

    template <Side SIDE>
    static constexpr inline uint64_t en_passant_is_legal(const Position &pos, const Square sq) {
        // here we need to consider if taking en passant will reveal a check
        // for example, in `3k4/8/8/1KPp3r/8/8/8/8 w - d6 0 1`,
        // the pawn taking en passant is not pinned, but doing so
        // would open the rook's attack, so it is illegal.

        Bitboard testOcc = pos.by_side(WHITE_SIDE) | pos.by_side(BLACK_SIDE);
        Bitboard kingBB = pos.by_side(SIDE) & pos.by_type(KING);
        Square kingSq = std::countr_zero(kingBB);

        // this piece has moved away, and the en passant target has been captured
        // enPassantTarget points to the square "behind" the pawn, so we have to add an offset.
        testOcc ^= to_bitboard(sq);
        // TODO: Do we have to set the bit at the enPassantTarget square? YES WE DO! This is important to calculate pinned en passanters.
        testOcc |= to_bitboard(pos.state.enPassantTarget);
        testOcc ^= to_bitboard(pos.state.enPassantTarget + (SIDE == WHITE_SIDE ? Dir::S : Dir::N));
        uint64_t illegal = false; // or should i use bool and ?1:0

        // here, possible pinners MUST be recalculated because
        // the en passant might remove a pawn that was previously preventing
        // another piece from being pinned
        // see 8/2p5/3p4/KP5r/1R3pPk/8/4P3/8 b - g3 0 1
        Bitboard pinnerIter = pos.by_side(opposite_side(SIDE));
        pinnerIter &= (pseudo_attacks<ROOK_MAGICS>(kingSq) & (pos.by_type(ROOK) | pos.by_type(QUEEN)))
                | (pseudo_attacks<BISHOP_MAGICS>(kingSq) & (pos.by_type(BISHOP) | pos.by_type(QUEEN)));
        // is this computation even worth the time it saves in the loop?

        while (!illegal && pinnerIter) {
            Square pinnerSq = pop_lsb(pinnerIter);

            // check if this piece can attack the king
            switch (type_of(pos.pieces[pinnerSq])) {
            case BISHOP:
                illegal |= (lookup<BISHOP_MAGICS>(pinnerSq, testOcc) & kingBB);
                break;
            case ROOK:
                illegal |= (lookup<ROOK_MAGICS>(pinnerSq, testOcc) & kingBB);
                break;
            case QUEEN:
                illegal |= ((lookup<ROOK_MAGICS>(pinnerSq, testOcc) | lookup<BISHOP_MAGICS>(pinnerSq, testOcc)) & kingBB);
                break;
            default:
                UNDEFINED();
            }
        }

        return !illegal;
    }

    template MoveList standard_moves<WHITE_SIDE>(Position &pos);
    template MoveList standard_moves<BLACK_SIDE>(Position &pos);
}
*/