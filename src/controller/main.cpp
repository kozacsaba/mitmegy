#include "MessageHandler.hpp"

#include "protocol/messages.hpp"

#include <windows.h>

#include <iostream>
#include <exception>
#include <atomic>
#include <sstream>
#include <string>

namespace
{

std::atomic_bool g_running{true};

BOOL WINAPI consoleHandler(DWORD event)
{
  if (event == CTRL_C_EVENT)
  {
    g_running.store(false, std::memory_order_relaxed);
    g_running.notify_all();
    return TRUE;
  }

  return FALSE;
}

} // namespace

int main()
{

  mitmegy::MessageHandler handler;
  handler.start();

  while(g_running)
  {
    std::string line;
    std::getline(std::cin, line);

    std::istringstream stream(line);

    try
    {
      std::string word;
      stream >> word;

      if(word == "query")
      {
        std::string subject;
        stream >> subject;
        if(subject == "plugin")
          handler.send_query((size_t) mitmegy::msg::query_subject::plugin_list);
        else if(subject == "chain")
          handler.send_query((size_t) mitmegy::msg::query_subject::active_chain);
        else if(subject == "dev")
          handler.send_query((size_t) mitmegy::msg::query_subject::audio_devices);
        else if(subject == "path")
          handler.send_query((size_t) mitmegy::msg::query_subject::search_paths);
        else
          std::cout << "unrecognised subject: " << subject << std::endl;
      }
      else if(word == "addplugin")
      {
        std::string arg;
        stream >> arg;
        const size_t id = std::stoi(arg);

        stream >> arg;
        const int idx = std::stoi(arg);

        handler.send_add_plugin(id, idx);
      }
      else if(word == "bypass")
      {
        std::string arg;
        stream >> arg;
        const size_t id = std::stoi(arg);

        stream >> arg;
        const int bypass = std::stoi(arg);
        handler.send_bypass(id, bypass == 1);
      }
      else if(word == "rmplugin")
      {
        std::string arg;
        stream >> arg;
        const size_t id = std::stoi(arg);
        handler.send_remove_plugin(id);
      }
      else if(word == "mvplugin")
      {
        std::string arg;
        stream >> arg;
        const size_t id = std::stoi(arg);

        stream >> arg;
        const int idx = std::stoi(arg);
        handler.send_move_plugin(id, idx);
      }
      else if(word == "gui")
      {
        std::string arg;
        stream >> arg;
        const size_t id = std::stoi(arg);
        handler.send_open_gui(id);
      }
      else if(word == "rescan")
      {
        handler.send_rescan();
      }
      else if(word == "addpath")
      {
        std::string path;
        stream >> path;
        handler.send_add_path(path);
      }
      else if(word == "rmpath")
      {
        std::string path;
        stream >> path;
        handler.send_remove_path(path);
      }
      else if(word == "device")
      {
        std::string id;
        stream >> id;
        handler.send_select_device(id);
      }
      else if(word == "save")
      {
        std::string path;
        stream >> path;
        handler.send_save_snapshot(path);
      }
      else if(word == "load")
      {
        std::string path;
        stream >> path;
        handler.send_load_snapshot(path);
      }
      else
      {
        std::cout << "unrecognised command " << word << std::endl;
      }
    }
    catch(const std::exception& e)
    {
      std::cout << "Error: " << e.what() << std::endl;
    }
  } // while running

  handler.stop();
  return 0;
}
