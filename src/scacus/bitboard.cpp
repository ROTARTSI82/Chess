#include "scacus/bitboard.hpp"

#include <exception>
#include <stdexcept>

namespace sc {

    void print_bb(const Bitboard b) {
        Square cursor = 0;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                std::cout << '\t' << (b & to_bitboard(cursor++) ? '1' : '0');
            }
            std::cout << '\n';
        }
    }

    static inline bool is_number(const char c) {
        return '0' <= c && c <= '9';
    }

    static inline bool is_fen_char(const char c) {
        switch (tolower(c)) {
            case 'k': case 'q': case 'r':
            case 'b': case 'n': case 'p':
                return true;
            default:
                return is_number(c);
        }
    }

    Type type_from_char(const char c) {
        switch (tolower(c)) {
        case 'k':
            return KING;
        case 'q':
            return QUEEN;
        case 'r':
            return ROOK;
        case 'b':
            return BISHOP;
        case 'n':
            return KNIGHT;
        case 'p':
            return PAWN;
        }
        throw std::runtime_error{"Invalid char passed to coloredTypeFromChar"};
    }

    Position::Position(const std::string &fen, int *store) {
        for (int i = 0; i < BOARD_SIZE; i++) pieces[i] = NULL_COLORED_TYPE;
        for (int i = 0; i < NUM_UNCOLORED_PIECE_TYPES; i++) byType[i] = 0;
        for (int i = 0; i < NUM_SIDES; i++) byColor[i] = 0;

        int i = -1;

        // find the first fen char
        while (!is_fen_char(fen.at(++i)));

        Square cursor = 0;
        while (cursor < 64) {
            char c = fen.at(i++);
            if (is_number(c))
                cursor += c - '0';
            else if (c == '/') continue;
            else
                set(cursor++, type_from_char(c), isupper(c) ? WHITE_SIDE : BLACK_SIDE);
        }

        i++; // skip the space
        turn = fen.at(i++) == 'w' ? WHITE_SIDE : BLACK_SIDE;

        for (int s = 0; s < NUM_SIDES; s++) for (int mode = 0; mode < NUM_CASTLES; mode++) castlingRights[s][mode] = false;
        char castling = fen.at(++i); // skip space and get next char
        if (castling != '-') {
            while (fen.at(i) != ' ') {
                char c = fen.at(i++);
                castlingRights[isupper(c) ? WHITE_SIDE : BLACK_SIDE][tolower(c) == 'k' ? KINGSIDE : QUEENSIDE] = true;
            }
        }

        if (fen.at(++i) != '-') { // skip over space and get next char
            enPassantTarget = make_square(fen.at(i), fen.at(i + 1) - '0');
            i++;
        } else {
            enPassantTarget = NULL_SQUARE;
        }

        i += 2; // skip both '-' AND space

        std::size_t offset;
        halfmoves = std::stoi(fen.substr(i), &offset);
        fullmoves = std::stoi(fen.substr(i + offset), &offset);
        if (store) *store = i + offset;
    }

    std::string Position::get_fen() {
        // assuming 3-digit fullmoves because yeah
        constexpr auto LONGEST_LEGAL_FEN = 88;

        std::string ret;
        ret.reserve(LONGEST_LEGAL_FEN);

        int emptySpaces = 0;

        for (uint8_t row = 0; row < 8; row++) {
            for (uint8_t col = 0; col < 8; col++) {
                const auto value = pieces[make_square(row, col)];
                if (value == NULL_COLORED_TYPE) emptySpaces++;
                else {
                    if (emptySpaces) {
                        ret += std::to_string(emptySpaces);
                        emptySpaces = 0;
                    }
                    ret += ct_to_char(value);
                }
            }

            if (emptySpaces) ret += std::to_string(emptySpaces);
            emptySpaces = 0;

            // omit appending / for the last one
            if (row != 7) ret += '/';
        }

        ret += ' ';
        ret += (turn == WHITE_SIDE) ? 'w' : 'b';
        ret += ' ';

        bool nobodyCanCastle = true;
        for (int side = 0; side < NUM_SIDES; side++)
            for (int castleMode = 0; castleMode < NUM_CASTLES; castleMode++)
                if (castlingRights[side][castleMode]) {
                    nobodyCanCastle = false;
                    ret += ((castleMode == KINGSIDE ? 'K' : 'Q') 
                           + (side == BLACK_SIDE ? 'a' - 'A' : 0)); // make lowercase for black
                }
        if (nobodyCanCastle) ret += '-';

        ret += ' ';
        ret += (enPassantTarget != NULL_SQUARE ? sq_to_str(enPassantTarget) : "-");
        ret += ' ';
        ret += std::to_string(halfmoves);
        ret += ' ';
        ret += std::to_string(fullmoves);
        return ret;
    }
}
