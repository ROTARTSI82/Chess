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


    class UCI {
    public:
        void run();

    
    private:

        void position(const std::string &cmd);

        friend void workerFunc(UCI *);
        // we use a thread to actually think and stuff!
        // the main thread simply reads stdin & stdout
        std::thread worker;

        std::mutex mtx;
        std::condition_variable mainToWorker;
        std::condition_variable workerToMain;
        
        Position pos;
        bool running = true;
        bool readyok = false;
        bool go = false;

        Variant variant = Variant::STANDARD;
    };
}
