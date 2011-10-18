/*========================================================================
  VES --- VTK OpenGL ES Rendering Toolkit

      http://www.kitware.com/ves

  Copyright 2011 Kitware, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 ========================================================================*/

#include "vesKiwiImageWidgetRepresentation.h"

#include "vesRenderer.h"
#include "vesCamera.h"
#include "vesMapper.h"
#include "vesDataConversionTools.h"
#include "vesKiwiImagePlaneDataRepresentation.h"
#include "vesKiwiPolyDataRepresentation.h"

#include <vtkNew.h>
#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkLookupTable.h>
#include <vtkOutlineFilter.h>
#include <vtkPointData.h>
#include <vtkExtractVOI.h>
#include <vtkContourFilter.h>
#include <vtkCellLocator.h>
#include <vtkAppendPolyData.h>

#include <vector>
#include <cassert>

//----------------------------------------------------------------------------
class vesKiwiImageWidgetRepresentation::vesInternal
{
public:

  vesInternal()
  {
    this->SelectedImageDimension = -1;
    this->ContourVis = 0;
    this->ImageScalarRange[0] = 0;
    this->ImageScalarRange[1] = 1;
    for (int i = 0; i < 3; ++i)
      this->CurrentSliceIndices[i] = 0;

    this->ContourRep = 0;
    this->OutlineRep = 0;
    this->Renderer = 0;
  }

  ~vesInternal()
  {
  }

  vesRenderer* Renderer;

  int SelectedImageDimension;
  int CurrentSliceIndices[3];
  int ContourVis;
  double ImageScalarRange[2];

  std::vector<vesKiwiDataRepresentation*> AllReps;
  std::vector<vesKiwiImagePlaneDataRepresentation*> SliceReps;

  vesKiwiPolyDataRepresentation* ContourRep;
  vesKiwiPolyDataRepresentation* OutlineRep;

  vtkSmartPointer<vtkExtractVOI> SliceFilter;
  vtkSmartPointer<vtkCellLocator> Locator;
  vtkSmartPointer<vtkAppendPolyData> AppendFilter;
};

//----------------------------------------------------------------------------
vesKiwiImageWidgetRepresentation::vesKiwiImageWidgetRepresentation()
{
  this->Internal = new vesInternal();
}

//----------------------------------------------------------------------------
vesKiwiImageWidgetRepresentation::~vesKiwiImageWidgetRepresentation()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vesKiwiImageWidgetRepresentation::setImageData(vtkImageData* image)
{
  image->GetPointData()->GetScalars()->GetRange(this->Internal->ImageScalarRange);
  vtkSmartPointer<vtkScalarsToColors> colorMap =
    vesDataConversionTools::GetGrayscaleLookupTable(this->Internal->ImageScalarRange);
  for (int i = 0; i < 3; ++i)
    this->Internal->SliceReps[i]->setColorMap(colorMap);


  int dimensions[3];
  image->GetDimensions(dimensions);
  this->Internal->CurrentSliceIndices[0] = dimensions[0]/2;
  this->Internal->CurrentSliceIndices[1] = dimensions[1]/2;
  this->Internal->CurrentSliceIndices[2] = dimensions[2]/2;

  this->Internal->SliceFilter = vtkSmartPointer<vtkExtractVOI>::New();
  this->Internal->SliceFilter->SetInput(image);

  for (int i = 0; i < 3; ++i)
    this->setSliceIndex(i, this->Internal->CurrentSliceIndices[i]);

  vtkNew<vtkOutlineFilter> outline;
  outline->SetInput(image);
  outline->Update();
  this->Internal->OutlineRep->setPolyData(outline->GetOutput());

  if (image->GetNumberOfPoints() < 600000) {
    vtkNew<vtkContourFilter> contour;
    contour->SetInput(image);
    // contour value hardcoded for head image dataset
    contour->SetValue(0, 1400);
    contour->ComputeScalarsOff();
    contour->ComputeNormalsOff();
    contour->Update();

    this->Internal->ContourRep->setPolyData(contour->GetOutput());
    this->Internal->ContourRep->setColor(0.8, 0.8, 0.8, 0.4);
    this->Internal->ContourVis = 1;
  }
}

//----------------------------------------------------------------------------
void vesKiwiImageWidgetRepresentation::initializeWithShader(vesShaderProgram* geometryShader, vesShaderProgram* textureShader)
{
  this->Internal->AppendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

  for (int i = 0; i < 3; ++i) {
    vesKiwiImagePlaneDataRepresentation* rep = new vesKiwiImagePlaneDataRepresentation();
    rep->initializeWithShader(textureShader);
    rep->setBinNumber(1);
    this->Internal->SliceReps.push_back(rep);
    this->Internal->AllReps.push_back(rep);
    this->Internal->AppendFilter->AddInput(vtkSmartPointer<vtkPolyData>::New());
  }

  this->Internal->ContourRep = new vesKiwiPolyDataRepresentation();
  this->Internal->ContourRep->initializeWithShader(geometryShader);
  this->Internal->ContourRep->setBinNumber(2);
  this->Internal->OutlineRep = new vesKiwiPolyDataRepresentation();
  this->Internal->OutlineRep->initializeWithShader(geometryShader);
  this->Internal->OutlineRep->setBinNumber(2);
  this->Internal->AllReps.push_back(this->Internal->ContourRep);
  this->Internal->AllReps.push_back(this->Internal->OutlineRep);
}

//----------------------------------------------------------------------------
bool vesKiwiImageWidgetRepresentation::scrollSliceModeActive() const
{
  return (this->Internal->SelectedImageDimension >= 0);
}

//----------------------------------------------------------------------------
void vesKiwiImageWidgetRepresentation::scrollImageSlice(double deltaX, double deltaY)
{
  deltaY *= -1;

  vesRenderer* ren = this->renderer();
  vesCamera* camera = ren->camera();
  vesVector3f viewFocus = camera->GetFocalPoint();
  vesVector3f viewPoint = camera->GetPosition();
  vesVector3f viewFocusDisplay = ren->computeWorldToDisplay(viewFocus);
  float focalDepth = viewFocusDisplay[2];

  double x0 = viewFocusDisplay[0];
  double y0 = viewFocusDisplay[1];
  double x1 = x0 + deltaX;
  double y1 = y0 + deltaY;

  // map change into world coordinates
  vesVector3f point0 = ren->computeDisplayToWorld(vesVector3f(x0, y0, focalDepth));
  vesVector3f point1 = ren->computeDisplayToWorld(vesVector3f(x1, y1, focalDepth));
  vesVector3f motionVector = point1 - point0;

  int flatDimension = this->Internal->SelectedImageDimension;

  vesVector3f planeNormal(0, 0, 0);
  planeNormal[flatDimension] = 1.0;

  double vectorDot = gmtl::dot(motionVector, planeNormal);
  double delta = vectorDot;
  if (fabs(delta) < 1e-6) {
    delta = deltaY;
  }

  int sliceDelta = static_cast<int>(delta);
  if (sliceDelta == 0) {
    sliceDelta = delta > 0 ? 1 : -1;
  }

  // Get new slice index
  int sliceIndex = this->Internal->CurrentSliceIndices[flatDimension] + sliceDelta;
  this->setSliceIndex(flatDimension, sliceIndex);
}

//----------------------------------------------------------------------------
void vesKiwiImageWidgetRepresentation::setSliceIndex(int planeIndex, int sliceIndex)
{
  int dimensions[3];
  vtkImageData::SafeDownCast(this->Internal->SliceFilter->GetInput())->GetDimensions(dimensions);

  if (sliceIndex < 0) {
    sliceIndex = 0;
  }
  else if (sliceIndex >= dimensions[planeIndex]) {
    sliceIndex = dimensions[planeIndex] - 1;
  }

  if (planeIndex == 0) {
    this->Internal->SliceFilter->SetVOI(sliceIndex, sliceIndex, 0, dimensions[1], 0, dimensions[2]);
  }
  else if (planeIndex == 1) {
    this->Internal->SliceFilter->SetVOI(0, dimensions[0], sliceIndex, sliceIndex, 0, dimensions[2]);
  }
  else {
    this->Internal->SliceFilter->SetVOI(0, dimensions[0], 0, dimensions[1], sliceIndex, sliceIndex);
  }

  this->Internal->SliceFilter->Update();
  vtkImageData* sliceImage = this->Internal->SliceFilter->GetOutput();

  vesKiwiImagePlaneDataRepresentation* rep = this->Internal->SliceReps[planeIndex];
  rep->setImageData(sliceImage);
  this->Internal->AppendFilter->GetInput(planeIndex)->DeepCopy(rep->imagePlanePolyData());
  this->Internal->CurrentSliceIndices[planeIndex] = sliceIndex;
}

//----------------------------------------------------------------------------
bool vesKiwiImageWidgetRepresentation::handleSingleTouchPanGesture(double deltaX, double deltaY)
{
  if (!this->scrollSliceModeActive())
    return false;

  this->scrollImageSlice(deltaX, deltaY);
  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiImageWidgetRepresentation::handleSingleTouchDown(int displayX, int displayY)
{
  // calculate the focal depth so we'll know how far to move
  vesRenderer* ren = this->renderer();

  // flip Y coordinate
  displayY = ren->height() - displayY;

  vesCamera* camera = ren->camera();
  vesVector3f cameraFocalPoint = camera->GetFocalPoint();
  vesVector3f cameraPosition = camera->GetPosition();
  vesVector3f displayFocus = ren->computeWorldToDisplay(cameraFocalPoint);
  float focalDepth = displayFocus[2];

  vesVector3f rayPoint0 = cameraPosition;
  vesVector3f rayPoint1 = ren->computeDisplayToWorld(vesVector3f(displayX, displayY, focalDepth));

  vesVector3f rayDirection = rayPoint1 - rayPoint0;

  gmtl::normalize(rayDirection);
  rayDirection *= 1000.0;
  rayPoint1 += rayDirection;

  vtkNew<vtkCellLocator> locator;
  this->Internal->AppendFilter->Update();
  locator->SetDataSet(this->Internal->AppendFilter->GetOutput());
  locator->BuildLocator();

  double p0[3] = {rayPoint0[0], rayPoint0[1], rayPoint0[2]};
  double p1[3] = {rayPoint1[0], rayPoint1[1], rayPoint1[2]};

  double pickPoint[3];
  double t;
  double paramCoords[3];
  vtkIdType cellId = -1;
  int subId;

  int result = locator->IntersectWithLine(p0, p1, 0.0, t, pickPoint, paramCoords, subId, cellId);
  if (result == 1) {
    this->Internal->SelectedImageDimension = cellId;
    return true;
  }
  else {
    this->Internal->SelectedImageDimension = -1;
    return false;
  }
}

//----------------------------------------------------------------------------
bool vesKiwiImageWidgetRepresentation::handleDoubleTap()
{
  this->Internal->ContourVis = (this->Internal->ContourVis + 1) % 3;
  if (this->Internal->ContourVis == 0) {
    this->Internal->ContourRep->removeSelfFromRenderer(this->renderer());
  }
  else if (this->Internal->ContourVis == 1) {
    this->Internal->ContourRep->addSelfToRenderer(this->renderer());
    this->Internal->ContourRep->mapper()->setColor(0.8, 0.8, 0.8, 0.3);
  }
  else {
    this->Internal->ContourRep->mapper()->setColor(0.8, 0.8, 0.8, 1.0);
  }

  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiImageWidgetRepresentation::handleSingleTouchUp()
{
  if (!this->scrollSliceModeActive())
    return false;

  this->Internal->SelectedImageDimension = -1;
  return true;
}

//----------------------------------------------------------------------------
vesRenderer* vesKiwiImageWidgetRepresentation::renderer()
{
  return this->Internal->Renderer;
}

//----------------------------------------------------------------------------
void vesKiwiImageWidgetRepresentation::addSelfToRenderer(vesRenderer* renderer)
{
  this->Internal->Renderer = renderer;
  for (size_t i = 0; i < this->Internal->AllReps.size(); ++i)
    this->Internal->AllReps[i]->addSelfToRenderer(renderer);
}

//----------------------------------------------------------------------------
void vesKiwiImageWidgetRepresentation::removeSelfFromRenderer(vesRenderer* renderer)
{
  this->Internal->Renderer = 0;
  for (size_t i = 0; i < this->Internal->AllReps.size(); ++i)
    this->Internal->AllReps[i]->removeSelfFromRenderer(renderer);
}

//----------------------------------------------------------------------------
int vesKiwiImageWidgetRepresentation::numberOfFacets()
{
  int count = 0;
  for (size_t i = 0; i < this->Internal->AllReps.size(); ++i)
    count += this->Internal->AllReps[i]->numberOfFacets();
  return count;
}

//----------------------------------------------------------------------------
int vesKiwiImageWidgetRepresentation::numberOfVertices()
{
  int count = 0;
  for (size_t i = 0; i < this->Internal->AllReps.size(); ++i)
    count += this->Internal->AllReps[i]->numberOfVertices();
  return count;
}

//----------------------------------------------------------------------------
int vesKiwiImageWidgetRepresentation::numberOfLines()
{
  int count = 0;
  for (size_t i = 0; i < this->Internal->AllReps.size(); ++i)
    count += this->Internal->AllReps[i]->numberOfLines();
  return count;
}