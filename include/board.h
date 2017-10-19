#pragma once

#include <stdint.h>
#include <vector>


namespace morphy {

const int CASTLE_KINGSIDE = 0x1;
const int CASTLE_QUEENSIDE = 0x2;

enum class PieceType {
    PAWN, ROOK, BISHOP, KNIGHT, QUEEN, KING
};

enum class PieceColor {
    WHITE, BLACK
};


struct Vec2 {
    int16_t x;
    int16_t y;

    Vec2 (int16_t rank, int16_t file) :
        x(rank),
        y(file)
    {}

    Vec2 (int16_t idx) :
        x(idx%8),
        y(idx/8)
    {}

    int16_t idx () const { return y * 8 + x; }
    operator int16_t() const { return idx(); }
};

struct Move {
    PieceType type;
    uint8_t from;
    uint8_t to;
};

struct MoveCandidate {
    PieceType type;
    uint64_t pieces;          // target piece is first bit set
    uint64_t availableMoves;  //
};

struct Board {
    PieceColor color;
    uint64_t rooks;
    uint64_t bishops;
    uint64_t knights;
    uint64_t queens;
    uint64_t kings;
    uint64_t pawns;
    uint64_t castle_flags;
    uint32_t en_passant_sq;
    uint8_t double_moves;
};

struct GameState {
    Board whiteBoard;
    Board blackBoard;
};

uint64_t all_pieces (const Board& board);
uint64_t all_pieces (const GameState& state);
uint64_t flip_bb(uint64_t x);
uint64_t* getPieceBoard (Board& board, const PieceType& type);
void setPiece (Board& board, PieceType& type, const Vec2& pos);
void clearPiece (Board& board, PieceType& type, const Vec2& pos) ;
void initializeBoard (Board& board, PieceColor color);
void initializeGameState (GameState& state);
uint64_t generateMoveMask (GameState& state, const Vec2& pos, const PieceColor& color, const PieceType& type);
void generateAllPieceMoves (std::vector<uint64_t>& masks, GameState& state, const PieceType& type, const PieceColor& color);
std::vector<uint64_t> generateAllMoves (GameState& state, const PieceColor& color);
bool applyMove (Board& board, const Move& move);
void testMasks();

} // end namespace
