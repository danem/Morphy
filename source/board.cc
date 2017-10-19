#include "../include/board.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>

#define BIT_MASK(idx) (static_cast<uint64_t>(1) << (idx))
#define SET_BIT(v,idx) ((v) | BIT_MASK(idx))
#define CLEAR_BIT(v,idx) ((v) ^ BIT_MASK(idx))
#define CHECK_BIT(v,idx) (((v) & BIT_MASK(idx)) == BIT_MASK(idx))
#define INVERT_MASK(v) ((v) ^ 0xffffffffffffffff)
#define LSB_FIRST(v) (__builtin_ffsll(v))
#define MSB_FIRST(v) (__builtin_clzll(v))
#define ROW_MAJOR(x,y) (y * 8 + x)

using namespace morphy;

enum Direction {
    NORTH, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST
};

static const Vec2 dir_vectors[] = {{0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}};
static const std::array<PieceType,6> all_piece_types{{PieceType::PAWN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT, PieceType::QUEEN, PieceType::KING}};

static uint64_t rank_mask (uint8_t rank) {
    return static_cast<uint64_t>(0xff) << static_cast<uint64_t>(rank * 8);
}

static uint64_t file_mask (uint8_t file) {
    return static_cast<uint64_t>(0x101010101010101) << file;
}

static const uint64_t two_guard = file_mask(0) | file_mask(1);
static const uint64_t three_guard = two_guard | file_mask(2);

// https://chessprogramming.wikispaces.com/On+an+empty+Board#RayAttacks
static uint64_t diagonal_mask(uint8_t center) {
   const uint64_t maindia = 0x8040201008040201;
   int diag =8*(center & 7) - (center & 56);
   int nort = -diag & ( diag >> 31);
   int sout =  diag & (-diag >> 31);
   return (maindia >> sout) << nort;
}

static uint64_t antidiagonal_mask(uint8_t center) {
  const uint64_t maindia = 0x0102040810204080;
  int diag =56- 8*(center&7) - (center&56);
  int nort = -diag & ( diag >> 31);
  int sout =  diag & (-diag >> 31);
  return (maindia >> sout) << nort;
}

// https://chessprogramming.wikispaces.com/Flipping+Mirroring+and+Rotating
// See 'flipDiagA1H8'
static uint64_t flipDiagA1H8(uint64_t x) {
  uint64_t t;
  const uint64_t k1 = 0x5500550055005500;
  const uint64_t k2 = 0x3333000033330000;
  const uint64_t k4 = 0x0f0f0f0f00000000;
  t  = k4 & (x ^ (x << 28));
  x ^=       t ^ (t >> 28) ;
  t  = k2 & (x ^ (x << 14));
  x ^=       t ^ (t >> 14) ;
  t  = k1 & (x ^ (x <<  7));
  x ^=       t ^ (t >>  7) ;
  return x;
}

static uint64_t mirror_horizontal (uint64_t x) {
   const uint64_t k1 = 0x5555555555555555;
   const uint64_t k2 = 0x3333333333333333;
   const uint64_t k4 = 0x0f0f0f0f0f0f0f0f;
   x = ((x >> 1) & k1) +  2*(x & k1);
   x = ((x >> 2) & k2) +  4*(x & k2);
   x = ((x >> 4) & k4) + 16*(x & k4);
   return x;
}

static uint64_t flip_vertical(uint64_t x)
{
  return
    ( (x << 56)                        ) |
    ( (x << 40) & (0x00ff000000000000) ) |
    ( (x << 24) & (0x0000ff0000000000) ) |
    ( (x <<  8) & (0x000000ff00000000) ) |
    ( (x >>  8) & (0x00000000ff000000) ) |
    ( (x >> 24) & (0x0000000000ff0000) ) |
    ( (x >> 40) & (0x000000000000ff00) ) |
    ( (x >> 56)                        );
}

// TODO: This is probably unnecessarily slow
static uint64_t rect_mask (const Vec2& center, const Vec2& size) {
    uint64_t res = 0;
    uint16_t hw = size.x / 2;
    uint16_t hh = size.y / 2;
    for (int x = center.x-hw; x <= center.x+hw; x++){
        for (int y = center.y-hh; y <= center.y+hh; y++){
            if (x >= 0 && y >= 0 && x < 8 && y < 8) res |= BIT_MASK(ROW_MAJOR(x,y));
        }
    }
    return res;
}

static uint64_t rayUntilBlocked (uint64_t own, uint64_t enemy, const Vec2& pos, Direction dir) {
    uint64_t mask = 0;
    Vec2 dirV = dir_vectors[dir];
    bool hit = false;
    Vec2 tmp = pos;
    while (tmp.x >= 0 && tmp.x < 8 && tmp.y >= 0 && tmp.y < 8 && !hit) {
        if (CHECK_BIT(own,tmp.idx())) { hit = true; continue; }
        else if (CHECK_BIT(enemy,tmp.idx())) hit = true; // no continue so we mark the spot as available
        mask = SET_BIT(mask,tmp.idx());
        tmp.x += dirV.x; tmp.y += dirV.y;
    }
    return mask;
}


static uint64_t knight_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    uint64_t tl = 258;
    uint64_t bl = 513;
    int16_t idx = pos.idx();
    uint64_t mask = (tl << (idx + 9)) | (bl << (idx + 6));
    if (idx - 15 >= 0) mask |= bl << (idx - 15);
    if (idx - 18 >= 0) mask |= tl << (idx - 18);
    if (pos.x <= 1) mask &= three_guard;
    else if (pos.x >= 6) mask &= (three_guard << 5);
    return (mask & enemy) | (mask & ~own);
}

static uint64_t bishop_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    return rayUntilBlocked(own,enemy, pos, NORTHEAST)
         | rayUntilBlocked(own,enemy, pos, NORTHWEST)
         | rayUntilBlocked(own,enemy, pos, SOUTHEAST)
         | rayUntilBlocked(own,enemy, pos, SOUTHWEST);
}

static uint64_t rook_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    return rayUntilBlocked(own,enemy, pos, NORTH)
         | rayUntilBlocked(own,enemy, pos, SOUTH)
         | rayUntilBlocked(own,enemy, pos, EAST)
         | rayUntilBlocked(own,enemy, pos, WEST);
}

static uint64_t queen_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    return rook_mask(own,enemy,pos) | bishop_mask(own,enemy,pos);
}

static uint64_t king_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    uint64_t mask = rect_mask(pos,{3,3});
    return (mask & enemy) | (mask & ~own);
}

static uint64_t pawn_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    uint64_t mask = SET_BIT(0,ROW_MAJOR(pos.x,pos.y+2))
                  | SET_BIT(0,ROW_MAJOR(pos.x,pos.y+3))
                  | (SET_BIT(0,ROW_MAJOR(pos.x-1,pos.y+2)) & enemy)
                  | (SET_BIT(0,ROW_MAJOR(pos.x+1,pos.y+2)) & enemy);
    if (pos.x == 7) mask &= three_guard << 6;
    else if (pos.x == 0) mask &= three_guard;
    return mask;
}

static uint64_t castle_mask (uint64_t own, uint64_t enemy, uint8_t flags, const Vec2& pos) {
    uint64_t mask = 0;
    if (pos.idx() != 3) return 0;
    // Treat own pieces as capture. This way we can check if king
    // has clear path to rook.
    if (CHECK_BIT(flags,CASTLE_KINGSIDE) && rayUntilBlocked(enemy,own,pos,Direction::EAST) == 15) mask |= 2;
    if (CHECK_BIT(flags,CASTLE_QUEENSIDE) && rayUntilBlocked(enemy,own,pos,Direction::WEST) == 0xf8) mask |= 0x20;
    return mask;
}

static void testMask (const char * name, uint64_t target, uint64_t real) {
    bool passed = target == real;
    std::cout << name << ": " ;
    if (passed) std::cout << "PASSED!\n";
    else std::cout << "FAILED " << real << " != " << target << "\n";
}

void morphy::testMasks () {
    testMask("rank 0", 0xff, rank_mask(0));
    testMask("rank 7", 0xff00000000000000, rank_mask(7));
    testMask("file 0", 0x101010101010101, file_mask(0));
    testMask("file 7", 0x8080808080808080, file_mask(7));
    testMask("file 7", 0x8080808080808080, file_mask(7));
    testMask("rect mask 3,3 3,3", 0x1c1c1c0000, rect_mask({3,3},{3,3}));
    testMask("rect mask 3,3 1,1", 0x8000000, rect_mask({3,3},{1,1}));
    testMask("ray north", 0x808080808000000, rayUntilBlocked(0,0,{3,3},Direction::NORTH));
    testMask("ray south", 0x8080808, rayUntilBlocked(0,0,{3,3},Direction::SOUTH));
    testMask("ray east", 0xf000000, rayUntilBlocked(0,0,{3,3},Direction::EAST));
    testMask("ray west", 0xf8000000, rayUntilBlocked(0,0,{3,3},Direction::WEST));
    testMask("ray northeast", 0x1020408000000, rayUntilBlocked(0,0,{3,3},Direction::NORTHEAST));
    testMask("ray northwest", 0x8040201008000000, rayUntilBlocked(0,0,{3,3},Direction::NORTHWEST));
    testMask("ray southeast", 0x8040201, rayUntilBlocked(0,0,{3,3},Direction::SOUTHEAST));
    testMask("ray southwest", 0x8102040, rayUntilBlocked(0,0,{3,3},Direction::SOUTHWEST));
    testMask("bishop mask 3,3", 0x8041221408142241, bishop_mask(0,0,{3,3}));
    testMask("rook mask 3,3", 0x8080808ff080808, rook_mask(0,0,{3,3}));
    testMask("queen mask 3,3", 0x88492a1cff1c2a49, queen_mask(0,0,{3,3}));
    testMask("queen mask 3,0",0x8080888492a1cff, queen_mask(0,0,{3,0}));
    testMask("king mask 0,0",0x303, king_mask(0,0,{0,0}));
    testMask("knight 3,3", 0x142200221400, knight_mask(0,0,{3,3}));
    testMask("knight 0,3", 0x20400040200, knight_mask(0,0,{0,3}));
    testMask("knight 7,3", 0x402000204000, knight_mask(0,0,{7,3}));
    testMask("pawn 0,1", 0x1030000, pawn_mask(0,0xffffffffffffffff,{0,1}));
    testMask("pawn 7,1", 0x80c00000, pawn_mask(0,0,{7,1}));

    GameState gs;
    initializeGameState(gs);
    testMask("bishop move", 0, generateMoveMask(gs, {2,0}, PieceColor::WHITE, PieceType::BISHOP));
    testMask("queen move", 0, generateMoveMask(gs, {4,0}, PieceColor::WHITE, PieceType::QUEEN));
    testMask("king move", 0, generateMoveMask(gs, {3,0}, PieceColor::WHITE, PieceType::KING));
    testMask("rook move", 0, generateMoveMask(gs, {0,0}, PieceColor::WHITE, PieceType::ROOK));
    testMask("rook move 2", 0, generateMoveMask(gs, {7,0}, PieceColor::WHITE, PieceType::ROOK));
    testMask("knight move", 0x50000, generateMoveMask(gs, {1,0}, PieceColor::WHITE, PieceType::KNIGHT));
    testMask("knight move 2",0xa00000, generateMoveMask(gs, {6,0}, PieceColor::WHITE, PieceType::KNIGHT));
    testMask("pawn move", 0x1010000, generateMoveMask(gs, {0,1}, PieceColor::WHITE, PieceType::PAWN));
    testMask("pawn move", 0x80800000, generateMoveMask(gs, {7,1}, PieceColor::WHITE, PieceType::PAWN));
    testMask("rook capture", 0x80808ff080000, generateMoveMask(gs,{3,3}, PieceColor::WHITE, PieceType::ROOK));

    std::vector<uint64_t> moves = generateAllMoves(gs, PieceColor::WHITE);
    for (auto& v : moves){
        std::cout << v << std::endl;
    }

}


void morphy::setPiece (Board& board, PieceType& type, const Vec2& pos) {
    uint64_t* bb = getPieceBoard(board, type);
    *bb = SET_BIT(*bb, pos);
}

void morphy::clearPiece (Board& board, PieceType& type, const Vec2& pos) {
    uint64_t* bb = getPieceBoard(board, type);
    *bb = CLEAR_BIT(*bb, pos);
}

void morphy::initializeBoard (Board& board, PieceColor color) {
    board.rooks   = SET_BIT(0,ROW_MAJOR(0,0)) | SET_BIT(0,ROW_MAJOR(7,0));
    board.knights = SET_BIT(0,ROW_MAJOR(1,0)) | SET_BIT(0,ROW_MAJOR(6,0));
    board.bishops = SET_BIT(0,ROW_MAJOR(2,0)) | SET_BIT(0,ROW_MAJOR(5,0));
    board.queens  = SET_BIT(0,ROW_MAJOR(3,0));
    board.kings   = SET_BIT(0,ROW_MAJOR(4,0));
    board.pawns   = 0xff << 8;
    board.castle_flags = CASTLE_KINGSIDE | CASTLE_QUEENSIDE;
    board.double_moves = 0xff;
    board.color = color;
    if (color == PieceColor::BLACK) {
        board.rooks = flip_bb(board.rooks);
        board.knights = flip_bb(board.knights);
        board.bishops = flip_bb(board.bishops);
        board.queens = flip_bb(board.queens);
        board.kings = flip_bb(board.kings);
        board.pawns = flip_bb(board.pawns);
    }
}

void morphy::initializeGameState (GameState& state) {
    initializeBoard(state.whiteBoard, PieceColor::WHITE);
    initializeBoard(state.blackBoard, PieceColor::BLACK);
}


uint64_t morphy::all_pieces (const Board& board) {
    return board.rooks
         | board.bishops
         | board.knights
         | board.queens
         | board.kings
         | board.pawns;
}

uint64_t morphy::all_pieces (const GameState& state){
    return all_pieces(state.whiteBoard)
         | all_pieces(state.blackBoard);
}

uint64_t morphy::flip_bb(uint64_t x) {
  return __builtin_bswap64(x);
}

uint64_t* morphy::getPieceBoard (Board& board, const PieceType& type) {
    uint64_t* bb;
    switch (type) {
    case PieceType::ROOK:   bb = &board.rooks; break;
    case PieceType::BISHOP: bb = &board.bishops; break;
    case PieceType::KNIGHT: bb = &board.knights; break;
    case PieceType::QUEEN:  bb = &board.queens; break;
    case PieceType::KING:   bb = &board.kings; break;
    case PieceType::PAWN:   bb = &board.pawns; break;
    }
    return bb;
}

uint64_t morphy::generateMoveMask (GameState& state, const Vec2& pos, const PieceColor& color, const PieceType& type) {
    uint64_t mask = 0;
    uint64_t wboard = all_pieces(state.whiteBoard);
    uint64_t bboard = all_pieces(state.blackBoard);
    Board& ownB     = color == PieceColor::WHITE ? state.whiteBoard : state.blackBoard;
    uint64_t own    = color == PieceColor::WHITE ? wboard : bboard;
    uint64_t enemy  = color == PieceColor::WHITE ? bboard : wboard;

    switch(type) {
    case PieceType::ROOK:   mask = rook_mask(own,enemy,pos); break;
    case PieceType::KNIGHT: mask = knight_mask(own,enemy,pos); break;
    case PieceType::BISHOP: mask = bishop_mask(own,enemy,pos); break;
    case PieceType::QUEEN:  mask = queen_mask(own,enemy,pos); break;
    case PieceType::PAWN:{
        mask = pawn_mask(own,enemy,pos);
        if (color == PieceColor::BLACK) mask = flip_vertical(mask);
        break;
    }
    case PieceType::KING:{
        mask = king_mask(own,enemy,pos);
        if (ownB.castle_flags != 0) mask |= castle_mask(own,enemy,ownB.castle_flags,pos);
        break;
    }
    }
    return mask;
}

void morphy::generateAllPieceMoves (std::vector<uint64_t>& masks, GameState& state, const PieceType& type, const PieceColor& color) {
    Board& board = color == PieceColor::WHITE ? state.whiteBoard : state.blackBoard;
    uint64_t bb = *getPieceBoard(board, type);
    int idx = LSB_FIRST(bb);
    while (idx != 0) {
        Vec2 pos(idx-1);
        masks.emplace_back(generateMoveMask(state, pos, color,type));
        bb = CLEAR_BIT(bb,idx-1);
        idx = LSB_FIRST(bb);
    }
}

std::vector<uint64_t> morphy::generateAllMoves (GameState& state, const PieceColor& color) {
    std::vector<uint64_t> masks;
    for (const PieceType& t : all_piece_types) {
        generateAllPieceMoves(masks, state, t, color);
    }
    return masks;
}


