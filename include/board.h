#pragma once

#include <stdint.h>
#include <vector>
#include <array>


namespace morphy {

const uint8_t NO_CASTLE = 0;
const uint8_t CASTLE_KINGSIDE = 1 << 0;
const uint8_t CASTLE_QUEENSIDE = 1 << 2;

enum class PieceType {
    PAWN, ROOK, BISHOP, KNIGHT, QUEEN, KING
};

static const std::array<PieceType,6> all_piece_types{
    {PieceType::PAWN, PieceType::ROOK, PieceType::BISHOP,
     PieceType::KNIGHT, PieceType::QUEEN, PieceType::KING}
};

enum class PieceColor {
    WHITE=0, BLACK=1
};
static const std::array<PieceColor,2> all_piece_colors{{PieceColor::WHITE, PieceColor::BLACK}};


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
    uint16_t from;
    uint16_t to;
};

struct MaskIterator {
    uint64_t mask;
    bool hasBits () const;
    bool nextBit (uint16_t* dest);
    int bitCount () const;
};

struct MoveIterator {
    PieceType type;
    uint16_t from;
    MaskIterator maskIter;
    bool hasMoves () const;
    int moveCount () const;
    bool nextMove (Move* dest);
};

struct Board {
    // all bitboards contain pieces for both white
    // and black. To get white rooks, rooks & current_bb.
    // To get black rooks, (all_pieces ^ current_bb) & rooks.
    // All functions are from white's perspective.
    uint64_t rooks;
    uint64_t bishops;
    uint64_t knights;
    uint64_t queens;
    uint64_t kings;
    uint64_t pawns;
    uint32_t en_passant_sq;
    uint8_t current_double_moves;
    uint8_t other_double_moves;
    uint8_t current_castle_flags;
    uint8_t other_castle_flags;
    uint64_t current_bb;
    bool is_white;
};

/*
namespace detail {

    uint64_t diagonal_mask(uint8_t center);
    uint64_t diagonal_mask(uint8_t center);
    uint64_t flipDiagA1H8(uint64_t x);
    uint64_t mirror_horizontal (uint64_t x);
    uint64_t flip_vertical(uint64_t x);
    uint64_t rect_mask (const Vec2& center, const Vec2& size);
    //uint64_t rayUntilBlocked (uint64_t own, uint64_t enemy, const Vec2& pos, Direction dir);
    uint64_t knight_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t bishop_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t rook_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t queen_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t king_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t pawn_attack_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t pawn_mask (uint64_t own, uint64_t enemy, const Vec2& pos);
    uint64_t castle_mask (uint64_t own, uint64_t enemy, uint8_t flags, const Vec2& pos);

} // end namespace
*/



void initializeBoard (Board& board);
void flipBoard (Board& board);

void setPiece (Board& board, PieceType& type, const Vec2& pos);
void clearPiece (Board& board, PieceType& type, const Vec2& pos) ;

uint64_t all_pieces (const Board& board);
uint64_t enemy_pieces (const Board& board);

// eg Get ALL rooks
uint64_t* getPieceBoard (Board& board, const PieceType& type);
const uint64_t* getPieceBoard (const Board& board, const PieceType& type);

// eg Get white rooks
uint64_t getPieceBoard (Board& board, uint64_t mask, const PieceType& type);
uint64_t getPieceBoard (const Board& board, uint64_t mask, const PieceType& type);

MoveIterator generateMoveMask (const Board& state, const Vec2& pos, const PieceType& type);
std::vector<MoveIterator> generateAllMoves (const Board& state);

std::vector<Move> threatsToCell (const Board& board, const Vec2& pos);

bool validateMove (const Board& state, const Move& move);
void applyMove (Board& state, const Move& move);
Move bestMove (const Board& state, std::vector<MoveIterator>& moves);
int scoreBoard (const Board& state);

void testMasks();
void printBoard (const Board& board);

} // end namespace
