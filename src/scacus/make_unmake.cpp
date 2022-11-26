#include "scacus/movegen.hpp"

namespace sc::makeimpl {
    using namespace sc;

    struct PositionFriend {
        inline static constexpr std::pair<Square, Square> castle_info(const Move mov) {
            Square targetRook = 0, rookNewDst = 0;
            if ((int) mov.dst - (int) mov.src > 0) { // kingside castling
                targetRook = mov.src + 3 * Dir::E;
                rookNewDst = mov.dst + Dir::W;
            } else {
                rookNewDst = mov.dst + Dir::E;
                targetRook = mov.src + 4 * Dir::W;
            }
            return std::make_pair(targetRook, rookNewDst);
        }

        inline static constexpr void forbid_castling(Position *pos) {
            CastlingRights kingside = KINGSIDE_MASK;
            CastlingRights queenside = QUEENSIDE_MASK;
            int zobBase = 0;

            if (pos->turn == WHITE_SIDE) {
                kingside <<= 2;
                queenside <<= 2;
                zobBase = 2;
            }

            if (pos->state.castlingRights & kingside) {
                pos->state.hash ^= zob_CastlingRights[zobBase + 1];
            }
            if (pos->state.castlingRights & queenside) {
                pos->state.hash ^= zob_CastlingRights[zobBase];
            }

            pos->state.castlingRights &= ~(kingside | queenside);
        }

        inline static constexpr void remove_castling_rights(Position *pos, const Square rookSq, Side side) {
            const auto file = file_ind_of(rookSq);
            const auto rank = rank_ind_of(rookSq);

            // this rook is not on a square of a castling rook, so it does not affect castling rights.
            if (file != 0 && file != 7) return;

            int zobIndex = file == 0 ? 0 // queenside (1)
                                     : 1; // kingside (2)

            if (side == WHITE_SIDE) {
                if (rank != 0) return;
                zobIndex += 2;
            } else {
                if (rank != 7) return;
            }

            CastlingRights mask = 1 << zobIndex;
            // if we have the right (i.e. we actually unset it), flip the hash
            if (pos->state.castlingRights & mask) {
                pos->state.hash ^= zob_CastlingRights[zobIndex];
                pos->state.castlingRights &= ~mask;
            }
        }
    };
}

namespace sc {
    using namespace makeimpl;

    void make_move(Position &pos, const Move mov, StateInfo *ret) {
        // make a copy of the current state
        // the current state will be updated into oblivion.
//        StateInfo *ret = new StateInfo{*pos.state};
        *ret = pos.state;

        if (pos.turn == BLACK_SIDE) pos.fullmoves++;
        pos.state.halfmoves++;

        if (pos.pieces[mov.dst] != NULL_COLORED_TYPE) pos.state.halfmoves = 0;

        if (pos.state.enPassantTarget != NULL_SQUARE)
            pos.state.hash ^= zob_EnPassantFile[file_ind_of(pos.state.enPassantTarget)];

        pos.state.enPassantTarget = NULL_SQUARE;

        switch (mov.typeFlags) {
            case NORMAL: {
                pos.state.capturedPiece = pos.pieces[mov.dst];

                Type movedType = type_of(pos.pieces[mov.src]);
                pos.clear(mov.dst);
                pos.set(mov.dst, movedType, pos.turn);
                pos.clear(mov.src);

                switch (movedType) {
                    case PAWN:
                        pos.state.halfmoves = 0;
                        if (std::abs((int) mov.dst - (int) mov.src) == Dir::N * 2) {
                            pos.state.enPassantTarget = mov.dst + (pos.turn == WHITE_SIDE ? Dir::S : Dir::N);
                            pos.state.hash ^= zob_EnPassantFile[file_ind_of(pos.state.enPassantTarget)];
                        }
                        break;
                    case KING:
                        PositionFriend::forbid_castling(&pos);
                        break;
                    case ROOK: {
                        PositionFriend::remove_castling_rights(&pos, mov.src, pos.turn);
                        break;
                    }
                    default:
                        ; // no special logic needed in most cases
                }
                break;
            }
            case CASTLE: {
                pos.set(mov.dst, KING, pos.turn);
                pos.clear(mov.src);

                auto [targetRook, rookNewDst] = PositionFriend::castle_info(mov);

                pos.clear(targetRook);
                pos.set(rookNewDst, ROOK, pos.turn);

                PositionFriend::forbid_castling(&pos);
                break;
            }
            case EN_PASSANT: {
                // use enPassantTarget from ret: pos.state.enPassant target has already been set to null.
                Square capturedPawn = ret->enPassantTarget + (pos.turn == WHITE_SIDE ? Dir::S : Dir::N);
                pos.state.capturedPiece = pos.pieces[capturedPawn];
                pos.clear(capturedPawn);
                pos.clear(mov.src);
                pos.set(mov.dst, PAWN, pos.turn);
                break;
            }
            case PROMOTION:
                pos.state.capturedPiece = pos.pieces[mov.dst];
                pos.clear(mov.dst);
                pos.clear(mov.src);
                pos.set(mov.dst, static_cast<Type>((int) mov.promote + 2), pos.turn);
                break;
            default:
                UNDEFINED();
        }

        if (type_of(pos.state.capturedPiece) == ROOK)
            PositionFriend::remove_castling_rights(&pos, mov.dst, side_of(pos.state.capturedPiece));

        pos.turn = opposite_side(pos.turn);
        pos.state.hash ^= zob_IsWhiteTurn; // no need to reset because it is stored in state!
        pos.isInCheck = false;

        pos.state.prev = ret;
        pos.state.prevMove = mov;

        // advance by two each time because we can't match something on the turn of the opponent
        StateInfo *it = ret->prev;
        
        // we expect halfmoves to decrease on each step we take back.
        // if it doesn't, then state was irreversibly changed
        // so no repetition was possible before that point.
        auto halfmoves = pos.state.halfmoves;

        while (it != nullptr) {
            if (it->hash == pos.state.hash) {
                pos.state.reps = it->reps + 1;
                break;
            }

            if (halfmoves <= it->halfmoves)
                break; // no repetition possible

            halfmoves = it->halfmoves;
            it = it->prev ? it->prev->prev : nullptr;
        }
    }

    void unmake_move(Position &pos, const Move mov) {
        pos.turn = opposite_side(pos.turn);
        if (pos.turn == BLACK_SIDE) pos.fullmoves--;

        switch (mov.typeFlags) {
            case NORMAL:
                pos.set(mov.src, type_of(pos.pieces[mov.dst]), pos.turn);
                pos.clear(mov.dst);
                if (pos.state.capturedPiece != NULL_COLORED_TYPE)
                    pos.set(mov.dst, type_of(pos.state.capturedPiece), side_of(pos.state.capturedPiece));
                break;
            case PROMOTION:
                pos.set(mov.src, PAWN, pos.turn);
                pos.clear(mov.dst);
                if (pos.state.capturedPiece != NULL_COLORED_TYPE)
                    pos.set(mov.dst, type_of(pos.state.capturedPiece), side_of(pos.state.capturedPiece));
                break;
            case EN_PASSANT: {
                Square captureSq = pos.state.prev->enPassantTarget + (pos.turn == WHITE_SIDE ? Dir::S : Dir::N);
                pos.set(captureSq, PAWN, opposite_side(pos.turn));
                pos.clear(mov.dst);
                pos.set(mov.src, PAWN, pos.turn);
                break;
            }
            case CASTLE: {
                pos.set(mov.src, KING, pos.turn);
                pos.clear(mov.dst);

                auto [targetRook, rookNewDst] = PositionFriend::castle_info(mov);

                pos.clear(rookNewDst);
                pos.set(targetRook, ROOK, pos.turn);
                break;
            }
            default:
                UNDEFINED();
        }

        pos.isInCheck = false;

//        StateInfo *toDelete = pos.state;
        pos.state = *pos.state.prev; // resets the hash too!
//        delete toDelete;
    }
}
