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
 * @file ctb-tile.cpp
 * @brief Convert a GDAL raster to a tile format
 *
 * This tool takes a GDAL raster and by default converts it to gzip compressed
 * terrain tiles which are written to an output directory on the filesystem.
 *
 * In the case of a multiband raster, only the first band is used to create the
 * terrain heights.  No water mask is currently set and all tiles are flagged
 * as being 'all land'.
 *
 * It is recommended that the input raster is in the EPSG 4326 spatial
 * reference system. If this is not the case then the tiles will be reprojected
 * to EPSG 4326 as required by the terrain tile format.
 *
 * Using the `--output-format` flag this tool can also be used to create tiles
 * in other raster formats that are supported by GDAL.
 */

#include <iostream>
#include <sstream>
#include <string.h>             // for strcmp
#include <stdlib.h>             // for atoi
#include <thread>
#include <mutex>
#include <future>
#include <sqlite3.h>
#include <filesystem>

#include "cpl_multiproc.h"      // for CPLGetNumCPUs
#include "cpl_vsi.h"            // for virtual filesystem
#include "gdal_priv.h"
#include "commander.hpp"        // for cli parsing
#include "concat.hpp"

#include "GlobalMercator.hpp"
#include "RasterIterator.hpp"
#include "TerrainIterator.hpp"
#include "TerrainTiler.hpp"

//#include "API/PIEEnvironment.h"
//#include "API/PIETileStorage.h"
//
//PIE_TileStorageH globalTileSorage = nullptr;

using namespace std;
using namespace ctb;

#ifdef _WIN32
static const char *osDirSep = "\\";
#else
static const char *osDirSep = "/";
#endif

/// Handle the terrain build CLI options
class TerrainBuild : public Command {
public:
  TerrainBuild(const char *name, const char *version) :
    Command(name, version),
    outputDir("."),
    outputFormat("Terrain"),
    profile("geodetic"),
    threadCount(-1),
    tileSize(0),
    startZoom(-1),
    endZoom(-1),
    verbosity(1),
	compress(0),
    waterMaskDir(""),
    resume(false)
  {}

  void
  check() const {
    switch(command->argc) {
    case 1:
      return;
    case 0:
      cerr << "  Error: The gdal datasource must be specified" << endl;
      break;
    default:
      cerr << "  Error: Only one command line argument must be specified" << endl;
      break;
    }

    help();                   // print help and exit
  }

  static void
  setOutputDir(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->outputDir = command->arg;
  }

  static void
  setOutputFormat(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->outputFormat = command->arg;
  }

  static void
  setProfile(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->profile = command->arg;
  }

  static void
  setThreadCount(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->threadCount = atoi(command->arg);
  }

  static void
  setTileSize(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->tileSize = atoi(command->arg);
  }

  static void
  setStartZoom(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->startZoom = atoi(command->arg);
  }

  static void
  setEndZoom(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->endZoom = atoi(command->arg);
  }

  static void
  setQuiet(command_t *command) {
    --(static_cast<TerrainBuild *>(Command::self(command))->verbosity);
  }

  static void
  setVerbose(command_t *command) {
    ++(static_cast<TerrainBuild *>(Command::self(command))->verbosity);
  }

  static void
	setCompress(command_t* command) {
	static_cast<TerrainBuild *>(Command::self(command))->compress = atoi(command->arg);
	ctb::Terrain::Compress = static_cast<TerrainBuild *>(Command::self(command))->compress;
  }

  static void
      setWaterMaskDir(command_t* command) {
      static_cast<TerrainBuild*>(Command::self(command))->waterMaskDir = command->arg;
      ctb::Terrain::WaterMaskDir = static_cast<TerrainBuild*>(Command::self(command))->waterMaskDir;
      VSIStatBufL stat;
      if (!VSIStatExL(ctb::Terrain::WaterMaskDir.c_str(), &stat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG))
      {
          size_t pos = std::string(ctb::Terrain::WaterMaskDir).find("_alllayers");
          if (pos != std::string::npos) {
              std::string inputPath(ctb::Terrain::WaterMaskDir);
              // 截取 _alllayers 后面的路径
              std::string zoomStr = inputPath.substr(pos + 10);

              //判断zoom层级上一层的目录名称不是_alllayers 如果slash_pos = 0证明上一层是_alllayers不是allyers带后缀
              size_t slash_pos = zoomStr.find("/");
              if (slash_pos != std::string::npos && slash_pos != 0)
              {
                  //如果海洋掩膜对应的输入目录里zoom层级上一层的目录名称不是_alllayers，则认为输入异常，报错！！！
                  std::cout << "WARRNING: WaterMaskDir Format must be _alllayers + zoom" + Terrain::WaterMaskDir << std::endl;
              }

              // 去掉路径中的 '/' 字符
              zoomStr.erase(std::remove(zoomStr.begin(), zoomStr.end(), '/'), zoomStr.end());

              // 检查是否可以转换为数字
              if (!zoomStr.empty() && std::all_of(zoomStr.begin(), zoomStr.end(), ::isdigit)) {
                  // 可以转换为数字，转换并设置给 Terrain::WaterMaskZoom
                  Terrain::WaterMaskZoom = std::stoi(zoomStr);

                  ctb::Terrain::bUseWaterMask = true;
              }

              // 包含 _alllayers 文件夹，截取 _alllayers 之前的路径
              std::string pathBeforeAllLayers = std::string(ctb::Terrain::WaterMaskDir).substr(0, pos + 10); // 加上 "_alllayers/"
              // pathBeforeAllLayers 现在包含 _alllayers 这一层目录
              // ctb::Terrain::WaterMaskDir 也被更新为 pathBeforeAllLayers
              static_cast<TerrainBuild*>(Command::self(command))->waterMaskDir = pathBeforeAllLayers.c_str();
              ctb::Terrain::WaterMaskDir = pathBeforeAllLayers.c_str();

          }
      }
      //ctb::Terrain::WaterMaskDir = static_cast<TerrainBuild*>(Command::self(command))->waterMaskDir;

  }

  static void
      setUseMBTiles(command_t* command) {
      static_cast<TerrainBuild*>(Command::self(command))->bUseMbtiles = atoi(command->arg);
      ctb::Terrain::bUseMbtiles = static_cast<TerrainBuild*>(Command::self(command))->bUseMbtiles;
  }

  static void
  setResume(command_t* command) {
    static_cast<TerrainBuild *>(Command::self(command))->resume = true;
  }

  static void
  setResampleAlg(command_t *command) {
    GDALResampleAlg eResampleAlg;

    if (strcmp(command->arg, "nearest") == 0)
      eResampleAlg = GRA_NearestNeighbour;
    else if (strcmp(command->arg, "bilinear") == 0)
      eResampleAlg = GRA_Bilinear;
    else if (strcmp(command->arg, "cubic") == 0)
      eResampleAlg = GRA_Cubic;
    else if (strcmp(command->arg, "cubicspline") == 0)
      eResampleAlg = GRA_CubicSpline;
    else if (strcmp(command->arg, "lanczos") == 0)
      eResampleAlg = GRA_Lanczos;
    else if (strcmp(command->arg, "average") == 0)
      eResampleAlg = GRA_Average;
    else if (strcmp(command->arg, "mode") == 0)
      eResampleAlg = GRA_Mode;
    else if (strcmp(command->arg, "max") == 0)
      eResampleAlg = GRA_Max;
    else if (strcmp(command->arg, "min") == 0)
      eResampleAlg = GRA_Min;
    else if (strcmp(command->arg, "med") == 0)
      eResampleAlg = GRA_Med;
    else if (strcmp(command->arg, "q1") == 0)
      eResampleAlg = GRA_Q1;
    else if (strcmp(command->arg, "q3") == 0)
      eResampleAlg = GRA_Q3;
    else {
      cerr << "Error: Unknown resampling algorithm: " << command->arg << endl;
      static_cast<TerrainBuild *>(Command::self(command))->help(); // exit
    }

    static_cast<TerrainBuild *>(Command::self(command))->tilerOptions.resampleAlg = eResampleAlg;
  }

  static void
  addCreationOption(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->creationOptions.AddString(command->arg);
  }

  static void
  setErrorThreshold(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->tilerOptions.errorThreshold = atof(command->arg);
  }

  static void
  setWarpMemory(command_t *command) {
    static_cast<TerrainBuild *>(Command::self(command))->tilerOptions.warpMemoryLimit = atof(command->arg);
  }

  const char *
  getInputFilename() const {
    return  (command->argc == 1) ? command->argv[0] : NULL;
  }

  const char *outputDir,
    *outputFormat,
    *waterMaskDir,
    *profile;

  int threadCount,
    tileSize,
    startZoom,
    endZoom,
    verbosity;

  bool compress;
  bool useWaterMask = false;
  bool resume;
  bool bUseMbtiles = false;

  //std::string waterMaskDir;
  CPLStringList creationOptions;
  TilerOptions tilerOptions;
};

/**
 * Create a filename for a tile coordinate
 *
 * This also creates the tile directory structure.
 */
static string
getTileFilename(const TileCoordinate *coord, const string dirname, const char *extension) {
  static mutex mutex;

  VSIStatBufL stat;
  string filename = concat(dirname, coord->zoom, osDirSep, coord->x);

  lock_guard<std::mutex> lock(mutex);

  // Check whether the `{zoom}/{x}` directory exists or not
  if (VSIStatExL(filename.c_str(), &stat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG)) {
    filename = concat(dirname, coord->zoom);

    // Check whether the `{zoom}` directory exists or not
    if (VSIStatExL(filename.c_str(), &stat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG)) {
      // Create the `{zoom}` directory
      if (VSIMkdir(filename.c_str(), 0755))
        throw CTBException("Could not create the zoom level directory");

    } else if (!VSI_ISDIR(stat.st_mode)) {
      throw CTBException("Zoom level file path is not a directory");
    }

    // Create the `{zoom}/{x}` directory
    filename += concat(osDirSep, coord->x);
    if (VSIMkdir(filename.c_str(), 0755))
      throw CTBException("Could not create the x level directory");

  } else if (!VSI_ISDIR(stat.st_mode)) {
    throw CTBException("X level file path is not a directory");
  }

  // Create the filename itself, adding the extension if required
  filename += concat(osDirSep, coord->y);
  if (extension != NULL) {
    filename += ".";
    filename += extension;
  }

  return filename;
}

//ddx:获取mbtiles文件的文件路径
static string
getMbtilesname(const string dirname, const char* extension)
{
    static mutex mutex;

    VSIStatBufL stat;
    string filename = dirname;

    lock_guard<std::mutex> lock(mutex);

    filename += "Tile";
    if (extension != NULL) {
        filename += ".";
        filename += extension;
    }

    return filename;
}


/**
 * Increment a TilerIterator whilst cooperating between threads
 *
 * This function maintains an global index on an iterator and when called
 * ensures the iterator is incremented to point to the next global index.  This
 * can therefore be called with different tiler iterators by different threads
 * to ensure all tiles are iterated over consecutively.  It assumes individual
 * tile iterators point to the same source GDAL dataset.
 */
template<typename T> int
incrementIterator(T &iter, int currentIndex) {
  static int globalIteratorIndex = 0; // keep track of where we are globally
  static mutex mutex;        // ensure iterations occur serially between threads

  lock_guard<std::mutex> lock(mutex);

  while (currentIndex < globalIteratorIndex) {
    ++iter;
    ++currentIndex;
  }
  ++globalIteratorIndex;

  return currentIndex;
}

/// Get a handle on the total number of tiles to be created
static int iteratorSize = 0;    // the total number of tiles
template<typename T> void
setIteratorSize(T &iter) {
  static mutex mutex;

  lock_guard<std::mutex> lock(mutex);

  if (iteratorSize == 0) {
    iteratorSize = iter.getSize();
  }
}

/// A thread safe wrapper around `GDALTermProgress`
static int
CPL_STDCALL termProgress(double dfComplete, const char *pszMessage, void *pProgressArg) {
  static mutex mutex;          // GDALTermProgress isn't thread safe, so lock it
  int status;

  lock_guard<std::mutex> lock(mutex);
  status = GDALTermProgress(dfComplete, pszMessage, pProgressArg);

  return status;
}

/// In a thread safe manner describe the file just created
static int
CPL_STDCALL verboseProgress(double dfComplete, const char *pszMessage, void *pProgressArg) {
  stringstream stream;
  stream << "[" << (int) (dfComplete*100) << "%] " << pszMessage << endl;
  cout << stream.str();

  return TRUE;
}

// Default to outputting using the GDAL progress meter
static GDALProgressFunc progressFunc = termProgress;

/// Output the progress of the tiling operation
int
showProgress(int currentIndex, string filename) {
  stringstream stream;
  stream << "created " << filename << " in thread " << this_thread::get_id();
  string message = stream.str();

  return progressFunc(currentIndex / (double) iteratorSize, message.c_str(), NULL);
}

static bool
fileExists(const std::string& filename) {
  VSIStatBufL statbuf;
  return VSIStatExL(filename.c_str(), &statbuf, VSI_STAT_EXISTS_FLAG) == 0;
}

/// Output GDAL tiles represented by a tiler to a directory
static void
buildGDAL(const RasterTiler &tiler, TerrainBuild *command) {
  GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(command->outputFormat);

  if (poDriver == NULL) {
    throw CTBException("Could not retrieve GDAL driver");
  }

  if (poDriver->pfnCreateCopy == NULL) {
    throw CTBException("The GDAL driver must be write enabled, specifically supporting 'CreateCopy'");
  }

  const char *extension = poDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
  const string dirname = string(command->outputDir) + osDirSep;
  i_zoom startZoom = (command->startZoom < 0) ? tiler.maxZoomLevel() : command->startZoom,
    endZoom = (command->endZoom < 0) ? 0 : command->endZoom;

  RasterIterator iter(tiler, startZoom, endZoom);
  int currentIndex = incrementIterator(iter, 0);
  setIteratorSize(iter);

  while (!iter.exhausted()) {
    const TileCoordinate *coordinate = iter.GridIterator::operator*();
    GDALDataset *poDstDS;
    const string filename = getTileFilename(coordinate, dirname, extension);

    
    if( !command->resume || !fileExists(filename) ) {
      GDALTile *tile = *iter;
      const string temp_filename = concat(filename, ".tmp");
      poDstDS = poDriver->CreateCopy(temp_filename.c_str(), tile->dataset, FALSE,
                                     command->creationOptions.List(), NULL, NULL );
      delete tile;

      // Close the datasets, flushing data to destination
      if (poDstDS == NULL) {
        throw CTBException("Could not create GDAL tile");
      }

      GDALClose(poDstDS);

      if (VSIRename(temp_filename.c_str(), filename.c_str()) != 0) {
        throw new CTBException("Could not rename temporary file");
      }
    }

    currentIndex = incrementIterator(iter, currentIndex);
    showProgress(currentIndex, filename);
  }
}

static unsigned char *createTileBuffer(
	const std::vector<ctb::i_terrain_height> &heights,
	char children,
	const std::vector<char> &mask,
	size_t &size)
{
	if (heights.empty() || mask.empty()) {
		size = 0;
		return nullptr;
	}

	const size_t n1 = heights.size() * sizeof(ctb::i_terrain_height);
	const size_t n2 = 1;
	const size_t n3 = mask.size();
	size = n1 + n2 + n3;

	unsigned char *data = new unsigned char[size];
	memcpy(data, heights.data(), n1);
	memcpy(data + n1, &children, n2);
	memcpy(data + n1 + n2, mask.data(), n3);

	return data;
}


/// Output terrain tiles represented by a tiler to a directory
static void
buildTerrain(const TerrainTiler &tiler, TerrainBuild *command) {
  const string dirname = string(command->outputDir) + osDirSep;
  i_zoom startZoom = (command->startZoom < 0) ? tiler.maxZoomLevel() : command->startZoom,
    endZoom = (command->endZoom < 0) ? 0 : command->endZoom;

  TerrainIterator iter(tiler, startZoom, endZoom);
  int currentIndex = incrementIterator(iter, 0);
  setIteratorSize(iter);
  while (!iter.exhausted()) {
    const TileCoordinate *coordinate = iter.GridIterator::operator*();
	
    // todo:重写writeFile函数 用一个单例 一个锁 mbtiles写的时候加上锁  ，写mbTile需要用到的二进制数据信息在tile里面，coordinate里面有行列号层级
    //是否重写
    if (command->bUseMbtiles)
    {

        if (!command->resume) {

            TerrainTile* tile = *iter;

            //默认文件已经存在则直接继续写瓦片文件即可
            //获取当瓦片输出的二进制数据，并输出
            std::vector<uint8_t> buffer = tile->writeMBTIlesFile();
            Singleton::getInstance().WriteTile(coordinate->zoom, coordinate->x, coordinate->y, buffer);
            std::cout << "writeMBTIlesFile  " + std::to_string(coordinate->zoom) + "  " + std::to_string(coordinate->x) + "  " + std::to_string(coordinate->y) <<endl;

            delete tile;
        }
    }
    else
    {
        //不生成mbtiles的文件，使用原来的格式
        const string filename = getTileFilename(coordinate, dirname, "terrain");
        std::cout << "writeTerrainFile  " + std::to_string(coordinate->zoom) + "  " + std::to_string(coordinate->x) + "  " + std::to_string(coordinate->y) << endl;
        if (!command->resume || !fileExists(filename)) {
            TerrainTile* tile = *iter;
            const string temp_filename = concat(filename, ".tmp");

            if (command->compress)
            {
                tile->writeFile(temp_filename.c_str());
            }
            else
            {
                FILE* fp = fopen(temp_filename.c_str(), "wb");
                if (fp != NULL)
                {
                    tile->writeFile(fp);
                    fclose(fp);
                    fp = NULL;
                }
            }
            delete tile;

            if (VSIRename(temp_filename.c_str(), filename.c_str()) != 0) {
                throw new CTBException("Could not rename temporary file");
            }
        }
    }

    currentIndex = incrementIterator(iter, currentIndex);
  }
}

/**
 * Perform a tile building operation
 *
 * This function is designed to be run in a separate thread.
 */
static int
runTiler(TerrainBuild *command, ctb::Grid *grid) {
  GDALDataset  *poDataset = (GDALDataset *) GDALOpen(command->getInputFilename(), GA_ReadOnly);
  if (poDataset == NULL) {
    cerr << "Error: could not open GDAL dataset" << endl;
    return 1;
  }

  try {
    if (strcmp(command->outputFormat, "Terrain") == 0) {
      const TerrainTiler tiler(poDataset, *grid);
      buildTerrain(tiler, command);
    } else {                    // it's a GDAL format
      const RasterTiler tiler(poDataset, *grid, command->tilerOptions);
      buildGDAL(tiler, command);
    }

  } catch (CTBException &e) {
    cerr << "Error: " << e.what() << endl;
  }

  GDALClose(poDataset);

  return 0;
}

int
main(int argc, char *argv[]) {
  // Specify the command line interface
  TerrainBuild command = TerrainBuild(argv[0], version.cstr);
  command.setUsage("[options] GDAL_DATASOURCE");
  command.option("-o", "--output-dir <dir>", "specify the output directory for the tiles (defaults to working directory)", TerrainBuild::setOutputDir);
  command.option("-f", "--output-format <format>", "specify the output format for the tiles. This is either `Terrain` (the default) or any format listed by `gdalinfo --formats`", TerrainBuild::setOutputFormat);
  command.option("-p", "--profile <profile>", "specify the TMS profile for the tiles. This is either `geodetic` (the default) or `mercator`", TerrainBuild::setProfile);
  command.option("-c", "--thread-count <count>", "specify the number of threads to use for tile generation. On multicore machines this defaults to the number of CPUs", TerrainBuild::setThreadCount);
  command.option("-t", "--tile-size <size>", "specify the size of the tiles in pixels. This defaults to 65 for terrain tiles and 256 for other GDAL formats", TerrainBuild::setTileSize);
  command.option("-s", "--start-zoom <zoom>", "specify the zoom level to start at. This should be greater than the end zoom level", TerrainBuild::setStartZoom);
  command.option("-e", "--end-zoom <zoom>", "specify the zoom level to end at. This should be less than the start zoom level and >= 0", TerrainBuild::setEndZoom);
  command.option("-r", "--resampling-method <algorithm>", "specify the raster resampling algorithm.  One of: nearest; bilinear; cubic; cubicspline; lanczos; average; mode; max; min; med; q1; q3. Defaults to average.", TerrainBuild::setResampleAlg);
  command.option("-n", "--creation-option <option>", "specify a GDAL creation option for the output dataset in the form NAME=VALUE. Can be specified multiple times. Not valid for Terrain tiles.", TerrainBuild::addCreationOption);
  command.option("-z", "--error-threshold <threshold>", "specify the error threshold in pixel units for transformation approximation. Larger values should mean faster transforms. Defaults to 0.125", TerrainBuild::setErrorThreshold);
  command.option("-m", "--warp-memory <bytes>", "The memory limit in bytes used for warp operations. Higher settings should be faster. Defaults to a conservative GDAL internal setting.", TerrainBuild::setWarpMemory);
  command.option("-g", "--gzip-compress <gzip>", "Do gzip compress file", TerrainBuild::setCompress);
  command.option("-R", "--resume", "Do not overwrite existing files", TerrainBuild::setResume);
  command.option("-q", "--quiet", "only output errors", TerrainBuild::setQuiet);
  command.option("-v", "--verbose", "be more noisy", TerrainBuild::setVerbose);
  command.option("-w", "--water-mask <dir>", "-specify the water mask directory for the tiles , the Dir befor zoomDir Name must be /_alllayers/", TerrainBuild::setWaterMaskDir);
  command.option("-b", "--mbtiles <mbtiles>", "Use MBTiles format for output. Specify the MBTiles file name.", TerrainBuild::setUseMBTiles);


  // Parse and check the arguments
  command.parse(argc, argv);
  command.check();

  //PIE_SetWorkPath("D:/code/PIE-SVN/PIE-Map-Tools/terrain-builder/thirdparty/PIE");
  //PIE_Init();

  //globalTileSorage = PIE_TileStorage_Create();
  //PIE_TileStorage_FromConfigFile(globalTileSorage, "D:/gisdata/pieterrain/pieterrain.xml");

  // Set the output type
  if (command.verbosity > 1) {
    progressFunc = verboseProgress; // noisy
  } else if (command.verbosity < 1) {
    progressFunc = GDALDummyProgress; // quiet
  }

  // Check whether or not the output directory exists
  VSIStatBufL stat;
  if (VSIStatExL(command.outputDir, &stat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG)) {
    cerr << "Error: The output directory does not exist: " << command.outputDir << endl;
    return 1;
  } else if (!VSI_ISDIR(stat.st_mode)) {
    cerr << "Error: The output filepath is not a directory: " << command.outputDir << endl;
    return 1;
  }

  // Define the grid we are going to use
  ctb::Grid grid;
  if (strcmp(command.profile, "geodetic") == 0) {
    int tileSize = (command.tileSize < 1) ? 65 : command.tileSize;
    grid = GlobalGeodetic(tileSize, true);
  } else if (strcmp(command.profile, "mercator") == 0) {
    int tileSize = (command.tileSize < 1) ? 256 : command.tileSize;
    grid = GlobalMercator(tileSize);
  } else {
    cerr << "Error: Unknown profile: " << command.profile << endl;
    return 1;
  }

  // Run the tilers in separate threads
  vector<future<int>> tasks;
  int threadCount = (command.threadCount > 0) ? command.threadCount : CPLGetNumCPUs();

  //如果使用mbtiles类型的文件，在这里创建
  if (command.bUseMbtiles)
  {
      const string dirname = string(command.outputDir) + osDirSep;
      const string mbTileFileName = getMbtilesname(dirname, "mbtiles");
      if (!fileExists(mbTileFileName))
      {
          //如果文件不存在说明是该层级的第一个瓦片，需要初始化一下db还有文件名
          Singleton::getInstance().CreatMBTiles(mbTileFileName);
          Singleton::getInstance().WriteTileMetadata();
      }
      else
      {
          if (!command.resume)
          {
              //如果覆盖现有文件，先删除再初始化
              if (std::remove(mbTileFileName.c_str()) != 0) {
                  std::cerr << "Error deleting file" << std::endl; // 如果删除文件出错，输出错误信息
              }
              else {
                  std::cout << "File successfully deleted" << std::endl; // 删除成功
              }

              Singleton::getInstance().CreatMBTiles(mbTileFileName);
              Singleton::getInstance().WriteTileMetadata();
          }
      }

  }
  std::cout << "InputInfo : " << std::endl;
  std::cout << "InputFIleFile = " + std::string(argv[argc - 1]) << std::endl; //打印输出目录作为cmd窗口的提示
  std::cout << "OutputFile = " + std::string(command.outputDir) << std::endl; //打印输出目录作为cmd窗口的提示
  std::cout << "TILE_SIZE = " + std::to_string(TILE_SIZE) << std::endl;//打印TILE_SIZE作为cmd窗口的提示
  if (ctb::Terrain::bUseWaterMask)
  {
      std::cout << "WaterMaskDir = " + Terrain::WaterMaskDir << std::endl;//打印WaterMaskDir作为cmd窗口的提示

  }
  else
  {
      std::cout << "bUseWaterMask = false" << std::endl;//打印bUseWaterMask作为cmd窗口的提示
  }

  // Instantiate the threads using futures from a packaged_task
  for (int i = 0; i < threadCount ; ++i) {
    packaged_task<int(TerrainBuild *, ctb::Grid *)> task(runTiler); // wrap the function
    tasks.push_back(task.get_future());                        // get a future
    thread(move(task), &command, &grid).detach(); // launch on a thread
  }

  // Synchronise the completion of the threads
  for (auto &task : tasks) {
    task.wait();
  }

  // Get the value from the futures
  for (auto &task : tasks) {
    int retval = task.get();

    // return on the first encountered problem
    if (retval)
      return retval;
  }

  return 0;
}


