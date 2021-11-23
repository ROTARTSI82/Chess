#pragma once

#include <cstdint>

#include <vector>
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

inline uint8_t valueOf(PType piece) {
    switch (piece) {
    case PType::PAWN: return 1;
    case PType::KNIGHT:
    case PType::BISHOP:
        return 3;
    case PType::ROOK: return 5;
    case PType::QUEEN: return 9;
    default:
        return -1;
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
};

struct Move {
    uint8_t srcRow : 3;
    uint8_t srcCol : 3;
    PromoteType promoteTo : 2;
    uint8_t dstRow : 3;
    uint8_t dstCol : 3;
    bool doPromote : 1;

    Move(BoardPosition f, BoardPosition t);
    Move(uint8_t src, uint8_t dst);
    Move() = default;
};

// problems: How do you undo "move states"?
class Board {
public:
    Board();
    ~Board();

    // example: get('e', 4) will do exactly what you expect
    inline Piece &get(char file, uint8_t rank) {
        return board[(8 - rank) * 8 + (file - 'a')];
    }

    inline Piece &rget(uint8_t row, uint8_t col) {
        return board[row * 8 + col];
    }

    void dbgPrint();
    
    std::string getFen(bool sideWhite = false);

    void draw(SDL_Renderer *rend, int w, int h, SDL_Texture **font);

    // precondition: Moves are legal!
    std::pair<Piece, MoveState> make(Move mov); // Returns captured piece + previous state of piece moved
    void unmake(std::pair<Piece, MoveState> capInfo, Move mov); // undos a Board::make()

    inline void capture(Piece &which) {
        // std::cout << "Captured " << which.toString() << std::endl;
        which = Piece();
    }

    std::vector<Move> pseudoLegalMoves(bool doWhite);
    void collectMovesFor(uint8_t row, uint8_t col, std::vector<Move> &store);

private:
    // layout: 1 [a, b, c...] 2 [a, b, c...] 3 [a, b, c...]
    Piece board[64];
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
};

