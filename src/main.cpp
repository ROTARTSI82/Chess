#include <iostream>

#include "scacus/movegen.hpp"

#include <chrono>

struct PerftInfo {
    int nodes = 0;
    int castles = 0;
    int promotions = 0;
    int captures = 0;
    int enPassants = 0;
};

int perft_worker(sc::Position &p, int depth, PerftInfo &store) {
    if (depth == 0)
        return 1;
    

    int tot = 0;
    // std::cout << "legals call\n";
    sc::MoveList legals = p.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(p) : sc::standard_moves<sc::BLACK_SIDE>(p);
    // std::cout << "legals call\n";

    for (const auto &m : legals) {
        if (m.typeFlags == sc::PROMOTION) store.promotions++;
        if (m.typeFlags == sc::EN_PASSANT) { store.enPassants++; store.captures++; }
        if (m.typeFlags == sc::CASTLE) store.castles++;
        if (p.pieces[m.dst] != sc::NULL_COLORED_TYPE) store.captures++;

        sc::Position cpy = p;
        sc::make_move(cpy, m);
        tot += perft_worker(cpy, depth - 1, store);
    }

    return tot;
}

void perft(const std::string &fen, int depth) {
    sc::Position pos{fen};
    PerftInfo p;
    // std::cout << "first legals call\n";
    sc::MoveList legals = pos.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(pos) : sc::standard_moves<sc::BLACK_SIDE>(pos);
    // std::cout << "past first legals call\n";
    for (const auto &m : legals) {
        sc::Position cpy = pos;
        sc::make_move(cpy, m);
        int res = perft_worker(cpy, depth, p);
        p.nodes += res;
        std::cout << m.long_alg_notation() << " - " << res << '\n';
    }

    std::cout << "Total on depth " << depth << " = " << p.nodes << "\n";
    std::cout << "captures " << p.captures << '\n';
    std::cout << "en passants " << p.enPassants << '\n';
    std::cout << "castles " << p.castles << '\n';
    std::cout << "promotions " << p.promotions << '\n';
    std::cout << "\n\n\n";
}


uint64_t Perft(int depth, sc::Position &pos)
{
  sc::Move move_list[256];
  int n_moves, i;
  uint64_t nodes = 0;

  if (depth == 0) 
    return 1ULL;

//   std::cout << "legal moves\n";
  sc::MoveList legals = pos.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(pos) : sc::standard_moves<sc::BLACK_SIDE>(pos);

//   std::cout << "end moves\n";
  for (auto m : legals) {
    sc::Position cpy = pos;

    // std::cout << "make move\n";
    sc::make_move(cpy, m);
//   std::cout << "end make move\n";
    nodes += Perft(depth - 1, cpy);

  }
  return nodes;
}


int main(int argc, char **argv) {
    auto start = std::chrono::high_resolution_clock::now();
    sc::init_movegen();
    auto dur = std::chrono::high_resolution_clock::now() - start;

    std::cout << "Magic brute-forcing took " <<  std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << "ms\n"; 

    std::cout << "Hello World!\n";
    while (true) {
        std::cout << "Enter FEN> ";
        std::string line;
        std::getline(std::cin, line);
        if (line == "quit") break;

        sc::Position pos{line};
        for (int i = 1; i < 8; i++) {
            std::cout << "perft(" << i << ") = " << Perft(i, pos) << '\n';
        }
    }
}
