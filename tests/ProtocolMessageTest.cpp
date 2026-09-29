#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "protocol/messages.hpp"

using namespace mitmegy::msg;

void test_noop()
{
  Noop original;

  const auto bytes = to_bytes(original);
  [[maybe_unused]] const auto result = from_bytes<Noop>(bytes);
}

void test_completed()
{
  Completed original;

  const auto bytes = to_bytes(original);
  [[maybe_unused]] const auto result = from_bytes<Completed>(bytes);
}

void test_rescan()
{
  Rescan original;

  const auto bytes = to_bytes(original);
  [[maybe_unused]] const auto result = from_bytes<Rescan>(bytes);
}

void test_path()
{
  Path original {
    .path = R"(C:\Program Files\Common Files\VST3)"
  };

  const auto result = from_bytes<Path>(to_bytes(original));

  assert(result.path == original.path);
}

void test_add_path()
{
  AddPath original {
    .path = R"(D:\Plugins\VST3)"
  };

  const auto result = from_bytes<AddPath>(to_bytes(original));

  assert(result.path == original.path);
}

void test_remove_path()
{
  RemovePath original {
    .path = R"(D:\Old Plugins)"
  };

  const auto result = from_bytes<RemovePath>(to_bytes(original));

  assert(result.path == original.path);
}

void test_save_snapshot()
{
  SaveSnapshot original {
    .path = R"(C:\Users\Test\snapshot.mitmegy)"
  };

  const auto result = from_bytes<SaveSnapshot>(to_bytes(original));

  assert(result.path == original.path);
}

void test_load_snapshot()
{
  LoadSnapshot original {
    .path = R"(C:\Users\Test\snapshot.mitmegy)"
  };

  const auto result = from_bytes<LoadSnapshot>(to_bytes(original));

  assert(result.path == original.path);
}

void test_error()
{
  Error original {
    .message = "Could not load plugin: invalid plugin format"
  };

  const auto result = from_bytes<Error>(to_bytes(original));

  assert(result.message == original.message);
}

void test_failed_plugins()
{
  FailedScans original = { 
    .plugins = {"plugin a", "plugin b", "plugin c"}
  };

  const auto result = from_bytes<FailedScans>(to_bytes(original));

  assert(result.plugins.size() == original.plugins.size());
  for(size_t i = 0; i < result.plugins.size(); i++)
    assert(result.plugins[i] == original.plugins[i]);
}

void test_query()
{
  Query original {
    .subject = query_subject::active_chain
  };

  const auto result = from_bytes<Query>(to_bytes(original));

  assert(result.subject == original.subject);
}

void test_list_plugins()
{
  ListPlugins original {
    .plugins = {
      {
        .id = 1,
        .plugin_name = "Compressor",
        .manufacturer = "Test Manufacturer",
        .category = "Dynamics"
      },
      {
        .id = 42,
        .plugin_name = "Super Reverb",
        .manufacturer = "Another Manufacturer",
        .category = "Reverb"
      },
      {
        .id = 999,
        .plugin_name = "Distortion",
        .manufacturer = "Example Audio",
        .category = "Distortion"
      }
    }
  };

  const auto result = from_bytes<ListPlugins>(to_bytes(original));

  assert(result.plugins.size() == original.plugins.size());

  for (size_t i = 0; i < original.plugins.size(); ++i)
  {
    assert(result.plugins[i].id == original.plugins[i].id);
    assert(result.plugins[i].plugin_name == original.plugins[i].plugin_name);
    assert(result.plugins[i].manufacturer == original.plugins[i].manufacturer);
    assert(result.plugins[i].category == original.plugins[i].category);
  }
}

void test_list_chain()
{
  ListChain original {
    .plugins = {
      {
        .id = 10,
        .display_name = "Input Compressor",
        .bypassed = false
      },
      {
        .id = 11,
        .display_name = "EQ",
        .bypassed = true
      },
      {
        .id = 12,
        .display_name = "Limiter",
        .bypassed = false
      }
    }
  };

  const auto result = from_bytes<ListChain>(to_bytes(original));

  assert(result.plugins.size() == original.plugins.size());

  for (size_t i = 0; i < original.plugins.size(); ++i)
  {
    assert(result.plugins[i].id == original.plugins[i].id);
    assert(result.plugins[i].display_name == original.plugins[i].display_name);
    assert(result.plugins[i].bypassed == original.plugins[i].bypassed);
  }
}

void test_list_devices()
{
  ListDevices original {
    .devices = {
      {
        .id = "0",
        .name = "Speakers (Realtek Audio)"
      },
      {
        .id = "7",
        .name = "Focusrite USB ASIO"
      }
    }
  };

  const auto result = from_bytes<ListDevices>(to_bytes(original));

  assert(result.devices.size() == original.devices.size());

  for (size_t i = 0; i < original.devices.size(); ++i)
  {
    assert(result.devices[i].id == original.devices[i].id);
    assert(result.devices[i].name == original.devices[i].name);
  }
}

void test_list_paths()
{
  ListPaths original {
    .paths = {
      R"(C:\Program Files\Common Files\VST3)",
      R"(C:\Program Files\VstPlugins)",
      R"(D:\Audio\Plugins)"
    }
  };

  const auto result = from_bytes<ListPaths>(to_bytes(original));

  assert(result.paths == original.paths);
}

void test_add_plugin()
{
  AddPlugin original {
    .id = 1234,
    .index = 3
  };

  const auto result = from_bytes<AddPlugin>(to_bytes(original));

  assert(result.id == original.id);
  assert(result.index == original.index);
}

void test_add_plugin_append()
{
  AddPlugin original {
    .id = 5678,
    .index = -1
  };

  const auto result = from_bytes<AddPlugin>(to_bytes(original));

  assert(result.id == original.id);
  assert(result.index == -1);
}

void test_bypass_plugin()
{
  BypassPlugin original {
    .id = 45,
    .bypass = true
  };

  const auto result = from_bytes<BypassPlugin>(to_bytes(original));

  assert(result.id == original.id);
  assert(result.bypass == original.bypass);
}

void test_unbypass_plugin()
{
  BypassPlugin original {
    .id = 45,
    .bypass = false
  };

  const auto result = from_bytes<BypassPlugin>(to_bytes(original));

  assert(result.id == original.id);
  assert(result.bypass == original.bypass);
}

void test_move_plugin()
{
  MovePlugin original {
    .id = 77,
    .index = 4
  };

  const auto result = from_bytes<MovePlugin>(to_bytes(original));

  assert(result.id == original.id);
  assert(result.index == original.index);
}

void test_simple_id()
{
  SimpleId original {
    .id = 123456
  };

  const auto result = from_bytes<SimpleId>(to_bytes(original));

  assert(result.id == original.id);
}

void test_remove_plugin()
{
  RemovePlugin original {
    .id = 100
  };

  const auto result = from_bytes<RemovePlugin>(to_bytes(original));

  assert(result.id == original.id);
}

void test_open_gui()
{
  OpenGui original {
    .id = 200
  };

  const auto result = from_bytes<OpenGui>(to_bytes(original));

  assert(result.id == original.id);
}

void test_select_device()
{
  SelectDevice original {
    .id = "300"
  };

  const auto result = from_bytes<SelectDevice>(to_bytes(original));

  assert(result.id == original.id);
}

void test_empty_plugin_list()
{
  ListPlugins original;

  const auto result = from_bytes<ListPlugins>(to_bytes(original));

  assert(result.plugins.empty());
}

void test_empty_chain()
{
  ListChain original;

  const auto result = from_bytes<ListChain>(to_bytes(original));

  assert(result.plugins.empty());
}

void test_empty_device_list()
{
  ListDevices original;

  const auto result = from_bytes<ListDevices>(to_bytes(original));

  assert(result.devices.empty());
}

void test_empty_path_list()
{
  ListPaths original;

  const auto result = from_bytes<ListPaths>(to_bytes(original));

  assert(result.paths.empty());
}

void test_empty_strings()
{
  Error error {
    .message = ""
  };

  Path path {
    .path = ""
  };

  const auto error_result = from_bytes<Error>(to_bytes(error));
  const auto path_result = from_bytes<Path>(to_bytes(path));

  assert(error_result.message.empty());
  assert(path_result.path.empty());
}

int main()
{
  test_noop();
  test_completed();
  test_rescan();

  test_path();
  test_add_path();
  test_remove_path();
  test_save_snapshot();
  test_load_snapshot();

  test_error();
  test_query();

  test_list_plugins();
  test_list_chain();
  test_list_devices();
  test_list_paths();

  test_add_plugin();
  test_add_plugin_append();
  test_bypass_plugin();
  test_unbypass_plugin();
  test_move_plugin();

  test_simple_id();
  test_remove_plugin();
  test_open_gui();
  test_select_device();

  test_empty_plugin_list();
  test_empty_chain();
  test_empty_device_list();
  test_empty_path_list();
  test_empty_strings();

  std::cout << "All protocol serialization tests passed.\n";

  return 0;
}
