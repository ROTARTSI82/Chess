#include "scacus/engine.hpp"
#include <algorithm>

#define ENABLE_TT

namespace sc {

    // We can't actually use min because -min is not max! In fact, -min is negative! 
    constexpr auto NEARLY_MIN = -10000 * 100;
    constexpr auto ACTUAL_MIN = std::numeric_limits<int>::min() + 2;

    constexpr auto ACTUAL_MAX = std::numeric_limits<int>::max() - 2;

    class Searcher {
    public:
        Position *pos = nullptr;
        int startDepth = 0;
        int depth = 0; // current depth number (goes negative in quiescence_search())

        int quiescenceDepthReached = 0;
        int overrides = 0;

        DefaultEngine *eng;

        inline int eval(const MoveList &legalMoves) {
            if (legalMoves.empty())
                return pos->isInCheck ? NEARLY_MIN - depth*1000 : 0;
            if (pos->state.halfmoves >= 50 /* || pos->threefoldTable[pos->state.hash] >= 3 */ ) return 0;

            auto count = [&](const Type t) {
                return popcnt(pos->by_side(pos->turn) & pos->by_type(t)) - popcnt(pos->by_side(opposite_side(pos->turn)) & pos->by_type(t));
            };

            MoveList opposingMoves(0);
            if (pos->turn == WHITE_SIDE) standard_moves<BLACK_SIDE>(opposingMoves, *pos);
            else standard_moves<WHITE_SIDE>(opposingMoves, *pos);

            int ret = legalMoves.size() - 3*opposingMoves.size() + 108 * count(PAWN) + 305 * count(BISHOP)
                    + 300 * count(KNIGHT) + 500 * count(ROOK) + 900 * count(QUEEN);

            return ret + 2*popcnt(pos->by_side(pos->turn)) - popcnt(pos->by_side(opposite_side(pos->turn)));
        }

        inline int quiescence_search(int alpha, int beta) {
            MoveList legalMoves(0);
            legal_moves_from(legalMoves, *pos);

            int stand_pat = eval(legalMoves);
            if (stand_pat >= beta) {
                return stand_pat; // beta?
            }

            alpha = std::max(stand_pat, alpha);

//            if (!eng->continueSearch || pos->threefoldTable[pos->state.hash] >= 3)
//                return alpha;

            for (const auto mov : legalMoves) {
                if ((pos->by_side(opposite_side(pos->turn)) & to_bitboard(mov.dst)) || mov.typeFlags == EN_PASSANT) {
                    auto undo = sc::make_move(*pos, mov);

                    depth--;
                    int score = -quiescence_search(-beta, -alpha);
                    depth++;

                    sc::unmake_move(*pos, undo, mov);

                    if (score >= beta)
                        return score;
                    alpha = std::max(alpha, score);
                }
            }

            quiescenceDepthReached = std::min(quiescenceDepthReached, depth);
            return alpha;
        }

        inline int primitive_eval(int alpha, int beta) {

            if (depth <= 0 || !eng->continueSearch) {
                return quiescence_search(alpha, beta);
            }

            MoveList legalMoves(0);
            legal_moves_from(legalMoves, *pos);
            if (legalMoves.empty())
                return pos->isInCheck ? NEARLY_MIN - depth*1000 : 0;
            if (pos->state.halfmoves >= 50 /* || pos->threefoldTable[pos->state.hash] >= 3 */) {
                return 0;
            }

#ifdef ENABLE_TT
            std::unique_lock<std::mutex> lg(eng->ttMtx);
            if (eng->tt.count(pos->state.hash) > 0) {
                Transposition &val = eng->tt.at(pos->state.hash);
                eng->ttHits++;
                if (val.depth > depth) {
                    overrides++;
                    return val.score;
                }
                eng->order_moves(legalMoves, val.bestMove);
                // alpha = std::max(alpha, val.alpha);
                // beta = std::min(beta, val.beta);
                lg.unlock();
            } else
#endif
            {
#ifdef ENABLE_TT
                lg.unlock();
#endif
                eng->order_moves(legalMoves);
            }

            int value = ACTUAL_MIN;
            Move bestMove;
            for (const auto mov : legalMoves) {
                auto undoInfo = sc::make_move(*pos, mov);
                ColoredType captured = pos->state.capturedPiece;

                depth--;
                auto result = -primitive_eval(-beta, -alpha);
                depth++;

                sc::unmake_move(*pos, undoInfo, mov);

                if (result > value) {
                    bestMove = mov;
                    value = result;
                }

                if (value >= beta) {
                    if (captured != NULL_COLORED_TYPE)
                        eng->history[pos->turn][mov.src][mov.dst] = (startDepth-depth) * (startDepth-depth);
                    return value;
                }
                
                alpha = std::max(value, alpha);
            }

            constexpr auto alphaWindow = 450;
            constexpr auto betaWindow = 450;

#ifdef ENABLE_TT
            Transposition ins;
            ins.depth = depth;
            ins.score = value;
            ins.bestMove = bestMove;
            // ins.alpha = alpha - alphaWindow;
            // ins.beta = beta + betaWindow;

            std::lock_guard<std::mutex> lg2(eng->ttMtx);
            if (eng->tt.count(pos->state.hash) == 0 || eng->tt[pos->state.hash].depth < depth)
                eng->tt[pos->state.hash] = ins;
#endif
            return value;
        }

    };

    void DefaultEngine::start_search(int maxDepth) {
        continueSearch = true;
        MoveList legalMoves(0);
        legal_moves_from(legalMoves, *pos);
        order_moves(legalMoves);

        workers = new std::thread[legalMoves.size()];
        numWorkers = (int) legalMoves.size();
        ttHits = 0;

        runningAlpha.store(ACTUAL_MIN);
        evaluation = ACTUAL_MIN;
        worstEval = ACTUAL_MAX;
//        tt.clear();

        constexpr int alphaWindow = 450; // 0.5 queen

        for (std::size_t i = 0; i < legalMoves.size(); i++) {
            auto do_work = [&, mov{legalMoves.at(i)}, cpy{*pos}]() mutable {
                Searcher search;
                search.depth = 1;
                search.pos = &cpy;
                search.eng = this;

                int result = ACTUAL_MIN;
                while (continueSearch && search.depth < maxDepth) {
                    search.startDepth = search.depth;
                    search.quiescenceDepthReached = maxDepth + 10;

                    auto undoInfo = sc::make_move(cpy, mov);
                    int intermedRes = -search.primitive_eval(ACTUAL_MIN, std::numeric_limits<int>::max()); // -runningAlpha.load());
                    sc::unmake_move(cpy, undoInfo, mov);

                    std::lock_guard<std::mutex> lg(mtx);
                    // std::cout << "info string depth " << search.depth << " move " << mov.long_alg_notation() << " cp score " << result << " hash " << search.pos->state.hash << '\n';
                    if (continueSearch) result = intermedRes; // do not consider incomplete searches!
                    // if (result > runningAlpha) {
                    //     runningAlpha = result - alphaWindow;
                    // }

                    if (search.depth < 5) search.depth += 2;
                    else search.depth++;
                }

                auto adjust = (std::sqrt(search.depth) * 1.5 + std::cbrt((double) (search.depth - search.quiescenceDepthReached)) / 2.0);
                double doubleRes = result + adjust;
//
                std::lock_guard<std::mutex> lg2(mtx);
                std::cout << "info string move " << mov.long_alg_notation() << " evaluates " << doubleRes / 100
                          << " adjust " << adjust << " depth " << (search.depth-1)
                          << " quiescence depth " << (search.depth - search.quiescenceDepthReached - 1)
                          << " ttOverrides " << search.overrides << '\n';

                if (doubleRes > evaluation) {
                    bestMove = mov;
                    evaluation = doubleRes;
                }

                if (doubleRes < worstEval) {
                    worstMove = mov;
                    worstEval = doubleRes;
                }
            };

            workers[i] = std::thread(do_work);
        }
    }

    void DefaultEngine::stop_search() {
        continueSearch = false;
        for (int i = 0; i < numWorkers; i++)
            workers[i].join();
        delete[] workers;

        std::cout << "info string " << ttHits << " hits on transposition table of size " << tt.size() << '\n';
        std::cout << "info string evaluation " << evaluation / 100 << '\n';
    }

    void DefaultEngine::order_moves(MoveList &list, Move best) {

        // TODO: Here, a capture base of 512 allows up to a depth of 22
        Bitboard occ = pos->by_side(WHITE_SIDE) | pos->by_side(BLACK_SIDE);
        for (auto &mov : list) {
            mov.ranking = 0;
            if (mov == best) {
                mov.ranking = std::numeric_limits<int16_t>::max();
            } else if (mov.typeFlags == EN_PASSANT) {
                mov.ranking = 512; // he he, en passant!
            } else if (to_bitboard(mov.dst) & occ) {
                // lower the type_of, the more valuable
                mov.ranking = 512 + (int) type_of(pos->pieces[mov.src]) - (int) type_of(pos->pieces[mov.dst]);
            } else if (mov.ranking == 0) {
                mov.ranking = history[pos->turn][mov.src][mov.dst];
            }
        }

        std::sort(list.begin(), list.end(), [&](Move a, Move b) -> bool {
            return a.ranking > b.ranking;
        });
    };

}