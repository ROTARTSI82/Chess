#pragma once

#include <cstdint>

#include <vector>
#include <random>
#include <unordered_set>
#include <iostream>
#include <string>
#include <tuple>

#include <SDL.h>

#define WHITE_SIDE true
#define BLACK_SIDE false

enum class PType : uint8_t {
    NIL = 0, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
};

enum class PromoteType : uint8_t {
    KNIGHT = 0, BISHOP, ROOK, QUEEN
};

enum class MoveState : uint8_t {
    NOT_MOVED = 0, AFTER_DOUBLE = 1,
    MOVED = 2
};

inline double valueOf(PType piece) {
    switch (piece) {
    case PType::PAWN: return 1;
    case PType::KNIGHT:
    case PType::BISHOP:
        return 3;
    case PType::ROOK: return 5;
    case PType::QUEEN: return 9;
    case PType::KING:
        return 9001; // over nine thousand
    default: return 0;
    }
}

struct Piece {
    PType type : 3;
    bool isWhite : 1;
    MoveState moveState : 2;
    bool isPassant : 1; // only used for Board::unmake

    Piece(PType t, bool white);
    Piece();

    inline bool exists() {
        return type != PType::NIL;
    }

    inline std::string toString() {
        std::string ret = isWhite ? "w" : "b";
        switch (type) {
        case PType::KING: return ret + "K";
        case PType::QUEEN: return ret + "Q";
        case PType::ROOK: return ret + "R";
        case PType::BISHOP: return ret + "B";
        case PType::KNIGHT: return ret + "N";
        case PType::PAWN: return ret + "P";
        default: return ret + "?";
        }
    }

    inline char fenChar() {
        char c = 0;
        switch (type) {
        case PType::KING: c = 'K'; break;
        case PType::QUEEN: c = 'Q'; break;
        case PType::ROOK: c = 'R'; break;
        case PType::BISHOP: c = 'B'; break;
        case PType::KNIGHT: c = 'N'; break;
        case PType::PAWN: c = 'P'; break;
        default: return '?';
        }

        return c + (isWhite ? 0 : 32);
    }
};

struct BoardPosition {
    uint8_t row : 3;
    uint8_t col : 3;

    BoardPosition(int r, int c) : row(r), col(c) {}

    inline static BoardPosition fromRankFile(char file, uint8_t rank) {
        return BoardPosition{static_cast<uint8_t>(8 - rank), static_cast<uint8_t>(file - 'a')};
    }

    inline std::string toString() const { return std::to_string(row) + ", " + std::to_string(col); }

    inline bool operator==(const BoardPosition rhs) const { return row == rhs.row && col == rhs.col; }
};

namespace std {
  template <>
  struct hash<BoardPosition> {
    std::size_t operator()(const BoardPosition k) const {
      return (static_cast<std::size_t>(k.row) << 3) | static_cast<std::size_t>(k.col);
    }
  };
}

struct Move {
    uint8_t srcRow : 3;
    uint8_t srcCol : 3;
    PromoteType promoteTo : 2;
    uint8_t dstRow : 3;
    uint8_t dstCol : 3;
    bool doPromote : 1;

    uint8_t rank = 0;

    Move(BoardPosition f, BoardPosition t);
    Move(uint8_t src, uint8_t dst);
    Move() = default;
};

// problems: How do you undo "move states"?
class Board {
public:
    std::unordered_set<BoardPosition> blackPieces;
    std::unordered_set<BoardPosition> whitePieces;

    static void initZobrist();

    Board(const Board &other);
    Board();
    ~Board();

    inline Piece &at(BoardPosition p) {
        return rget(p.row, p.col);
    }

    // example: get('e', 4) will do exactly what you expect
    inline Piece &get(char file, uint8_t rank) {
        return board[(8 - rank) * 8 + (file - 'a')];
    }

    inline Piece &rget(uint8_t row, uint8_t col) {
        /**
         * 8 - rank = row
         * file - 'a' = col
         * 
         * file = col + 'a'
         * rank = 8 - row
         * 
         */
        return board[row * 8 + col];
    }

    void dbgPrint(std::ostream &out);
    
    std::string getFen(bool sideWhite = false);

    void draw(SDL_Renderer *rend, int w, int h, SDL_Texture **font);

    inline uint64_t hash() {
        return runningHash;
    }

    // precondition: Moves are legal!
    std::pair<Piece, MoveState> make(Move mov); // Returns captured piece + previous state of piece moved
    void unmake(std::pair<Piece, MoveState> capInfo, Move mov); // undos a Board::make()

    std::vector<Move> pseudoLegalMoves(bool doWhite, bool ONLY_CAP = false);

    void collectMovesFor(uint8_t row, uint8_t col, std::vector<Move> &store, bool ONLY_CAP = false);

private:
    uint64_t runningHash = 0;
    static uint64_t zobristTable[64][12];

    // layout: 1 [a, b, c...] 2 [a, b, c...] 3 [a, b, c...]
    Piece board[64];

    inline void singularMov(BoardPosition src, BoardPosition dst, bool side) {
        auto &which = (side ? whitePieces : blackPieces);
        which.erase(src);
        which.emplace(dst);

        hashUpdate(src, at(src));
        hashUpdate(dst, at(src));
        at(dst) = at(src);
    }

    inline void capture(BoardPosition pos) {
        hashUpdate(pos, at(pos));
        (at(pos).isWhite ? whitePieces : blackPieces).erase(pos);
        at(pos) = Piece();
    }

    inline void hashUpdate(BoardPosition p, Piece pi) {
        runningHash ^= zobristTable[std::hash<BoardPosition>()(p)][(int) pi.type + (pi.isWhite ? -1 : 5)];
    }
};


// to write an engine: inherit from this class and implement the search function!
class Engine {
public:
    virtual ~Engine() = default;
    virtual Move search(Board &b, bool sideIsWhite) = 0;
};

class RandomEngine : public Engine {
public:
    ~RandomEngine() = default;
    Move search(Board &b, bool sideIsWhite) override;

private:
    std::random_device dev;
    std::mt19937 rng = std::mt19937(dev());
};

inline std::string toStr(Move m) {
    return (char) (m.srcCol + 'a') + std::string("") + std::to_string((int) 8 -m.srcRow) + (char) (m.dstCol + 'a') + "" + std::to_string((int) 8-m.dstRow);
}
