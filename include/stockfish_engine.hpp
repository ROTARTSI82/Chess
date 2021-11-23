#include "chess.hpp"

#include "popen2.hpp"

class AbstractStockfishEngine : public Engine {
public:
    AbstractStockfishEngine();
    ~AbstractStockfishEngine() override;

    Move search(Board &b, bool sideIsWhite) override;
    virtual bool lineReceived(const std::string &line, Move &selected) = 0;
    virtual void resetSearch(Board &b, bool isSideWhite) = 0;

    void moveFromAlgebraic(const std::string &mov, Move &store, Board &b);

    std::string searchCommand = "setoption name UCI_Elo value 2850\nposition fen {FEN}\ngo movetime 1000\n";
    struct popen2 proc;
};


class StockfishEngine : public AbstractStockfishEngine {
public:
    StockfishEngine() : AbstractStockfishEngine() {};
    ~StockfishEngine() = default;
    bool lineReceived(const std::string &line, Move &selected) override;
    void resetSearch(Board &b, bool isSideWhite) override;

private:
    Board *cachedBoard = nullptr;
};

class WorstFishEngine : public AbstractStockfishEngine {
public:
    WorstFishEngine();
    ~WorstFishEngine() = default;

    bool lineReceived(const std::string &line, Move &selected) override;
    void resetSearch(Board &b, bool isSideWhite) override;

    // return true if we want to select newScore over currentBest
    inline virtual bool compare(int currentBest, int newScore) {
        return newScore < currentBest;
    }

private:
    Board *cachedBoard = nullptr;
    int worstScore = 9999;
};


class DrawFishEngine : public WorstFishEngine {
public:
    DrawFishEngine() : WorstFishEngine() {};
    ~DrawFishEngine() = default;

    inline bool compare(int currentBest, int newScore) override {
        return std::abs(newScore) < std::abs(currentBest);
    }
};
