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
 * @file TerrainTiler.cpp
 * @brief This defines the `TerrainTiler` class
 */

#include "CTBException.hpp"
#include "TerrainTiler.hpp"

using namespace ctb;

TerrainTile *
ctb::TerrainTiler::createTile(const TileCoordinate &coord) const {
  // Get a terrain tile represented by the tile coordinate 获取由瓦片坐标表示的地形瓦片
  TerrainTile *terrainTile = new TerrainTile(coord);
  GDALTile *rasterTile = createRasterTile(coord); // the raster associated with this tile coordinate
  GDALRasterBand *heightsBand = rasterTile->dataset->GetRasterBand(1);

  // Copy the raster data into an array 将栅格数据复制到数组中
  float rasterHeights[TerrainTile::TILE_CELL_SIZE];
  if (heightsBand->RasterIO(GF_Read, 0, 0, TILE_SIZE, TILE_SIZE,
                            (void *) rasterHeights, TILE_SIZE, TILE_SIZE, GDT_Float32,
                            0, 0) != CE_None) {
   // throw CTBException("Could not read heights from raster");
	  for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
		  rasterHeights[i] = (i_terrain_height)(0);
          //unsigned short  最大是65535 TILE_CELL_SIZE65536越界了，越界之后又变成0所以会死循环，改成int就可以了
          //if (i == 65535)
          //{
          //    int temp = i;
          //    printf("i = %d",i);
          //}
	  }
  }

  delete rasterTile;

  // Convert the raster data into the terrain tile heights.  This assumes the 将栅格数据转换为地形图块高度
  // input raster data represents meters above sea level. Each terrain height 这假设输入栅格数据表示海拔米数
  //value is the number of 1/5 meter units above -1000 meters. 各地形高度值是 -1000 米以上 1/5 米单位的数量。
  // TODO: try doing this using a VRT derived band:  尝试使用 VRT 派生频段执行此操作：
  // (http://www.gdal.org/gdal_vrttut.html)
  //for (unsigned int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
  //unsigned short  最大是65535 TILE_CELL_SIZE 是256*256的话=65536越界了，越界之后又变成0所以会死循环，改成int就可以了

	 // for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
  //  terrainTile->mHeights[i] = (i_terrain_height) ((rasterHeights[i] + 1000) * 5);
  //}

  //ddx:融合256*256的掩膜数据
  int maskZoom = Terrain::WaterMaskZoom;  // 11;

  //ddx_degbug:调试过程中把直接读取的每个值也记录下来
  float* HeightBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* oriBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* preasBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* signedShortBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* preasSignedShortBuffer = new float[TerrainTile::TILE_CELL_SIZE];

  if (coord.zoom < maskZoom  || !Terrain::bUseWaterMask)   //如果要切的层级比掩膜还要大，或者没有输入有效的掩膜参数  则执行原来的逻辑
  {
      ////这个if里面是小于maskZoom不做处理，以下是测试内容
      //float max = 0, min = 0;
      //  for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
      //    terrainTile->mHeights[i] = (i_terrain_height) ((rasterHeights[i] + 1000) * 5);

      //    //debug：

      //    HeightBuffer[i] = rasterHeights[i];
      //    oriBuffer[i] = (i_terrain_height)((rasterHeights[i] + 1000) * 5);
      //    signedShortBuffer[i] = (short)((rasterHeights[i] + 1000) * 5);

      //    
      //    if (HeightBuffer[i] > max)
      //    {
      //        max = HeightBuffer[i];
      //    }
      //    max = max > HeightBuffer[i] ? max : HeightBuffer[i];
      //    min = min < HeightBuffer[i] ? min : HeightBuffer[i];
      //  }

      //  //if (min < -653 || max > 3917)
      //  //{
      //  //    int a = 0;
      //  //}

      //  //debug反解析回来看看对不对
      //  for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
      //      preasBuffer[i] = (oriBuffer[i] / 5) - 1000;
      //      preasSignedShortBuffer[i] = (signedShortBuffer[i] / 5) - 1000;
      //  }

        delete HeightBuffer;
        delete oriBuffer;
        delete preasBuffer;
        delete signedShortBuffer;
        delete preasSignedShortBuffer;

  }
  else
  {
      //首先获取海洋掩膜瓦片的高度数组信息 256*256
      float* maskHeights = nullptr;
      
      //ddx_debug:
      int maskArraySize = TerrainTile::MASK_CELL_SIZE;//256 * 256;
      float* oriHeights_Debug = new float[maskArraySize];
      float* ddxHeights_Debug = new float[maskArraySize];

      maskHeights = ctb::TerrainTiler::getMaskHeights(coord); 

      //然后分别计算海洋掩膜和当前瓦片的bounds（webmercator 投影）
      TileCoordinate maskCoordinate;
      int maskTileSize = MASK_SIZE; //256
      maskCoordinate.zoom = Terrain::WaterMaskZoom; //11
      int zoomDifference = coord.zoom - maskCoordinate.zoom;
      maskCoordinate.x = coord.x >> zoomDifference;
      maskCoordinate.y = coord.y >> zoomDifference;

      CRSBounds maskBounds = calBounds(maskCoordinate);
      CRSBounds tileBounds = calBounds(coord);
      
      //区分影像与地形，地形相当于是点阵，影像相当于是格子集，因此4*4的地形相当于分了三个瓦片块出来但是记录的是四个分割点的位置
      //含义:x和y方向上，当前瓦片 单位格网所代表的距离长度（webmercator坐标） 单位是（m/网格）
      float scaleFactorX = (tileBounds.getMaxX() - tileBounds.getMinX()) / (TILE_SIZE - 1);
      float scaleFactorY = (tileBounds.getMaxY() - tileBounds.getMinY()) / (TILE_SIZE - 1);

      //mask瓦片上     计算结果是webmercator坐标的单位距离 相当于多少个网格,单位是(网格/m)
      double dCacheRatioX = (maskTileSize - 1) / (maskBounds.getMaxX() - maskBounds.getMinX());
      double dCacheRatioY = (maskTileSize - 1) / (maskBounds.getMaxY() - maskBounds.getMinY());

      //进行插值
      for (int i = 0; i < TILE_SIZE; i++)       //i 行间循环 对应纬度 Latitude
      {
          for (int j = 0; j < TILE_SIZE; j++)       //j列间循环 对应经度 Longitude
          {
              // 创建一个 OGRPoint 对象表示要遍历的二维点   遍历顺序从左到右，从上到下
              OGRPoint pointPos(tileBounds.getMinX() + scaleFactorX * j , tileBounds.getMaxY() - scaleFactorY * i);
              //计算tile上当前遍历点在mask上的行列号（未取整）
              float fCol = (pointPos.getX() - maskBounds.getMinX()) * dCacheRatioX;
              float fRow = (maskBounds.getMaxY() - pointPos.getY()) * dCacheRatioY;

              int nMinRow = (int)fRow;
              int nMaxRow = (int)std::ceil(fRow);
              int nMinCol = (int)fCol;
              int nMaxCol = (int)std::ceil(fCol);

              //防止行列号因为过大而越界
              if (nMinRow >= maskTileSize)
                  nMinRow = maskTileSize - 1;
              if (nMaxRow >= maskTileSize)
                  nMaxRow = maskTileSize - 1;
              if (nMinCol >= maskTileSize)
                  nMinCol = maskTileSize - 1;
              if (nMaxCol >= maskTileSize)
                  nMaxCol = maskTileSize - 1;

              //防止行列号因为过小而越界
              if (nMinRow < 0)
                  nMinRow = 0;
              if (nMaxRow < 0)
                  nMaxRow = 0;
              if (nMinCol < 0)
                  nMinCol = 0;
              if (nMaxCol < 0)
                  nMaxCol = 0;
              
              //双线性插值
              float fDelta = fRow - nMinRow;
              float fWestElevation = maskHeights[nMinCol + nMinRow * maskTileSize] * (1.0 - fDelta) + maskHeights[nMinCol + nMaxRow * maskTileSize] * fDelta;
              float fEastElevation = maskHeights[nMaxCol + nMinRow * maskTileSize] * (1.0 - fDelta) + maskHeights[nMaxCol + nMaxRow * maskTileSize] * fDelta;

              fDelta = fCol - nMinCol;
              float dElevation = fWestElevation * (1.0 - fDelta) + fEastElevation * fDelta;

              int index = i * TILE_SIZE + j;

              //ddx_debug:
              oriHeights_Debug[index] = rasterHeights[index];
              ddxHeights_Debug[index] = dElevation;


              if (rasterHeights[index] > 0)
              {
                  terrainTile->mHeights[index] = (i_terrain_height)((rasterHeights[index] + 1000) * 5);
              }
              else
              {
                  terrainTile->mHeights[index] = (i_terrain_height)((dElevation + 1000) * 5);
                  //terrainTile->mHeights[index] = (i_terrain_height)((rasterHeights[index] + 1000) * 5);
              }
             
          }
      }
      
      //ddx_debug:
      delete oriHeights_Debug;
      delete ddxHeights_Debug;

      delete maskHeights;
      
      
      //for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
      //    if (rasterHeights[i] > 0)
      //    {
      //        //大于0默认是水面上，不做处理
      //        terrainTile->mHeights[i] = (i_terrain_height)((rasterHeights[i] + 1000) * 5);
      //    }
      //    else
      //    {
      //        //小于0认为是水下，需要插值，插值结果为rasterHeightValue
      //        float rasterHeightValue = 0;
      //        //todo:
      //        //1.根据现在的coord，x y zoom计算出当前瓦片在mask（11级）上的x y zoom,找到对应瓦片，打开解析 将baty转换成short然后记得（/5-1000）转换成真实的高度数据，得到一个65535的数组
      //        //2.计算现在的这个65*65的bounds:已知web墨卡托投影的正方形宽度为20037508.3427892*2
      //        // dx=20037508.3427892*2/（2^coord.zoom）
      //        //left=coord.x * dx
      //        //right = left + dx
      //        //buttom = coord.y * dx
      //        //top = buttom + dx
      //        //计算当前瓦片的bound和 对应的bound  对应nTileSize还是65  pntPos是65*65瓦片上的每个瓦片的中心点的web墨卡托坐标
      //        //nCacheWidth和rcCacheBounds.Width一个是web墨卡托投影下的宽度一个是瓦片的网格宽度个数
      //        //double dCacheRatioX = (nCacheWidth - 1) / rcCacheBounds.Width();是每格子多宽，还是单位宽度占多少个格子
      //        //dCacheRatioX是单位宽度占多少个格子，所以
      //        //nCacheWidthrc是瓦片的网格宽度个数CacheBounds.Width是web墨卡托投影下的宽度 
      //        terrainTile->mHeights[i] = (i_terrain_height)((rasterHeightValue + 1000) * 5);
      //    }
      //}

  }

  // If we are not at the maximum zoom level we need to set child flags on the  如果我们没有达到最大缩放级别，我们需要在
  // tile where child tiles overlap the dataset bounds.  子图块与数据集边界重叠的图块。
  if (coord.zoom != maxZoomLevel()) {
    CRSBounds tileBounds = mGrid.tileBounds(coord);

    if (! (bounds().overlaps(tileBounds))) {
      terrainTile->setAllChildren(false);
    } else {
      if (bounds().overlaps(tileBounds.getSW())) {
        terrainTile->setChildSW();
      }
      if (bounds().overlaps(tileBounds.getNW())) {
        terrainTile->setChildNW();
      }
      if (bounds().overlaps(tileBounds.getNE())) {
        terrainTile->setChildNE();
      }
      if (bounds().overlaps(tileBounds.getSE())) {
        terrainTile->setChildSE();
      }
    }
  }

  return terrainTile;
}

GDALTile *
ctb::TerrainTiler::createRasterTile(const TileCoordinate &coord) const {
  // Ensure we have some data from which to create a tile  确保我们有一些用于创建图块的数据
  if (poDataset && poDataset->GetRasterCount() < 1) {
    throw CTBException("At least one band must be present in the GDAL dataset");
  }

  // Get the bounds and resolution for a tile coordinate which represents the  获取代表瓦片坐标的边界和分辨率
  // data overlap requested by the terrain specification.  地形规范要求的数据重叠。
  double resolution;
  CRSBounds tileBounds = terrainTileBounds(coord, resolution);

  // Convert the tile bounds into a geo transform
  double adfGeoTransform[6];
  adfGeoTransform[0] = tileBounds.getMinX(); // min longitude
  adfGeoTransform[1] = resolution;
  adfGeoTransform[2] = 0;
  adfGeoTransform[3] = tileBounds.getMaxY(); // max latitude
  adfGeoTransform[4] = 0;
  adfGeoTransform[5] = -resolution;

  GDALTile *tile = GDALTiler::createRasterTile(adfGeoTransform);

  // The previous geotransform represented the data with an overlap as required
  // by the terrain specification.  This now needs to be overwritten so that
  // the data is shifted to the bounds defined by tile itself.
  tileBounds = mGrid.tileBounds(coord);
  resolution = mGrid.resolution(coord.zoom);
  adfGeoTransform[0] = tileBounds.getMinX(); // min longitude
  adfGeoTransform[1] = resolution;
  adfGeoTransform[2] = 0;
  adfGeoTransform[3] = tileBounds.getMaxY(); // max latitude
  adfGeoTransform[4] = 0;
  adfGeoTransform[5] = -resolution;

  // Set the shifted geo transform to the VRT
  if (GDALSetGeoTransform(tile->dataset, adfGeoTransform) != CE_None) {
    throw CTBException("Could not set geo transform on VRT");
  }

  return tile;
}

TerrainTiler &
ctb::TerrainTiler::operator=(const TerrainTiler &other) {
  GDALTiler::operator=(other);

  return *this;
}


CRSBounds 
ctb::TerrainTiler::calBounds(const TileCoordinate& coord) const
{
    CRSBounds bounds;

    //地球周长，也就是webmercator的长宽。
    const double earthCircumference = 2.0 * M_PI * 6378137.0;
    double deltaX = earthCircumference / pow(2.0, coord.zoom);
    double deltaY = deltaX; // webmercator的瓦片是正方形
    double left = coord.x * deltaX - (earthCircumference * 0.5);
    double right = left + deltaX;
    double bottom = coord.y * deltaY - (earthCircumference * 0.5);
    double top = bottom + deltaY;

    bounds = CRSBounds(left, bottom, right, top); // set the bounds

    return bounds;
}


float* 
ctb::TerrainTiler::getMaskHeights(const TileCoordinate& tileCoord) const
{
    // 这里目前默认mask就是11级的 256*256的瓦片 terrain 格式 大小不可改
    int maskArraySize = 256 * 256;
    float* MaskHeightsArray = new float[maskArraySize];
    TileCoordinate maskCoordinate;
    maskCoordinate.zoom = 11;

    //ddx_degbug:
    unsigned short* debug_short_MaskHeightsArray = new unsigned short[maskArraySize];
    

    if (tileCoord.zoom > maskCoordinate.zoom)
    {
        int zoomDifference = tileCoord.zoom - maskCoordinate.zoom;
        maskCoordinate.x = tileCoord.x >> zoomDifference;
        maskCoordinate.y = tileCoord.y >> zoomDifference;
    }
    else
    {
        throw CTBException(" ddx_ErrorInfo: tileCoord.zoom < maskCoordinate.zoom");
    }

    // mask 的terrain缓存的地址:文件结构存储顺序是z/x/y 
    bool bUseWaterMask_debug = Terrain::bUseWaterMask;
    const char* maskPath_Debug = "F:/Heightmap_Terrain_Format/Terrain_Output/taiwan90mDEM_mask256/_alllayers/";//Terrain::WaterMaskDir.c_str()
    int maskZoom_debug = Terrain::WaterMaskZoom;
    const char* mask_path = Terrain::WaterMaskDir.c_str();
    // 使用字符串流来构建 maskTerrainPath
    std::stringstream maskTerrainPathStream;
    maskTerrainPathStream << mask_path << "/" << maskCoordinate.zoom << "/" << maskCoordinate.x << "/" << maskCoordinate.y << ".terrain";

    // 从字符串流中获取构建好的 maskTerrainPath
    std::string maskTerrainPath = maskTerrainPathStream.str();
    // 打开二进制文件
    FILE* maskFile = fopen(maskTerrainPath.c_str(), "rb");

    // 检查文件是否成功打开
    if (!maskFile) {
        std::string errorMessage = "ErrorInfo:  Failed to open the mask terrain file , maskFilePath: " + maskTerrainPath;
        throw CTBException(errorMessage.c_str());
    }

    // 获取文件大小
    fseek(maskFile, 0, SEEK_END);       //将文件指针（在这里是 maskFile）移动到文件的末尾
    long fileSize = ftell(maskFile);    //使用 ftell 函数获取当前文件指针的位置，计算大小
    fseek(maskFile, 0, SEEK_SET);       //将文件指针重新移动到文件的开头

    // 分配缓冲区来存储文件内容
    unsigned char* buffer = (unsigned char*)malloc(fileSize);

    // 读取文件内容到缓冲区
    if (fread(buffer, 1, fileSize, maskFile) != fileSize) {
        throw CTBException(" ddx_ErrorInfo: Failed to read the  mask terrain file.");
    }

    // 关闭文件
    fclose(maskFile);

    ////ddx_degbug:调试过程中把直接读取的每个值也记录下来
    //unsigned char* ucharBuffer = new unsigned char[fileSize];
    //for (long i = 0; i < fileSize; i++) {
    //    ucharBuffer[i] = static_cast<unsigned char>(buffer[i]);
    //}


    // 这里假设每两个字节表示一个16位的有符号短整数，且数据是小端字节序
    // 这里的for循环直接用maskArraySize（即256*256）是一种偷懒的做法，应该 fileSize / sizeof(unsigned short) 但这个会多一位child和水掩码
    int fileSizeofArray = fileSize / sizeof(unsigned short); // 65537
    for (int i = 0; i < maskArraySize; i++) { 
        // 计算当前要解析的字节偏移量
        size_t byteOffset = i * 2;

        // 使用位操作将两个字节合并为一个16位整数（小端字节序）
        unsigned short value = (unsigned short)(buffer[byteOffset] | (buffer[byteOffset + 1] << 8));
        short signedShortValue = (short)(buffer[byteOffset] | (buffer[byteOffset + 1] << 8));
        
        ////ddx_degbug:调试过程中把高度图还原前的每个值也记录下来
        //debug_short_MaskHeightsArray[i] = value;
        //debug_short_MaskHeightsArray[i] = signedShortValue;

        // 将解析后的值存储到 MaskHeightsArray （/5 -1000）转换成真实的高度数据，得到一个65535的数组
        //MaskHeightsArray[i] = static_cast<float>((value / 5) - 1000);
        MaskHeightsArray[i] = static_cast<float>((signedShortValue / 5) - 1000);
    }

    //切记防止内存泄漏
    delete[] buffer;
    delete[] debug_short_MaskHeightsArray; // 如果不再需要可以释放

    return MaskHeightsArray;
}