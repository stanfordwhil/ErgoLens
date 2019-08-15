// Authored by Sam Kwong on August 13, 2019

#include <iostream>
using namespace std;

int convertJointAngles() {
    /* place holding for joint angles in biomechanical analysis (B) */

    // trunk flexion
    int B_T = 0;
    // neck flexion
    int B_N = 0;
    // upperleg horizontal
    int B_UL_H = 0;
    // upperleg vertical
    int B_UL_V = 0;
    // lowerleg horizontal
    int B_LL_H = 0;
    // lowerleg vertical
    int B_LL_V = 0;
    // upperarm horizontal
    int B_UA_H = 0;
    // upperarm vertical
    int B_UA_V = 0;
    // lowerarm horizontal
    int B_LA_H = 0;
    // lowerarm vertical
    int B_LA_V = 0;
    // hand horizontal
    int B_H_H = 0;
    // hand vertical
    int B_H_V = 0;

    /* conversions for joint angles in REBA/RULA (R) */

    // trunk
    int R_T = 90 - B_T;
    // neck
    int R_N = B_T - B_N;
    // leg
    int R_L = 0;
    if (B_UL_H > 0 && B_LL_H > 0) {
        R_L = abs(B_UL_V - B_LL_V);
    } else if (B_UL_H > 0 && B_LL_H < 0) {
        R_L = 180 - abs(B_UL_V + B_LL_V);
    } else if (B_UL_H < 0 && B_LL_H > 0) {
        R_L = 180 - abs(B_UL_V + B_LL_V);
    } else if (B_UL_H < 0 && B_LL_H < 0) {
        R_L = abs(B_UL_V - B_LL_V);
    } else {
        throw "Error with leg angle calculations (B_UL_H and B_LL_H)!";
    }
    // upperarm
    int R_UA = 0;
    if (B_UA_H > 0) {
        R_UA = 180 - abs(B_T - B_UA_V);
    } else if (B_UA_H < 0) {
        R_UA = -abs(B_T-B_UA_V);
    } else {
        throw "Error with upperarm angle calculations (B_UA_H)!";
    }
    // lowerarm
    int R_LA = 0;
    if (B_UA_H > 0 && B_LA_H > 0) {
        R_LA = abs(B_UA_V - B_LA_V);
    } else if (B_UA_H > 0 && B_LA_H < 0) {
        R_LA = 180 - abs(B_UA_V + B_LA_V);
    } else if (B_UA_H < 0 && B_LA_H > 0) {
        R_LA = 180 - abs(B_UA_V + B_LA_V);
    } else if (B_UA_H < 0 && B_LA_H < 0) {
        R_LA = abs(B_UA_V - B_LA_V);
    } else {
        throw "Error with lowerarm angle calculations (B_UA_H and B_LA_H)!";
    }
    // wrist
    int R_W = 0;
    if (B_LA_H > 0 && B_H_H > 0) {
        R_W = abs(B_LA_V - B_H_V);
    } else if (B_LA_H > 0 && B_H_H < 0) {
        R_W = 180 - abs(B_LA_V + B_H_V);
    } else if (B_LA_H > 0 && B_H_H > 0) {
        R_W = 180 - abs(B_LA_V + B_H_V);
    } else if (B_LA_H > 0 && B_H_H > 0) {
        R_W = abs(B_LA_V - B_H_V);
    } else {
        throw "Error with wrist angle calculations (B_LA_H and B_H_H)!";
    }

}

int main(int argc, char *argv[]) {
    cout << "Hello World";
    return 0;
}

