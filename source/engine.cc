#include "../include/engine.h"
#include <random>

using namespace morphy;

static uint64_t popcount64(uint64_t x) {
    x = (x & 0x5555555555555555ULL) + ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x & 0x0F0F0F0F0F0F0F0FULL) + ((x >> 4) & 0x0F0F0F0F0F0F0F0FULL);
    return (x * 0x0101010101010101ULL) >> 56;
}

static int roll(int min, int max) {
   double x = rand()/static_cast<double>(RAND_MAX);
   int that = min + static_cast<int>( x * (max - min) );
   return that;
}

int EngineConfig::pieceValue(PieceType type) const {
    return piece_values[static_cast<uint8_t>(type)];
}

int morphy::scorePieces (const EngineConfig& config, const Board& state, uint64_t mask) {
    return popcount64(state.pawns & mask) * config.pieceValue(PieceType::PAWN) +
           popcount64(state.bishops & mask) * config.pieceValue(PieceType::BISHOP) +
           popcount64(state.knights & mask) * config.pieceValue(PieceType::KNIGHT) +
           popcount64(state.rooks & mask) * config.pieceValue(PieceType::ROOK) +
           popcount64(state.queens & mask) * config.pieceValue(PieceType::QUEEN);
}

int morphy::scoreBoard (const EngineConfig& config, const Board& state) {
    return roll(-100,100);
}

Move morphy::findBestMove (const EngineConfig& config, MoveGenCache& genState, const Board& state){
    Move best;
    Move currentMove;
    int bestScore = std::numeric_limits<int>::min();
    for (auto& mi : genState.moves) {
        while (mi.nextMove(&currentMove)) {
            Board tmp = state;
            applyMove(tmp, currentMove);
            int score = -scoreBoard(config, tmp);
            if (score > bestScore) {
                bestScore = score;
                best = currentMove;
            }
        }
    }
    return best;
}

void Engine::clearState () {
    while (!_states.empty()) _states.pop();
}

Board& Engine::getState () {
    return _states.top();
}

void Engine::restart() {
    clearState();
    Board state;
    initializeBoard(state);
    _states.push(state);
    _genState = MoveGenCache{state};
}

void Engine::setBoard(const Board& board) {
    clearState();
    _states.emplace(board);
    _genState = MoveGenCache{board};
}

void Engine::makeMove (const Move& move) {
    Board newState = getState();
    ::applyMove(newState, move);
    //flipBoard(newState);
    _genState = MoveGenCache{newState};
    _states.emplace(newState);
}

Move Engine::makeMove() {
    generateAllLegalMoves(_genState, getState());
    Move m = findBestMove(config, _genState, getState());
    makeMove(m);
    return m;
}

void Engine::undoMove() {
    _states.pop();
    _genState = MoveGenCache{_states.top()};
}

UCIAdaptor::UCIAdaptor (Engine& engine, uci::IOPipe& pipe) :
    _engine(engine),
    _io(pipe),
    _isRunning(true)
{}

void UCIAdaptor::handleUCIMessage (const std::vector<std::string>& message) {
    if (message[0] == "isready") uci::signalReady(_io);
    else if (message[0] == "uci") {
        uci::UCIConfigurator()
                .setEngineName("Morphy")
                .setAuthorName("danem")
                .setHashRange(1, 128)
                .setELORange(1,20)
                .build(_io);
    }
    else if (message[0] == "ucinewgame") _engine.restart();
    else if (message[0] == "quit") _isRunning = false;
    else if (message[0] == "go") {
        Move m = _engine.makeMove();
        uci::signalBestMove(_io, m);
    }
    else if (message[0] == "position"){
        if (message[1] == "startpos") _engine.restart();
        if (message[2] == "moves") {
            uint16_t from;
            uint16_t to;
            for (size_t i = 3; i < message.size(); i++){
                if (!uci::parseMove(message[i], from, to)) {
                    uci::logMessage(_io,"Invalid move position supplied by GUI");
                    break;
                }

                PieceType t;
                if ((t = getPieceTypeAtCell(_engine.getState(), from)) == PieceType::NONE) {
                    uci::logMessage(_io, "GUI state and engine state out of sync. Quitting");
                    _isRunning = false;
                    break;
                }
                _engine.makeMove({t,from,to});
            }
            // morphy::printBoard(_engine.getState(), _io.logstream());
        }
    }
}

bool UCIAdaptor::isRunning () { return _isRunning; }




