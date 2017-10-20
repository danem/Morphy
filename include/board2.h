#pragma once

#include <stdint.h>
#include <vector>
#include <array>

namespace morphy {

enum class PieceType {
    PAWN, ROOK, BISHOP, KNIGHT, QUEEN, KING
};

static const std::array<PieceType,6> all_piece_types {
    {PieceType::PAWN, PieceType::ROOK, PieceType::BISHOP,
     PieceType::KNIGHT, PieceType::QUEEN, PieceType::KING}
};

enum class PieceColor {
    WHITE=0, BLACK=1
};

static const std::array<PieceColor,2> all_piece_colors{
    {PieceColor::WHITE, PieceColor::BLACK}
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

void flipBoard (Board& board);
uint64_t all_pieces (const Board& board);

uint64_t* getPieceBoard (Board& board, const PieceType& type);
const uint64_t* getPieceBoard (const Board& board, const PieceType& type);

void setPiece (Board& board, PieceType& type, const Vec2& pos);
void clearPiece (Board& board, PieceType& type, const Vec2& pos) ;

void initializeBoard (Board& board, PieceColor color);

uint64_t generateMoveMask (GameState& state, const Vec2& pos, const PieceColor& color, const PieceType& type);
void generateAllPieceMoves (std::vector<uint64_t>& masks, GameState& state, const PieceType& type, const PieceColor& color);
std::vector<uint64_t> generateAllMoves (GameState& state, const PieceColor& color);
int getNextMove (uint64_t& mask);
bool applyMove (GameState& state, const Move& move);
void testMasks();


}

