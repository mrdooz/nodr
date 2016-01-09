#include "ofApp.h"

//--------------------------------------------------------------
static const int FONT_HEIGHT = 12;
static const int FONT_PADDING = 4;
static const int HEADING_SIZE = FONT_HEIGHT + 2 * FONT_PADDING;
static const int RECT_UPPER_ROUNDING = 4;
static const int RECT_LOWER_ROUNDING = 2;
static const int INPUT_HEIGHT = 14;
static const int INPUT_PADDING = 5;
static const int MIN_NODE_WIDTH = 100;

//--------------------------------------------------------------
// NB: The GLUT modifiers always returned 0, so I had to roll my own *shrug*
enum KeyMods
{
  KeyModShift = 0x1,
  KeyModAlt = 0x2,
  KeyModCtrl = 0x4,
};

static unsigned char g_modState = 0;

//--------------------------------------------------------------
bool ofKeyAlt()
{
  return g_modState & KeyModAlt;
}

//--------------------------------------------------------------
bool ofKeyShift()
{
  return g_modState & KeyModShift;
}

//--------------------------------------------------------------
bool ofKeyControl()
{
  return g_modState & KeyModCtrl;
}

//--------------------------------------------------------------
void drawStringCentered(const string& str, const ofTrueTypeFont& font, const ofRectangle& rect)
{
  ofRectangle r = font.getStringBoundingBox(str, rect.x, rect.y);
  font.drawString(str, rect.x + (rect.width - r.width) / 2, rect.y + r.height + FONT_PADDING);
}

//--------------------------------------------------------------
void Node::draw()
{
  int u = RECT_UPPER_ROUNDING;
  int l = RECT_LOWER_ROUNDING;

  // Draw body
  ofSetColor(selected ? 150 : 95);
  ofDrawRectRounded(rect, u, u, l, l);

  ofSetColor(78);
  ofDrawRectRounded(ofRectangle(rect.x, rect.y, rect.width, HEADING_SIZE), u, u, l, l);
  ofNoFill();
  ofSetColor(30);
  ofDrawRectRounded(ofRectangle(rect.x, rect.y, rect.width, HEADING_SIZE), u, u, l, l);
  ofDrawRectRounded(rect, u, u, l, l);
  ofFill();

  // Draw heading
  ofSetColor(0);
  drawStringCentered(name, *font, rect);

  // Draw inputs
  int y = HEADING_SIZE + FONT_HEIGHT + INPUT_PADDING + rect.y;
  for (const NodeConnector& input : inputs)
  {
    ofSetColor(0);
    font->drawString(input.name, rect.x + INPUT_PADDING, y);
    y += INPUT_PADDING + INPUT_HEIGHT;

    ofSetColor(95);
    ofDrawRectRounded(ofRectangle(rect.x - 10, y, 10, 10), 3, 0, 0, 3);

    ofNoFill();
    ofSetColor(30);
    ofDrawRectRounded(ofRectangle(rect.x - 10, y, 10, 10), 3, 0, 0, 3);
    ofFill();

    ofSetLineWidth(3);
    if (input.node)
    {
      ofPolyline p;
      p.quadBezierTo(ofPoint(rect.x, rect.y),
          ofPoint((rect.x + input.node->rect.x) / 3, (rect.y + input.node->rect.y) / 3),
          ofPoint(input.node->rect.x, input.node->rect.y));

      p.draw();
    }
    ofSetLineWidth(1);
  }
}

//--------------------------------------------------------------
Scene::Scene()
{
}

//--------------------------------------------------------------
Scene::~Scene()
{
  for (Node* node : _nodes)
  {
    delete node;
  }
  _nodes.clear();
}

//--------------------------------------------------------------
bool Scene::setup()
{
  _verdana14.load("verdana.ttf", FONT_HEIGHT, true, true);
  _verdana14.setLineHeight(FONT_HEIGHT);
  _verdana14.setLetterSpacing(1.037);

  // clang-format off
  _nodeTemplates[NodeType::Create] = NodeTemplate{
    "Create",
    {},
    { {"color", ParamType::Color} },
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

  _nodeTemplates[NodeType::Load] = NodeTemplate{
    "Load",
    {},
    { { "source", ParamType::Texture } },
    ParamType::Texture
  };

  _nodeTemplates[NodeType::Store] = NodeTemplate{
    "Store",
    { {"sink", ParamType::Texture} },
    {},
    ParamType::Void
  };
  // clang-format on

  for (auto& kv : _nodeTemplates)
  {
    kv.second.rect = calcTemplateRectangle(kv.second);
  }

  return true;
}

//--------------------------------------------------------------
void Scene::draw()
{
  for (auto& node : _nodes)
  {
    node->draw();
  }
}

//--------------------------------------------------------------
bool Scene::tryAddNode(int x, int y)
{
  const NodeTemplate& t = _nodeTemplates[_createType];

  ofRectangle cand = t.rect;
  cand.translate(x, y);

  for (auto& node : _nodes)
  {
    if (cand.intersects(node->rect))
    {
      // candidate overlaps with an existing rectangle, so bail
      return false;
    }
  }

  // Create the node, and add the inputs
  Node* node = new Node(t.name, cand, &_verdana14);
  _nodes.push_back(node);

  for (const NodeTemplate::NodeParam& input : t.inputs)
  {
    node->inputs.push_back(NodeConnector(input.name, input.type, NodeConnector::Dir::Input));
  }

  node->output.type = t.output;
  node->output.dir = NodeConnector::Dir::Output;
  node->output.name = "out";

  return true;
}

//--------------------------------------------------------------
Node* Scene::nodeAtPoint(const ofPoint& pt)
{
  for (auto& node : _nodes)
  {
    if (node->rect.inside(pt))
      return node;
  }

  return nullptr;
}

//--------------------------------------------------------------
void Scene::clearSelection()
{
  for (auto& node : _selectedNodes)
    node->selected = false;
  _selectedNodes.clear();
}

//--------------------------------------------------------------
void Scene::mouseDragged(int x, int y, int button)
{
  ofPoint pt(x, y);

  if (_mode == Mode::Default)
  {
    _lastDragPos = _dragStart = pt;
    for (auto& node : _selectedNodes)
      node->dragStart = node->rect.getPosition();
    _mode = Mode::Dragging;
  }

  ofPoint delta = pt - _lastDragPos;
  for (auto& node : _selectedNodes)
  {
    node->rect.translate(delta);
  }

  _lastDragPos = pt;
}

//--------------------------------------------------------------
NodeConnector* Scene::connectorAtPoint(const ofPoint& pt)
{
  for (auto& node : _nodes)
  {
    for (auto& input : node->inputs)
    {
      // skip already connected nodes
      if (input.node)
        continue;

      if (input.rect.inside(pt))
      {
        return &input;
      }
    }

    // check the output node
    if (node->output.type != ParamType::Void && node->output.rect.inside(pt))
    {
      return &node->output;
    }
  }

  return nullptr;
}

//--------------------------------------------------------------
void Scene::mousePressed(int x, int y, int button)
{
  ofPoint pt(x, y);
  auto node = nodeAtPoint(pt);
  if (!node)
  {
    clearSelection();

    // check if we clicked on any node connectors
    _connector = connectorAtPoint(pt);
    if (_connector)
    {
      _mode = Mode::Connecting;
    }
    return;
  }

  // clicking on an already selected node is a no-op
  if (!node->selected)
  {
    // ctrl allows multi-select
    if (!ofKeyControl())
      clearSelection();

    node->selected = true;
    _selectedNodes.push_back(node);
  }
}

//--------------------------------------------------------------
void Scene::mouseReleased(int x, int y, int button)
{
  if (_mode == Mode::Create)
  {
    tryAddNode(x, y);
  }

  resetState();
}

//--------------------------------------------------------------
void Scene::keyPressed(int key)
{
}

//--------------------------------------------------------------
void Scene::keyReleased(int key)
{
  if (key == OF_KEY_ESC)
  {
    if (_mode == Mode::Dragging)
    {
      for (auto& node : _selectedNodes)
        node->rect.setPosition(node->dragStart);
    }
    clearSelection();
    _mode = Mode::Default;
  }
}

//--------------------------------------------------------------
void Scene::abortAction()
{
  // abort current action, reset draggers etc.
}

//--------------------------------------------------------------
void Scene::resetState()
{
  _mode = Mode::Default;
}

//--------------------------------------------------------------
ofRectangle Scene::calcTemplateRectangle(const NodeTemplate& node)
{
  int numRows = max(1, (int)node.inputs.size());
  int h = 2 * FONT_PADDING + FONT_HEIGHT + 2 * INPUT_PADDING + numRows * INPUT_HEIGHT
          + (numRows - 1) * INPUT_PADDING;

  int strWidth = (int)ceil(_verdana14.stringWidth(node.name));
  for (const NodeTemplate::NodeParam& p : node.inputs)
  {
    strWidth = max(strWidth, (int)ceil(_verdana14.stringWidth(p.name)));
  }

  if (node.output != ParamType::Void)
    strWidth += (int)ceil(_verdana14.stringWidth("out"));

  return ofRectangle(ofPoint(0, 0), max(MIN_NODE_WIDTH, strWidth), h);
}

//--------------------------------------------------------------
void ofApp::setupCreateNode(const void* sender)
{
  NodeType type = _buttonToType[sender];
  _scene._createType = type;
  _scene._mode = Mode::Create;
}

//--------------------------------------------------------------
void ofApp::setup()
{
  ofSetVerticalSync(true);
  ofGetMainLoop()->setEscapeQuitsLoop(false);

  auto& fnAddButton = [this](
      ofxPanel* panel, vector<ofxButton*>* buttons, const string& name, NodeType type) {
    ofxButton* b = new ofxButton();
    b->setup(name);
    b->addListener(this, &ofApp::setupCreateNode);
    buttons->push_back(b);
    panel->add(b);
    _buttonToType[b] = type;
  };

  _genPanel.setup("Generators");

  fnAddButton(&_genPanel, &_genButtons, "Create", NodeType::Create);

  _modPanel.setup("Modifiers");
  fnAddButton(&_modPanel, &_modButtons, "Modulate", NodeType::Modulate);

  _memPanel.setup("Memory");
  fnAddButton(&_memPanel, &_memButtons, "Load", NodeType::Load);
  fnAddButton(&_memPanel, &_memButtons, "Store", NodeType::Store);

  _mainPanel.setup("TextureGen");
  _mainPanel.add(&_genPanel);
  _mainPanel.add(&_modPanel);
  _mainPanel.add(&_memPanel);

  _scene.setup();
}

//--------------------------------------------------------------
void ofApp::exit()
{
}

//--------------------------------------------------------------
void ofApp::update()
{
}

//--------------------------------------------------------------
void ofApp::draw()
{
  ofBackgroundGradient(ofColor::white, ofColor::gray);

  _mainPanel.draw();

  _scene.draw();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
  if (key == OF_KEY_SHIFT)
    g_modState |= KeyModShift;

  if (key == OF_KEY_CONTROL)
    g_modState |= KeyModCtrl;

  if (key == OF_KEY_ALT)
    g_modState |= KeyModAlt;

  _scene.keyPressed(key);
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
  if (key == OF_KEY_SHIFT)
    g_modState &= ~KeyModShift;

  if (key == OF_KEY_CONTROL)
    g_modState &= ~KeyModCtrl;

  if (key == OF_KEY_ALT)
    g_modState &= ~KeyModAlt;

  _scene.keyReleased(key);
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
  _scene.mouseDragged(x, y, button);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
  _scene.mousePressed(x, y, button);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
  _scene.mouseReleased(x, y, button);
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg)
{
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{
}
