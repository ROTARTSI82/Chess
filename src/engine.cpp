#include "engine.hpp"

#include <thread>

#define ALWAYS_PLAY_EN_PASSANT 1

inline double invManhattanDist(BoardPosition a, BoardPosition b) {
    return 16 - (std::abs(a.row - b.row) + std::abs(a.col - b.col));
}

// return the absolute value lost by this threat. 
// there can be multiple threats on a single piece!
static inline double valueLost(double attacked, double attacker) {
    return attacked * 0.15;
}

double rushEvaluator(Board &b, bool side) {
    double acc = 0;
    bool kingExists = false;
    for (const BoardPosition i : (side ? b.whitePieces : b.blackPieces)) {
        acc += (i.row * (side ? -1 : 1) + (side ? 7 : 0)) * valueOf(b.at(i).type);
        kingExists |= b.at(i).type == PType::KING;
    }

    return kingExists ? acc : -std::numeric_limits<double>::infinity();
}


double swarmEvaluator(Board &b, bool side) {
    double acc = 0;
    BoardPosition opposingKing{0, 0};
    bool kingFound = false;

    for (const BoardPosition i : (side ? b.blackPieces : b.whitePieces)) {
        acc -= valueOf(b.at(i).type);

        if (b.at(i).type == PType::KING) {
            opposingKing = i;
            kingFound = true;
        }
    }

    if (!kingFound) return std::numeric_limits<double>::infinity(); // no opposing king!

    kingFound = false;
    for (const BoardPosition i : (side ? b.whitePieces : b.blackPieces)) {
        acc += invManhattanDist(i, opposingKing) * valueOf(b.at(i).type);
        kingFound |= b.at(i).type == PType::KING;
    }

    return kingFound ? acc : -std::numeric_limits<double>::infinity();
}

double huddleEvaluator(Board &b, bool side) {
    double acc = 0;
    BoardPosition myKing{0, 0};
    for (const BoardPosition i : (side ? b.whitePieces : b.blackPieces)) {
        if (b.at(i).type == PType::KING) {
            myKing = i;
            goto next;
        }
    }

    return -std::numeric_limits<double>::infinity();

next:
    for (const BoardPosition i : (side ? b.whitePieces : b.blackPieces)) {
        if (b.at(i).type == PType::KING) continue;
        acc += invManhattanDist(i, myKing) * valueOf(b.at(i).type);
    }

    return acc;
}

double defaultEvaluator(Board &b, bool side) {
    double acc = 0;

    bool kingFound = false;
    for (const BoardPosition i : b.whitePieces) {
        acc += valueOf(b.at(i).type) * (side ? 100 : -100);
        kingFound |= (b.at(i).type == PType::KING);
    }
    if (!kingFound) return (side ? -1 : 1) * std::numeric_limits<double>::infinity();

    kingFound = false;
    for (const BoardPosition i : b.blackPieces) {
        acc += valueOf(b.at(i).type) * (side ? -100 : 100);
        kingFound |= (b.at(i).type == PType::KING);
    }
    if (!kingFound) return (side ? 1 : -1) * std::numeric_limits<double>::infinity();

    // for (const Move threat : b.pseudoLegalMoves(side ^ true, true)) {
    //     acc -= valueLost(valueOf(b.rget(threat.dstRow, threat.dstCol).type), 
    //                      valueOf(b.rget(threat.srcRow, threat.srcCol).type));
    // }

    return acc;
}

double pawnGenocideEvaluator(Board &b, bool side) {
    double acc = 0;

    auto self = side ? b.whitePieces : b.blackPieces;
    auto other = side ? b.blackPieces : b.whitePieces;

    bool kingFound = false;
    for (const BoardPosition i : self) {
        acc += valueOf(b.at(i).type) * 100;
        kingFound |= (b.at(i).type == PType::KING);
    }
    if (!kingFound) return -std::numeric_limits<double>::infinity();

    kingFound = false;
    for (const BoardPosition i : other) {
        acc -= valueOf(b.at(i).type) * 100 + (b.at(i).type == PType::PAWN) * 900;
        kingFound |= (b.at(i).type == PType::KING);
    }
    if (!kingFound) return std::numeric_limits<double>::infinity();
    return acc;
}

double enPassantEvaluator(Board &b, bool side) {
    double acc = 0;
    bool kingFound = false;
    for (const BoardPosition p : (side ? b.whitePieces : b.blackPieces)) {
        kingFound |= b.at(p).type == PType::KING;

        if (b.at(p).type == PType::PAWN) {
            acc += 100;
            std::vector<Move> moves;
            b.collectMovesFor(p.row, p.col, moves, true);
            for (auto mov : moves) {
                // make en passants really good
                acc += mov.dstCol != mov.srcCol ? 9999 : 100;
            }
        }

        acc += valueOf(b.at(p).type);
    }

    return kingFound ? acc : -std::numeric_limits<double>::infinity();
}




double Searcher::scoreOf(double alpha, double beta) {
    // let alpha be the best score we can hope for now
    // beta is the worst score the opponent can hope for

    if (depth <= 0) return evaluator(*b, isWhite);

    uint64_t hash = b->hash();
    {
        std::lock_guard lg(*tableMtx);
        if (transpositionTable->contains(hash)) {
            return transpositionTable->at(hash) * (isWhite ? 1 : -1);
        }
    }

    auto moves = b->pseudoLegalMoves(isWhite);
    if (moves.empty()) return -std::numeric_limits<double>::infinity();

    for (const Move mov : moves) {
        auto undoInfo = b->make(mov);

        isWhite ^= true;
        depth--;
        double moveScore = -scoreOf(-beta, -alpha);
        depth++;
        isWhite ^= true;

        b->unmake(undoInfo, mov);

        if (moveScore >= beta) {
            // SNIP: Opponent has a better move that avoids this position
            // (which is too good for us)
            return beta;
        }

        alpha = std::max(alpha, moveScore);
    }
    // std::cout << "Depth " << depth << " score " << max << std::endl;

    std::lock_guard lg(*tableMtx);
    transpositionTable->emplace(hash, alpha * (isWhite ? 1 : -1));
    return alpha;
}


Move MyEngine::search(Board &bor, bool sideIsWhite) {

    Move selected = RandomEngine().search(bor, sideIsWhite);
    auto moves = bor.pseudoLegalMoves(sideIsWhite);

    double workers = std::thread::hardware_concurrency();
    int batchSize = std::ceil(moves.size() / workers);

    auto table = std::unordered_map<uint64_t, double>();
    std::mutex tableMtx;

    double alpha = -std::numeric_limits<double>::infinity();
    double beta = std::numeric_limits<double>::infinity();

    std::vector<std::thread> threads;
    for (int i = 0; i < workers; i++) {
        threads.push_back(std::thread([&, iCpy{i}]() {
            int low = iCpy * batchSize;
            int hi = (iCpy + 1) * batchSize;

            Board cpy = bor;

            Searcher search;
            search.tableMtx = &tableMtx;
            search.b = &cpy;
            search.depth = 5;
            search.isWhite = sideIsWhite ^ true;
            search.transpositionTable = &table;
            search.evaluator = evaluator;

            for (int j = low; j < hi && j < moves.size(); j++) {
                auto begin = std::chrono::high_resolution_clock::now();
                auto undo = cpy.make(moves.at(j));
                double score = -search.scoreOf(-std::numeric_limits<double>::infinity(), -alpha);

#if ALWAYS_PLAY_EN_PASSANT
                bool isEnPassant = cpy.rget(moves[j].dstRow, moves[j].dstCol).type == PType::PAWN 
                                   && moves[j].srcCol != moves[j].dstCol;
                if (isEnPassant) {
                    selected = moves.at(j);
                    alpha = std::numeric_limits<double>::infinity();
                }
#endif

                if (score > alpha) {
                    alpha = score;
                    selected = moves.at(j);
                }

                cpy.unmake(undo, moves.at(j));
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
                std::cout << "D8 search took (milliseconds): " << diff.count() << std::endl;
            }
            
        }));
    }

    for (int i = 0; i < workers; i++)
        threads.at(i).join();

    std::cout << "SELECTED " << std::to_string(selected.srcRow) << 
        std::to_string(selected.srcCol) << std::to_string(selected.dstRow) << std::to_string(selected.dstCol) << std::endl;
    std::cout << table.size() << " items cached. Current score of " << alpha << std::endl;
    return selected;
}