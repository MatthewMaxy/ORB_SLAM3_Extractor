/**
* This file is part of ORB-SLAM3
*
* Copyright (C) 2017-2021 Carlos Campos, Richard Elvira, Juan J. Gómez Rodríguez, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
* Copyright (C) 2014-2016 Raúl Mur-Artal, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
*
* ORB-SLAM3 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM3 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
* the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with ORB-SLAM3.
* If not, see <http://www.gnu.org/licenses/>.
*/

/**
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*
*/


#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <vector>
#include <iostream>

#include "ORBextractor.h"


using namespace cv;
using namespace std;

namespace ORB_SLAM3
{

    const int PATCH_SIZE = 31;
    const int HALF_PATCH_SIZE = 15;
    const int EDGE_THRESHOLD = 19;


    static float IC_Angle(const Mat& image, Point2f pt,  const vector<int> & u_max)
    {
        int m_01 = 0, m_10 = 0;

        const uchar* center = &image.at<uchar> (cvRound(pt.y), cvRound(pt.x));

        // Treat the center line differently, v=0
        for (int u = -HALF_PATCH_SIZE; u <= HALF_PATCH_SIZE; ++u)
            m_10 += u * center[u];

        // Go line by line in the circuI853lar patch
        int step = (int)image.step1();
        for (int v = 1; v <= HALF_PATCH_SIZE; ++v)
        {
            // Proceed over the two lines
            int v_sum = 0;
            int d = u_max[v];
            for (int u = -d; u <= d; ++u)
            {
                int val_plus = center[u + v*step], val_minus = center[u - v*step];
                v_sum += (val_plus - val_minus);
                m_10 += u * (val_plus + val_minus);
            }
            m_01 += v * v_sum;
        }

        return fastAtan2((float)m_01, (float)m_10);
    }


    const float factorPI = (float)(CV_PI/180.f);
    static void computeOrbDescriptor(const KeyPoint& kpt,
                                     const Mat& img, const Point* pattern,
                                     uchar* desc)
    {
        float angle = (float)kpt.angle*factorPI;
        float a = (float)cos(angle), b = (float)sin(angle);

        const uchar* center = &img.at<uchar>(cvRound(kpt.pt.y), cvRound(kpt.pt.x));
        const int step = (int)img.step;

#define GET_VALUE(idx) \
        center[cvRound(pattern[idx].x*b + pattern[idx].y*a)*step + \
               cvRound(pattern[idx].x*a - pattern[idx].y*b)]


        for (int i = 0; i < 32; ++i, pattern += 16)
        {
            int t0, t1, val;
            t0 = GET_VALUE(0); t1 = GET_VALUE(1);
            val = t0 < t1;
            t0 = GET_VALUE(2); t1 = GET_VALUE(3);
            val |= (t0 < t1) << 1;
            t0 = GET_VALUE(4); t1 = GET_VALUE(5);
            val |= (t0 < t1) << 2;
            t0 = GET_VALUE(6); t1 = GET_VALUE(7);
            val |= (t0 < t1) << 3;
            t0 = GET_VALUE(8); t1 = GET_VALUE(9);
            val |= (t0 < t1) << 4;
            t0 = GET_VALUE(10); t1 = GET_VALUE(11);
            val |= (t0 < t1) << 5;
            t0 = GET_VALUE(12); t1 = GET_VALUE(13);
            val |= (t0 < t1) << 6;
            t0 = GET_VALUE(14); t1 = GET_VALUE(15);
            val |= (t0 < t1) << 7;

            desc[i] = (uchar)val;
        }

#undef GET_VALUE
    }


    static int bit_pattern_31_[256*4] =
            {
                    8,-3, 9,5/*mean (0), correlation (0)*/,
                    4,2, 7,-12/*mean (1.12461e-05), correlation (0.0437584)*/,
                    -11,9, -8,2/*mean (3.37382e-05), correlation (0.0617409)*/,
                    7,-12, 12,-13/*mean (5.62303e-05), correlation (0.0636977)*/,
                    2,-13, 2,12/*mean (0.000134953), correlation (0.085099)*/,
                    1,-7, 1,6/*mean (0.000528565), correlation (0.0857175)*/,
                    -2,-10, -2,-4/*mean (0.0188821), correlation (0.0985774)*/,
                    -13,-13, -11,-8/*mean (0.0363135), correlation (0.0899616)*/,
                    -13,-3, -12,-9/*mean (0.121806), correlation (0.099849)*/,
                    10,4, 11,9/*mean (0.122065), correlation (0.093285)*/,
                    -13,-8, -8,-9/*mean (0.162787), correlation (0.0942748)*/,
                    -11,7, -9,12/*mean (0.21561), correlation (0.0974438)*/,
                    7,7, 12,6/*mean (0.160583), correlation (0.130064)*/,
                    -4,-5, -3,0/*mean (0.228171), correlation (0.132998)*/,
                    -13,2, -12,-3/*mean (0.00997526), correlation (0.145926)*/,
                    -9,0, -7,5/*mean (0.198234), correlation (0.143636)*/,
                    12,-6, 12,-1/*mean (0.0676226), correlation (0.16689)*/,
                    -3,6, -2,12/*mean (0.166847), correlation (0.171682)*/,
                    -6,-13, -4,-8/*mean (0.101215), correlation (0.179716)*/,
                    11,-13, 12,-8/*mean (0.200641), correlation (0.192279)*/,
                    4,7, 5,1/*mean (0.205106), correlation (0.186848)*/,
                    5,-3, 10,-3/*mean (0.234908), correlation (0.192319)*/,
                    3,-7, 6,12/*mean (0.0709964), correlation (0.210872)*/,
                    -8,-7, -6,-2/*mean (0.0939834), correlation (0.212589)*/,
                    -2,11, -1,-10/*mean (0.127778), correlation (0.20866)*/,
                    -13,12, -8,10/*mean (0.14783), correlation (0.206356)*/,
                    -7,3, -5,-3/*mean (0.182141), correlation (0.198942)*/,
                    -4,2, -3,7/*mean (0.188237), correlation (0.21384)*/,
                    -10,-12, -6,11/*mean (0.14865), correlation (0.23571)*/,
                    5,-12, 6,-7/*mean (0.222312), correlation (0.23324)*/,
                    5,-6, 7,-1/*mean (0.229082), correlation (0.23389)*/,
                    1,0, 4,-5/*mean (0.241577), correlation (0.215286)*/,
                    9,11, 11,-13/*mean (0.00338507), correlation (0.251373)*/,
                    4,7, 4,12/*mean (0.131005), correlation (0.257622)*/,
                    2,-1, 4,4/*mean (0.152755), correlation (0.255205)*/,
                    -4,-12, -2,7/*mean (0.182771), correlation (0.244867)*/,
                    -8,-5, -7,-10/*mean (0.186898), correlation (0.23901)*/,
                    4,11, 9,12/*mean (0.226226), correlation (0.258255)*/,
                    0,-8, 1,-13/*mean (0.0897886), correlation (0.274827)*/,
                    -13,-2, -8,2/*mean (0.148774), correlation (0.28065)*/,
                    -3,-2, -2,3/*mean (0.153048), correlation (0.283063)*/,
                    -6,9, -4,-9/*mean (0.169523), correlation (0.278248)*/,
                    8,12, 10,7/*mean (0.225337), correlation (0.282851)*/,
                    0,9, 1,3/*mean (0.226687), correlation (0.278734)*/,
                    7,-5, 11,-10/*mean (0.00693882), correlation (0.305161)*/,
                    -13,-6, -11,0/*mean (0.0227283), correlation (0.300181)*/,
                    10,7, 12,1/*mean (0.125517), correlation (0.31089)*/,
                    -6,-3, -6,12/*mean (0.131748), correlation (0.312779)*/,
                    10,-9, 12,-4/*mean (0.144827), correlation (0.292797)*/,
                    -13,8, -8,-12/*mean (0.149202), correlation (0.308918)*/,
                    -13,0, -8,-4/*mean (0.160909), correlation (0.310013)*/,
                    3,3, 7,8/*mean (0.177755), correlation (0.309394)*/,
                    5,7, 10,-7/*mean (0.212337), correlation (0.310315)*/,
                    -1,7, 1,-12/*mean (0.214429), correlation (0.311933)*/,
                    3,-10, 5,6/*mean (0.235807), correlation (0.313104)*/,
                    2,-4, 3,-10/*mean (0.00494827), correlation (0.344948)*/,
                    -13,0, -13,5/*mean (0.0549145), correlation (0.344675)*/,
                    -13,-7, -12,12/*mean (0.103385), correlation (0.342715)*/,
                    -13,3, -11,8/*mean (0.134222), correlation (0.322922)*/,
                    -7,12, -4,7/*mean (0.153284), correlation (0.337061)*/,
                    6,-10, 12,8/*mean (0.154881), correlation (0.329257)*/,
                    -9,-1, -7,-6/*mean (0.200967), correlation (0.33312)*/,
                    -2,-5, 0,12/*mean (0.201518), correlation (0.340635)*/,
                    -12,5, -7,5/*mean (0.207805), correlation (0.335631)*/,
                    3,-10, 8,-13/*mean (0.224438), correlation (0.34504)*/,
                    -7,-7, -4,5/*mean (0.239361), correlation (0.338053)*/,
                    -3,-2, -1,-7/*mean (0.240744), correlation (0.344322)*/,
                    2,9, 5,-11/*mean (0.242949), correlation (0.34145)*/,
                    -11,-13, -5,-13/*mean (0.244028), correlation (0.336861)*/,
                    -1,6, 0,-1/*mean (0.247571), correlation (0.343684)*/,
                    5,-3, 5,2/*mean (0.000697256), correlation (0.357265)*/,
                    -4,-13, -4,12/*mean (0.00213675), correlation (0.373827)*/,
                    -9,-6, -9,6/*mean (0.0126856), correlation (0.373938)*/,
                    -12,-10, -8,-4/*mean (0.0152497), correlation (0.364237)*/,
                    10,2, 12,-3/*mean (0.0299933), correlation (0.345292)*/,
                    7,12, 12,12/*mean (0.0307242), correlation (0.366299)*/,
                    -7,-13, -6,5/*mean (0.0534975), correlation (0.368357)*/,
                    -4,9, -3,4/*mean (0.099865), correlation (0.372276)*/,
                    7,-1, 12,2/*mean (0.117083), correlation (0.364529)*/,
                    -7,6, -5,1/*mean (0.126125), correlation (0.369606)*/,
                    -13,11, -12,5/*mean (0.130364), correlation (0.358502)*/,
                    -3,7, -2,-6/*mean (0.131691), correlation (0.375531)*/,
                    7,-8, 12,-7/*mean (0.160166), correlation (0.379508)*/,
                    -13,-7, -11,-12/*mean (0.167848), correlation (0.353343)*/,
                    1,-3, 12,12/*mean (0.183378), correlation (0.371916)*/,
                    2,-6, 3,0/*mean (0.228711), correlation (0.371761)*/,
                    -4,3, -2,-13/*mean (0.247211), correlation (0.364063)*/,
                    -1,-13, 1,9/*mean (0.249325), correlation (0.378139)*/,
                    7,1, 8,-6/*mean (0.000652272), correlation (0.411682)*/,
                    1,-1, 3,12/*mean (0.00248538), correlation (0.392988)*/,
                    9,1, 12,6/*mean (0.0206815), correlation (0.386106)*/,
                    -1,-9, -1,3/*mean (0.0364485), correlation (0.410752)*/,
                    -13,-13, -10,5/*mean (0.0376068), correlation (0.398374)*/,
                    7,7, 10,12/*mean (0.0424202), correlation (0.405663)*/,
                    12,-5, 12,9/*mean (0.0942645), correlation (0.410422)*/,
                    6,3, 7,11/*mean (0.1074), correlation (0.413224)*/,
                    5,-13, 6,10/*mean (0.109256), correlation (0.408646)*/,
                    2,-12, 2,3/*mean (0.131691), correlation (0.416076)*/,
                    3,8, 4,-6/*mean (0.165081), correlation (0.417569)*/,
                    2,6, 12,-13/*mean (0.171874), correlation (0.408471)*/,
                    9,-12, 10,3/*mean (0.175146), correlation (0.41296)*/,
                    -8,4, -7,9/*mean (0.183682), correlation (0.402956)*/,
                    -11,12, -4,-6/*mean (0.184672), correlation (0.416125)*/,
                    1,12, 2,-8/*mean (0.191487), correlation (0.386696)*/,
                    6,-9, 7,-4/*mean (0.192668), correlation (0.394771)*/,
                    2,3, 3,-2/*mean (0.200157), correlation (0.408303)*/,
                    6,3, 11,0/*mean (0.204588), correlation (0.411762)*/,
                    3,-3, 8,-8/*mean (0.205904), correlation (0.416294)*/,
                    7,8, 9,3/*mean (0.213237), correlation (0.409306)*/,
                    -11,-5, -6,-4/*mean (0.243444), correlation (0.395069)*/,
                    -10,11, -5,10/*mean (0.247672), correlation (0.413392)*/,
                    -5,-8, -3,12/*mean (0.24774), correlation (0.411416)*/,
                    -10,5, -9,0/*mean (0.00213675), correlation (0.454003)*/,
                    8,-1, 12,-6/*mean (0.0293635), correlation (0.455368)*/,
                    4,-6, 6,-11/*mean (0.0404971), correlation (0.457393)*/,
                    -10,12, -8,7/*mean (0.0481107), correlation (0.448364)*/,
                    4,-2, 6,7/*mean (0.050641), correlation (0.455019)*/,
                    -2,0, -2,12/*mean (0.0525978), correlation (0.44338)*/,
                    -5,-8, -5,2/*mean (0.0629667), correlation (0.457096)*/,
                    7,-6, 10,12/*mean (0.0653846), correlation (0.445623)*/,
                    -9,-13, -8,-8/*mean (0.0858749), correlation (0.449789)*/,
                    -5,-13, -5,-2/*mean (0.122402), correlation (0.450201)*/,
                    8,-8, 9,-13/*mean (0.125416), correlation (0.453224)*/,
                    -9,-11, -9,0/*mean (0.130128), correlation (0.458724)*/,
                    1,-8, 1,-2/*mean (0.132467), correlation (0.440133)*/,
                    7,-4, 9,1/*mean (0.132692), correlation (0.454)*/,
                    -2,1, -1,-4/*mean (0.135695), correlation (0.455739)*/,
                    11,-6, 12,-11/*mean (0.142904), correlation (0.446114)*/,
                    -12,-9, -6,4/*mean (0.146165), correlation (0.451473)*/,
                    3,7, 7,12/*mean (0.147627), correlation (0.456643)*/,
                    5,5, 10,8/*mean (0.152901), correlation (0.455036)*/,
                    0,-4, 2,8/*mean (0.167083), correlation (0.459315)*/,
                    -9,12, -5,-13/*mean (0.173234), correlation (0.454706)*/,
                    0,7, 2,12/*mean (0.18312), correlation (0.433855)*/,
                    -1,2, 1,7/*mean (0.185504), correlation (0.443838)*/,
                    5,11, 7,-9/*mean (0.185706), correlation (0.451123)*/,
                    3,5, 6,-8/*mean (0.188968), correlation (0.455808)*/,
                    -13,-4, -8,9/*mean (0.191667), correlation (0.459128)*/,
                    -5,9, -3,-3/*mean (0.193196), correlation (0.458364)*/,
                    -4,-7, -3,-12/*mean (0.196536), correlation (0.455782)*/,
                    6,5, 8,0/*mean (0.1972), correlation (0.450481)*/,
                    -7,6, -6,12/*mean (0.199438), correlation (0.458156)*/,
                    -13,6, -5,-2/*mean (0.211224), correlation (0.449548)*/,
                    1,-10, 3,10/*mean (0.211718), correlation (0.440606)*/,
                    4,1, 8,-4/*mean (0.213034), correlation (0.443177)*/,
                    -2,-2, 2,-13/*mean (0.234334), correlation (0.455304)*/,
                    2,-12, 12,12/*mean (0.235684), correlation (0.443436)*/,
                    -2,-13, 0,-6/*mean (0.237674), correlation (0.452525)*/,
                    4,1, 9,3/*mean (0.23962), correlation (0.444824)*/,
                    -6,-10, -3,-5/*mean (0.248459), correlation (0.439621)*/,
                    -3,-13, -1,1/*mean (0.249505), correlation (0.456666)*/,
                    7,5, 12,-11/*mean (0.00119208), correlation (0.495466)*/,
                    4,-2, 5,-7/*mean (0.00372245), correlation (0.484214)*/,
                    -13,9, -9,-5/*mean (0.00741116), correlation (0.499854)*/,
                    7,1, 8,6/*mean (0.0208952), correlation (0.499773)*/,
                    7,-8, 7,6/*mean (0.0220085), correlation (0.501609)*/,
                    -7,-4, -7,1/*mean (0.0233806), correlation (0.496568)*/,
                    -8,11, -7,-8/*mean (0.0236505), correlation (0.489719)*/,
                    -13,6, -12,-8/*mean (0.0268781), correlation (0.503487)*/,
                    2,4, 3,9/*mean (0.0323324), correlation (0.501938)*/,
                    10,-5, 12,3/*mean (0.0399235), correlation (0.494029)*/,
                    -6,-5, -6,7/*mean (0.0420153), correlation (0.486579)*/,
                    8,-3, 9,-8/*mean (0.0548021), correlation (0.484237)*/,
                    2,-12, 2,8/*mean (0.0616622), correlation (0.496642)*/,
                    -11,-2, -10,3/*mean (0.0627755), correlation (0.498563)*/,
                    -12,-13, -7,-9/*mean (0.0829622), correlation (0.495491)*/,
                    -11,0, -10,-5/*mean (0.0843342), correlation (0.487146)*/,
                    5,-3, 11,8/*mean (0.0929937), correlation (0.502315)*/,
                    -2,-13, -1,12/*mean (0.113327), correlation (0.48941)*/,
                    -1,-8, 0,9/*mean (0.132119), correlation (0.467268)*/,
                    -13,-11, -12,-5/*mean (0.136269), correlation (0.498771)*/,
                    -10,-2, -10,11/*mean (0.142173), correlation (0.498714)*/,
                    -3,9, -2,-13/*mean (0.144141), correlation (0.491973)*/,
                    2,-3, 3,2/*mean (0.14892), correlation (0.500782)*/,
                    -9,-13, -4,0/*mean (0.150371), correlation (0.498211)*/,
                    -4,6, -3,-10/*mean (0.152159), correlation (0.495547)*/,
                    -4,12, -2,-7/*mean (0.156152), correlation (0.496925)*/,
                    -6,-11, -4,9/*mean (0.15749), correlation (0.499222)*/,
                    6,-3, 6,11/*mean (0.159211), correlation (0.503821)*/,
                    -13,11, -5,5/*mean (0.162427), correlation (0.501907)*/,
                    11,11, 12,6/*mean (0.16652), correlation (0.497632)*/,
                    7,-5, 12,-2/*mean (0.169141), correlation (0.484474)*/,
                    -1,12, 0,7/*mean (0.169456), correlation (0.495339)*/,
                    -4,-8, -3,-2/*mean (0.171457), correlation (0.487251)*/,
                    -7,1, -6,7/*mean (0.175), correlation (0.500024)*/,
                    -13,-12, -8,-13/*mean (0.175866), correlation (0.497523)*/,
                    -7,-2, -6,-8/*mean (0.178273), correlation (0.501854)*/,
                    -8,5, -6,-9/*mean (0.181107), correlation (0.494888)*/,
                    -5,-1, -4,5/*mean (0.190227), correlation (0.482557)*/,
                    -13,7, -8,10/*mean (0.196739), correlation (0.496503)*/,
                    1,5, 5,-13/*mean (0.19973), correlation (0.499759)*/,
                    1,0, 10,-13/*mean (0.204465), correlation (0.49873)*/,
                    9,12, 10,-1/*mean (0.209334), correlation (0.49063)*/,
                    5,-8, 10,-9/*mean (0.211134), correlation (0.503011)*/,
                    -1,11, 1,-13/*mean (0.212), correlation (0.499414)*/,
                    -9,-3, -6,2/*mean (0.212168), correlation (0.480739)*/,
                    -1,-10, 1,12/*mean (0.212731), correlation (0.502523)*/,
                    -13,1, -8,-10/*mean (0.21327), correlation (0.489786)*/,
                    8,-11, 10,-6/*mean (0.214159), correlation (0.488246)*/,
                    2,-13, 3,-6/*mean (0.216993), correlation (0.50287)*/,
                    7,-13, 12,-9/*mean (0.223639), correlation (0.470502)*/,
                    -10,-10, -5,-7/*mean (0.224089), correlation (0.500852)*/,
                    -10,-8, -8,-13/*mean (0.228666), correlation (0.502629)*/,
                    4,-6, 8,5/*mean (0.22906), correlation (0.498305)*/,
                    3,12, 8,-13/*mean (0.233378), correlation (0.503825)*/,
                    -4,2, -3,-3/*mean (0.234323), correlation (0.476692)*/,
                    5,-13, 10,-12/*mean (0.236392), correlation (0.475462)*/,
                    4,-13, 5,-1/*mean (0.236842), correlation (0.504132)*/,
                    -9,9, -4,3/*mean (0.236977), correlation (0.497739)*/,
                    0,3, 3,-9/*mean (0.24314), correlation (0.499398)*/,
                    -12,1, -6,1/*mean (0.243297), correlation (0.489447)*/,
                    3,2, 4,-8/*mean (0.00155196), correlation (0.553496)*/,
                    -10,-10, -10,9/*mean (0.00239541), correlation (0.54297)*/,
                    8,-13, 12,12/*mean (0.0034413), correlation (0.544361)*/,
                    -8,-12, -6,-5/*mean (0.003565), correlation (0.551225)*/,
                    2,2, 3,7/*mean (0.00835583), correlation (0.55285)*/,
                    10,6, 11,-8/*mean (0.00885065), correlation (0.540913)*/,
                    6,8, 8,-12/*mean (0.0101552), correlation (0.551085)*/,
                    -7,10, -6,5/*mean (0.0102227), correlation (0.533635)*/,
                    -3,-9, -3,9/*mean (0.0110211), correlation (0.543121)*/,
                    -1,-13, -1,5/*mean (0.0113473), correlation (0.550173)*/,
                    -3,-7, -3,4/*mean (0.0140913), correlation (0.554774)*/,
                    -8,-2, -8,3/*mean (0.017049), correlation (0.55461)*/,
                    4,2, 12,12/*mean (0.01778), correlation (0.546921)*/,
                    2,-5, 3,11/*mean (0.0224022), correlation (0.549667)*/,
                    6,-9, 11,-13/*mean (0.029161), correlation (0.546295)*/,
                    3,-1, 7,12/*mean (0.0303081), correlation (0.548599)*/,
                    11,-1, 12,4/*mean (0.0355151), correlation (0.523943)*/,
                    -3,0, -3,6/*mean (0.0417904), correlation (0.543395)*/,
                    4,-11, 4,12/*mean (0.0487292), correlation (0.542818)*/,
                    2,-4, 2,1/*mean (0.0575124), correlation (0.554888)*/,
                    -10,-6, -8,1/*mean (0.0594242), correlation (0.544026)*/,
                    -13,7, -11,1/*mean (0.0597391), correlation (0.550524)*/,
                    -13,12, -11,-13/*mean (0.0608974), correlation (0.55383)*/,
                    6,0, 11,-13/*mean (0.065126), correlation (0.552006)*/,
                    0,-1, 1,4/*mean (0.074224), correlation (0.546372)*/,
                    -13,3, -9,-2/*mean (0.0808592), correlation (0.554875)*/,
                    -9,8, -6,-3/*mean (0.0883378), correlation (0.551178)*/,
                    -13,-6, -8,-2/*mean (0.0901035), correlation (0.548446)*/,
                    5,-9, 8,10/*mean (0.0949843), correlation (0.554694)*/,
                    2,7, 3,-9/*mean (0.0994152), correlation (0.550979)*/,
                    -1,-6, -1,-1/*mean (0.10045), correlation (0.552714)*/,
                    9,5, 11,-2/*mean (0.100686), correlation (0.552594)*/,
                    11,-3, 12,-8/*mean (0.101091), correlation (0.532394)*/,
                    3,0, 3,5/*mean (0.101147), correlation (0.525576)*/,
                    -1,4, 0,10/*mean (0.105263), correlation (0.531498)*/,
                    3,-6, 4,5/*mean (0.110785), correlation (0.540491)*/,
                    -13,0, -10,5/*mean (0.112798), correlation (0.536582)*/,
                    5,8, 12,11/*mean (0.114181), correlation (0.555793)*/,
                    8,9, 9,-6/*mean (0.117431), correlation (0.553763)*/,
                    7,-4, 8,-12/*mean (0.118522), correlation (0.553452)*/,
                    -10,4, -10,9/*mean (0.12094), correlation (0.554785)*/,
                    7,3, 12,4/*mean (0.122582), correlation (0.555825)*/,
                    9,-7, 10,-2/*mean (0.124978), correlation (0.549846)*/,
                    7,0, 12,-2/*mean (0.127002), correlation (0.537452)*/,
                    -1,-6, 0,-11/*mean (0.127148), correlation (0.547401)*/
            };

    ORBextractor::ORBextractor(int _nfeatures, float _scaleFactor, int _nlevels,
                               int _iniThFAST, int _minThFAST):
            nfeatures(_nfeatures), scaleFactor(_scaleFactor), nlevels(_nlevels),
            iniThFAST(_iniThFAST), minThFAST(_minThFAST)
    {
//        ORBextractor::ORBextractor(int _nfeatures,		//指定要提取的特征点数目
//                float _scaleFactor,	//指定图像金字塔的缩放系数
//                int _nlevels,		//指定图像金字塔的层数
//                int _iniThFAST,		//指定初始的FAST特征点提取阈值，可以提取出最明显的角点
//                int _minThFAST):		//如果初始阈值没有检测到角点，降低到这个阈值提取出弱一点的角点

        mvScaleFactor.resize(nlevels);
        mvLevelSigma2.resize(nlevels);
        mvScaleFactor[0]=1.0f;
        mvLevelSigma2[0]=1.0f;
        for(int i=1; i<nlevels; i++)
        {
            mvScaleFactor[i]=mvScaleFactor[i-1]*scaleFactor;
            mvLevelSigma2[i]=mvScaleFactor[i]*mvScaleFactor[i];
        }

        mvInvScaleFactor.resize(nlevels);
        mvInvLevelSigma2.resize(nlevels);
        for(int i=0; i<nlevels; i++)
        {
            mvInvScaleFactor[i]=1.0f/mvScaleFactor[i];
            mvInvLevelSigma2[i]=1.0f/mvLevelSigma2[i];
        }

        mvImagePyramid.resize(nlevels);

        mnFeaturesPerLevel.resize(nlevels);
        float factor = 1.0f / scaleFactor;
        float nDesiredFeaturesPerScale = nfeatures*(1 - factor)/(1 - (float)pow((double)factor, (double)nlevels));

        int sumFeatures = 0;
        for( int level = 0; level < nlevels-1; level++ )
        {
            mnFeaturesPerLevel[level] = cvRound(nDesiredFeaturesPerScale);
            sumFeatures += mnFeaturesPerLevel[level];
            nDesiredFeaturesPerScale *= factor;
        }
        mnFeaturesPerLevel[nlevels-1] = std::max(nfeatures - sumFeatures, 0);

        const int npoints = 512;
        const Point* pattern0 = (const Point*)bit_pattern_31_;
        std::copy(pattern0, pattern0 + npoints, std::back_inserter(pattern));

        //This is for orientation
        // pre-compute the end of a row in a circular patch
        umax.resize(HALF_PATCH_SIZE + 1);

        int v, v0, vmax = cvFloor(HALF_PATCH_SIZE * sqrt(2.f) / 2 + 1);
        int vmin = cvCeil(HALF_PATCH_SIZE * sqrt(2.f) / 2);
        const double hp2 = HALF_PATCH_SIZE*HALF_PATCH_SIZE;
        for (v = 0; v <= vmax; ++v)
            umax[v] = cvRound(sqrt(hp2 - v * v));

        // Make sure we are symmetric
        for (v = HALF_PATCH_SIZE, v0 = 0; v >= vmin; --v)
        {
            while (umax[v0] == umax[v0 + 1])
                ++v0;
            umax[v] = v0;
            ++v0;
        }
    }

    // 在ComputeKeyPointsOctTree()中调用
    static void computeOrientation(const Mat& image, vector<KeyPoint>& keypoints, const vector<int>& umax)
    {
        for (vector<KeyPoint>::iterator keypoint = keypoints.begin(),
                     keypointEnd = keypoints.end(); keypoint != keypointEnd; ++keypoint)
        {
            keypoint->angle = IC_Angle(image, keypoint->pt, umax);
        }
    }

    // 在 DistributeOctTree()中调用
    void ExtractorNode::DivideNode(ExtractorNode &n1, ExtractorNode &n2, ExtractorNode &n3, ExtractorNode &n4)
    {
        const int halfX = ceil(static_cast<float>(UR.x-UL.x)/2);
        const int halfY = ceil(static_cast<float>(BR.y-UL.y)/2);

        //Define boundaries of childs
        n1.UL = UL;
        n1.UR = cv::Point2i(UL.x+halfX,UL.y);
        n1.BL = cv::Point2i(UL.x,UL.y+halfY);
        n1.BR = cv::Point2i(UL.x+halfX,UL.y+halfY);
        n1.vKeys.reserve(vKeys.size());

        n2.UL = n1.UR;
        n2.UR = UR;
        n2.BL = n1.BR;
        n2.BR = cv::Point2i(UR.x,UL.y+halfY);
        n2.vKeys.reserve(vKeys.size());

        n3.UL = n1.BL;
        n3.UR = n1.BR;
        n3.BL = BL;
        n3.BR = cv::Point2i(n1.BR.x,BL.y);
        n3.vKeys.reserve(vKeys.size());

        n4.UL = n3.UR;
        n4.UR = n2.BR;
        n4.BL = n3.BR;
        n4.BR = BR;
        n4.vKeys.reserve(vKeys.size());

        //Associate points to childs
        for(size_t i=0;i<vKeys.size();i++)
        {
            const cv::KeyPoint &kp = vKeys[i];
            if(kp.pt.x<n1.UR.x)
            {
                if(kp.pt.y<n1.BR.y)
                    n1.vKeys.push_back(kp);
                else
                    n3.vKeys.push_back(kp);
            }
            else if(kp.pt.y<n1.BR.y)
                n2.vKeys.push_back(kp);
            else
                n4.vKeys.push_back(kp);
        }

        if(n1.vKeys.size()==1)
            n1.bNoMore = true;
        if(n2.vKeys.size()==1)
            n2.bNoMore = true;
        if(n3.vKeys.size()==1)
            n3.bNoMore = true;
        if(n4.vKeys.size()==1)
            n4.bNoMore = true;

    }

    // 在DistributeOctTree()中调用
    static bool compareNodes(pair<int,ExtractorNode*>& e1, pair<int,ExtractorNode*>& e2){
        if(e1.first < e2.first){
            return true;
        }
        else if(e1.first > e2.first){
            return false;
        }
        else{
            if(e1.second->UL.x < e2.second->UL.x){
                return true;
            }
            else{
                return false;
            }
        }
    }

    // finish
    // 在ComputeKeyPointsOctTree中调用
    vector<cv::KeyPoint> ORBextractor::DistributeOctTree(const vector<cv::KeyPoint>& vToDistributeKeys, const int &minX,
                                                         const int &maxX, const int &minY, const int &maxY, const int &N, const int &level)
    {
        /// Step 1 根据宽高比确定初始节点数目

        // Compute how many initial nodes
        // 根据宽高比确定初始节点数目，一般是1或者2
        // ?? bug: 如果宽高比小于0.5，nIni=0
        const int nIni = round(static_cast<float>(maxX-minX)/(maxY-minY));

        const float hX = static_cast<float>(maxX-minX)/nIni;

        // 存储有提取器节点的列表
        list<ExtractorNode> lNodes;
        // 存储初始提取器节点指针的vector,然后重新设置其大小
        vector<ExtractorNode*> vpIniNodes;
        vpIniNodes.resize(nIni);

        /// Step 2 生成初始提取器节点，遍历初始节点个数
        for(int i=0; i<nIni; i++)
        {
            // 构造一个提取器节点 ni
            ExtractorNode ni;

            // 确定提取器节点的图像边界,注意这里和提取FAST角点区域相同，都是“半径扩充图像”，特征点坐标从0开始
            ni.UL = cv::Point2i(hX*static_cast<float>(i),0);
            ni.UR = cv::Point2i(hX*static_cast<float>(i+1),0);
            ni.BL = cv::Point2i(ni.UL.x,maxY-minY);
            ni.BR = cv::Point2i(ni.UR.x,maxY-minY);
            // 将刚才生成的提取节点添加到列表中
            ni.vKeys.reserve(vToDistributeKeys.size());// 虽然这里的ni是局部变量，但是由于这里的push_back()是拷贝参数的内容到一个
            // 新的对象中然后再添加到列表中，所以当本函数退出之后这里的内存不会成为“野指针”
            lNodes.push_back(ni);
            vpIniNodes[i] = &lNodes.back();
        }

        /// Step 3 将特征点分配到子提取器节点中
        //Associate points to childs
        for(size_t i=0;i<vToDistributeKeys.size();i++)
        {
            // 按特征点的横轴位置，分配给属于那个图像区域的提取器节点（最初的提取器节点）
            const cv::KeyPoint &kp = vToDistributeKeys[i];
            vpIniNodes[kp.pt.x/hX]->vKeys.push_back(kp);
        }

        /// Step 4 遍历此提取器节点列表，标记那些不可再分裂的节点，删除那些没有分配到特征点的节点
        list<ExtractorNode>::iterator lit = lNodes.begin();
        while(lit!=lNodes.end())
        {
            if(lit->vKeys.size()==1) // 如果初始的提取器节点所分配到的特征点个数为1
            {
                lit->bNoMore=true;  // 那么就标志位置位，表示此节点不可再分
                lit++;
            }
            else if(lit->vKeys.empty()) // 如果一个提取器节点没有被分配到特征点，那么就从列表中直接删除它
                lit = lNodes.erase(lit);
            else // 如果上面的这些情况和当前的特征点提取器节点无关，那么就只是更新迭代器
                lit++;
        }

        bool bFinish = false;

        int iteration = 0;
        // 声明一个vector用于存储节点的vSize和句柄对
        // 这个变量记录了在一次分裂循环中，那些可以再继续进行分裂的节点中包含的特征点数目和其句柄
        vector<pair<int,ExtractorNode*> > vSizeAndPointerToNode;

        // 调整大小，这里的意思是一个初始化节点将“分裂”成为四个，当然实际上不会有那么多，这里多分配了一些只是预防万一
        vSizeAndPointerToNode.reserve(lNodes.size()*4);

        /// Step 5 根据兴趣点分布,利用4叉树方法对图像进行划分区域
        while(!bFinish)
        {
            // 更新迭代次数计数器，只是记录，并未起到作用
            iteration++;

            // 保存当前节点个数，prev在这里理解为“保留”比较好
            int prevSize = lNodes.size();

            // 重新定位迭代器指向列表头部
            lit = lNodes.begin();

            // 需要展开的节点计数，这个一直保持累计，不清零
            int nToExpand = 0;

            vSizeAndPointerToNode.clear();

            // 将目前的子区域进行划分，开始遍历列表中所有的提取器节点，并进行分解或者保留
            while(lit!=lNodes.end())
            {
                // 如果提取器节点只有一个特征点，
                if(lit->bNoMore)
                {
                    // If node only contains one point do not subdivide and continue
                    // 如果节点只包含一个点不细分继续，跳过当前节点，继续下一个
                    lit++;
                    continue;
                }
                else
                {
                    // If more than one point, subdivide
                    // 如果当前的提取器节点具有超过一个的特征点，那么就要进行继续细分
                    ExtractorNode n1,n2,n3,n4;
                    lit->DivideNode(n1,n2,n3,n4);

                    // Add childs if they contain points
                    // 如果这里分出来的子区域中有特征点，那么就将这个子区域的节点添加到提取器节点的列表中
                    if(n1.vKeys.size()>0)
                    {
                        lNodes.push_front(n1);
                        if(n1.vKeys.size()>1)// 再判断其中子提取器节点中的特征点数目是否大于1
                        {//保存这个特征点数目和节点指针的信息
                            nToExpand++;// 如果有超过一个的特征点，那么“待展开的节点计数++”
                            vSizeAndPointerToNode.push_back(make_pair(n1.vKeys.size(),&lNodes.front()));
                            lNodes.front().lit = lNodes.begin();
                        }
                    }
                    if(n2.vKeys.size()>0)
                    {
                        lNodes.push_front(n2);
                        if(n2.vKeys.size()>1)
                        {
                            nToExpand++;
                            vSizeAndPointerToNode.push_back(make_pair(n2.vKeys.size(),&lNodes.front()));
                            lNodes.front().lit = lNodes.begin();
                        }
                    }
                    if(n3.vKeys.size()>0)
                    {
                        lNodes.push_front(n3);
                        if(n3.vKeys.size()>1)
                        {
                            nToExpand++;
                            vSizeAndPointerToNode.push_back(make_pair(n3.vKeys.size(),&lNodes.front()));
                            lNodes.front().lit = lNodes.begin();
                        }
                    }
                    if(n4.vKeys.size()>0)
                    {
                        lNodes.push_front(n4);
                        if(n4.vKeys.size()>1)
                        {
                            nToExpand++;
                            vSizeAndPointerToNode.push_back(make_pair(n4.vKeys.size(),&lNodes.front()));
                            lNodes.front().lit = lNodes.begin();
                        }
                    }
                    // 当这个母节点expand之后就从列表中删除它了，能够进行分裂操作说明至少有一个子节点的区域中特征点的数量是>1的
                    lit=lNodes.erase(lit);
                    continue;
                }
            }

            // Finish if there are more nodes than required features
            // or all nodes contain just one point
            // 停止这个过程的条件有两个，满足其中一个即可：1、当前的节点数已经超过了要求的特征点数，2、当前所有的节点中都只包含一个特征点
            if((int)lNodes.size()>=N || (int)lNodes.size()==prevSize)
            {
                bFinish = true;
            }
            else if(((int)lNodes.size()+nToExpand*3)>N)
            {
                // Step 6 当再划分之后所有的Node数大于要求数目时,就慢慢划分直到使其刚刚达到或者超过要求的特征点个数
                // 可以展开的子节点个数nToExpand x3，是因为一分四之后，会删除原来的主节点，所以乘以3
                // 如果再分裂一次那么数目就要超了，这里想办法尽可能使其刚刚达到或者超过要求的特征点个数时就退出

                // 一直循环，直到结束标志位被置位
                while(!bFinish)
                {

                    prevSize = lNodes.size();
                    // Prev这里是应该是保留的意思吧，保留那些还可以分裂的节点的信息, 这里是深拷贝
                    vector<pair<int,ExtractorNode*> > vPrevSizeAndPointerToNode = vSizeAndPointerToNode;
                    vSizeAndPointerToNode.clear();
                    // 对需要划分的节点进行排序，对pair对的第一个元素进行排序，默认是从小到大排序
                    sort(vPrevSizeAndPointerToNode.begin(),vPrevSizeAndPointerToNode.end(),compareNodes);
                    // 遍历这个存储了pair对的vector，注意是从后往前遍历
                    for(int j=vPrevSizeAndPointerToNode.size()-1;j>=0;j--)
                    {
                        // 对每个需要进行分裂的节点进行分裂，一份四，与上第一层分裂类似
                        ExtractorNode n1,n2,n3,n4;
                        vPrevSizeAndPointerToNode[j].second->DivideNode(n1,n2,n3,n4);

                        // Add childs if they contain points
                        if(n1.vKeys.size()>0)
                        {
                            lNodes.push_front(n1);
                            if(n1.vKeys.size()>1)
                            {
                                vSizeAndPointerToNode.push_back(make_pair(n1.vKeys.size(),&lNodes.front()));
                                lNodes.front().lit = lNodes.begin();
                            }
                        }
                        if(n2.vKeys.size()>0)
                        {
                            lNodes.push_front(n2);
                            if(n2.vKeys.size()>1)
                            {
                                vSizeAndPointerToNode.push_back(make_pair(n2.vKeys.size(),&lNodes.front()));
                                lNodes.front().lit = lNodes.begin();
                            }
                        }
                        if(n3.vKeys.size()>0)
                        {
                            lNodes.push_front(n3);
                            if(n3.vKeys.size()>1)
                            {
                                vSizeAndPointerToNode.push_back(make_pair(n3.vKeys.size(),&lNodes.front()));
                                lNodes.front().lit = lNodes.begin();
                            }
                        }
                        if(n4.vKeys.size()>0)
                        {
                            lNodes.push_front(n4);
                            if(n4.vKeys.size()>1)
                            {
                                vSizeAndPointerToNode.push_back(make_pair(n4.vKeys.size(),&lNodes.front()));
                                lNodes.front().lit = lNodes.begin();
                            }
                        }
                        // 删除母节点，在这里其实应该是一级子节点
                        lNodes.erase(vPrevSizeAndPointerToNode[j].second->lit);
                        //判断是是否超过了需要的特征点数？是的话就退出，不是的话就继续这个分裂过程，直到刚刚达到或者超过要求的特征点个数
                        if((int)lNodes.size()>=N)
                            break;
                    }
                    // 判断是否达到了停止条件
                    if((int)lNodes.size()>=N || (int)lNodes.size()==prevSize)
                        bFinish = true;

                }
            }
        }

        // Retain the best point in each node
        // Step 7 保留每个区域响应值最大的一个兴趣点
        vector<cv::KeyPoint> vResultKeys;
        vResultKeys.reserve(nfeatures);
        for(list<ExtractorNode>::iterator lit=lNodes.begin(); lit!=lNodes.end(); lit++)
        {
            vector<cv::KeyPoint> &vNodeKeys = lit->vKeys; // 得到这个节点区域中的特征点容器句柄
            cv::KeyPoint* pKP = &vNodeKeys[0]; // 得到指向第一个特征点的指针，后面作为最大响应值对应的关键点
            float maxResponse = pKP->response; //用第1个关键点响应值初始化最大响应值

            // 开始遍历这个节点区域中的特征点容器中的特征点，注意是从1开始哟，0已经用过了
            for(size_t k=1;k<vNodeKeys.size();k++)
            {
                if(vNodeKeys[k].response>maxResponse)//更新最大响应值
                {
                    pKP = &vNodeKeys[k];
                    maxResponse = vNodeKeys[k].response;
                }
            }

            vResultKeys.push_back(*pKP);
        }
        // 返回最终结果容器，其中保存有分裂出来的区域中，我们最感兴趣、响应值最大的特征点
        return vResultKeys;
    }

    // finish
    // 在operator()中调用
    void ORBextractor::ComputeKeyPointsOctTree(vector<vector<KeyPoint> >& allKeypoints)
    {
        // 重新调整图像层数 关键帧
        allKeypoints.resize(nlevels);

        // 图像cell的尺寸，是个正方形，可以理解为边长in像素坐标
        const float W = 35;

        // 遍历每层金字塔，提取Fast特征到vToDistributeKeys
        for (int level = 0; level < nlevels; ++level)
        {
            // 计算这层图像的坐标边界值，这里的 3 是因为在计算FAST特征点的时候，需要建立一个半径为3的圆
            const int minBorderX = EDGE_THRESHOLD-3;
            const int minBorderY = minBorderX;
            const int maxBorderX = mvImagePyramid[level].cols-EDGE_THRESHOLD+3;
            const int maxBorderY = mvImagePyramid[level].rows-EDGE_THRESHOLD+3;

            // 预分配的空间，一般地都是过量采集，所以这里大小是nfeatures*10
            vector<cv::KeyPoint> vToDistributeKeys;
            vToDistributeKeys.reserve(nfeatures*10);

            // 计算进行特征点提取的图像区域尺寸： 宽+高
            const float width = (maxBorderX-minBorderX);
            const float height = (maxBorderY-minBorderY);

            // 每个区域35像素，计算 特征区域(网格)的行数和列数
            const int nCols = width/W;
            const int nRows = height/W;
            const int wCell = ceil(width/nCols);
            const int hCell = ceil(height/nRows);

            // 遍历特征区域(网格)，先遍历行
            for(int i=0; i<nRows; i++)
            {
                // 计算当前网格初始行 和 最大行坐标
                const float iniY =minBorderY+i*hCell;
                // 注意：这里的+6=+3+3，即考虑到了多出来以便进行FAST特征点提取用的3像素边界
                float maxY = iniY+hCell+6;

                // 如果初始的行坐标就已经超过了有效的图像边界了，就跳过这一行
                if(iniY>=maxBorderY-3)
                    continue;
                //如果图像的大小导致不能够正好划分出来整齐的图像网格，那么就要委屈最后一行了
                if(maxY>maxBorderY)
                    maxY = maxBorderY;

                // 遍历 当前网格(特征区域)的列
                for(int j=0; j<nCols; j++)
                {
                    // 得到特征区域的最小列数和最需大列数，需与 bord相关联
                    const float iniX =minBorderX+j*wCell;
                    float maxX = iniX+wCell+6;
                    if(iniX>=maxBorderX-6)
                        continue;
                    if(maxX>maxBorderX)
                        maxX = maxBorderX;

                    vector<cv::KeyPoint> vKeysCell;

                    FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                         vKeysCell,iniThFAST,true);

                    /*if(bRight && j <= 13){
                        FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                             vKeysCell,10,true);
                    }
                    else if(!bRight && j >= 16){
                        FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                             vKeysCell,10,true);
                    }
                    else{
                        FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                             vKeysCell,iniThFAST,true);
                    }*/

                    // 若未提取出fast特征时，按最小阈值 minThFAST 提取特征值
                    if(vKeysCell.empty())
                    {
                        FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                             vKeysCell,minThFAST,true);
                        /*if(bRight && j <= 13){
                            FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                                 vKeysCell,5,true);
                        }
                        else if(!bRight && j >= 16){
                            FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                                 vKeysCell,5,true);
                        }
                        else{
                            FAST(mvImagePyramid[level].rowRange(iniY,maxY).colRange(iniX,maxX),
                                 vKeysCell,minThFAST,true);
                        }*/
                    }

                    // 若特征值不为空时，将fast特征放入vToDistributeKeys容器 (刚刚reserve的vector）
                    if(!vKeysCell.empty())
                    {
                        // 这些角点的坐标都是基于图像cell的，现在我们要先将其恢复到当前的【坐标边界】下的坐标
                        // 这样做是因为在下面使用八叉树法整理特征点的时候将会使用得到这个坐标
                        for(vector<cv::KeyPoint>::iterator vit=vKeysCell.begin(); vit!=vKeysCell.end();vit++)
                        {
                            (*vit).pt.x+=j*wCell;
                            (*vit).pt.y+=i*hCell;
                            vToDistributeKeys.push_back(*vit);
                        }
                    }

                }
            }

            //声明一个对当前图层的特征点的容器的引用,并且调整其大小为欲提取出来的特征点个数
            vector<KeyPoint> & keypoints = allKeypoints[level];
            keypoints.reserve(nfeatures);

            // 根据mnFeaturesPerLevel,即该层的兴趣点数,对特征点进行剔除,返回值是一个保存有特征点的vector容器，含有剔除后的保留下来的特征点,
            // 得到的特征点的坐标，vToDistributeKeys：当前图层提取出来的特征点，也即是等待剔除的特征点

            keypoints = DistributeOctTree(vToDistributeKeys, minBorderX, maxBorderX,
                                          minBorderY, maxBorderY,mnFeaturesPerLevel[level], level);

            // 计算当前层的尺寸，PATCH_SIZE是对于底层的初始图像来说的，现在要根据当前图层的尺度缩放倍数进行缩放得到缩放后的PATCH大小 和特征点的方向计算有关
            const int scaledPatchSize = PATCH_SIZE*mvScaleFactor[level];

            // Add border to coordinates and scale information
            // 获取剔除过程后保留下来的特征点数目,	然后开始遍历这些特征点，恢复其在当前图层图像坐标系下的坐标
            const int nkps = keypoints.size();
            for(int i=0; i<nkps ; i++)
            {
                //对每一个保留下来的特征点，恢复到相对于当前图层“边缘扩充图像下”的坐标系的坐标
                keypoints[i].pt.x+=minBorderX;
                keypoints[i].pt.y+=minBorderY;
                keypoints[i].octave=level;
                // 记录计算方向的patch，缩放后对应的大小， 又被称作为特征点半径
                keypoints[i].size = scaledPatchSize;
            }
        }

        // compute orientations
        // 然后计算这些特征点的方向信息，注意这里还是分层计算的
        for (int level = 0; level < nlevels; ++level)
            computeOrientation(mvImagePyramid[level], allKeypoints[level], umax);
    }

    // pass
    void ORBextractor::ComputeKeyPointsOld(std::vector<std::vector<KeyPoint> > &allKeypoints)
    {
        allKeypoints.resize(nlevels);

        float imageRatio = (float)mvImagePyramid[0].cols/mvImagePyramid[0].rows;

        for (int level = 0; level < nlevels; ++level)
        {
            const int nDesiredFeatures = mnFeaturesPerLevel[level];

            const int levelCols = sqrt((float)nDesiredFeatures/(5*imageRatio));
            const int levelRows = imageRatio*levelCols;

            const int minBorderX = EDGE_THRESHOLD;
            const int minBorderY = minBorderX;
            const int maxBorderX = mvImagePyramid[level].cols-EDGE_THRESHOLD;
            const int maxBorderY = mvImagePyramid[level].rows-EDGE_THRESHOLD;

            const int W = maxBorderX - minBorderX;
            const int H = maxBorderY - minBorderY;
            const int cellW = ceil((float)W/levelCols);
            const int cellH = ceil((float)H/levelRows);

            const int nCells = levelRows*levelCols;
            const int nfeaturesCell = ceil((float)nDesiredFeatures/nCells);

            vector<vector<vector<KeyPoint> > > cellKeyPoints(levelRows, vector<vector<KeyPoint> >(levelCols));

            vector<vector<int> > nToRetain(levelRows,vector<int>(levelCols,0));
            vector<vector<int> > nTotal(levelRows,vector<int>(levelCols,0));
            vector<vector<bool> > bNoMore(levelRows,vector<bool>(levelCols,false));
            vector<int> iniXCol(levelCols);
            vector<int> iniYRow(levelRows);
            int nNoMore = 0;
            int nToDistribute = 0;


            float hY = cellH + 6;

            for(int i=0; i<levelRows; i++)
            {
                const float iniY = minBorderY + i*cellH - 3;
                iniYRow[i] = iniY;

                if(i == levelRows-1)
                {
                    hY = maxBorderY+3-iniY;
                    if(hY<=0)
                        continue;
                }

                float hX = cellW + 6;

                for(int j=0; j<levelCols; j++)
                {
                    float iniX;

                    if(i==0)
                    {
                        iniX = minBorderX + j*cellW - 3;
                        iniXCol[j] = iniX;
                    }
                    else
                    {
                        iniX = iniXCol[j];
                    }


                    if(j == levelCols-1)
                    {
                        hX = maxBorderX+3-iniX;
                        if(hX<=0)
                            continue;
                    }


                    Mat cellImage = mvImagePyramid[level].rowRange(iniY,iniY+hY).colRange(iniX,iniX+hX);

                    cellKeyPoints[i][j].reserve(nfeaturesCell*5);

                    FAST(cellImage,cellKeyPoints[i][j],iniThFAST,true);

                    if(cellKeyPoints[i][j].size()<=3)
                    {
                        cellKeyPoints[i][j].clear();

                        FAST(cellImage,cellKeyPoints[i][j],minThFAST,true);
                    }


                    const int nKeys = cellKeyPoints[i][j].size();
                    nTotal[i][j] = nKeys;

                    if(nKeys>nfeaturesCell)
                    {
                        nToRetain[i][j] = nfeaturesCell;
                        bNoMore[i][j] = false;
                    }
                    else
                    {
                        nToRetain[i][j] = nKeys;
                        nToDistribute += nfeaturesCell-nKeys;
                        bNoMore[i][j] = true;
                        nNoMore++;
                    }

                }
            }


            // Retain by score

            while(nToDistribute>0 && nNoMore<nCells)
            {
                int nNewFeaturesCell = nfeaturesCell + ceil((float)nToDistribute/(nCells-nNoMore));
                nToDistribute = 0;

                for(int i=0; i<levelRows; i++)
                {
                    for(int j=0; j<levelCols; j++)
                    {
                        if(!bNoMore[i][j])
                        {
                            if(nTotal[i][j]>nNewFeaturesCell)
                            {
                                nToRetain[i][j] = nNewFeaturesCell;
                                bNoMore[i][j] = false;
                            }
                            else
                            {
                                nToRetain[i][j] = nTotal[i][j];
                                nToDistribute += nNewFeaturesCell-nTotal[i][j];
                                bNoMore[i][j] = true;
                                nNoMore++;
                            }
                        }
                    }
                }
            }

            vector<KeyPoint> & keypoints = allKeypoints[level];
            keypoints.reserve(nDesiredFeatures*2);

            const int scaledPatchSize = PATCH_SIZE*mvScaleFactor[level];

            // Retain by score and transform coordinates
            for(int i=0; i<levelRows; i++)
            {
                for(int j=0; j<levelCols; j++)
                {
                    vector<KeyPoint> &keysCell = cellKeyPoints[i][j];
                    KeyPointsFilter::retainBest(keysCell,nToRetain[i][j]);
                    if((int)keysCell.size()>nToRetain[i][j])
                        keysCell.resize(nToRetain[i][j]);


                    for(size_t k=0, kend=keysCell.size(); k<kend; k++)
                    {
                        keysCell[k].pt.x+=iniXCol[j];
                        keysCell[k].pt.y+=iniYRow[i];
                        keysCell[k].octave=level;
                        keysCell[k].size = scaledPatchSize;
                        keypoints.push_back(keysCell[k]);
                    }
                }
            }

            if((int)keypoints.size()>nDesiredFeatures)
            {
                KeyPointsFilter::retainBest(keypoints,nDesiredFeatures);
                keypoints.resize(nDesiredFeatures);
            }
        }

        // and compute orientations
        for (int level = 0; level < nlevels; ++level)
            computeOrientation(mvImagePyramid[level], allKeypoints[level], umax);
    }

    // 在operator()中调用
    static void computeDescriptors(const Mat& image, vector<KeyPoint>& keypoints, Mat& descriptors,
                                   const vector<Point>& pattern)
    {
        descriptors = Mat::zeros((int)keypoints.size(), 32, CV_8UC1);

        for (size_t i = 0; i < keypoints.size(); i++)
            computeOrbDescriptor(keypoints[i], image, &pattern[0], descriptors.ptr((int)i));
    }

    // ★★★ finish
    int ORBextractor::operator()( InputArray _image, InputArray _mask, vector<KeyPoint>& _keypoints,
                                  OutputArray _descriptors, std::vector<int> &vLappingArea)
    {
        /* InputArray() 是一个接口类， 可以传入多种类型，例如Mat, Mat_<T>, Mat_<T, m,n>, vector<vector<T>>, vector<Mat>等；
         * OutputArray是InputArray的派生类。使用时需要注意的问题和InputArray一样。
         * 和InputArray不同的是，需要注意在使用_OutputArray：：getMat（）之前一定要调用_OutputArray：：create（）为矩阵分配空间。
         * 使用_InputArray::getMat() 函数为该参数构建一个矩阵的头指针， 这样相当于直接对传输参数的内存区域进行了操作
         *
         */

        // cout << "[ORBextractor]: Max Features: " << nfeatures << endl;
        // 图像为空判断
        if(_image.empty())
            return -1;

        Mat image = _image.getMat();
        assert(image.type() == CV_8UC1);


        // Pre-compute the scale pyramid
        ComputePyramid(image);

        vector < vector<KeyPoint> > allKeypoints;
        ComputeKeyPointsOctTree(allKeypoints);
        //ComputeKeyPointsOld(allKeypoints);

        Mat descriptors;

        //计算总共的keypoints个数
        int nkeypoints = 0;
        for (int level = 0; level < nlevels; ++level)
            nkeypoints += (int)allKeypoints[level].size();
        if( nkeypoints == 0 )
            _descriptors.release();
        else
        {
            // 为每个关键帧描述子分配32位内存
            _descriptors.create(nkeypoints, 32, CV_8U);
            descriptors = _descriptors.getMat();
        }

        //_keypoints.clear();
        //_keypoints.reserve(nkeypoints);
        _keypoints = vector<cv::KeyPoint>(nkeypoints);

        int offset = 0;
        //Modified for speeding up stereo fisheye matching
        // 为加快立体鱼眼匹配而修改   遍历 图像金字塔
        int monoIndex = 0, stereoIndex = nkeypoints-1;
        for (int level = 0; level < nlevels; ++level)
        {

            vector<KeyPoint>& keypoints = allKeypoints[level];
            int nkeypointsLevel = (int)keypoints.size();
            // 取出该层所有特征点，若特征点个数为0，则continue
            if(nkeypointsLevel==0)
                continue;

            // preprocess the resized image
            // 对图像进行高斯模糊，用BORDER_REFLECT_101方法处理边缘
            Mat workingMat = mvImagePyramid[level].clone();
            GaussianBlur(workingMat, workingMat, Size(7, 7), 2, 2, BORDER_REFLECT_101);

            // Compute the descriptors
            //Mat desc = descriptors.rowRange(offset, offset + nkeypointsLevel);
            // 计算描述子,offset是偏移量，此处取出的是该层的描述子
            Mat desc = cv::Mat(nkeypointsLevel, 32, CV_8U);
            computeDescriptors(workingMat, keypoints, desc, pattern);

            offset += nkeypointsLevel;

            // 再进一步遍历当前图层上的每一个特征点，根据对应的缩放系数缩放它的像素坐标，再根据它与LappingArea的关系，
            // 确定当前特征点描述子在总描述子容器中的位置，如果是keypoint->pt.x >= vLappingArea[0] &&
            // keypoint->pt.x <= vLappingArea[1]，那么它的描述子放在后面，否则放在前面。

            float scale = mvScaleFactor[level]; //getScale(level, firstLevel, scaleFactor);
            int i = 0;
            for (vector<KeyPoint>::iterator keypoint = keypoints.begin(),
                         keypointEnd = keypoints.end(); keypoint != keypointEnd; ++keypoint){

                // Scale keypoint coordinates
                if (level != 0){
                    keypoint->pt *= scale;// 按缩放因子进行坐标缩放
                }

                if(keypoint->pt.x >= vLappingArea[0] && keypoint->pt.x <= vLappingArea[1]){
                    _keypoints.at(stereoIndex) = (*keypoint);
                    desc.row(i).copyTo(descriptors.row(stereoIndex));
                    stereoIndex--;
                }
                else{
                    _keypoints.at(monoIndex) = (*keypoint);
                    desc.row(i).copyTo(descriptors.row(monoIndex));
                    monoIndex++;
                }
                i++;
            }
        }
        //cout << "[ORBextractor]: extracted " << _keypoints.size() << " KeyPoints" << endl;
        return monoIndex;
    }

    // 输入：image
    // 改变：std::vector<cv::Mat> mvImagePyramid;
    // finish
    // 在operator()中调用
    void ORBextractor::ComputePyramid(cv::Mat image)
    {
        // 计算图像金字塔，按一定的尺度比例获取nlevels张分辨率不同的图像
        for (int level = 0; level < nlevels; ++level)
        {
            float scale = mvInvScaleFactor[level];
            // 基于该尺度变换的后图像的尺寸
            Size sz(cvRound((float)image.cols*scale), cvRound((float)image.rows*scale));

            // 相当于给原图像加边，在计算关键点时便不需要检测所加边缘，又能保证原图像边缘的关键点也能检测
            Size wholeSize(sz.width + EDGE_THRESHOLD*2, sz.height + EDGE_THRESHOLD*2);

            //temp通过Mat类的一种构造函数获得了尺寸及图像的类型
            Mat temp(wholeSize, image.type()), masktemp;

            //mvImagePyramid[level]通过 赋值与 temp 相关联
            mvImagePyramid[level] = temp(Rect(EDGE_THRESHOLD, EDGE_THRESHOLD, sz.width, sz.height));

            // Compute the resized image
            if( level != 0 )
            {
                //把上一层图片缩放成 sz ： sz 为基于该尺度变换的后图像的尺寸
                resize(mvImagePyramid[level-1], mvImagePyramid[level], sz, 0, 0, INTER_LINEAR);

                // 在图像周围以对称的方式加一个宽度为EDGE_THRESHOLD的边，便于后面的计算
                // 在后面计算特征点时的时候省去边缘的判断；
                // 可以省去加边操作，然后在后面计算的时候做原图像的边缘判断即可
                copyMakeBorder(mvImagePyramid[level], temp, EDGE_THRESHOLD, EDGE_THRESHOLD, EDGE_THRESHOLD, EDGE_THRESHOLD,
                               BORDER_REFLECT_101+BORDER_ISOLATED);
            }
            else
            {
                //第一张为原图分辨率，无需缩放，进行了加边操作
                copyMakeBorder(image, temp, EDGE_THRESHOLD, EDGE_THRESHOLD, EDGE_THRESHOLD, EDGE_THRESHOLD,
                               BORDER_REFLECT_101);
            }
        }

    }

} //namespace ORB_SLAM