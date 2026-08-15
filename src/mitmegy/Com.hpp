#pragma once

#include <windows.h>

namespace mitmegy
{

class ComScope
{
public:
  explicit ComScope(DWORD concurrency = COINIT_MULTITHREADED);
  ~ComScope();

  ComScope(const ComScope&) = delete;
  ComScope& operator=(const ComScope&) = delete;
  ComScope(ComScope&&) = delete;
  ComScope& operator=(ComScope&&) = delete;

  bool ok() const;
  HRESULT result() const;

private:
  HRESULT result_;
};

} // namespace mitmegy
