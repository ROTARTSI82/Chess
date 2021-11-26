#include <connect4/connect4.hpp>

#include <iostream>

void C4Board::update(C4Move changed) {
    int row = columns[changed].size() - 1;
    bool verticalWon = row >= 3; // 3 2 1 0 is 4 values
    for (int i = 0; i < 4 && verticalWon; i++)
        verticalWon &= columns[changed].at(row--) == turn;

    if (verticalWon) {
        gameState = turn;
        return;
    }
    
    row = columns[changed].size() - 1;
    int horizPos = 0;
    int horizNeg = 0;
    for (int i = changed + 1; i < BWIDTH; i++) {
        if (columns[i].size() <= row || columns[i][row] != turn) break;
        horizPos++;
    }

    for (int i = changed - 1; i >= 0; i--) {
        if (columns[i].size() <= row || columns[i][row] != turn) break;
        horizNeg++;
    }

    if ((horizPos + horizNeg + 1) >= 4) { // horizontal win
        gameState = turn;
        return;
    }

    // diagonal win 1
    if ((countDiag(changed, row, 1, 1) + countDiag(changed, row, -1, -1) + 1) >= 4) {
        gameState = turn;
        return;
    }

    // diagonal win 2
    if ((countDiag(changed, row, -1, 1) + countDiag(changed, row, 1, -1) + 1) >= 4) {
        gameState = turn;
        return;
    }

    bool isDraw = true;
    for (const auto &v : columns)
        isDraw &= v.size() == BHEIGHT;
    if (isDraw) gameState = C4State::DRAW;
}

int C4Board::countDiag(int8_t col, int8_t row, int8_t dcol, int8_t drow) {
    int count = 0;

    col += dcol;
    row += drow;
    while (col >= 0 && col < BWIDTH && row >= 0 && row < BHEIGHT) {
        if (columns[col].size() <= row || columns[col][row] != turn) break;
        count++;
        col += dcol;
        row += drow;
    }

    return count;
}


int C4Board::score(int alpha, int beta, int depth) {
    switch (gameState) {
    case C4State::NIL: {
        if (depth <= 0) return 0;

        // // disable alpha beta pruning
        // alpha = -9999;
        for (int move = 0; move < BWIDTH; move++) {
            if (!legal(move)) continue;
            make(move);
            int s = -score(-beta, -alpha, depth - 1);
            unmake(move);

            // alpha beta pruning
            if (s >= beta) {
                return beta;
            }
            alpha = std::max(alpha, s);
        }

        return alpha;
    }
    case C4State::DRAW: { 
        // std::cout << "FOUND DRAW\n";
        return -1; 
    } 
    case C4State::RED: { 
        // std::cout << "FOUND RED WIN\n";
        return 100 * (turn == C4State::RED ? 1 : -1);
    }
    case C4State::YELLOW: {
        // std::cout << "FOUND YELLOW WIN\n";
        return 100 * (turn == C4State::YELLOW ? 1 : -1);
    }
    }

    throw std::runtime_error("What how did we get here");
}

C4Move C4Board::search(int depth) {
    if (gameState != C4State::NIL) throw std::runtime_error("Game is already over bozo");
    int alpha = -9999;

    int selected = -1;
    for (int move = 0; move < BWIDTH; move++) {
        if (!legal(move)) continue;
        make(move);
        int s = -score(-9999, -alpha, depth - 1);
        unmake(move);

        if (s > alpha) {
            selected = move;
            alpha = s;
        }
    }

    if (selected == -1) {
        throw std::runtime_error("no legal moves");
    }

    std::cout << "My score is " << alpha << std::endl;

    return static_cast<uint8_t>(selected);
}