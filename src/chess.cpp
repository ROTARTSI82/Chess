#include "chess.hpp"

#include <iostream>


Piece::Piece() : type(PType::NIL), isWhite(false), moveState(MoveState::NOT_MOVED), isPassant(false) {};
Piece::Piece(PType t, bool white) : type(t), isWhite(white), moveState(MoveState::NOT_MOVED), isPassant(false) {};

Move::Move(BoardPosition from, BoardPosition to) : srcCol(from.col), srcRow(from.row), dstRow(to.row), dstCol(to.col),
                                                   promoteTo(PromoteType::QUEEN), doPromote(false) {};

Move::Move(uint8_t src, uint8_t dst) : Move(BoardPosition{(uint8_t) (src % 8), (uint8_t) (src / 8)},
                                            BoardPosition{(uint8_t) (dst % 8), (uint8_t) (dst / 8)}) {};


uint64_t Board::zobristTable[64][12];

Board::Board() {
    for (int i = 0; i < 64; i++) board[i] = Piece();

    std::cout << "Init board" << std::endl;

    for (char r = 'a'; r <= 'h'; r++) {
        get(r, 2) = Piece(PType::PAWN, true);
        get(r, 7) = Piece(PType::PAWN, false);

        whitePieces.emplace(BoardPosition::fromRankFile(r, 2));
        whitePieces.emplace(BoardPosition::fromRankFile(r, 1));
        blackPieces.emplace(BoardPosition::fromRankFile(r, 7));
        blackPieces.emplace(BoardPosition::fromRankFile(r, 8));
    }

    get('a', 1) = Piece(PType::ROOK, true);
    get('b', 1) = Piece(PType::KNIGHT, true);
    get('c', 1) = Piece(PType::BISHOP, true);
    get('d', 1) = Piece(PType::QUEEN, true);
    get('e', 1) = Piece(PType::KING, true);
    get('f', 1) = Piece(PType::BISHOP, true);
    get('g', 1) = Piece(PType::KNIGHT, true);
    get('h', 1) = Piece(PType::ROOK, true);

    get('a', 8) = Piece(PType::ROOK, false);
    get('b', 8) = Piece(PType::KNIGHT, false);
    get('c', 8) = Piece(PType::BISHOP, false);
    get('d', 8) = Piece(PType::QUEEN, false);
    get('e', 8) = Piece(PType::KING, false);
    get('f', 8) = Piece(PType::BISHOP, false);
    get('g', 8) = Piece(PType::KNIGHT, false);
    get('h', 8) = Piece(PType::ROOK, false);
}

Board::~Board() {
    
}

std::string Board::getFen(bool sideWhite) {
    std::string ret;
    int emptySpaces = 0;
    for (uint8_t r = 0; r < 8; r++) {
        emptySpaces = 0;
        for (uint8_t c = 0; c < 8; c++) {
            if (!rget(r, c).exists())
                emptySpaces++;
            else {
                if (emptySpaces > 0) {
                    ret += std::to_string(emptySpaces);
                    emptySpaces = 0;
                }

                ret += rget(r, c).fenChar();
            }
        }

        if (emptySpaces > 0)
            ret += std::to_string(emptySpaces);
        ret += "/";
    }

    ret = ret.substr(0, ret.size() - 1);

    // always black's turn from engine's perspective.
    // no castling rights or en passant targets. Maybe these later?
    // always 30 halfmoves and 30 fullmoves.
    // hopefully hardcoding these don't screw the engine over.
    ret += (sideWhite ? " w" : " b") + std::string(" - - 30 30");

    return ret;
}

std::pair<Piece, MoveState> Board::make(Move mov) {
    Piece &dst = rget(mov.dstRow, mov.dstCol);
    Piece &src = rget(mov.srcRow, mov.srcCol);
    Piece oldDst = dst;
    MoveState rstate = src.moveState;
    Piece ret = Piece(); // NIL

    if (src.type == PType::PAWN) {
        int8_t diff = mov.dstRow - mov.srcRow;
        if (diff == 2 || diff == -2) {
            src.moveState = MoveState::AFTER_DOUBLE;
        } else {
            src.moveState = MoveState::MOVED;
        }
    } else src.moveState = MoveState::MOVED;

    if (dst.exists()) {
        ret = dst;
        ret.isPassant = false;
        capture({mov.dstRow, mov.dstCol});
    }

    // dst = src;
    singularMov({mov.srcRow, mov.srcCol}, {mov.dstRow, mov.dstCol}, src.isWhite);
    if (mov.doPromote) dst.type = static_cast<PType>(static_cast<uint8_t>(mov.promoteTo) + 2);

    // en passant
    if (src.type == PType::PAWN && mov.srcCol != mov.dstCol && !oldDst.exists()) {
        ret = rget(mov.srcRow, mov.dstCol);
        ret.isPassant = true;
        capture({mov.srcRow, mov.dstCol}); // original row, new column
    }

    // castling
    if (src.type == PType::KING) {
        int8_t diff = mov.dstCol - mov.srcCol;
        if (diff > 1) {
            singularMov({mov.srcRow, 7}, {mov.dstRow, mov.dstCol - 1}, src.isWhite);
            rget(mov.srcRow, 7) = Piece();
        } else if (diff < -1) {
            singularMov({mov.srcRow, 0}, {mov.dstRow, mov.dstCol + 1}, src.isWhite);
            rget(mov.srcRow, 0) = Piece();
        }
    }

    src = Piece();
    return std::make_pair(ret, rstate);
}

void Board::unmake(std::pair<Piece, MoveState> capInfo, Move mov) {
    Piece &dst = rget(mov.dstRow, mov.dstCol);
    Piece &src = rget(mov.srcRow, mov.srcCol);

    // src = dst;
    singularMov({mov.dstRow, mov.dstCol}, {mov.srcRow, mov.srcCol}, dst.isWhite);
    src.moveState = capInfo.second;
    dst = Piece(); // nil

    // restore the captured piece
    if (capInfo.first.exists()) {
        if (!capInfo.first.isPassant) {
            capInfo.first.isPassant = false;
            dst = capInfo.first;
            (capInfo.first.isWhite ? whitePieces : blackPieces)
                .emplace(BoardPosition{mov.dstRow, mov.dstCol});
        } else {
            // en passant
            capInfo.first.isPassant = false;
            rget(mov.srcRow, mov.dstCol) = capInfo.first;
            (capInfo.first.isWhite ? whitePieces : blackPieces)
                .emplace(BoardPosition{mov.srcRow, mov.dstCol});
        }
    }

    if (mov.doPromote) src.type = PType::PAWN; // undo promotion

    // undo castling
    if (src.type == PType::KING) {
        int8_t diff = mov.dstCol - mov.srcCol;
        if (diff > 1) {
            singularMov({mov.dstRow, mov.dstCol - 1}, {mov.srcRow, 7}, src.isWhite);
            rget(mov.dstRow, mov.dstCol - 1) = Piece();
        } else if (diff < -1) {
            singularMov({mov.dstRow, mov.dstCol + 1}, {mov.srcRow, 0}, src.isWhite);
            rget(mov.dstRow, mov.dstCol + 1) = Piece();
        }
    }
}

void Board::collectMovesFor(uint8_t r, uint8_t c, std::vector<Move> &ret, bool ONLY_CAP) {
    // too lazy to properly do these programatically so i'm hardcoding them!
    constexpr static int8_t dirs[8][2] = {
        {0, 1}, {0, -1}, // up, down
        {1, 0}, {-1, 0}, // right, left
        {1, 1}, {-1, -1},
        {1, -1}, {-1, 1}
    };

    constexpr static int8_t knightMoves[8][2] = {
        { 2, -1}, { 2, 1},
        {-2, -1}, {-2, 1},
        { 1, -2}, { 1, 2},
        {-1, -2}, {-1, 2}
    };

    switch (rget(r, c).type) {
    case (PType::BISHOP):
    case (PType::ROOK):
    case (PType::QUEEN):
    {
        uint8_t start = rget(r, c).type == PType::BISHOP ? 4 : 0;
        uint8_t end = rget(r, c).type == PType::ROOK ? 4 : 8;

        for (int dir = start; dir < end; dir++) {
            uint8_t cRow = r + dirs[dir][1];
            uint8_t cCol = c + dirs[dir][0];
            while (cRow >= 0 && cRow < 8 && cCol >= 0 && cCol < 8) {
                if (rget(cRow, cCol).exists() && rget(cRow, cCol).isWhite == rget(r,c).isWhite) break;

                // skip adding non-captures in ONLY_CAP
                if (!ONLY_CAP || rget(cRow, cCol).exists()) {
                    Move considering = Move({r, c}, {cRow, cCol});
                    considering.rank = rget(cRow, cCol).exists() * 10 + (rget(cRow, cCol).type >= rget(r,c).type) * 30;
                    ret.emplace_back(considering);
                }
                
                // do not check for same or opposite color: allow self-captures!
                if (rget(cRow, cCol).exists()) break;
                cRow += dirs[dir][1];
                cCol += dirs[dir][0];
            }
        }
        break;
    }

    case (PType::PAWN): {
        int8_t lookDir = rget(r, c).isWhite ? -1 : 1;
        uint8_t lRow = r + lookDir;

        // no bounds checking because pawns should never hug the boundary
        if (!rget(lRow, c).exists() && !ONLY_CAP) {
            Move mov = Move{{r, c}, {lRow, c}};
            if (lRow == 0 || lRow == 7) {
                // checks for promoting to both queen and knight
                mov.doPromote = true;
                mov.rank = 255;
                mov.promoteTo = PromoteType::QUEEN;
                ret.push_back(mov);
                mov.rank = 240;
                mov.promoteTo = PromoteType::KNIGHT;
            } else if (rget(r,c).moveState == MoveState::NOT_MOVED && 
                        !rget(r + 2*lookDir, c).exists()) {
                // no need to check for promotion when NOT_MOVED
                ret.emplace_back(Move{{r, c}, {static_cast<uint8_t>(r + 2*lookDir), c}});
            }

            ret.push_back(mov);
        }

        if (c + 1 < 8) {
            if (rget(lRow, c + 1).exists() && rget(lRow, c + 1).isWhite != rget(r,c).isWhite) {
                Move mov = Move{{r, c}, {lRow, (uint8_t) (c + 1)}};
                mov.rank = 220;
                if (lRow == 0 || lRow == 7) { 
                    mov.doPromote = true; 
                    mov.promoteTo = PromoteType::QUEEN;
                    mov.rank = 255;
                    ret.push_back(mov); // posibility of promoting to both queen and knight
                    mov.rank = 240;
                    mov.promoteTo = PromoteType::KNIGHT;
                }
                ret.push_back(mov);
            }

            // en passant, no possibility of promotion.
            auto square = rget(r, c + 1);
            if (square.type == PType::PAWN && square.moveState == MoveState::AFTER_DOUBLE
                 && square.isWhite != rget(r,c).isWhite) {
                auto consider = Move{{r, c}, {lRow, static_cast<uint8_t>(c + 1)}};
                consider.rank = 200;
                ret.emplace_back(consider);
            }
        }
        if (c - 1 >= 0){ 
            if (rget(lRow, c - 1).exists() && rget(lRow, c - 1).isWhite != rget(r,c).isWhite) {
                Move mov = Move{{r, c}, {lRow, (uint8_t) (c - 1)}};
                mov.rank = 220;
                if (lRow == 0 || lRow == 7) { 
                    mov.doPromote = true; 
                    mov.promoteTo = PromoteType::QUEEN;
                    mov.rank = 255;
                    ret.push_back(mov);
                    mov.rank = 240;
                    mov.promoteTo = PromoteType::KNIGHT;
                }
                ret.push_back(mov);
            }

            auto square = rget(r, c - 1);
            if (square.type == PType::PAWN && square.moveState == MoveState::AFTER_DOUBLE
                    && square.isWhite != rget(r,c).isWhite) {
                auto consider = Move{{r, c}, {lRow, static_cast<uint8_t>(c - 1)}};
                consider.rank = 200;
                ret.emplace_back(consider);
            }
        }
        break;
    }
    case (PType::KNIGHT): {
        for (const int8_t *d : knightMoves) {
            BoardPosition check = {r+d[1], c+d[0]};
            if (ONLY_CAP && !at(check).exists()) continue;

            if (r+d[1] >= 0 && r+d[1] < 8 && c+d[0] >= 0 && c+d[0] < 8 && (!at(check).exists() || at(check).isWhite != rget(r,c).isWhite)) {
                auto consider = Move{{r, c}, {static_cast<uint8_t>(r+d[1]), static_cast<uint8_t>(c+d[0])}};
                consider.rank = at(check).exists() * 10 + (at(check).type >= rget(r,c).type) * 30;
                ret.emplace_back(consider);
            }
        }
        break;
    }

    case (PType::KING): {
        for (const int8_t *dir : dirs) {
            BoardPosition check = {r+dir[1], c+dir[0]};
            if (ONLY_CAP && !at(check).exists()) continue;

            if (r+dir[1] >= 0 && r+dir[1] < 8 && c+dir[0] >= 0 && c+dir[0] < 8 && (!at(check).exists() || at(check).isWhite != rget(r,c).isWhite)) {
                auto consider = Move{{r, c}, {static_cast<uint8_t>(r + dir[1]), 
                                static_cast<uint8_t>(c + dir[0])}};
                consider.rank = at(check).exists() * 10;
                ret.emplace_back(consider);
            }
        }

        // castling
        if (rget(r, c).moveState == MoveState::NOT_MOVED && !ONLY_CAP) {
            // kingside
            if (rget(r, 7).type == PType::ROOK && rget(r, 7).moveState == MoveState::NOT_MOVED
                    && rget(r, 7).isWhite == rget(r, c).isWhite) {
                bool blocked = false;
                for (uint8_t scan = c + 1; scan < 7; scan++) blocked |= rget(r, scan).exists();
                if (!blocked) ret.emplace_back(Move{{r, c}, {r, 6}});
            }

            // queenside
            if (rget(r, 0).type == PType::ROOK && rget(r, 0).moveState == MoveState::NOT_MOVED
                    && rget(r, 0).isWhite == rget(r, c).isWhite) {
                bool blocked = false;
                for (uint8_t scan = 1; scan < c; scan++) blocked |= rget(r, scan).exists();
                if (!blocked) ret.emplace_back(Move{{r, c}, {r, 2}});
            }
        }
        break;
    }
    
    }
}

std::vector<Move> Board::pseudoLegalMoves(bool doWhite, bool ONLY_CAP) {
    std::vector<Move> ret;

    // Checks/checkmates? Also, this algo searches entire board. Maybe keep list of pieces for each side?
    // for (uint8_t r = 0; r < 8; r++) { for (uint8_t c = 0; c < 8; c++) {
    //     if (rget(r, c).exists() && rget(r, c).isWhite == doWhite) {
    //         collectMovesFor(r, c, ret);
    //     }
    // }}

    for (const BoardPosition i : (doWhite ? whitePieces : blackPieces))
        collectMovesFor(i.row, i.col, ret, ONLY_CAP);

    std::sort(ret.begin(), ret.end(), [](Move a, Move b) {
        return a.rank > b.rank;
    });

    return ret;
}

void Board::dbgPrint() {
    for (int rank = 8; rank > 0; rank--) {
        std::cout << rank << "\t";
        for (char file = 'a'; file <= 'h'; file++){
            if (get(file, rank).exists()) {
                std::cout << get(file, rank).toString();
            } else std::cout << "-";
            std::cout << "\t";
        }
        std::cout << std::endl;
    }
}


// this function should probably NOT be in the board class
void Board::draw(SDL_Renderer *rend, int w, int h, SDL_Texture **font) {
    SDL_Rect drawArea = {0, 0, w / 8, h / 8};

#define DRAW(arr, off) for (const BoardPosition i : arr) { \
        drawArea.x = i.col * w/8; \
        drawArea.y = i.row * h/8; \
        SDL_RenderCopy(rend, font[off + static_cast<int>(at(i).type)], nullptr, &drawArea); \
    }

    DRAW(blackPieces, 0);
    DRAW(whitePieces, 7);
}



Move RandomEngine::search(Board &b, bool side) {
    auto moves = b.pseudoLegalMoves(side);
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, moves.size() - 1);
    auto select = dist(rng);
    return select >= 0 && select < moves.size() ? moves.at(select) : Move{};
}


void Board::initZobrist() {
    std::random_device rd;

    std::mt19937_64 e2(rd());

    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());

    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 12; j++)
            zobristTable[i][j] = dist(e2);
}
