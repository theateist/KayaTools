#include "stdafx.h"
#include "log4cpp/Appender.hh"
#include "log4cpp/OstreamAppender.hh"
#include "log4cpp/FileAppender.hh"
#include "log4cpp/Category.hh"
#include "log4cpp/PatternLayout.hh"

using namespace std;
using namespace TCLAP;
namespace fs = boost::filesystem;

struct Camera
{
	CAMHANDLE handle;
	string id;
};
typedef vector<Camera> CameraVec;

struct FrameGrabber
{
	FGHANDLE handle;
	int64_t serialNumber;
};
typedef vector<FrameGrabber> FrameGrabberVec;

namespace KAYA
{
	 enum POCXP_REMOTE_CONTROL
	 {
		POCXP_OFF = 0,
		POCXP_AUTO = 0x01000000, // big endian
		POCXP_ON = 0x02000000, // big endian
	 };
	 const uint64_t POCXP_REMOTE_CONTROL_ADDR = 0x1000;
}

namespace OPTRONIS
{
	const uint64_t LUT_X_ADDR = 0x610C;
	const uint64_t LUT_Y_ADDR = 0x6110;
	const uint64_t LUT_FLASH_ADDR = 0x6100;
	const uint64_t LUT_MODE_ADDR = 0x6114;
	const unsigned int LUT_SIZE = 1024;

	enum LUT_MODE
	{
		FACTORY = 0,
		USER = 2
	};
};

void getCameras(map<FGHANDLE, CameraVec>& camsFG)
{	
	log4cpp::Category& log = log4cpp::Category::getRoot();

	// scan for frame grabbers
	unsigned int numFG = KYFG_Scan(nullptr, 0);													
	//TODO: in the current API, a virtual frame grabber is always counted as one of the discovered frame grabbers. And, it's always the last one.
	//		To not include it to the list I just subscribe 1 from the total frame grabbers. 
	//		Future API should have a better way to identify the virtual frame frabber.
	numFG--;

	for(unsigned int grabberIndex = 0; grabberIndex < numFG; ++grabberIndex)
	{
		// =============== open frame grabber ===============

		FGHANDLE fgHandle = KYFG_Open(grabberIndex);
		//TODO: how to check if it's a vritual?
		if (fgHandle == -1)
		{
			log << log4cpp::Priority::ERROR << "Failed to connect to grabber. Reason: " << std::hex << fgHandle;
			continue;	
		}
		camsFG[fgHandle] = CameraVec();

		// =============== discover cams ===============

		static const unsigned int MAX_CAMS = 4;
		CAMHANDLE detectedCams[MAX_CAMS];
		int numDetectedCams;
		FGSTATUS status = KYFG_CameraScan(fgHandle, detectedCams, &numDetectedCams);
		if(status != FGSTATUS_OK)
		{
			log << log4cpp::Priority::ERROR << "Failed to discover cameras connected to grabber. Reason: " << std::hex << status;			
			continue;								
		}

		log << log4cpp::Priority::INFO << "Discovered a grabber";

		// =============== add cams to the list ===============
		
		for (int camIndex = 0; camIndex < numDetectedCams; ++camIndex)
		{
			// opens a connection to chosen camera and retrieves native xml file
			FGSTATUS status = KYFG_CameraOpen2(detectedCams[camIndex], nullptr);
			if(status != FGSTATUS_OK)
			{
				log << log4cpp::Priority::ERROR << "Failed to connect to the cam. Reason: " << std::hex << status;
				continue;
			}

			static const unsigned int CONFIG_VALUE_SIZE = 50;
			char configVal[CONFIG_VALUE_SIZE];
			unsigned int configLen = CONFIG_VALUE_SIZE;
			status = KYFG_GetCameraValueStringCopy(detectedCams[camIndex], "DeviceID", configVal, &configLen);			
			if(status != FGSTATUS_OK)
			{				
				log << log4cpp::Priority::ERROR << "Failed to get Id for cam. Reason: " << std::hex << status;
				status = KYFG_CameraClose(detectedCams[camIndex]);
				if(status != FGSTATUS_OK)
				{					
					log << log4cpp::Priority::WARN << "Failed to close cam. Reason: " << std::hex << status;
				}
				continue;
			}

			Camera cam;
			cam.handle = detectedCams[camIndex];
			cam.id = string(configVal);
			camsFG[fgHandle].push_back(cam);

			log << log4cpp::Priority::INFO << "Discovered cam " << cam.id;
		}		
	}
}

void getFrameGrabbers(FrameGrabberVec& frameGrabbers)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	// scan for frame grabbers
	unsigned int numFG = KYFG_Scan(nullptr, 0);	
	//TODO: in the current API, a virtual frame grabber is always counted as one of the discovered frame grabbers. And, it's always the last one.
	//		To not include it to the list I just subscribe 1 from the total frame grabbers. 
	//		Future API should have a better way to identify the virtual frame frabber.
	numFG--;

	// =============== open frame grabber ===============
	for(unsigned int grabberIndex = 0; grabberIndex < numFG; ++grabberIndex)
	{
		FGHANDLE fgHandle = KYFG_Open(grabberIndex);
		//TODO: how to check if it's a vritual?
		if (fgHandle == -1)
		{
			log << log4cpp::Priority::ERROR << "Failed to connect to grabber. Reason: " << std::hex << fgHandle;
			continue;	
		}

		char* tmpConfigVal = nullptr;
		int64_t serialNumber = KYFG_GetGrabberValueInt(fgHandle, "SerialNumber");
		if(serialNumber == INT_MAX)
		{
			log << log4cpp::Priority::ERROR << "Failed to get SerialNumber of frame grabber";
			FGSTATUS status = KYFG_Close(fgHandle);
			// close frame grabber
			if(status != FGSTATUS_OK)
				log << log4cpp::Priority::ERROR << "Failed to close frame grabber. Reason: " << std::hex << status;
			
			continue;
		}

		FrameGrabber fg;
		fg.handle = fgHandle;
		fg.serialNumber = serialNumber;
		frameGrabbers.push_back(fg);

		log << log4cpp::Priority::INFO << "Discovered frame grabber " << fg.serialNumber;
	}
}

void closeCameras(const map<FGHANDLE, CameraVec>& camsFG)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	for each (const pair<FGHANDLE, CameraVec>& pair in camsFG)
	{
		const CameraVec& cams = pair.second;
		for each (const Camera& cam in cams)
		{
			FGSTATUS status = KYFG_CameraClose(cam.handle);
			if(status != FGSTATUS_OK)
				log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Failed to close. Reason: " << std::hex << status;
			else
				log << log4cpp::Priority::INFO << "Cam " << cam.id << ": Closed successfully";
		}

		FGHANDLE fgHandle = pair.first;
		FGSTATUS status = KYFG_Close(fgHandle);
		if(status != FGSTATUS_OK)
			log << log4cpp::Priority::WARN << "FrameGrabber: Failed to close. Reason: " << std::hex << status;
		else
			log << log4cpp::Priority::INFO << "FrameGrabber: Closed successfully";
	}
}

void closeFrameGrabbers(const FrameGrabberVec& frameGrabbers)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	for each (const FrameGrabber& fg in frameGrabbers)
	{
		FGSTATUS status = KYFG_Close(fg.handle);
		if(status != FGSTATUS_OK)
			log << log4cpp::Priority::WARN << "FrameGrabber " << fg.serialNumber << ": Failed to close. Reason: " << std::hex << status;
		else
			log << log4cpp::Priority::INFO << "FrameGrabber " << fg.serialNumber << ": Closed successfully";
	}
}

// the order is based on the list from where this function is called
vector<unsigned int> selectDevices(unsigned int numDevices)
{
	cout << "Select devices by entering their indexes. Press 0 to select all or ENTER to finish." << endl;
	
	vector<unsigned int> selectedIndexes;
    while (1)
    {
		cout << "Enter index: ";
		std::string line;
		getline(std::cin, line);
		if (line.empty())
			break;

		unsigned int index;
        stringstream ss(line);
        if ((ss >> index) && 0 <= index && index <= numDevices)
        {
			if (index == 0)
			{
				// select all devices
				selectedIndexes.clear();
				for (unsigned int i = 0; i < numDevices; ++i)
				{
					selectedIndexes.push_back(i);
				}
				break;
			}
			else
			{
				selectedIndexes.push_back(index - 1);
			}
        }
		else
		{
			std::cout << "Invalid index. Try again." << std::endl;
		}
    }

	return selectedIndexes;
}

void readLUTFromFile(vector<int>& lut, const string& lutFilename)
{
	if (!fs::exists(lutFilename))
	{
		std::stringstream error;
		error << "File " << lutFilename << " not found";
		throw exception(error.str().c_str());
	}

	lut.reserve(OPTRONIS::LUT_SIZE);

	string line;
	ifstream lutFile(lutFilename);
	while (getline(lutFile, line))
	{
		std::istringstream iss(line);
		unsigned int lutY;
		if (iss >> lutY)
		{
			lut.push_back(lutY);
		}
		else
		{
			std::stringstream error;
			error << "Invalid LUT_Y value for LUT_X = " << lut.size();
			throw exception(error.str().c_str());
		}
	}

	if (lut.size() != OPTRONIS::LUT_SIZE)
	{
		std::stringstream error;
		error << "LUT file should have " << OPTRONIS::LUT_SIZE << " entries";
		throw exception(error.str().c_str());
	}
}

bool readLUTFromCam(const Camera& cam, vector<unsigned int>& lut)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	lut.reserve(OPTRONIS::LUT_SIZE);

	unsigned int lutRegLen = 4;
	for (unsigned int lutX = 0; lutX < OPTRONIS::LUT_SIZE; ++lutX)
	{
		// set the lutX for which we want to ready the lutY below
		unsigned int lutXBigEndian = _byteswap_ulong(lutX);
		FGSTATUS status = KYFG_CameraWriteReg(cam.handle, OPTRONIS::LUT_X_ADDR, &lutXBigEndian, &lutRegLen);
		if(status != FGSTATUS_OK)
		{
			log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Failed to write LUT_X = " << lutXBigEndian << "(" << lutX << ") into reg at " 
				<< std::hex << OPTRONIS::LUT_X_ADDR << ". Reason: " << status;
			return false;
		}

		unsigned int lutYBigEndian;			
		status = KYFG_CameraReadReg(cam.handle, OPTRONIS::LUT_Y_ADDR, &lutYBigEndian, &lutRegLen);
		if(status != FGSTATUS_OK)
		{
			log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Failed to read LUT_Y from reg at "	<< std::hex << OPTRONIS::LUT_Y_ADDR << ". Reason: " << status;
			return false;
		}

		// convert back to little endian
		unsigned int lutY = _byteswap_ulong(lutYBigEndian);
		lut.push_back(lutY);
	}

	return true;
}

inline bool setLUTValues(const Camera& cam, unsigned int lutX, unsigned lutY)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	static unsigned int regLen = 4;

	// set the lutX for which we want to ready the lutY below
	unsigned int lutXBigEndian = _byteswap_ulong(lutX);
	log << log4cpp::Priority::DEBUG << "Cam " << cam.id << ": Writing LUT_X = " << lutXBigEndian << "(" << lutX << ") into reg at " << std::hex << OPTRONIS::LUT_X_ADDR;
	FGSTATUS status = KYFG_CameraWriteReg(cam.handle, OPTRONIS::LUT_X_ADDR, &lutXBigEndian, &regLen);
	if(status != FGSTATUS_OK)
	{
		log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Failed to write LUT_X = " << lutXBigEndian << "(" << lutX << ") into reg at " 
			<< std::hex << OPTRONIS::LUT_X_ADDR << ". Reason: " << status;
		return false;
	}

	unsigned int lutYBigEndian = _byteswap_ulong(lutY);		
	log << log4cpp::Priority::DEBUG << "Cam " << cam.id << ": Writing LUT_Y = " << lutYBigEndian << "(" << lutY << ") into reg at " << std::hex << OPTRONIS::LUT_Y_ADDR;
	status = KYFG_CameraWriteReg(cam.handle, OPTRONIS::LUT_Y_ADDR, &lutYBigEndian, &regLen);
	if(status != FGSTATUS_OK)
	{
		log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Failed to write LUT_Y = " << lutYBigEndian << "(" << _byteswap_ulong(lutYBigEndian) << ") into reg at " 
			<< std::hex << OPTRONIS::LUT_Y_ADDR << ". Reason: " << status;
		return false;
	}

	return true;
}

inline bool flashLUT(const Camera& cam)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	unsigned int regLen = 4;
	unsigned int flashCmdBigEndian = _byteswap_ulong(1);			
	FGSTATUS status = KYFG_CameraWriteReg(cam.handle, OPTRONIS::LUT_FLASH_ADDR, &flashCmdBigEndian, &regLen);
	if(status != FGSTATUS_OK)
	{
		log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Failed to write FLASH_CMD = " << flashCmdBigEndian << "(" << _byteswap_ulong(flashCmdBigEndian) << ") into reg at " 
			<< std::hex << OPTRONIS::LUT_FLASH_ADDR << ". Reason: " << status;
		return false;
	}

	return true;
}

void applyLUT(const string& lutFilename, bool flash)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	map<FGHANDLE, CameraVec> camsFG;
	try
	{
		log << log4cpp::Priority::NOTICE << "Reading LUT values from " << lutFilename;
		vector<int> lut;
		readLUTFromFile(lut, lutFilename);

		log << log4cpp::Priority::NOTICE << "Discovering cams";
		getCameras(camsFG);

		if (camsFG.size() > 0)
		{
			// put all cams in one list and print
			vector<Camera> detectedCams;
			unsigned int camIndex = 1;
			cout << "Detected cams:" << endl;
			for each (const pair<FGHANDLE, CameraVec>& pair in camsFG)
			{			
				const CameraVec& cams = pair.second;
				for each (const Camera& cam in cams)
				{
					cout << "\t" << camIndex++ << " - " << cam.id << endl;
					//TODO: avoid copying
					detectedCams.push_back(cam);
				}
			}

			// get cams that LUT should be applied to
			vector<unsigned int> selectedCams = selectDevices(detectedCams.size());

			// apply LUT
			for each (auto camIndex in selectedCams)
			{
				Camera& cam = detectedCams.at(camIndex);

				bool lutSuccess = true;
				log << log4cpp::Priority::NOTICE << "Cam " << cam.id << ": Updating LUT";
				for (unsigned int lutX = 0; lutX < lut.size(); ++lutX)
				{
					if (!setLUTValues(cam, lutX, lut[lutX]))
					{
						log << log4cpp::Priority::ERROR << "Cam " << cam.id << ": Failed updating LUT";
						lutSuccess = false;
						break;
					}
				}

				if (lutSuccess && flash)
				{
					log << log4cpp::Priority::NOTICE << "Cam " << cam.id << ": Flashing LUT";
					if (flashLUT(cam))
						log << log4cpp::Priority::NOTICE << "Cam " << cam.id << ": Flash succeeded";
					else
						log << log4cpp::Priority::WARN << "Cam " << cam.id << ": Flash failed";
				}
			}
		}
		else
		{
			log << log4cpp::Priority::WARN << "No cameras were discovered";
		}
	}
	catch(exception& ex)
	{
		log << log4cpp::Priority::ERROR << ex.what();
	}

	log << log4cpp::Priority::NOTICE << "Closing cameras";
	closeCameras(camsFG);
	log << log4cpp::Priority::NOTICE << "Done";
}

void powerCycleRangeExtender()
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	FrameGrabberVec frameGrabbers;
	try
	{
		log << log4cpp::Priority::NOTICE << "Discovering frame grabbers";
		getFrameGrabbers(frameGrabbers);

		if (frameGrabbers.size() > 0)
		{
			unsigned int fgIndex = 1;
			cout << "Detected frame grabbers:" << endl;
			for each (const FrameGrabber& fg in frameGrabbers)
			{
				cout << "\t" << fgIndex++ << " - " << fg.serialNumber << endl;
			}

			vector<unsigned int> selectedFGs = selectDevices(frameGrabbers.size());

			// power cycle
			for each (auto fgIndex in selectedFGs)
			{
				log << log4cpp::Priority::NOTICE << "FrameGrabber: " << frameGrabbers.at(fgIndex).serialNumber << ". Power cycling";

				// disable pocxp on each of 4 inputs on RE
				for(int rePhysicalLink = 0; rePhysicalLink < 4; ++rePhysicalLink)
				{
					FGSTATUS status = KYFG_WritePortReg(frameGrabbers.at(fgIndex).handle, rePhysicalLink, KAYA::POCXP_REMOTE_CONTROL_ADDR, KAYA::POCXP_REMOTE_CONTROL::POCXP_OFF);				
					if(status != FGSTATUS_OK)
					{
						log << log4cpp::Priority::ERROR << "Failed to turn off PoCXP on link" << rePhysicalLink << ". Reason: " << std::hex << status;
					}
				}
			
				// delay to make sure cameras are turned off completely
				Sleep(2000);	
			
				// enable pocxp on each of 4 inputs on RE
				for(int rePhysicalLink = 0; rePhysicalLink < 4; ++rePhysicalLink)
				{
					FGSTATUS status = KYFG_WritePortReg(frameGrabbers.at(fgIndex).handle, rePhysicalLink, KAYA::POCXP_REMOTE_CONTROL_ADDR, KAYA::POCXP_REMOTE_CONTROL::POCXP_AUTO);				
					if(status != FGSTATUS_OK)
					{
						log << log4cpp::Priority::ERROR << "Failed to turn on PoCXP on link" << rePhysicalLink << ". Reason: " << std::hex << status;
					}
				}

				log << log4cpp::Priority::NOTICE << "Done";
			}
		}
		else
		{
			log << log4cpp::Priority::WARN << "No frame grabbers were discovered";
		}
	}
	catch(exception& ex)
	{
		log << log4cpp::Priority::ERROR << ex.what();
	}

	log << log4cpp::Priority::NOTICE << "Closing frame grabbers";
	closeFrameGrabbers(frameGrabbers);
	log << log4cpp::Priority::NOTICE << "Done";
}

void dumpLUT(const string& outDir)
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	map<FGHANDLE, CameraVec> camsFG;
	try
	{
		if (!fs::exists(outDir))
		{
			std::stringstream error;
			error << "Directory " << outDir << " not found";
			throw exception(error.str().c_str());
		}
		if(!fs::is_directory(fs::path(outDir)))
		{
			std::stringstream error;
			error << "The output path is not directory " << outDir;
			throw exception(error.str().c_str());
		}

		log << log4cpp::Priority::NOTICE << "Discovering cams";
		getCameras(camsFG);

		if (camsFG.size() > 0)
		{
			// put all cams in one list and print
			vector<Camera> detectedCams;
			unsigned int camIndex = 1;
			cout << "Detected cams:" << endl;
			for each (const pair<FGHANDLE, CameraVec>& pair in camsFG)
			{			
				const CameraVec& cams = pair.second;
				for each (const Camera& cam in cams)
				{
					cout << "\t" << camIndex++ << " - " << cam.id << endl;
					//TODO: avoid copying
					detectedCams.push_back(cam);
				}
			}

			// get cams to dump LUT from
			vector<unsigned int> selectedCams = selectDevices(detectedCams.size());

			// dump LUT
			for each (auto camIndex in selectedCams)
			{
				Camera& cam = detectedCams.at(camIndex);

				stringstream ss;
				ss << "lut_" << cam.id << ".txt";
				fs::path dumpCamFilenamePath(ss.str());
				fs::path dumpDirPath(outDir);
				ofstream dumpLUTFile((dumpDirPath / dumpCamFilenamePath).string());

				log << log4cpp::Priority::NOTICE << "Cam " << cam.id << ": Dumping LUT";

				vector<unsigned int> lut;
				if (readLUTFromCam(cam, lut))
				{
					for each (auto lutY in lut)
					{
						dumpLUTFile << lutY << endl;
					}
					log << log4cpp::Priority::NOTICE << "Done";
				}
				else
				{
					log << log4cpp::Priority::ERROR << "Failed";
				}

				dumpLUTFile.close();
			}
		}
	}
	catch(exception& ex)
	{
		log << log4cpp::Priority::ERROR << ex.what();
	}

	log << log4cpp::Priority::NOTICE << "Closing cameras";
	closeCameras(camsFG);
	log << log4cpp::Priority::NOTICE << "Done";
}

//TODO: finish log
//TODO: each tool should have its own log
//TODO: create Logs folder where logs should go
void configLoggers()
{
	//TODO: move to properties file
	log4cpp::Appender *consoleAppender = new log4cpp::OstreamAppender("console", &std::cout);
	auto consoleLayout = new log4cpp::PatternLayout();
	consoleLayout->setConversionPattern("%m%n");
    consoleAppender->setLayout(consoleLayout);

	log4cpp::Appender *fileAppender = new log4cpp::FileAppender("default", "KayaTools.log");
	auto fileLayout = new log4cpp::PatternLayout();
	fileLayout->setConversionPattern("%d{%Y-%m-%d %H:%M:%S.%l} [%p] %m%n");
    fileAppender->setLayout(fileLayout);

    log4cpp::Category& root = log4cpp::Category::getRoot();
    root.setPriority(log4cpp::Priority::INFO);
    root.addAppender(consoleAppender);
    root.addAppender(fileAppender);

	//root << log4cpp::Priority::DEBUG << "test1";
	//root << log4cpp::Priority::INFO << "test2";
}

void fooGet()
{
	log4cpp::Category& log = log4cpp::Category::getRoot();

	FrameGrabberVec frameGrabbers;
	try
	{
		log << log4cpp::Priority::NOTICE << "Discovering frame grabbers";
		getFrameGrabbers(frameGrabbers);

		if (frameGrabbers.size() > 0)
		{
			unsigned int fgIndex = 1;
			cout << "Detected frame grabbers:" << endl;
			for each (const FrameGrabber& fg in frameGrabbers)
			{
				cout << "\t" << fgIndex++ << " - " << fg.serialNumber << endl;
			}

			vector<unsigned int> selectedFGs = selectDevices(frameGrabbers.size());
			for each (auto fgIndex in selectedFGs)
			{
				double timeout = KYFG_GetGrabberValueFloat(frameGrabbers.at(fgIndex).handle, "DeviceLinkCommandTimeout");
				if(timeout == INT_MAX)
				{
					cout << frameGrabbers.at(fgIndex).serialNumber << ": Failed to read timeout" << endl;
					continue;
				}

				cout << frameGrabbers.at(fgIndex).serialNumber << ": " << timeout << endl;
			}
		}
		else
		{
			log << log4cpp::Priority::WARN << "No frame grabbers were discovered";
		}
	}
	catch(exception& ex)
	{
		log << log4cpp::Priority::ERROR << ex.what();
	}

	log << log4cpp::Priority::NOTICE << "Closing frame grabbers";
	closeFrameGrabbers(frameGrabbers);
	log << log4cpp::Priority::NOTICE << "Done";
}

void fooSet(float t)
{
	std::cout << std::fixed << std::setprecision(3) << t;
	log4cpp::Category& log = log4cpp::Category::getRoot();

	FrameGrabberVec frameGrabbers;
	try
	{
		log << log4cpp::Priority::NOTICE << "Discovering frame grabbers";
		getFrameGrabbers(frameGrabbers);

		if (frameGrabbers.size() > 0)
		{
			unsigned int fgIndex = 1;
			cout << "Detected frame grabbers:" << endl;
			for each (const FrameGrabber& fg in frameGrabbers)
			{
				cout << "\t" << fgIndex++ << " - " << fg.serialNumber << endl;
			}

			vector<unsigned int> selectedFGs = selectDevices(frameGrabbers.size());
			for each (auto fgIndex in selectedFGs)
			{
				double timeout = KYFG_SetGrabberValueFloat(frameGrabbers.at(fgIndex).handle, "DeviceLinkCommandTimeout", t);
				if(timeout == INT_MAX)
				{
					cout << frameGrabbers.at(fgIndex).serialNumber << ": Failed to read timeout to" << t << endl;
					continue;
				}

				cout << frameGrabbers.at(fgIndex).serialNumber << ": " << timeout << endl;
			}
		}
		else
		{
			log << log4cpp::Priority::WARN << "No frame grabbers were discovered";
		}
	}
	catch(exception& ex)
	{
		log << log4cpp::Priority::ERROR << ex.what();
	}

	log << log4cpp::Priority::NOTICE << "Closing frame grabbers";
	closeFrameGrabbers(frameGrabbers);
	log << log4cpp::Priority::NOTICE << "Done";
}

//TODO: add log4cpp_debug.lib
int main(int argc, _TCHAR* argv[])
{
	configLoggers();
	
	try
	{				
		CmdLine cmd("KayaTools", ' ', "1.0");

		//TODO: how to do {-l <string> [--no_flush] | -o <string> | -p}
		ValueArg<string> lutArg("l", "lut", "Set LUT", true, "", "lut_filename");
		SwitchArg noLUTFlashSwitch("", "no_flash", "Do not flash LUT");
		cmd.add(noLUTFlashSwitch);

		ValueArg<string> lutOutArg("o", "out_lut", "Dump LUT values", true, "", "out_dir");
		SwitchArg pocxpSwitch("p", "pocxp", "Power cycle cameras");

		SwitchArg timeoutGetArg("", "t1", "Set timeout");
		ValueArg<double> timeoutSetArg("", "t2", "Set timeout", false, 0, "timeout");
		

		vector<Arg*> xorlist;
        xorlist.push_back(&lutArg);
        xorlist.push_back(&lutOutArg);
        xorlist.push_back(&pocxpSwitch);

        xorlist.push_back(&timeoutGetArg);
        xorlist.push_back(&timeoutSetArg);

		cmd.xorAdd(xorlist);

		cmd.parse(argc,argv);
		
		if(lutArg.isSet())
		{
			applyLUT(lutArg.getValue(), !noLUTFlashSwitch.isSet());
		}
		else if(lutOutArg.isSet())
		{
			dumpLUT(lutOutArg.getValue());
		}
		else if (pocxpSwitch.isSet())
		{
			powerCycleRangeExtender();
		}
		else if(timeoutGetArg.isSet())
		{
			fooGet();
		}
		else if (timeoutSetArg.isSet())
		{
			fooSet(timeoutSetArg.getValue());
		}
	}
	catch (ArgException& e)
	{
		cerr << "Error: " << e.error() << " for arg " << e.argId() << endl;
	}
	catch (ExitException& e)
	{
		if (e.getExitStatus() != 0)
		{
			cerr << "Error: " << e.getExitStatus() << endl;
		}
	}
	catch (exception& e)
	{
		cerr << "Error: " << e.what() << endl;
	}
	catch (...)
	{
		cerr << "Unhandled exception." << endl;
	}

	cout << "Press ENTER to exit..." << endl;
	getchar();
	
	return 0;
}

