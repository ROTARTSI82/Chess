#include "scacus/uci.hpp"

#include <chrono>
#include <mutex>
#include <sstream>

namespace sc {

#define COUT std::cout

    void workerFunc(UCI *uci) {
        auto start = std::chrono::high_resolution_clock::now();
        sc::init_movegen();
        auto duration = std::chrono::high_resolution_clock::now() - start;
        COUT << "info string Magic generation took ";
        COUT << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms\n";
        uci->readyok = true;
        uci->workerToMain.notify_all();

        while (uci->running) {
            {
                std::unique_lock<std::mutex> lg(uci->mtx);
                uci->mainToWorker.wait(lg, [&]() -> bool { return uci->go || !uci->running; });
                if (!uci->running) return;
            }

            Position cpy = uci->pos;
            COUT << "bestmove " << primitive_search(cpy, 3).first.long_alg_notation() << '\n';
            uci->go = false;
        }
    }

    void UCI::run() {
        worker = std::thread(workerFunc, this);

        COUT << "Scuffed Scacus chess engine 2021.12.22\n";
        while (running) {
            std::string line;
            std::getline(std::cin, line);
            running = !std::cin.eof();

            if (line.rfind("uci", 0) == 0) {
                // just pretend :) these are required for the lichess-bot python thing
                // to play nice with our engine.
                COUT << "option name Hash type spin default 16 min 1 max 33554432\n"
                "option name Threads type spin default 1 min 1 max 512\n"
                "option name Move Overhead type spin default 10 min 0 max 5000\n"
                "uciok\n";
            } else if (line.rfind("isready", 0) == 0) {
                std::unique_lock<std::mutex> lg(mtx);
                workerToMain.wait(lg, [&]() -> bool { return readyok; });
                COUT << "readyok\n";
            } else if (line.rfind("position", 0) == 0) {
                position(line.substr(8));
            } else if (line.rfind("go", 0) == 0) {
                // go = true;
                // mainToWorker.notify_all();
                COUT << "bestmove " << primitive_search(pos, 4).first.long_alg_notation() << '\n';
            } else if (line == "quit") {
                running = false;
            } else if (line == "d") {
                dbg_dump_position(pos);
            } else {
                COUT << "info string Unknown command: " << line << '\n';
            }
        }

        mainToWorker.notify_all();
        worker.join();
    }

    void UCI::position(const std::string &in) {
        std::string cmd = in;
        std::istringstream stream(cmd);
        
        std::string tok;
        stream >> tok;
        if (tok == "startpos") {
            pos.set_state_from_fen(STARTING_POS_FEN);
        } else if (tok == "fen") {
            int offset;
            cmd = cmd.substr(stream.tellg());
            pos.set_state_from_fen(cmd, &offset);
            stream.clear();
            stream.str(cmd.substr(offset));
        }

        stream >> tok;

        if (tok == "moves") {
            while (!stream.eof()) {
                stream >> tok;

                Move mov;
                mov.typeFlags = NORMAL;
                mov.promote = PROMOTE_QUEEN;

                char last = tok.at(tok.size() - 1);
                if (last > 'a') { // last character is promotion type! (otherwise it would be a number and this test fails)
                    mov.typeFlags = PROMOTION;
                    mov.promote = static_cast<PromoteType>((int) type_from_char(last) - 2);
                }

                mov.src = make_square(tok.at(0), tok.at(1) - '0');
                mov.dst = make_square(tok.at(2), tok.at(3) - '0');

                if ((pos.by_type(KING) & to_bitboard(mov.src)) && std::abs(mov.dst - mov.src) == 2)
                    mov.typeFlags = CASTLE;

                if (pos.state.enPassantTarget == mov.dst)
                    mov.typeFlags = EN_PASSANT;

                sc::make_move(pos, mov);
            }
        }
    }
}