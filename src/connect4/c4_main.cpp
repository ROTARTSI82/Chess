#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>

#include <cstdlib>
#include <iostream>

#include <connect4/connect4.hpp>
#include <connect4/neural_net.hpp>

#define SEARCH_DEPTH 10

int main(int argc, char **argv) {

    // retutns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "error initializing SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

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

    SDL_Texture *boardTex = tex("./res/connect4.png");
    SDL_Texture *yellow = tex("./res/yellow.png");
    SDL_Texture *red = tex("./res/red.png");

    // controls annimation loop
    bool running = true;

    C4Board b;
    b.make(b.search(SEARCH_DEPTH));

    NeuralNetwork nn;
    nn.randomize();

    auto checkWins = [&]() -> bool {
        if (b.gameState != C4State::NIL) {
            std::cout << "GAME IS OVER: ";
            switch (b.gameState) {
            case C4State::YELLOW: std::cout << "YELLOW WINS!"; break;
            case C4State::DRAW: std::cout << "DRAW!"; break;
            case C4State::RED: std::cout << "RED WINS!"; break;
            default: break;
            }
            std::cout << std::endl;

            // b = C4Board();
            return true;
        }
        return false;
    };

    auto runNN = [&](bool learn = true, bool playMinimax = false) {
        if (b.turn == C4State::YELLOW) {
            if (playMinimax) b.make(b.search(SEARCH_DEPTH)); else b.randMove();
            // b.make(b.search(SEARCH_DEPTH));
            if (checkWins()) {
                b = C4Board();
                nn.applyBackprops();
            }
            return;
        }

        Vector<84> inp;
        memset(inp.data, 0, 84 * sizeof(double));

        for (int x = 0; x < BWIDTH; x++) {
            int y = 0;
            for (const auto val : b.columns[x]) {
                inp[(x * BHEIGHT + y) * 2 + (val == C4State::RED ? 0 : 1)] = 1.0;
                y++;
            }
        }

        nn.input = inp;
        if (learn) {
            C4Move goodMov = b.search(SEARCH_DEPTH);
            Vector<7> expected;
            memset(expected.data, 0, 7 * sizeof(double));
            expected[goodMov] = 1.0;
            nn.backprop(expected);

            if (playMinimax) b.make(goodMov); else b.randMove();
            if (checkWins()) {
                b = C4Board();
                nn.applyBackprops();
            }
            return;
        }

        Vector<7> res = nn.run();
        int selected = 0;
        double max = -9999;
        for (int i = 0; i < 7; i++) {
            if (res[i] > max) {
                max = res[i];
                selected = i;
            }
            std::cout << "Neural net[" << i << "] = " << res[i] << std::endl;
        }

        std::cout << "Learning disabled. Tried move: " << selected << std::endl;
        if (b.legal(selected))
            b.make(selected);
        else {
            std::cout << "Fell back to minimax search" << std::endl;
            b.make(b.search(SEARCH_DEPTH));
        }

        if (checkWins()) {
            b = C4Board();
        }
    };

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
                uint8_t col = event.button.x * BWIDTH / w;
                if (b.columns[col].size() == BHEIGHT) {
                    std::cout << "ILLEGAL MOVE YOU IDIOT" << std::endl;
                    break;
                }

                b.make(col);
                if (checkWins()) break;


                runNN(false); // no learning when we're playing it
                break;
            }
            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    b = C4Board();
                    break;
                case SDLK_g: 
                    runNN();
                    break;
                case SDLK_m:
                    runNN(true, true);
                    break;
                case SDLK_RETURN:
                    nn.toFile("nn.json");
                    break;
                }

                break;
            }
            }
        }

        // clears the screen
        SDL_RenderClear(rend);
        SDL_RenderCopy(rend, boardTex, NULL, NULL);

        SDL_Rect target = {0, 0, w / BWIDTH, h / BHEIGHT};

        for (int i = 0; i < BWIDTH; i++) {
            target.y = h;
            for (const auto elem : b.columns[i]) {
                target.y -= h / BHEIGHT;
                if (elem == C4State::NIL) continue;
                SDL_RenderCopy(rend, elem == C4State::RED ? red : yellow, NULL, &target);
            }
            target.x += w / BWIDTH;
        }

        // triggers the double buffers
        // for multiple rendering
        SDL_RenderPresent(rend);

        // calculates to 60 fps
        SDL_Delay(1000 / 60);
    }

    SDL_DestroyTexture(boardTex);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
