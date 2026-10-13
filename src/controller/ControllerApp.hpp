#pragma once

#include "MessageHandler.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

namespace mitmegy
{

class ControllerApp : public juce::JUCEApplication
{
public:
  const juce::String getApplicationName() override;
  const juce::String getApplicationVersion() override;

  bool moreThanOneInstanceAllowed() override;
  void anotherInstanceStarted(const juce::String&) override;

  void initialise(const juce::String&) override;
  void shutdown() override;
  void systemRequestedQuit() override;

private:
  class MainWindow;

  MessageHandler message_handler_;
  std::unique_ptr<MainWindow> main_window_;

}; // ControllerApp

class ControllerApp::MainWindow : public juce::DocumentWindow
{
public:
  explicit MainWindow(ControllerApp& app);
  void closeButtonPressed() override;

private:
  ControllerApp& app_;
}; // ControllerApp::MainWindow

} // namespace mitmegy
