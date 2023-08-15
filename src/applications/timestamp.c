#include <wsong/timing.h>

int main(int argc, char** argv) {
    ws_timing_punch(1000,1,2);
    ws_timing_punch(1001,2,3);
    ws_timing_punch(1002,3,4);
    ws_timing_save("time1.dat");
    ws_timing_clear();
    ws_timing_punch(2000,1,2);
    ws_timing_punch(2001,2,3);
    ws_timing_punch(2002,3,4);
    ws_timing_save("time2.dat");
    return 0;
}
