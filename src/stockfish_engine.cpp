#include "stockfish_engine.hpp"

#include <chrono>
#include <string>
#include <fstream>

StockfishEngine::StockfishEngine() {
    pipe = popen("stockfish", "r+");
}

StockfishEngine::~StockfishEngine() {
    std::cout << "Proper dispose" << std::endl;
    pclose(pipe);
}

Move StockfishEngine::search(Board &b, bool isSideWhite) {
    std::string cmd = "setoption name UCI_Elo value 2850\nuci\nposition fen " + b.getFen(isSideWhite) + "\ngo movetime 500\nd";

    fwrite(cmd.data(), cmd.size(), 1, pipe);

    char reply[4096];
    // std::istringstream iss(outp);

    Move mov{};
    while (true)
    {
        fgets(reply, 4096, pipe);
        std::string line = std::string(reply);

        std::cout << reply;
        if (line.rfind("bestmove", 0) == 0) {
            std::string bestMove = line.substr(9, 4);
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

