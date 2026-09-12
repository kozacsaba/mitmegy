#include "vsthost/PluginRegistry.hpp"

#include <iostream>

int main()
{
  mitmegy::PluginRegistry registry;

  const auto failed = registry.scan();
  const auto& plugins = registry.plugins().getTypes();

  std::cout << "\nDiscovered plugins: "
            << plugins.size()
            << "\n\n";

  for (int i = 0; i < plugins.size(); ++i)
  {
    const auto& plugin = plugins.getReference(i);

    std::cout
        << "[" << i << "] "
        << plugin.name
        << "\n"
        << "    Manufacturer: "
        << plugin.manufacturerName
        << "\n"
        << "    Format: "
        << plugin.pluginFormatName
        << "\n"
        << "    Identifier: "
        << plugin.createIdentifierString()
        << "\n"
        << "    File: "
        << plugin.fileOrIdentifier
        << "\n\n";
  }

  if (!failed.isEmpty())
  {
    std::cout << "Failed to scan:\n";

    for (const auto& file : failed)
      std::cout << "    " << file << '\n';
  }

  return 0;
}
