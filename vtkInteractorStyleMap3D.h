/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMap

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

   This software is distributed WITHOUT ANY WARRANTY; without even
   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkInteractorStyleMap3D - interactor style specifically for map views
// .SECTION Description
//


#ifndef __vtkInteractorStyleMap3D_h
#define __vtkInteractorStyleMap3D_h

#include <vtkInteractorStyleTrackballCamera.h>

#include <vtkmap_export.h>

class vtkMap;
class vtkCamera;

class VTKMAP_EXPORT vtkInteractorStyleMap3D
  : public vtkInteractorStyleTrackballCamera
{
public:
  // Description:
  // Standard VTK functions.
  static vtkInteractorStyleMap3D* New();
  vtkTypeMacro(vtkInteractorStyleMap3D, vtkInteractorStyleTrackballCamera);

  virtual void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Constructor / Destructor.
  vtkInteractorStyleMap3D();
  ~vtkInteractorStyleMap3D();

  // Description:
  // Overriding these functions to implement custom
  // interactions.
  virtual void OnLeftButtonUp();
  virtual void OnRightButtonUp();
  virtual void OnMouseMove();
  virtual void OnMouseWheelForward();
  virtual void OnMouseWheelBackward();
  // Map
  void SetMap(vtkMap* map);

protected:
  virtual void Pan();
  void fixCameraZ(vtkCamera* camera);

private:
// Not implemented.
  vtkInteractorStyleMap3D(const vtkInteractorStyleMap3D&);
  void operator=(const vtkInteractorStyleMap3D&);

  vtkMap *Map;
};

#endif // __vtkInteractorStyleMap3D_h
