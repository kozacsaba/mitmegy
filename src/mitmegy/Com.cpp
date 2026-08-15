#include "Com.hpp"

namespace mitmegy
{

ComScope::ComScope(DWORD concurrency)
  : result_(CoInitializeEx(nullptr, concurrency))
{}

ComScope::~ComScope()
{
  if (SUCCEEDED(result_))
    CoUninitialize();
}

bool ComScope::ok() const
{
  return SUCCEEDED(result_);
}

HRESULT ComScope::result() const
{
  return result_;
}

} // namespace mitmegy
