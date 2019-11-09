#include <atomic>
#include <memory>
#include <snapshot.hpp>
#include <worker.hpp>

Snapshot::Snapshot(int capacity) {
    this->capacity = capacity;
    this->registers = new ValuePtr[capacity];

    for (int i = 0; i < capacity; ++i) {
        this->registers[i] = std::make_shared<StampedValue>(0);
    }
}

Snapshot::~Snapshot() {
    delete[] registers;
} 

ValuePtr * Snapshot::collect() {
    ValuePtr *copy = new ValuePtr[capacity];
    
    for (int i = 0; i < capacity; ++i) {
        copy[i] = std::atomic_load(&registers[i]);
    }

    return copy;
}

void Snapshot::update(int value) {
    int me = WorkerThread::currentThreadId();
    int* snap = scan();
    auto oldValue = std::atomic_load(&registers[me - 1]);
    auto newValue = std::make_shared<StampedValue>(oldValue->stamp + 1, value, snap, capacity);
    std::atomic_store(&registers[me - 1], newValue);
    delete[] snap;
}

int* Snapshot::scan() {
    ValuePtr *oldCopy, *newCopy;
    bool *moved = new bool[capacity]();
    int *result = new int[capacity]();
    oldCopy = collect();

COLLECT:
    newCopy = collect();
    for (int i = 0; i < capacity; ++i) {
        if (oldCopy[i]->stamp != newCopy[i]->stamp) {
            if (moved[i]) {
                for (int j = 0; j < capacity; ++j) {
                    result[j] = oldCopy[i]->snap[j];
                }

                delete[] oldCopy;
                goto RESULT;
            } else {
                moved[i] = true;
                delete[] oldCopy;
                oldCopy = newCopy;

                goto COLLECT;
            }
        }
    }

    for (int i = 0; i < capacity; ++i) {
        result[i] = newCopy[i]->value;
    }

    delete[] oldCopy;
RESULT:
    delete[] newCopy;
    delete[] moved;

    return result;
}
