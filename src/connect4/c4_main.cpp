#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>

#include <cstdlib>
#include <iostream>

#include <connect4/connect4.hpp>
#include <connect4/neural_net.hpp>

#define SEARCH_DEPTH 10

// how many training inputs to run before running backpropagation
#define BACKPROPS_EVERY 128

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

    int numRun = 0;
    NeuralNetwork nn;
    nn.randomize();
    nn.toFile("randomNet.json");
    nn.fromFile("nn-0.16.json");

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
            std::vector<C4Move> goodMovs = b.searchAll(SEARCH_DEPTH);
            Vector<7> expected;
            memset(expected.data, 0, 7 * sizeof(double));
            for (C4Move goodMov : goodMovs)
                expected[goodMov] = 1.0;
            nn.backprop(expected);
            if (++numRun > BACKPROPS_EVERY) {
                numRun = 0;
                nn.applyBackprops();
                std::cout << "======================[ BATCH COMPLETE ]===================" << std::endl;
            }

            if (playMinimax) b.make(randOf(goodMovs)); else b.randMove();
            if (checkWins()) {
                b = C4Board();
            }
            return;
        }

        Vector<7> res = nn.run();
        std::vector<std::pair<int,double>> sorted;
        for (int i = 0; i < 7; i++) {
            sorted.emplace_back(std::make_pair(i, res[i]));
            std::cout << "Neural net[" << i << "] = " << res[i] << std::endl;
        }

        std::sort(sorted.begin(), sorted.end(), [](const std::pair<int,double> &a, const std::pair<int,double> &b) { return a.second > b.second; });

        for (auto p : sorted) {
            std::cout << "pair " << p.first << ", " << p.second << std::endl;
            if (b.legal(p.first)) {
                b.make(p.first);
                std::cout << "CHOSE " << p.first << std::endl;
                break;
            }
        }

        if (checkWins()) {
            b = C4Board();
        }
    };

    bool isTraining = false;
    bool useMinimax = true;

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
                    runNN(false);
                    break;
                case SDLK_m:
                    isTraining ^= true;
                    break;
                case SDLK_r:
                    useMinimax ^= true;
                    std::cout << "USE MINIMAX = " << useMinimax << std::endl;
                    break;
                case SDLK_RETURN:
                    nn.toFile("nn.json");
                    break;
                }

                break;
            }
            }
        }

        if (isTraining) runNN(true, useMinimax);

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

    }

    SDL_DestroyTexture(boardTex);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
