#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <vector>
#include <stack>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>

#include "board.h"
#include "uci.h"

namespace morphy {


struct EngineConfig {
    RuleSet ruleset;
    int searchDepth;
    int theadCount;
    std::array<int,6> piece_values;
    int pieceValue (PieceType type) const;
};

const static EngineConfig DEFAULT_ENGINE_CONFIG {
    RuleSet::STANDARD,      // ruleset
    100,                    // search depth
    1,                      // thread count
    {{100,300,300,500,900}} // piece_values
};



class Engine {
private:
    // TODO: While this isn't necessary for UCI it is necesasry for other protocols.
    std::stack<Board> _states;
    MoveGenCache _genState;

    void clearState ();

public:
    EngineConfig config;

    Engine () :
        config(DEFAULT_ENGINE_CONFIG)
    {
        restart();
    }

    Engine (const EngineConfig& config) :
        config(config)
    {
        restart();
    }

    void restart ();
    void setBoard (const Board& board);
    std::vector<Move> getAvailableMoves ();
    std::vector<Move> getAvailableMoves (PieceType type, int pos);
    void undoMove ();
    void makeMove (const Move& move);
    Move makeMove ();
    Board& getState ();
};

class UCIAdaptor {
private:
    Engine& _engine;
    uci::IOPipe& _io;
    bool _isRunning;

public:

    UCIAdaptor (Engine& engine, uci::IOPipe& pipe);
    void handleUCIMessage (const std::vector<std::string>& message);
    bool isRunning ();
};


Move findBestMove (const EngineConfig& config, MoveGenCache& genState, const Board& state);
int scoreBoard (const EngineConfig& config, const Board& state);
int scorePieces (const EngineConfig& config, const Board& state, uint64_t mask);

} // end namespace

#endif // ENGINE_H
