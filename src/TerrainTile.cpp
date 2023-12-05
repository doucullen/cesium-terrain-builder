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
  * @file TerrainTile.cpp
  * @brief This defines the `Terrain` and `TerrainTile` classes
  */

#include <string.h>             // for memcpy

#include "zlib.h"
#include "ogr_spatialref.h"

#include "CTBException.hpp"
#include "TerrainTile.hpp"
#include "GlobalGeodetic.hpp"
#include "Bounds.hpp"


using namespace ctb;

bool Terrain::Compress = false;
bool Terrain::bUseWaterMask = false;
std::string Terrain::WaterMaskDir = "";
int Terrain::WaterMaskZoom = 11;
bool Terrain::bUseMbtiles = true;

Terrain::Terrain() :
	mHeights(TILE_CELL_SIZE),
	mChildren(0)
{
	setIsLand();
}

/**
 * @details This reads gzipped terrain data from a file.
 */
Terrain::Terrain(const char* fileName) :
	mHeights(TILE_CELL_SIZE)
{
	if (Terrain::Compress)
	{
		readFile(fileName);
	}
	else
	{
		FILE* fp = fopen(fileName, "rb");
		if (fp != NULL)
		{
			readFile(fp);
			fclose(fp);
			fp = NULL;
		}
	}
}

/**
 * @details This reads raw uncompressed terrain data from a file handle.
 */
Terrain::Terrain(FILE* fp) :
	mHeights(TILE_CELL_SIZE)
{
	unsigned char bytes[2];
	int count = 0;

	// Get the height data from the file handle
	while (count < TILE_CELL_SIZE && fread(bytes, 2, 1, fp) != 0) {
		/* adapted from
		   <http://stackoverflow.com/questions/13001183/how-to-read-little-endian-integers-from-file-in-c> */
		mHeights[count++] = bytes[0] | (bytes[1] << 8);
	}

	// Check we have the expected amound of height data
	if (count + 1 != TILE_CELL_SIZE) {
		throw CTBException("Not enough height data");
	}

	// Get the child flag
	if (fread(&(mChildren), 1, 1, fp) != 1) {
		throw CTBException("Could not read child tile byte");
	}

	// Get the water mask
	mMaskLength = fread(mMask, 1, MASK_CELL_SIZE, fp);
	switch (mMaskLength) {
	case MASK_CELL_SIZE:
		break;
	case 1:
		break;
	default:
		throw CTBException("Not contain enough water mask data");
	}
}

void Terrain::readFile(FILE* fp)
{
	unsigned char bytes[2];
	int count = 0;

	// Get the height data from the file handle
	while (count < TILE_CELL_SIZE && fread(bytes, 2, 1, fp) != 0) {
		/* adapted from
		<http://stackoverflow.com/questions/13001183/how-to-read-little-endian-integers-from-file-in-c> */
		mHeights[count++] = bytes[0] | (bytes[1] << 8);
	}

	// Check we have the expected amound of height data
	if (count + 1 != TILE_CELL_SIZE) {
		throw CTBException("Not enough height data");
	}

	// Get the child flag
	if (fread(&(mChildren), 1, 1, fp) != 1) {
		throw CTBException("Could not read child tile byte");
	}

	// Get the water mask
	mMaskLength = fread(mMask, 1, MASK_CELL_SIZE, fp);
	switch (mMaskLength) {
	case MASK_CELL_SIZE:
		break;
	case 1:
		break;
	default:
		throw CTBException("Not contain enough water mask data");
	}
}

/**
 * @details This reads gzipped terrain data from a file.
 */
void
Terrain::readFile(const char* fileName) {
	unsigned char inflateBuffer[MAX_TERRAIN_SIZE];
	unsigned int inflatedBytes;
	gzFile terrainFile = gzopen(fileName, "rb");

	if (terrainFile == NULL) {
		throw CTBException("Failed to open file");
	}

	// Uncompress the file into the buffer
	inflatedBytes = gzread(terrainFile, inflateBuffer, MAX_TERRAIN_SIZE);
	if (gzread(terrainFile, inflateBuffer, 1) != 0) {
		gzclose(terrainFile);
		throw CTBException("File has too many bytes to be a valid terrain");
	}
	gzclose(terrainFile);

	// Check the water mask type
	switch (inflatedBytes) {
	case MAX_TERRAIN_SIZE:      // a water mask is present
		mMaskLength = MASK_CELL_SIZE;
		break;
	case (TILE_CELL_SIZE * 2) + 2:   // there is no water mask
		mMaskLength = 1;
		break;
	default:                    // it can't be a terrain file
		throw CTBException("File has wrong file size to be a valid terrain");
	}

	// Get the height data
	short int byteCount = 0;
	for (short int i = 0; i < TILE_CELL_SIZE; i++, byteCount = i * 2) {
		mHeights[i] = inflateBuffer[byteCount] | (inflateBuffer[byteCount + 1] << 8);
	}

	// Get the child flag
	mChildren = inflateBuffer[byteCount]; // byte 8451

	// Get the water mask
	memcpy(mMask, &(inflateBuffer[++byteCount]), mMaskLength);
}

/**
 * @details This writes raw uncompressed terrain data to a filehandle.
 */
void
Terrain::writeFile(FILE* fp) const {
	fwrite(mHeights.data(), TILE_CELL_SIZE * 2, 1, fp);
	fwrite(&mChildren, 1, 1, fp);
	fwrite(mMask, mMaskLength, 1, fp);
}

/**
 * 要写mbtiles的二进制
 */
std::vector<uint8_t> Terrain::writeMBTIlesFile() const {

	std::vector<uint8_t> buffer;
	
	// 将 mHeights 写入二进制缓冲区
	buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(mHeights.data()), reinterpret_cast<const uint8_t*>(mHeights.data() + mHeights.size()));
	//buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(mHeights.data()), reinterpret_cast<const uint8_t*>(mHeights.data() + TILE_CELL_SIZE * 2));

	// 将 mChildren 写入二进制缓冲区
	buffer.push_back(mChildren);

	// 将 mMask 写入二进制缓冲区
	buffer.insert(buffer.end(), mMask, mMask + mMaskLength);

	// 获取 vector 元素数量
	size_t elementsCount = buffer.size();
	// 获取 vector 元素占用的内存大小
	size_t elementsSize = buffer.size() * sizeof(uint8_t);
	// 获取 vector 对象本身占用的内存大小
	size_t vectorSize = sizeof(buffer);
	// 获取 vector 总共占用的内存大小
	size_t totalSize = vectorSize + elementsSize;

	return buffer;
}

/**
 * @details This writes gzipped terrain data to a file.
 */
void
Terrain::writeFile(const char* fileName) const {
	gzFile terrainFile = gzopen(fileName, "wb");

	if (terrainFile == NULL) {
		throw CTBException("Failed to open file");
	}

	// Write the height data
	if (gzwrite(terrainFile, mHeights.data(), TILE_CELL_SIZE * 2) == 0) {
		gzclose(terrainFile);
		throw CTBException("Failed to write height data");
	}

	// Write the child flags
	if (gzputc(terrainFile, mChildren) == -1) {
		gzclose(terrainFile);
		throw CTBException("Failed to write child flags");
	}

	// Write the water mask
	if (gzwrite(terrainFile, mMask, mMaskLength) == 0) {
		gzclose(terrainFile);
		throw CTBException("Failed to write water mask");
	}

	// Try and close the file
	switch (gzclose(terrainFile)) {
	case Z_OK:
		break;
	case Z_STREAM_ERROR:
	case Z_ERRNO:
	case Z_MEM_ERROR:
	case Z_BUF_ERROR:
	default:
		throw CTBException("Failed to close file");
	}
}

std::vector<bool>
Terrain::mask() {
	std::vector<bool> mask;
	mask.assign(mMask, mMask + mMaskLength);
	return mask;
}

bool
Terrain::hasChildren() const {
	return mChildren;
}

bool
Terrain::hasChildSW() const {
	return ((mChildren & TERRAIN_CHILD_SW) == TERRAIN_CHILD_SW);
}

bool
Terrain::hasChildSE() const {
	return ((mChildren & TERRAIN_CHILD_SE) == TERRAIN_CHILD_SE);
}

bool
Terrain::hasChildNW() const {
	return ((mChildren & TERRAIN_CHILD_NW) == TERRAIN_CHILD_NW);
}

bool
Terrain::hasChildNE() const {
	return ((mChildren & TERRAIN_CHILD_NE) == TERRAIN_CHILD_NE);
}

void
Terrain::setChildSW(bool on) {
	if (on) {
		mChildren |= TERRAIN_CHILD_SW;
	}
	else {
		mChildren &= ~TERRAIN_CHILD_SW;
	}
}

void
Terrain::setChildSE(bool on) {
	if (on) {
		mChildren |= TERRAIN_CHILD_SE;
	}
	else {
		mChildren &= ~TERRAIN_CHILD_SE;
	}
}

void
Terrain::setChildNW(bool on) {
	if (on) {
		mChildren |= TERRAIN_CHILD_NW;
	}
	else {
		mChildren &= ~TERRAIN_CHILD_NW;
	}
}

void
Terrain::setChildNE(bool on) {
	if (on) {
		mChildren |= TERRAIN_CHILD_NE;
	}
	else {
		mChildren &= ~TERRAIN_CHILD_NE;
	}
}

void
Terrain::setAllChildren(bool on) {
	if (on) {
		mChildren = TERRAIN_CHILD_SW | TERRAIN_CHILD_SE | TERRAIN_CHILD_NW | TERRAIN_CHILD_NE;
	}
	else {
		mChildren = 0;
	}
}

void
Terrain::setIsWater() {
	mMask[0] = 1;
	mMaskLength = 1;
}

bool
Terrain::isWater() const {
	return mMaskLength == 1 && (bool)mMask[0];
}

void
Terrain::setIsLand() {
	mMask[0] = 0;
	mMaskLength = 1;
}

bool
Terrain::isLand() const {
	return mMaskLength == 1 && !(bool)mMask[0];
}

bool
Terrain::hasWaterMask() const {
	return mMaskLength == MASK_CELL_SIZE;
}

const std::vector<i_terrain_height>&
Terrain::getHeights() const {
	return mHeights;
}

/**
 * @details The data in the returned vector can be altered but do not alter
 * the number of elements in the vector.
 */
std::vector<i_terrain_height>&
Terrain::getHeights() {
	return mHeights;
}

char ctb::Terrain::getChildren() const
{
	return mChildren;
}

std::vector<char> ctb::Terrain::getMask() const
{
	std::vector<char> mask;
	for (int i = 0; i < mMaskLength; ++i)
		mask.push_back(mMask[i]);
	return mask;
}

TerrainTile::TerrainTile(const TileCoordinate& coord) :
	Terrain(),
	Tile(coord)
{}

TerrainTile::TerrainTile(const char* fileName, const TileCoordinate& coord) :
	Terrain(fileName),
	Tile(coord)
{}

TerrainTile::TerrainTile(const Terrain& terrain, const TileCoordinate& coord) :
	Terrain(terrain),
	Tile(coord)
{}

GDALDatasetH
TerrainTile::heightsToRaster() const {
	// Create the geo transform for this raster tile
	const GlobalGeodetic profile;
	const CRSBounds tileBounds = profile.tileBounds(*this);
	const i_tile tileSize = profile.tileSize();
	const double resolution = tileBounds.getWidth() / tileSize;
	double adfGeoTransform[6] = {
	  tileBounds.getMinX(),
	  resolution,
	  0,
	  tileBounds.getMaxY(),
	  0,
	  -resolution
	};

	// Create the spatial reference system for the raster
	OGRSpatialReference oSRS;
	if (oSRS.importFromEPSG(4326) != OGRERR_NONE) {
		throw CTBException("Could not create EPSG:4326 spatial reference");
	}

	char* pszDstWKT = NULL;
	if (oSRS.exportToWkt(&pszDstWKT) != OGRERR_NONE) {
		CPLFree(pszDstWKT);
		throw CTBException("Could not create EPSG:4326 WKT string");
	}

	// Create an 'In Memory' raster
	GDALDriverH hDriver = GDALGetDriverByName("MEM");
	GDALDatasetH hDstDS;
	GDALRasterBandH hBand;

	hDstDS = GDALCreate(hDriver, "", tileSize, tileSize, 1, GDT_Int16, NULL);
	if (hDstDS == NULL) {
		CPLFree(pszDstWKT);
		throw CTBException("Could not create in memory raster");
	}

	// Set the projection
	if (GDALSetProjection(hDstDS, pszDstWKT) != CE_None) {
		GDALClose(hDstDS);
		CPLFree(pszDstWKT);
		throw CTBException("Could not set projection on in memory raster");
	}
	CPLFree(pszDstWKT);

	// Apply the geo transform
	if (GDALSetGeoTransform(hDstDS, adfGeoTransform) != CE_None) {
		GDALClose(hDstDS);
		throw CTBException("Could not set projection on VRT");
	}

	// Finally write the height data
	hBand = GDALGetRasterBand(hDstDS, 1);
	if (GDALRasterIO(hBand, GF_Write, 0, 0, tileSize, tileSize,
		(void*)mHeights.data(), tileSize, tileSize, GDT_Int16, 0, 0) != CE_None) {
		GDALClose(hDstDS);
		throw CTBException("Could not write heights to in memory raster");
	}

	return hDstDS;
}

void Singleton::CreatMBTiles(const std::string& filename)
{
	// 先锁定互斥锁，确保只有一个线程能够访问文件写入部分
	std::lock_guard<std::mutex> lock(this->fileMutex);

	this->db = NULL;
	this->filename = filename;
	int rc = sqlite3_open(filename.c_str(), &this->db);
	rc = sqlite3_exec(
		db,
		"CREATE TABLE metadata (name text, value text);",
		NULL, NULL, NULL);
	rc = sqlite3_exec(
		db,
		"CREATE TABLE tiles "
		"(zoom_level integer, tile_column integer, "
		"tile_row integer, tile_data blob);",
		NULL, NULL, NULL);
	// 退出作用域时，lock_guard 将自动解锁互斥锁
}

bool Singleton::WriteTileMetadata()
{
	// 先锁定互斥锁，确保只有一个线程能够访问文件写入部分
	std::lock_guard<std::mutex> lock(this->fileMutex);

	//声明一个 SQLite 语句对象，用于执行 SQL 语句
	sqlite3_stmt* meta_stmt = NULL;

	std::map<std::string, std::string> metadata;
	metadata["tilejson"] = "2.1.0";
	metadata["format"] = "heightmap - 1.0";
	metadata["version"] = "1.0.0";
	metadata["scheme"] = "tms";
	metadata["tiles"] = "[\" (z) / (x) / y)terrain ? y = [version) \"]";


	int rc = sqlite3_prepare_v2(this->db, "INSERT INTO metadata ""(name, value) ""VALUES (?1, ?2);", -1, &meta_stmt, NULL);

	for (auto rowInfo : metadata)
	{
		//sqlite3_bind_text：这是 SQLite 提供的函数，用于将文本数据绑定到 SQL 预备语句的参数中。
		//SQLITE_STATIC：这个参数指示 SQLite 不需要复制传递的字符串。这意味着在预备语句执行之后，数据仍然由调用者管理
		rc = sqlite3_bind_text(meta_stmt, 1, rowInfo.first.c_str(), -1, SQLITE_STATIC);
		rc = sqlite3_bind_text(meta_stmt, 2, rowInfo.second.c_str(), -1, SQLITE_STATIC);

		//sqlite3_step：这是 SQLite 提供的函数，用于执行 SQL 预备语句。执行一次 sqlite3_step 相当于执行一次 SQL 语句。如果该函数返回 SQLITE_DONE，则表示执行成功，否则可能会返回不同的错误代码。
		rc = sqlite3_step(meta_stmt);

		//sqlite3_reset: 这是 SQLite 提供的函数，用于将 SQL 预备语句恢复到初始状态，以便可以重新执行它。重置后，预备语句可以再次使用。
		rc = sqlite3_reset(meta_stmt);
	}

	//sqlite3_finalize 函数用于销毁一个 SQL 预备语句并释放相关资源
	rc = sqlite3_finalize(meta_stmt);

	// 退出作用域时，lock_guard 将自动解锁互斥锁
	return true;
}


bool Singleton::WriteTile(unsigned int zoomLevel, unsigned int tileColumn, unsigned int tileRow, const std::vector<uint8_t>& blobOut)
{
	// 先锁定互斥锁，确保只有一个线程能够访问文件写入部分
	std::lock_guard<std::mutex> lock(this->fileMutex);

	//1.声明一个 SQLite 语句对象
	sqlite3_stmt* tile_stmt = NULL;
	//2.SQL 插入语句 目标:tiles 分别是z x y file对应的占位符1 2 3 4 ，自动计算SQ语句长度-1 ， SQ语句对象tile_stmt , 无回调函数
	int rc = sqlite3_prepare_v2(this->db, "INSERT INTO tiles ""(zoom_level, tile_column, tile_row, tile_data) ""VALUES (?1, ?2, ?3, ?4);", -1, &tile_stmt, NULL);
	//3.重置tile_stmt
	rc = sqlite3_reset(tile_stmt);
	//4.绑定数值和二进制分别到对应的占位符1 2 3 和 4中去。
	rc = sqlite3_bind_int(tile_stmt, 1, zoomLevel);
	rc = sqlite3_bind_int(tile_stmt, 2, tileColumn);
	rc = sqlite3_bind_int(tile_stmt, 3, tileRow);
	rc = sqlite3_bind_blob(tile_stmt, 4, blobOut.data(), static_cast<int>(blobOut.size()), SQLITE_STATIC);
	// 5.sqlite3_step 函数用于逐步执行准备好的 SQL 语句，返回执行的结果或状态。
	rc = sqlite3_step(tile_stmt);
	//6.于销毁一个 SQL语句并释放相关资源
	sqlite3_finalize(tile_stmt);

	// 退出作用域时，lock_guard 将自动解锁互斥锁
	return true;
}


