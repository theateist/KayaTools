// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <windows.h>

#include "KYFGLib.h"
#include "TCLAP\CmdLine.h"
#include "TCLAP\SwitchArg.h"
#include "TCLAP\ArgException.h"
#include "TCLAP\MultiArg.h"
#include "TCLAP\ValueArg.h"
#include "TCLAP\Arg.h"

#include <boost/filesystem.hpp>

#include "log4cpp/Appender.hh"
#include "log4cpp/OstreamAppender.hh"
#include "log4cpp/FileAppender.hh"
#include "log4cpp/Category.hh"
#include "log4cpp/PatternLayout.hh"
#include "log4cpp/PropertyConfigurator.hh"

// TODO: reference additional headers your program requires here
