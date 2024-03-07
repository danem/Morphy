#include <morphy/fen.h>
#include <sstream>
#include <string>
#include <map>

namespace morphy {
namespace fen {

static const std::map<char,PieceType> _FEN2PIECE = {
    {'p', PieceType::PAWN},
    {'q', PieceType::QUEEN},
    {'r', PieceType::ROOK},
    {'b', PieceType::BISHOP},
    {'k', PieceType::KING},
    {'n', PieceType::KNIGHT},
    {'-', PieceType::PAWN}
};

bool fen_to_board (Board &board, const std::string& fen) {
    std::stringstream ss(fen);
    std::string rank_str;

    uint64_t white_bb = 0;
    uint8_t rank = 7;
    uint8_t file = 0;

    // Parse the piece placement ranks
    while (std::getline(ss, rank_str, '/')) {
        for (char c : rank_str) {
            if (c == ' ') break; // Skip spaces
            if (isdigit(c)) {
                file += (c - '0');
                continue;
            }

            // printf("%c\n", c);

            PieceType pt = _FEN2PIECE.at(tolower(c));
            bool is_white = true;
            uint8_t curank = rank;
            if (islower(c)) {
                is_white = false;
                curank = 7 - curank; // TODO: Rework board API so this isnt necessary
            }
            morphy::setBoardColor(board, is_white);
            morphy::setPiece(board, pt, {file, curank});
            file += 1;
        }
        rank -= 1;
        file = 0;
    }
    // TODO: Handle castling
    morphy::setBoardColor(board, true);
    return true;
}

}} // end namespace