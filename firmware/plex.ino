#include <array>
#include <vector>
#include <utility>
#include <algorithm>

std::array<std::pair<int, int>, 15> pins = {{
    {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 16},
    {0, 17}, {0, 18}, {0, 19}, {0, 20}, {0, 21},
    {0, 22}, {0, 23}, {0, 27}, {0, 28}, {1, 10}
}};

std::array<std::vector<std::pair<int, int>>, 14> pairs = {
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
};


std::array<std::vector<bool>, 14> leds = {
                     {false, false, false, false, false, false, false, false},
              {false, false, false, false, false, false, false, false, false, false},
       {false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
{false, false, false, false, false, false, false, false, false, false, false, false, false, false},
       {false, false, false, false, false, false, false, false, false, false, false, false},
              {false, false, false, false, false, false, false, false, false, false},
                     {false, false, false, false, false, false, false, false}
};

void pinInput(const std::pair<int, int>& p) {
    PORT->Group[p.first].DIRCLR.reg = (1ul << p.second);
}

void pinOutput(const std::pair<int, int>& p) {
    PORT->Group[p.first].DIRSET.reg = (1ul << p.second);
}

void pinHigh(const std::pair<int, int>& p) {
    PORT->Group[p.first].OUTSET.reg = (1ul << p.second);
}

void pinLow(const std::pair<int, int>& p) {
    PORT->Group[p.first].OUTCLR.reg = (1ul << p.second);
}

void setup() {
    for (auto& pin : pins) {
        pinInput(pin);
    }
}

void loop() {
    plex();
}

void plex() {
    for (int y = 0; y < 14; y++) {
        auto& pr = pairs[y];
        auto& lr = leds[y];

        for (int x = 0; x < pr.size(); x++) {
            auto [lo, hi] = pr[x];

            for (int i = 0; i < 2; i++) {
                bool on = lr[x * 2 + i];

                if (on) {
                    pinOutput(pins[lo]);
                    pinOutput(pins[hi]);

                    pinLow(pins[lo]);
                    pinHigh(pins[hi]);
                }

                delayMicroseconds(500);

                pinInput(pins[lo]);
                pinInput(pins[hi]);

                std::swap(lo, hi);
            }
        }
    }
}
