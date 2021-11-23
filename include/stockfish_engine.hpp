#include "chess.hpp"

// #include "subprocess.hpp"
// using namespace subprocess;

#include <cstdio>

class StockfishEngine : public Engine {
public:
    StockfishEngine();
    ~StockfishEngine() override;
    Move search(Board &b, bool sideIsWhite) override;

    FILE *pipe;
    // Popen proc;
};

