#include "ModeHistory.h"

#define MH_MAX_SEGS 64

struct _Seg { float s; float e; uint8_t t; };

static _Seg  _segs[MH_MAX_SEGS];
static int   _count = 0;

void mhStart(uint8_t type, float hourFloat) {
    // Ferme le segment en cours s'il est ouvert
    if (_count > 0 && _segs[_count - 1].e < 0.0f) {
        _segs[_count - 1].e = hourFloat;
    }
    // Ouvre un nouveau segment si on a de la place
    if (_count < MH_MAX_SEGS) {
        _segs[_count].s = hourFloat;
        _segs[_count].e = -1.0f;  // -1 = en cours
        _segs[_count].t = type;
        _count++;
    }
}

void mhReset() {
    _count = 0;
}

String mhToJSON() {
    String json = "[";
    for (int i = 0; i < _count; i++) {
        if (i > 0) json += ",";
        json += "{\"s\":" + String(_segs[i].s, 3)
              + ",\"e\":" + String(_segs[i].e, 3)
              + ",\"t\":" + String(_segs[i].t) + "}";
    }
    json += "]";
    return json;
}
