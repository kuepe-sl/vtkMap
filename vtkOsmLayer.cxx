/*=========================================================================

  Program:   Visualization Toolkit

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

   This software is distributed WITHOUT ANY WARRANTY; without even
   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOsmLayer.h"

#include "vtkMercator.h"
#include "vtkMapTile.h"

#include <vtkObjectFactory.h>
#include <vtksys/SystemTools.hxx>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>

#include <vtkCamera.h>
#include <vtkPerspectiveTransform.h>

#include <algorithm>
#include <cstring>  // strdup
#include <iomanip>
#include <iterator>
#include <math.h>
#include <sstream>

vtkStandardNewMacro(vtkOsmLayer)

//----------------------------------------------------------------------------
struct sortTiles
{
  inline bool operator() (vtkMapTile* tile1,  vtkMapTile* tile2)
  {
    return (tile1->GetBin() < tile2->GetBin());
  }
};

//----------------------------------------------------------------------------
vtkOsmLayer::vtkOsmLayer() : vtkFeatureLayer()
{
  this->BaseOn();
  this->MapTileServer = strdup("tile.openstreetmap.org");
  this->MapTileExtension = strdup("png");
  this->MapTileAttribution = strdup("(c) OpenStreetMap contributors");
  this->AttributionActor = NULL;
  this->CacheDirectory = NULL;
}

//----------------------------------------------------------------------------
vtkOsmLayer::~vtkOsmLayer()
{
  if (this->AttributionActor)
    {
    this->AttributionActor->Delete();
    }
  this->RemoveTiles();
  free(this->CacheDirectory);
  free(this->MapTileAttribution);
  free(this->MapTileExtension);
  free(this->MapTileServer);
}

//----------------------------------------------------------------------------
void vtkOsmLayer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkOsmLayer::
SetMapTileServer(const char *server, const char *attribution,
                 const char *extension)
{
  if (!this->Map)
    {
    vtkWarningMacro("Cannot set map-tile server before adding layer to vtkMap");
    return;
    }

  // Set cache directory
  // Do *not* use SystemTools::JoinPath(), because it omits the first slash
  std::string fullPath = this->Map->GetStorageDirectory() + std::string("/")
    + server;

  // Create directory if it doesn't already exist
  if(!vtksys::SystemTools::FileIsDirectory(fullPath.c_str()))
    {
    if (vtksys::SystemTools::MakeDirectory(fullPath.c_str()))
      {
      std::cerr << "Created map-tile cache directory "
                << fullPath << std::endl;
      }
    else
      {
      vtkErrorMacro("Unable to create directory for map-tile cache: "
                    << fullPath);
      return;
      }
    }

  // Clear tile cached and update internals
  // Remove tiles from renderer before calling RemoveTiles()
  std::vector<vtkMapTile*>::iterator iter = this->CachedTiles.begin();
  for (; iter != this->CachedTiles.end(); iter++)
    {
    vtkMapTile *tile = *iter;
    this->Renderer->RemoveActor(tile->GetActor());
    }
  this->RemoveTiles();

  this->MapTileExtension = strdup(extension);
  this->MapTileServer = strdup(server);
  this->MapTileAttribution = strdup(attribution);
  this->CacheDirectory = strdup(fullPath.c_str());

  if (this->AttributionActor)
    {
    this->AttributionActor->SetInput(this->MapTileAttribution);
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkOsmLayer::Update()
{
  if (!this->Map)
    {
    return;
    }

  if (!this->CacheDirectory)
    {
    this->SetMapTileServer(
      this->MapTileServer, this->MapTileAttribution, this->MapTileExtension);
    }

  if (!this->AttributionActor && this->MapTileAttribution)
    {
    this->AttributionActor = vtkTextActor::New();
    this->AttributionActor->SetInput(this->MapTileAttribution);
    this->AttributionActor->SetDisplayPosition(10, 0);
    vtkTextProperty *textProperty = this->AttributionActor->GetTextProperty();
    textProperty->SetFontSize(12);
    textProperty->SetFontFamilyToArial();
    textProperty->SetJustificationToLeft();
    textProperty->SetColor(0, 0, 0);
    // Background properties available in vtk 6.2
    //textProperty->SetBackgroundColor(1, 1, 1);
    //textProperty->SetBackgroundOpacity(1.0);
    this->Map->GetRenderer()->AddActor2D(this->AttributionActor);
    }

  this->AddTiles();

  this->Superclass::Update();
}

//----------------------------------------------------------------------------
void vtkOsmLayer::SetCacheSubDirectory(const char *relativePath)
{
  if (!this->Map)
    {
    vtkWarningMacro("Cannot set cache directory before adding layer to vtkMap");
    return;
    }

  if (vtksys::SystemTools::FileIsFullPath(relativePath))
    {
    vtkWarningMacro("Cannot set cache direcotry to relative path");
    return;
    }

  // Do *not* use SystemTools::JoinPath(), because it omits the first slash
  std::string fullPath = this->Map->GetStorageDirectory() + std::string("/")
    + relativePath;

  // Create directory if it doesn't already exist
  if(!vtksys::SystemTools::FileIsDirectory(fullPath.c_str()))
    {
    std::cerr << "Creating tile cache directory" << fullPath << std::endl;
    vtksys::SystemTools::MakeDirectory(fullPath.c_str());
    }
  this->CacheDirectory = strdup(fullPath.c_str());
}

//----------------------------------------------------------------------------
void vtkOsmLayer::RemoveTiles()
{
  this->CachedTilesMap.clear();
  std::vector<vtkMapTile*>::iterator iter = this->CachedTiles.begin();
  for (; iter != this->CachedTiles.end(); iter++)
    {
    vtkMapTile *tile = *iter;
    tile->Delete();
    }
  this->CachedTiles.clear();
}

//----------------------------------------------------------------------------
void vtkOsmLayer::AddTiles()
{
  if (!this->Renderer)
    {
    return;
    }

  std::vector<vtkMapTile*> tiles;
  std::vector<vtkMapTileSpecInternal> tileSpecs;

  if (this->Map->GetPerspectiveProjection())
    this->SelectTilesPerspective(tiles, tileSpecs);
  else
    this->SelectTiles(tiles, tileSpecs);
  if (tileSpecs.size() > 0)
    {
    this->InitializeTiles(tiles, tileSpecs);
    }
  this->RenderTiles(tiles);
}


//----------------------------------------------------------------------------
// Builds two lists based on current viewpoint:
//  * Existing tiles to render
//  * New tile-specs, representing tiles to be instantiated & initialized
void vtkOsmLayer::
SelectTiles(std::vector<vtkMapTile*>& tiles,
            std::vector<vtkMapTileSpecInternal>& tileSpecs)
{
  double focusDisplayPoint[3], bottomLeft[4], topRight[4];
  int width, height, tile_llx, tile_lly;

  this->Renderer->SetWorldPoint(0.0, 0.0, 0.0, 1.0);
  this->Renderer->WorldToDisplay();
  this->Renderer->GetDisplayPoint(focusDisplayPoint);

  this->Renderer->GetTiledSizeAndOrigin(&width, &height, &tile_llx, &tile_lly);
  this->Renderer->SetDisplayPoint(tile_llx, tile_lly, focusDisplayPoint[2]);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(bottomLeft);

  if (bottomLeft[3] != 0.0)
    {
    bottomLeft[0] /= bottomLeft[3];
    bottomLeft[1] /= bottomLeft[3];
    bottomLeft[2] /= bottomLeft[3];
    }

  //std::cerr << "Before bottomLeft " << bottomLeft[0] << " " << bottomLeft[1] << std::endl;

  if (this->Map->GetPerspectiveProjection())
    {
    bottomLeft[0] = std::max(bottomLeft[0], -180.0);
    bottomLeft[0] = std::min(bottomLeft[0],  180.0);
    bottomLeft[1] = std::max(bottomLeft[1], -180.0);
    bottomLeft[1] = std::min(bottomLeft[1],  180.0);
    }

  this->Renderer->SetDisplayPoint(tile_llx + width,
                                  tile_lly + height,
                                  focusDisplayPoint[2]);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(topRight);

  if (topRight[3] != 0.0)
    {
    topRight[0] /= topRight[3];
    topRight[1] /= topRight[3];
    topRight[2] /= topRight[3];
    }

  if (this->Map->GetPerspectiveProjection())
    {
    topRight[0] = std::max(topRight[0], -180.0);
    topRight[0] = std::min(topRight[0],  180.0);
    topRight[1] = std::max(topRight[1], -180.0);
    topRight[1] = std::min(topRight[1],  180.0);
    }

  int zoomLevel = this->Map->GetZoom();
  zoomLevel += this->Map->GetPerspectiveProjection() ? 1 : 0;
  int zoomLevelFactor = 1 << zoomLevel; // Zoom levels are interpreted as powers of two.

  int tile1x = vtkMercator::long2tilex(bottomLeft[0], zoomLevel);
  int tile2x = vtkMercator::long2tilex(topRight[0], zoomLevel);

  int tile1y = vtkMercator::lat2tiley(
                 vtkMercator::y2lat(bottomLeft[1]), zoomLevel);
  int tile2y = vtkMercator::lat2tiley(
                 vtkMercator::y2lat(topRight[1]), zoomLevel);

  //std::cerr << "tile1y " << tile1y << " " << tile2y << std::endl;

  if (tile2y > tile1y)
    {
    int temp = tile1y;
    tile1y = tile2y;
    tile2y = temp;
    }

  //std::cerr << "Before bottomLeft " << bottomLeft[0] << " " << bottomLeft[1] << std::endl;
  //std::cerr << "Before topRight " << topRight[0] << " " << topRight[1] << std::endl;

  /// Clamp tilex and tiley
  tile1x = std::max(tile1x, 0);
  tile1x = std::min(zoomLevelFactor - 1, tile1x);
  tile2x = std::max(tile2x, 0);
  tile2x = std::min(zoomLevelFactor - 1, tile2x);

  tile1y = std::max(tile1y, 0);
  tile1y = std::min(zoomLevelFactor - 1, tile1y);
  tile2y = std::max(tile2y, 0);
  tile2y = std::min(zoomLevelFactor - 1, tile2y);

  int noOfTilesX = std::max(1, zoomLevelFactor);
  int noOfTilesY = std::max(1, zoomLevelFactor);

  double lonPerTile = 360.0 / noOfTilesX;
  double latPerTile = 360.0 / noOfTilesY;

  //std::cerr << "llx " << llx << " lly " << lly << " " << height << std::endl;
  //std::cerr << "tile1y " << tile1y << " " << tile2y << std::endl;

  //std::cerr << "tile1x " << tile1x << " tile2x " << tile2x << std::endl;
  //std::cerr << "tile1y " << tile1y << " tile2y " << tile2y << std::endl;

  std::ostringstream ossKey, ossImageSource;
  std::vector<vtkMapTile*> pendingTiles;
  int xIndex, yIndex;
  for (int i = tile1x; i <= tile2x; ++i)
    {
    for (int j = tile2y; j <= tile1y; ++j)
      {
      xIndex = i;
      yIndex = zoomLevelFactor - 1 - j;

      vtkMapTile* tile = this->GetCachedTile(zoomLevel, xIndex, yIndex);
      if (tile)
        {
        tiles.push_back(tile);
        tile->SetVisible(true);
        }
      else
        {
        vtkMapTileSpecInternal tileSpec;

        tileSpec.Corners[0] = -180.0 + xIndex * lonPerTile;  // llx
        tileSpec.Corners[1] = -180.0 + yIndex * latPerTile;  // lly
        tileSpec.Corners[2] = -180.0 + (xIndex + 1) * lonPerTile;  // urx
        tileSpec.Corners[3] = -180.0 + (yIndex + 1) * latPerTile;  // ury

        tileSpec.ZoomRowCol[0] = zoomLevel;
        tileSpec.ZoomRowCol[1] = i;
        tileSpec.ZoomRowCol[2] =
          zoomLevelFactor - 1 - yIndex;

        tileSpec.ZoomXY[0] = zoomLevel;
        tileSpec.ZoomXY[1] = xIndex;
        tileSpec.ZoomXY[2] = yIndex;

        tileSpecs.push_back(tileSpec);
      }
    }
  }
}

static void GetViewCoords(double* devCoord, const double* worldCoord, vtkRenderer* renderer)
{
  devCoord[0] = worldCoord[0];
  devCoord[1] = worldCoord[1];
  devCoord[2] = worldCoord[2];
  renderer->WorldToView(devCoord[0], devCoord[1], devCoord[2]);
  
  return;
}

static bool IsPointOnScreen(const double* worldCoord, vtkRenderer* renderer)
{
  double devCoord[3];
  GetViewCoords(devCoord, worldCoord, renderer);
  
  return (devCoord[0] >= -1.0 && devCoord[0] <= 1.0) &&
         (devCoord[1] >= -1.0 && devCoord[1] <= 1.0);
}

// Note: assumes an X-Y plane with constant Z coordinate
static int IsRectOnScreen(const double* worldCoord1, const double* worldCoord2, vtkRenderer* renderer)
{
  int ptsOnScreen;
  double tempWCoord[3];
  
  tempWCoord[2] = worldCoord1[2];
  // D--C
  // |  |
  // A--B
  ptsOnScreen = 0;
  if (IsPointOnScreen(worldCoord1, renderer)) // test A
    ptsOnScreen ++;
  tempWCoord[0] = worldCoord1[0];
  tempWCoord[1] = worldCoord2[1];
  if (IsPointOnScreen(tempWCoord, renderer))  // test B
    ptsOnScreen ++;
  if (IsPointOnScreen(worldCoord2, renderer)) // test C
    ptsOnScreen ++;
  tempWCoord[0] = worldCoord2[0];
  tempWCoord[1] = worldCoord1[1];
  if (IsPointOnScreen(tempWCoord, renderer))  // test D
    ptsOnScreen ++;
  
  if (ptsOnScreen == 0)
    return 0; // completely off-screen
  else if (ptsOnScreen == 4)
    return 1; // completely on screen
  else
    return -1;  // partly on screen
}

static int IsTileOnScreen(const vtkMapTileSpecInternal* tileSpec, vtkRenderer* renderer)
{
  double coordSt[3];
  double coordEnd[3];
  
  coordSt[0] = tileSpec->Corners[0];
  coordSt[1] = tileSpec->Corners[1];
  coordEnd[0] = tileSpec->Corners[2];
  coordEnd[1] = tileSpec->Corners[3];
  coordSt[2] = coordEnd[2] = 0;
  return IsRectOnScreen(coordSt, coordEnd, renderer);
}

int vtkOsmLayer::
SelectTilesPerspective_DoTile(std::vector<vtkMapTile*>& tiles,
                              std::vector<vtkMapTileSpecInternal>& tileSpecs,
                              int tilex, int tiley, int zoomLevel, vtkRenderer* renderer)
{
  int zoomLevelFactor = 1 << zoomLevel; // Zoom levels are interpreted as powers of two.
  if (tilex < 0 || tilex >= zoomLevelFactor || tiley < 0 || tiley >= zoomLevelFactor)
    return 0;
  
  double degPerTile = 360.0 / zoomLevelFactor;
  
  int xIndex = tilex;
  int yIndex = zoomLevelFactor - 1 - tiley;
  
  vtkMapTileSpecInternal tileSpec;
  
  tileSpec.Corners[0] = -180.0 + (xIndex + 0) * degPerTile;  // llx
  tileSpec.Corners[1] = -180.0 + (yIndex + 0) * degPerTile;  // lly
  tileSpec.Corners[2] = -180.0 + (xIndex + 1) * degPerTile;  // urx
  tileSpec.Corners[3] = -180.0 + (yIndex + 1) * degPerTile;  // ury
  
  tileSpec.ZoomRowCol[0] = zoomLevel;
  tileSpec.ZoomRowCol[1] = tilex;
  tileSpec.ZoomRowCol[2] = tiley;
  
  tileSpec.ZoomXY[0] = zoomLevel;
  tileSpec.ZoomXY[1] = xIndex;
  tileSpec.ZoomXY[2] = yIndex;
  
  int retVal = IsTileOnScreen(&tileSpec, renderer);
  if (retVal == 0)
    return 0;
  
  vtkMapTile* tile = this->GetCachedTile(zoomLevel, xIndex, yIndex);
  if (tile != nullptr)
  {
    tiles.push_back(tile);
    tile->SetVisible(true);
    return 1;
  }
  else
  {
    tileSpecs.push_back(tileSpec);
    return 2;
  }
}

void vtkOsmLayer::
SelectTilesPerspective(std::vector<vtkMapTile*>& tiles,
                       std::vector<vtkMapTileSpecInternal>& tileSpecs)
{
  static const int TILE_LIMIT = 8;  // limit in every direction (+x, -x, +y, -y)
  auto renderCam = this->Renderer->GetActiveCamera();
  double focalPt[3];    // x, y, z
  
  renderCam->GetFocalPoint(focalPt);
  /* alternate way for getting the screen center point
  this->Renderer->SetWorldPoint(0.0, 0.0, 0.0, 1.0);
  this->Renderer->WorldToView();
  this->Renderer->GetViewPoint(focalPt);
  
  this->Renderer->SetViewPoint(0.0, 0.0, focalPt[2]);
  this->Renderer->ViewToWorld();
  this->Renderer->GetWorldPoint(focalPt);*/
  
  int zoomLevel = this->Map->GetZoom() + 1; // +1 due to perspective projection
  int zoomLevelFactor = 1 << zoomLevel; // Zoom levels are interpreted as powers of two.
  
  int tbasex = vtkMercator::long2tilex(focalPt[0], zoomLevel);
  int tbasey = vtkMercator::lat2tiley(vtkMercator::y2lat(focalPt[1]), zoomLevel);
  int tilex, tiley;
  
  // tile selection method:
  //  - start at focal point
  //  - from there, draw tiles in each direction
  //  - stop drawing tiles when a tile is completely off-screen OR the maximum number of tiles is reached
  for (int j = 0; j < TILE_LIMIT; j ++)
  {
    tiley = tbasey + j;
    if (tiley < 0 || tiley >= zoomLevelFactor)
      continue;
    for (int i = 0; i < TILE_LIMIT; i ++)
    {
      tilex = tbasex + i;
      if (SelectTilesPerspective_DoTile(tiles, tileSpecs, tilex, tiley, zoomLevel, this->Renderer) == 0)
        break;
    }
    for (int i = -1; i >= -TILE_LIMIT; i --)
    {
      tilex = tbasex + i;
      if (SelectTilesPerspective_DoTile(tiles, tileSpecs, tilex, tiley, zoomLevel, this->Renderer) == 0)
        break;
    }
  }
  for (int j = -1; j >= -TILE_LIMIT; j --)
  {
    tiley = tbasey + j;
    if (tiley < 0 || tiley >= zoomLevelFactor)
      continue;
    for (int i = 0; i < TILE_LIMIT; i ++)
    {
      tilex = tbasex + i;
      if (SelectTilesPerspective_DoTile(tiles, tileSpecs, tilex, tiley, zoomLevel, this->Renderer) == 0)
        break;
    }
    for (int i = -1; i >= -TILE_LIMIT; i --)
    {
      tilex = tbasex + i;
      if (SelectTilesPerspective_DoTile(tiles, tileSpecs, tilex, tiley, zoomLevel, this->Renderer) == 0)
        break;
    }
  }
}

//----------------------------------------------------------------------------
// Instantiates and initializes tiles from spec objects
void vtkOsmLayer::
InitializeTiles(std::vector<vtkMapTile*>& tiles,
                std::vector<vtkMapTileSpecInternal>& tileSpecs)
{
  std::stringstream oss;
  std::vector<vtkMapTileSpecInternal>::iterator tileSpecIter =
    tileSpecs.begin();
  for (; tileSpecIter != tileSpecs.end(); tileSpecIter++)
    {
    vtkMapTileSpecInternal spec = *tileSpecIter;

    vtkMapTile *tile = vtkMapTile::New();
    tile->SetLayer(this);
    tile->SetCorners(spec.Corners);

    // Set the local & remote paths
    this->MakeFileSystemPath(spec, oss);
    tile->SetFileSystemPath(oss.str());
    this->MakeUrl(spec, oss);
    tile->SetImageSource(oss.str());

    // Initialize the tile and add to the cache
    tile->Init();
    int zoom = spec.ZoomXY[0];
    int x = spec.ZoomXY[1];
    int y = spec.ZoomXY[2];
    this->AddTileToCache(zoom, x, y, tile);
    tiles.push_back(tile);
    tile->SetVisible(true);
    }
  tileSpecs.clear();
}

//----------------------------------------------------------------------------
// Updates display to incorporate all new tiles
void vtkOsmLayer::RenderTiles(std::vector<vtkMapTile*>& tiles)
{
  this->TileBorders[0] = this->TileBorders[1] = this->TileBorders[2] = this->TileBorders[3] = 0;
  if (tiles.size() > 0)
    {
    // Remove old tiles
    std::vector<vtkMapTile*>::iterator itr = this->CachedTiles.begin();
    for (; itr != this->CachedTiles.end(); ++itr)
      {
      this->Renderer->RemoveActor((*itr)->GetActor());
      }

    tiles[0]->GetCorners(this->TileBorders);
    // Add new tiles
    for (std::size_t i = 0; i < tiles.size(); ++i)
      {
      this->Renderer->AddActor(tiles[i]->GetActor());
      if (tiles[i]->GetCorners()[0] < this->TileBorders[0])
        this->TileBorders[0] = tiles[i]->GetCorners()[0];
      if (tiles[i]->GetCorners()[1] < this->TileBorders[1])
        this->TileBorders[1] = tiles[i]->GetCorners()[1];
      if (tiles[i]->GetCorners()[2] > this->TileBorders[2])
        this->TileBorders[2] = tiles[i]->GetCorners()[2];
      if (tiles[i]->GetCorners()[3] > this->TileBorders[3])
        this->TileBorders[3] = tiles[i]->GetCorners()[3];
      }

    tiles.clear();
    }
}

//----------------------------------------------------------------------------
void vtkOsmLayer::AddTileToCache(int zoom, int x, int y, vtkMapTile* tile)
{
  this->CachedTilesMap[zoom][x][y] = tile;
  this->CachedTiles.push_back(tile);
}

//----------------------------------------------------------------------------
vtkMapTile *vtkOsmLayer::GetCachedTile(int zoom, int x, int y)
{
  if (this->CachedTilesMap.find(zoom) == this->CachedTilesMap.end() &&
      this->CachedTilesMap[zoom].find(x) == this->CachedTilesMap[zoom].end() &&
      this->CachedTilesMap[zoom][x].find(y) == this->CachedTilesMap[zoom][x].end())
    {
    return NULL;
    }

  return this->CachedTilesMap[zoom][x][y];
}

//----------------------------------------------------------------------------
void vtkOsmLayer::MakeFileSystemPath(
  vtkMapTileSpecInternal& tileSpec, std::stringstream& ss)
{
  ss.str("");
  ss << this->GetCacheDirectory() << "/"
     << tileSpec.ZoomRowCol[0]
     << "-" << tileSpec.ZoomRowCol[1]
     << "-" << tileSpec.ZoomRowCol[2]
     << "." << this->MapTileExtension;
}

//----------------------------------------------------------------------------
void vtkOsmLayer::MakeUrl(
  vtkMapTileSpecInternal& tileSpec, std::stringstream& ss)
{
  ss.str("");
  ss << "http://" << this->MapTileServer
     << "/" << tileSpec.ZoomRowCol[0]
     << "/" << tileSpec.ZoomRowCol[1]
     << "/" << tileSpec.ZoomRowCol[2]
     << "." << this->MapTileExtension;;
}
