/*******************************************************************************
 * Copyright 2014 GeoData <geodata@soton.ac.uk>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *******************************************************************************/

/**
 * @file GlobalMercator.cpp
 * @brief This defines the `GlobalMercator` class
 */

#define _USE_MATH_DEFINES       // for M_PI

#include "GlobalMercator.hpp"

#include "gdal_priv.h"
#include <windows.h>
#include <iostream>

#define PATHSEPSTRING "\\"
#define ISPATHSEP(c) ((c)=='/' || (c)=='\\')
#define FOLD(c) ((flags&FILEMATCH_CASEFOLD)?tolower(c):(c))

std::string GetFileDir(const std::string& strPathFile)
{
	int n = 0, i = 0;
	if (!strPathFile.empty())
	{
		i = 0;
#ifdef WIN32
		if (isalpha((unsigned char)strPathFile[0]) && strPathFile[1] == ':')
		{
			i = 2;
			n = i;
		}
#endif

		int nLen = strPathFile.length();
		while (i < nLen)
		{
			if (ISPATHSEP(strPathFile[i]))
			{
				n = i;
			}
			i++;
		}
		if (n != 0 || ISPATHSEP(strPathFile[0]))
		{
			// 剔除没有路径信息的情况
			return std::string(strPathFile.c_str(), n + 1);
		}
	}
	return "";
}

std::string GetModulePath()
{
	HMODULE hModule = GetModuleHandleA("ctb.dll");
	if (hModule != NULL)
	{
		char chPath[_MAX_PATH] = { 0 };
		GetModuleFileNameA(hModule, chPath, _MAX_PATH);
		return GetFileDir(chPath);
	}
	char chPath[_MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, chPath, _MAX_PATH);
	return GetFileDir(chPath);
}

static void InitMercatorGdal()
{
	if (GDALGetDriverCount() == 0)
	{
		GDALAllRegister();

		std::string dataPath = GetModulePath() + "gdaldata";
		std::cout << dataPath.c_str() << std::endl;
		CPLSetConfigOption("GDAL_DATA", dataPath.c_str());
	}
}


using namespace ctb;

// Set the class level properties
const unsigned int GlobalMercator::cSemiMajorAxis = 6378137;
const double GlobalMercator::cEarthCircumference = 2 * M_PI * GlobalMercator::cSemiMajorAxis;
const double GlobalMercator::cOriginShift = GlobalMercator::cEarthCircumference / 2.0;

// Set the spatial reference
static OGRSpatialReference
setSRS(void) {
  InitMercatorGdal();
  OGRSpatialReference srs;
  srs.importFromEPSG(3857);
  return srs;
}

const OGRSpatialReference GlobalMercator::cSRS = setSRS();
