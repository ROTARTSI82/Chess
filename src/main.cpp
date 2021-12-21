#include <iostream>

#include "scacus/movegen.hpp"

#include <chrono>

#if 0
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
    sc::Side t = p.turn;
    sc::MoveList legals = p.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(p) : sc::standard_moves<sc::BLACK_SIDE>(p);
    // std::cout << "legals call\n";

    for (const auto &m : legals) {
        if (m.typeFlags == sc::PROMOTION) store.promotions++;
        if (m.typeFlags == sc::EN_PASSANT) { store.enPassants++; store.captures++; }
        if (m.typeFlags == sc::CASTLE) store.castles++;
        if (p.pieces[m.dst] != sc::NULL_COLORED_TYPE) store.captures++;

        sc::StateInfo undo = sc::make_move(p, m);
        tot += perft_worker(p, depth - 1, store);
        sc::unmake_move(p, undo, m);
        if (p.turn != t) throw std::runtime_error{"Turn corrupted"};
    }

    return tot;
}

void perft(const std::string &fen, int depth) {
    sc::Position pos{fen};
    PerftInfo p;
    // std::cout << "first legals call\n";
    sc::Side t = pos.turn;
    sc::MoveList legals = pos.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(pos) : sc::standard_moves<sc::BLACK_SIDE>(pos);
    // std::cout << "past first legals call\n";
    for (const auto &m : legals) {
        sc::StateInfo undo = sc::make_move(pos, m);
        int res = perft_worker(pos, depth, p);
        p.nodes += res;
        std::cout << m.long_alg_notation() << " - " << res << '\n';
        sc::unmake_move(pos, undo, m);
        if (pos.turn != t) throw std::runtime_error{"Turn corrupted!"};
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

    // std::cout << "make move\n";
    sc::StateInfo undo = sc::make_move(pos, m);
//   std::cout << "end make move\n";
    nodes += Perft(depth - 1, pos);
    sc::unmake_move(pos, undo, m);

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
        sc::standard_moves<sc::BLACK_SIDE>(pos);

        // while (true) {
        //     auto legals = pos.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(pos) : sc::standard_moves<sc::BLACK_SIDE>(pos);
        //     sc::dbg_dump_position(pos);

        //     int i = 0;
        //     for (const auto m : legals) std::cout << "moves[" << i++ << "] = " << m.long_alg_notation() << '\n';

        //     std::cout << "select one> ";
        //     std::cin >> i;
        //     sc::make_move(pos, legals.at(i));
        // }
        for (int i = 0; i < 8; i++) {
            perft(line, i);
            // std::cout << "perft(" << i << ") = " << Perft(i, pos) << '\n';
        }
    }
}

#endif