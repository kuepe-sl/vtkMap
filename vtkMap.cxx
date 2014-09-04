/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMap.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

   This software is distributed WITHOUT ANY WARRANTY; without even
   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkMap.h"
#include "vtkMapMarker.h"
#include "vtkMapTile.h"

// VTK Includes
#include <vtkActor2D.h>
#include <vtkImageInPlaceFilter.h>
#include <vtkInteractorStyleImage.h>
#include <vtkObjectFactory.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkCamera.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkPlaneSource.h>

#include <algorithm>
#include <math.h>
#include <sstream>

vtkStandardNewMacro(vtkMap)

//----------------------------------------------------------------------------
int long2tilex(double lon, int z)
{
  return (int)(floor((lon + 180.0) / 360.0 * pow(2.0, z)));
}

//----------------------------------------------------------------------------
int lat2tiley(double lat, int z)
{
  return (int)(floor((1.0 - log( tan(lat * M_PI/180.0) + 1.0 /
    cos(lat * M_PI/180.0)) / M_PI) / 2.0 * pow(2.0, z)));
}

//----------------------------------------------------------------------------
double tilex2long(int x, int z)
{
  return x / pow(2.0, z) * 360.0 - 180;
}

//----------------------------------------------------------------------------
double tiley2lat(int y, int z)
{
  double n = M_PI - 2.0 * M_PI * y / pow(2.0, z);
  return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

//----------------------------------------------------------------------------
double y2lat(double a)
{
  return 180 / M_PI * (2 * atan(exp(a * M_PI / 180.0)) - M_PI / 2.0);
}

//----------------------------------------------------------------------------
double lat2y(double a)
{
  return 180.0 / M_PI * log(tan(M_PI / 4.0 + a * (M_PI / 180.0) / 2.0));
}

//----------------------------------------------------------------------------
struct sortTiles
{
  inline bool operator() (vtkMapTile* tile1,  vtkMapTile* tile2)
  {
    return (tile1->GetBin() < tile2->GetBin());
  }
};

//----------------------------------------------------------------------------
double computeCameraDistance(vtkCamera* cam, int zoomLevel)
{
  double deg = 360.0 / std::pow(2, zoomLevel);
  return (deg / std::sin(vtkMath::RadiansFromDegrees(cam->GetViewAngle())));
}

//----------------------------------------------------------------------------
int computeZoomLevel(vtkCamera* cam)
{
  int i;
  double* pos = cam->GetPosition();
  double width = pos[2] * sin(vtkMath::RadiansFromDegrees(cam->GetViewAngle()));

  for (i = 0; i < 20; i += 1) {
    if (width >= (360.0 / pow(2, i))) {
      /// We are forcing the minimum zoom level to 2 so that we can get
      /// high res imagery even at the zoom level 0 distance
      return i;
    }
  }
}

//----------------------------------------------------------------------------
vtkMap::vtkMap()
{
  this->Renderer = NULL;
  this->InteractorStyle = vtkInteractorStyleImage::New();
  this->Zoom = 1;
  this->Center[0] = this->Center[1] = 0.0;
  this->Initialized = false;

  // Load marker image
  // Todo Embed image data in header or source file?
  vtkMapMarker::LoadMarkerImage(MARKER_IMAGE_FILE);
}

//----------------------------------------------------------------------------
vtkMap::~vtkMap()
{
  if (this->InteractorStyle)
    {
    this->InteractorStyle->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkMap::PrintSelf(ostream &os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << "vtkMap" << std::endl
     << "Zoom Level: " << this->Zoom
     << "Center: " << this->Center[0] << " " << this->Center[1] << std::endl;
}

//----------------------------------------------------------------------------
void vtkMap::Update()
{
  /// Compute the zoom level here
  this->SetZoom(computeZoomLevel(this->Renderer->GetActiveCamera()));

  RemoveTiles();
  AddTiles();
}

//----------------------------------------------------------------------------
void vtkMap::Draw()
{
  if (!this->Initialized && this->Renderer)
    {
    this->Initialized = true;
    this->Renderer->GetActiveCamera()->SetPosition(this->Center[1],
                                                   this->Center[0],
                                                   computeCameraDistance(this->Renderer->GetActiveCamera(), this->Zoom));
    this->Renderer->GetActiveCamera()->SetFocalPoint(this->Center[1],
                                                     this->Center[0],
                                                    0.0);
    this->Renderer->GetRenderWindow()->Render();
    this->Renderer->GetRenderWindow()->Render();
    }
  this->Update();
  this->Renderer->GetRenderWindow()->Render();
}

//----------------------------------------------------------------------------
void vtkMap::RemoveTiles()
{
  // To do.
}

//----------------------------------------------------------------------------
void vtkMap::AddTiles()
{
  double focusDisplayPoint[3], bottomLeft[4], topRight[4];
  int width, height, llx, lly;

  this->Renderer->SetWorldPoint(0.0, 0.0, 0.0, 1.0);
  this->Renderer->WorldToDisplay();
  this->Renderer->GetDisplayPoint(focusDisplayPoint);

  this->Renderer->GetTiledSizeAndOrigin(&width, &height, &llx, &lly);
  this->Renderer->SetDisplayPoint(llx, lly, focusDisplayPoint[2]);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(bottomLeft);

  if (bottomLeft[3] != 0.0)
    {
    bottomLeft[0] /= bottomLeft[3];
    bottomLeft[1] /= bottomLeft[3];
    bottomLeft[2] /= bottomLeft[3];
    }

  //std::cerr << "Before bottomLeft " << bottomLeft[0] << " " << bottomLeft[1] << std::endl;

  bottomLeft[0] = std::max(bottomLeft[0], -180.0);
  bottomLeft[0] = std::min(bottomLeft[0],  180.0);
  bottomLeft[1] = std::max(bottomLeft[1], -180.0);
  bottomLeft[1] = std::min(bottomLeft[1],  180.0);

  this->Renderer->SetDisplayPoint(llx + width, lly + height, focusDisplayPoint[2]);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(topRight);

  if (topRight[3] != 0.0)
    {
    topRight[0] /= topRight[3];
    topRight[1] /= topRight[3];
    topRight[2] /= topRight[3];
    }

  topRight[0] = std::max(topRight[0], -180.0);
  topRight[0] = std::min(topRight[0],  180.0);
  topRight[1] = std::max(topRight[1], -180.0);
  topRight[1] = std::min(topRight[1],  180.0);

  int tile1x = long2tilex(bottomLeft[0], this->Zoom);
  int tile2x = long2tilex(topRight[0], this->Zoom);

  int tile1y = lat2tiley(y2lat(bottomLeft[1]), this->Zoom);
  int tile2y = lat2tiley(y2lat(topRight[1]), this->Zoom);

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
  tile1x = std::min(static_cast<int>(pow(2, this->Zoom)) - 1, tile1x);
  tile2x = std::max(tile2x, 0);
  tile2x = std::min(static_cast<int>(pow(2, this->Zoom)) - 1, tile2x);

  tile1y = std::max(tile1y, 0);
  tile1y = std::min(static_cast<int>(pow(2, this->Zoom)) - 1, tile1y);
  tile2y = std::max(tile2y, 0);
  tile2y = std::min(static_cast<int>(pow(2, this->Zoom)) - 1, tile2y);

  int noOfTilesX = std::max(1, static_cast<int>(pow(2, this->Zoom)));
  int noOfTilesY = std::max(1, static_cast<int>(pow(2, this->Zoom)));

  double lonPerTile = 360.0 / noOfTilesX;
  double latPerTile = 360.0 / noOfTilesY;

  //std::cerr << "llx " << llx << " lly " << lly << " " << height << std::endl;
  //std::cerr << "tile1y " << tile1y << " " << tile2y << std::endl;

  //std::cerr << "tile1x " << tile1x << " tile2x " << tile2x << std::endl;
  //std::cerr << "tile1y " << tile1y << " tile2y " << tile2y << std::endl;

  int xIndex, yIndex;
  for (int i = tile1x; i <= tile2x; ++i)
    {
    for (int j = tile2y; j <= tile1y; ++j)
      {
      xIndex = i;
      yIndex = static_cast<int>(pow(2, this->Zoom)) - 1 - j;

      vtkMapTile* tile = this->GetCachedTile(this->Zoom, xIndex, yIndex);
      if (!tile)
        {
        tile = vtkMapTile::New();
        double llx = -180.0 + xIndex * lonPerTile;
        double lly = -180.0 + yIndex * latPerTile;
        double urx = -180.0 + (xIndex + 1) * lonPerTile;
        double ury = -180.0 + (yIndex + 1) * latPerTile;

        tile->SetCorners(llx, lly, urx, ury);

        std::ostringstream oss;
        oss << this->Zoom;
        std::string zoom = oss.str();
        oss.str("");

        oss << i;
        std::string row = oss.str();
        oss.str("");

        oss << (static_cast<int>(pow(2, this->Zoom)) - 1 - yIndex);
        std::string col = oss.str();
        oss.str("");

        // Set tile texture source
        oss << zoom << row << col;
        tile->SetImageKey(oss.str());
        tile->SetImageSource("http://tile.openstreetmap.org/" + zoom + "/" + row +
                             "/" + col + ".png");
        tile->Init();
        this->AddTileToCache(this->Zoom, xIndex, yIndex, tile);
      }
    this->NewPendingTiles.push_back(tile);
    tile->SetVisible(true);
    }
  }

  if (this->NewPendingTiles.size() > 0)
    {
    std::vector<vtkActor*>::iterator itr = this->CachedActors.begin();
    for (itr; itr != this->CachedActors.end(); ++itr)
      {
      this->Renderer->RemoveActor(*itr);
      }

    vtkPropCollection* props = this->Renderer->GetViewProps();

    props->InitTraversal();
    vtkProp* prop = props->GetNextProp();
    std::vector<vtkProp*> otherProps;
    while (prop)
      {
      otherProps.push_back(prop);
      prop = props->GetNextProp();
      }

    this->Renderer->RemoveAllViewProps();

    std::sort(this->NewPendingTiles.begin(), this->NewPendingTiles.end(),
              sortTiles());

    for (int i = 0; i < this->NewPendingTiles.size(); ++i)
      {
      // Add tile to the renderer
      this->Renderer->AddActor(this->NewPendingTiles[i]->GetActor());
      }

    std::vector<vtkProp*>::iterator itr2 = otherProps.begin();
    while (itr2 != otherProps.end())
      {
      this->Renderer->AddViewProp(*itr2);
      ++itr2;
      }

    this->NewPendingTiles.clear();
    }
}

//----------------------------------------------------------------------------
double vtkMap::Clip(double n, double minValue, double maxValue)
{
  double max = n > minValue ? n : minValue;
  double min = max > maxValue ? maxValue : max;
  return min;
}

//----------------------------------------------------------------------------
void vtkMap::AddTileToCache(int zoom, int x, int y, vtkMapTile* tile)
{
  this->CachedTiles[zoom][x][y] = tile;
  this->CachedActors.push_back(tile->GetActor());
}

//----------------------------------------------------------------------------
vtkMapTile *vtkMap::GetCachedTile(int zoom, int x, int y)
{
  if (this->CachedTiles.find(zoom) == this->CachedTiles.end() &&
      this->CachedTiles[zoom].find(x) == this->CachedTiles[zoom].end() &&
      this->CachedTiles[zoom][x].find(y) == this->CachedTiles[zoom][x].end())
    {
    return NULL;
    }

  return this->CachedTiles[zoom][x][y];
}


//----------------------------------------------------------------------------
vtkMapMarker *vtkMap::AddMarker(double Latitude, double Longitude)
{
  vtkMapMarker *marker = vtkMapMarker::New();
  marker->SetCoordinates(Latitude, Longitude);
  this->Renderer->AddActor(marker->GetActor());
  this->MapMarkers.insert(marker);
  this->Draw();
  return marker;
}

//----------------------------------------------------------------------------
void vtkMap::RemoveMapMarkers()
{
  std::set<vtkMapMarker*>::iterator iter = this->MapMarkers.begin();
  for (; iter != this->MapMarkers.end(); iter++)
    {
    vtkMapMarker *marker = *iter;
    vtkActor2D *actor = marker->GetActor();
    this->Renderer->RemoveActor(actor);
    }
  this->MapMarkers.clear();
  this->Draw();
}
