#ifndef _SNAPSHOT_HPP
#define _SNAPSHOT_HPP

#include <stdio.h>
#include <memory>
#include <atomic>

// Assume that creation and assignment are atomic
struct StampedValue {
    int stamp;
    int value;
    int* snap;
    int capacity;

    StampedValue(): stamp(0), value(0), snap(NULL), capacity(0) {}

    StampedValue(int value): stamp(0), value(value), snap(NULL), capacity(0) {}

    StampedValue(int stamp, int value, int* snap, int capacity): stamp(stamp), value(value), capacity(capacity) {
        if (snap != NULL && capacity > 0) {
            this->snap = new int[capacity];

            for (int i = 0; i < capacity; ++i) {
                this->snap[i] = snap[i];
            }
        }
    }

    ~StampedValue() {
        if (snap != NULL) {
            delete[] this->snap;
        }
    }

    StampedValue& operator=(const StampedValue& other) {
        if (this == &other) {
            return *this;
        }

        if (this->snap == NULL && other.capacity > 0) {
            this->snap = new int[other.capacity];
        }

        this->stamp = other.stamp;
        this->value = other.value;
        this->capacity = other.capacity;

        for (int i = 0; i < other.capacity; ++i) {
            this->snap[i] = other.snap[i];
        }

        return *this;
    }
};

typedef std::shared_ptr<StampedValue> ValuePtr;

class Snapshot {
private:
    int capacity;
    ValuePtr * registers;

    ValuePtr * collect();
public:
    Snapshot(int capacity);

    ~Snapshot();

    void update(int value);

    int* scan();
};

#endif
