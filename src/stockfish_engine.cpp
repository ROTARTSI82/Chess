#include "stockfish_engine.hpp"

#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <csignal>

#include <poll.h>
#include <fcntl.h>
#include <sys/wait.h>

AbstractStockfishEngine::AbstractStockfishEngine() {
    popen2("stockfish", &proc);
    fcntl(proc.from_child, F_SETFL, fcntl(proc.from_child, F_GETFL, 0) | O_NONBLOCK);
}

AbstractStockfishEngine::~AbstractStockfishEngine() {
    write(proc.to_child, "quit\n", 5);
    close(proc.to_child);
    kill(proc.child_pid, 0);
    waitpid(proc.child_pid, NULL, 0);
    kill(proc.child_pid, 0);
}

void AbstractStockfishEngine::moveFromAlgebraic(const std::string &mov, Move &store, Board &b) {
    // std::cout << mov;
    store.srcCol = mov.at(0) - 'a';
    store.dstCol = mov.at(2) - 'a';

    store.srcRow = 8 - (mov.at(1) - '0');
    store.dstRow = 8 - (mov.at(3) - '0');

    if (b.rget(store.srcRow, store.srcCol).type == PType::PAWN 
        && (store.dstRow == 0 || store.dstRow == 7)) {
        store.doPromote = true;
        store.promoteTo = PromoteType::QUEEN;
    }
}

Move AbstractStockfishEngine::search(Board &b, bool isSideWhite) {
    resetSearch(b, isSideWhite);
    std::string cmd = searchCommand;
    cmd.replace(cmd.find("{FEN}"), 5, b.getFen(isSideWhite));

    std::cout << "CMD> " << cmd << std::endl;
    write(proc.to_child, cmd.data(), cmd.size());

    char reply[4097];
    // std::istringstream iss(outp);

    Move mov = RandomEngine().search(b, isSideWhite);
    while (true)
    {
        pollfd pInfo;
        pInfo.events = POLL_IN;
        pInfo.fd = proc.from_child;
        poll(&pInfo, 1, 1000);

        int nbytes = read(proc.from_child, reply, 4096);
        if (nbytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            std::cerr << "READ FAILED: " << strerror(errno) << std::endl;
            return mov;
        }

        reply[nbytes] = '\0';
        std::string line = std::string(reply);

        std::cout << reply;
        if (lineReceived(line, mov)) return mov;
    }
}

void StockfishEngine::resetSearch(Board &b, bool isSideWhite) {
    cachedBoard = &b;
}

bool StockfishEngine::lineReceived(const std::string &line, Move &mov) {
    auto bestmove = line.find("bestmove");
    if (bestmove != std::string::npos) {
        std::string bestMove = line.substr(bestmove + 9, 4);
        moveFromAlgebraic(bestMove, mov, *cachedBoard);
        return true;
    }
    return false;
}


WorstFishEngine::WorstFishEngine() : AbstractStockfishEngine() {
    searchCommand = "setoption name MultiPV value 500\nposition fen {FEN}\ngo movetime 1000\n";
}

void WorstFishEngine::resetSearch(Board &b, bool isSideWhite) {
    cachedBoard = &b;
    worstScore = 999999;
}

bool WorstFishEngine::lineReceived(const std::string &line, Move &mov) {
    auto score = line.find("score cp");
    try {
        while (score != std::string::npos) {
            auto end = line.find(' ', score + 9);
            auto sub = line.substr(score + 9, end - score - 9);
            long scoreValue = sub.at(0) == '-' ? -std::stoi(sub.substr(1)) : std::stoi(sub);
            // std::cout << "Score = " << scoreValue << std::endl;
            if (compare(worstScore, scoreValue)) {
                worstScore = scoreValue;
                // std::cout << "\n\nNEW BEST SCORE = " << scoreValue << "\n\n";

                auto moveToPlay = line.find(" pv ", score + 3);
                std::string alg = line.substr(moveToPlay + 4, 4);
                // std::cout << "FOUND WORST: " << alg << std::endl;
                moveFromAlgebraic(alg, mov, *cachedBoard);
            }
            score = line.find("score cp", score + 3);
        }
    } catch (const std::exception &e) {
        std::cout << "EXCEPTION! Ending search prematurely" << std::endl;
        return true;
    }

    if (line.find("bestmove") != std::string::npos) {
        std::cout << "Finished search. Centipawn score = " << worstScore << std::endl; 
        return true;
    }
    return false;
}
