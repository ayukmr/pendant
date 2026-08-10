#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>

struct V2; // arduino header weirdness

const bool LOG = false;

#define AI __attribute__((always_inline)) inline

// == fixed point ==

typedef int32_t fx;

#define FX_ONE 65536
#define FX(f) ((fx)((f) * 65536.0f))

#define FX_INT(a)  ((a) >> 16)
#define FX_FRAC(a) ((a) & 0xFFFF)

AI fx fmul(fx a, fx b) { return (fx)(((int64_t)a * b) >> 16); }
AI fx fdiv(fx a, fx b) { return (fx)(((int64_t)a << 16) / b); }

fx fsqrt(fx a) {
    if (a <= 0) return 0;

    uint64_t v = (uint64_t)a << 16, r = 0, b = 1ull << 46;

    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }

    return (fx)r;
}

// == flip ==

const int SIZE = 16;
const int NP = 64;
const int MARGIN = 1;

#define CUT ((SIZE * 3.0f + 7.0f) / 14.0f)

#define DT FX(0.015f)
#define O  FX(1.5f)

struct V2 {
    fx x = 0, y = 0;

    AI V2 operator+(V2 o) const { return {x + o.x, y + o.y}; }
    AI V2 operator-(V2 o) const { return {x - o.x, y - o.y}; }
    AI V2 operator*(fx s) const { return {fmul(x, s),   fmul(y, s)};   }
    AI V2 operator*(V2 o) const { return {fmul(x, o.x), fmul(y, o.y)}; }
    AI V2 operator/(fx s) const { return {fdiv(x, s),   fdiv(y, s)};   }
    AI V2& operator+=(V2 o) { x += o.x; y += o.y; return *this; }
    AI V2& operator-=(V2 o) { x -= o.x; y -= o.y; return *this; }
};

struct Cell {
    uint8_t cx, cy;
    uint8_t s;
    V2 force;
    V2 weights;
    bool fluid = false;

    Cell(uint8_t cx, uint8_t cy) : cx(cx), cy(cy) {
        int dx = cx * 2 - (SIZE - 1), dy = cy * 2 - (SIZE - 1);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        s = (dx + dy > 22) ? 0 : 1;
    }

    void reset() {
        force = {0, 0};
        weights = {0, 0};
        fluid = false;
    }

    void norm_force() {
        if (s == 0) {
            force = {0, 0};
            return;
        }

        if (weights.x > 64) force.x = fdiv(force.x, weights.x);
        if (weights.y > 64) force.y = fdiv(force.y, weights.y);
    }

    void solve(std::vector<Cell>& cells) {
        if (s == 0 || !fluid) return;

        static const fx INV[5] = {0, FX_ONE, FX_ONE/2, FX_ONE/3, FX_ONE/4};

        int i = cx + cy * SIZE;

        Cell& c2 = cells[i + 1];
        Cell& c3 = cells[i + SIZE];
        Cell& c4 = cells[i - 1];
        Cell& c5 = cells[i - SIZE];

        int ss = c2.s + c3.s + c4.s + c5.s;
        if (ss == 0) return;
        fx inv = INV[ss];

        fx d = fmul(O, c2.force.x - force.x + c3.force.y - force.y);
        fx dd = fmul(d, inv);

        if (c4.s) force.x += dd;
        if (c2.s) c2.force.x -= dd;
        if (c5.s) force.y += dd;
        if (c3.s) c3.force.y -= dd;
    }
};

struct Particle {
    V2 pos;
    V2 vel;
    V2 grid_vel;
    size_t idx;

    fx wu[4], wv[4];
    int iu, iv;

    Particle(V2 pos, size_t idx) : pos(pos), idx(idx) {}

    void update(V2 g) {
        vel += g * DT;
        pos += vel * DT;
    }

    void clamp() {
        fx k = FX(0.70710678f);
        fx c = FX((SIZE - 1) / 2.0f);
        fx lim = FX((SIZE - 1) - CUT - MARGIN * 2 * 0.70710678f);
        fx bnd = FX((SIZE - 1) / 2.0f - MARGIN);

        fx px = pos.x - FX(0.5f), py = pos.y - FX(0.5f);
        fx dx = px - c, dy = py - c;

        for (int pass = 0; pass < 2; pass++) {
            if (dx > bnd) {
                dx = bnd;
                if (vel.x > 0) vel.x = -fmul(vel.x, FX(0.2f));
            }
            if (dx < -bnd) {
                dx = -bnd;
                if (vel.x < 0) vel.x = -fmul(vel.x, FX(0.2f));
            }
            if (dy > bnd) {
                dy = bnd;
                if (vel.y > 0) vel.y = -fmul(vel.y, FX(0.2f));
            }
            if (dy < -bnd) {
                dy = -bnd;
                if (vel.y < 0) vel.y = -fmul(vel.y, FX(0.2f));
            }

            fx ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
            if (ax + ay > lim) {
                fx over = (ax + ay - lim) >> 1;

                fx sx = dx < 0 ? -FX_ONE : FX_ONE, sy = dy < 0 ? -FX_ONE : FX_ONE;
                dx -= fmul(sx, over);
                dy -= fmul(sy, over);

                fx vn = fmul(fmul(vel.x, sx) + fmul(vel.y, sy), k);
                if (vn > 0) {
                    fx t = fmul(fmul(vn, k), FX(1.2f));
                    vel.x -= fmul(sx, t);
                    vel.y -= fmul(sy, t);
                }
            }
        }

        pos.x = dx + c + FX(0.5f);
        pos.y = dy + c + FX(0.5f);
    }

    V2 sample_grid(std::vector<Cell>& cells) {
        fx u = component_from_grid(cells, true);
        fx v = component_from_grid(cells, false);
        return {u, v};
    }

    void to_grid(std::vector<Cell>& cells) {
        component_to_grid(cells, true);
        component_to_grid(cells, false);
    }

    void from_grid(std::vector<Cell>& cells) {
        V2 pic = sample_grid(cells);
        V2 flip = vel + (pic - grid_vel);
        vel = flip * FX(0.98f) + pic * FX(0.02f);
    }

    void cache_weights() {
        fx px = pos.x, py = pos.y - FX(0.5f);

        int cx = FX_INT(px), cy = FX_INT(py);
        iu = cx + cy * SIZE;

        fx dx = FX_FRAC(px), dy = FX_FRAC(py);
        fx ix = FX_ONE - dx, iy = FX_ONE - dy;

        wu[0] = fmul(ix, iy);
        wu[1] = fmul(dx, iy);
        wu[2] = fmul(ix, dy);
        wu[3] = fmul(dx, dy);

        px = pos.x - FX(0.5f); py = pos.y;

        cx = FX_INT(px); cy = FX_INT(py);
        iv = cx + cy * SIZE;

        dx = FX_FRAC(px); dy = FX_FRAC(py);
        ix = FX_ONE - dx; iy = FX_ONE - dy;

        wv[0] = fmul(ix, iy);
        wv[1] = fmul(dx, iy);
        wv[2] = fmul(ix, dy);
        wv[3] = fmul(dx, dy);
    }

    void component_to_grid(std::vector<Cell>& cells, bool u) {
        fx val = u ? vel.x : vel.y;

        fx* w = u ? wu : wv;
        int i = u ? iu : iv;

        int o[4] = {i, i + 1, i + SIZE, i + SIZE + 1};

        for (int k = 0; k < 4; k++) {
            Cell& c = cells[o[k]];

            if (u) {
              c.force.x += fmul(w[k], val);
              c.weights.x += w[k];
            } else {
              c.force.y += fmul(w[k], val);
              c.weights.y += w[k];
            }
        }
    }

    fx component_from_grid(std::vector<Cell>& cells, bool u) {
        fx* w = u ? wu : wv;
        int i = u ? iu : iv;

        if (u) {
          return fmul(w[0], cells[i].force.x)
            + fmul(w[1], cells[i + 1].force.x)
            + fmul(w[2], cells[i + SIZE].force.x)
            + fmul(w[3], cells[i + SIZE + 1].force.x);
        } else   {
          return fmul(w[0], cells[i].force.y)
            + fmul(w[1], cells[i + 1].force.y)
            + fmul(w[2], cells[i + SIZE].force.y)
            + fmul(w[3], cells[i + SIZE + 1].force.y);
        }
    }
};

uint32_t rng_state = 12345u;
inline fx frand() {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (fx)((rng_state >> 16) & 0xFFFF);
}

struct Grid {
    std::vector<Cell> cells;
    std::vector<Particle> particles;

    Grid() {
        cells.reserve(SIZE * SIZE);
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {
                cells.emplace_back(x, y);
            }
        }

        particles.reserve(NP);
        for (int i = 0; i < NP; i++) {
            fx x = FX(6.0f) + fmul(frand(), FX(SIZE - 12.0f));
            fx y = FX(6.0f) + fmul(frand(), FX(SIZE - 12.0f));
            particles.emplace_back(V2{x, y}, i);
        }
    }

    void update(V2 g) {
        for (Particle& p : particles) p.update(g);

        push_apart();

        for (Particle& p : particles) {
            p.clamp();
            p.cache_weights();
        }

        for (Cell& c : cells) c.reset();

        for (Particle& p : particles) {
            fx px = p.pos.x - FX(0.5f), py = p.pos.y - FX(0.5f);

            int cx = FX_INT(px), cy = FX_INT(py);
            int i = cx + cy * SIZE;

            fx dx = FX_FRAC(px), dy = FX_FRAC(py);
            fx ix = FX_ONE - dx, iy = FX_ONE - dy;

            fx TH = FX(0.15f);

            if (fmul(ix, iy) > TH) cells[i].fluid = true;
            if (fmul(dx, iy) > TH) cells[i + 1].fluid = true;
            if (fmul(ix, dy) > TH) cells[i + SIZE].fluid = true;
            if (fmul(dx, dy) > TH) cells[i + SIZE + 1].fluid = true;

            p.to_grid(cells);
        }

        for (Cell& c : cells) c.norm_force();

        for (Particle& p : particles) p.grid_vel = p.sample_grid(cells);

        for (int i = 0; i < 6; i++) {
            for (Cell& c : cells) c.solve(cells);
        }

        for (Particle& p : particles) p.from_grid(cells);
    }

    void push_apart() {
        static uint8_t bin_count[SIZE * SIZE];
        static Particle* bin_data[SIZE * SIZE][8];

        fx r = FX(1.0f);
        fx r2 = fmul(r, r);

        for (int i = 0; i < SIZE * SIZE; i++) bin_count[i] = 0;

        for (Particle& p : particles) {
            int cx = FX_INT(p.pos.x), cy = FX_INT(p.pos.y);
            if (cx < 0 || cx >= SIZE || cy < 0 || cy >= SIZE) continue;

            int b = cx + cy * SIZE;
            if (bin_count[b] < 8) {
                bin_data[b][bin_count[b]++] = &p;
            }
        }

        for (Particle& p : particles) {
            int cx = FX_INT(p.pos.x), cy = FX_INT(p.pos.y);

            for (int gy = cy - 1; gy < cy + 2; gy++) {
                for (int gx = cx - 1; gx < cx + 2; gx++) {
                    if (gx < 0 || gx >= SIZE || gy < 0 || gy >= SIZE) continue;

                    int b = gx + gy * SIZE;
                    for (int k = 0; k < bin_count[b]; k++) {
                        Particle& q = *bin_data[b][k];
                        if (q.idx <= p.idx) continue;

                        V2 d = q.pos - p.pos;
                        fx d2 = fmul(d.x, d.x) + fmul(d.y, d.y);
                        if (d2 > r2 || d2 == 0) continue;

                        fx dl = fsqrt(d2);
                        if (dl < 16) continue;

                        fx scale = fdiv((r - dl) >> 1, dl);
                        V2 push = {fmul(d.x, scale), fmul(d.y, scale)};

                        p.pos -= push;
                        q.pos += push;
                    }
                }
            }
        }
    }
};

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

const uint16_t SLOT_US = 50;

AI void on(int lo, int hi) {
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

AI void off(int lo, int hi) {
    int gl = pins[lo].first, bl = pins[lo].second;
    int gh = pins[hi].first, bh = pins[hi].second;

    if (gl == gh) {
        PORT->Group[gl].DIRCLR.reg = (1ul << bl) | (1ul << bh);
    } else {
        PORT->Group[gl].DIRCLR.reg = 1ul << bl;
        PORT->Group[gh].DIRCLR.reg = 1ul << bh;
    }
}

Grid grid;

const int CPL_Y = 0;
const int CPL_PAIR  = 3;
const int CPL_WORKS = 1;

bool wanted(int x, int y, int dir) {
    int rw = pairs[y].size() * 2;
    int rox = (14 - rw) / 2;
    int gy = y + 1;

    if (x == CPL_PAIR && y == CPL_Y) {
        if (dir != CPL_WORKS) return false;
        int gx = x * 2 + rox + 1;
        return grid.cells[gx + gy * SIZE].fluid || grid.cells[gx + 1 + gy * SIZE].fluid;
    } else {
      int gx = x * 2 + dir + rox + 1;
      return grid.cells[gx + gy * SIZE].fluid;
  }
}

// == timer ==

struct Slot { uint8_t a, b; };
Slot sched[172];
volatile uint8_t fb[22];
uint16_t nslots = 0;
uint16_t cur = 0;

void build_sched() {
    nslots = 0;

    for (int y = 0; y < 14; y++) {
        for (size_t x = 0; x < pairs[y].size(); x++) {
            auto [lo, hi] = pairs[y][x];

            for (int dir = 0; dir < 2; dir++) {
                int a = dir == 0 ? hi : lo;
                int b = dir == 0 ? lo : hi;

                sched[nslots++] = {(uint8_t)a, (uint8_t)b};
            }
        }
    }
}

void publish() {
    uint16_t i = 0;

    for (int y = 0; y < 14; y++) {
        for (size_t x = 0; x < pairs[y].size(); x++) {
            for (int dir = 0; dir < 2; dir++, i++) {
                if (wanted(x, y, dir)) {
                  fb[i >> 3] |= (1u << (i & 7));
                } else {
                  fb[i >> 3] &= ~(1u << (i & 7));
                }
            }
        }
    }
}

void TC4_Handler() {
    if (!TC4->COUNT16.INTFLAG.bit.MC0) return;
    TC4->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

    off(sched[cur].a, sched[cur].b);

    cur++;
    if (cur >= nslots) cur = 0;

    if (fb[cur >> 3] & (1u << (cur & 7))) {
      on(sched[cur].a, sched[cur].b);
    }
}

void timer_init(uint16_t us) {
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TC4_TC5;
    while (GCLK->STATUS.bit.SYNCBUSY);

    TC4->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    while (TC4->COUNT16.CTRLA.bit.SWRST);

    TC4->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ | TC_CTRLA_PRESCALER_DIV8;
    TC4->COUNT16.CC[0].reg = 6 * us - 1;        // 48MHz / 8 = 6 ticks per us
    while (TC4->COUNT16.STATUS.bit.SYNCBUSY);

    TC4->COUNT16.INTENSET.bit.MC0 = 1;
    NVIC_SetPriority(TC4_IRQn, 1);
    NVIC_EnableIRQ(TC4_IRQn);
    TC4->COUNT16.CTRLA.bit.ENABLE = 1;
    while (TC4->COUNT16.STATUS.bit.SYNCBUSY);
}

// == accel ==

#define PA PORT->Group[0]

const uint8_t SDA_BIT = 8;
const uint8_t SCL_BIT = 9;
const uint32_t SDA_MASK = 1ul << SDA_BIT;
const uint32_t SCL_MASK = 1ul << SCL_BIT;

AI void tick() { delayMicroseconds(3); }
AI bool sda_read() { return (PA.IN.reg & SDA_MASK) != 0; }

AI void sda_hi() { PA.DIRCLR.reg = SDA_MASK; }
AI void sda_lo() { PA.OUTCLR.reg = SDA_MASK; PA.DIRSET.reg = SDA_MASK; }
AI void scl_hi() { PA.DIRCLR.reg = SCL_MASK; }
AI void scl_lo() { PA.OUTCLR.reg = SCL_MASK; PA.DIRSET.reg = SCL_MASK; }

void i2c_start() { sda_hi(); scl_hi(); tick(); sda_lo(); tick(); scl_lo(); tick(); }
void i2c_stop()  { sda_lo(); tick(); scl_hi(); tick(); sda_hi(); tick(); }

bool i2c_write(uint8_t v) {
    for (uint8_t i = 0; i < 8; i++) {
        scl_lo();
        if (v & 0x80) sda_hi(); else sda_lo();
        v <<= 1; tick(); scl_hi(); tick();
    }
    scl_lo(); sda_hi(); tick(); scl_hi(); tick();
    bool ack = !sda_read();
    scl_lo(); tick();
    return ack;
}

uint8_t i2c_read(bool last) {
    uint8_t v = 0;
    sda_hi();
    for (uint8_t i = 0; i < 8; i++) {
        scl_lo(); tick(); scl_hi(); tick();
        v = (v << 1) | (sda_read() ? 1 : 0);
    }
    scl_lo();
    if (last) sda_hi(); else sda_lo();
    tick(); scl_hi(); tick(); scl_lo(); tick(); sda_hi();
    return v;
}

const uint8_t ACC_ADDR = 0x18;

bool accel_init() {
    PA.PINCFG[SDA_BIT].reg = PORT_PINCFG_INEN;
    PA.PINCFG[SCL_BIT].reg = PORT_PINCFG_INEN;
    sda_hi(); scl_hi(); tick();
    delay(10);

    uint8_t id = 0;
    i2c_start(); i2c_write(ACC_ADDR << 1); i2c_write(0x0f | 0x80);
    i2c_start(); i2c_write((ACC_ADDR << 1) | 1); id = i2c_read(true); i2c_stop();
    if (id != 0x33) return false;

    i2c_start(); i2c_write(ACC_ADDR << 1); i2c_write(0x20); i2c_write(0x57); i2c_stop();
    i2c_start(); i2c_write(ACC_ADDR << 1); i2c_write(0x23); i2c_write(0x88); i2c_stop();

    return true;
}

V2 accel_gravity() {
    uint8_t b[6];
    i2c_start(); i2c_write(ACC_ADDR << 1); i2c_write(0x28 | 0x80);
    i2c_start(); i2c_write((ACC_ADDR << 1) | 1);
    for (uint8_t i = 0; i < 6; i++) b[i] = i2c_read(i == 5);
    i2c_stop();

    int16_t ax = (int16_t)((b[1] << 8) | b[0]) >> 4;
    int16_t ay = (int16_t)((b[3] << 8) | b[2]) >> 4;

    return {(fx)(ay * 32113), (fx)(-ax * 32113)};
}

bool have_accel = false;

// == entry ==

void setup() {
    Serial.begin(115200);

    for (auto& p : pins) {
        PORT->Group[p.first].PINCFG[p.second].reg = 0;
        PORT->Group[p.first].PINCFG[p.second].bit.DRVSTR = 1;
        PORT->Group[p.first].PINCFG[p.second].bit.PMUXEN = 0;
        PORT->Group[p.first].DIRCLR.reg = 1ul << p.second;
    }

    build_sched();
    timer_init(SLOT_US);

    have_accel = accel_init();
}

void log_frame() {
    static char frame[SIZE * (SIZE * 3 + 1) + 8];
    int n = 0;

    for (int y = 1; y < SIZE - 1; y++) {
        for (int x = 1; x < SIZE - 1; x++) {
            Cell& c = grid.cells[x + y * SIZE];
            char ch = c.s == 0 ? ' ' : (c.fluid ? '#' : '.');

            frame[n++] = ch;
            frame[n++] = ch;
            frame[n++] = ' ';
        }
        frame[n++] = '\n';
    }
    frame[n] = 0;

    Serial.write("\033[H\033[J");
    Serial.write(frame);
}

uint32_t last = 0;

void loop() {
    V2 g = {0, FX(-490.0f)};
    if (have_accel) g = accel_gravity();

    grid.update(g);
    publish();

    if (LOG && millis() - last > 200) {
      last = millis();
      log_frame();
    }
}
