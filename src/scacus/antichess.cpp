#include "scacus/antichess.hpp"

#include <thread>
#include <mutex>
#include <atomic>

namespace sc {
    // Assumption: You can't en passant and promote at the same time.
    template <Side SIDE>
    MoveList antichess_moves(const Position &pos) {
        MoveList normalMoves(0);
        MoveList captures(0);

        const Bitboard occ = pos.by_side(WHITE_SIDE) | pos.by_side(BLACK_SIDE);
        const Bitboard capturePos = pos.by_side(opposite_side(SIDE));
        const Bitboard mySide = pos.by_side(SIDE);

        Bitboard iter = pos.by_side(SIDE);

        while (iter) {
            Square sq = pop_lsb(iter);
            Type type = type_of(pos.pieces[sq]);
            Bitboard tmpAtk = 0;

            auto add_attacks = [&]() {
                while (tmpAtk) {
                    Square atkSq = pop_lsb(tmpAtk);
                    Bitboard bb = to_bitboard(atkSq);
                    ((bb & capturePos) ? captures : normalMoves)
                        .push_back(make_normal(sq, atkSq));
                }
            };

            auto add_pawn_movs_to = [&](MoveList &addTo) {
                while (tmpAtk) {
                    Square atkSq = pop_lsb(tmpAtk);

                    if ((0 <= atkSq && atkSq <= 7) || (56 <= atkSq && atkSq <= 63)) {
                        for (PromoteType to : {PROMOTE_QUEEN, PROMOTE_KNIGHT, PROMOTE_BISHOP, PROMOTE_ROOK})
                            addTo.push_back(make_promotion(sq, atkSq, to));

                        // in antichess, you are also allowed to promote to king, and you aren't
                        // allowed to castle. Thus, promotions to king are encoded with a move type of castling
                        addTo.push_back(make_move<CASTLE>(sq, atkSq));
                    } else {
                        addTo.push_back(make_normal(sq, atkSq));
                    }
                }
            };

            switch (type) {
            case PAWN:
                tmpAtk = pawn_attacks<SIDE>(sq);
                if ((tmpAtk & to_bitboard(pos.state.enPassantTarget)) && pos.state.enPassantTarget != NULL_SQUARE) {
                    captures.push_back(make_move<EN_PASSANT>(sq, pos.state.enPassantTarget));
                }

                tmpAtk &= capturePos;
                add_pawn_movs_to(captures);

                // TODO: Remove PAWN_MOVES table and calculate this properly
                tmpAtk = PAWN_MOVES[SIDE][sq];
                if (tmpAtk & occ) tmpAtk &= ~to_bitboard(sq + 2 * (SIDE == WHITE_SIDE ? Dir::N : Dir::S)); // disallow double advance if single is blocked.
                tmpAtk &= ~occ;
                add_pawn_movs_to(normalMoves);
                break;
            case KING:
                tmpAtk = king_moves(sq) & ~mySide;
                add_attacks();
                break;
            case QUEEN:
                tmpAtk = (lookup<BISHOP_MAGICS>(sq, occ) | lookup<ROOK_MAGICS>(sq, occ)) & ~mySide;
                add_attacks();
                break;
            case BISHOP:
                tmpAtk = lookup<BISHOP_MAGICS>(sq, occ) & ~mySide;
                add_attacks();
                break;
            case ROOK:
                tmpAtk = lookup<ROOK_MAGICS>(sq, occ) & ~mySide;
                add_attacks();
                break;
            case KNIGHT:
                tmpAtk = knight_moves(sq) & ~mySide;
                add_attacks();
                break;
            default:
                UNDEFINED();
            }
        }

        return std::move(captures.empty() ? normalMoves : captures);
    }

    template MoveList antichess_moves<WHITE_SIDE>(const Position &pos);
    template MoveList antichess_moves<BLACK_SIDE>(const Position &pos);

    StateInfo antichess_make_move(Position &pos, Move mov) {
        if (mov.typeFlags == CASTLE) {
            StateInfo ret = pos.state;

            if (pos.turn == BLACK_SIDE) pos.fullmoves++;
            pos.state.halfmoves++;

            if (pos.pieces[mov.dst] != NULL_COLORED_TYPE) pos.state.halfmoves = 0;

            pos.state.enPassantTarget = NULL_SQUARE;

            pos.state.capturedPiece = pos.pieces[mov.dst];
            pos.clear(mov.dst);
            pos.clear(mov.src);
            pos.set(mov.dst, KING, pos.turn);

            pos.turn = opposite_side(pos.turn);

            return ret;
        } else {
            return make_move(pos, mov);
        }
    }

    void antichess_unmake_move(Position &pos, const StateInfo &info, Move mov) {
        if (mov.typeFlags == CASTLE) {
            pos.turn = opposite_side(pos.turn);
            if (pos.turn == BLACK_SIDE) pos.fullmoves--;

            pos.set(mov.src, PAWN, pos.turn);
            pos.clear(mov.dst);
            if (pos.state.capturedPiece != NULL_COLORED_TYPE)
                pos.set(mov.dst, type_of(pos.state.capturedPiece), side_of(pos.state.capturedPiece));

            pos.state = info;
        } else {
            unmake_move(pos, info, mov);
        }
    }




    // We can't actually use min because -min is not max! In fact, -min is negative! 
//    constexpr auto NEARLY_MIN = std::numeric_limits<int>::min() + 4;
    constexpr auto ACTUAL_MIN = std::numeric_limits<int>::min() + 2;
    constexpr auto ACTUAL_MAX = std::numeric_limits<int>::max() - 2;

    static inline int antichess_eval(Position &pos, const MoveList &legalMoves) {
        if (legalMoves.empty()) return ACTUAL_MAX;

        auto count = [&](const Type t) {
            return popcnt(pos.by_side(opposite_side(pos.turn)) & pos.by_type(t)) - popcnt(pos.by_side(pos.turn) & pos.by_type(t));
        };

        auto opposingMoves = pos.turn == WHITE_SIDE ? antichess_moves<BLACK_SIDE>(pos) : antichess_moves<WHITE_SIDE>(pos);

        return (opposingMoves.size() - legalMoves.size()) + 100 * count(PAWN) + 300 * count(BISHOP) + 300 * count(KNIGHT) + 500 * count(ROOK) + 900 * count(QUEEN) + 200 * count(KING);
    }

    static inline int anti_quiescence_search(Position &pos, int alpha, int beta) {
        auto legalMoves = pos.turn == WHITE_SIDE ? antichess_moves<WHITE_SIDE>(pos) : antichess_moves<BLACK_SIDE>(pos);
        int stand_pat = antichess_eval(pos, legalMoves);
        if (stand_pat >= beta)
            return stand_pat;
        alpha = std::max(stand_pat, alpha);

        for (const auto mov : legalMoves) {
            if ((pos.by_side(opposite_side(pos.turn)) & to_bitboard(mov.dst)) || mov.typeFlags == EN_PASSANT) {
                auto undo = sc::antichess_make_move(pos, mov);
                int score = -anti_quiescence_search(pos, -beta, -alpha);
                sc::antichess_unmake_move(pos, undo, mov);

                if (score >= beta)
                    return score;
                alpha = std::max(alpha, score);
            }
        }

        return alpha;
    }

    static inline int antichess_eval(Position &pos, int alpha, int beta, int depth) {

        if (depth <= 0) {
            return anti_quiescence_search(pos, alpha, beta);
        }

        auto legalMoves = pos.turn == WHITE_SIDE ? antichess_moves<WHITE_SIDE>(pos) : antichess_moves<BLACK_SIDE>(pos);
        if (legalMoves.empty())
            return ACTUAL_MAX;

        int value = ACTUAL_MIN;
        for (const auto mov : legalMoves) {
            auto undoInfo = sc::antichess_make_move(pos, mov);
            auto result = -antichess_eval(pos, -beta, -alpha, depth - 1);
            sc::antichess_unmake_move(pos, undoInfo, mov);

            value = std::max(value, result);
            if (value >= beta)
                return value;
            
            alpha = std::max(value, alpha);
        }
        
        return value;
    }

    #define MULTITHREAD 1

    std::pair<Move, int> antichess_search(Position &pos, int depth) {
        auto legalMoves = pos.turn == WHITE_SIDE ? antichess_moves<WHITE_SIDE>(pos) : antichess_moves<BLACK_SIDE>(pos);
        // std::cout << pos.get_fen() << '\n';
        // for (const auto m : legalMoves)
        //     std::cout << '\t' << m.long_alg_notation();
        // std::cout << '\n';
        // return std::make_pair(rand_of(legalMoves), 0);

        #if MULTITHREAD
            std::thread *workers = new std::thread[legalMoves.size()];
        #endif
        std::mutex mtx;
        std::atomic_int value;
        std::atomic_int alpha;
        value.store(ACTUAL_MIN);
        alpha.store(ACTUAL_MIN);

        auto currentMov = make_normal(0, 0);
        for (unsigned i = 0; i < legalMoves.size(); i++) {
            auto do_work = [&, mov{legalMoves.at(i)}, cpy{pos}]() mutable {
                auto undoInfo = sc::antichess_make_move(cpy, mov);
                int result = -antichess_eval(cpy, ACTUAL_MIN, -alpha, depth - 1);
                sc::antichess_unmake_move(cpy, undoInfo, mov);

                std::lock_guard<std::mutex> lg(mtx);
                value = std::max(value.load(), result);
                if (value > alpha) {
                    currentMov = mov;
                    alpha.store(value);
                }
            };

        #if MULTITHREAD
            workers[i] = std::thread(do_work);
        #else
            do_work();
        #endif
        }

    #if MULTITHREAD
        for (unsigned i = 0; i < legalMoves.size(); i++)
            workers[i].join();
        delete[] workers;
    #endif
        
        return std::make_pair(currentMov, value.load());
    }
}

