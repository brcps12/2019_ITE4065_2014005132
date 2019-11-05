#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

using namespace std;

#define NUM_THREAD_IN_WORKERPOOL 10
#define NUM_THREAD_IN_PRINTERPOOL 1

void printCnt(int seqno, int range_start, int range_end, int cnt) {
    cout << "(" << seqno << ")number of primes in " << range_start << " ~ " << range_end << " is " << cnt << endl;
}

bool isPrime(int n) {
    if (n < 2) {
        return false;
    }

    for (int i = 2; i <= sqrt(n); i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

void numPrimes(boost::asio::io_service *printer, int seqno, int range_start, int range_end) {
    long cnt_prime = 0;

    for (int i = range_start; i < range_end; i++) {
        if (isPrime(i)) {
            cnt_prime++;
        }
    }

    printer->post(boost::bind(printCnt, seqno, range_start, range_end, cnt_prime));
}

int main() {
    int seqno = 0;
    boost::asio::io_service workerio, printerio;
    boost::thread_group workerpool, printerpool;
    boost::asio::io_service::work
        *findwork = new boost::asio::io_service::work(workerio),
        *printwork = new boost::asio::io_service::work(printerio);

    for (int i = 0; i < NUM_THREAD_IN_WORKERPOOL; i++) {
        workerpool.create_thread(
            boost::bind(&boost::asio::io_service::run, &workerio)
        );
    }

    for (int i = 0; i < NUM_THREAD_IN_PRINTERPOOL; i++) {
        printerpool.create_thread(
            boost::bind(&boost::asio::io_service::run, &printerio)
        );
    }

    while (1) {
        int range_start, range_end;
        cin >> range_start;

        if (range_start == -1) break;
        cin >> range_end;

        workerio.post(
            boost::bind(
                numPrimes,
                &printerio,
                seqno,
                range_start,
                range_end
            )
        );

        ++seqno;
    }

    delete findwork;
    workerpool.join_all();
    workerio.stop();

    delete printwork;
    printerpool.join_all();
    printerio.stop();

    return 0;
}
