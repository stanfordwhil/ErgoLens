﻿#include <atomic>
#include <mutex>
#include <stdio.h>
#include "CWebSocketServer.hpp"
#ifdef USE_3D_RENDERER
    #include <GL/glut.h>
    #include <GL/freeglut_ext.h> // glutLeaveMainLoop
    #include <GL/freeglut_std.h>
#endif
#include <opencv2/opencv.hpp>
#include <openpose/face/faceParameters.hpp>
#include <openpose/hand/handParameters.hpp>
#include <openpose/pose/poseParameters.hpp>
#include <openpose/utilities/keypoint.hpp>
#include <openpose/gui/gui3D.hpp>

CWEBSOCKETS_STATIC_DEFINITIONS
Common::CWebSocketServer m_WSTransceiver;

namespace op
{

    #ifdef USE_3D_RENDERER
        const bool LOG_VERBOSE_3D_RENDERER = false;
        const auto WINDOW_WIDTH = 1280;
        const auto WINDOW_HEIGHT = 720;
        std::atomic<bool> sConstructorSet{false};

        struct Keypoints3D
        {
            Array<float> poseKeypoints;
            Array<float> faceKeypoints;
            Array<float> leftHandKeypoints;
            Array<float> rightHandKeypoints;
            bool validKeypoints;
            std::mutex mutex;
        };

        enum class CameraMode {
            CAM_DEFAULT,
            CAM_ROTATE,
            CAM_PAN,
            CAM_PAN_Z
        };

        Keypoints3D gKeypoints3D;
        PoseModel sPoseModel = PoseModel::BODY_25;
        int sLastKeyPressed = -1;

        CameraMode gCameraMode = CameraMode::CAM_DEFAULT;

        const std::vector<GLfloat> LIGHT_DIFFUSE{ 1.f, 1.f, 1.f, 1.f };  // Diffuse light
        const std::vector<GLfloat> LIGHT_POSITION{ 1.f, 1.f, 1.f, 0.f };  // Infinite light location
        const std::vector<GLfloat> COLOR_DIFFUSE{ 0.5f, 0.5f, 0.5f, 1.f };

        const auto RAD_TO_DEG = 0.0174532925199433;

        //View Change by Mouse
        bool gBButton1Down = false;
        auto gXClick = 0.f;
        auto gYClick = 0.f;
        auto gGViewDistance = -250.f; // -82.3994f; //-45;
        auto gMouseXRotateDeg = 180.f; // -63.2f; //0;
        auto gMouseYRotateDeg = -5.f; // 7.f; //60;
        auto gMouseXPan = -70.f; // 0;
        auto gMouseYPan = -30.f; // 0;
        auto gMouseZPan = 0.f;
        auto gScaleForMouseMotion = 0.1f;

        void drawConeByTwoPts(const cv::Point3f& pt1, const cv::Point3f& pt2, const float ptSize)
        {
            const GLdouble x1 = pt1.x;
            const GLdouble y1 = pt1.y;
            const GLdouble z1 = pt1.z;
            const GLdouble x2 = pt2.x;
            const GLdouble y2 = pt2.y;
            const GLdouble z2 = pt2.z;

            const double x = x2 - x1;
            const double y = y2 - y1;
            const double z = z2 - z1;

            glPushMatrix();

            glTranslated(x1, y1, z1);

            if ((x != 0.) || (y != 0.))
            {
                glRotated(std::atan2(y, x) / RAD_TO_DEG, 0., 0., 1.);
                glRotated(std::atan2(std::sqrt(x*x + y*y), z) / RAD_TO_DEG, 0., 1., 0.);
            }
            else if (z<0)
                glRotated(180, 1., 0., 0.);

            const auto height = std::sqrt((pt1.x - pt2.x)*(pt1.x - pt2.x) + (pt1.y - pt2.y)*(pt1.y - pt2.y)
                                          + (pt1.z - pt2.z)*(pt1.z - pt2.z));
            glutSolidCone(ptSize, height, 5, 5);

            glPopMatrix();
        }

        //we don't want to define this here..
        // const std::map<unsigned int, std::string> BODY {
        //     {"Nose", 0},             
        //     {"Neck", 1},
        //     {"RShoulder", 2},
        //     {"RElbow", 3},
        //     {"RWrist", 4},
        //     {"LShoulder", 5},
        //     {"LElbow", 6},
        //     {"LWrist", 7},
        //     {"MidHip", 8},
        //     {"RHip", 9},
        //     {"RKnee", 10},
        //     {"RAnkle", 11},
        //     {"LHip", 12},
        //     {"LKnee", 13},
        //     {"LAnkle", 14},
        //     {"REye", 15},
        //     {"LEye", 16},
        //     {"REar", 17},
        //     {"LEar", 18},
        //     {"LBigToe", 19},
        //     {"LSmallToe", 20},
        //     {"LHeel", 21},
        //     {"RBigToe", 22},
        //     {"RSmallToe", 23},
        //     {"RHeel", 24},
        //     {"Background", 25},
        //     };

        const auto& poseBody = getPoseBodyPartMapping(PoseModel::BODY_25);

        cv::Point3f coordinateGetter(const Array<float>& keypoints, int part) {
            //const auto x = poseKeypoints3d[{person, part, 0}];
            //const auto y = poseKeypoints3d[{person, part, 1}];
            const auto baseIndex = 4 * part;

            //getting x,y,z of the keypoint for body_part of interest

            return cv::Point3f (keypoints[baseIndex], keypoints[baseIndex + 1], keypoints[baseIndex + 2]);
            //return {keypoints[baseIndex], keypoints[baseIndex + 1], keypoints[baseIndex + 2]};
        }

        float calcVerticalAngles(const cv::Point3f& joint, const cv::Point3f& refjoint) {
            cv::Point3f partVect = refjoint - joint;


            const auto xOffset = -30000.f; // 640.f; //-3000
            const auto yOffset = 2000000.f; // 360.f; //1000
            const auto zOffset = 10000.f; // 360.f; //1000
            const auto xScale = 50000.f; //before 43.f //5000
            const auto yScale = 25000.f; //before 24.f //2500
            const auto zScale = 25000.f; //before 24.f //2500


            //figure out the axis, ref.x, ref.y, j.z BIG Q MARK
            cv::Point3f v_ref_pt = {joint.x, joint.y, refjoint.z};
            cv::Point3f v_ref_kpt{
                -(v_ref_pt.x - xOffset) / xScale,
                -(v_ref_pt.y - yOffset) / yScale,
                (v_ref_pt.z - zOffset) / zScale
            };
            // Create and add new sphere
            glPushMatrix();
            glTranslatef(v_ref_kpt.x, v_ref_kpt.y, v_ref_kpt.z);
            // Draw sphere
            glutSolidSphere(0.5 * 1.f, 20, 20);
            glPopMatrix();

            
            cv::Point3f refVect = v_ref_pt - joint;
            // for (int i = 0, i < 3, i++) {
            //     refVect[i] = r_ref_pt[i] - joint[i];
            // }

            float ref_mag = sqrt(refVect.dot(refVect));
            float pt_mag = sqrt(partVect.dot(partVect));
            float part_dot_ref = partVect.dot(refVect);
            float cos_theta = part_dot_ref/(ref_mag*pt_mag);
            //std::cout << "cos_theta :" << cos_theta << std::endl;

            auto part_cross_ref = partVect.cross(refVect);
            float part_cross_ref_mag = sqrt(part_cross_ref.dot(part_cross_ref));


            float sin_theta = part_cross_ref_mag/(ref_mag*pt_mag);

            auto theta = std::atan(sin_theta/cos_theta);
            //std::cout << "cos_theta :" << cos_theta << std::endl;
            //std::cout << "sin_theta :" << sin_theta << std::endl;
            //std::cout << "theta :" << theta << std::endl;

            auto pii = 3.141592653589793238463;

            auto degrees = theta * (180.0/pii); 
            //std::cout << "degrees : " << degrees << std::endl;
            //auto coef = 1 - (cos_theta  / abs(cos_theta));
            //std::cout << coef << std::endl;
            
            //theta = theta + coef * pii / 2;

            return degrees;

            //vec2angle: compute cos theta from dot product, compute sin from cross product, get atan...
            //auto cos_theta = std::inner_product(partVect, partVect+3, refVect, 0);
            //int cross_P[] = crossProduct(partVect, refVect);
            
        }

        void calculateAngles(const Array<float>& keypoints) 
        {
            const auto person = 0;
            //const auto numberPeople = keypoints.getSize(0);
            const auto numberBodyParts = keypoints.getSize(1);
            std::cout << numberBodyParts << std::endl; 
            // joint - ref_joint
            // elbow - wrist
            // hip - knee
            // knee - ankle
            // shoulder - elbow
            // foot

            std::stringstream wss;

            for (int i = 0; i < numberBodyParts-1; i++) {
                //if number of body_part
                cv::Point3f joint = coordinateGetter(keypoints, i);
                wss << joint.x << "," << joint.y << "," << joint.z << ",";
            }
            cv::Point3f jointlast = coordinateGetter(keypoints, numberBodyParts-1);
            wss << jointlast.x << "," << jointlast.y << "," << jointlast.z << ";";
            
            //wss  << joint.x << "," << joint.y << "," << joint.z;
            //m_WSTransceiver.SendData(wss.str());


            //auto l_lower_arm = calcVerticalAngles(BODY['LElbow'], BODY['LWrist']); //6, 7
            auto l_lower_arm = calcVerticalAngles(coordinateGetter(keypoints, 6), coordinateGetter(keypoints, 7));
            std::cout << "angle l_lower_arm:" << l_lower_arm << std::endl;

            //auto r_lower_arm = calcVerticalAngles(BODY['RElbow'], BODY['RWrist']); //3, 4
            auto r_lower_arm = calcVerticalAngles(coordinateGetter(keypoints, 3), coordinateGetter(keypoints, 4));
            std::cout << "angle r_lower_arm:" << r_lower_arm << std::endl;
            
            // r upper arm = Rshoulder and RElbow
            auto r_upper_arm = calcVerticalAngles(coordinateGetter(keypoints, 2), coordinateGetter(keypoints, 3));
            std::cout << "angle r_upper_arm:" << r_upper_arm << std::endl;
            
            // l upper arm = LShoulder and LElbow
            auto l_upper_arm = calcVerticalAngles(coordinateGetter(keypoints, 5), coordinateGetter(keypoints, 6));
            std::cout << "angle l_upper_arm:" << l_upper_arm << std::endl;
            
            
            //auto r_upper_leg = calcVerticalAngles(BODY['RHip'], BODY['RKnee']); //9, 10
            auto r_upper_leg = calcVerticalAngles(coordinateGetter(keypoints, 9), coordinateGetter(keypoints, 10));
            std::cout << "angle r_upper_leg:" << r_upper_leg << std::endl;
            //l_hip and l_knee
            auto l_upper_leg = calcVerticalAngles(coordinateGetter(keypoints, 12), coordinateGetter(keypoints, 13));
            std::cout << "angle l_upper_leg:" << l_upper_leg << std::endl;

            //r knee and r ankle 
            auto r_lower_leg = calcVerticalAngles(coordinateGetter(keypoints, 10), coordinateGetter(keypoints, 11));
            std::cout << "angle r_lower_leg:" << r_lower_leg << std::endl;

            // l knee and l ankle
            auto l_lower_leg = calcVerticalAngles(coordinateGetter(keypoints, 13), coordinateGetter(keypoints, 14));
            std::cout << "angle l_lower_leg:" << l_lower_leg << std::endl;

            wss << int(l_lower_arm) << "," << int(r_lower_arm) << "," << int(r_upper_arm) << "," << int(l_upper_arm) << "," << int(r_upper_leg) << "," << int(l_upper_leg) << "," << int(r_lower_leg) << "," << int(l_lower_leg);
            m_WSTransceiver.SendData(wss.str());

        }




        void renderHumanBody(const Array<float>& keypoints, const std::vector<unsigned int>& pairs,
                             const std::vector<float> colors, const float ratio)
        {
            const auto person = 0;
            const auto numberPeople = keypoints.getSize(0);
            const auto numberBodyParts = keypoints.getSize(1);
            const auto numberColors = colors.size();
            const auto xOffset = -30000.f; // 640.f; //-3000
            const auto yOffset = 2000000.f; // 360.f; //1000
            const auto zOffset = 10000.f; // 360.f; //1000
            const auto xScale = 50000.f; //before 43.f //5000
            const auto yScale = 25000.f; //before 24.f //2500
            const auto zScale = 25000.f; //before 24.f //2500

            if (numberPeople > person)
            //for(int person=0;person<numberPeople;++person)
            {
                // Circle for each keypoint
                for (auto part = 0; part < numberBodyParts; part++)
                {
                    // Set color
                    const auto colorIndex = part * 3;
                    const std::vector<float> keypointColor{
                        colors[colorIndex % numberColors] / 255.f,
                        colors[(colorIndex + 1) % numberColors] / 255.f,
                        colors[(colorIndex + 2) % numberColors] / 255.f,
                        1.f
                    };
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, COLOR_DIFFUSE.data());
                    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, keypointColor.data());
                    // Draw circle
                    const auto baseIndex = 4 * part + person*numberBodyParts;
                    if (keypoints[baseIndex + 3] > 0)
                    {
                        cv::Point3f keypoint{
                            -(keypoints[baseIndex] - xOffset) / xScale,
                            -(keypoints[baseIndex + 1] - yOffset) / yScale,
                            (keypoints[baseIndex + 2] - zOffset) / zScale
                        };
                        // Create and add new sphere
                        glPushMatrix();
                        glTranslatef(keypoint.x, keypoint.y, keypoint.z);
                        // Draw sphere
                        glutSolidSphere(0.5 * ratio, 20, 20);
                        glPopMatrix();
                    }
                }

                // Lines connecting each keypoint pair
                for (auto pair = 0u; pair < pairs.size(); pair += 2)
                {
                    // Set color
                    const auto colorIndex = pairs[pair+1] * 3;
                    const std::vector<float> keypointColor{
                        colors[colorIndex % numberColors] / 255.f,
                        colors[(colorIndex + 1) % numberColors] / 255.f,
                        colors[(colorIndex + 2) % numberColors] / 255.f,
                        1.f
                    };
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, COLOR_DIFFUSE.data());
                    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, keypointColor.data());
                    // Draw line
                    const auto baseIndexPairA = 4 * pairs[pair] + person*numberBodyParts;
                    const auto baseIndexPairB = 4 * pairs[pair + 1] + person*numberBodyParts;
                    if (keypoints[baseIndexPairA + 3] > 0 && keypoints[baseIndexPairB + 3] > 0)
                    {
                        cv::Point3f pairKeypointA{
                            -(keypoints[baseIndexPairA] - xOffset) / xScale,
                            -(keypoints[baseIndexPairA + 1] - yOffset) / yScale,
                            (keypoints[baseIndexPairA + 2] - zOffset) / zScale
                        };
                        cv::Point3f pairKeypointB{
                            -(keypoints[baseIndexPairB] - xOffset) / xScale,
                            -(keypoints[baseIndexPairB + 1] - yOffset) / yScale,
                            (keypoints[baseIndexPairB + 2] - zOffset) / zScale
                        };
                        drawConeByTwoPts(pairKeypointA, pairKeypointB, 0.5f * ratio);
                    }
                }
            }
        }

        void initGraphics()
        {
            // Enable a single OpenGL light
            glLightfv(GL_LIGHT0, GL_AMBIENT, LIGHT_DIFFUSE.data());
            glLightfv(GL_LIGHT0, GL_DIFFUSE, LIGHT_DIFFUSE.data());
            glLightfv(GL_LIGHT0, GL_POSITION, LIGHT_POSITION.data());
            glEnable(GL_LIGHT0);
            glEnable(GL_LIGHTING);

            // Use depth buffering for hidden surface elimination
            glEnable(GL_DEPTH_TEST);

            // Setup the view of the cube
            glMatrixMode(GL_PROJECTION);
            gluPerspective( /* field of view in degree */ 40.0,
                /* aspect ratio */ 1.0,
                /* Z near */ 1.0, /* Z far */ 15000.0); //increased far clipping plane from 1000 to 15000
            glMatrixMode(GL_MODELVIEW);
            gluLookAt(
                0.0, 0.0, 5.0,  // eye is at (0,0,5)
                0.0, 0.0, 0.0,  // center is at (0,0,0)
                0.0, 1.0, 0.  // up is in positive Y direction
            );

            // Adjust cube position to be asthetic angle
            glTranslatef(0.0, 0.0, -1.0);
            glRotatef(60, 1.0, 0.0, 0.0);
            glRotatef(-20, 0.0, 0.0, 1.0);

            glColorMaterial(GL_FRONT, GL_DIFFUSE);
            glEnable(GL_COLOR_MATERIAL);
        }

        // this is the actual idle function
        void idleFunction()
        {
            glutPostRedisplay();
            glutSwapBuffers();
        }

        void renderFloor()
        {
            glDisable(GL_LIGHTING);

            const cv::Point3f gGloorCenter{ 0,0,0 };   //ankle
            const cv::Point3f Noise{ 0,1,0 };

            cv::Point3f upright = Noise - gGloorCenter;
            upright = 1.0 / sqrt(upright.x *upright.x + upright.y *upright.y + upright.z *upright.z)*upright;
            const cv::Point3f gGloorAxis2 = cv::Point3f{ 1,0,0 }.cross(upright);
            const cv::Point3f gGloorAxis1 = gGloorAxis2.cross(upright);

            const auto gridNum = 10;
            const auto width = 50.;//sqrt(Distance(gGloorPts.front(),gGloorCenter)*2 /gridNum) * 1.2;
            const cv::Point3f origin = gGloorCenter - gGloorAxis1*(width*gridNum / 2)
                                     - gGloorAxis2*(width*gridNum / 2);
            const cv::Point3f axis1 = gGloorAxis1 * width;
            const cv::Point3f axis2 = gGloorAxis2 * width;
            for (auto y = 0; y <= gridNum; ++y)
            {
                for (auto x = 0; x <= gridNum; ++x)
                {
                    if ((x + y) % 2 == 0)
                        glColor4f(0.2f, 0.2f, 0.2f, 1.f); //black
                    else
                        glColor4f(0.5f, 0.5f, 0.5f, 1.f); //grey

                    const cv::Point3f p1 = origin + axis1*x + axis2*y;
                    const cv::Point3f p2 = p1 + axis1;
                    const cv::Point3f p3 = p1 + axis2;
                    const cv::Point3f p4 = p1 + axis1 + axis2;

                    glBegin(GL_QUADS);

                    glVertex3f(p1.x, p1.y, p1.z);
                    glVertex3f(p2.x, p2.y, p2.z);
                    glVertex3f(p4.x, p4.y, p4.z);
                    glVertex3f(p3.x, p3.y, p3.z);
                    glEnd();
                }
            }
            glEnable(GL_LIGHTING);
        }



        void renderMain(void)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glLoadIdentity();
            //gluLookAt(0,0,0, 0, 0, 1, 0, -1, 0);
            gluLookAt(
                0.0, 0.0, 5.0,  // eye is at (0,0,5)
                0.0, 0.0, 0.0,  // center is at (0,0,0)
                0.0, 1.0, 0.  // up is in positive Y direction
            );

            glTranslatef(0, 0, gGViewDistance);
            glRotatef(-gMouseYRotateDeg, 1.f, 0.f, 0.f);
            glRotatef(-gMouseXRotateDeg, 0.f, 1.f, 0.f);

            glTranslatef(-gMouseXPan, gMouseYPan, -gMouseZPan);

            renderFloor(); // Disabled, how to know where the floor is?
            std::unique_lock<std::mutex> lock{gKeypoints3D.mutex};

             //***************************************** Editted by Golrokh ************************************************
            glColor3f(1.0,0.0,0.0); // red x 
            glBegin(GL_LINES);
            // x aix
        
            glVertex3f(-400.0, 0.0f, 0.0f);
            glVertex3f(400.0, 0.0f, 0.0f);
        
            // arrow
            glVertex3f(4.0, 0.0f, 0.0f);
            glVertex3f(3.0, 1.0f, 0.0f);
        
            glVertex3f(4.0, 0.0f, 0.0f);
            glVertex3f(3.0, -1.0f, 0.0f);
            glEnd();
            glFlush();
        
        
        
            // y 
            glColor3f(0.0,1.0,0.0); // green y //vertical
            glBegin(GL_LINES);
            glVertex3f(0.0, -400.0f, 0.0f);
            glVertex3f(0.0, 400.0f, 0.0f);
        
            // arrow
            glVertex3f(0.0, 4.0f, 0.0f);
            glVertex3f(1.0, 3.0f, 0.0f);
        
            glVertex3f(0.0, 4.0f, 0.0f);
            glVertex3f(-1.0, 3.0f, 0.0f);
            glEnd();
            glFlush();
        
            // z 
            glColor3f(0.0,0.0,1.0); // blue z
            glBegin(GL_LINES);
            glVertex3f(0.0, 0.0f ,-400.0f );
            glVertex3f(0.0, 0.0f ,400.0f );
        
            // arrow
            glVertex3f(0.0, 0.0f ,4.0f );
            glVertex3f(0.0, 1.0f ,3.0f );
        
            glVertex3f(0.0, 0.0f ,4.0f );
            glVertex3f(0.0, -1.0f ,3.0f );
            glEnd();
            glFlush(); 
            //***************************************** finish editing ************************************************          
            m_WSTransceiver.Initialize();
            m_WSTransceiver.StartServer();
            if (gKeypoints3D.validKeypoints)
            {
                renderHumanBody(gKeypoints3D.poseKeypoints, getPoseBodyPartPairsRender(sPoseModel),
                                getPoseColors(sPoseModel), 1.f);
                renderHumanBody(gKeypoints3D.faceKeypoints, FACE_PAIRS_RENDER, FACE_COLORS_RENDER, 0.5f);
                renderHumanBody(gKeypoints3D.leftHandKeypoints, HAND_PAIRS_RENDER, HAND_COLORS_RENDER, 0.5f);
                renderHumanBody(gKeypoints3D.rightHandKeypoints, HAND_PAIRS_RENDER, HAND_COLORS_RENDER, 0.5f);
                calculateAngles(gKeypoints3D.poseKeypoints);
            }
            lock.unlock();

            glutSwapBuffers();
        }

        void mouseButton(const int button, const int state, const int x, const int y)
        {

            if (button == 3 || button == 4) //mouse wheel
            {
                if (button == 3)  //zoom in
                    gGViewDistance += 10 * gScaleForMouseMotion;
                else  //zoom out
                    gGViewDistance -= 10 * gScaleForMouseMotion;
                if (LOG_VERBOSE_3D_RENDERER)
                    log("gGViewDistance: " + std::to_string(gGViewDistance));
            }
            else
            {
                if (button == GLUT_LEFT_BUTTON)
                {
                    gBButton1Down = (state == GLUT_DOWN) ? 1 : 0;
                    gXClick = (float)x;
                    gYClick = (float)y;

                    if (glutGetModifiers() == GLUT_ACTIVE_SHIFT)
                        gCameraMode = CameraMode::CAM_PAN;
                    else
                        gCameraMode = CameraMode::CAM_ROTATE;
                }
                if (LOG_VERBOSE_3D_RENDERER)
                    log("Clicked: [" + std::to_string(gXClick) + "," + std::to_string(gYClick) + "]");
            }
            glutPostRedisplay();
        }

        void mouseMotion(const int x, const int y)
        {

            // If button1 pressed, zoom in/out if mouse is moved up/down.
            if (gBButton1Down)
            {
                if (gCameraMode == CameraMode::CAM_ROTATE)
                {
                    gMouseXRotateDeg += (x - gXClick)*0.2f;
                    gMouseYRotateDeg -= (y - gYClick)*0.2f;
                }
                else if (gCameraMode == CameraMode::CAM_PAN)
                {
                    gMouseXPan -= (x - gXClick) / 2 * gScaleForMouseMotion;
                    gMouseYPan -= (y - gYClick) / 2 * gScaleForMouseMotion;
                }
                else if (gCameraMode == CameraMode::CAM_PAN_Z)
                {
                    auto dist = std::sqrt(pow((x - gXClick), 2.0f) + pow((y - gYClick), 2.0f));
                    if (y < gYClick)
                        dist *= -1;
                    gMouseZPan -= dist / 5 * gScaleForMouseMotion;
                }

                gXClick = (float)x;
                gYClick = (float)y;

                glutPostRedisplay();
                if (LOG_VERBOSE_3D_RENDERER)
                {
                    log("gMouseXRotateDeg = " + std::to_string(gMouseXRotateDeg));
                    log("gMouseYRotateDeg = " + std::to_string(gMouseYRotateDeg));
                    log("gMouseXPan = " + std::to_string(gMouseXPan));
                    log("gMouseYPan = " + std::to_string(gMouseYPan));
                    log("gMouseZPan = " + std::to_string(gMouseZPan));
                }
            }
        }

        void keyPressed(const unsigned char key, const int x, const int y)
        {
            try
            {
                UNUSED(x);
                UNUSED(y);
                const std::lock_guard<std::mutex> lock{gKeypoints3D.mutex};
                sLastKeyPressed = key;
            }
            catch (const std::exception& e)
            {
                error(e.what(), __LINE__, __FUNCTION__, __FILE__);
            }
        }

        void initializeVisualization()
        {
            try
            {
                char *my_argv[] = { NULL };
                int my_argc = 0;
                glutInit(&my_argc, my_argv);
                // Setup the size, position, and display mode for new windows
                glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
                glutInitWindowPosition(200, 0);
                // glutSetOption(GLUT_MULTISAMPLE,8);
                // Ideally adding also GLUT_BORDERLESS | GLUT_CAPTIONLESS should fix the problem of disabling the `x`
                // button, but it does not work (tested in Ubuntu)
                // https://stackoverflow.com/questions/3799803/is-it-possible-to-make-a-window-withouth-a-top-in-glut
                glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
                // Create and set up a window
                glutCreateWindow(std::string{OPEN_POSE_NAME_AND_VERSION + " - 3-D Display"}.c_str());
                initGraphics();
                glutDisplayFunc(renderMain);
                glutMouseFunc(mouseButton);
                glutMotionFunc(mouseMotion);
                // Only required if glutMainLoop() called
                // glutIdleFunc(idleFunction);
                // Full screen would fix the problem of disabling `x` button
                // glutFullScreen();
                // Key presses
                glutKeyboardFunc(keyPressed);
            }
            catch (const std::exception& e)
            {
                error(e.what(), __LINE__, __FUNCTION__, __FILE__);
            }
        }
    #else
        const std::string USE_3D_RENDERER_ERROR{"OpenPose CMake must be compiled with the `USE_3D_RENDERER` flag in"
            " order to use the 3-D visualization renderer. Alternatively, set 2-D rendering with `--display 2`."};
    #endif

    Gui3D::Gui3D(const Point<int>& outputSize, const bool fullScreen,
                 const std::shared_ptr<std::atomic<bool>>& isRunningSharedPtr,
                 const std::shared_ptr<std::pair<std::atomic<bool>, std::atomic<int>>>& videoSeekSharedPtr,
                 const std::vector<std::shared_ptr<PoseExtractorNet>>& poseExtractorNets,
                 const std::vector<std::shared_ptr<FaceExtractorNet>>& faceExtractorNets,
                 const std::vector<std::shared_ptr<HandExtractorNet>>& handExtractorNets,
                 const std::vector<std::shared_ptr<Renderer>>& renderers, const PoseModel poseModel,
                 const DisplayMode displayMode, const bool copyGlToCvMat) :
        Gui{outputSize, fullScreen, isRunningSharedPtr, videoSeekSharedPtr, poseExtractorNets, faceExtractorNets,
            handExtractorNets, renderers, displayMode},
        mCopyGlToCvMat{copyGlToCvMat}
    {
        try
        {
            #ifdef USE_3D_RENDERER
                // Update sPoseModel
                sPoseModel = poseModel;
                if (!sConstructorSet)
                    sConstructorSet = true;
                else
                    error("The Gui3D class can only be initialized once.", __LINE__, __FUNCTION__, __FILE__);
            #else
                UNUSED(poseModel);
                if (mDisplayMode == DisplayMode::DisplayAll || mDisplayMode == DisplayMode::Display3D)
                    error(USE_3D_RENDERER_ERROR, __LINE__, __FUNCTION__, __FILE__);
            #endif
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    Gui3D::~Gui3D()
    {
        #ifdef USE_3D_RENDERER
            try
            {
                glutLeaveMainLoop();
            }
            catch (const std::exception& e)
            {
                errorDestructor(e.what(), __LINE__, __FUNCTION__, __FILE__);
            }
        #endif
    }

    void Gui3D::initializationOnThread()
    {
        try
        {
            // Init parent class
            Gui::initializationOnThread();
            #ifdef USE_3D_RENDERER
                // OpenGL - Initialization
                initializeVisualization();
            #endif
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    void Gui3D::setKeypoints(const Array<float>& poseKeypoints3D, const Array<float>& faceKeypoints3D,
                             const Array<float>& leftHandKeypoints3D, const Array<float>& rightHandKeypoints3D)
    {
        try
        {   
            // 3-D rendering
            #ifdef USE_3D_RENDERER
                if (mDisplayMode == DisplayMode::DisplayAll || mDisplayMode == DisplayMode::Display3D)
                {
                    if (!poseKeypoints3D.empty() || !faceKeypoints3D.empty()
                        || !leftHandKeypoints3D.empty() || !rightHandKeypoints3D.empty())
                    {
                        // OpenGL Rendering
                        // Copy new keypoints
                        std::unique_lock<std::mutex> lock{gKeypoints3D.mutex};
                        gKeypoints3D.poseKeypoints = poseKeypoints3D;
                        gKeypoints3D.faceKeypoints = faceKeypoints3D;
                        gKeypoints3D.leftHandKeypoints = leftHandKeypoints3D;
                        gKeypoints3D.rightHandKeypoints = rightHandKeypoints3D;
                        gKeypoints3D.validKeypoints = true;
                        // From m to mm
                        scaleKeypoints(gKeypoints3D.poseKeypoints, 1e3f);
                        scaleKeypoints(gKeypoints3D.faceKeypoints, 1e3f);
                        scaleKeypoints(gKeypoints3D.leftHandKeypoints, 1e3f);
                        scaleKeypoints(gKeypoints3D.rightHandKeypoints, 1e3f);
                        // Unlock mutex
                        lock.unlock();
                    }
                }
            #else
                UNUSED(poseKeypoints3D);
                UNUSED(faceKeypoints3D);
                UNUSED(leftHandKeypoints3D);
                UNUSED(rightHandKeypoints3D);
            #endif
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    void Gui3D::update()
    {
        try
        {
            // 2-D rendering
            // Display all 2-D views
            if (mDisplayMode == DisplayMode::DisplayAll || mDisplayMode == DisplayMode::Display2D)
                Gui::update();
            // 3-D rendering
            #ifdef USE_3D_RENDERER
                if (mDisplayMode == DisplayMode::DisplayAll || mDisplayMode == DisplayMode::Display3D)
                {
                    // OpenGL Rendering
                    std::unique_lock<std::mutex> lock{gKeypoints3D.mutex};
                    // Esc pressed -> Close program
                    if (sLastKeyPressed == 27)
                        if (spIsRunning != nullptr)
                            *spIsRunning = false;
                    lock.unlock();
                    // OpenCL - Run main loop event
                    // It is run outside loop, or it would get visually stuck if loop to slow
                    idleFunction();
                    glutMainLoopEvent();
                    // This alternative can only be called once, and it will block the thread until program exit
                    // glutMainLoop();
                }
            #endif
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    cv::Mat Gui3D::readCvMat()
    {
        try
        {
            // 3-D rendering
            cv::Mat image;
            #ifdef USE_3D_RENDERER
                if (mDisplayMode == DisplayMode::DisplayAll || mDisplayMode == DisplayMode::Display3D)
                {
                    // Save/display 3D display in OpenCV window
                    if (mCopyGlToCvMat)
                    {
                        image = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);
                        #ifdef _WIN32
                            glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_BGR_EXT, GL_UNSIGNED_BYTE, image.data);
                        #else
                            glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_BGR, GL_UNSIGNED_BYTE, image.data);
                        #endif
                        cv::flip(image, image, 0);
                    }
                }
            #endif
            return image;
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
            return cv::Mat();
        }
    }
}
