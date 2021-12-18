#include <iostream>
#include <fstream>
#include <thread>

#include "engine.hpp"
#include "stockfish_engine.hpp"


int main(int argc, char **argv) {
    for (int i = 0; i < argc; i++)
        std::cout << "ARGV[" << i << "] = " << argv[i] << '\n';

    Board b{};
    bool isWhite = true;

    while (true) {
        std::string in;
        std::getline(std::cin, in);
        // std::cout << "In[] := " << in << '\n';
        // out << in << std::endl;
        // out.flush();

        if (in == "uci") {
            std::cout << "option name Hash type spin default 16 min 1 max 33554432\n"
            "option name Threads type spin default 1 min 1 max 512\n"
            "option name Move Overhead type spin default 10 min 0 max 5000\n"
            "uciok\n";
        } else if (in == "isready")
            std::cout << "readyok\n";
        else if (in.rfind("position startpos moves ", 0) == 0) {
            std::string moves = in.substr(24) + " ";
            // out << moves;
            auto space = moves.find(' ');
            
            b = Board{};
            isWhite = true;
            while (space != std::string::npos) {
                std::string m = moves.substr(0, space);
                Move store;
                AbstractStockfishEngine::moveFromAlgebraic(m, store, b);
                // out << '\t' << m;
                
                b.make(store);

                moves = moves.substr(space + 1);
                space = moves.find(' ');
                isWhite ^= true;
            }
            // out << std::endl;
            // b.dbgPrint(out);
        } else if (in.rfind("go", 0) == 0) {
            std::string outp = "bestmove " + toStr(MyEngine(defaultEvaluator).search(b, isWhite));
            std::cout << outp << '\n';
        }
    }
}
