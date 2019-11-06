#ifndef _SNAPSHOT_HPP
#define _SNAPSHOT_HPP

#include <types.hpp>
#include <worker.hpp>

template<typename T>
class LabeledValue {
private:
    u_int32_t label;
    T value;
    Array<T> snap;
public:
    LabeledValue() {
    }

    LabeledValue(T initValue): label(0), value(initValue) {
    }

    LabeledValue(u_int32_t label, T value, Array<T> snap): label(label), value(value), snap(snap) {
    }

    u_int32_t getLabel() {
        return label;
    }

    T getValue() {
        return value;
    }

    const Array<T> getSnap() {
        return snap;
    }

    LabeledValue& operator=(const LabeledValue& other) {
         if (this == &other) {
             return *this;
         }

        this->label = other.label;
        this->value = other.value;
        this->snap = other.snap;

        return *this;
    }
};

template<typename T>
class Snapshot {
private:
    int capacity;
    Array<LabeledValue<T>> registers;
    Array<LabeledValue<T>> collect() {
        Array<LabeledValue<T>> copy(capacity);
        
        for (int i = 0; i < capacity; ++i) {
            copy[i] = registers[i];
        }

        return copy;
    }
public:
    Snapshot(int capacity, T initValue) {
        this->capacity = capacity;
        this->registers = Array<LabeledValue<T>>(capacity);

        for (int i = 0; i < capacity; ++i) {
            registers[i] = LabeledValue<T>(initValue);
        }
    }

    void update(T value) {
        int me = WorkerThread::getThreadId();
        Array<T> snap = scan();
        LabeledValue<T> oldValue = registers[me - 1];
        LabeledValue<T> newValue = LabeledValue<T>(oldValue.getLabel() + 1, value, snap);
        registers[me - 1] = newValue;
    }

    Array<T> scan() {
        Array<LabeledValue<T>> oldCopy, newCopy;
        Array<bool> moved(capacity, false);
        oldCopy = collect();

    COLLECT:
        newCopy = collect();
        for (int i = 0; i < capacity; ++i) {
            if (oldCopy[i].getLabel() != newCopy[i].getLabel()) {
                if (moved[i]) {
                    return oldCopy[i].getSnap();
                } else {
                    moved[i] = true;
                    oldCopy = newCopy;
                    goto COLLECT;
                }
            }
        }

        Array<T> result(capacity);
        for (int i = 0; i < capacity; ++i) {
            result[i] = newCopy[i].getValue();
        }

        return result;
    }
};

#endif
