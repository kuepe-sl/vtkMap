#include "vtkMap.h"

#include <QVTKWidget.h>

#include <vtkActor.h>
#include <vtkCallbackCommand.h>
#include <vtkInteractorStyleImage.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

#include <QApplication>
#include <QFrame>
#include <QMainWindow>
#include <QPushButton>



// ------------------------------------------------------------
// Callback command for handling mouse events
// In the future, will be replaced by interactor style
class MapCallback : public vtkCallbackCommand
{
public:
  MapCallback(vtkMap *map) : Map(map), MouseDown(false) {}

  virtual void Execute(vtkObject *caller, unsigned long eventId, void *callData)
  {
    switch (eventId)
      {
      case vtkCommand::MiddleButtonPressEvent:
        this->MouseDown = true;
        break;
      case vtkCommand::MiddleButtonReleaseEvent:
        this->MouseDown = false;
        break;
      case vtkCommand::MouseMoveEvent:
        if (this->MouseDown)
          {
          this->Map->Draw();
          }
      case vtkCommand::MouseWheelForwardEvent:
      case vtkCommand::MouseWheelBackwardEvent:
        this->Map->Draw();
      }
  }

protected:
  vtkMap *Map;
  bool MouseDown;
};


// ------------------------------------------------------------
int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  QMainWindow mainWindow;
  QFrame frame;
  mainWindow.setCentralWidget(&frame);

  // Setup push button (plain Qt widget)
  QPushButton *button = new QPushButton("Test", &frame);
  button->move(50, 50);


  // Setup sphere (QVTKWidget) from VTK wiki
  QVTKWidget widget(&frame);
  widget.resize(256,256);
  widget.move(50, 100);

  vtkSmartPointer<vtkSphereSource> sphereSource =
      vtkSmartPointer<vtkSphereSource>::New();
  sphereSource->Update();
  vtkSmartPointer<vtkPolyDataMapper> sphereMapper =
      vtkSmartPointer<vtkPolyDataMapper>::New();
  sphereMapper->SetInputConnection(sphereSource->GetOutputPort());
  vtkSmartPointer<vtkActor> sphereActor =
      vtkSmartPointer<vtkActor>::New();
  sphereActor->SetMapper(sphereMapper);

  vtkSmartPointer<vtkRenderWindow> renderWindow =
      vtkSmartPointer<vtkRenderWindow>::New();
  vtkSmartPointer<vtkRenderer> renderer =
      vtkSmartPointer<vtkRenderer>::New();
  renderWindow->AddRenderer(renderer);

  renderer->AddActor(sphereActor);
  renderer->ResetCamera();

  widget.SetRenderWindow(renderWindow);


  // Setup map (QVTKWwidget)
  QVTKWidget mapWidget(&frame);
  mapWidget.resize(500, 500);
  mapWidget.move(400, 100);

  vtkNew<vtkMap> map;
  vtkNew<vtkRenderer> mapRenderer;
  map->SetRenderer(mapRenderer.GetPointer());
  map->SetCenter(0, 0);
  map->SetZoom(5);

  vtkNew<vtkRenderWindow> mapRenderWindow;
  mapRenderWindow->AddRenderer(mapRenderer.GetPointer());
  mapWidget.SetRenderWindow(mapRenderWindow.GetPointer());

  vtkRenderWindowInteractor *intr = mapWidget.GetInteractor();
  intr->SetInteractorStyle((map->GetInteractorStyle()));
  intr->Initialize();


  MapCallback *mapCallback = new MapCallback(map.GetPointer());
  intr->AddObserver(vtkCommand::AnyEvent, mapCallback);
  intr->Start();

  // Display main window and start Qt event loop
  mainWindow.show();
  mainWindow.resize(1000, 800);
  map->Draw();

  app.exec();

  return EXIT_SUCCESS;
}
