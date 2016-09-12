#include "Navigator.h"

Navigator* g_pNavigator;
void onMouseNavigator(int event, int x, int y, int flags, void* userdata)
{
	g_pNavigator->handleMouse(event, x, y, flags);
}

namespace kai
{

Navigator::Navigator()
{
	AppBase();

	m_pCamFront = NULL;
	m_pAP = NULL;
	m_pMavlink = NULL;
	m_pCascade = NULL;
	m_pFlow = NULL;
	m_pROITracker = NULL;
	m_pDD = NULL;
	m_pMD = NULL;
	m_pUniverse = NULL;
	m_pFCN = NULL;
	m_pSSD = NULL;
	m_pFrame = NULL;
	m_pAT = NULL;
	m_pAM = NULL;
}

Navigator::~Navigator()
{
}

bool Navigator::start(Config* pConfig)
{
	//TODO: Caffe set data to GPU directly
	//TODO: Solve caffe ROI in DepthDetector
	//TODO: Optimize FCN

	g_pNavigator = this;
	int FPS;
	string camName;

	CHECK_INFO(pConfig->obj("APP")->var("bShowScreen", &m_bShowScreen));
	CHECK_INFO(pConfig->obj("APP")->var("bFullScreen", &m_bFullScreen));
	CHECK_INFO(pConfig->obj("APP")->var("waitKey", &m_waitKey));

	m_pFrame = new Frame();

	//Init Camera
	CHECK_INFO(pConfig->obj("APP")->var("camMain", &camName));
	if (camName != "video")
	{
		m_pCamFront = new _Stream();
		CHECK_FATAL(m_pCamFront->init(pConfig, camName));
		m_pCamFront->start();
	}

	//Init Automaton
	FPS=0;
	CHECK_INFO(pConfig->obj("Automaton0")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pAM = new _Automaton();
		CHECK_FATAL(m_pAM->init(pConfig, "Automaton0"));
		m_pAM->start();
	}

	//Init ROI Tracker
	FPS=0;
	CHECK_INFO(pConfig->obj("roiTracker0")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pROITracker = new _ROITracker();
		m_pROITracker->init(pConfig, "roiTracker0");
		m_pROITracker->m_pCamStream = m_pCamFront;
		m_pROITracker->start();
	}

	//Init Marker Detector
	FPS=0;
	CHECK_INFO(pConfig->obj("markerLanding")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pMD = new _Bullseye();
		m_pMD->init(pConfig, "markerLanding");
		m_pMD->m_pCamStream = m_pCamFront;
		m_pCamFront->m_bGray = true;
		m_pCamFront->m_bHSV = true;
		m_pMD->start();
	}

	//Init AprilTags Detector
	FPS=0;
	CHECK_INFO(pConfig->obj("AprilTagLanding")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pAT = new _AprilTags();
		m_pAT->init(pConfig, "AprilTagLanding");
		m_pAT->m_pCamStream = m_pCamFront;
		m_pAT->start();
	}

	//Init Mavlink
	FPS=0;
	CHECK_INFO(pConfig->obj("Mavlink0")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pMavlink = new _Mavlink();
		CHECK_FATAL(m_pMavlink->setup(pConfig, "Mavlink0"));
		m_pMavlink->start();
	}

	//Init Autopilot
	FPS=0;
	CHECK_INFO(pConfig->obj("Autopilot0")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pAP = new _AutoPilot();
		m_pAP->m_pMavlink = m_pMavlink;
		m_pAP->m_pROITracker = m_pROITracker;
		m_pAP->m_pMD = m_pMD;
		m_pAP->m_pAT = m_pAT;
		m_pAP->m_pAM = m_pAM;
		CHECK_FATAL(m_pAP->init(pConfig, "Autopilot0"));
		m_pAP->start();
	}

	//Init Universe
	FPS=0;
	CHECK_INFO(pConfig->obj("Universe")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pUniverse = new _Universe();
		m_pUniverse->init(pConfig,"Universe");
		m_pUniverse->start();
	}

	//Init SSD
	FPS=0;
	CHECK_INFO(pConfig->obj("SSD")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pSSD = new _SSD();
		m_pSSD->init(pConfig, "SSD");
		m_pSSD->m_pCamStream = m_pCamFront;
		m_pSSD->m_pUniverse = m_pUniverse;
		m_pSSD->start();
	}

	//Init Optical Flow
	FPS=0;
	CHECK_INFO(pConfig->obj("optflow0")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pFlow = new _Flow();
		CHECK_FATAL(m_pFlow->init(pConfig, "optflow0"));
		m_pFlow->m_pCamStream = m_pCamFront;
		m_pCamFront->m_bGray = true;
		m_pFlow->start();
	}

	//Init Depth Object Detector
	FPS=0;
	CHECK_INFO(pConfig->obj("depthObjDetector")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pDD = new _Depth();
		CHECK_FATAL(m_pDD->init(pConfig, "depthObjDetector"));
		m_pDD->m_pCamStream = m_pCamFront;
		m_pDD->m_pUniverse = m_pUniverse;
		m_pDD->m_pFlow = m_pFlow;
		m_pDD->start();
	}

	//Init FCN
	FPS=0;
	CHECK_INFO(pConfig->obj("FCN")->var("FPS", &FPS));
	if (FPS > 0)
	{
		m_pFCN = new _FCN();
		m_pFCN->init(pConfig,"FCN");
		m_pFCN->m_pCamStream = m_pCamFront;
		m_pFCN->start();
	}


	//UI thread
	m_bRun = true;

	if (m_bShowScreen)
	{
		namedWindow(APP_NAME, CV_WINDOW_NORMAL);
		if (m_bFullScreen)
		{
			setWindowProperty(APP_NAME,
					CV_WND_PROP_FULLSCREEN,
					CV_WINDOW_FULLSCREEN);
		}
		setMouseCallback(APP_NAME, onMouseNavigator, NULL);
	}

	while (m_bRun)
	{
//		Mavlink_Messages mMsg;
//		mMsg = m_pMavlink->current_messages;
//		m_pCamFront->m_pCamL->m_bGimbal = true;
//		m_pCamFront->m_pCamL->setAttitude(mMsg.attitude.roll, 0, mMsg.time_stamps.attitude);

		if (m_bShowScreen)
		{
			draw();
		}

		//Handle key input
		m_key = waitKey(m_waitKey);
		handleKey(m_key);
	}

	STOP(m_pAP);
	STOP(m_pMavlink);
	STOP(m_pROITracker);
	STOP(m_pMD);
	STOP(m_pUniverse);
	STOP(m_pDD);
	STOP(m_pFlow);
	STOP(m_pFCN);

	m_pMavlink->complete();
	m_pMavlink->close();

	COMPLETE(m_pAP);
	COMPLETE(m_pROITracker);
	COMPLETE(m_pMD);
	COMPLETE(m_pUniverse);
	COMPLETE(m_pDD);
	COMPLETE(m_pFlow);
	COMPLETE(m_pFCN);
	COMPLETE(m_pCamFront);

	RELEASE(m_pMavlink);
	RELEASE(m_pCamFront);
	RELEASE(m_pROITracker);
	RELEASE(m_pAP);
	RELEASE(m_pMD);
	RELEASE(m_pUniverse);
	RELEASE(m_pDD);
	RELEASE(m_pFlow);
	RELEASE(m_pFCN);

	return 0;

}

void Navigator::draw(void)
{
	iVector4 textPos;
	textPos.m_x = 15;
	textPos.m_y = 20;
	textPos.m_w = 20;
	textPos.m_z = 500;

	if(m_pCamFront)
	{
		if(!m_pCamFront->draw(m_pFrame, &textPos))return;
	}

	if(m_pAM)
	{
		m_pAM->draw(m_pFrame, &textPos);
	}

	if(m_pAP)
	{
		m_pAP->draw(m_pFrame, &textPos);
	}

	if(m_pAT)
	{
		m_pAT->draw(m_pFrame, &textPos);
	}

	if(m_pUniverse)
	{
		m_pUniverse->draw(m_pFrame, &textPos);
	}

	if(m_pSSD)
	{
		m_pSSD->draw(m_pFrame, &textPos);
	}

	if(m_pMavlink)
	{
		m_pMavlink->draw(m_pFrame, &textPos);
	}

	if(m_pFCN)
	{
		m_pFCN->draw(m_pFrame, &textPos);
	}


	imshow(APP_NAME, *m_pFrame->getCMat());

}

void Navigator::handleKey(int key)
{
	switch (key)
	{
	case 27:
		m_bRun = false;	//ESC
		break;
	default:
		break;
	}
}

void Navigator::handleMouse(int event, int x, int y, int flags)
{
	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		break;
	case EVENT_LBUTTONUP:
		break;
	case EVENT_MOUSEMOVE:
		break;
	case EVENT_RBUTTONDOWN:
		break;
	default:
		break;
	}
}

}
