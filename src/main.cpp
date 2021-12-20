#include <iostream>

#include "scacus/movegen.hpp"

#include <chrono>

int main(int argc, char **argv) {
    auto start = std::chrono::high_resolution_clock::now();
    sc::init_movegen();
    auto dur = std::chrono::high_resolution_clock::now() - start;

    std::cout << "Magic brute-forcing took " <<  std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << "ms\n"; 

    while (true) {
        std::cout << "Enter Coord> ";
        std::string line;
        std::getline(std::cin, line);

        if (line == "quit") return 0;


        sc::Square sq = sc::make_square(line.at(0), line.at(1) - '0');
        std::cout << "sq = " << std::to_string(sq) << " to_str = " << sc::sq_to_str(sq) << std::endl;
        std::cout << "RANK " << (int)sc::rank_ind_of(sq) << " FILE " << (int)sc::file_ind_of(sq) << std::endl;

        sc::print_bb(sc::file_bb(line.at(0)) | sc::rank_bb(line.at(1) - '0'));
        
        std::cout << "\n\nKNIGHTS\n";
        sc::print_bb(sc::KNIGHT_MOVES[sq]);
        std::cout << "\n\nKINGS\n";
        sc::print_bb(sc::KING_MOVES[sq]);
        std::cout << "\n\nBLACK PAWN ATTACKS\n";
        sc::print_bb(sc::PAWN_ATTACKS[sc::BLACK_SIDE][sq]);
        std::cout << "\n\nWHITE PAWN ATTACKS\n";
        sc::print_bb(sc::PAWN_ATTACKS[sc::WHITE_SIDE][sq]);
        std::cout << "\n\nBLACK PAWN MOVES\n";
        sc::print_bb(sc::PAWN_MOVES[sc::BLACK_SIDE][sq]);
        std::cout << "\n\nWHITE PAWN MOVES\n";
        sc::print_bb(sc::PAWN_MOVES[sc::WHITE_SIDE][sq]);

        std::cout << "\n\nBISHOP OCCUPANCY MASK (SHIFT=" << (int) sc::BISHOP_MAGICS[sq].shift << ")\n";
        sc::print_bb(sc::BISHOP_MAGICS[sq].mask);

        std::cout << "\n\nROOK OCCUPANCY MASK (SHIFT=" << (int) sc::ROOK_MAGICS[sq].shift << ")\n";
        sc::print_bb(sc::ROOK_MAGICS[sq].mask);

        std::cout << "\n\nROOK ATTACKS (NO OCCUPIED)\n";
        sc::print_bb(sc::lookup<sc::ROOK_MAGICS>(sq, 0));


        std::cout << "\n\nBISHOP ATTACKS (NO OCCUPIED)\n";
        sc::print_bb(sc::lookup<sc::BISHOP_MAGICS>(sq, 0));
    }


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
