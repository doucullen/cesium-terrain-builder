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
 * @file GlobalGeodetic.cpp
 * @brief This defines the `GlobalGeodetic` class
 */

#include "GlobalGeodetic.hpp"

#include "gdal_priv.h"
#include <windows.h>
#include <iostream>

#define PATHSEPSTRING "\\"
#define ISPATHSEP(c) ((c)=='/' || (c)=='\\')
#define FOLD(c) ((flags&FILEMATCH_CASEFOLD)?tolower(c):(c))

std::string GetFileDir1(const std::string& strPathFile)
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

std::string GetModulePath1()
{
	HMODULE hModule = GetModuleHandleA("ctb.dll");
	if (hModule != NULL)
	{
		char chPath[_MAX_PATH] = { 0 };
		GetModuleFileNameA(hModule, chPath, _MAX_PATH);
		return GetFileDir1(chPath);
	}
	char chPath[_MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, chPath, _MAX_PATH);
	return GetFileDir1(chPath);
}

static void InitGeodeticGdal()
{
	if (GDALGetDriverCount() == 0)
	{
		GDALAllRegister();

		std::string dataPath = GetModulePath1() + "gdaldata";
		std::cout << dataPath.c_str() << std::endl;
		CPLSetConfigOption("GDAL_DATA", dataPath.c_str());
	}
}

using namespace ctb;

// Set the spatial reference
static OGRSpatialReference
setSRS(void) {
  InitGeodeticGdal();
  OGRSpatialReference srs;
  srs.importFromEPSG(4326);
  return srs;
}

const OGRSpatialReference GlobalGeodetic::cSRS = setSRS();
