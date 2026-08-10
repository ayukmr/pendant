#include <array>
#include <vector>
#include <utility>

// == plexing ==

std::array<std::pair<int, int>, 15> pins = {{
    {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 16},
    {0, 17}, {0, 18}, {0, 19}, {0, 20}, {0, 21},
    {0, 22}, {0, 23}, {0, 27}, {0, 28}, {1, 10}
}};

std::array<std::vector<std::pair<int, int>>, 14> pairs = {{
             {{ 4,  0}, { 6,  4}, { 8,  6}, {10,  8}},
          {{ 4,  1}, { 6,  0}, { 8,  4}, {10,  6}, {11,  8}},
   {{ 4,  2}, { 6,  1}, { 8,  0}, {10,  4}, {11,  6}, {12,  8}},
{{ 4,  3}, { 6,  2}, { 8,  1}, {10,  0}, {11,  4}, {12,  6}, {13,  8}},
{{ 6,  3}, { 8,  2}, {10,  1}, {11,  0}, {12,  4}, {13,  6}, {14,  8}},
{{ 6,  5}, { 8,  3}, {10,  2}, {11,  1}, {12,  0}, {13,  4}, {14,  6}},
{{ 8,  5}, {10,  3}, {11,  2}, {12,  1}, {13,  0}, {14,  4}, { 9,  6}},
{{ 8,  7}, {10,  5}, {11,  3}, {12,  2}, {13,  1}, {14,  0}, { 9,  4}},
{{10,  7}, {11,  5}, {12,  3}, {13,  2}, {14,  1}, { 9,  0}, { 7,  4}},
{{10,  9}, {11,  7}, {12,  5}, {13,  3}, {14,  2}, { 9,  1}, { 7,  0}},
{{11,  9}, {12,  7}, {13,  5}, {14,  3}, { 9,  2}, { 7,  1}, { 5,  0}},
   {{12,  9}, {13,  7}, {14,  5}, { 9,  3}, { 7,  2}, { 5,  1}},
          {{13,  9}, {14,  7}, { 9,  5}, { 7,  3}, { 5,  2}},
             {{14,  9}, { 9,  7}, { 7,  5}, { 5,  3}}
}};

std::array<std::vector<bool>, 14> leds;

const int CPL_Y = 0;
const int CPL_PAIR  = 3;
const int CPL_WORKS = 1;

const uint16_t SLOT_US = 50;

void on(int lo, int hi) {
    int gl = pins[lo].first, bl = pins[lo].second;
    int gh = pins[hi].first, bh = pins[hi].second;

    PORT->Group[gl].OUTCLR.reg = 1ul << bl;
    PORT->Group[gh].OUTSET.reg = 1ul << bh;

    if (gl == gh) {
        PORT->Group[gl].DIRSET.reg = (1ul << bl) | (1ul << bh);
    } else {
        PORT->Group[gl].DIRSET.reg = 1ul << bl;
        PORT->Group[gh].DIRSET.reg = 1ul << bh;
    }
}

void off(int lo, int hi) {
    int gl = pins[lo].first, bl = pins[lo].second;
    int gh = pins[hi].first, bh = pins[hi].second;

    if (gl == gh) {
        PORT->Group[gl].DIRCLR.reg = (1ul << bl) | (1ul << bh);
    } else {
        PORT->Group[gl].DIRCLR.reg = 1ul << bl;
        PORT->Group[gh].DIRCLR.reg = 1ul << bh;
    }
}

int width(int y) {
    return pairs[y].size() * 2;
}

void clear() {
    for (int y = 0; y < 14; y++) {
        for (int x = 0; x < width(y); x++) {
            leds[y][x] = false;
        }
    }
}

bool wanted(int x, int y, int dir) {
    if (y == CPL_Y && x == CPL_PAIR) {
        if (dir != CPL_WORKS) return false;
        return leds[y][x * 2] || leds[y][x * 2 + 1];
    } else {
        return leds[y][x * 2 + dir];
    }
}

void plex() {
    for (int y = 0; y < 14; y++) {
        for (size_t x = 0; x < pairs[y].size(); x++) {
            auto [lo, hi] = pairs[y][x];

            for (int dir = 0; dir < 2; dir++) {
                int a = dir == 0 ? hi : lo;
                int b = dir == 0 ? lo : hi;

                if (wanted((int) x, y, dir)) on(a, b);
                delayMicroseconds(SLOT_US);
                off(a, b);
            }
        }
    }
}

// == images ==

const uint16_t IMG_HEART[14] = {
    0b00000000000000,
    0b00000000000000,
    0b00011000011000,
    0b00111100111100,
    0b01111111111110,
    0b01111111111110,
    0b01111111111110,
    0b00111111111100,
    0b00111111111100,
    0b00011111111000,
    0b00001111110000,
    0b00000111100000,
    0b00000011000000,
    0b00000000000000
};
const uint16_t IMG_SMILE[14] = {
    0b00000000000000,
    0b00000111100000,
    0b00011000011000,
    0b00100000000100,
    0b00101100110100,
    0b01001100110010,
    0b01000000000010,
    0b01000000000010,
    0b01001000011010,
    0b00101111110100,
    0b00100111100100,
    0b00011000011000,
    0b00000111100000,
    0b00000000000000
};
const uint16_t IMG_RING[14] = {
    0b00000000000000,
    0b00000011000000,
    0b00001100110000,
    0b00010000001000,
    0b00100000000100,
    0b00100000000100,
    0b01000000000010,
    0b01000000000010,
    0b00100000000100,
    0b00100000000100,
    0b00010000001000,
    0b00001100110000,
    0b00000011000000,
    0b00000000000000
};
const uint16_t IMG_F[14] = {
    0b00000000000000,
    0b00111111111000,
    0b00111111111000,
    0b00110000000000,
    0b00110000000000,
    0b00111111100000,
    0b00111111100000,
    0b00110000000000,
    0b00110000000000,
    0b00110000000000,
    0b00110000000000,
    0b00110000000000,
    0b00000000000000,
    0b00000000000000
};
const uint16_t IMG_FILL[14] = {
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111,
    0b11111111111111
};

const uint16_t* IMGS[] = {IMG_HEART, IMG_RING, IMG_SMILE, IMG_F, IMG_FILL};
const int NIMG = 5;

void draw(const uint16_t* rows) {
    clear();

    for (int cy = 0; cy < 14; cy++) {
        int w = width(cy);
        int ofs = (14 - w) / 2;

        for (int cx = 0; cx < 14; cx++) {
            if (rows[cy] & (1u << (13 - cx))) {
                int lx = cx - ofs;
                if (lx < 0 || lx >= w) continue;
                leds[cy][lx] = true;
            }
        }
    }
}

// == entry ==

void setup() {
    Serial.begin(115200);

    for (int y = 0; y < 14; y++) leds[y].assign(width(y), false);

    for (auto& p : pins) {
        PORT->Group[p.first].PINCFG[p.second].reg = 0;
        PORT->Group[p.first].PINCFG[p.second].bit.DRVSTR = 1;
        PORT->Group[p.first].PINCFG[p.second].bit.PMUXEN = 0;
        PORT->Group[p.first].DIRCLR.reg = 1ul << p.second;
    }

    draw(IMG_HEART);
}

int cur = 0;
uint32_t last = 0;

void loop() {
    plex();

    if (millis() - last > 500) {
        last = millis();
        draw(IMGS[cur]);
        cur = (cur + 1) % NIMG;
    }
}
