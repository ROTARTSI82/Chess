#pragma once

#include "scacus/bitboard.hpp"

#include <functional>

namespace sc {
    extern Bitboard KNIGHT_MOVES[BOARD_SIZE];
    extern Bitboard PAWN_ATTACKS[NUM_SIDES][BOARD_SIZE];
    extern Bitboard PAWN_MOVES[NUM_SIDES][BOARD_SIZE];
    extern Bitboard KING_MOVES[BOARD_SIZE];

    // https://www.chessprogramming.org/Magic_Bitboards
    struct Magic {
        Bitboard *table = nullptr;
        Bitboard mask = 0;  // to mask relevant squares of both lines (no outer squares)
        Bitboard magic = 0; // magic 64-bit factor to multiply by
        uint8_t shift = 0; // shift right

        ~Magic() {
            delete[] table;
        }
    };

    extern Magic ROOK_MAGICS[BOARD_SIZE];
    extern Magic BISHOP_MAGICS[BOARD_SIZE];

    // TODO: Consider using _pext_u64?

    template <Magic *TABLE>
    constexpr inline int occupancy_to_index(const Square sq, const uint64_t occupancy) {
        auto &entry = TABLE[sq]; return ((entry.mask & occupancy) * entry.magic) >> entry.shift;
    }

    template <Magic *TABLE>
    constexpr inline uint64_t lookup(const Square square, const uint64_t occupancy) {
        return TABLE[square].table[occupancy_to_index<TABLE>(square, occupancy)];
    }

    // returns a bitboard of the squares attacked if the board were empty.
    template <Magic *TABLE>
    constexpr inline uint64_t pseudo_attacks(const Square sq) {
        // stockfish uses a separate table for this? would that be because of cache shananigans?
        return TABLE[sq].table[0]; 
    }

    void init_movegen();

    StateInfo make_move(Position &pos, const Move mov);

    typedef std::vector<Move> MoveList;


    template <Side SIDE>
    static constexpr inline uint64_t en_passant_is_legal(const Position &pos, const Bitboard possiblePinners, const Square sq);

    // checks if there is no check given by checker with the given occupancy bits
    static constexpr inline bool check_resolved(const Position &pos, const Bitboard occ, const Bitboard checker, const Bitboard kingBB) {
        Square sq = std::__countr_zero<uint64_t>(checker);
        switch (type_of(pos.pieces[sq])) {
        case QUEEN:
            return !((lookup<BISHOP_MAGICS>(sq, occ) | lookup<ROOK_MAGICS>(sq, occ)) & kingBB);
        case ROOK:
            return !(lookup<ROOK_MAGICS>(sq, occ) & kingBB);
        case BISHOP:
            return !(lookup<BISHOP_MAGICS>(sq, occ) & kingBB);
        default:
            throw std::runtime_error{"Non queen/rook/bishop checker in checked_resolved"};
        }
    }

    // Assumption: You can't en passant and promote at the same time.
    template <Side SIDE>
    MoveList standard_moves(const Position &pos) {
        MoveList ret;

        const Bitboard occ = pos.by_side(WHITE_SIDE) | pos.by_side(BLACK_SIDE);

        // now we generate a list of pinned pieces
        Bitboard kingBB = pos.by_side(SIDE) & pos.by_type(KING);
        Square king = std::countr_zero<uint64_t>(kingBB);
        // is seperating these into two passes of rooks/queens + bishops/queens worth it?

        // // mask of possible pieces that can be checking the king
        // // QUEEN_MOVES + BISHOP_MOVES covers pawns and opposing king
        // Bitboard checkersMask = lookup<ROOK_MAGICS>(king,  occ) | lookup<BISHOP_MAGICS>(king, occ) | KNIGHT_MOVES[king];
        // // is computing the checkers mask even worth it? Would just looping through all opponent pieces be faster?
        Bitboard opposingIter = pos.by_side(opposite_side(SIDE));
        Bitboard checkers = 0;
        Bitboard checkerAttk = 0; // squares attacked by checker (set to the last checker found)
        Bitboard underFire = 0; // squares attacked by other side. pinned pieces and king are considered to attack normal squares.
        while (opposingIter) {
            Square sq = pop_lsb(opposingIter);
            Type type = type_of(pos.pieces[sq]);
            Bitboard tmpAtk = 0;

            auto add_other_side_threats = [&]() {
                if (tmpAtk & kingBB) {
                    checkerAttk = tmpAtk;
                    checkers |= to_bitboard(sq);
                }
                underFire |= tmpAtk;
            };

            switch (type) {
            case PAWN:
                tmpAtk = PAWN_ATTACKS[opposite_side(SIDE)][sq];
                add_other_side_threats();
                break;
            case KING:
                tmpAtk = KING_MOVES[sq];
                add_other_side_threats();
                break;
            case QUEEN:
                tmpAtk = lookup<BISHOP_MAGICS>(sq, occ) | lookup<ROOK_MAGICS>(sq, occ);
                add_other_side_threats();
                break;
            case BISHOP:
                tmpAtk = lookup<BISHOP_MAGICS>(sq, occ);
                add_other_side_threats();
                break;
            case ROOK:
                tmpAtk = lookup<ROOK_MAGICS>(sq, occ);
                add_other_side_threats();
                break;
            case KNIGHT:
                tmpAtk = KNIGHT_MOVES[sq];
                add_other_side_threats();
                break;
            default:
                dbg_dump_position(pos);
                throw std::runtime_error{"Bitboards desynced from piece type array"};
            }
        }

        opposingIter = pos.by_side(opposite_side(SIDE));
        Bitboard possiblePinners = opposingIter & (pos.by_type(QUEEN) | pos.by_type(ROOK) | pos.by_type(BISHOP));
        // blocked by opposing pieces, ignoring own pieces.
        Bitboard pinLines = lookup<ROOK_MAGICS>(king, opposingIter) | lookup<BISHOP_MAGICS>(king, opposingIter);
        possiblePinners &= pinLines;
        possiblePinners &= ~checkers; // cannot be already giving check!
        Bitboard maybePinned = pos.by_side(SIDE) & pinLines;
        Bitboard maybePinnedIter = maybePinned;
        Bitboard pinned = 0;

        struct {
            Bitboard attacks;
            Square pinner;
        } pinInfo[64];

        while (maybePinnedIter) {
            Square sq = pop_lsb(maybePinnedIter);
            Bitboard bb = to_bitboard(sq);

            // test if removing this piece will reveal an attack on the king
            maybePinned ^= bb; // remove this piece from the occupancy

            Bitboard pinnerIter = possiblePinners;
            bool done = false;
            while (pinnerIter && !done) {
                Square pinnerSq = pop_lsb(pinnerIter);
                Bitboard pinnerAttk = 0;

                auto update_pin_info = [&]() {
                    if (pinnerAttk & kingBB) {
                        pinned |= bb;
                        done = true;
                        pinInfo[sq].pinner = pinnerSq;
                        pinInfo[sq].attacks = pinnerAttk;
                    }
                };

                switch (type_of(pos.pieces[pinnerSq])) {
                case BISHOP:
                    pinnerAttk = lookup<BISHOP_MAGICS>(pinnerSq, maybePinned);
                    update_pin_info();
                    break;
                case ROOK:
                    pinnerAttk = lookup<ROOK_MAGICS>(pinnerSq, maybePinned) & kingBB;
                    update_pin_info();
                    break;
                case QUEEN:
                    pinnerAttk = (lookup<ROOK_MAGICS>(pinnerSq, maybePinned) | lookup<BISHOP_MAGICS>(pinnerSq, maybePinned));
                    update_pin_info();
                    break;
                default:
                    throw std::runtime_error{"Non bishop/rook/queene pinner?"};
                }
            }

            maybePinned ^= bb; // add it back
        }

        Bitboard iter = pos.by_side(SIDE); // & ~pinned
        Bitboard attk = 0;

        if (checkers) { // i.e: in check
            // std::cout << "CHECKERS\n";
            // print_bb(checkers);
            if (popcnt(checkers) == 1) { // only 1 checker. we can block by moving a piece.


                iter &= ~pinned; // pinned pieces cannot be moved while in check, i hope! TODO: find if this assumption is correct

                // taking to checker, or squares the checker attacks (so that we might block the check)
                const Bitboard landingSquares = checkers | (checkerAttk & ~occ);

                while (iter) {
                    Square sq = pop_lsb(iter);

                    auto if_resolves_check = [&](const std::function<void(const Square)> &action) {
                        while (attk) {
                            Square _dstsq = pop_lsb(attk);

                            /* in this case we capture the piece giving check! */
                            if (to_bitboard(_dstsq) & checkers) {
                                action(_dstsq);
                            } else {
                                Bitboard testOcc = occ;
                                testOcc ^= to_bitboard(sq);
                                testOcc |= to_bitboard(_dstsq);
                                if (check_resolved(pos, testOcc, checkers, kingBB)) {
                                    action(_dstsq);
                                }
                            }
                        }
                    };


                    switch (type_of(pos.pieces[sq])) {
                    case KNIGHT:
                        attk = KNIGHT_MOVES[sq] & landingSquares;
                        if_resolves_check([&](const Square _dstsq) { ret.push_back(make_normal(sq, _dstsq)); });
                        break;
                    case QUEEN:
                        attk = (lookup<ROOK_MAGICS>(sq, occ) | lookup<BISHOP_MAGICS>(sq, occ)) & landingSquares;
                        if_resolves_check([&](const Square _dstsq) { ret.push_back(make_normal(sq, _dstsq)); });
                        break;
                    case BISHOP:
                        attk = lookup<BISHOP_MAGICS>(sq, occ) & landingSquares;
                        if_resolves_check([&](const Square _dstsq) { ret.push_back(make_normal(sq, _dstsq)); });
                        break;
                    case ROOK:
                        attk = lookup<ROOK_MAGICS>(sq, occ) & landingSquares;
                        if_resolves_check([&](const Square _dstsq) { ret.push_back(make_normal(sq, _dstsq)); });
                        break;
                    case KING:
                        // ignore the king. it's handled down below
                        break;
                    case PAWN: {
                        attk = PAWN_ATTACKS[SIDE][sq];

                        Bitboard enPassant = to_bitboard(pos.state.enPassantTarget); // will be 0 if enPassantTarget is NULL_SQUARE
                        if (enPassant & attk) {
                            if (en_passant_is_legal<SIDE>(pos, possiblePinners, sq)) {
                                // now check if this en passant resolves the check!
                                Bitboard testOcc = occ;
                                testOcc ^= to_bitboard(sq); // this piece has moved
                                testOcc |= enPassant; // move to the target square
                                testOcc ^= to_bitboard(pos.state.enPassantTarget + (SIDE == WHITE_SIDE ? Dir::S : Dir::N)); // other has been captured

                                if (check_resolved(pos, testOcc, checkers, kingBB))
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
                        if_resolves_check(add_and_consider_promote);
                        
                        // add moves
                        attk = PAWN_MOVES[SIDE][sq];

                        // hacky hack to prevent double advancing over a piece. TODO: Remove PAWN_MOVES table completely?
                        if (attk & occ) attk &= ~to_bitboard(sq + 2 * (SIDE == WHITE_SIDE ? Dir::N : Dir::S));
                        attk &= landingSquares;
                        attk &= ~occ; // pawn advances cannot capture pieces. we must do this since landingSquares includes an or of the checkers.

                        if_resolves_check(add_and_consider_promote);
                        break;
                    }
                    default:
                        throw std::runtime_error{"Illegal null piece on bitboard?"};
                    }
                }

            }

            attk = KING_MOVES[king] & ~underFire & ~pos.by_side(SIDE);
            while (attk) ret.push_back(make_normal(king, pop_lsb(attk)));
            // castling is illegal in check anyways; don't consider it
            return ret;
        }

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
                if (srcBb & pinned) { // this piece is pinned
                    // we MUST move onto a square that our pinner attacks or capture it
                    Bitboard pinnerBb = to_bitboard(pinInfo[sq].pinner);
                    attk &= (pinInfo[sq].attacks | pinnerBb);
                    while (attk) {
                        Square candidateDst = pop_lsb(attk);
                        Bitboard testOcc = occ;
                        testOcc ^= srcBb; // unset the square we left
                        testOcc |= to_bitboard(candidateDst); // set the square we move to
                        if (candidateDst == pinInfo[sq].pinner || check_resolved(pos, testOcc, pinnerBb, kingBB)) {
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
                attk = KNIGHT_MOVES[sq] & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case QUEEN:
                attk = (lookup<ROOK_MAGICS>(sq, occ) | lookup<BISHOP_MAGICS>(sq, occ)) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case BISHOP:
                attk = lookup<BISHOP_MAGICS>(sq, occ) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case ROOK:
                attk = lookup<ROOK_MAGICS>(sq, occ) & ~pos.by_side(SIDE);
                add_moves(plain_adder);
                break;
            case KING: {
                attk = KING_MOVES[sq] & ~pos.by_side(SIDE) & ~underFire;
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
                canKingside = canKingside && !(checkMaskKingside & underFire) && !(occMaskKingside & occ);
                canQueenside = canQueenside && !(checkMaskQueenside & underFire) && !(occMaskQueenside & occ);

                if (canKingside)
                    ret.push_back(make_move<CASTLE>(sq, sq + 2 * Dir::E));
                if (canQueenside)
                    ret.push_back(make_move<CASTLE>(sq, sq + 2 * Dir::W));

                break;
            }
            case PAWN: {
                attk = PAWN_ATTACKS[SIDE][sq];

                Bitboard enPassant = to_bitboard(pos.state.enPassantTarget); // will be 0 if enPassantTarget is NULL_SQUARE
                if (enPassant & attk) {
                    if (en_passant_is_legal<SIDE>(pos, possiblePinners, sq)) // en_passant_is_legal implicitly handles pinned pawns.
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
                add_moves(add_moves_check_promotions);
                
                // add moves
                // TODO: Remove PAWN_MOVES table and calculate this properly
                attk = PAWN_MOVES[SIDE][sq];
                if (attk & occ) attk &= ~to_bitboard(sq + 2 * (SIDE == WHITE_SIDE ? Dir::N : Dir::S)); // disallow double advance if single is blocked.
                attk &= ~occ;

                add_moves(add_moves_check_promotions); // false -> do not allow_taking_pinner with a pawn advance
                break;
            }
            default:
                throw std::runtime_error{"Illegal null piece on bitboard?"};
            }
        }

        return ret;
    }

    template <Side SIDE>
    static constexpr inline uint64_t en_passant_is_legal(const Position &pos, const Bitboard possiblePinners, const Square sq) {
        // here we need to consider if taking en passant will reveal a check
        // for example, in `3k4/8/8/1KPp3r/8/8/8/8 w - d6 0 1`,
        // the pawn taking en passant is not pinned, but doing so
        // would open the rook's attack, so it is illegal.

        Bitboard testOcc = pos.by_side(WHITE_SIDE) | pos.by_side(BLACK_SIDE);
        Bitboard kingBB = pos.by_side(SIDE) & pos.by_type(KING);
        // this piece has moved away, and the en passant target has been captured
        // enPassantTarget points to the square "behind" the pawn, so we have to add an offset.
        testOcc ^= to_bitboard(sq);
        // TODO: Do we have to set the bit at the enPassantTarget square? YES WE DO! This is important to calculate pinned en passanters.
        testOcc |= to_bitboard(pos.state.enPassantTarget);
        testOcc ^= to_bitboard(pos.state.enPassantTarget + (SIDE == WHITE_SIDE ? Dir::S : Dir::N));
        uint64_t illegal = false; // or should i use bool and ?1:0
        Bitboard pinnerIter = possiblePinners;
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
                throw std::runtime_error{"non bishop/rook/queen pinner for en passant?"};
            }
        }

        return !illegal;
    }
}