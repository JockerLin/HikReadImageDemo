/***************************************************************************************************
* 
* Notes about how to configure your OpenCV environment and project.
* 1. You can prepare the required installation package from the official website. https://opencv.org/releases.html
* 2. If the *.lib files doesn't exist in the package download, you need to compile by yourself with the CMake tool.
* 3. Add the 'bin' folder path to the PATH.
* 4. Configure the 'Additional Include Directories', 'Additional Library Directories' and 'Additional Dependencies' for current project property.
* 
* If there is any question or request, please feel free to contact us.

***************************************************************************************************/

#include "MvCameraControl.h"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <conio.h>
#include "string.h"
#include <iostream>

unsigned int g_nPayloadSize = 0;

enum CONVERT_TYPE
{
    OpenCV_Mat        = 0,    // Most of the time, we use 'Mat' format to store image data after OpenCV V2.1
    OpenCV_IplImage       = 1,   //we may also use 'IplImage' format to store image data, usually before OpenCV V2.1
};


// Wait for key press
void WaitForKeyPress(void)
{
    while(!_kbhit())
    {
        Sleep(10);
    }
    _getch();
}


// print the discovered devices information to user
bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\n");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

        // print current ip and user defined name
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("UserDefinedName: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
        printf("Serial Number: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
        printf("Device Number: %d\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.nDeviceNumber);
    }
    else
    {
        printf("Not support.\n");
    }

    return true;
}


int RGB2BGR( unsigned char* pRgbData, unsigned int nWidth, unsigned int nHeight )
{
    if ( NULL == pRgbData )
    {
        return MV_E_PARAMETER;
    }

    for (unsigned int j = 0; j < nHeight; j++)
    {
        for (unsigned int i = 0; i < nWidth; i++)
        {
            unsigned char red = pRgbData[j * (nWidth * 3) + i * 3];
            pRgbData[j * (nWidth * 3) + i * 3]     = pRgbData[j * (nWidth * 3) + i * 3 + 2];
            pRgbData[j * (nWidth * 3) + i * 3 + 2] = red;
        }
    }

    return MV_OK;
}


// convert data stream in Mat format
bool Convert2Mat(MV_FRAME_OUT_INFO_EX* pstImageInfo, unsigned char * pData, cv::Mat &targetMat)
{
    cv::Mat srcImage;
    if ( pstImageInfo->enPixelType == PixelType_Gvsp_Mono8 )
    {
        srcImage = cv::Mat(pstImageInfo->nHeight, pstImageInfo->nWidth, CV_8UC1, pData);
    }
    else if ( pstImageInfo->enPixelType == PixelType_Gvsp_RGB8_Packed )
    {
        RGB2BGR(pData, pstImageInfo->nWidth, pstImageInfo->nHeight);
        srcImage = cv::Mat(pstImageInfo->nHeight, pstImageInfo->nWidth, CV_8UC3, pData);
    }
    else
    {
        printf("unsupported pixel format\n");
        return false;
    }

    if ( NULL == srcImage.data )
    {
        return false;
    }


    //save converted image in a local file
	bool save_status = false;
	if (save_status) {
		try {
#if defined (VC9_COMPILE)
			cvSaveImage("MatImage.bmp", &(IplImage(srcImage)));
#else
			cv::imwrite("MatImage.bmp", srcImage);
#endif
		}
		catch (cv::Exception& ex) {
			fprintf(stderr, "Exception saving image to bmp format: %s\n", ex.what());
		}
	}

	srcImage.copyTo(targetMat);
    srcImage.release();

    return true;
}


// convert data stream in Ipl format
bool Convert2Ipl(MV_FRAME_OUT_INFO_EX* pstImageInfo, unsigned char * pData)
{
    IplImage* srcImage = NULL;
    if ( pstImageInfo->enPixelType == PixelType_Gvsp_Mono8 )
    {
        srcImage = cvCreateImage(cvSize(pstImageInfo->nWidth, pstImageInfo->nHeight), IPL_DEPTH_8U, 1);
    }
    else if ( pstImageInfo->enPixelType == PixelType_Gvsp_RGB8_Packed )
    {
        RGB2BGR(pData, pstImageInfo->nWidth, pstImageInfo->nHeight);
        srcImage = cvCreateImage(cvSize(pstImageInfo->nWidth, pstImageInfo->nHeight), IPL_DEPTH_8U, 3);
    }
    else
    {
        printf("unsupported pixel format\n");
        return false;
    }
    if ( NULL == srcImage )
    {
        printf("CreatImage failed.\n");
        return false;
    }

    srcImage->imageData = (char *)pData;

    // save converted image in a local file
    try {
        cvSaveImage("IplImage.bmp", srcImage);
    }
    catch (cv::Exception& ex) {
        fprintf(stderr, "Exception saving image to bmp format: %s\n", ex.what());
    }

    cvReleaseImage(&srcImage);
    return true;
}


int main()
{
    int nRet = MV_OK;
    void* handle = NULL;

    do
    {
        // 枚举相机列表
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("Enum Devices fail! nRet [0x%x]\n", nRet);
            break;
        }

        if (stDeviceList.nDeviceNum > 0)
        {
            for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                printf("[device %d]:\n", i);
                // 获取设备信息
				MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
                if (NULL == pDeviceInfo)
                {
                    break;
                }
                PrintDeviceInfo(pDeviceInfo);
            }
        }
        else
        {
            printf("Find No Devices!\n");
            break;
        }

        // input the format to convert
        printf("[0] OpenCV_Mat\n");
        printf("[1] OpenCV_IplImage\n");
        printf("Please Input Format to convert:");
        unsigned int nFormat = 0;
        //scanf("%d", &nFormat);
        if (nFormat >= 2)
        {
            printf("Input error!\n");
            return 0;
        }

        // select device to connect
        printf("Please Input camera index(0-%d):", stDeviceList.nDeviceNum-1);
        unsigned int nIndex = 0;
        //scanf("%d", &nIndex);
        if (nIndex >= stDeviceList.nDeviceNum)
        {
            printf("Input error!\n");
            break;
        }

        // Select device and create handle
		// 创建相机句柄
        nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_OK != nRet)
        {
            printf("Create Handle fail! nRet [0x%x]\n", nRet);
            break;
        }

        // open device
		// 打开设备 多次打开会失败
        nRet = MV_CC_OpenDevice(handle);
        if (MV_OK != nRet)
        {
            printf("Open Device fail! nRet [0x%x]\n", nRet);
            //break;
        }

		nRet = MV_CC_SetGamma(handle, 0.15);
		if (MV_OK != nRet)
		{
			printf("Set Gamma Failed! nRet [0x%x]\n", nRet);
			break;
		}

        // Detection network optimal package size(It only works for the GigE camera)
		// 检测网络最佳封装尺寸(只适用于GigE摄像机)
        if (stDeviceList.pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
            if (nPacketSize > 0)
            {
                nRet = MV_CC_SetIntValue(handle,"GevSCPSPacketSize",nPacketSize);
                if(nRet != MV_OK)
                {
                    printf("Warning: Set Packet Size fail nRet [0x%x]!", nRet);
                }
            }
            else
            {
                printf("Warning: Get Packet Size fail nRet [0x%x]!", nPacketSize);
            }
        }

        // Set trigger mode as off
		// 触发模式
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 0);
        if (MV_OK != nRet)
        {
            printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
            break;
        }
		
		// 软触发则开启 其他触发则对应设置即可
		/*nRet = MV_CC_SetEnumValue(handle, "TriggerMode", MV_TRIGGER_MODE_ON);
		nRet = MV_CC_SetEnumValue(handle, "TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);*/

		// 设置行触发使能以及数值
		/*MV_CC_SetBoolValue(handle, "AcquisitionLineRateEnable", true);
		MV_CC_SetIntValue(handle, "AcquisitionLineRate", 12345);*/
		// SetBoolValue("AcquisitionLineRateEnable", param), "AcquisitionLineRateEnable")

		// 设置增益
		MV_XML_AccessMode access22;
		memset(&access22, 0, sizeof(MV_XML_AccessMode));
		auto access23 = MV_XML_GetNodeAccessMode(handle, "Gain", &access22);
		if (access23 != AM_RW) {
			printf("no allow write");
		}
		MVCC_ENUMVALUE stParam2;
		memset(&stParam2, 0, sizeof(MVCC_ENUMVALUE));
		if (MV_CC_GetEnumValue(handle, "PreampGain", &stParam2) != MV_OK)
		{
			printf("获取PreampGain失败");
		}

        // Get payload size
        MVCC_INTVALUE stParam;
        memset(&stParam, 0, sizeof(MVCC_INTVALUE));
        nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
        if (MV_OK != nRet)
        {
            printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
            break;
        }
        g_nPayloadSize = stParam.nCurValue;

        MV_FRAME_OUT_INFO_EX stImageInfo = {0};
        memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
        unsigned char * pData = (unsigned char *)malloc(sizeof(unsigned char) * (g_nPayloadSize));
        if (pData == NULL)
        {
            printf("Allocate memory failed.\n");
            break;
        }

		// 多次采集开始
		// Start grab image
		nRet = MV_CC_StartGrabbing(handle);
		if (MV_OK != nRet)
		{
			printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
			break;
		}

		cv::namedWindow("realtime image", cv::WINDOW_NORMAL);
		
		while (1) {
			// get one frame from camera with timeout=1000ms
			// 读取每帧图像

			// 软触发一次 读帧一次
			// nRet = MV_CC_SetCommandValue(handle, "TriggerSoftware");

			nRet = MV_CC_GetOneFrameTimeout(handle, pData, g_nPayloadSize, &stImageInfo, 1000);
			if (nRet == MV_OK)
			{
				// 为什么是 4096 * 480 ?
				printf("Get One Frame: Width[%d], Height[%d], nFrameNum[%d]\n",
					stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum);
			}
			else
			{
				printf("No data[0x%x]\n", nRet);
				free(pData);
				pData = NULL;
				break;
			}

			// 数据转换
			bool bConvertRet = false;
			cv::Mat cv_image;
			if (0 == nFormat)
			{
				bConvertRet = Convert2Mat(&stImageInfo, pData, cv_image);
			}
			else
			{
				bConvertRet = Convert2Ipl(&stImageInfo, pData);
			}

			cv::rotate(cv_image, cv_image, cv::ROTATE_180);
			/*cv::Mat cv_image;
			cv_image.data = pData;*/

			cv::imshow("realtime image", cv_image);
			char key = cv::waitKey(1);
			std::cout << "key:" << key << std::endl;
			if (key == 'q') {
				break;
			}
		}
		// print result
		/*if (bConvertRet)
		{
			printf("OpenCV format convert finished.\n");
			free(pData);
			pData = NULL;
		}
		else*/
		{
			//printf("OpenCV format convert failed.\n");
			free(pData);
			pData = NULL;
			//break;
		}

        // Stop grab image
		// 停止采集
        nRet = MV_CC_StopGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
            break;
        }

        // Close device
        nRet = MV_CC_CloseDevice(handle);
        if (MV_OK != nRet)
        {
            printf("ClosDevice fail! nRet [0x%x]\n", nRet);
            break;
        }

        // Destroy handle
        nRet = MV_CC_DestroyHandle(handle);
        if (MV_OK != nRet)
        {
			// llngsa 
            printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
            break;
        }
    } while (0);


    if (nRet != MV_OK)
    {
        if (handle != NULL)
        {
            MV_CC_DestroyHandle(handle);
            handle = NULL;
        }
    }

    printf("Press a key to exit.\n");
    //WaitForKeyPress();

    return 0;
}

/*
依赖cut
opencv_calib3d249d.lib
opencv_contrib249d.lib
opencv_core249d.lib
opencv_features2d249d.lib
opencv_flann249d.lib
opencv_gpu249d.lib
opencv_highgui249d.lib
opencv_imgproc249d.lib
opencv_legacy249d.lib
opencv_ml249d.lib
opencv_nonfree249d.lib
opencv_objdetect249d.lib
opencv_ocl249d.lib
opencv_photo249d.lib
opencv_stitching249d.lib
opencv_superres249d.lib
opencv_ts249d.lib
opencv_video249d.lib
opencv_videostab249d.lib
*/