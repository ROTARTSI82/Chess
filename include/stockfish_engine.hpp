#include "chess.hpp"

#include "popen2.hpp"

class StockfishEngine : public Engine {
public:
    StockfishEngine();
    ~StockfishEngine() override;
    Move search(Board &b, bool sideIsWhite) override;

    struct popen2 proc;
};

