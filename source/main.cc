#include "../include/board.h"
#include <vector>
#include <array>
#include <iostream>
#include <time.h>
#include <random>

using namespace morphy;

int roll(int min, int max) {
   double x = rand()/static_cast<double>(RAND_MAX);
   int that = min + static_cast<int>( x * (max - min) );
   return that;
}

int main () {
    //morphy::testMasks();
    testMasks();
    Board state;
    initializeBoard(state);
    printBoard(state);

    std::cout << "----------------------\n";

    //applyMove(state,{PieceType::PAWN, 8, 16});
    //printBoard(state);

    for (int i = 0; i < 6; i++){
        std::vector<MoveIterator> moveIters = generateAllMoves(state);
        MoveIterator& moves = moveIters[roll(0, moveIters.size())-1];
        while (!moves.hasMoves()){
            moves = moveIters[roll(0, moveIters.size())-1];
        }

        int moveNum = roll(0, moves.moveCount());
        int idx;
        Move move;
        do {
            moves.nextMove(&move);
            idx += 1;
        } while (idx < moveNum);
        applyMove(state,move);

        std::cout << "----------------------\n";

        printBoard(state);
        //std::cout << all_pieces(state) << std::endl;
        flipBoard(state);
    }
}
