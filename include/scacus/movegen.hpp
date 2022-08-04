#pragma once

#include "scacus/move_list.hpp"

#include <functional>
#include <execution>

namespace sc {
    // https://www.chessprogramming.org/Magic_Bitboards
    struct Magic {
        constexpr Magic() = default;

        Bitboard *table = nullptr;
        Bitboard mask = 0;  // to mask relevant squares of both lines (no outer squares)
        Bitboard magic = 0; // magic 64-bit factor to multiply by
        uint_fast64_t shift = 0; // shift right. only a uint8_t is needed but we don't wanna screw with alignment.

        [[nodiscard]] inline constexpr uint64_t index(const Bitboard occ) const {
            // stolen from stockfish: Fancy Pext bitboards.
            // Not any faster than multiplication + shift, sadly.
            //return _pext_u64(occupancy, entry.mask);
            return ((mask & occ) * magic) >> shift;
        }
    };

    struct ZobInfo {
        uint64_t isWhiteTurn;
        uint64_t enPassantFile[8];
        uint64_t castlingRights[4];

        // TODO: This wastes a TON of memory but we don't really care, right?
        uint64_t pieces[BOARD_SIZE][NUM_COLORED_PIECE_TYPES];
    };

    enum TableTag : uint_fast8_t {
        BISHOP_TB, ROOK_TB
    };

    struct MovegenInfo {
        MovegenInfo();

        MovegenInfo(const MovegenInfo &rhs) noexcept = delete;
        MovegenInfo &operator=(const MovegenInfo &rhs) noexcept = delete;

        [[nodiscard]] inline constexpr Bitboard pawn_attk(Side a, Square b) const { return PAWN_ATTACKS[a][b]; }
        [[nodiscard]] inline constexpr Bitboard pin_line(Square a, Square b) const { return PIN_LINE[a][b]; }
        [[nodiscard]] inline constexpr Bitboard knight_attk(Square a) const { return KNIGHT_MOVES[a]; }
        [[nodiscard]] inline constexpr Bitboard pawn_mov(Side a, Square b) const { return PAWN_MOVES[a][b]; }
        [[nodiscard]] inline constexpr Bitboard king_attk(Square b) const { return KING_MOVES[b]; }
        [[nodiscard]] inline constexpr const ZobInfo &get_zob() const { return zob; }

        template <TableTag TAG>
        [[nodiscard]] inline constexpr const Magic &tb(Square sq) const {
            return TAG == ROOK_TB ? ROOK_MAGICS[sq] : BISHOP_MAGICS[sq];
        }

    private:
        Bitboard KNIGHT_MOVES[BOARD_SIZE] = {};
        Bitboard PAWN_ATTACKS[NUM_SIDES][BOARD_SIZE] = {};
        Bitboard PAWN_MOVES[NUM_SIDES][BOARD_SIZE] = {};
        Bitboard KING_MOVES[BOARD_SIZE] = {};

        // https://github.com/official-stockfish/Stockfish/blob/90cf8e7d2bde9e480aac4b119ce130e09dd2be39/src/bitboard.h#L220
        // Bitboard containing line from first square to the other, excluding the first but including the
        // second. If the two squares are not on a line, it's a bitboard of the second square.
        // Used for generating king moves in check and calculating pins.
        Bitboard PIN_LINE[BOARD_SIZE][BOARD_SIZE] = {};

        ZobInfo zob;

        Magic ROOK_MAGICS[BOARD_SIZE] = {};
        Magic BISHOP_MAGICS[BOARD_SIZE] = {};

        // table to keep track of how much of ROOK_MAGICS and BISHOP_MAGICS have been allocated so far
        // during magic initialization
        uint64_t tableLoc[2] = {0, 0};
        uint64_t seed = 0xe4e35f44eb8290d1ULL;

        void init_magics(Square sq, bool isBishop);
    };

    namespace {
        MovegenInfo movegen{};
    }


    inline constexpr Bitboard pawn_attacks_old(Side side, Square sq) {
        return movegen.pawn_attk(side, sq);
    }

    template <Side SIDE>
    inline constexpr Bitboard pawn_attacks(Square sq) {
        return movegen.pawn_attk(SIDE, sq);
    }

    template <Side SIDE>
    inline constexpr Bitboard pawn_moves(Square sq, Bitboard occ) {
        constexpr Bitboard rowMask = rank_bb(SIDE == BLACK_SIDE ? 6 : 3);
        constexpr int direction = SIDE == BLACK_SIDE ? Dir::S : Dir::N;

        const Bitboard mov = to_bitboard(sq + direction) & ~occ;
        const Bitboard shift = mov & rowMask;

        return mov | ((SIDE == BLACK_SIDE ? shift >> 8 : shift << 8) & ~occ);
    }

    inline constexpr Bitboard DEPRECATED_pawn_moves(Side side, Square sq) {
        return movegen.pawn_mov(side, sq);
    }

    inline constexpr Bitboard king_moves(Square sq) { return movegen.king_attk(sq); }
    inline constexpr Bitboard knight_moves(Square sq) { return movegen.knight_attk(sq); }
    inline constexpr Bitboard pin_line(Square king, Square pinner) { return movegen.pin_line(king, pinner); }


    // TODO: Consider using _pext_u64?
    template <TableTag TABLE>
    constexpr inline int occupancy_to_index(const Square sq, const uint64_t occupancy) {
        return movegen.tb<TABLE>(sq).index(occupancy);
    }

    template <TableTag TABLE>
    constexpr inline uint64_t lookup(const Square square, const uint64_t occupancy) {
        return movegen.tb<TABLE>(square).table[occupancy_to_index<TABLE>(square, occupancy)];
    }

    // returns a bitboard of the squares attacked if the board were empty.
    template <TableTag TABLE>
    constexpr inline uint64_t pseudo_attacks(const Square sq) {
        // stockfish uses a separate table for this? would that be because of cache shananigans?
        return movegen.tb<TABLE>(sq).table[0];
    }

    constexpr inline const ZobInfo &get_zob() {
        return movegen.get_zob();
    }

    template <Side SIDE>
    void standard_moves(MoveList &ls, Position &pos);
    extern template void standard_moves<BLACK_SIDE>(MoveList &, Position &);
    extern template void standard_moves<WHITE_SIDE>(MoveList &, Position &);

    StateInfo make_move(Position &pos, const Move mov);
    void unmake_move(Position &pos, const StateInfo &info, const Move mov);


}


