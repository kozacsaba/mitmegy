#include "ControllerApp.hpp"
#include "gui/MainComponent.hpp"

#ifndef MITMEGY_VERSION
  #define MITMEGY_VERSION untracked
#endif // MITMEGY_VERSION

#ifndef stringify_impl
  #define stringify_impl(x) #x
#endif // stringify_impl

#ifndef stringify
  #define stringify(x) stringify_impl(x)
#endif // stringify

namespace
{
  constexpr char APPLICATION_NAME[] = "Mitmegy Controller";
  constexpr char APPLICATION_VERSION_STRING[] = stringify(MITMEGY_VERSION);
} // namespace

namespace mitmegy
{

// ControllerApp

ControllerApp::ControllerApp()
  : request_manager_(message_handler_)
{}

const juce::String ControllerApp::getApplicationName()
{
  return APPLICATION_NAME;
}

const juce::String ControllerApp::getApplicationVersion()
{
  return APPLICATION_VERSION_STRING;
}

bool ControllerApp::moreThanOneInstanceAllowed()
{
  return false;
}

void ControllerApp::initialise(const juce::String&)
{
  message_handler_.start();
  request_manager_.start();
  main_window_ = std::make_unique<MainWindow>(*this);
}

void ControllerApp::shutdown()
{
  main_window_.reset();
  request_manager_.stop();
  message_handler_.stop();
}

void ControllerApp::systemRequestedQuit()
{
  quit();
}

void ControllerApp::anotherInstanceStarted(const juce::String&)
{
  if(main_window_)
    main_window_->toFront(true);
}

// ControllerApp::MainWindow

ControllerApp::MainWindow::MainWindow(ControllerApp& app)
  : DocumentWindow(APPLICATION_NAME,
                   juce::Colours::darkgrey,
                   juce::DocumentWindow::allButtons)
  , app_(app)
  , request_manager_(app_.getRequestManager())
{
  setUsingNativeTitleBar(true);
  setResizable(true, false);

  auto mainComponent = std::make_unique<MainComponent>(request_manager_);
  // set mainComponent up here, and/or in the ctor above
  // unique ptr is used so that failure in initialisation would not leak
  setContentOwned(mainComponent.release(), true);

  centreWithSize(900, 600);
  setVisible(true);
}

void ControllerApp::MainWindow::closeButtonPressed()
{
  app_.systemRequestedQuit();
}

} // namespace mitmegy
