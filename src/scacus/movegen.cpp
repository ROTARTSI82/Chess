#include "scacus/movegen.hpp"
#include "scacus/bitboard.hpp"

namespace {
    // stolen from Stockfish
    sc::Bitboard RookTable[0x19000];  // To store rook attacks
    sc::Bitboard BishopTable[0x1480]; // To store bishop attacks
}

namespace sc {
    // checks if moving in `direction` from square `s` would take you off of the board
    // by seeing if the file number jumps by more than 2
    [[nodiscard]] static inline constexpr bool does_wrap(Square s, int direction) {
        int nsq = s + direction;
        return nsq < 0|| nsq >= 64 || std::abs(file_ind_of(nsq) - file_ind_of(s)) > 2;
    }

    // this code doesn't have to be efficient as it's only run once at startup
    // so i've gone and done everything the lazy way!
    MovegenInfo::MovegenInfo() {
        std::cout << "MOVEGEN INFO CTOR\n";

        {
            for (auto &i: zob.enPassantFile)
                i = rand_u64(seed);
            for (auto &zob_CastlingRight: zob.castlingRights)
                zob_CastlingRight = rand_u64(seed);

            for (auto &zob_Piece: zob.pieces)
                for (auto &j: zob_Piece)
                    j = rand_u64(seed);
        }

        std::cout << "INIT " << tableLoc[0] << "\t" << tableLoc[1] << '\n';

        for (Square sq = 0; sq < BOARD_SIZE; sq++) {
            int rank = rank_ind_of(sq);

            KING_MOVES[sq] = 0;

            for (Square dst = 0; dst < BOARD_SIZE; dst++)
                PIN_LINE[sq][dst] = to_bitboard(dst);

            for (int dir : {Dir::NW, Dir::N, Dir::NE, Dir::W, Dir::E, Dir::SW, Dir::S, Dir::SE}) {
                if (!does_wrap(sq, dir)) KING_MOVES[sq] |= to_bitboard(sq + dir); // generate 1-square king move

                Square it = sq;
                Bitboard bb = 0;
                while (!does_wrap(it, dir))
                    PIN_LINE[sq][it] = bb |= to_bitboard(it += dir);
            }


            // this will give dumb results for 1st and 8th ranks but pawns they're an illegal position anyways
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

            std::cout << "CTOR FOR " << sq_to_str(sq) << '\n';
            init_magics(sq, false);
            init_magics(sq, true);
        }
        std::cout << "=========== [EXIT MOVEGEN INFO CTOR] ========\n";
    }


    void MovegenInfo::init_magics(const Square sq, bool isBishop) {
        // std::cout << "init_magics(" << sq_to_str(sq) << ", " << isBishop << ")\n";

        auto &TABLE = isBishop ? BISHOP_MAGICS : ROOK_MAGICS;
        const auto directions = isBishop ?
                                std::initializer_list<int>{Dir::NE, Dir::NW, Dir::SE, Dir::SW} :
                                std::initializer_list<int>{Dir::N, Dir::S, Dir::E, Dir::W};

        auto numBits = popcnt(TABLE[sq].mask);
        TABLE[sq].shift = (64 - numBits);

        // 2^numBits possible hash keys because we have rookBits of occupancy info
        auto tableSize = 1 << numBits;
        //TABLE[sq].table = new Bitboard[tableSize];
        TABLE[sq].table = (isBishop ? BishopTable : RookTable) + tableLoc[isBishop?1:0];
        tableLoc[isBishop?1:0] += tableSize;

        if (tableLoc[1] > 0x1480)
            std::cerr << "BISHOP OVERFLOW, tableSize = " << tableSize << ", loc = " << tableLoc[1] << "\n";
        if (tableLoc[0] > 0x19000)
            std::cerr << "ROOK OVERFLOW, tableSize = " << tableSize << ", loc = " << tableLoc[0] << "\n";

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
            for (const auto bit: bitsToExtract) {
                if (current & selectedMask) occupancy |= (1ULL << bit);
                selectedMask <<= 1;
            }

            // cache this for use in checking the correctness of a magic
            occupancyBBs[current] = occupancy;

            Bitboard moveMask = 0;
            for (int dir: directions) {
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
                auto lookIndex = TABLE[sq].index(occupancyBBs[current]);
                if (TABLE[sq].table[lookIndex] == EVERYTHING_BB)
                    TABLE[sq].table[lookIndex] = attackMaskStrictOrder[current];
                else if (TABLE[sq].table[lookIndex] != attackMaskStrictOrder[current])
                    goto outerLoopContinue; // horrible hacky continue. this magic doesn't work
            }

            return; // we found a magic that works!

            outerLoopContinue:;
        }
        std::cerr << "ERROR! MAGIC GEN FAILED\n";

        UNDEFINED();
    }
}
