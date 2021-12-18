#pragma once

#include <cstdint>
#include <bit>
#include <string>

#include <iostream>

namespace sc {
    constexpr auto BOARD_SIZE = 64;
    constexpr char *STARTING_POS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    constexpr auto NULL_SQUARE = 255;

    typedef uint8_t Square;
    typedef uint64_t Bitboard;

    constexpr Bitboard FIRST_SQ_BB = 1UL << 63UL;

    void print_bb(const Bitboard b);

    inline Square make_square(uint8_t row, uint8_t col) { return row * 8 + col; };
    inline Square make_square(char file, uint8_t rank) { return (8 - rank) * 8 + file - 'a'; }
    inline std::string sq_to_str(const Square s) {
        return std::string{static_cast<char>('a' + s % 8)} + std::string{static_cast<char>('8' - s / 8)};
    }

    inline Bitboard to_bitboard(const Square s) { return FIRST_SQ_BB >> s; }

    enum Type : uint8_t {
        NULL_TYPE = 0,
        KING = 1, QUEEN, ROOK, BISHOP, KNIGHT, PAWN,
        NUM_UNCOLORED_PIECE_TYPES
    };

    inline char type_to_char(const Type t) {
        static constexpr char table[] = {0, 'K', 'Q', 'R', 'B', 'N', 'P'};
        return table[t];
    }

    enum ColoredType : uint8_t {
        NULL_COLORED_TYPE = 0,
        BLACK_KING = 1, BLACK_QUEEN, BLACK_ROOK, BLACK_BISHOP, BLACK_KNIGHT, BLACK_PAWN,
        // 7th and 8th slots wasted
        WHITE_KING = 9, WHITE_QUEEN, WHITE_ROOK, WHITE_BISHOP, WHITE_KNIGHT, WHITE_PAWN,
        NUM_COLORED_PIECE_TYPES
    };

    enum CastlingType : uint8_t {
        QUEENSIDE, KINGSIDE, NUM_CASTLES
    };

    enum Side : uint8_t {
        BLACK_SIDE, WHITE_SIDE, NUM_SIDES
    };

    inline ColoredType make_ColoredType(const Type t, const Side s) {
        return static_cast<ColoredType>((int) t | (s == WHITE_SIDE ? 8 : 0));
    }

    inline Side side_of(const ColoredType p) { return (static_cast<uint8_t>(p) & 8) ? WHITE_SIDE : BLACK_SIDE; }
    inline Type type_of(const ColoredType p) { return static_cast<Type>(p & 0b111); }

    inline char ct_to_char(const ColoredType t) {
        return type_to_char(type_of(t)) + 
               (side_of(t) == BLACK_SIDE ? 'a' - 'A' : 0); // make lowercase if black
    }

    class Position {
    public:
        explicit Position(const std::string &fen, int *store = nullptr); // store: Used to store the index into the string that we read to
        Position() : Position(std::string{STARTING_POS_FEN}) {};
        std::string get_fen();

        inline void set(const Square p, const Type type, const Side side) {
            pieces[p] = make_ColoredType(type, side);
            byType[type] |= to_bitboard(p);
            byColor[side] |= to_bitboard(p);
        }

        inline void clear(const Square p) {
            byType[type_of(pieces[p])] &= ~to_bitboard(p); 
            byColor[side_of(pieces[p])] &= ~to_bitboard(p);
            pieces[p] = NULL_COLORED_TYPE;
        }

    public:
        int halfmoves = 0; // number of plies since a capture or pawn advance
        int fullmoves = 1; // increment every time black moves

        Side turn = WHITE_SIDE;

        bool castlingRights[NUM_SIDES][NUM_CASTLES] = {{false, false}, {false, false}};
        Square enPassantTarget = NULL_SQUARE;

        ColoredType pieces[BOARD_SIZE];
        Bitboard byColor[NUM_SIDES]; // black = 0 white = 1
        Bitboard byType[NUM_UNCOLORED_PIECE_TYPES];
    };

    struct Move {
        Square from;
        Square to;
    };
}
