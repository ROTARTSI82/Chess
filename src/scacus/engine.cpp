#include "scacus/engine.hpp"
#include <algorithm>

namespace sc {

    // We can't actually use min because -min is not max! In fact, -min is negative! 
    constexpr auto NEARLY_MIN = std::numeric_limits<int>::min() + 128;
    constexpr auto ACTUAL_MIN = std::numeric_limits<int>::min() + 2;

    class Searcher {
    public:
        Position *pos = nullptr;
        int depth = 0; // current depth number (goes negative in quiescence_search())
        int maxDepth = 0; // maximum depth reached by quiescence_search()
        DefaultEngine *eng;

        inline int eval(const MoveList &legalMoves) {
            if (legalMoves.empty()) return pos->isInCheck ? NEARLY_MIN - depth : 0;

            auto count = [&](const Type t) {
                return popcnt(pos->by_side(pos->turn) & pos->by_type(t)) - popcnt(pos->by_side(opposite_side(pos->turn)) & pos->by_type(t));
            };

            auto opposingMoves = pos->turn == WHITE_SIDE ? standard_moves<BLACK_SIDE>(*pos) : standard_moves<WHITE_SIDE>(*pos);
            int ret = (legalMoves.size() - opposingMoves.size()) + 100 * count(PAWN) + 300 * count(BISHOP) + 300 * count(KNIGHT) + 500 * count(ROOK) + 900 * count(QUEEN);
            return ret;
        }

        inline int quiescence_search(int alpha, int beta) {
            auto legalMoves = legal_moves_from(*pos);
            int stand_pat = eval(legalMoves);
            if (stand_pat >= beta || !eng->continueSearch)
                return stand_pat;
            alpha = std::max(stand_pat, alpha);

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

            // maxDepth = std::min(maxDepth, depth);

            return alpha;
        }

        inline int primitive_eval(int alpha, int beta) {

            if (depth <= 0 || !eng->continueSearch) {
                return quiescence_search(alpha, beta);
            }

            auto legalMoves = legal_moves_from(*pos);
            if (legalMoves.empty())
                return pos->isInCheck ? NEARLY_MIN - depth : 0;

            std::unique_lock<std::mutex> lg(eng->ttMtx);
            if (eng->tt.count(pos->state.hash) > 0) {
                Transposition &val = eng->tt.at(pos->state.hash);
                eng->ttHits++;
                if (val.depth >= depth) return val.score;
                Move chosenOne = val.bestMove;
                lg.unlock();

                eng->order_moves(legalMoves, chosenOne);
            } else {
                lg.unlock();
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
                        eng->history[pos->turn][mov.src][mov.dst] = depth * depth;
                    return value;
                }
                
                alpha = std::max(value, alpha);
            }

            Transposition ins;
            ins.depth = depth;
            ins.score = value;
            ins.bestMove = bestMove;

            std::lock_guard<std::mutex> lg2(eng->ttMtx);
            eng->tt[pos->state.hash] = ins;
            return value;
        }

    };

    void DefaultEngine::start_search(int maxDepth) {
        continueSearch = true;
        auto legalMoves = legal_moves_from(*pos);
        order_moves(legalMoves);

        workers = new std::thread[legalMoves.size()];
        numWorkers = legalMoves.size();
        ttHits = 0;

        runningAlpha.store(ACTUAL_MIN);
        evaluation = ACTUAL_MIN;

        constexpr int alphaWindow = 450; // 0.5 queen

        for (int i = 0; i < legalMoves.size(); i++) {
            auto do_work = [&, mov{legalMoves.at(i)}, cpy{*pos}]() mutable {
                Searcher search;
                search.depth = 0;
                search.pos = &cpy;
                search.eng = this;

                int result = ACTUAL_MIN;
                while (continueSearch && ++search.depth < maxDepth) {
                    auto undoInfo = sc::make_move(cpy, mov);
                    result = -search.primitive_eval(ACTUAL_MIN, -runningAlpha.load());
                    sc::unmake_move(cpy, undoInfo, mov);

                    std::lock_guard<std::mutex> lg(mtx);
                    std::cout << "info string depth " << search.depth << " move " << mov.long_alg_notation() << " cp score " << result << " hash " << search.pos->state.hash << '\n';

                    if (result > runningAlpha) {
                        runningAlpha = result - alphaWindow;
                    }
                }

                std::lock_guard<std::mutex> lg(mtx);
                std::cout << "info string Move " << mov.long_alg_notation() << " evaluates " << result <<'\n';
                if (result > evaluation) {
                    bestMove = mov;
                    evaluation = result;
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