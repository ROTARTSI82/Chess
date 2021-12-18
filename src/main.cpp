#include <iostream>

#include "scacus/bitboard.hpp"

int main(int argc, char **argv) {
    std::cout << "Hello World!\n";
    while (true) {
        std::cout << "Enter FEN> ";
        std::string line;
        std::getline(std::cin, line);
        if (line == "quit") break;

        sc::Position p{line};
        std::cout << "FEN: " << p.get_fen() << '\n';

        std::cout << "\n\nWhite: \n"; 
        sc::print_bb(p.byColor[sc::WHITE_SIDE]);
        std::cout << "\n\nBlack: \n";
        sc::print_bb(p.byColor[sc::BLACK_SIDE]);

        std::cout << "\n\nPawns:\n";
        sc::print_bb(p.byType[sc::PAWN]);
        std::cout << "\n\nBishops:\n";
        sc::print_bb(p.byType[sc::BISHOP]);
        std::cout << "\n\nKnights:\n";
        sc::print_bb(p.byType[sc::KNIGHT]);
        std::cout << "\n\nRooks:\n";
        sc::print_bb(p.byType[sc::ROOK]);
        std::cout << "\n\nQueens:\n";
        sc::print_bb(p.byType[sc::QUEEN]);
        std::cout << "\n\nKings:\n";
        sc::print_bb(p.byType[sc::KING]);
    }
}
