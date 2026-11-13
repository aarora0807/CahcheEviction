// Tracer.cpp
#include "Tracer.h"

#include <iomanip>
#include <ostream>

namespace cachesim {

Tracer::Tracer(std::ostream& out, std::size_t maxLines) : out_(out), maxLines_(maxLines) {
    out_ << " step | request | result | victim | cache contents\n"
         << "------+---------+--------+--------+---------------------------\n";
}

void Tracer::onHit(Key) { lastHit_ = true; }
void Tracer::onMiss(Key) { lastHit_ = false; }
void Tracer::onEvict(Key victim, std::int64_t) { lastVictim_ = victim; }

void Tracer::onRequestComplete(const Cache& cache, Key key) {
    ++step_;
    if (step_ <= maxLines_) {
        out_ << std::setw(5) << step_ << " | " << std::setw(7) << key << " | "
             << (lastHit_ ? "HIT   " : "MISS  ") << " | ";
        if (lastVictim_) {
            out_ << std::setw(6) << *lastVictim_;
        } else {
            out_ << std::setw(6) << '-';
        }
        out_ << " | [";
        bool first = true;
        for (Key k : cache.keys()) {
            out_ << (first ? "" : " ") << k;
            first = false;
        }
        out_ << "]\n";
    } else if (step_ == maxLines_ + 1) {
        out_ << "  ... (trace truncated after " << maxLines_ << " steps)\n";
    }
    lastVictim_.reset();
}

}  // namespace cachesim
