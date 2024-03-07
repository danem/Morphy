#pragma once

#include <string>
#include "board.h"

namespace morphy {
namespace fen {

bool fen_to_board (morphy::Board& board, const std::string& fen);

}} // end namespaces
