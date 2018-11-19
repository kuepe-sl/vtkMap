/*=========================================================================

  Program:   Visualization Toolkit

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

   This software is distributed WITHOUT ANY WARRANTY; without even
   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkOsmLayer -
// .SECTION Description
//

#ifndef __vtkOsmLayer_h
#define __vtkOsmLayer_h

#include "vtkFeatureLayer.h"
#include "vtkMapTile.h"
#include "vtkMapTileSpecInternal.h"
#include "vtkmap_export.h"

// VTK Includes
#include <vtkObject.h>
#include <vtkRenderer.h>

#include <map>
#include <sstream>
#include <vector>

class vtkTextActor;

class VTKMAP_EXPORT vtkOsmLayer : public vtkFeatureLayer
{
public:
  static vtkOsmLayer* New();
  virtual void PrintSelf(ostream &os, vtkIndent indent);
  vtkTypeMacro(vtkOsmLayer, vtkFeatureLayer)

  // Set the map tile server and corresponding attribute text.
  // The default server is tile.openstreetmap.org.
  // The attribution will be displayed at the bottom of the window.
  // The file extension is typically "png" or "jpg".
  void SetMapTileServer(const char *server,
                        const char *attribution,
                        const char *extension);

  // Description:
  // The full path to the directory used for caching map-tile files.
  // Set automatically by vtkMap.
  vtkGetStringMacro(CacheDirectory);

  // Description:
  virtual void Update();

  // Description:
  // Set the subdirectory used for caching map files.
  // This method is intended for *testing* use only.
  // The argument is *relative* to vtkMap::StorageDirectory.
  void SetCacheSubDirectory(const char *relativePath);

  // Description: get borders of loaded tiles
  vtkGetVector4Macro(TileBorders, double);

protected:
  vtkOsmLayer();
  virtual ~vtkOsmLayer();

  vtkSetStringMacro(CacheDirectory);

  virtual void AddTiles();
  void RemoveTiles();

  // Next 5 methods used to add tiles to layer
  void SelectTiles(std::vector<vtkMapTile*>& tiles,
                   std::vector<vtkMapTileSpecInternal>& tileSpecs);
  int SelectTilesPerspective_DoTile(std::vector<vtkMapTile*>& tiles,
                                    std::vector<vtkMapTileSpecInternal>& tileSpecs,
                                    int tilex, int tiley, int zoomLevel, vtkRenderer* renderer);
  void SelectTilesPerspective(std::vector<vtkMapTile*>& tiles,
                              std::vector<vtkMapTileSpecInternal>& tileSpecs);
  void InitializeTiles(std::vector<vtkMapTile*>& tiles,
                       std::vector<vtkMapTileSpecInternal>& tileSpecs);
  void RenderTiles(std::vector<vtkMapTile*>& tiles);

  void AddTileToCache(int zoom, int x, int y, vtkMapTile* tile);
  vtkMapTile* GetCachedTile(int zoom, int x, int y);

  // Construct paths for local & remote tile access
  // A stringstream is passed in for performance reasons
  void MakeFileSystemPath(
    vtkMapTileSpecInternal& tileSpec, std::stringstream& ss);
  void MakeUrl(vtkMapTileSpecInternal& tileSpec, std::stringstream& ss);

protected:
  enum
  {
	  ModeOsmHost = 0,
	  ModeOsmFull = 1,
	  ModeBingFull = 2
  };

  char *MapTileExtension;
  char *MapTileServer;
  char *MapTileAttribution;
  int MapTileSvrMode;
  vtkTextActor *AttributionActor;
  double TileBorders[4];
  double VirtualCenter[2];  // in x and y, NOT lat/lon

  char *CacheDirectory;
  std::map< int, std::map< int, std::map <int, vtkMapTile*> > > CachedTilesMap;
  std::vector<vtkMapTile*> CachedTiles;

private:
  vtkOsmLayer(const vtkOsmLayer&);    // Not implemented
  vtkOsmLayer& operator=(const vtkOsmLayer&); // Not implemented
};

#endif // __vtkOsmLayer_h

