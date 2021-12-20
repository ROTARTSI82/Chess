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

    // generates a random number with a seed input, which it updates in place
    // see xorshift64. Useful for deterministic results when selecting magics.
    constexpr inline uint64_t rand_u64(uint64_t &x) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return x;
    }

    // returns the index of the least significant bit and sets it to 0
    static inline constexpr int pop_lsb(uint64_t &x) {
        auto ret = std::__countr_zero(x);
        x ^= (1ULL << ret); // flip that bit (sets to 0)
        return ret;
    }


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
                while (!does_wrap(cursor, dir)) {
                    cursor += dir;
                    ROOK_MAGICS[sq].mask |= to_bitboard(cursor);
                }
            }

            BISHOP_MAGICS[sq].mask = 0;
            for (int dir : {Dir::NE, Dir::NW, Dir::SE, Dir::SW}) {
                int cursor = sq;
                while (!does_wrap(cursor, dir)) {
                    cursor += dir;
                    BISHOP_MAGICS[sq].mask |= to_bitboard(cursor);
                }
            }

            init_magics<BISHOP_MAGICS>(sq, {Dir::NE, Dir::NW, Dir::SE, Dir::SW});
            init_magics<ROOK_MAGICS>(sq, {Dir::N, Dir::S, Dir::E, Dir::W});
        }
    }

    int extractBits(uint64_t from, const std::vector<int> &bits);

    // rng seed for init_magics
    static uint64_t seed = 0x094409fce6bf3211;

    template <Magic *TABLE>
    void init_magics(const Square sq, const std::initializer_list<int> &directions) {
            // occupancy of board edges don't affect the squares we're attacking at all
            TABLE[sq].mask &= ~EDGES_BB;

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

            // std::cout << "seed = " << seed << '\n';
            // std::cout << "[" << sq_to_str(sq) << "] Checking " << (int) tableSize << std::endl;

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

                std::cout << "Y[" << sq_to_str(sq) << "] MAGIC: " << TABLE[sq].magic << std::endl;
                return; // we found a magic that works!

                outerLoopContinue:;
            }

            std::cout << "N[" << sq_to_str(sq) << "] FAILS\n";

    }

    int extractBits(uint64_t from, const std::vector<int> &bits) {
        int ret = 0;

        // loop through most significant to least significant so that
        // the least significant bit in the key corresponds to the least significant bit on the mask `from`
        for (auto it = bits.rbegin(); it != bits.rend(); it++) {
            ret <<= 1;
            if (from & to_bitboard(*it)) ret |= 1;
        }

        return ret;
    }

    MoveList Position::legal_moves() {
        return MoveList{};
    }
}