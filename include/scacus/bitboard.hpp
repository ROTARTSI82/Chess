#pragma once

#include <cstdint>
#include <bit>
#include <string>

#include <vector>
#include <iostream>
#include <array>

#include <bitset>

namespace sc {
    constexpr auto BOARD_SIZE = 64;
    constexpr auto STARTING_POS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    constexpr auto NULL_SQUARE = 255;

    typedef uint8_t Square;
    typedef uint64_t Bitboard;

    constexpr Bitboard RANK1_BB =      0x00000000000000FF;
    constexpr Bitboard FILEA_BB =      0x0101010101010101;
    constexpr Bitboard EVERYTHING_BB = 0xFFFFFFFFFFFFFFFF;

    constexpr inline int popcnt(const uint64_t v) {
        return std::__popcount<uint64_t>(v);
        // return std::bitset<64>(v).count();
    }

    /*
     *     -- rank index = square / 8
     *     |
     * 8 | 7 | 56 57 58 59 60 61 62 63
     * 7 | 6 | 48 49 50 51 52 53 54 55
     * 6 | 5 | 40 41 42 43 44 45 46 47
     * 5 | 4 | 32 33 34 35 36 37 38 39
     * 4 | 3 | 24 25 26 27 28 29 30 31
     * 3 | 2 | 16 17 18 19 20 21 22 23
     * 2 | 1 |  8  9 10 11 12 13 14 15
     * 1 | 0 |  0  1  2  3  4  5  6  7
     *       *------------------------
     *        0  1  2  3  4  5  6  7  # file index = square % 8
     *        a  b  c  d  e  f  g  h
     */
    inline constexpr Bitboard file_bb(const char file) { return FILEA_BB << (file - 'a'); }
    inline constexpr Bitboard rank_bb(const int rank) { return RANK1_BB << (rank - 1) * 8; }
    inline constexpr uint8_t rank_ind_of(const Square sq) { return sq / 8; }
    inline constexpr uint8_t file_ind_of(const Square sq) { return sq % 8; }

    constexpr Bitboard EDGES_BB = file_bb('a') | file_bb('h') | rank_bb(1) | rank_bb(8);

    namespace Dir {
        enum : int {
            NW = 7, N = 8, NE = 9,
            W = -1, NONE = 0, E = 1,
            SW = -9, S = -8, SE = -7,
        };

        constexpr std::array<int, 8> KNIGHT_OFFSETS = {{15, 17, 6, 10, -10, -6, -17, -15}};
    }

    void print_bb(const Bitboard b);

    inline constexpr Square make_square(char file, uint8_t rank) { return (rank - 1) * 8 + file - 'a'; }
    inline std::string sq_to_str(const Square s) {
        return std::string{static_cast<char>('a' + s % 8), static_cast<char>('1' + s / 8)};
    }

    inline constexpr Bitboard to_bitboard(const Square s) { return 1ULL << s; }
    inline constexpr Bitboard bb_rank_file(char file, int rank) { return to_bitboard(make_square(file, rank)); }

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

    enum Side : uint8_t {
        BLACK_SIDE = 0, WHITE_SIDE = 1, NUM_SIDES
    };

    inline constexpr Side opposite_side(const Side s) { return static_cast<Side>((uint8_t) s ^ 1); }

    inline constexpr ColoredType make_ColoredType(const Type t, const Side s) {
        return static_cast<ColoredType>((int) t | (s == WHITE_SIDE ? 8 : 0));
    }

    inline constexpr Side side_of(const ColoredType p) { return (static_cast<uint8_t>(p) & 8) ? WHITE_SIDE : BLACK_SIDE; }
    inline constexpr Type type_of(const ColoredType p) { return static_cast<Type>(p & 0b111); }

    inline char ct_to_char(const ColoredType t) {
        return type_to_char(type_of(t)) + 
               (side_of(t) == BLACK_SIDE ? 'a' - 'A' : 0); // make lowercase if black
    }

    typedef uint8_t CastlingRights;

    // rule: shift left 2 if white
    constexpr uint8_t KINGSIDE_MASK =  0b10;
    constexpr uint8_t QUEENSIDE_MASK = 0b01;

    enum PromoteType : uint8_t {
        PROMOTE_QUEEN = 0, PROMOTE_ROOK, PROMOTE_BISHOP, PROMOTE_KNIGHT
    };

    // squares to check if we are under attack in for castling
    // 0 = black queenside 1 = black kingside
    // 2 = white queenside 3 = white kingside
    constexpr Bitboard CASTLING_ATTACK_MASKS[] = {
        bb_rank_file('c', 8) | bb_rank_file('d', 8), // black queenside
        bb_rank_file('f', 8) | bb_rank_file('g', 8), // black kingside
        bb_rank_file('c', 1) | bb_rank_file('d', 1), // white queenside
        bb_rank_file('f', 1) | bb_rank_file('g', 1) // white kingside
    };

    constexpr Bitboard CASTLING_OCCUPANCY_MASKS[] = {
        bb_rank_file('b', 8) | CASTLING_ATTACK_MASKS[0],
        CASTLING_ATTACK_MASKS[1],
        bb_rank_file('b', 1) | CASTLING_ATTACK_MASKS[2],
        CASTLING_ATTACK_MASKS[3]
    };

    enum MoveType : uint8_t {
        NORMAL = 0, PROMOTION, EN_PASSANT, CASTLE
    };

    // see https://github.com/official-stockfish/Stockfish/blob/0a318cdddf8b6bdd05c2e0ee9b3b61a031d398ed/src/types.h#L112
    struct Move {
        Square src : 6;
        Square dst : 6;
        PromoteType promote : 2;
        MoveType typeFlags : 2;

        inline std::string long_alg_notation() const {
            std::string ret = sq_to_str(src) + sq_to_str(dst);
            if (typeFlags == PROMOTION) ret += type_to_char(static_cast<Type>((int) promote + 2));
            return ret;
        }
    };

    template <MoveType TYPE>
    constexpr inline Move make_move(const Square from, const Square to) { return Move{from, to, PROMOTE_QUEEN, TYPE}; }
    constexpr inline Move make_normal(const Square from, const Square to) { return make_move<NORMAL>(from, to); }
    constexpr inline Move make_promotion(const Square from, const Square to, const PromoteType promote) { return Move{from, to, promote, PROMOTION}; }

    // info about a position that we would like to store separately for move undo
    struct StateInfo {
        // Bitboard pinned[NUM_SIDES]; // pinned pieces

        CastlingRights castlingRights;
        Square enPassantTarget = NULL_SQUARE;
        ColoredType capturedPiece = NULL_COLORED_TYPE;
    };

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

        constexpr inline Bitboard by_side(const Side c) const { return byColor[c]; }
        constexpr inline Bitboard by_type(const Type t) const { return byType[t]; }

    public:
        int halfmoves = 0; // number of plies since a capture or pawn advance
        int fullmoves = 1; // increment every time black moves

        ColoredType pieces[BOARD_SIZE];
        Bitboard byColor[NUM_SIDES]; // black = 0 white = 1
        Bitboard byType[NUM_UNCOLORED_PIECE_TYPES];

        StateInfo state;
        Side turn = WHITE_SIDE;
    };

    // generates a random number with a seed input, which it updates in place
    // see xorshift64. Useful for deterministic results when selecting magics.
    constexpr inline uint64_t rand_u64(uint64_t &x) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return x;
    }

    // returns the index of the least significant bit and sets it to 0
    constexpr inline int pop_lsb(uint64_t &x) {
        auto ret = std::__countr_zero(x);
        x ^= (1ULL << ret); // flip that bit (sets to 0)
        return ret;
    }

}
