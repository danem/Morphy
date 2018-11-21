#include "../include/board.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <random>

#define BIT_MASK(idx) (static_cast<uint64_t>(1) << (idx))
#define SET_BIT(v,idx) ((v) | BIT_MASK(idx))
#define CLEAR_BIT(v,idx) ((v) & ~BIT_MASK(idx))
#define MOVE_BIT(v,from,to) (SET_BIT(CLEAR_BIT((v),(from)),(to)))
#define CHECK_BIT(v,idx) (((v) & BIT_MASK(idx)) == BIT_MASK(idx))
#define INVERT_MASK(v) ((v) ^ 0xffffffffffffffff)
#define LSB_FIRST(v) (__builtin_ffsll(v))
#define MSB_FIRST(v) (__builtin_clzll(v))
#define ROW_MAJOR(x,y) ((y) * 8 + (x))
#define FLIP_BB(bb) __builtin_bswap64(bb)

using namespace morphy;

MoveGenCache::MoveGenCache (const Board& board) :
    allPieces(all_pieces(board)),
    enemyPieces(enemy_pieces(board))
{}

bool MaskIterator::hasBits() const {
    return mask != 0;
}

bool MaskIterator::nextBit(uint16_t * dest){
    int idx = LSB_FIRST(mask);
    if (idx == 0) return false;
    mask = CLEAR_BIT(mask, idx-1);
    *dest = static_cast<int16_t>(idx - 1);
    return true;
}

int MaskIterator::bitCount() const {
    return __builtin_clzll(mask);
}

 void MaskIterator::clearBit(uint16_t idx) {
     mask = CLEAR_BIT(mask, idx);
 }

bool MoveIterator::hasMoves() const {
    return maskIter.hasBits();
}

bool MoveIterator::hasMove(const Move& move) const {
    return type == move.type
        && from == move.from
        && CHECK_BIT(maskIter.mask,move.to);
}

int MoveIterator::moveCount() const {
    return maskIter.bitCount();
}

bool MoveIterator::nextMove(Move* move){
    if (!hasMoves()) return false;
    move->type = type;
    move->from = from;
    maskIter.nextBit(&move->to);
    return true;
}

void MoveIterator::clearMove(uint16_t idx) {
    maskIter.clearBit(idx);
}

enum Direction {
    NORTH, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST
};

static const Vec2 dir_vectors[] = {{0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}};


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
    //tmp.x += dirV.x; tmp.y += dirV.y;
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
    return mask & ~own;
}

static uint64_t bishop_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    return CLEAR_BIT(rayUntilBlocked(own,enemy, pos, NORTHEAST)
         | rayUntilBlocked(own,enemy, pos, NORTHWEST)
         | rayUntilBlocked(own,enemy, pos, SOUTHEAST)
         | rayUntilBlocked(own,enemy, pos, SOUTHWEST), pos.idx());
}

static uint64_t rook_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    return CLEAR_BIT(rayUntilBlocked(own,enemy, pos, NORTH)
         | rayUntilBlocked(own,enemy, pos, SOUTH)
         | rayUntilBlocked(own,enemy, pos, EAST)
         | rayUntilBlocked(own,enemy, pos, WEST), pos.idx());
}

static uint64_t queen_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    return rook_mask(own,enemy,pos) | bishop_mask(own,enemy,pos);
}

static uint64_t king_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    uint64_t mask = rect_mask(pos,{3,3});
    if (pos.idx() == 3) mask |= SET_BIT(0,1) | SET_BIT(0,6); // castle squares
    return CLEAR_BIT((mask & enemy) | (mask & ~own), pos.idx());
}

static uint64_t pawn_attack_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    uint64_t mask = (SET_BIT(0,ROW_MAJOR(pos.x-1,pos.y+1)) & enemy)
                  | (SET_BIT(0,ROW_MAJOR(pos.x+1,pos.y+1)) & enemy);
    if (pos.x == 7) mask &= three_guard << 5;
    else if (pos.x == 0) mask &= three_guard;
    return CLEAR_BIT(mask,pos.idx());

}

static uint64_t pawn_mask (uint64_t own, uint64_t enemy, const Vec2& pos) {
    uint64_t mask = (SET_BIT(0,ROW_MAJOR(pos.x,pos.y+1)) & ~enemy)
                  | (SET_BIT(0,ROW_MAJOR(pos.x,pos.y+2)) & ~enemy)
                  | (SET_BIT(0,ROW_MAJOR(pos.x-1,pos.y+1)) & enemy)
                  | (SET_BIT(0,ROW_MAJOR(pos.x+1,pos.y+1)) & enemy);
    if (pos.x == 7) mask &= three_guard << 5;
    else if (pos.x == 0) mask &= three_guard;
    return CLEAR_BIT(mask,pos.idx());
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

void morphy::initializeBoard (Board& board) {
    board.rooks   = SET_BIT(0,ROW_MAJOR(0,0)) | SET_BIT(0,ROW_MAJOR(7,0)) | SET_BIT(0,ROW_MAJOR(0,7)) | SET_BIT(0,ROW_MAJOR(7,7));
    board.knights = SET_BIT(0,ROW_MAJOR(1,0)) | SET_BIT(0,ROW_MAJOR(6,0)) | SET_BIT(0,ROW_MAJOR(1,7)) | SET_BIT(0,ROW_MAJOR(6,7));
    board.bishops = SET_BIT(0,ROW_MAJOR(2,0)) | SET_BIT(0,ROW_MAJOR(5,0)) | SET_BIT(0,ROW_MAJOR(2,7)) | SET_BIT(0,ROW_MAJOR(5,7));
    board.queens  = SET_BIT(0,ROW_MAJOR(3,0)) | SET_BIT(0,ROW_MAJOR(3,7));
    board.kings   = SET_BIT(0,ROW_MAJOR(4,0)) | SET_BIT(0,ROW_MAJOR(4,7));
    board.pawns   = 0xff00 | 0xff000000000000;
    board.current_castle_flags = CASTLE_KINGSIDE | CASTLE_QUEENSIDE;
    board.other_castle_flags = board.current_castle_flags;
    board.current_bb = 0xffff;
    board.is_white = true;
}

void morphy::flipBoard (Board& board) {
    board.current_bb ^= all_pieces(board);
    board.current_bb = FLIP_BB(board.current_bb);
    board.bishops = FLIP_BB(board.bishops);
    board.rooks   = FLIP_BB(board.rooks);
    board.queens  = FLIP_BB(board.queens);
    board.pawns   = FLIP_BB(board.pawns);
    board.knights = FLIP_BB(board.knights);
    board.kings   = FLIP_BB(board.kings);
    board.is_white = !board.is_white;
}

void morphy::setPiece (Board& board, PieceType& type, const Vec2& pos) {
    uint64_t* bb = getPieceBoard(board, type);
    *bb = SET_BIT(*bb, pos);
}

void morphy::clearPiece (Board& board, PieceType& type, const Vec2& pos) {
    uint64_t* bb = getPieceBoard(board, type);
    *bb = CLEAR_BIT(*bb, pos);
}

uint64_t morphy::all_pieces (const Board& board) {
    return board.rooks
         | board.bishops
         | board.knights
         | board.queens
         | board.kings
         | board.pawns;
}

uint64_t morphy::enemy_pieces (const Board& board) {
    return all_pieces(board) ^ board.current_bb;
}


const uint64_t* morphy::getPieceBoard (const Board& board, const PieceType& type) {
    switch (type) {
    case PieceType::ROOK:   return &board.rooks;
    case PieceType::BISHOP: return &board.bishops;
    case PieceType::KNIGHT: return &board.knights;
    case PieceType::QUEEN:  return &board.queens;
    case PieceType::KING:   return &board.kings;
    case PieceType::PAWN:   return &board.pawns;
    case PieceType::NONE:   return nullptr;
    }
}

uint64_t* morphy::getPieceBoard (Board& board, const PieceType& type) {
    const uint64_t* res = getPieceBoard(static_cast<const Board&>(board),type);
    return const_cast<uint64_t*>(res);
}

uint64_t morphy::getPieceBoard (Board& board, uint64_t mask, const PieceType& type) {
    return (*getPieceBoard(board,type)) & mask;
}

uint64_t morphy::getPieceBoard (const Board& board, uint64_t mask, const PieceType& type) {
    return (*getPieceBoard(board,type)) & mask;
}

bool morphy::cellOccupiedByType (const Board& board, const PieceType type, uint16_t cell) {
    return CHECK_BIT(*getPieceBoard(board, type), cell);
}

PieceType morphy::getPieceTypeAtCell (const Board& board, uint16_t cell) {
    PieceType type = PieceType::NONE;
    for (const auto v : all_piece_types) {
        type = v;
        if (type == PieceType::NONE) break;
        if (cellOccupiedByType(board, v, cell)) break;
    }
    return type;

}

MoveIterator morphy::generateMoveMask (MoveGenCache& genState, const Board& state, const Vec2& pos, const PieceType& type) {
    uint64_t mask = 0;
    uint64_t own = state.current_bb;
    uint64_t enemy = genState.enemyPieces;

    switch(type) {
    case PieceType::ROOK:   mask = rook_mask(own,enemy,pos); break;
    case PieceType::KNIGHT: mask = knight_mask(own,enemy,pos); break;
    case PieceType::BISHOP: mask = bishop_mask(own,enemy,pos); break;
    case PieceType::QUEEN:  mask = queen_mask(own,enemy,pos); break;
    case PieceType::PAWN:   mask = pawn_mask(own,enemy,pos); break;
    case PieceType::KING:{
        mask = king_mask(own,enemy,pos);
        if (state.current_castle_flags != 0) mask |= castle_mask(own,enemy,state.current_castle_flags,pos);
        break;
    }
    case PieceType::NONE: mask = 0;
    }
    return {type, static_cast<uint16_t>(pos.idx()), {mask}};
}

void morphy::generateAllMoves (MoveGenCache& genState, const Board& state) {
    uint64_t moveCount = 0;
    for (const PieceType& t : all_piece_types) {
        if (t == PieceType::NONE) continue;
        MaskIterator mask{getPieceBoard(state,state.current_bb,t)};
        uint16_t idx = 0;
        while (mask.nextBit(&idx)) {
            MoveIterator mi = generateMoveMask(genState, state, Vec2{static_cast<int16_t>(idx)}, t);
            moveCount += mi.moveCount();
            genState.moves.emplace_back(mi);
        }
    }
    genState.moveCount = moveCount;
}

void morphy::generateAllLegalMoves (MoveGenCache& genState, const Board& state) {
    uint64_t moveCount = 0;
    for (const PieceType& t : all_piece_types) {
        if (t == PieceType::NONE) continue;
        MaskIterator mask{getPieceBoard(state,state.current_bb,t)};
        uint16_t idx = 0;
        while (mask.nextBit(&idx)) {
            MoveIterator mi = generateMoveMask(genState, state, Vec2{static_cast<int16_t>(idx)}, t);
            MoveIterator tmp = mi;
            Move move;
            while (tmp.nextMove(&move)) {
                if (!validateMove(genState,state,move)) mi.clearMove(move.to);
            }
            moveCount += mi.moveCount();
            genState.moves.emplace_back(mi);
        }
    }
    genState.moveCount = moveCount;
}

static uint8_t isCastleMove (const Move& move) {
    if (move.type != PieceType::KING) return NO_CASTLE;
    else if (move.from == 3 && move.to == 1)  return CASTLE_KINGSIDE;
    else if (move.from == 3 && move.to == 5) return CASTLE_QUEENSIDE;
    else return NO_CASTLE;
}

bool morphy::validateMove (const MoveGenCache& genState, const Board& state, const Move& move) {
    uint64_t allPieces = genState.allPieces;
    if (!CHECK_BIT(state.current_bb,move.from)) return false;

    uint8_t castle = isCastleMove(move);
    if (castle) {
        if (!state.current_castle_flags) return false;
        if (castle == CASTLE_KINGSIDE) {
            if (allPieces & 6) return false; // path to rook isn't clear
            if (threatsToCells(std::move(genState),state,{{2,0},{1,0}}).size()) return false;
        }
        else if (castle == CASTLE_QUEENSIDE) {
            if (allPieces & 112) return false; // path to rook isn't clear
            if (threatsToCells(std::move(genState),state,{{2,0},{1,0},{4,0},{5,0},{6,0}}).size()) return false;
        }
    }

    // Handle double pawn move
    if (move.type == PieceType::PAWN && (move.to - move.from) == 16){
        if (move.from < 8 && move.from > 15) return false; // piece has already moved
        if ((BIT_MASK(move.from + 8)) & genState.allPieces) return false; // blocked by another piece
    }

    return true;
}

void morphy::applyMove (Board& state, const Move& move) {
    state.current_bb = MOVE_BIT(state.current_bb, move.from, move.to);
    for (const PieceType& t : all_piece_types) {
        if (t == PieceType::NONE) continue;
        if (t == move.type) continue;
        uint64_t * bb = getPieceBoard(state, t);
        *bb = CLEAR_BIT(*bb, move.to);
    }

    if (move.type == PieceType::PAWN && move.to >= 56) {
        state.promotion_needed = true;
        state.promotion_sq = move.to;
    }

    uint64_t * bb = getPieceBoard(state, move.type);
    *bb = MOVE_BIT(*bb, move.from, move.to);

    uint8_t castle = isCastleMove(move);
    if (castle == CASTLE_KINGSIDE) {
        uint64_t* cb = getPieceBoard(state,PieceType::ROOK);
        *cb = MOVE_BIT(*cb, 0, 2);
    }
    else if (castle == CASTLE_QUEENSIDE) {
        uint64_t* cb = getPieceBoard(state,PieceType::ROOK);
        *cb = MOVE_BIT(*cb, 7, 5);
    }
}

// Determines where a cell is being attacked from.
std::vector<Move> morphy::threatsToCells (const MoveGenCache& genState, const Board& board, const std::initializer_list<Vec2>& positions){
    std::vector<Move> res;
    uint64_t enemy = genState.enemyPieces;
    uint64_t own = board.current_bb;

    for (const auto& p : positions){
        uint16_t idx = 0;
        uint16_t posIdx = static_cast<uint16_t>(p.idx());

        for (const PieceType& t : all_piece_types) {
            if (t == PieceType::NONE) continue;
            if (t == PieceType::KING || t == PieceType::KNIGHT || t == PieceType::PAWN) continue;

            uint64_t mask = 0;
            if (t == PieceType::ROOK) mask = rook_mask(own,enemy,p);
            else if (t == PieceType::BISHOP) mask = bishop_mask(own,enemy,p);
            else if (t == PieceType::QUEEN) mask = bishop_mask(own,enemy,p) | rook_mask(own,enemy,p);

            uint64_t bb = getPieceBoard(board,enemy,t);
            MaskIterator p{mask & bb};
            while (p.nextBit(&idx)){
                res.emplace_back(t,idx,posIdx);
            }
        }

        MaskIterator pi{ pawn_mask(own,enemy,p) & getPieceBoard(board,enemy,PieceType::PAWN) };
        while (pi.nextBit(&idx)) { res.emplace_back(PieceType::PAWN, idx, posIdx); }

        MaskIterator ni{ knight_mask(own,enemy,p) & getPieceBoard(board,enemy,PieceType::KNIGHT) };
        while (ni.nextBit(&idx)) { res.emplace_back(PieceType::KNIGHT, idx, posIdx); }

        uint64_t km = king_mask(own,enemy,p)  & getPieceBoard(board,enemy, PieceType::KING);
        if (km) {
            res.emplace_back(PieceType::KING, km, posIdx);
        }
    }
    return res;
}

std::vector<Move> morphy::threatsToCell (const MoveGenCache& genState, const Board& board, const Vec2& pos) {
    return threatsToCells(genState,board,{pos});
}



template <class T>
static void testMask (const char * name, T target, T real) {
    bool passed = target == real;
    std::cout << name << ": " ;
    if (passed) std::cout << "PASSED!\n";
    else std::cout << "FAILED got: " << real << " expected: " << target << "\n";
}

void morphy::testMasks () {
    std::cout << "-------------------------\n";
    std::cout << "START TESTS\n";
    std::cout << "-------------------------\n";

    testMask<uint64_t>("rank 0", 0xff, rank_mask(0));
    testMask<uint64_t>("rank 7", 0xff00000000000000, rank_mask(7));
    testMask<uint64_t>("file 0", 0x101010101010101, file_mask(0));
    testMask<uint64_t>("file 7", 0x8080808080808080, file_mask(7));
    testMask<uint64_t>("file 7", 0x8080808080808080, file_mask(7));
    testMask<uint64_t>("rect mask 3,3 3,3", 0x1c1c1c0000, rect_mask({3,3},{3,3}));
    testMask<uint64_t>("rect mask 3,3 1,1", 0x8000000, rect_mask({3,3},{1,1}));
    testMask<uint64_t>("ray north", 0x808080808000000, rayUntilBlocked(0,0,{3,3},Direction::NORTH));
    testMask<uint64_t>("ray south", 0x8080808, rayUntilBlocked(0,0,{3,3},Direction::SOUTH));
    testMask<uint64_t>("ray east", 0xf000000, rayUntilBlocked(0,0,{3,3},Direction::EAST));
    testMask<uint64_t>("ray west", 0xf8000000, rayUntilBlocked(0,0,{3,3},Direction::WEST));
    testMask<uint64_t>("ray northeast", 0x1020408000000, rayUntilBlocked(0,0,{3,3},Direction::NORTHEAST));
    testMask<uint64_t>("ray northwest", 0x8040201008000000, rayUntilBlocked(0,0,{3,3},Direction::NORTHWEST));
    testMask<uint64_t>("ray southeast", 0x8040201, rayUntilBlocked(0,0,{3,3},Direction::SOUTHEAST));
    testMask<uint64_t>("ray southwest", 0x8102040, rayUntilBlocked(0,0,{3,3},Direction::SOUTHWEST));
    testMask<uint64_t>("bishop mask 3,3", 9241705379636978241, bishop_mask(0,0,{3,3}));
    testMask<uint64_t>("rook mask 3,3", 578721386714368008, rook_mask(0,0,{3,3}));
    testMask<uint64_t>("queen mask 3,3", 9820426766351346249, queen_mask(0,0,{3,3}));
    testMask<uint64_t>("queen mask 3,0", 578721933553179895, queen_mask(0,0,{3,0}));
    testMask<uint64_t>("king mask 0,0",770, king_mask(0,0,{0,0}));
    testMask<uint64_t>("knight 3,3", 0x142200221400, knight_mask(0,0,{3,3}));
    testMask<uint64_t>("knight 0,3", 0x20400040200, knight_mask(0,0,{0,3}));
    testMask<uint64_t>("knight 7,3", 0x402000204000, knight_mask(0,0,{7,3}));
    testMask<uint64_t>("pawn 0,1", 0x1030000, pawn_mask(0,0xffffffffffffffff,{0,1}));
    testMask<uint64_t>("pawn 7,1", 0x80c00000, pawn_mask(0,0xffffffffffffffff,{7,1}));

    Board standardBoard;
    initializeBoard(standardBoard);

    // Standard board with one black pawn on (7,6)
    Board oneBlackPawn = standardBoard;
    oneBlackPawn.pawns &= 0x8000000000ff00;

    Board castleKingSide = standardBoard;
    castleKingSide.knights = CLEAR_BIT(castleKingSide.knights,1);
    castleKingSide.bishops = CLEAR_BIT(castleKingSide.bishops,2);
    castleKingSide.current_bb &= ~static_cast<uint64_t>(6);

    Board castleQueenSide = standardBoard;
    castleQueenSide.queens  = CLEAR_BIT(castleQueenSide.queens,4);
    castleQueenSide.bishops = CLEAR_BIT(castleQueenSide.bishops,5);
    castleQueenSide.knights = CLEAR_BIT(castleQueenSide.knights,6);
    castleQueenSide.current_bb &= ~static_cast<uint64_t>(112);

    Board pawnBoard = standardBoard;
    pawnBoard.pawns = 0x103fe00;
    pawnBoard.current_bb = CLEAR_BIT(SET_BIT(pawnBoard.current_bb,16),8);

    testMask<uint64_t>("threats 1", 4, threatsToCell({oneBlackPawn}, oneBlackPawn, {3,6}).size());
    testMask<uint64_t>("threats 2", 1, threatsToCell({oneBlackPawn}, oneBlackPawn, {0,6}).size());
    testMask<uint64_t>("threats 3", 2, threatsToCell({oneBlackPawn}, oneBlackPawn, {6,5}).size());
    testMask<uint64_t>("threats 4", 1, threatsToCell({oneBlackPawn}, oneBlackPawn, {3,2}).size());

    testMask<bool>("validate kingside castle", false, validateMove({standardBoard},standardBoard,{PieceType::KING, 3, 1}));        // Can't castle with pieces in the way
    testMask<bool>("validate kingside castle", true, validateMove({castleKingSide},castleKingSide,{PieceType::KING, 3, 1}));       // Can castle
    testMask<bool>("validate queenside castle", false, validateMove({standardBoard},standardBoard,{PieceType::KING, 3, 5}));       // Can't castle with piece in the way
    testMask<bool>("validate queenside castle", true, validateMove({castleQueenSide},castleQueenSide,{PieceType::KING, 3, 5}));    // Can castle
    testMask<bool>("validate queenside castle", false, validateMove({castleQueenSide},castleQueenSide,{PieceType::KING, 30, 30})); // Can't castle after having moved
    testMask<bool>("validate pawn double", false, validateMove({pawnBoard}, pawnBoard, {PieceType::PAWN, 16, 32}));                // Can't double after it's moved
    testMask<bool>("validate pawn double", false, validateMove({pawnBoard}, pawnBoard, {PieceType::PAWN, 9, 25}));                 // Can't double with piece in the way
    testMask<bool>("validate pawn double", true, validateMove({pawnBoard}, pawnBoard, {PieceType::PAWN, 10, 26}));                 // Can double

    std::cout << "-------------------------\n";
    std::cout << "END TESTS\n";
    std::cout << "-------------------------\n";
}

static void printBoard_internal (const Board& board, uint64_t mask, const char * tiles, char * output){
    for (const auto& t : all_piece_types){
        uint16_t tile = tiles[static_cast<uint8_t>(t)];
        MaskIterator bb{getPieceBoard(board, mask, t)};
        uint16_t idx = 0;
        while (bb.nextBit(&idx)) {
            output[63-idx] = tile;
        }
    }
}

void morphy::printBoard (const Board& gs, std::ostream& out) {
    char output[64] = {};
    for (int i = 0; i < 64; i++) { output[i] = '.'; }

    std::array<const char *,2> pieces{{"PRBNQK","prbnqk"}};
    //std::array<char,2> pieces{{"♙♖♗♘♕♔","♟♜♝♞♛♚"}};
    // Always print from white's perspective
    bool was_white = gs.is_white;
    if (!was_white) flipBoard(const_cast<Board&>(gs));

    printBoard_internal(gs, gs.current_bb, pieces[0], output);
    printBoard_internal(gs, enemy_pieces(gs), pieces[1], output);

    if (!was_white) flipBoard(const_cast<Board&>(gs));

    for (int i = 0; i < 64; i++) {
        if (i%8==0 && i != 0) std::cout << "\n";
        out << output[i];
    }
    out << "\n";
}

std::string morphy::boardToFEN (const Board& board) {

}

