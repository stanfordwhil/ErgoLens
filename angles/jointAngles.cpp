/* 
Script to:
1) Convert biomechanical joint angles to REBA/RULA joint angles
2) Compute the REBA/RULA score
Authored by Sam Kwong on August 13, 2019
 */

#include <iostream>
#include <vector>
using namespace std;

/*
Params: (some data structure) containing biomechanical joint angles
Output: vector of REBA/RULA joint angles
*/
vector<int> convertJointAngles(vector<int> biomechanical_angles) {
    /* Place holding for joint angles in biomechanical analysis (B) */

    // trunk flexion
    int B_T = biomechanical_angles[0];
    // neck flexion
    int B_N = biomechanical_angles[1];
    // upperleg horizontal
    int B_UL_H = biomechanical_angles[2];
    // upperleg vertical
    int B_UL_V = biomechanical_angles[3];
    // lowerleg horizontal
    int B_LL_H = biomechanical_angles[4];
    // lowerleg vertical
    int B_LL_V = biomechanical_angles[5];
    // upperarm horizontal
    int B_UA_H = biomechanical_angles[6];
    // upperarm vertical
    int B_UA_V = biomechanical_angles[7];
    // lowerarm horizontal
    int B_LA_H = biomechanical_angles[8];
    // lowerarm vertical
    int B_LA_V = biomechanical_angles[9];
    // hand horizontal
    int B_H_H = biomechanical_angles[10];
    // hand vertical
    int B_H_V = biomechanical_angles[11];

    /* Maintain vector of REBA/RULA joint angles */
    vector<int> reba_rula_angles;

    /* Conversions for joint angles in REBA/RULA (R) */

    // trunk
    int R_T = 90 - B_T;
    reba_rula_angles.push_back(R_T);

    // neck
    int R_N = B_T - B_N;
    reba_rula_angles.push_back(R_N);

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
    reba_rula_angles.push_back(R_L);

    // upperarm
    int R_UA = 0;
    if (B_UA_H > 0) {
        R_UA = 180 - abs(B_T - B_UA_V);
    } else if (B_UA_H < 0) {
        R_UA = -abs(B_T-B_UA_V);
    } else {
        throw "Error with upperarm angle calculations (B_UA_H)!";
    }
    reba_rula_angles.push_back(R_UA);

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
    reba_rula_angles.push_back(R_LA);

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
    reba_rula_angles.push_back(R_W);

    return reba_rula_angles;
}

int main(int argc, char *argv[]) {
    cout << "Converting biomechanical angles to REBA/RULA angles...";
    /* Place holder for biomechanical_angles */
    vector<int> biomechanical_angles;
    biomechanical_angles.push_back(0);

    vector<int> reba_rula_angles = convertJointAngles(biomechanical_angles);
    return 0;
}

