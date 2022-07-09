#pragma once

#include "scacus/bitboard.hpp"

#include <functional>

namespace sc {
    extern Bitboard KNIGHT_MOVES[BOARD_SIZE];
    extern Bitboard PAWN_ATTACKS[NUM_SIDES][BOARD_SIZE];
    extern Bitboard PAWN_MOVES[NUM_SIDES][BOARD_SIZE];
    extern Bitboard KING_MOVES[BOARD_SIZE];
    extern Bitboard PIN_LINE[BOARD_SIZE][BOARD_SIZE];

    inline constexpr Bitboard pawn_attacks(Side side, Square sq) {
        return PAWN_ATTACKS[side][sq];
    }

    inline constexpr Bitboard king_moves(Square sq) {
        return KING_MOVES[sq];
    }

    inline constexpr Bitboard knight_moves(Square sq) {
        return KNIGHT_MOVES[sq];
    }

    inline constexpr Bitboard pin_line(Square king, Square pinner) {
        return PIN_LINE[king][pinner];
    }


    // https://www.chessprogramming.org/Magic_Bitboards
    struct Magic {
        Bitboard *table = nullptr;
        Bitboard mask = 0;  // to mask relevant squares of both lines (no outer squares)
        Bitboard magic = 0; // magic 64-bit factor to multiply by
        uint8_t shift = 0; // shift right

        ~Magic() {
            delete[] table;
        }
    };

    extern Magic ROOK_MAGICS[BOARD_SIZE];
    extern Magic BISHOP_MAGICS[BOARD_SIZE];

    // TODO: Consider using _pext_u64?

    template <Magic *TABLE>
    constexpr inline int occupancy_to_index(const Square sq, const uint64_t occupancy) {
        auto &entry = TABLE[sq]; return ((entry.mask & occupancy) * entry.magic) >> entry.shift;
    }

    template <Magic *TABLE>
    constexpr inline uint64_t lookup(const Square square, const uint64_t occupancy) {
        return TABLE[square].table[occupancy_to_index<TABLE>(square, occupancy)];
    }

    // returns a bitboard of the squares attacked if the board were empty.
    template <Magic *TABLE>
    constexpr inline uint64_t pseudo_attacks(const Square sq) {
        // stockfish uses a separate table for this? would that be because of cache shananigans?
        return TABLE[sq].table[0]; 
    }

    void init_movegen();

    class MoveList {
    public:
        Move *head = nullptr;
        Move *tail = nullptr;

        inline MoveList() = delete;

        MoveList &operator=(const MoveList &&rhs) noexcept = delete;;
        MoveList(const MoveList &rhs) noexcept = delete;

        inline explicit MoveList(int) : head(new Move[4096]), tail(head) {};

        inline ~MoveList() {
            delete[] head;
        }

        inline MoveList &operator=(MoveList &&rhs) noexcept {
            // skipping same-identity check
            // if (&rhs == this) return *this;

            head = rhs.head;
            tail = rhs.tail;
            rhs.head = nullptr;
            return *this;
        }

        inline MoveList(MoveList &&rhs) noexcept : head(rhs.head), tail(rhs.tail) {
            rhs.head = nullptr;
        }

        inline void push_back(const Move &mov) {
            *tail++ = mov;
        }

        inline void push_back(Move &&mov) {
            *tail++ = mov;
        }

        [[nodiscard]] inline bool empty() const {
            return tail == head;
        }

        [[nodiscard]] inline std::size_t size() const {
            return tail - head;
        }

        [[nodiscard]] inline Move *begin() const {
            return head;
        }

        [[nodiscard]] inline Move *end() const {
            return tail;
        }

        [[nodiscard]] inline Move &at(std::size_t x) const {
            return head[x];
        }

        inline void clear() {
            tail = head;
        }
    };

//    template <Side SIDE>
//    MoveList standard_moves(Position &pos);

//    inline MoveList legal_moves_from(Position &pos) {
//        return pos.turn == WHITE_SIDE ? standard_moves<WHITE_SIDE>(pos) : standard_moves<BLACK_SIDE>(pos);
//    }

//    StateInfo make_move(Position &pos, const Move mov);
//    void unmake_move(Position &pos, const StateInfo &info, const Move mov);
}


#include "scacus/impl/std_moves.hpp"
#include "scacus/impl/make_unmake.hpp"

