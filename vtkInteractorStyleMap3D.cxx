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

#include "vtkInteractorStyleMap3D.h"
#include "vtkGeoMapSelection.h"
#include "vtkMap.h"

// VTK includes.
#include <vtkCamera.h>
#include <vtkObjectFactory.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

vtkStandardNewMacro(vtkInteractorStyleMap3D);

//-----------------------------------------------------------------------------
vtkInteractorStyleMap3D::vtkInteractorStyleMap3D() :
  vtkInteractorStyleTrackballCamera()
{
  this->Map = NULL;
}

//-----------------------------------------------------------------------------
vtkInteractorStyleMap3D::~vtkInteractorStyleMap3D()
{
}

//-----------------------------------------------------------------------------
void vtkInteractorStyleMap3D::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-----------------------------------------------------------------------------
void vtkInteractorStyleMap3D::OnLeftButtonUp()
{
  this->Superclass::OnLeftButtonUp();
  this->Map->Draw();
}

//-----------------------------------------------------------------------------
void vtkInteractorStyleMap3D::OnRightButtonUp()
{
  this->Superclass::OnRightButtonUp();
  this->Map->Draw();
}

//--------------------------------------------------------------------------
void vtkInteractorStyleMap3D::OnMouseMove()
{
  this->Superclass::OnMouseMove();

  switch (this->State)
  {
  case VTKIS_ROTATE:
  //case VTKIS_PAN: // handled by this::Pan function
  case VTKIS_SPIN:
  case VTKIS_DOLLY:
  case VTKIS_ZOOM:
    if (this->Map != NULL)
      this->Map->Draw();
    break;
  }
}

//----------------------------------------------------------------------------
void vtkInteractorStyleMap3D::OnMouseWheelForward()
{
  if (this->Map)
  {
    this->SetCurrentRenderer(this->Map->GetRenderer());

    vtkCamera *camera = this->Map->GetRenderer()->GetActiveCamera();

    // Apply the dolly operation (move closer to focal point)
    camera->Dolly(2.0);

    double focalCoords[3];
    camera->GetFocalPoint(focalCoords);
    focalCoords[2] = 0.0;
    camera->SetFocalPoint(focalCoords);

    // Redraw the map
    this->GetCurrentRenderer()->ResetCameraClippingRange(); // ensure that everything is visible
    this->Map->Draw();
    return; // don't forward event to prevent VTK from doing its own camera handling
  }
  this->Superclass::OnMouseWheelForward();
}

//----------------------------------------------------------------------------
void vtkInteractorStyleMap3D::OnMouseWheelBackward()
{
  if (this->Map)
  {
    this->SetCurrentRenderer(this->Map->GetRenderer());

    vtkCamera *camera = this->Map->GetRenderer()->GetActiveCamera();

    // Apply the dolly operation (move away from focal point)
    camera->Dolly(0.5);

    double focalCoords[3];
    camera->GetFocalPoint(focalCoords);
    focalCoords[2] = 0.0;
    camera->SetFocalPoint(focalCoords);

    // Redraw the map
    this->GetCurrentRenderer()->ResetCameraClippingRange(); // ensure that everything is visible
    this->Map->Draw();
    return; // don't forward event to prevent VTK from doing its own camera handling
  }
  this->Superclass::OnMouseWheelBackward();
}

//-----------------------------------------------------------------------------
void vtkInteractorStyleMap3D::SetMap(vtkMap *map)
{
  this->Map = map;
  this->SetCurrentRenderer(map->GetRenderer());
}

//-----------------------------------------------------------------------------
// This is an exact copy-paste from VTK/Interaction/Style/vtkInteractorStyleTrackballCamera.cxx
// except for the two lines marked with "NEW".
// We're not reusing the parent's code to prevent flickering. (We would move the camera, render, fix the camera, then render again.)
void vtkInteractorStyleMap3D::Pan()
{
  if (this->CurrentRenderer == NULL)
  {
    return;
  }

  vtkRenderWindowInteractor *rwi = this->Interactor;

  double viewFocus[4], focalDepth, viewPoint[3];
  double newPickPoint[4], oldPickPoint[4], motionVector[3];

  // Calculate the focal depth since we'll be using it a lot

  vtkCamera *camera = this->CurrentRenderer->GetActiveCamera();
  camera->GetFocalPoint(viewFocus);
  this->ComputeWorldToDisplay(viewFocus[0], viewFocus[1], viewFocus[2],
                              viewFocus);
  focalDepth = viewFocus[2];

  this->ComputeDisplayToWorld(rwi->GetEventPosition()[0],
                              rwi->GetEventPosition()[1],
                              focalDepth,
                              newPickPoint);

  // Has to recalc old mouse point since the viewport has moved,
  // so can't move it outside the loop

  this->ComputeDisplayToWorld(rwi->GetLastEventPosition()[0],
                              rwi->GetLastEventPosition()[1],
                              focalDepth,
                              oldPickPoint);

  // Camera motion is reversed

  motionVector[0] = oldPickPoint[0] - newPickPoint[0];
  motionVector[1] = oldPickPoint[1] - newPickPoint[1];
  motionVector[2] = oldPickPoint[2] - newPickPoint[2];

  camera->GetFocalPoint(viewFocus);
  camera->GetPosition(viewPoint);
  camera->SetFocalPoint(motionVector[0] + viewFocus[0],
                        motionVector[1] + viewFocus[1],
                        motionVector[2] + viewFocus[2]);

  camera->SetPosition(motionVector[0] + viewPoint[0],
                      motionVector[1] + viewPoint[1],
                      motionVector[2] + viewPoint[2]);

  fixCameraZ(camera); // <--- NEW

  if (rwi->GetLightFollowCamera())
  {
    this->CurrentRenderer->UpdateLightsGeometryToFollowCamera();
  }

  if (this->Map != NULL)
    this->Map->Draw();  // <--- NEW (internally calls rwi->Render())
  else
    rwi->Render();
}

void vtkInteractorStyleMap3D::fixCameraZ(vtkCamera *camera)
{
  double zMove = camera->GetFocalPoint()[2];

  if (std::fabs(zMove) < 0.000001)
    return; // nothing to fix

  // Since we're looking at a 2D map, not having the focal point makes no sense.
  // Thus we turn the Z-movement into an XY-movement.

  double focalCoords[3];
  double cameraCoords[3];
  double losVector[3];  // line-of-sight vector, from camera position to focal point

  camera->GetFocalPoint(focalCoords);
  camera->GetPosition(cameraCoords);

  vtkMath::Subtract(focalCoords, cameraCoords, losVector);
  vtkMath::Normalize(losVector);
  vtkMath::MultiplyScalar(losVector, zMove);
  losVector[2] = -zMove;  // make sure that the Z coordinate returns to 0.

  // transform the z-movement into an xy-movement (in direction of LOS vector)
  vtkMath::Add(focalCoords, losVector, focalCoords);
  vtkMath::Add(cameraCoords, losVector, cameraCoords);
  camera->SetPosition(cameraCoords);
  camera->SetFocalPoint(focalCoords);
}
