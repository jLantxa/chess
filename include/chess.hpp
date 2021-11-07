/*
 * Copyright (C) 2021  Javier Lancha Vázquez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _CHESS_INCLUDE_CHESS_HPP_
#define _CHESS_INCLUDE_CHESS_HPP_

#include <cstdint>

#include <array>
#include <string>

namespace chess {

enum class Colour {
    WHITE,
    BLACK
};

enum class PieceType {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

constexpr uint8_t PAWN_VALUE   = 1U;
constexpr uint8_t KNIGHT_VALUE = 3U;
constexpr uint8_t BISHOP_VALUE = 3U;
constexpr uint8_t ROOK_VALUE   = 5U;
constexpr uint8_t QUEEN_VALUE  = 9U;
constexpr uint8_t KING_VALUE = UINT8_MAX;

struct Square {
    uint8_t file;
    uint8_t rank;

    bool operator==(const Square& other) const;
};

struct Move {
    Square src;
    Square dst;

    bool operator==(const Move& other) const;
};

constexpr std::array<char, 8> FILES {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

uint8_t FileToNumber(char file);
char NumberToFile(uint8_t number);

[[nodiscard]] std::string SquareToString(const Square& square);
[[nodiscard]] Square StringToSquare(const std::string& str);
[[nodiscard]] std::string MoveToUCI(const Move& move);
[[nodiscard]] Move UCIToMove(const std::string& uci);

/**
 * @brief Toggle colour
 */
inline void ToggleColour(Colour* colour);

}  // namespace chess

#endif  // _CHESS_INCLUDE_CHESS_HPP_
