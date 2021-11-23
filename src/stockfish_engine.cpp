#include "stockfish_engine.hpp"

#include <chrono>
#include <string>
#include <fstream>
#include <sstream>

StockfishEngine::StockfishEngine() {
    popen2("stockfish", &proc);
}

StockfishEngine::~StockfishEngine() {
    close(proc.to_child);
}

Move StockfishEngine::search(Board &b, bool isSideWhite) {
    std::string cmd = "setoption name UCI_Elo value 2850\nuci\nposition fen " + b.getFen(isSideWhite) + "\ngo movetime 500\nd";

    write(proc.to_child, cmd.data(), cmd.size());

    char reply[4097];
    // std::istringstream iss(outp);

    Move mov{};
    while (true)
    {
        int nbytes = read(proc.from_child, reply, 4096);
        reply[nbytes] = '\0';
        std::string line = std::string(reply);

        std::cout << reply;

        auto bestmove = line.find("bestmove");
        if (bestmove != std::string::npos) {
            std::string bestMove = line.substr(bestmove + 9, 4);
            std::cout << bestMove;
            mov.srcCol = bestMove.at(0) - 'a';
            mov.dstCol = bestMove.at(2) - 'a';

            mov.srcRow = 8 - (bestMove.at(1) - '0');
            mov.dstRow = 8 - (bestMove.at(3) - '0');

            if (b.rget(mov.srcRow, mov.srcCol).type == PType::PAWN 
                && (mov.dstRow == 0 || mov.dstRow == 7)) {
                mov.doPromote = true;
                mov.promoteTo = PromoteType::QUEEN;
            }
            std::cout << "Interped " << std::endl;
            return mov;
        }
    }
}

