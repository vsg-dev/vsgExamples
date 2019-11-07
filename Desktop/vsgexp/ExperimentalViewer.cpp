#include "ExperimentalViewer.h"

using namespace vsg;

ExperimentalViewer::ExperimentalViewer()
{
}

ExperimentalViewer::~ExperimentalViewer()
{
}

void ExperimentalViewer::addWindow(ref_ptr<Window> window)
{
    Viewer::addWindow(window);
}

bool ExperimentalViewer::advanceToNextFrame()
{
    return Viewer::advanceToNextFrame();
}

void ExperimentalViewer::advance()
{
    Viewer::advance();
}

void ExperimentalViewer::handleEvents()
{
    Viewer::handleEvents();
}

void ExperimentalViewer::reassignFrameCache()
{
    Viewer::reassignFrameCache();
}

bool ExperimentalViewer::acquireNextFrame()
{
    return Viewer::acquireNextFrame();
}

bool ExperimentalViewer::populateNextFrame()
{
    return Viewer::populateNextFrame();
}

bool ExperimentalViewer::submitNextFrame()
{
    return Viewer::submitNextFrame();
}

void ExperimentalViewer::compile(BufferPreferences bufferPreferences)
{
    Viewer::compile(bufferPreferences);
}
