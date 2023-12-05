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
  // Get a terrain tile represented by the tile coordinate ��ȡ����Ƭ�����ʾ�ĵ�����Ƭ
  TerrainTile *terrainTile = new TerrainTile(coord);
  GDALTile *rasterTile = createRasterTile(coord); // the raster associated with this tile coordinate
  GDALRasterBand *heightsBand = rasterTile->dataset->GetRasterBand(1);

  // Copy the raster data into an array ��դ�����ݸ��Ƶ�������
  float rasterHeights[TerrainTile::TILE_CELL_SIZE];
  if (heightsBand->RasterIO(GF_Read, 0, 0, TILE_SIZE, TILE_SIZE,
                            (void *) rasterHeights, TILE_SIZE, TILE_SIZE, GDT_Float32,
                            0, 0) != CE_None) {
   // throw CTBException("Could not read heights from raster");
	  for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
		  rasterHeights[i] = (i_terrain_height)(0);
          //unsigned short  �����65535 TILE_CELL_SIZE65536Խ���ˣ�Խ��֮���ֱ��0���Ի���ѭ�����ĳ�int�Ϳ�����
          //if (i == 65535)
          //{
          //    int temp = i;
          //    printf("i = %d",i);
          //}
	  }
  }

  delete rasterTile;

  // Convert the raster data into the terrain tile heights.  This assumes the ��դ������ת��Ϊ����ͼ��߶�
  // input raster data represents meters above sea level. Each terrain height ���������դ�����ݱ�ʾ��������
  //value is the number of 1/5 meter units above -1000 meters. �����θ߶�ֵ�� -1000 ������ 1/5 �׵�λ��������
  // TODO: try doing this using a VRT derived band:  ����ʹ�� VRT ����Ƶ��ִ�д˲�����
  // (http://www.gdal.org/gdal_vrttut.html)
  //for (unsigned int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
  //unsigned short  �����65535 TILE_CELL_SIZE ��256*256�Ļ�=65536Խ���ˣ�Խ��֮���ֱ��0���Ի���ѭ�����ĳ�int�Ϳ�����

	 // for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
  //  terrainTile->mHeights[i] = (i_terrain_height) ((rasterHeights[i] + 1000) * 5);
  //}

  //ddx:�ں�256*256����Ĥ����
  int maskZoom = Terrain::WaterMaskZoom;  // 11;

  //ddx_degbug:���Թ����а�ֱ�Ӷ�ȡ��ÿ��ֵҲ��¼����
  float* HeightBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* oriBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* preasBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* signedShortBuffer = new float[TerrainTile::TILE_CELL_SIZE];
  float* preasSignedShortBuffer = new float[TerrainTile::TILE_CELL_SIZE];

  if (coord.zoom < maskZoom  || !Terrain::bUseWaterMask)   //���Ҫ�еĲ㼶����Ĥ��Ҫ�󣬻���û��������Ч����Ĥ����  ��ִ��ԭ�����߼�
  {
      ////���if������С��maskZoom�������������ǲ�������
      //float max = 0, min = 0;
      //  for (int i = 0; i < TerrainTile::TILE_CELL_SIZE; i++) {
      //    terrainTile->mHeights[i] = (i_terrain_height) ((rasterHeights[i] + 1000) * 5);

      //    //debug��

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

      //  //debug���������������Բ���
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
      //���Ȼ�ȡ������Ĥ��Ƭ�ĸ߶�������Ϣ 256*256
      float* maskHeights = nullptr;
      
      //ddx_debug:
      int maskArraySize = TerrainTile::MASK_CELL_SIZE;//256 * 256;
      float* oriHeights_Debug = new float[maskArraySize];
      float* ddxHeights_Debug = new float[maskArraySize];

      maskHeights = ctb::TerrainTiler::getMaskHeights(coord); 

      //Ȼ��ֱ���㺣����Ĥ�͵�ǰ��Ƭ��bounds��webmercator ͶӰ��
      TileCoordinate maskCoordinate;
      int maskTileSize = MASK_SIZE; //256
      maskCoordinate.zoom = Terrain::WaterMaskZoom; //11
      int zoomDifference = coord.zoom - maskCoordinate.zoom;
      maskCoordinate.x = coord.x >> zoomDifference;
      maskCoordinate.y = coord.y >> zoomDifference;

      CRSBounds maskBounds = calBounds(maskCoordinate);
      CRSBounds tileBounds = calBounds(coord);
      
      //����Ӱ������Σ������൱���ǵ���Ӱ���൱���Ǹ��Ӽ������4*4�ĵ����൱�ڷ���������Ƭ��������Ǽ�¼�����ĸ��ָ���λ��
      //����:x��y�����ϣ���ǰ��Ƭ ��λ����������ľ��볤�ȣ�webmercator���꣩ ��λ�ǣ�m/����
      float scaleFactorX = (tileBounds.getMaxX() - tileBounds.getMinX()) / (TILE_SIZE - 1);
      float scaleFactorY = (tileBounds.getMaxY() - tileBounds.getMinY()) / (TILE_SIZE - 1);

      //mask��Ƭ��     ��������webmercator����ĵ�λ���� �൱�ڶ��ٸ�����,��λ��(����/m)
      double dCacheRatioX = (maskTileSize - 1) / (maskBounds.getMaxX() - maskBounds.getMinX());
      double dCacheRatioY = (maskTileSize - 1) / (maskBounds.getMaxY() - maskBounds.getMinY());

      //���в�ֵ
      for (int i = 0; i < TILE_SIZE; i++)       //i �м�ѭ�� ��Ӧγ�� Latitude
      {
          for (int j = 0; j < TILE_SIZE; j++)       //j�м�ѭ�� ��Ӧ���� Longitude
          {
              // ����һ�� OGRPoint �����ʾҪ�����Ķ�ά��   ����˳������ң����ϵ���
              OGRPoint pointPos(tileBounds.getMinX() + scaleFactorX * j , tileBounds.getMaxY() - scaleFactorY * i);
              //����tile�ϵ�ǰ��������mask�ϵ����кţ�δȡ����
              float fCol = (pointPos.getX() - maskBounds.getMinX()) * dCacheRatioX;
              float fRow = (maskBounds.getMaxY() - pointPos.getY()) * dCacheRatioY;

              int nMinRow = (int)fRow;
              int nMaxRow = (int)std::ceil(fRow);
              int nMinCol = (int)fCol;
              int nMaxCol = (int)std::ceil(fCol);

              //��ֹ���к���Ϊ�����Խ��
              if (nMinRow >= maskTileSize)
                  nMinRow = maskTileSize - 1;
              if (nMaxRow >= maskTileSize)
                  nMaxRow = maskTileSize - 1;
              if (nMinCol >= maskTileSize)
                  nMinCol = maskTileSize - 1;
              if (nMaxCol >= maskTileSize)
                  nMaxCol = maskTileSize - 1;

              //��ֹ���к���Ϊ��С��Խ��
              if (nMinRow < 0)
                  nMinRow = 0;
              if (nMaxRow < 0)
                  nMaxRow = 0;
              if (nMinCol < 0)
                  nMinCol = 0;
              if (nMaxCol < 0)
                  nMaxCol = 0;
              
              //˫���Բ�ֵ
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
      //        //����0Ĭ����ˮ���ϣ���������
      //        terrainTile->mHeights[i] = (i_terrain_height)((rasterHeights[i] + 1000) * 5);
      //    }
      //    else
      //    {
      //        //С��0��Ϊ��ˮ�£���Ҫ��ֵ����ֵ���ΪrasterHeightValue
      //        float rasterHeightValue = 0;
      //        //todo:
      //        //1.�������ڵ�coord��x y zoom�������ǰ��Ƭ��mask��11�����ϵ�x y zoom,�ҵ���Ӧ��Ƭ���򿪽��� ��batyת����shortȻ��ǵã�/5-1000��ת������ʵ�ĸ߶����ݣ��õ�һ��65535������
      //        //2.�������ڵ����65*65��bounds:��֪webī����ͶӰ�������ο��Ϊ20037508.3427892*2
      //        // dx=20037508.3427892*2/��2^coord.zoom��
      //        //left=coord.x * dx
      //        //right = left + dx
      //        //buttom = coord.y * dx
      //        //top = buttom + dx
      //        //���㵱ǰ��Ƭ��bound�� ��Ӧ��bound  ��ӦnTileSize����65  pntPos��65*65��Ƭ�ϵ�ÿ����Ƭ�����ĵ��webī��������
      //        //nCacheWidth��rcCacheBounds.Widthһ����webī����ͶӰ�µĿ��һ������Ƭ�������ȸ���
      //        //double dCacheRatioX = (nCacheWidth - 1) / rcCacheBounds.Width();��ÿ���Ӷ�����ǵ�λ���ռ���ٸ�����
      //        //dCacheRatioX�ǵ�λ���ռ���ٸ����ӣ�����
      //        //nCacheWidthrc����Ƭ�������ȸ���CacheBounds.Width��webī����ͶӰ�µĿ�� 
      //        terrainTile->mHeights[i] = (i_terrain_height)((rasterHeightValue + 1000) * 5);
      //    }
      //}

  }

  // If we are not at the maximum zoom level we need to set child flags on the  �������û�дﵽ������ż���������Ҫ��
  // tile where child tiles overlap the dataset bounds.  ��ͼ�������ݼ��߽��ص���ͼ�顣
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
  // Ensure we have some data from which to create a tile  ȷ��������һЩ���ڴ���ͼ�������
  if (poDataset && poDataset->GetRasterCount() < 1) {
    throw CTBException("At least one band must be present in the GDAL dataset");
  }

  // Get the bounds and resolution for a tile coordinate which represents the  ��ȡ������Ƭ����ı߽�ͷֱ���
  // data overlap requested by the terrain specification.  ���ι淶Ҫ��������ص���
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

    //�����ܳ���Ҳ����webmercator�ĳ���
    const double earthCircumference = 2.0 * M_PI * 6378137.0;
    double deltaX = earthCircumference / pow(2.0, coord.zoom);
    double deltaY = deltaX; // webmercator����Ƭ��������
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
    // ����ĿǰĬ��mask����11���� 256*256����Ƭ terrain ��ʽ ��С���ɸ�
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

    // mask ��terrain����ĵ�ַ:�ļ��ṹ�洢˳����z/x/y 
    bool bUseWaterMask_debug = Terrain::bUseWaterMask;
    const char* maskPath_Debug = "F:/Heightmap_Terrain_Format/Terrain_Output/taiwan90mDEM_mask256/_alllayers/";//Terrain::WaterMaskDir.c_str()
    int maskZoom_debug = Terrain::WaterMaskZoom;
    const char* mask_path = Terrain::WaterMaskDir.c_str();
    // ʹ���ַ����������� maskTerrainPath
    std::stringstream maskTerrainPathStream;
    maskTerrainPathStream << mask_path << "/" << maskCoordinate.zoom << "/" << maskCoordinate.x << "/" << maskCoordinate.y << ".terrain";

    // ���ַ������л�ȡ�����õ� maskTerrainPath
    std::string maskTerrainPath = maskTerrainPathStream.str();
    // �򿪶������ļ�
    FILE* maskFile = fopen(maskTerrainPath.c_str(), "rb");

    // ����ļ��Ƿ�ɹ���
    if (!maskFile) {
        std::string errorMessage = "ErrorInfo:  Failed to open the mask terrain file , maskFilePath: " + maskTerrainPath;
        throw CTBException(errorMessage.c_str());
    }

    // ��ȡ�ļ���С
    fseek(maskFile, 0, SEEK_END);       //���ļ�ָ�루�������� maskFile���ƶ����ļ���ĩβ
    long fileSize = ftell(maskFile);    //ʹ�� ftell ������ȡ��ǰ�ļ�ָ���λ�ã������С
    fseek(maskFile, 0, SEEK_SET);       //���ļ�ָ�������ƶ����ļ��Ŀ�ͷ

    // ���仺�������洢�ļ�����
    unsigned char* buffer = (unsigned char*)malloc(fileSize);

    // ��ȡ�ļ����ݵ�������
    if (fread(buffer, 1, fileSize, maskFile) != fileSize) {
        throw CTBException(" ddx_ErrorInfo: Failed to read the  mask terrain file.");
    }

    // �ر��ļ�
    fclose(maskFile);

    ////ddx_degbug:���Թ����а�ֱ�Ӷ�ȡ��ÿ��ֵҲ��¼����
    //unsigned char* ucharBuffer = new unsigned char[fileSize];
    //for (long i = 0; i < fileSize; i++) {
    //    ucharBuffer[i] = static_cast<unsigned char>(buffer[i]);
    //}


    // �������ÿ�����ֽڱ�ʾһ��16λ���з��Ŷ���������������С���ֽ���
    // �����forѭ��ֱ����maskArraySize����256*256����һ��͵����������Ӧ�� fileSize / sizeof(unsigned short) ��������һλchild��ˮ����
    int fileSizeofArray = fileSize / sizeof(unsigned short); // 65537
    for (int i = 0; i < maskArraySize; i++) { 
        // ���㵱ǰҪ�������ֽ�ƫ����
        size_t byteOffset = i * 2;

        // ʹ��λ�����������ֽںϲ�Ϊһ��16λ������С���ֽ���
        unsigned short value = (unsigned short)(buffer[byteOffset] | (buffer[byteOffset + 1] << 8));
        short signedShortValue = (short)(buffer[byteOffset] | (buffer[byteOffset + 1] << 8));
        
        ////ddx_degbug:���Թ����аѸ߶�ͼ��ԭǰ��ÿ��ֵҲ��¼����
        //debug_short_MaskHeightsArray[i] = value;
        //debug_short_MaskHeightsArray[i] = signedShortValue;

        // ���������ֵ�洢�� MaskHeightsArray ��/5 -1000��ת������ʵ�ĸ߶����ݣ��õ�һ��65535������
        //MaskHeightsArray[i] = static_cast<float>((value / 5) - 1000);
        MaskHeightsArray[i] = static_cast<float>((signedShortValue / 5) - 1000);
    }

    //�мǷ�ֹ�ڴ�й©
    delete[] buffer;
    delete[] debug_short_MaskHeightsArray; // ���������Ҫ�����ͷ�

    return MaskHeightsArray;
}