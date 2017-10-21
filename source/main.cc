#include "../include/board.h"
#include <vector>
#include <array>
#include <iostream>
#include <time.h>
#include <random>

using namespace morphy;

/*
int roll(int min, int max) {
   double x = rand()/static_cast<double>(RAND_MAX);
   int that = min + static_cast<int>( x * (max - min) );
   return that;
}
*/

int main () {
    //testMasks();
    std::cout << L"♚" << std::endl;
    srand(time(NULL));

    Board state;
    initializeBoard(state);

    for (int i = 0; i < 40; i++){
        MoveGenState gs{state};
        generateAllMoves(gs, state);
        Move move = findBestMove(gs, state, gs.moves);
        applyMove(state, move);

        std::cout << "--------------------\n";
        std::cout << "--------------------\n";
        printBoard(state);
        flipBoard(state);
    }

}
