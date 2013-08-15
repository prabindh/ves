#include <vesConfigure.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <vesActor.h>
#include <vesBackground.h>
#include <vesCamera.h>
#include <vesGeometryData.h>
#include <vesMapper.h>
#include <vesMaterial.h>
#include <vesRenderer.h>
#include <vesVertexAttribute.h>
#include <vesVertexAttributeKeys.h>
#include <vesViewport.h>

#include <vesKiwiBaseApp.h>
#include <vesKiwiBaselineImageTester.h>
#include <vesKiwiDataLoader.h>
#include <vesKiwiPolyDataRepresentation.h>

#include <vtkPolyData.h>

#ifdef VES_QNX
  #include "vesKiwiQNXTestDriver.h"
  #include <stdio.h>
  #include <stdlib.h>
  #include <errno.h>
  #include <sys/neutrino.h>
  #include <sys/syspage.h>
#elif defined (VES_HOST)
  #include "vesKiwiX11TestDriver.h"
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/keysym.h>
#endif

//----------------------------------------------------------------------------
namespace {

class vesMultipleViewports : public vesKiwiBaseApp {
public:

  vesMultipleViewports()
  {
    this->DataRep = 0;
    this->IsTesting = false;
    this->setBackgroundColor(0.8, 0.6, 0.6);
  }

  ~vesMultipleViewports()
  {
    this->unloadData();
  }

  void initClipMaterial()
  {
    vesSharedPtr<vesMaterial> material
      = this->addMaterial();
    this->addVertexPositionAttribute(material);
    this->addVertexNormalAttribute(material);
    this->addVertexColorAttribute(material);
    this->addVertexTextureCoordinateAttribute(material);
    this->ClipMaterial = material;
  }

  void unloadData()
  {
    if (this->DataRep) {
      this->DataRep->removeSelfFromRenderer(this->renderer());
      delete this->DataRep;
      this->DataRep = 0;
    }
  }

  void loadData(const std::string& filename)
  {
    this->unloadData();

    vesKiwiDataLoader loader;
    vtkSmartPointer<vtkPolyData> polyData
      = vtkPolyData::SafeDownCast(loader.loadDataset(filename));
    assert(polyData.GetPointer());

    vesKiwiPolyDataRepresentation* rep = new vesKiwiPolyDataRepresentation();
    rep->initializeWithMaterial(this->ClipMaterial);
    rep->setPolyData(polyData);
    rep->addSelfToRenderer(this->renderer());

    vesRenderer::Ptr ren = vesRenderer::Ptr(new vesRenderer());
    vesKiwiPolyDataRepresentation* rep2 = new vesKiwiPolyDataRepresentation();
    rep2->initializeWithMaterial(this->ClipMaterial);
    rep2->setPolyData(polyData);
    rep2->addSelfToRenderer(ren);
    this->addRenderer(ren);

    rep->actor()->mapper()->enableWireframe(true);
    this->DataRep = rep;

    // This should make the background color same for both renderers
    // this->setBackgroundColor(0.8, 0.6, 0.9);
  }

  std::string sourceDirectory() {
    return this->SourceDirectory;
  }

  void setSourceDirectory(std::string dir) {
    this->SourceDirectory = dir;
  }

  bool isTesting() {
    return this->IsTesting;
  }

  void setTesting(bool testing) {
    this->IsTesting = testing;
  }

  void loadData()
  {
    std::string filename = this->sourceDirectory() +
      std::string("/Apps/iOS/Kiwi/Kiwi/Data/bunny.vtp");

    this->loadData(filename);
    this->resetView();
  }

  void loadDefaultData()
  {
    this->loadData();
  }

  bool doTesting()
  {
    const double threshold = 10.0;
    const std::string testName = "Multiple Viewports";

    vesKiwiBaselineImageTester baselineTester;
    baselineTester.setApp(this);
    baselineTester.setBaselineImageDirectory(this->dataDirectory());
    return baselineTester.performTest(testName, threshold);
  }

  std::string getFileContents(const std::string& filename)
  {
    std::ifstream file(filename.c_str());
    std::stringstream buffer;
    if (file) {
      buffer << file.rdbuf();
      file.close();
    }
    return buffer.str();
  }

  void initRendering()
  {
    this->initClipMaterial();
  }

  bool initTest(int argc, char* argv[])
  {
    if (argc < 2) {
      printf("Usage: %s <path to VES source directory> [path to testing data directory]\n", argv[0]);
      return false;
    }

    this->setSourceDirectory(argv[1]);

    if (argc == 3) {
      this->setDataDirectory(argv[2]);
      this->setTesting(true);
    }
    return true;
  }

  void finalizeTest()
  {
  }

  std::string dataDirectory() {
    return this->DataDirectory;
  }

  void setDataDirectory(std::string dir) {
    this->DataDirectory = dir;
  }

  std::string       SourceDirectory;
  std::string       DataDirectory;
  bool              IsTesting;

  vesSharedPtr<vesMaterial> ClipMaterial;
  vesKiwiPolyDataRepresentation* DataRep;
};

}; // end namespace


int
main(int argc, char *argv[])
{
  vesMultipleViewports app;

  if (!app.initTest(argc, argv)) {
    return -1;
    fprintf(stderr, "Looping\n");
  }

#ifdef VES_QNX
  vesKiwiQNXTestDriver testDriver(&app);
#elif defined (VES_HOST)
  vesKiwiX11TestDriver testDriver(&app);
#endif

  testDriver.init();

  app.initRendering();
  app.loadDefaultData();
  app.resizeView(testDriver.width(), testDriver.height());
  app.resetView();

  app.setViewRect(0, 0, 0, 500, 600);
  app.setViewRect(1, 500, 0, 300, 600);

  testDriver.render();

  // begin the event loop if not in testing mode
  bool testPassed = true;
  if (!app.isTesting()) {
    testDriver.start();
  }
  else {
    testPassed = app.doTesting();
  }

  app.finalizeTest();
  testDriver.finalize();

  return testPassed ? 0 : 1;
}