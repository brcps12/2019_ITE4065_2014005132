#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <algorithm>

using namespace std;

typedef int32_t record_index_t;
typedef int64_t record_value_t;

struct Record {
    record_index_t index;
    record_value_t value;
};

struct Log {
    int commitId;
    record_index_t i, j, k;
    record_value_t vi, vj, vk;
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cout << "usage: " << argv[0] << " <N> <R> <E>" << endl;
        cout << "N" << "\tthe number of worker threads" << endl;
        cout << "R" << "\tthe number of records" << endl;
        cout << "E" << "\tlast global execution order" << endl;

        return 0;
    }

    int N = atoi(argv[1]), R = atoi(argv[2]), E = atoi(argv[3]);

    vector<Record> records(R);
    vector<Log> logs;
    vector<ifstream> files(N);

    for (int i = 0; i < R; i++) {
        records[i].index = i + 1;
        records[i].value = 100;
    }

    Log log;
    for (int i = 0; i < N; i++) {
        char buf[30];
        sprintf(buf, "thread%d.txt", i + 1);
        files[i].open(buf);
        
        while (!files[i].eof()) {
            files[i] >> log.commitId;

            if (files[i].eof()) break;

            files[i] >> log.i >> log.j >> log.k >> log.vi >> log.vj >> log.vk;
            logs.push_back(log);
        }
    }

    if (logs.size() != E) {
        cout << "Expected the number of logs is " << E << ", but actually is " << logs.size() << endl;
        goto VALFAILED;
    }

    sort(logs.begin(), logs.end(), [](const Log& a, const Log& b) {
        return a.commitId < b.commitId;
    });

    for (int i = 0; i < E; i++) {
        if (logs[i].commitId != i + 1) {
            cout << "[CommitId:" << (i + 1) << "] >> Missing log which commitId = " << (i + 1) << endl;
            goto VALFAILED;
        }

        record_index_t ri = logs[i].i, rj = logs[i].j, rk = logs[i].k;
        record_index_t wrongIndex = -1;

        if (ri > R) wrongIndex = ri;
        if (rj > R) wrongIndex = rj;
        if (rk > R) wrongIndex = rk;

        if (wrongIndex != -1) {
            cout << "[CommitId:" << (i + 1) << "] >> Record " << wrongIndex << " does not exist. All records should be indexed lower than " << R << endl;
            goto VALFAILED;
        }

        record_value_t vj = logs[i].vj, vk = logs[i].vk;
        records[rj - 1].value += records[ri - 1].value + 1;
        records[rk - 1].value -= records[ri - 1].value;

        bool hasWrongValue = false;
        record_value_t expectedValue, actualValue;

        if (vj != records[rj - 1].value) {
            hasWrongValue = true;
            expectedValue = records[rj - 1].value;
            actualValue = vj;
        }

        if (vk != records[rk - 1].value) {
            hasWrongValue = true;
            expectedValue = records[rk - 1].value;
            actualValue = vk;
        }

        if (hasWrongValue) {
            cout << "[CommitId:" << (i + 1) << "] >> Expected record value: " << expectedValue << ", but actual value: " << actualValue << endl;
            goto VALFAILED;
        }
    }

    goto VALSUCCESS;

VALFAILED:
    cout << "Validation failed!" << endl;
    return 0;

VALSUCCESS:
    cout << "Validation succeeded!" << endl;
    return 0;
}
