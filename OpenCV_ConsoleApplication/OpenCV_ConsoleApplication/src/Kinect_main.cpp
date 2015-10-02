#include <iostream>
#include "windows.h"
#include "NuiApi.h"

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"

using namespace std;
using namespace cv;

//----�����ں��¼��;��-----------------------------------------------------------------  
HANDLE       m_hNextVideoFrameEvent;
HANDLE       m_hNextDepthFrameEvent;
HANDLE       m_hNextSkeletonEvent;
HANDLE       m_pVideoStreamHandle;
HANDLE       m_pDepthStreamHandle;
HANDLE       m_hEvNuiProcessStop;//���ڽ������¼�����; 

//---ͼ���С�Ȳ���--------------------------------------------  
#define COLOR_WIDTH					640  
#define COLOR_HIGHT					480  
#define DEPTH_WIDTH					640  
#define DEPTH_HIGHT					480  
#define SKELETON_WIDTH				640  
#define SKELETON_HIGHT				480  
#define CHANNEL                     3  
#define WIDTH						80
#define HEIGHT						40


//---Image stream �ֱ���-------------------------------
NUI_IMAGE_RESOLUTION colorResolution = NUI_IMAGE_RESOLUTION_640x480;	//����ͼ��ֱ���
NUI_IMAGE_RESOLUTION depthResolution = NUI_IMAGE_RESOLUTION_640x480;

//---��ʾͼ��------------------------------------------;
Mat depthRGB(DEPTH_HIGHT, DEPTH_WIDTH, CV_8UC3, Scalar(0, 0, 0));
Mat depthContinue(DEPTH_HIGHT, DEPTH_WIDTH, CV_16UC1, Scalar(0, 0, 0));

// windowsʱ��
SYSTEMTIME sys;

char g_file_name_color_image[100];
char g_file_name_depth_image[100];

//---��ɫ------------------------------
const Scalar SKELETON_COLORS[NUI_SKELETON_COUNT] =
{
	Scalar(0, 0, 255),      // Blue
	Scalar(0, 255, 0),      // Green
	Scalar(255, 255, 64),   // Yellow
	Scalar(64, 255, 255),   // Light blue
	Scalar(255, 64, 255),   // Purple
	Scalar(255, 128, 128)   // Pink
};


//�����Ϣת��Ϊ��ɫ��Ϣ����ʾ������������;
HRESULT DepthToRGB(USHORT depth, uchar& red, uchar& green, uchar& blue)
{
	SHORT realDepth = NuiDepthPixelToDepth(depth);
	USHORT playerIndex = NuiDepthPixelToPlayerIndex(depth);//������ͼ�õ�����λ�ö�Ӧ�� UserID;
	// Convert depth info into an intensity for display
	BYTE b = 255 - static_cast<BYTE>(256 * realDepth / 0x0fff);
	switch (playerIndex)
	{
		//������û�ж���ʱ��ֻ���ƻҶ�ͼ,�Ҷ�ֵ[0,128];
	case 0:
		red = b / 2;
		green = b / 2;
		blue = b / 2;
		break;

	case 1:
		red = b;
		green = 0;
		blue = 0;
		break;

	case 2:
		red = 0;
		green = b;
		blue = 0;
		break;

	case 3:
		red = b / 4;
		green = b;
		blue = b;
		break;

	case 4:
		red = b;
		green = b;
		blue = b / 4;
		break;

	case 5:
		red = b;
		green = b / 4;
		blue = b;
		break;

	case 6:
		red = b / 2;
		green = b / 2;
		blue = b;
		break;

	case 7:
		red = 255 - b / 2;
		green = 255 - b / 2;
		blue = 255 - b / 2;
		break;

	default:
		red = 0;
		green = 0;
		blue = 0;
		break;

	}
	return S_OK;
}



//��ȡ��ɫͼ�����ݣ���������ʾ  
int DrawColor(HANDLE h)
{
	const NUI_IMAGE_FRAME * pImageFrame = NULL;
	HRESULT hr = NuiImageStreamGetNextFrame(h, 0, &pImageFrame);
	if (FAILED(hr))
	{
		cout << "GetColor Image Frame Failed" << endl;
		return-1;
	}
	INuiFrameTexture* pTexture = pImageFrame->pFrameTexture;
	NUI_LOCKED_RECT LockedRect;
	pTexture->LockRect(0, &LockedRect, NULL, 0);
	if (LockedRect.Pitch != 0)
	{
		BYTE* pBuffer = (BYTE*)LockedRect.pBits;
		//OpenCV��ʾ��ɫ��Ƶ
		if (true)
		{
			Mat temp(COLOR_HIGHT, COLOR_WIDTH, CV_8UC4, pBuffer);
			imshow("Color", temp);
			imwrite(g_file_name_color_image, temp);
		}
		int c = waitKey(1);//����ESC����  
		//�������Ƶ���水��ESC,q,Q���ᵼ�����������˳�  
		if (c == 27 || c == 'q' || c == 'Q')
		{
			SetEvent(m_hEvNuiProcessStop);
		}
	}
	NuiImageStreamReleaseFrame(h, pImageFrame);
	return 0;
}

#define MAX_DEPTH_VALUE 30000
#define MIN_DEPTH_VALUE 1000
#define MAX_COLOR_VALUE 255
#define MIN_COLOR_VALUE 0

void Cap_depth2RGB(unsigned short depthValue, unsigned char* redValue, unsigned char* greenValue, unsigned char* blueValue)
{
	/* depth is too large */
	if(depthValue > MAX_DEPTH_VALUE)
	{
		*redValue   = MIN_COLOR_VALUE;
		*greenValue = MIN_COLOR_VALUE;
		*blueValue  = MIN_COLOR_VALUE;

		return;
	}

	if(depthValue < MIN_DEPTH_VALUE)
	{
		*redValue   = MAX_COLOR_VALUE;
		*greenValue = MAX_COLOR_VALUE;
		*blueValue  = MAX_COLOR_VALUE;

		return;
	}

	/*
	          colorValue    - MIN_COLOR_VALUE                         depthValue        - MIN_DEPTH_VALUE
	------------------------------------- =  1  -  --------------------------------
	MAX_COLOR_VALUE - MIN_COLOR_VALUE				MAX_DEPTH_VALUE  - MIN_DEPTH_VALUE

	*/
	unsigned int rate_t = (MAX_DEPTH_VALUE - MIN_DEPTH_VALUE) / (MAX_COLOR_VALUE - MIN_COLOR_VALUE);

	*redValue = (MAX_DEPTH_VALUE - depthValue)/rate_t;
	*greenValue = (MAX_DEPTH_VALUE - depthValue)/rate_t;
	*blueValue = (MAX_DEPTH_VALUE - depthValue)/rate_t;

	return;
}

void depthImg2RGB(Mat depthTmp, Mat depthRGB)
{
	if (CV_16U != depthTmp.type())
	{
		return;
	}

	if (CV_8UC3 != depthRGB.type())
	{
		return;
	}

	unsigned char redValue, greenValue, blueValue;

	for (int y = 0; y<DEPTH_HIGHT; y++)
	{
		const unsigned short* p_depthTmp = depthTmp.ptr<unsigned short>(y);
		unsigned char* p_depthRGB = depthRGB.ptr<unsigned char>(y);
		for (int x = 0; x<DEPTH_WIDTH; x++)
		{
			unsigned short depthValue = p_depthTmp[x];
			if (depthValue != 63355)
			{
				redValue = 0;
				greenValue = 0;
				blueValue = 0;

				Cap_depth2RGB(depthValue, &redValue, &greenValue, &blueValue);

				p_depthRGB[3 * x] = blueValue;
				p_depthRGB[3 * x + 1] = greenValue;
				p_depthRGB[3 * x + 2] = redValue;
			}
			else
			{
				p_depthRGB[3 * x] = 0;
				p_depthRGB[3 * x + 1] = 0;
				p_depthRGB[3 * x + 2] = 0;
			}

		}
	}

	return;	
}


//��ȡ���ͼ�����ݣ���������ʾ  
int DrawDepth(HANDLE h)
{
	const NUI_IMAGE_FRAME * pImageFrame = NULL;
	HRESULT hr = NuiImageStreamGetNextFrame(h, 0, &pImageFrame);
	if (FAILED(hr))
	{
		cout << "GetDepth Image Frame Failed" << endl;
		return-1;
	}
	INuiFrameTexture* pTexture = pImageFrame->pFrameTexture;
	NUI_LOCKED_RECT LockedRect;
	pTexture->LockRect(0, &LockedRect, NULL, 0);
	if (LockedRect.Pitch != 0)
	{
		BYTE* pBuff = (BYTE*)LockedRect.pBits;
		//OpenCV��ʾ�����Ƶ
		if (true)
		{
			Mat depthTmp(DEPTH_HIGHT, DEPTH_WIDTH, CV_16U, pBuff);

			unsigned short p_depth = depthTmp.ptr<USHORT>(DEPTH_HIGHT/2)[DEPTH_WIDTH/2];
			printf(" p_depth is %d\n", p_depth);
			
			
			//imshow("Depth", depthTmp);
			
			depthRGB.setTo(0);
			depthContinue.setTo(0);

			depthContinue = depthTmp.clone();
			//depthImg2RGB(depthTmp, depthRGB2);
			imshow("depthContinue", depthContinue);
			imwrite(g_file_name_depth_image, depthContinue);
			
			//��ʾ��������������Ϣ;
			
			for (int y = 0; y<DEPTH_HIGHT; y++)
			{
				const USHORT* p_depthTmp = depthTmp.ptr<USHORT>(y);
				uchar* p_depthRGB = depthRGB.ptr<uchar>(y);
				for (int x = 0; x<DEPTH_WIDTH; x++)
				{
					USHORT depthValue = p_depthTmp[x];
					if (depthValue != 63355)
					{
						uchar redValue, greenValue, blueValue;
						DepthToRGB(depthValue, redValue, greenValue, blueValue);
						p_depthRGB[3 * x] = blueValue;
						p_depthRGB[3 * x + 1] = greenValue;
						p_depthRGB[3 * x + 2] = redValue;
					}
					else
					{
						p_depthRGB[3 * x] = 0;
						p_depthRGB[3 * x + 1] = 0;
						p_depthRGB[3 * x + 2] = 0;
					}

				}
			}
			//imshow("DepthRGB",depthRGB);


		}
		int c = waitKey(1);//����ESC����  
		if (c == 27 || c == 'q' || c == 'Q')
		{
			SetEvent(m_hEvNuiProcessStop);
		}
	}
	NuiImageStreamReleaseFrame(h, pImageFrame);
	return 0;
}


//��ȡ�������ݣ��������ͼ�Ͻ�����ʾ  
int DrawSkeleton()
{
	NUI_SKELETON_FRAME SkeletonFrame;
	cv::Point pt[20];
	HRESULT hr = NuiSkeletonGetNextFrame(0, &SkeletonFrame);
	if (FAILED(hr))
	{
		cout << "GetSkeleton Image Frame Failed" << endl;
		return -1;
	}

	bool bFoundSkeleton = false;
	for (int i = 0; i < NUI_SKELETON_COUNT; i++)
	{
		if (SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED)
		{
			bFoundSkeleton = true;
		}
	}
	//Has skeletons!  
	if (bFoundSkeleton)
	{
		NuiTransformSmooth(&SkeletonFrame, NULL);
		//cout<<"skeleton num:"<<NUI_SKELETON_COUNT<<endl;
		for (int i = 0; i < NUI_SKELETON_COUNT; i++)
		{
			NUI_SKELETON_TRACKING_STATE trackingState = SkeletonFrame.SkeletonData[i].eTrackingState;
			if (trackingState == NUI_SKELETON_TRACKED)  //����λ�ø��ٳɹ�����ֱ�Ӷ�λ;
			{
				//NUI_SKELETON_DATA *pSkel = &(SkeletonFrame.SkeletonData[i]);
				NUI_SKELETON_DATA  SkelData = SkeletonFrame.SkeletonData[i];
				Point jointPositions[NUI_SKELETON_POSITION_COUNT];

				for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
				{
					LONG x, y;

					USHORT depth;
					cout << j << " :(" << SkelData.SkeletonPositions[j].x << "," << SkelData.SkeletonPositions[j].y << ") ";
					NuiTransformSkeletonToDepthImage(SkelData.SkeletonPositions[j], &x, &y, &depth, depthResolution);

					//circle(depthRGB, Point(x,y), 5, Scalar(255,255,255), -1, CV_AA);
					jointPositions[j] = Point(x, y);
				}

				for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
				{
					if (SkelData.eSkeletonPositionTrackingState[j] == NUI_SKELETON_POSITION_TRACKED)
					{
						circle(depthRGB, jointPositions[j], 5, SKELETON_COLORS[i], -1, CV_AA);
						circle(depthRGB, jointPositions[j], 6, Scalar(0, 0, 0), 1, CV_AA);
					}
					else if (SkelData.eSkeletonPositionTrackingState[j] == NUI_SKELETON_POSITION_INFERRED)
					{
						circle(depthRGB, jointPositions[j], 5, Scalar(255, 255, 255), -1, CV_AA);
					}

				}
				cout << endl;
			}
			else if (trackingState == NUI_SKELETON_POSITION_INFERRED) //�������λ�ø���δ�ɹ���ͨ���Ʋⶨλ����λ��;
			{
				LONG x, y;
				USHORT depth = 0;
				NuiTransformSkeletonToDepthImage(SkeletonFrame.SkeletonData[i].Position, &x, &y, &depth, depthResolution);
				cout << SkeletonFrame.SkeletonData[i].Position.x << ";" << SkeletonFrame.SkeletonData[i].Position.y << endl;
				circle(depthRGB, Point(x, y), 7, CV_RGB(0, 0, 0), CV_FILLED);
			}

		}

	}
	imshow("SkeletonDepth", depthRGB);
	int c = waitKey(1);//����ESC����  
	if (c == 27 || c == 'q' || c == 'Q')
	{
		SetEvent(m_hEvNuiProcessStop);
	}
	return 0;
}


//kinect��ȡ�������߳�;
DWORD WINAPI KinectDataThread(LPVOID pParam)
{
	HANDLE hEvents[4] = { m_hEvNuiProcessStop, m_hNextVideoFrameEvent,
		m_hNextDepthFrameEvent, m_hNextSkeletonEvent };
	while (1)
	{
		int nEventIdx;
		nEventIdx = WaitForMultipleObjects(sizeof(hEvents) / sizeof(hEvents[0]),
			hEvents, FALSE, 100);
		if (WAIT_OBJECT_0 == WaitForSingleObject(m_hEvNuiProcessStop, 0))
		{
			break;
		}
		//Process signal events  
		char file_name_color_image[100] = {0};
		char file_name_depth_image[100] = {0};

		GetLocalTime(&sys);
		sprintf_s(file_name_color_image, 100, "color_photos/color_%02d_%02d_%03d.jpg", sys.wMinute, sys.wSecond, sys.wMilliseconds);
		sprintf_s(file_name_depth_image, 100, "depth_photos/depth_%02d_%02d_%03d.png", sys.wMinute, sys.wSecond, sys.wMilliseconds);
		
		memcpy(g_file_name_color_image, file_name_color_image, 100 * sizeof(char));
		memcpy(g_file_name_depth_image, file_name_depth_image, 100 * sizeof(char));

		if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextVideoFrameEvent, 0))
		{
			DrawColor(m_pVideoStreamHandle);
		}
		if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextDepthFrameEvent, 0))
		{
			DrawDepth(m_pDepthStreamHandle);
		}
		if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0))
		{
			//DrawSkeleton();
		}

	}
	CloseHandle(m_hEvNuiProcessStop);
	m_hEvNuiProcessStop = NULL;
	CloseHandle(m_hNextSkeletonEvent);
	CloseHandle(m_hNextDepthFrameEvent);
	CloseHandle(m_hNextVideoFrameEvent);
	return 0;
}


DWORD WINAPI TestThread(LPVOID pParam)
{
	int count = 0;
	
	while(1)
	{
		//printf("count is %d\n", count++);
		waitKey(1000);
	}

	return 0;
}

int main(int argc, char * argv[])
{
	//��ʼ��NUI  
	HRESULT hr = NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON);
	if (hr != S_OK)
	{
		cout << "Nui Initialize Failed" << endl;
		return hr;
	}


	//��KINECT�豸�Ĳ�ɫͼ��Ϣͨ��  
	m_hNextVideoFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_pVideoStreamHandle = NULL;
	hr = NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, colorResolution, 0,
		2, m_hNextVideoFrameEvent, &m_pVideoStreamHandle);
	if (FAILED(hr))
	{
		cout << "Could not open image stream video" << endl;
		return hr;
	}

	//��KINECT�豸�������Ϣͨ��  
	m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_pDepthStreamHandle = NULL;
	hr = NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, depthResolution, 0,
		2, m_hNextDepthFrameEvent, &m_pDepthStreamHandle);
	if (FAILED(hr))
	{
		cout << "Could not open depth stream video" << endl;
		return hr;
	}
	m_hNextSkeletonEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	//���������¼�flag���ã� 
	//���� flag|NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT;ֻ��ͷ���ֱ�10���ڵ㣻
	//վ�� flag&~(NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT);��20���ڵ㣻
	hr = NuiSkeletonTrackingEnable(m_hNextSkeletonEvent,
		NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE/*&(~NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT)*/);
	if (FAILED(hr))
	{
		cout << "Could not open skeleton stream video" << endl;
		return hr;
	}
	m_hEvNuiProcessStop = CreateEvent(NULL, TRUE, FALSE, NULL);//���ڽ������¼�����

	//����һ���߳�---���ڶ�ȡ��ɫ����ȡ��������ݣ�  
	HANDLE m_hProcesss = CreateThread(NULL, 0, KinectDataThread, 0, 0, 0);
	///////////////////////////////////////////////  
	HANDLE m_hProcesssTest = CreateThread(NULL, 0, TestThread, 0, 0, 0);
	while (m_hEvNuiProcessStop != NULL)
	{
		WaitForSingleObject(m_hProcesss, INFINITE);
		CloseHandle(m_hProcesss);
		m_hProcesss = NULL;
	}

	//Clean up.  
	NuiShutdown();
	return 0;

}