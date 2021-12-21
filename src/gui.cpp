#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>

#include <cstdlib>
#include <iostream>

#include "scacus/movegen.hpp"

#if 1

using namespace sc;

int main(int argc, char **argv) {
    sc::init_movegen();

    // retutns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "error initializing SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Sizeof (move) = " << sizeof(sc::Move) << std::endl;
    std::cout << "Sizeof (Position) = " << sizeof(sc::Position) << std::endl;

    sc::Position pos;

    SDL_Window* win = SDL_CreateWindow("Chess", // creates a window
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        512, 512, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);


    auto tex = [&](std::string filename) -> SDL_Texture * {
        filename = "./res/" + filename;
        SDL_Surface *bsurf = IMG_Load(filename.c_str());
        SDL_Texture *ret = SDL_CreateTextureFromSurface(rend, bsurf);
        SDL_FreeSurface(bsurf);
        return ret;
    };

    SDL_Texture *boardTex = tex("board.png");
    SDL_Texture *font[] = {
        nullptr, tex("bking.png"), tex("bqueen.png"), tex("brook.png"), tex("bbishop.png"), tex("bknight.png"), tex("bpawn.png"),
        tex("red.png"), 
        tex("red.png"), tex("wking.png"), tex("wqueen.png"), tex("wrook.png"), tex("wbishop.png"), tex("wknight.png"), tex("wpawn.png"),
        tex("yellow.png"), tex("yellow.png")
    };

    SDL_Texture *selectTex = tex("select.png");
    SDL_Texture *moveTex = tex("move.png");

    // controls annimation loop
    bool running = true;

    bool isSelected = false;
    sc::Square selected;

    std::vector<sc::Move> undoMoves;
    std::vector<sc::StateInfo> undoCaps;
    sc::MoveList legalMoves;

    sc::Bitboard drawBb = 0;

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
                // if (!turn) break;
                sc::Square prev = selected;
                isSelected ^= true;

                selected = sc::make_square('a' + event.button.x * 8 / w, 8 - event.button.y * 8 / h);
                std::cout << "Selected " << (int)selected << std::endl;
                // std::cout << "Selected " << (int) selected.col << ", " << (int) selected.row << std::endl;

                if (!isSelected) { // make the move
                    sc::Move prototype = sc::make_normal(prev, selected);
                    sc::Move mov;
                    bool found = false;
                    for (const auto m : legalMoves) {
                        if (found |= (m.src == prototype.src && m.dst == prototype.dst)) {
                            mov = m;
                            break;
                        }
                    }

                    if (!found || (sc::to_bitboard(prev) & pos.by_side(sc::opposite_side(pos.turn)))) {
                        std::cout << "Illegal move you idiot" << std::endl;
                        legalMoves.clear();
                        break;
                    }

                    
                    // if (board.rget(prev.row, prev.col).type == PType::PAWN 
                    //     && (selected.row == 0 || selected.row == 7)) {
                    //     mov.doPromote = true;
                    //     mov.promoteTo = PromoteType::QUEEN; // always promote to queen :/
                    // }

                    undoCaps.push_back(sc::make_move(pos, mov));
                    undoMoves.push_back(mov);
                    legalMoves.clear();
                    // turn = sc::opposite_side(turn);

                    // undoMoves.push_back(blackEngine->search(board, false));
                    // undoCaps.push_back(board.make(undoMoves.back()));
                } else {
                    legalMoves = pos.turn == WHITE_SIDE ? sc::standard_moves<WHITE_SIDE>(pos) : sc::standard_moves<BLACK_SIDE>(pos);
                    std::cout << "update legal moves";
                    // if (board.rget(selected.row, selected.col).isWhite)
                    //     board.collectMovesFor(selected.row, selected.col, legalMoves);
                }

                break;
            }
            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_f:
                    std::cout << pos.get_fen() << std::endl;
                    break;
                case SDLK_b:
                    drawBb = pos.by_type(BISHOP);
                    break;
                case SDLK_t:
                    drawBb = pos.by_side(pos.turn);
                    break;
                case SDLK_n:
                    drawBb = pos.by_type(KNIGHT);
                    break;
                case SDLK_k:
                    drawBb = pos.by_type(KING); break;
                case SDLK_q:
                    drawBb = pos.by_type(QUEEN);
                    break;
                case SDLK_p:
                    drawBb = pos.by_type(PAWN); break;
                case SDLK_r:
                    drawBb = pos.by_type(ROOK); break;
                case SDLK_ESCAPE:
                    drawBb = 0; break;
                case SDLK_BACKSPACE:
                    if (undoMoves.empty()) break;
                    sc::unmake_move(pos, undoCaps.back(), undoMoves.back());
                    undoCaps.pop_back(); undoMoves.pop_back();
                    // turn ^= true;
                    break;
                case SDLK_s:
                    break;
                case SDLK_i:
                    std::cout << "ENTER FEN> ";
                    std::string fen;
                    std::getline(std::cin, fen);
                    pos = Position{fen};
                    break;
                }
            }
            }
        }

        // clears the screen
        SDL_RenderClear(rend);
        SDL_RenderCopy(rend, boardTex, NULL, NULL);

        SDL_Rect drawArea = {0, 0, w / 8, h / 8};

        for (sc::Square sq = 0; sq < 64; sq++) {
            drawArea.x = sc::file_ind_of(sq) * w / 8;
            drawArea.y = (7-sc::rank_ind_of(sq)) * h / 8;
            SDL_RenderCopy(rend, font[(int) pos.pieces[sq]], nullptr, &drawArea);
        }

        for (sc::Square sq = 0; sq < 64; sq++) {
            drawArea.x = sc::file_ind_of(sq) * w / 8;
            drawArea.y = (7-sc::rank_ind_of(sq)) * h / 8;
            if (drawBb & to_bitboard(sq))
                SDL_RenderCopy(rend, moveTex, nullptr, &drawArea);
        }

        for (sc::Move mov : legalMoves) {
            auto srcX = sc::file_ind_of(mov.src) * w / 8 + w/16;
            auto srcY = (7-sc::rank_ind_of(mov.src)) * h / 8 + h/16;
            auto dstX = sc::file_ind_of(mov.dst) * w / 8 + w/16;
            auto dstY = (7-sc::rank_ind_of(mov.dst)) * h / 8 + h/16;
            SDL_RenderDrawLine(rend, srcX, srcY, dstX, dstY);

            if (mov.src == selected) {
                drawArea.x =  sc::file_ind_of(mov.dst) * w / 8;
                drawArea.y = (7-sc::rank_ind_of(mov.dst)) * h / 8;
                SDL_RenderCopy(rend, moveTex, nullptr, &drawArea);
            }
        }

        // toDraw = board.pseudoLegalMoves(WHITE_SIDE, true);
        // for (const auto i : toDraw) {
        //     // if (board.rget(i.srcRow, i.srcCol).type == PType::KING)
        //     SDL_RenderDrawLine(rend, i.srcCol * w/8 + w/16, i.srcRow * h/8 + h/16, i.dstCol * w/8 + w/16, i.dstRow * h/8 + h/16);
        // }

        // for (int i = undoMoves.size() - 2; i > 0 && i < undoMoves.size(); i++) {
        //     auto m = undoMoves[i];
        //     SDL_RenderDrawLine(rend, m.srcCol * w/8 + w/16, m.srcRow * h/8 + h/16, m.dstCol * w/8 + w/16, m.dstRow * h/8 + h/16);
        // }

        // SDL_Rect moveDst = {0, 0, w / 8, h / 8};
        // for (const auto i : legalMoves) {
        //     moveDst.x = i.dstCol * w/8;
        //     moveDst.y = i.dstRow * h/8;
        //     SDL_RenderCopy(rend, moveTex, NULL, &moveDst);
        // }

        if (isSelected) {
            SDL_Rect dstRect;
            dstRect.x = w * sc::file_ind_of(selected) / 8;
            dstRect.y = h * (7-sc::rank_ind_of(selected)) / 8;
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

    for (auto i : font) SDL_DestroyTexture(i);
    SDL_DestroyTexture(boardTex);
    SDL_DestroyTexture(selectTex);
    SDL_DestroyTexture(moveTex);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
#endif