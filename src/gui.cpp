#ifdef SDL_AVAIL
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>
#endif

#include <cstdlib>
#include <iostream>

#include "scacus/movegen.hpp"
#include "scacus/uci.hpp"

#if 1

using namespace sc;


int main(int, char **) {
    if (true) {
        sc::UCI().run();
        return 0;
    }

    if (false) {
        sc::init_movegen();
        Position pos{};
        run_perft(pos, 6);
        return 0;
    }

#ifdef SDL_AVAIL
    sc::init_movegen();

    // retutns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "error initializing SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Sizeof (move) = " << sizeof(sc::Move) << std::endl;
    std::cout << "Sizeof (Position) = " << sizeof(sc::Position) << std::endl;

    sc::Position pos;

    std::string pgn;
    int turnNum = 1;
    auto add_and_make = [&](Move mov) {
        if (pos.get_turn() == WHITE_SIDE) pgn += std::to_string(turnNum++) + ". ";
        pgn += mov.standard_alg_notation(pos);

        StateInfo ret = make_move(pos, mov);

        auto numLegals = legal_moves_from(pos).size();
        if (numLegals == 0 && pos.in_check()) pgn += "#";
        else if (pos.in_check()) pgn += "+";
        pgn += " ";

        return ret;
    };

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
        tex("connect4.png"), tex("wking.png"), tex("wqueen.png"), tex("wrook.png"), tex("wbishop.png"), tex("wknight.png"), tex("wpawn.png"),
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
    sc::MoveList legalMoves(0);

    sc::Bitboard drawBb = 0;
    sc::EngineV2 eng;
    eng.set_pos(&pos);

    // annimation loop
    while (running) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);

        if (legal_moves_from(pos).empty()) {
            running = false;
            break;
        }

//        eng.start_search(64);
//        std::this_thread::sleep_for(std::chrono::seconds(8));
//        eng.stop_search();
//        undoCaps.push_back(add_and_make(eng.bestMove));
//        undoMoves.push_back(eng.bestMove);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_MOUSEBUTTONDOWN: {
                sc::Square prev = selected;
                isSelected ^= true;

                selected = sc::new_square('a' + event.button.x * 8 / w, 8 - event.button.y * 8 / h);

                if (!isSelected) { // make the move
                    sc::Move prototype = sc::new_move_normal(prev, selected);
                    sc::Move mov;
                    bool found = false;
                    for (const auto m : legalMoves) {
                        if (m.typeFlags == PROMOTION && m.promote != PROMOTE_ROOK) continue;
                        found |= (m.src == prototype.src && m.dst == prototype.dst);
                        if (found) {
                            mov = m;
                            break;
                        }
                    }

                    if (!found || (sc::to_bitboard(prev) & pos.by_side(sc::opposite_side(pos.get_turn())))) {
                        legalMoves.clear();
                        break;
                    }

                    undoCaps.push_back(add_and_make(mov));
                    undoMoves.push_back(mov);
                    legalMoves.clear();

                    // auto engineMove = sc::primitive_search(pos, 1);
                    // undoCaps.push_back(sc::make_move(pos, engineMove.first));
                    // undoMoves.push_back(engineMove.first);
                    // std::cout << "Engine evaluation: " << engineMove.second << "\n";
                } else {
                    legalMoves.clear();
                    if (pos.get_turn() == WHITE_SIDE) sc::standard_moves<WHITE_SIDE, false>(legalMoves, pos);
                    else sc::standard_moves<BLACK_SIDE, false>(legalMoves, pos);
                }

                break;
            }
            case SDL_KEYDOWN: {
                if (SDLK_0 <= event.key.keysym.sym && event.key.keysym.sym <= SDLK_9) {
                    run_perft(pos, event.key.keysym.sym - SDLK_0);
                } else switch (event.key.keysym.sym) {
                case SDLK_f:
                    std::cout << pos.get_fen() << std::endl;
                    std::cout << "zob: " << pos.get_state().hash << std::endl;
                    break;
                case SDLK_b:
                    drawBb = pos.by_type(BISHOP);
                    break;
                case SDLK_t:
                    drawBb = pos.by_side(pos.get_turn());
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
                case SDLK_g: {
                    // auto engineMove = eng.primitive_search(pos, 3);
                    eng.start_search(64);
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    eng.stop_search();
                    undoCaps.push_back(add_and_make(eng.best_move()));
                    undoMoves.push_back(eng.best_move());
                    // std::cout << "Engine evaluation: " << engineMove.second << "\n";
                    break;
                }
                case SDLK_w: {
                    // auto engineMove = eng.primitive_search(pos, 3);
                    eng.start_search(64);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    eng.stop_search();
                    undoCaps.push_back(add_and_make(eng.worst_move()));
                    undoMoves.push_back(eng.worst_move());
                    // std::cout << "Engine evaluation: " << engineMove.second << "\n";
                    break;
                }
                case SDLK_e:
                    std::cout << pgn << std::endl;
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
            SDL_RenderCopy(rend, font[(int) pos.piece_at(sq)], nullptr, &drawArea);
        }

        for (sc::Square sq = 0; sq < 64; sq++) {
            drawArea.x = sc::file_ind_of(sq) * w / 8;
            drawArea.y = (7-sc::rank_ind_of(sq)) * h / 8;
            if (drawBb & to_bitboard(sq))
                SDL_RenderCopy(rend, moveTex, nullptr, &drawArea);
        }

        for (sc::Move mov : legalMoves) {
            // auto srcX = sc::file_ind_of(mov.src) * w / 8 + w/16;
            // auto srcY = (7-sc::rank_ind_of(mov.src)) * h / 8 + h/16;
            // auto dstX = sc::file_ind_of(mov.dst) * w / 8 + w/16;
            // auto dstY = (7-sc::rank_ind_of(mov.dst)) * h / 8 + h/16;
            // SDL_RenderDrawLine(rend, srcX, srcY, dstX, dstY);

            if (mov.src == selected) {
                drawArea.x =  sc::file_ind_of(mov.dst) * w / 8;
                drawArea.y = (7-sc::rank_ind_of(mov.dst)) * h / 8;
                SDL_RenderCopy(rend, moveTex, nullptr, &drawArea);
            }
        }

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

    std::cout << pgn;
    for (auto i : font) SDL_DestroyTexture(i);
    SDL_DestroyTexture(boardTex);
    SDL_DestroyTexture(selectTex);
    SDL_DestroyTexture(moveTex);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
#endif

}
#endif
