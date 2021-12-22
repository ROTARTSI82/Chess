#include "scacus/movegen.hpp"
#include "scacus/bitboard.hpp"


namespace sc {

    Bitboard KNIGHT_MOVES[BOARD_SIZE];
    Bitboard PAWN_ATTACKS[NUM_SIDES][BOARD_SIZE];
    Bitboard PAWN_MOVES[NUM_SIDES][BOARD_SIZE];
    Bitboard KING_MOVES[BOARD_SIZE];

    Magic ROOK_MAGICS[BOARD_SIZE];
    Magic BISHOP_MAGICS[BOARD_SIZE];

    template <Magic *TABLE>
    void init_magics(const Square sq, const std::initializer_list<int> &directions);


    // checks if moving in `direction` from square `s` would take you off of the board
    // by seeing if the file number jumps by more than 2
    static bool does_wrap(Square s, int direction) {
        int nsq = s + direction;
        return nsq < 0|| nsq >= 64 || std::abs(file_ind_of(nsq) - file_ind_of(s)) > 2;
    }

    // this code doesn't have to be efficient as it's only run once at startup
    // so i've gone and done everything the lazy way!
    void init_movegen() {
        for (Square sq = 0; sq < BOARD_SIZE; sq++) {
            int rank = rank_ind_of(sq);

            KING_MOVES[sq] = 0;

            for (int dir : {Dir::NW, Dir::N, Dir::NE, Dir::W, Dir::E, Dir::SW, Dir::S, Dir::SE})
                if (!does_wrap(sq, dir)) KING_MOVES[sq] |= to_bitboard(sq + dir);


            // this will give dumb results for 1st and 8th ranks but pawns there are an illegal position anyways
            PAWN_MOVES[WHITE_SIDE][sq] = to_bitboard(sq + Dir::N);
            PAWN_MOVES[BLACK_SIDE][sq] = to_bitboard(sq + Dir::S);

            if (rank == 1) PAWN_MOVES[WHITE_SIDE][sq] |= to_bitboard(sq + Dir::N * 2);
            if (rank == 6) PAWN_MOVES[BLACK_SIDE][sq] |= to_bitboard(sq + Dir::S * 2);

            PAWN_ATTACKS[WHITE_SIDE][sq] = 0;
            for (int dir : {Dir::NW, Dir::NE})
                if (!does_wrap(sq, dir)) PAWN_ATTACKS[WHITE_SIDE][sq] |= to_bitboard(sq + dir);

            PAWN_ATTACKS[BLACK_SIDE][sq] = 0;
            for (int dir : {Dir::SW, Dir::SE})
                if (!does_wrap(sq, dir)) PAWN_ATTACKS[BLACK_SIDE][sq] |= to_bitboard(sq + dir);


            KNIGHT_MOVES[sq] = 0;
            for (int dir : Dir::KNIGHT_OFFSETS) {
                if (!does_wrap(sq, dir))
                    KNIGHT_MOVES[sq] |= to_bitboard(sq + dir);
            }


            // Now for the MAGIC!

            ROOK_MAGICS[sq].mask = 0;
            for (int dir : {Dir::E, Dir::N, Dir::W, Dir::S}) {
                int cursor = sq;
                while (!does_wrap(cursor, 2 * dir)) {
                    cursor += dir;
                    ROOK_MAGICS[sq].mask |= to_bitboard(cursor);
                }
            }

            BISHOP_MAGICS[sq].mask = 0;
            for (int dir : {Dir::NE, Dir::NW, Dir::SE, Dir::SW}) {
                int cursor = sq;
                while (!does_wrap(cursor, 2 * dir)) {
                    cursor += dir;
                    BISHOP_MAGICS[sq].mask |= to_bitboard(cursor);
                }
            }

            init_magics<BISHOP_MAGICS>(sq, {Dir::NE, Dir::NW, Dir::SE, Dir::SW});
            init_magics<ROOK_MAGICS>(sq, {Dir::N, Dir::S, Dir::E, Dir::W});
        }
    }

    // rng seed for init_magics
    static uint64_t seed = 0x094409fce6bf3211;

    template <Magic *TABLE>
    void init_magics(const Square sq, const std::initializer_list<int> &directions) {
        auto numBits = popcnt(TABLE[sq].mask);
        TABLE[sq].shift = (64 - numBits);

        // 2^numBits possible hash keys because we have rookBits of occupancy info
        auto tableSize = 1 << numBits;
        TABLE[sq].table = new uint64_t[tableSize];

        // list of the indices of the bits we want to extract, sorted
        // from least significant to most significant.
        // the least significant bit is said to have index 0 and the most significant has index 63
        std::vector<int> bitsToExtract;

        uint64_t mask = TABLE[sq].mask;
        while (mask)
            bitsToExtract.push_back(pop_lsb(mask));


        // occupancy masks corresponding to each index
        Bitboard occupancyBBs[4096];
        
        // table of attack masks indexed strictly in the correct order.
        // the real table may have constructive collisions and the index generated by the magic
        // may not be the "correct" index.
        Bitboard attackMaskStrictOrder[4096];

        // generate bitboards corresponding to each key in the table
        // and generate the table entries themselves
        for (int current = 0; current < tableSize; current++) {
            // now we need to turn the bits in `current` (a key into the table) into a bitboard,
            // the reverse process of lookup<TABLE>(...)
            // this part does the same thing as index_to_uint64() in https://www.chessprogramming.org/Looking_for_Magics
            int selectedMask = 1;
            Bitboard occupancy = 0;
            for (const auto bit : bitsToExtract) {
                if (current & selectedMask) occupancy |= (1ULL << bit);
                selectedMask <<= 1;
            }

            // cache this for use in checking the correctness of a magic
            occupancyBBs[current] = occupancy;

            Bitboard moveMask = 0;
            for (int dir : directions) {
                int sqCursor = sq;
                while (!does_wrap(sqCursor, dir)) {
                    sqCursor += dir;
                    
                    Bitboard sqBB = to_bitboard(sqCursor);
                    moveMask |= sqBB;
                    if (occupancy & sqBB) 
                        break; // don't go any futher; we are blocked!
                }
            }
            
            TABLE[sq].table[current] = EVERYTHING_BB;
            attackMaskStrictOrder[current] = moveMask;
        }
        
        for (int attemptNo = 0; attemptNo < 100000000; attemptNo++) {
            TABLE[sq].magic = rand_u64(seed) & rand_u64(seed) & rand_u64(seed);

            // not enough bits at the top when trying all occupancy bits set. can't possibly work
            if (popcnt((TABLE[sq].mask * TABLE[sq].magic) & rank_bb(8)) < 6) continue;

            // clear the table as its values were generated using the old, bad magic
            // here, EVERYTHING_BB is used as a sentinel value
            for (int k = 0; k < tableSize; k++) TABLE[sq].table[k] = EVERYTHING_BB;

            for (int current = tableSize - 1; current >= 0; current--) {
                // We WANT constructive collisions where
                // a magic refers you to an entry for a different occupancy situation
                // that happens to have the same resulting attack mask! This can happen when
                // only the occupancy behind a blocker changes.
                // Also note that the index it stores the entry in can be arbitrary
                // and isn't necessarily the "correct" one used in `attackMaskStrictOrder`
                auto lookIndex = occupancy_to_index<TABLE>(sq, occupancyBBs[current]);
                if (TABLE[sq].table[lookIndex] == EVERYTHING_BB) 
                    TABLE[sq].table[lookIndex] = attackMaskStrictOrder[current];
                else if (TABLE[sq].table[lookIndex] != attackMaskStrictOrder[current]) 
                    goto outerLoopContinue; // horrible hacky continue. this magic doesn't work
            }

            return; // we found a magic that works!

            outerLoopContinue:;
        }

        throw std::runtime_error{"Magic generation failed!"};
    }

    inline constexpr std::pair<Square, Square> castle_info(const Move mov) {
        Square targetRook = 0, rookNewDst = 0;
        if ((int) mov.dst - (int) mov.src > 0) { // kingside castling
            targetRook = mov.src + 3 * Dir::E;
            rookNewDst = mov.dst + Dir::W;
        } else {
            rookNewDst = mov.dst + Dir::E;
            targetRook = mov.src + 4 * Dir::W;
        }
        return std::make_pair(targetRook, rookNewDst);
    }

    StateInfo make_move(Position &pos, const Move mov) {
        StateInfo ret = pos.state;

        pos.state.enPassantTarget = NULL_SQUARE;

        const auto remove_castling_rights = [&](const Square rookSq, Side side) {
            const auto file = file_ind_of(rookSq);
            const auto rank = rank_ind_of(rookSq);

            // this rook is not on a square of a castling rook, so it does not affect castling rights.
            if ((file != 0 && file != 7) || (rank != 0 && rank != 7)) return;

            CastlingRights mask = file == 0 ? QUEENSIDE_MASK : KINGSIDE_MASK;

            if (side == WHITE_SIDE) mask <<= 2;
            pos.state.castlingRights &= ~mask;
        };

        const auto completely_forbid_castling = [&]() {
            pos.state.castlingRights &= ~((KINGSIDE_MASK | QUEENSIDE_MASK) << (pos.turn == WHITE_SIDE ? 2 : 0));
        };

        switch (mov.typeFlags) {
        case NORMAL: {
            pos.state.capturedPiece = pos.pieces[mov.dst];

            Type movedType = type_of(pos.pieces[mov.src]);
            if (movedType == 0) { 
                dbg_dump_position(pos); 
                std::cout << mov.long_alg_notation() << '\n';
                std::cout << (int) mov.typeFlags << '\n';
                throw std::runtime_error{"move from null"}; 
            }

            pos.clear(mov.dst);
            pos.set(mov.dst, movedType, pos.turn);
            pos.clear(mov.src);

            switch (movedType) {
            case PAWN: 
                if (std::abs((int) mov.dst - (int) mov.src) == Dir::N * 2) {
                    pos.state.enPassantTarget = mov.dst + (pos.turn == WHITE_SIDE ? Dir::S : Dir::N);
                    // std::cout << "ADD EP TARG " << (int) pos.state.enPassantTarget << '\n';
                }
                break;
            case KING:
                completely_forbid_castling();
                break;
            case ROOK: {
                remove_castling_rights(mov.src, pos.turn);
                break;
            }
            default:
                break; // do nothing.
            }
            break;
        }
        case CASTLE: {
            pos.set(mov.dst, KING, pos.turn);
            pos.clear(mov.src);

            Square targetRook, rookNewDst;
            std::tie(targetRook, rookNewDst) = castle_info(mov);

            pos.clear(targetRook);
            pos.set(rookNewDst, ROOK, pos.turn);

            completely_forbid_castling();
            break;
        }
        case EN_PASSANT: {
            // use enPassantTarget from ret: pos.state.enPassant target has already been set to null.
            Square capturedPawn = ret.enPassantTarget + (pos.turn == WHITE_SIDE ? Dir::S : Dir::N);
            // std::cout << "domove CapturedPawn = " << (int) capturedPawn << std::endl;
            pos.state.capturedPiece = pos.pieces[capturedPawn];
            pos.clear(capturedPawn);
            pos.clear(mov.src);
            pos.set(mov.dst, PAWN, pos.turn);
            break;
        }
        case PROMOTION:
            pos.state.capturedPiece = pos.pieces[mov.dst];
            pos.clear(mov.dst);
            pos.clear(mov.src);
            pos.set(mov.dst, static_cast<Type>((int) mov.promote + 2), pos.turn);
            break;
        }

        if (type_of(pos.state.capturedPiece) == ROOK)
            remove_castling_rights(mov.dst, side_of(pos.state.capturedPiece));

        pos.turn = opposite_side(pos.turn);
        return ret;
    }

    void unmake_move(Position &pos, const StateInfo &info, const Move mov) {
        pos.turn = opposite_side(pos.turn);

        switch (mov.typeFlags) {
        case NORMAL:
            pos.set(mov.src, type_of(pos.pieces[mov.dst]), pos.turn);
            pos.clear(mov.dst);
            if (pos.state.capturedPiece != NULL_COLORED_TYPE)
                pos.set(mov.dst, type_of(pos.state.capturedPiece), side_of(pos.state.capturedPiece));
            break;
        case PROMOTION:
            pos.set(mov.src, PAWN, pos.turn);
            pos.clear(mov.dst);
            if (pos.state.capturedPiece != NULL_COLORED_TYPE)
                pos.set(mov.dst, type_of(pos.state.capturedPiece), side_of(pos.state.capturedPiece));
            break;
        case EN_PASSANT: {
            Square captureSq = info.enPassantTarget + (pos.turn == WHITE_SIDE ? Dir::S : Dir::N);
            // std::cout << "undomove captureSq = " << (int) captureSq << std::endl;
            pos.set(captureSq, PAWN, opposite_side(pos.turn));
            pos.clear(mov.dst);
            pos.set(mov.src, PAWN, pos.turn);
            break;
        }
        case CASTLE:
            pos.set(mov.src, KING, pos.turn);
            pos.clear(mov.dst);

            Square targetRook, rookNewDst;
            std::tie(targetRook, rookNewDst) = castle_info(mov);

            pos.clear(rookNewDst);
            pos.set(targetRook, ROOK, pos.turn);
            break;
        }

        pos.state = info;
    }
}