// sprites.h — pixel art for the alarm clock pet ("Mochi" the cat)
//
// Each sprite is drawn from text: '#' is a lit pixel, anything else is
// see-through. Edit the pictures right here and re-upload to change the art.
// Sprites are drawn at 2x (each '#' becomes a 2x2 block) for chunky pixels.

#pragma once
#include <Adafruit_GFX.h>

// ---------- Mochi sitting (16x16). Face is drawn separately on top. ----------
const char* const CAT_SIT_A[] = {
  "................",
  ".#.......#......",
  ".##.....##......",
  ".#.#...#.#......",
  ".#..###..#......",
  "#.........#.....",
  "#.........#.....",
  "#.........#.....",
  ".#.......#......",
  "..#######...##..",
  ".#.......#.#..#.",
  ".#.......#.#....",
  "#.........##....",
  "#.........#.....",
  "#...#.#...#.....",
  ".###.#.###......",
};

// Same cat, tail swished the other way.
const char* const CAT_SIT_B[] = {
  "................",
  ".#.......#......",
  ".##.....##......",
  ".#.#...#.#......",
  ".#..###..#......",
  "#.........#.....",
  "#.........#.....",
  "#.........#.....",
  ".#.......#......",
  "..#######.......",
  ".#.......#......",
  ".#.......#......",
  "#.........#.....",
  "#.........#..##.",
  "#...#.#...#.#..#",
  ".###.#.###.##...",
};

// Arms up! Used while the alarm is ringing.
const char* const CAT_PANIC[] = {
  "................",
  ".#.......#......",
  ".##.....##......",
  ".#.#...#.#......",
  ".#..###..#......",
  "#.........#.....",
  "#.........#.....",
  "#.........#.....",
  ".#.......#......",
  "#.#######.#.....",
  "##.......##..##.",
  ".#.......#..#..#",
  ".#.......#..#...",
  ".#.......#...#..",
  ".#..#.#..#####..",
  "..###.###.......",
};

// Curled-up sleeping loaf (eyes already closed).
const char* const CAT_SLEEP[] = {
  "................",
  "................",
  "................",
  "................",
  "................",
  "................",
  "................",
  ".#.......#......",
  ".##.....##......",
  ".#.#...#.#......",
  ".#..###..####...",
  "#.........#..##.",
  "#.##...##.#....#",
  "#.........#....#",
  ".#..#.#..#.....#",
  "..#######.######",
};

// ---------- Faces (7x4), drawn on the cat at column 2, row 5 ----------
const char* const FACE_OPEN[] = {   // normal
  ".#...#.",
  ".#...#.",
  "...#...",
  "..#.#..",
};
const char* const FACE_BLINK[] = {  // eyes closed
  ".......",
  "##...##",
  "...#...",
  "..#.#..",
};
const char* const FACE_HAPPY[] = {  // ^ ^
  ".#...#.",
  "#.#.#.#",
  ".......",
  "..###..",
};
const char* const FACE_SHOCK[] = {  // O O + open mouth
  "##...##",
  "##...##",
  "...#...",
  "..#.#..",
};
const char* const FACE_CONFUSED[] = { // no WiFi :(
  ".#...#.",
  ".#...#.",
  ".......",
  "..###..",
};

// ---------- Small icons (drawn at 1x) ----------
const char* const ICON_BELL[] = {
  "...#....",
  "..###...",
  ".#####..",
  ".#####..",
  ".#####..",
  "#######.",
  "........",
  "..##....",
};
const char* const ICON_BELL_OFF[] = {
  "...#...#",
  "..###.#.",
  ".####...",
  ".###.#..",
  ".##.##..",
  "##.####.",
  "#.......",
  "..##....",
};
const char* const ICON_ZZ[] = {
  "####....",
  "..#.....",
  ".#......",
  "####....",
  "....###.",
  ".....#..",
  "....#...",
  "....###.",
};
const char* const ICON_HEART[] = {
  ".##.##.",
  "#######",
  "#######",
  ".#####.",
  "..###..",
  "...#...",
};

#define SPRITE_ROWS(s) (sizeof(s) / sizeof((s)[0]))

// Draws a text sprite with its top-left corner at (x, y).
void drawSprite(Adafruit_GFX& gfx, const char* const* rows, int rowCount,
                int x, int y, int scale = 1, uint16_t color = 1) {
  for (int r = 0; r < rowCount; r++) {
    for (int c = 0; rows[r][c] != '\0'; c++) {
      if (rows[r][c] == '#') gfx.fillRect(x + c * scale, y + r * scale, scale, scale, color);
    }
  }
}
#define DRAW_SPRITE(gfx, s, x, y, scale) drawSprite(gfx, s, SPRITE_ROWS(s), x, y, scale)