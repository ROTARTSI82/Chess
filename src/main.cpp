#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>

#include <cstdlib>
#include <iostream>

#include <chess.hpp>

#include "stockfish_engine.hpp"

int main(int argc, char **argv) {
    // retutns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "error initializing SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Sizeof (move) = " << sizeof(Move) << std::endl;

    Board board{};
    board.dbgPrint();

    Engine *eng = new StockfishEngine();

    SDL_Window* win = SDL_CreateWindow("Chess", // creates a window
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        512, 512, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);


    auto tex = [&](const std::string &filename) -> SDL_Texture * {
        SDL_Surface *bsurf = IMG_Load(filename.c_str());
        SDL_Texture *ret = SDL_CreateTextureFromSurface(rend, bsurf);
        SDL_FreeSurface(bsurf);
        return ret;
    };

    SDL_Texture *boardTex = tex("./board.png");
    SDL_Texture *font[] = {
        nullptr, tex("./bpawn.png"), tex("./bknight.png"), tex("./bbishop.png"), tex("./brook.png"), tex("./bqueen.png"), tex("./bking.png"),
        nullptr, tex("./wpawn.png"), tex("./wknight.png"), tex("./wbishop.png"), tex("./wrook.png"), tex("./wqueen.png"), tex("./wking.png")
    };

    SDL_Texture *selectTex = tex("./select.png");

    // controls annimation loop
    bool running = true;

    bool isSelected = false;
    bool turn = true;
    BoardPosition selected = {0, 0};

    std::vector<Move> undoMoves;
    std::vector<std::pair<Piece, MoveState>> undoCaps;

    // annimation loop
    while (running) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_MOUSEBUTTONDOWN: {
                if (!turn) break;

                BoardPosition prev = selected;
                isSelected ^= true;

                selected.col = event.button.x * 8 / w;
                selected.row = event.button.y * 8 / h;
                // std::cout << "Selected " << (int) selected.col << ", " << (int) selected.row << std::endl;

                if (!isSelected) { // make the move
                    auto legalMoves = std::vector<Move>();
                    board.collectMovesFor(prev.row, prev.col, legalMoves);

                    bool found = false;
                    for (const auto m : legalMoves)
                        found |= (m.dstCol == selected.col && m.dstRow == selected.row);

                    if (!found || board.rget(prev.row, prev.col).isWhite == false) {
                        std::cout << "Illegal move you idiot" << std::endl;
                        break;
                    }

                    Move mov = Move{{prev.row, prev.col}, {selected.row, selected.col}};
                    if (board.rget(prev.row, prev.col).type == PType::PAWN 
                        && (selected.row == 0 || selected.row == 7)) {
                        mov.doPromote = true;
                        mov.promoteTo = PromoteType::QUEEN; // always promote to queen :/
                    }

                    board.make(mov);

                    board.make(eng->search(board, BLACK_SIDE));
                }

                break;
            }
            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_f:
                    std::cout << board.getFen() << std::endl;
                    break;
                case SDLK_g:
                    board.make(eng->search(board, turn));
                    turn ^= true;
                    break;
                }
            }
            }
        }

        // clears the screen
        SDL_RenderClear(rend);
        SDL_RenderCopy(rend, boardTex, NULL, NULL);
        board.draw(rend, w, h, font);

        // std::vector<Move> moves = board.pseudoLegalMoves(WHITE_SIDE);
        // for (const auto i : moves) {
        //     // if (board.rget(i.srcRow, i.srcCol).type == PType::KING)
        //     SDL_RenderDrawLine(rend, i.srcCol * w/8 + w/16, i.srcRow * w/8 + w/16, i.dstCol * w/8 + w/16, i.dstRow * w/8 + w/16);
        // }

        if (isSelected) {
            SDL_Rect dstRect;
            dstRect.x = w * selected.col / 8;
            dstRect.y = h * selected.row / 8;
            dstRect.w = w / 8;
            dstRect.h = h / 8;
            SDL_RenderCopy(rend, selectTex, NULL, &dstRect);
        }


        // triggers the double buffers
        // for multiple rendering
        SDL_RenderPresent(rend);

        // calculates to 60 fps
        SDL_Delay(1000 / 60);
    }

    for (int i = 1; i < 14; i++) SDL_DestroyTexture(font[i]);
    SDL_DestroyTexture(boardTex);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    delete eng;
    SDL_Quit();
}