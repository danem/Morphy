#pragma once

#include <stdint.h>
#include <vector>
#include <array>
#include <initializer_list>


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

    Move () {}

    Move (PieceType type, uint16_t from, uint16_t to) :
        type(type), from(from), to(to)
    {}
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

// Struct for caching calculated attribs
// during move generation and validation.
struct MoveGenState {
    std::vector<uint8_t> pinnedPieces;
    std::vector<MoveIterator> moves;
    uint64_t allPieces;
    uint64_t enemyPieces;
    MoveGenState (const Board& board);
};


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

MoveIterator generateMoveMask (MoveGenState& genState, const Board& state, const Vec2& pos, const PieceType& type);
void generateAllMoves (MoveGenState& genState, const Board& state);

std::vector<Move> threatsToCells (const MoveGenState& genState, const Board& board, const std::initializer_list<Vec2>& positions);
std::vector<Move> threatsToCell (const MoveGenState& genState, const Board& board, const Vec2& pos);

bool validateMove (const MoveGenState& genState, const Board& state, const Move& move);
void applyMove (Board& state, const Move& move);
Move findBestMove (const MoveGenState& genState, const Board& state, std::vector<MoveIterator>& moves);
int scoreBoard (const Board& state);

void testMasks();
void printBoard (const Board& board);

} // end namespace
