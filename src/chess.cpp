#include "chess.hpp"

#include <iostream>
#include <random>

Piece::Piece() : type(PType::NIL), isWhite(false), moveState(MoveState::NOT_MOVED), isPassant(false) {};
Piece::Piece(PType t, bool white) : type(t), isWhite(white), moveState(MoveState::NOT_MOVED), isPassant(false) {};

Move::Move(BoardPosition from, BoardPosition to) : srcCol(from.col), srcRow(from.row), dstRow(to.row), dstCol(to.col),
                                                   promoteTo(PromoteType::QUEEN), doPromote(false) {};

Move::Move(uint8_t src, uint8_t dst) : Move(BoardPosition{(uint8_t) (src % 8), (uint8_t) (src / 8)},
                                            BoardPosition{(uint8_t) (dst % 8), (uint8_t) (dst / 8)}) {};

Board::Board() {
    for (int i = 0; i < 64; i++) board[i] = Piece();

    std::cout << "Init board" << std::endl;

    for (char r = 'a'; r <= 'h'; r++) {
        get(r, 2) = Piece(PType::PAWN, true);
        get(r, 7) = Piece(PType::PAWN, false);
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
        capture(dst);
    }

    dst = src;
    if (mov.doPromote) dst.type = static_cast<PType>(static_cast<uint8_t>(mov.promoteTo) + 2);

    // en passant
    if (src.type == PType::PAWN && mov.srcCol != mov.dstCol && !oldDst.exists()) {
        ret = rget(mov.srcRow, mov.dstCol);
        ret.isPassant = true;
        capture(rget(mov.srcRow, mov.dstCol)); // original row, new column
    }

    // castling
    if (src.type == PType::KING) {
        int8_t diff = mov.dstCol - mov.srcCol;
        if (diff > 1) {
            Piece &rook = rget(mov.srcRow, 7);
            rget(mov.dstRow, mov.dstCol - 1) = rook;
            rook = Piece();
        } else if (diff < -1) {
            Piece &rook = rget(mov.srcRow, 0);
            rget(mov.dstRow, mov.dstCol + 1) = rook;
            rook = Piece();
        }
    }

    src = Piece();
    return std::make_pair(ret, rstate);
}

void Board::unmake(std::pair<Piece, MoveState> capInfo, Move mov) {
    Piece &dst = rget(mov.dstRow, mov.dstCol);
    Piece &src = rget(mov.srcRow, mov.srcCol);

    src = dst;
    src.moveState = capInfo.second;
    dst = Piece(); // nil

    // restore the captured piece
    if (!capInfo.first.isPassant) {
        capInfo.first.isPassant = false;
        dst = capInfo.first;
    } else {
        capInfo.first.isPassant = false;
        rget(mov.srcRow, mov.dstCol) = capInfo.first;
    }

    if (mov.doPromote) src.type = PType::PAWN; // undo promotion

    // undo castling
    if (src.type == PType::KING) {
        int8_t diff = mov.dstCol - mov.srcCol;
        if (diff > 1) {
            Piece &rook = rget(mov.dstRow, mov.dstCol - 1);
            rget(mov.srcRow, 7) = rook;
            rook = Piece();
        } else if (diff < -1) {
            Piece &rook = rget(mov.dstRow, mov.dstCol + 1);
            rget(mov.srcRow, 0) = rook;
            rook = Piece();
        }
    }
}

void Board::collectMovesFor(uint8_t r, uint8_t c, std::vector<Move> &ret) {
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
                ret.emplace_back(Move({r, c}, {cRow, cCol}));
                if (rget(cRow, cCol).exists()) break; // do not check for same or opposite color: allow self-captures!
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
        if (!rget(lRow, c).exists()) {
            Move mov = Move{{r, c}, {lRow, c}};
            if (lRow == 0 || lRow == 7) {
                // checks for promoting to both queen and knight
                mov.doPromote = true;
                mov.promoteTo = PromoteType::QUEEN;
                ret.push_back(mov);
                mov.promoteTo = PromoteType::KNIGHT;
            } else if (rget(r,c).moveState == MoveState::NOT_MOVED && 
                        !rget(r + 2*lookDir, c).exists()) {
                // no need to check for promotion when NOT_MOVED
                ret.emplace_back(Move{{r, c}, {static_cast<uint8_t>(r + 2*lookDir), c}});
            }

            ret.push_back(mov);
        }

        if (c + 1 < 8) {
            if (rget(lRow, c + 1).exists()) {
                Move mov = Move{{r, c}, {lRow, (uint8_t) (c + 1)}};
                if (lRow == 0 || lRow == 7) { 
                    mov.doPromote = true; 
                    mov.promoteTo = PromoteType::QUEEN;
                    ret.push_back(mov);
                    mov.promoteTo = PromoteType::KNIGHT;
                }
                ret.push_back(mov);
            }

            auto sqaure = rget(r, c + 1);
            if (sqaure.type == PType::PAWN && sqaure.moveState == MoveState::AFTER_DOUBLE) {
                ret.emplace_back(Move{{r, c}, {lRow, static_cast<uint8_t>(c + 1)}});
            }
        }
        if (c - 1 >= 0){ 
            if (rget(lRow, c - 1).exists()) {
                Move mov = Move{{r, c}, {lRow, (uint8_t) (c - 1)}};
                if (lRow == 0 || lRow == 7) { 
                    mov.doPromote = true; 
                    mov.promoteTo = PromoteType::QUEEN;
                    ret.push_back(mov);
                    mov.promoteTo = PromoteType::KNIGHT;
                }
                ret.push_back(mov);
            }

            auto sqaure = rget(r, c - 1);
            if (sqaure.type == PType::PAWN && sqaure.moveState == MoveState::AFTER_DOUBLE) {
                ret.emplace_back(Move{{r, c}, {lRow, static_cast<uint8_t>(c - 1)}});
            }
        }
        break;
    }
    case (PType::KNIGHT): {
        for (const int8_t *d : knightMoves) {
            if (r+d[1] >= 0 && r+d[1] < 8 && c+d[0] >= 0 && c+d[0] < 8) {
                ret.emplace_back(Move{{r, c}, {static_cast<uint8_t>(r+d[1]), 
                                    static_cast<uint8_t>(c+d[0])}});
            }
        }
        break;
    }

    case (PType::KING): {
        for (const int8_t *dir : dirs) {
            if (r+dir[1] >= 0 && r+dir[1] < 8 && c+dir[0] >= 0 && c+dir[0] < 8)
                ret.emplace_back(Move{{r, c}, {static_cast<uint8_t>(r + dir[1]), 
                                static_cast<uint8_t>(c + dir[0])}});
        }

        // castling
        if (rget(r, c).moveState == MoveState::NOT_MOVED) {
            // kingside
            if (rget(r, 7).type == PType::ROOK && rget(r, 7).moveState == MoveState::NOT_MOVED) {
                bool blocked = false;
                for (uint8_t scan = c + 1; scan < 7; scan++) blocked |= rget(r, scan).exists();
                if (!blocked) ret.emplace_back(Move{{r, c}, {r, 6}});
            }

            // queenside
            if (rget(r, 0).type == PType::ROOK && rget(r, 0).moveState == MoveState::NOT_MOVED) {
                bool blocked = false;
                for (uint8_t scan = 1; scan < c; scan++) blocked |= rget(r, scan).exists();
                if (!blocked) ret.emplace_back(Move{{r, c}, {r, 2}});
            }
        }
        break;
    }
    
    }
}

std::vector<Move> Board::pseudoLegalMoves(bool doWhite) {
    std::vector<Move> ret;

    // Checks/checkmates? Also, this algo searches entire board. Maybe keep list of pieces for each side?
    for (uint8_t r = 0; r < 8; r++) { for (uint8_t c = 0; c < 8; c++) {
        if (rget(r, c).exists() && rget(r, c).isWhite == doWhite) {
            collectMovesFor(r, c, ret);
        }
    }}

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

    // this algo isn't very efficient. We should keep a running list of pieces in play
    // (for either side? do we track kings too?)
    for (uint8_t r = 0; r < 8; r++) {
        drawArea.x = 0;
        for (uint8_t f = 0; f < 8; f++) {
            if (rget(r, f).exists()) {
                SDL_RenderCopy(rend, font[rget(r, f).isWhite * 7 + static_cast<int>(rget(r, f).type)],
                               NULL, &drawArea);
            }
            drawArea.x += w / 8;
        }
        drawArea.y += h / 8;
    }
}



Move RandomEngine::search(Board &b, bool side) {
    auto moves = b.pseudoLegalMoves(side);

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, moves.size() - 1); // distribution in range [1, 6]

    return moves.at(dist(rng));
}
