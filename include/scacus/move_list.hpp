#pragma once

#include "scacus/bitboard.hpp"

namespace sc {
    class MoveList {
    public:
        Move data[256];
        Move *head = nullptr;
        Move *tail = nullptr;

        inline MoveList() = delete;

        MoveList &operator=(const MoveList &&rhs) noexcept = delete;;
        MoveList(const MoveList &rhs) noexcept = delete;

        inline explicit MoveList(int) : head(data), tail(head) {};

        inline MoveList &operator=(MoveList &&rhs) noexcept {
            // skipping same-identity check
            // if (&rhs == this) return *this;

            head = rhs.head;
            tail = rhs.tail;
            rhs.head = nullptr;
            return *this;
        }

        inline MoveList(MoveList &&rhs) noexcept : head(rhs.head), tail(rhs.tail) {
            rhs.head = nullptr;
        }

        inline void push_back(const Move &mov) {
            *tail++ = mov;
        }

        inline void push_back(Move &&mov) {
            *tail++ = mov;
        }

        [[nodiscard]] inline bool empty() const {
            return tail == head;
        }

        [[nodiscard]] inline std::size_t size() const {
            return tail - head;
        }

        [[nodiscard]] inline Move *begin() const {
            return head;
        }

        [[nodiscard]] inline Move *end() const {
            return tail;
        }

        [[nodiscard]] inline Move &at(std::size_t x) const {
            return head[x];
        }

        inline void clear() {
            tail = head;
        }
    };
}