#include "ofApp.h"

void Scene::initNodes()
{
  // clang-format off
  _nodeTemplates[NodeType::Create] = NodeTemplate{
    "Create",
    {},
    { { "color", ParamType::Color } },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::RadialGradient] = NodeTemplate{
    "RadialGradient",
    {},
    {
      { "center", ParamType::Vec2 },
      { "power", ParamType::Float },
    },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::LinearGradient] = NodeTemplate{
    "LinearGradient",
    {},
    {
      { "pt0", ParamType::Vec2 },
      { "pt1", ParamType::Vec2 },
      { "power", ParamType::Float },
    },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::Sinus] = NodeTemplate{
    "Sinus",
    {},
    {
      { "freq", ParamType::Float },
      { "amp", ParamType::Float },
      { "power", ParamType::Float },
    },
    ParamType::Texture
  };


  _nodeTemplates[NodeType::Modulate] = NodeTemplate{
    "Modulate",
    {
      { "a", ParamType::Texture },
      { "b", ParamType::Texture },
    },
    {
      { "factor_a", ParamType::Float },
      { "factor_b", ParamType::Float }
    },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::RotateScale] = NodeTemplate{
    "RotateScale",
    {
      { "a", ParamType::Texture },
    },
    {
      { "angle", ParamType::Float },
      { "scale", ParamType::Vec2 }
    },
    ParamType::Texture
  };


  _nodeTemplates[NodeType::Distort] = NodeTemplate{
    "Distort",
    {
      { "a", ParamType::Texture },
      { "b", ParamType::Texture },
      { "c", ParamType::Texture },
    },
    {
      { "scale", ParamType::Float }
    },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::ColorGradient] = NodeTemplate{
    "ColorGradient",
    {
      { "a", ParamType::Texture },
    },
    {
      { "colA", ParamType::Color },
      { "colB", ParamType::Color },
    },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::Load] = NodeTemplate{
    "Load",
    {},
    { { "source", ParamType::Texture } },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::Store] = NodeTemplate{
    "Store",
    { { "sink", ParamType::Texture } },
    {},
    ParamType::Void
  };
  // clang-format on

  for (auto& kv : _nodeTemplates)
  {
    kv.second.rect = calcTemplateRectangle(kv.second);
  }
}