#pragma once

#include "juce_graphics/juce_graphics.h"

namespace mitmegy
{

constexpr int COMPONENT_MARGIN = 7;

const juce::Colour BACKGROUND_COLOR = juce::Colours::darkgrey;

constexpr size_t TIMEOUT_NORMAL = 200;
constexpr size_t TIMEOUT_LONG = 3600'000;

} // namespace mitmegy
