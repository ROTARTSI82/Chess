#pragma once

#include "scacus/move_list.hpp"

#include <functional>
#include <execution>

namespace sc {
    extern Bitboard KNIGHT_MOVES[BOARD_SIZE];
    extern Bitboard PAWN_ATTACKS[NUM_SIDES][BOARD_SIZE];
    extern Bitboard PAWN_MOVES[NUM_SIDES][BOARD_SIZE];
    extern Bitboard KING_MOVES[BOARD_SIZE];
    extern Bitboard PIN_LINE[BOARD_SIZE][BOARD_SIZE];

    template <Side SIDE>
    inline constexpr Bitboard pawn_attacks(Square sq) {
        return PAWN_ATTACKS[SIDE][sq];
    }

    template <Side SIDE>
    inline constexpr Bitboard pawn_moves(Square sq, Bitboard occ) {
        constexpr Bitboard rowMask = rank_bb(SIDE == BLACK_SIDE ? 6 : 3);
        constexpr int direction = SIDE == BLACK_SIDE ? Dir::S : Dir::N;

        const Bitboard mov = to_bitboard(sq + direction) & ~occ;
        const Bitboard shift = mov & rowMask;

        return mov | ((SIDE == BLACK_SIDE ? shift >> 8 : shift << 8) & ~occ);
    }

    inline Bitboard king_moves(Square sq) {
        return KING_MOVES[sq];
    }

    inline Bitboard knight_moves(Square sq) {
        return KNIGHT_MOVES[sq];
    }

    inline Bitboard pin_line(Square king, Square pinner) {
        return PIN_LINE[king][pinner];
    }


    // https://www.chessprogramming.org/Magic_Bitboards
    struct Magic {
        Bitboard *table = nullptr;
        Bitboard mask = 0;  // to mask relevant squares of both lines (no outer squares)
        Bitboard magic = 0; // magic 64-bit factor to multiply by
        uint64_t shift = 0; // shift right. only a uint8_t is needed but we don't wanna screw with alignment.
    };

    extern Magic ROOK_MAGICS[BOARD_SIZE];
    extern Magic BISHOP_MAGICS[BOARD_SIZE];

    // TODO: Consider using _pext_u64?

    template <Magic *TABLE>
    constexpr inline int occupancy_to_index(const Square sq, const uint64_t occupancy) {
        const auto &entry = TABLE[sq];

        // stolen from stockfish: Fancy Pext bitboards.
        // Not any faster than multiplication + shift, sadly.
        //return _pext_u64(occupancy, entry.mask);
        return ((entry.mask & occupancy) * entry.magic) >> entry.shift;
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

    template <Side SIDE, bool QUIESC>
    void standard_moves(MoveList &ls, Position &pos);
    extern template void standard_moves<BLACK_SIDE, false>(MoveList &, Position &);
    extern template void standard_moves<WHITE_SIDE, false>(MoveList &, Position &);
    extern template void standard_moves<BLACK_SIDE, true>(MoveList &, Position &);
    extern template void standard_moves<WHITE_SIDE, true>(MoveList &, Position &);

    // DESIGN TODO: returning a StateInfo * is no longer necessary because of the prev field in StateInfo
    void make_move(Position &pos, const Move mov);
    void unmake_move(Position &pos, const Move mov);

    template <bool QUIESC>
    inline constexpr void legal_moves_from(MoveList &ls, Position &pos) {
        if (pos.get_turn() == WHITE_SIDE)
            standard_moves<WHITE_SIDE, QUIESC>(ls, pos);
        else
            standard_moves<BLACK_SIDE, QUIESC>(ls, pos);
    }

    template <bool QUIESC>
    inline MoveList legal_moves_from(Position &pos) {
        MoveList legals(0);
        legal_moves_from<QUIESC>(legals, pos);
        return legals;
    }
}


