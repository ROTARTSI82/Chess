#include "scacus/bitboard.hpp"

#include <exception>
#include <stdexcept>

#include <array>

namespace sc {

    void print_bb(const Bitboard b) {
        for (int rank = 8; rank > 0; rank--) {
            for (char file = 'a'; file < 'i'; file++) {
                std::cout << ' ' << (b & to_bitboard(make_square(file, rank)) ? 'X' : '.');
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

        uint8_t cursor = 0;
        while (cursor < 64) {
            char c = fen.at(i++);
            if (is_number(c))
                cursor += c - '0';
            else if (c == '/') continue;
            else {
                Square toSet = 56 - cursor/8*8 + cursor%8;
                set(toSet, type_from_char(c), isupper(c) ? WHITE_SIDE : BLACK_SIDE);
                cursor++;
            }
        }

        i++; // skip the space
        turn = fen.at(i++) == 'w' ? WHITE_SIDE : BLACK_SIDE;

        castlingRights = 0;
        char castling = fen.at(++i); // skip space and get next char
        if (castling != '-') {
            while (fen.at(i) != ' ') {
                char c = fen.at(i++);
                uint8_t whichBit = tolower(c) == 'k' ? KINGSIDE_MASK : QUEENSIDE_MASK;
                if (isupper(c)) whichBit <<= 2; // white gets left shifted
                castlingRights |= whichBit;
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

        for (uint8_t rank = 8; rank > 0; rank--) {
            for (char file = 'a'; file < 'i'; file++) {
                const auto value = pieces[make_square(file, rank)];
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
            if (rank != 1) ret += '/';
        }

        ret += ' ';
        ret += (turn == WHITE_SIDE) ? 'w' : 'b';
        ret += ' ';

        enum CastlingType {
            BLACK_QUEENSIDE = QUEENSIDE_MASK, BLACK_KINGSIDE = KINGSIDE_MASK,
            WHITE_QUEENSIDE = QUEENSIDE_MASK << 2, WHITE_KINGSIDE = KINGSIDE_MASK << 2
        };

        bool nobodyCanCastle = true;
        constexpr std::array<std::pair<uint8_t, char>, 4> castleTable = {{
            {WHITE_KINGSIDE, 'K'}, {WHITE_QUEENSIDE, 'Q'}, {BLACK_KINGSIDE, 'k'}, {BLACK_QUEENSIDE, 'q'},
        }};

        for (const auto &way : castleTable)
            if (castlingRights & way.first) { ret += way.second; nobodyCanCastle = false; }
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
