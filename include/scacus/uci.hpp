#pragma once

#include "scacus/engine.hpp"

#include <thread>
#include <condition_variable>

namespace sc {

    class UCI;
    void workerFunc(UCI *);

    enum class Variant {
        STANDARD, ANTICHESS,
    };

    template <bool ROOT>
    uint64_t perft2(Position &pos, int depth);
    extern template uint64_t perft2<true>(Position &, int);
    extern template uint64_t perft2<false>(Position &, int);

    void run_perft(Position &pos, int depth);

    class UCI {
    public:
        void run();
        void process_cmd(const std::string &line);

        UCI()
        // : worker{workerFunc, this}
        {
            running = false;
            workerFunc(this);
        }
//
//        ~UCI() {
//            running = false;
//            mainToWorker.notify_all();
//            worker.join();
//        }
    
    private:

        void position(const std::string &cmd);

        friend void workerFunc(UCI *);
        // we use a thread to actually think and stuff!
        // the main thread simply reads stdin & stdout
//        std::thread worker;
//
//        std::mutex mtx;
//        std::condition_variable mainToWorker;
//        std::condition_variable workerToMain;
        
        Position pos;
        bool running = true;
        bool readyok = false;
        bool go = false;

        Variant variant = Variant::STANDARD;

        DefaultEngine eng{};
    };
}
