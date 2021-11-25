#include "engine.hpp"

#include <thread>

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

double Searcher::scoreOf(double alpha, double beta) {
    // let alpha be the best score we can hope for now
    // beta is the worst score the opponent can hope for

    if (depth <= 0) return defaultEvaluator(*b, isWhite);

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
        // if (moveScore > alpha) {
        //     alpha = moveScore;
        // }
    }
    // std::cout << "Depth " << depth << " score " << max << std::endl;

    std::lock_guard lg(*tableMtx);
    transpositionTable->emplace(hash, alpha * (isWhite ? 1 : -1));
    return alpha;
}


Move MyEngine::search(Board &bor, bool sideIsWhite) {

    Move selected{};
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
            search.depth = 6;
            search.isWhite = sideIsWhite ^ true;
            search.transpositionTable = &table;

            for (int j = low; j < hi && j < moves.size(); j++) {
                auto begin = std::chrono::high_resolution_clock::now();
                auto undo = cpy.make(moves.at(j));
                double score = -search.scoreOf(-std::numeric_limits<double>::infinity(), -alpha);

                if (score > alpha) {
                    alpha = score;
                    selected = moves.at(j);
                }

                cpy.unmake(undo, moves.at(j));
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
                std::cout << "D6 search took (milliseconds): " << diff.count() << std::endl;
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