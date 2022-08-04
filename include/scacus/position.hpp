#pragma once

#include "scacus/movegen.hpp"

namespace sc::makeimpl {
    struct PositionFriend;
}

namespace sc {
    class Position {
    public:
        explicit Position(const std::string &fen);
        Position() : Position(std::string{STARTING_POS_FEN}) {};
        [[nodiscard]] std::string get_fen() const;

        // store: Used to store the index into the string that we read to
        void set_state_from_fen(const std::string &fen, int *store = nullptr);

        constexpr inline void set(const Square p, const Type type, const Side side) {
            pieces[p] = make_ColoredType(type, side);
            byType[type] |= to_bitboard(p);
            byColor[side] |= to_bitboard(p);
            state.hash ^= get_zob().pieces[p][pieces[p]];
        }

        constexpr inline void clear(const Square p) {
            state.hash ^= get_zob().pieces[p][pieces[p]];
            byType[type_of(pieces[p])] &= ~to_bitboard(p);
            byColor[side_of(pieces[p])] &= ~to_bitboard(p);
            pieces[p] = NULL_COLORED_TYPE;
        }

        [[nodiscard]] constexpr inline Bitboard by_side(const Side c) const { return byColor[c]; }
        [[nodiscard]] constexpr inline Bitboard by_type(const Type t) const { return byType[t]; }

        [[nodiscard]] constexpr inline const StateInfo &get_state() const { return state; }
        [[nodiscard]] constexpr inline ColoredType piece_at(const int ind) const { return pieces[ind]; }
        [[nodiscard]] constexpr inline bool in_check() const { return isInCheck; }
        [[nodiscard]] constexpr Side get_turn() const { return turn; }

    private:
        ColoredType pieces[BOARD_SIZE];
        Bitboard byColor[NUM_SIDES]; // black = 0 white = 1
        Bitboard byType[NUM_UNCOLORED_PIECE_TYPES];

//        std::unordered_map<uint64_t, int8_t> threefoldTable;

        StateInfo state;
        int fullmoves = 1; // increment every time black moves
        Side turn = WHITE_SIDE;
        bool isInCheck = false; // this flag is set by standard_moves() if it detects that the side it generated moves for is in check.

        friend StateInfo antichess_make_move(Position &pos, Move mov);
        friend void antichess_unmake_move(Position &pos, const StateInfo &info, Move mov);
        friend StateInfo make_move(Position &pos, const Move mov);
        friend void unmake_move(Position &pos, const StateInfo &info, const Move mov);
        friend struct ::sc::makeimpl::PositionFriend;

        template <Side>
        friend void standard_moves(MoveList &ls, Position &pos);
    };

    inline constexpr void legal_moves_from(MoveList &ls, Position &pos) {
        if (pos.get_turn() == WHITE_SIDE)
            standard_moves<WHITE_SIDE>(ls, pos);
        else
            standard_moves<BLACK_SIDE>(ls, pos);
    }

    inline MoveList legal_moves_from(Position &pos) {
        MoveList legals(0);
        legal_moves_from(legals, pos);
        return legals;
    }
}