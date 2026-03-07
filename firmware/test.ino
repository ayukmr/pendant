#include <utility>

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
  pinOutput({0, 10});
  pinOutput({0, 16});

  pinLow({0, 10});
  pinHigh({0, 16});
}

void loop() {}
