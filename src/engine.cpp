#include "engine.hpp"

// return the absolute value lost by this threat. 
// there can be multiple threats on a single piece!
static inline double valueLost(double attacked, double attacker) {
    return attacked * 0.15;
}

double defaultEvaluator(Board &b, bool side) {
    double acc = 0;
    for (const BoardPosition i : b.whitePieces)
        acc += valueOf(b.at(i).type) * (side ? 1 : -1);
    
    for (const BoardPosition i : b.blackPieces)
        acc += valueOf(b.at(i).type) * (side ? -1 : 1);

    for (const Move threat : b.pseudoLegalMoves(side ^ true, true)) {
        acc -= valueLost(valueOf(b.rget(threat.dstRow, threat.dstCol).type), 
                         valueOf(b.rget(threat.srcRow, threat.srcCol).type));
    }

    return acc;
}

double MyEngine::scoreOf(double alpha, double beta) {
    // let alpha be the best score we can hope for now
    // beta is the worst score the opponent can hope for

    if (depth <= 0) return defaultEvaluator(*b, isWhite);

    uint64_t hash = b->hash();
    if (transpositionTable.contains(hash))
        return transpositionTable[hash] * (isWhite ? 1 : -1);

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

        if (moveScore > alpha) {
            alpha = moveScore;
        }
    }
    // std::cout << "Depth " << depth << " score " << max << std::endl;

    transpositionTable[hash] = alpha * (isWhite ? 1 : -1);
    return alpha;
}


Move MyEngine::search(Board &bor, bool sideIsWhite) {
    transpositionTable.clear();

    Move selected{};
    double max = -std::numeric_limits<double>::infinity();
    auto moves = bor.pseudoLegalMoves(sideIsWhite);

    for (const Move mov : moves) {
        auto undoInfo = bor.make(mov);

        b = &bor;
        isWhite = sideIsWhite ^ true;
        depth = 6;
        double moveScore = -scoreOf(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

        if (moveScore > max) {
            selected = mov;
            max = moveScore;
        }

        bor.unmake(undoInfo, mov);
    }

    std::cout << "SELECTED " << std::to_string(selected.srcRow) << 
        std::to_string(selected.srcCol) << std::to_string(selected.dstRow) << std::to_string(selected.dstCol) << std::endl;
    std::cout << transpositionTable.size() << " items cached. Current score of " << max << std::endl;
    return selected;
}