#include "scacus/uci.hpp"

#include <chrono>
#include <mutex>
#include <sstream>

#include <fstream>

#include "scacus/antichess.hpp"

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
            // COUT << "bestmove " << uci->eng.primitive_search(cpy, 3).first.long_alg_notation() << '\n';
            uci->go = false;
        }
    }

    uint64_t perft_worker(sc::Position &pos, int depth) {
        if (depth == 0)
            return 1;

        uint64_t tot = 0;
        sc::MoveList legals = pos.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(pos) : sc::standard_moves<sc::BLACK_SIDE>(pos);

        for (const auto &m : legals) {
            sc::StateInfo undo = sc::make_move(pos, m);
            tot += perft_worker(pos, depth - 1);
            sc::unmake_move(pos, undo, m);
        }

        return tot;
    }

    void perft(Position &pos, int depth) {
        uint64_t total = 0;
        auto start = std::chrono::high_resolution_clock::now();

        sc::MoveList legals = pos.turn == sc::WHITE_SIDE ? sc::standard_moves<sc::WHITE_SIDE>(pos) : sc::standard_moves<sc::BLACK_SIDE>(pos);

        for (const auto &m : legals) {
            sc::StateInfo undo = sc::make_move(pos, m);
            uint64_t res = perft_worker(pos, depth - 1);
            sc::unmake_move(pos, undo, m);

            total += res;
            std::cout << m.long_alg_notation() << ": " << res << '\n';
        }
        
        auto duration = std::chrono::high_resolution_clock::now() - start;
        auto nps = total / (std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000000.0);

        // mimick stockfish perft output
        std::cout << "Nodes searched (depth=" << depth << "): " << total << "\n";
        std::cout << "Speed: " << nps / 1000.0 << " thousand leaf nodes per second\n";
    }

    void UCI::run() {
        worker = std::thread(workerFunc, this);

        COUT << "Scuffed Scacus chess engine 2021.12.22\n";
        while (running) {
            std::string line;
            std::getline(std::cin, line);
            running = !std::cin.eof();

            if (line == "uci") {
                // just pretend :) these are required for the lichess-bot python thing
                // to play nice with our engine.
                COUT << "option name Hash type spin default 16 min 1 max 33554432\n"
                "option name Threads type spin default 1 min 1 max 512\n"
                "option name Move Overhead type spin default 10 min 0 max 5000\n"
                "option name UCI_Variant type combo default chess var 3check var 5check var ai-wok var almost var amazon var antichess var armageddon var asean var ataxx var atomic var breakthrough var bughouse var cambodian var chaturanga var chess var chessgi var chigorin var clobber var codrus var coregal var crazyhouse var dobutsu var euroshogi var extinction var fairy var fischerandom var gardner var giveaway var gorogoro var grasshopper var hoppelpoppel var horde var judkins var karouk var kinglet var kingofthehill var knightmate var koedem var kyotoshogi var loop var losalamos var losers var makpong var makruk var micro var mini var minishogi var minixiangqi var newzealand var nightrider var nocastle var nocheckatomic var normal var placement var pocketknight var racingkings var seirawan var shatar var shatranj var shouse var sittuyin var suicide var threekings var torishogi\n"
                "uciok\n";
            } else if (line.rfind("setoption", 0) == 0) {
                if (line.find("antichess") != std::string::npos) {
                    variant = Variant::ANTICHESS;
                } else {
                    variant = Variant::STANDARD;
                }
            } else if (line.rfind("isready", 0) == 0) {
                std::unique_lock<std::mutex> lg(mtx);
                workerToMain.wait(lg, [&]() -> bool { return readyok; });
                COUT << "readyok\n";
            } else if (line.rfind("position", 0) == 0) {
                position(line.substr(8));
            } else if (line.rfind("go perft", 0) == 0) { 
                int num = std::stoi(line.substr(8));
                perft(pos, num);
            } else if (line.rfind("go", 0) == 0) {
                // go = true;
                // mainToWorker.notify_all();
                if (variant == Variant::ANTICHESS) {
                    Move mov = antichess_search(pos, 8).first;
                    std::string out = mov.long_alg_notation();
                    if (mov.typeFlags == CASTLE) out += 'k';
                    COUT << "bestmove " << out << '\n';
                } else {
                    eng.pos = &pos;
                    eng.start_search(9999);
                    // std::this_thread::sleep_for(std::chrono::seconds(10));
                    eng.stop_search();
                    COUT << "bestmove " << eng.bestMove.long_alg_notation() << '\n';
                    COUT << "info string hash = " << eng.pos->state.hash << '\n';
                }
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
                if (last == 'k') { // antichess promote to king!
                    mov.typeFlags = CASTLE; // castling used since PromoteType only has enough bits for standard promotions, and castling is illegal in antichess.
                } else if (last > 'a') { // last character is promotion type! (otherwise it would be a number and this test fails)
                    mov.typeFlags = PROMOTION;
                    mov.promote = static_cast<PromoteType>((int) type_from_char(last) - 2);
                }

                mov.src = make_square(tok.at(0), tok.at(1) - '0');
                mov.dst = make_square(tok.at(2), tok.at(3) - '0');

                if ((pos.by_type(KING) & to_bitboard(mov.src)) && std::abs(mov.dst - mov.src) == 2)
                    mov.typeFlags = CASTLE;

                if (pos.state.enPassantTarget == mov.dst)
                    mov.typeFlags = EN_PASSANT;

                if (variant == Variant::ANTICHESS)
                    sc::antichess_make_move(pos, mov);
                else
                    sc::make_move(pos, mov);
            }
        }
    }
}
