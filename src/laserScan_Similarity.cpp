#include "ros/ros.h"
#include "ros/console.h"

#include "pointmatcher/PointMatcher.h"
#include "pointmatcher/Timer.h"
#include "pointmatcher_ros/point_cloud.h"
#include "pointmatcher_ros/get_params_from_server.h"

#include <dirent.h>
#include <fstream>
#include <vector>

using namespace std;
using namespace PointMatcherSupport;

class scan
{
    typedef PointMatcher<float> PM;
    typedef PM::DataPoints DP;
public:
    PM::DataPoints *pointCloud;
    int pointCount;
    float minRange;
    float maxRange;
    float sectionLength;
    vector<int> sectionCountVector;
    vector<float> sectionRatioVector;
    vector<float> rangeOfPointVector;
    void clear();
private:

protected:

};

void scan::clear()
{
    delete this->pointCloud;
    pointCount = 0;
    minRange = 9999;
    maxRange = 0;
    sectionCountVector.clear();
    sectionRatioVector.clear();
    rangeOfPointVector.clear();
}

class Similarity
{
    typedef PointMatcher<float> PM;
    typedef PM::DataPoints DP;
public:
    Similarity(ros::NodeHandle &n);
    ~Similarity();

    void getEMD1D();
    void forScan0();
    void forScan1(string fileName);
    float calculateRange(Eigen::Vector3f inputOrdinate);
    int judgeBucket(float range, float minRange, float sectionLength);

private:
    string scan0FileName;
    string scanDirName;
    string scan1FileName;

    scan scan0;
    scan scan1;

    ros::NodeHandle& n;
    const int sectionNum;
    const bool isLog;
    const int truncatedStart;

    float EMD;
    vector<float> EMDVector;
protected:

};

Similarity::Similarity(ros::NodeHandle& n):
    n(n),
    scanDirName(getParam<string>("scan_vtk", ".")),
    sectionNum(getParam<int>("sectionNum", 100)),
    isLog(getParam<bool>("isLog", false)),
    truncatedStart(getParam<int>("truncatedStart", 0))
{

    //Holy Shit!
    DIR *dir;
    struct dirent *ptr;
    vector<double> fileNameVector;

    if ((dir = opendir(scanDirName.c_str())) == NULL)
    {
        cout<<"OPEN DIR ERROR"<<endl;
        return;
    }

    while((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        scan1FileName = ptr->d_name;
        fileNameVector.push_back(atof(scan1FileName.substr(0, scan1FileName.length()-4).c_str()));
    }

    //Sort the fileName!
    sort(fileNameVector.begin(), fileNameVector.end());

    stringstream ss;
    ss<<std::fixed<<fileNameVector.at(0);
    scan0FileName = scanDirName + ss.str() + ".vtk";
    this->forScan0();

    ///JUST FPR TESTING TWO SCANS i=1
    for(int i = 0; i < fileNameVector.size(); i++)
    {
        cout<<"--------------------------------------"<<endl;
        stringstream ss;
        ss<<std::fixed<<fileNameVector.at(i);
        scan1FileName = scanDirName + ss.str() + ".vtk";

        this->forScan1(scan1FileName);
        this->getEMD1D();
        scan1.clear();
    }

    //save in the EMD.txt
    if(0)
    {
        ofstream recordEMD;
        stringstream ssEMD;

        ssEMD << scanDirName << "EMD1D.txt";
        recordEMD.open(ssEMD.str());
        for(int i = 0; i < EMDVector.size(); i++)
        {
            recordEMD << std::fixed << EMDVector.at(i) << endl;
        }
        recordEMD.close();
    }

}

Similarity::~Similarity()
{

}

void Similarity::forScan0()
{
    //load scan0
    try
    {
        DP* cloud(new DP(DP::load(scan0FileName)));
        scan0.pointCloud = cloud;
    }
    catch(std::exception &)
    {
        ROS_ERROR_STREAM("Cannot load Scan0 from vtk file " << scan0FileName);
    }

    //Scan0 Pretreated
    scan0.pointCount = scan0.pointCloud->features.cols();
    scan0.maxRange = 0;
    scan0.minRange = 9999;
    for(int i = 0; i < scan0.pointCount; i++)
    {
        float rangeOfPoint = calculateRange(scan0.pointCloud->features.col(i).head(3));
        scan0.rangeOfPointVector.push_back(rangeOfPoint);
        if(rangeOfPoint > scan0.maxRange)
            scan0.maxRange = rangeOfPoint;
        if(rangeOfPoint < scan0.minRange)
            scan0.minRange = rangeOfPoint;
    }
    scan0.sectionLength = (scan0.maxRange - scan0.minRange) / sectionNum;
    for(int i = 0; i < sectionNum; i++)
    {
        scan0.sectionCountVector.push_back(0);
    }
    cout<<"in Scan0 maxRange:  "<<scan0.maxRange
       <<"  minRange:  "<<scan0.minRange
       <<"  count:  "<<scan0.pointCount
      <<"  sectionLength:  "<<scan0.sectionLength<<endl;

    ///Scan0
    //Count
    for(int i = 0; i < scan0.pointCount; i++)
    {
        int index = this->judgeBucket(scan0.rangeOfPointVector.at(i), scan0.minRange, scan0.sectionLength);
        scan0.sectionCountVector.at(index)++;
    }
    //Ratio
    for(int i = 0; i < sectionNum; i++)
    {
        scan0.sectionRatioVector.push_back((float)scan0.sectionCountVector.at(i) / (float)scan0.pointCount);
    }
//    //Check
//    float sumRatio = 0;
//    int sumCount = 0;
//    for(int i = 0; i < sectionNum; i++)
//    {
//        sumRatio += scan0.sectionRatioVector.at(i);
//        sumCount += scan0.sectionCountVector.at(i);
//    }
//    cout<<"sum0: "<<sumRatio<<"  "<<sumCount<<endl;
}

void Similarity::forScan1(string fileName)
{
    //load scan1
    try
    {
        DP* cloud(new DP(DP::load(fileName)));
        scan1.pointCloud = cloud;
    }
    catch(std::exception &)
    {
        ROS_ERROR_STREAM("Cannot load Scan1 from vtk file " << fileName);
    }

    //Scan1 PreTreated
    scan1.pointCount = scan1.pointCloud->features.cols();
    scan1.maxRange = 0;
    scan1.minRange = 9999;
    for(int i = 0; i < scan1.pointCount; i++)
    {
        float rangeOfPoint = calculateRange(scan1.pointCloud->features.col(i).head(3));
        scan1.rangeOfPointVector.push_back(rangeOfPoint);
        if(rangeOfPoint > scan1.maxRange)
            scan1.maxRange = rangeOfPoint;
        if(rangeOfPoint < scan1.minRange)
            scan1.minRange = rangeOfPoint;
    }
    scan1.sectionLength = (scan1.maxRange - scan1.minRange) / sectionNum;
    for(int i = 0; i < sectionNum; i++)
    {
        scan1.sectionCountVector.push_back(0);
    }
//    cout<<"in Scan1 maxRange:  "<<scan1.maxRange
//       <<"  minRange:  "<<scan1.minRange
//        <<"  count:  "<<scan1.pointCount
//       <<"  sectionLength:  "<<scan1.sectionLength<<endl;

    ///Scan1
    //Count
    for(int i = 0; i < scan1.pointCount; i++)
    {
        int index = this->judgeBucket(scan1.rangeOfPointVector.at(i), scan1.minRange, scan1.sectionLength);
        scan1.sectionCountVector.at(index)++;
    }
    //Ratio
    for(int i = 0; i < sectionNum; i++)
    {
        scan1.sectionRatioVector.push_back((float)scan1.sectionCountVector.at(i) / (float)scan1.pointCount);
    }
//    //Check
//    sumRatio = 0;
//    sumCount = 0;
//    for(int i = 0; i < sectionNum; i++)
//    {
//        sumRatio += scan1.sectionRatioVector.at(i);
//        sumCount += scan1.sectionCountVector.at(i);
//    }
//    cout<<"sum1: "<<sumRatio<<"  "<<sumCount<<endl;

    if(isLog)
    {
        cout<<"Log two vectors"<<endl;
        ofstream recordVector0, recordVector1;
        stringstream recordName0, recordName1;

        //ugly code for test
        recordName0 << "/home/yh/SCAN0BUCKET.txt";
        recordName1 << "/home/yh/SCAN1BUCKET.txt";

        recordVector0.open(recordName0.str());
        recordVector1.open(recordName1.str());

        for(int i = 0; i < sectionNum; i++)
        {
            recordVector0 <<std::fixed<< scan0.sectionRatioVector.at(i) <<endl;
            recordVector1 <<std::fixed<< scan1.sectionRatioVector.at(i) <<endl;
        }

        recordVector0.close();
        recordVector1.close();
    }

}

void Similarity::getEMD1D()
{
    EMD = 0;
    for(int i = this->truncatedStart; i < sectionNum; i++)
    {
        for(int j = 0; j <= i; j++)
        {
            EMD += abs(scan0.sectionRatioVector.at(j) - scan1.sectionRatioVector.at(j));
        }
    }
    EMD /= sectionNum;
    EMDVector.push_back(EMD);
    cout<<"EMD Distance:  "<<EMD<<endl;
}

float Similarity::calculateRange(Eigen::Vector3f inputOrdinate)
{
    return pow(pow(inputOrdinate(0), 2) + pow(inputOrdinate(1), 2) + pow(inputOrdinate(2), 2), 0.5);
}

int Similarity::judgeBucket(float range, float minRange, float sectionLength)
{
    for(int i = 0; i < sectionNum; i++)
    {
        if(range >= minRange + i*sectionLength && range <= minRange + (i+1)*sectionLength)
            return i;
    }
    ///Problem exists
//    cout<<"Not in the Bucket!  Return the last Setcion!  Length:  "<<range<<endl;
    return sectionNum-1;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "laserScan_Similarity");
    ros::NodeHandle n;

    Similarity similarity(n);

    ros::spin();
    return 0;
}
