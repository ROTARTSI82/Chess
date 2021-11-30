#include "engine.hpp"

#include <thread>
#include <atomic>

#define ALWAYS_PLAY_EN_PASSANT 0

#define MULTITHREAD 1

// This probably affects deterministic multithreading too
#define ALPHA_BETA_PRUNING 1

#define SDEPTH 7

// Disable transposition tables for (mostly) deterministic multithreading
#define TRANSPOSITION_TABLE 1

template <typename T>
static inline T randOf(const std::vector<T> &vec) {
    static std::random_device dev;
    static std::mt19937_64 eng = std::mt19937_64(dev());

    auto dist = std::uniform_int_distribution<int>(0, vec.size() - 1);
    return vec.at(dist(eng));
}

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
    for (const BoardPosition i : b.whitePieces)
        acc += valueOf(b.at(i).type) * (side ? 100 : -100);
    for (const BoardPosition i : b.blackPieces)
        acc += valueOf(b.at(i).type) * (side ? -100 : 100);
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
    // beta is the worst score (for us) the opponent can hope for

    if (depth <= 0) return evaluator(*b, isWhite);

#if TRANSPOSITION_TABLE
    uint64_t hash = b->hash();
    {
        std::lock_guard lg(*tableMtx);
        if (transpositionTable->contains(hash) && transpositionTable->at(hash).second >= depth) {
            return transpositionTable->at(hash).first * (isWhite ? 1 : -1);
        }
    }
#endif

    auto moves = b->pseudoLegalMoves(isWhite);
    if (moves.empty()) return -std::numeric_limits<double>::infinity();

    double value = -std::numeric_limits<double>::infinity();
    std::vector<Move> line;
    for (const Move mov : moves) {
        auto undoInfo = b->make(mov);

        isWhite ^= true;
        depth--;
        value = std::max(-scoreOf(-beta, -alpha), value);
        depth++;
        isWhite ^= true;

        b->unmake(undoInfo, mov);

#if ALPHA_BETA_PRUNING
        if (value >= beta) {
            // SNIP: Opponent has a better move that avoids this position
            // (which is too good for us)
#if TRANSPOSITION_TABLE
            std::lock_guard lg(*tableMtx);
            transpositionTable->emplace(hash, std::make_pair(value * (isWhite ? 1 : -1), depth));
#endif
            return value;
        }
#endif

        alpha = std::max(alpha, value);
    }
    
#if TRANSPOSITION_TABLE
    std::lock_guard lg(*tableMtx);
    transpositionTable->emplace(hash, std::make_pair(value * (isWhite ? 1 : -1), depth));
#endif
    return value;
}


Move MyEngine::search(Board &bor, bool sideIsWhite) {

    std::vector<Move> candidates;
    auto moves = bor.pseudoLegalMoves(sideIsWhite);

    double workers = MULTITHREAD ? std::thread::hardware_concurrency() : 1;
    int batchSize = std::ceil(moves.size() / workers);

    auto table = std::unordered_map<uint64_t, std::pair<double, int>>();
    std::mutex tableMtx;
    std::mutex globalAlphaMtx;

    double alpha = -std::numeric_limits<double>::infinity();
    std::vector<std::thread> threads;
    for (int i = 0; i < workers; i++) {
        threads.push_back(std::thread([&, iCpy{i}]() {
            int low = iCpy * batchSize;
            int hi = (iCpy + 1) * batchSize;

            Board cpy = Board{bor};

            Searcher search;
            search.tableMtx = &tableMtx;
            search.b = &cpy;
            search.depth = SDEPTH;
            search.isWhite = sideIsWhite ^ true;
            search.transpositionTable = &table;
            search.evaluator = evaluator;

            for (int j = low; j < hi && j < moves.size(); j++) {
                auto begin = std::chrono::high_resolution_clock::now();
                auto undo = cpy.make(moves.at(j));
                double score = -search.scoreOf(-std::numeric_limits<double>::infinity(), -alpha);
                // std::cout << "moves[" << j << "] alpha-" << alpha << " = " << score << std::endl;

#if ALWAYS_PLAY_EN_PASSANT
                bool isEnPassant = cpy.rget(moves[j].dstRow, moves[j].dstCol).type == PType::PAWN 
                                   && moves[j].srcCol != moves[j].dstCol;
                if (isEnPassant) {
                    selected = moves.at(j);
                    alpha = std::numeric_limits<double>::infinity();
                }
#endif

                {
                    std::lock_guard<std::mutex> lg(globalAlphaMtx);
                    if (score > alpha) {
                        alpha = score;
                        candidates.clear();
                        candidates.push_back(moves.at(j));
                    } else if (alpha - score <= alpha*std::numeric_limits<double>::epsilon()) {
                        candidates.push_back(moves.at(j));
                    }
                }

                cpy.unmake(undo, moves.at(j));
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
                std::cout << "Search took (milliseconds): " << diff.count() << std::endl;
            }
            
        }));
    }

    for (int i = 0; i < workers; i++)
        threads.at(i).join();
    
    std::cout << table.size() << " items cached. Current score of " << alpha << " with " << candidates.size() << " candidates" << std::endl;

    // TODO: Choose good selector function! It matters
    auto sel = std::max_element(candidates.begin(), candidates.end(), [&](Move a, Move b) -> bool {
        int bDst = (int) bor.rget(b.dstRow, b.dstCol).type;
        int aDst = (int) bor.rget(a.dstRow, a.dstCol).type;

        auto aMatDiff = aDst != 0 ? aDst - (int) bor.rget(a.srcRow, a.srcCol).type : -1;
        auto bMatDiff = bDst != 0 ? bDst - (int) bor.rget(b.srcRow, b.srcCol).type : -1;
        return aMatDiff < bMatDiff;

        return (a.srcRow | a.dstCol << 3 | a.srcCol << 6 | a.dstRow << 9) >
               (b.srcRow | b.dstCol << 3 | b.srcCol << 6 | b.dstRow << 9);
    });
    return *sel;
}