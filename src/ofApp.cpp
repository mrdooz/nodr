#include "ofApp.h"

//--------------------------------------------------------------
static const int FONT_HEIGHT = 12;
static const int FONT_PADDING = 4;
static const int HEADING_SIZE = FONT_HEIGHT + 2 * FONT_PADDING;
static const int RECT_UPPER_ROUNDING = 4;
static const int RECT_LOWER_ROUNDING = 2;
static const int INPUT_HEIGHT = 14;
static const int INPUT_PADDING = 4;
static const int CONNECTOR_RADIUS = 5;
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
void drawStringCentered(const string& str,
    const ofTrueTypeFont& font,
    const ofRectangle& rect,
    bool centerHoriz,
    bool centerVert)
{
  // note, the y-coord for strings is the bottom left corner
  ofRectangle r = font.getStringBoundingBox(str, rect.x, rect.y);
  int dx = centerHoriz ? (rect.width - r.width) / 2 : 0;
  int dy = centerVert ? (rect.height - r.height) / 2 : 0;
  font.drawString(str, rect.getLeft() + dx, rect.getBottom() - dy);
}

//--------------------------------------------------------------
void drawOutlineRect(
    const ofRectangle& rect, const ofColor& fill, int upperRounding, int lowerRounding)
{
  ofSetColor(fill);
  ofDrawRectRounded(rect, upperRounding, upperRounding, lowerRounding, lowerRounding);
  ofNoFill();
  ofSetColor(30);
  ofDrawRectRounded(rect, upperRounding, upperRounding, lowerRounding, lowerRounding);
  ofFill();
}

//--------------------------------------------------------------
void drawOutlineCircle(const ofPoint& pt, float radius, const ofColor& fill)
{
  ofSetColor(fill);
  ofDrawCircle(pt, radius);
  ofNoFill();
  ofSetColor(30);
  ofDrawCircle(pt, radius);
  ofFill();
}

//--------------------------------------------------------------
Node::Node(const NodeTemplate& t, const ofPoint& pt, Scene* scene) : name(t.name), scene(scene)
{
  bodyRect = t.rect;
  bodyRect.translate(pt);

  headingRect = bodyRect;
  float h = 2 * FONT_PADDING + scene->_font.stringHeight(name);
  headingRect.setHeight(h);
  headingRect.translateY(-h);

  int y = bodyRect.y + INPUT_PADDING;
  for (size_t i = 0; i < t.inputs.size(); ++i)
  {
    const NodeTemplate::NodeParam& input = t.inputs[i];
    inputs.push_back(NodeConnector(input.name,
        input.type,
        NodeConnector::Dir::Input,
        ofPoint(bodyRect.x + INPUT_PADDING + CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2),
        this));
    y += INPUT_HEIGHT + INPUT_PADDING;
  }

  for (const NodeTemplate::NodeParam& param : t.params)
  {
    params.push_back(Node::Param(param.name, param.type));
  }

  output = NodeConnector("out",
      t.output,
      NodeConnector::Dir::Output,
      ofPoint(bodyRect.getRight() - INPUT_PADDING - CONNECTOR_RADIUS,
                             bodyRect.y + INPUT_PADDING + INPUT_HEIGHT / 2),
      this);
}

//--------------------------------------------------------------
void Node::translate(const ofPoint& delta)
{
  bodyRect.translate(delta);
  headingRect.translate(delta);

  for (auto& input : inputs)
    input.pt += delta;

  output.pt += delta;
}

//--------------------------------------------------------------
void Node::draw()
{
  // Draw body
  drawOutlineRect(bodyRect, 95, 0, RECT_LOWER_ROUNDING);

  // Draw heading
  drawOutlineRect(headingRect, 78, RECT_UPPER_ROUNDING, 0);
  ofSetColor(0);
  drawStringCentered(name, scene->_font, headingRect, true, true);

  int circleInset = CONNECTOR_RADIUS * 2 + 2 * INPUT_PADDING;

  // Draw inputs
  int y = bodyRect.y + INPUT_PADDING;
  for (const NodeConnector& input : inputs)
  {
    // each input gets its own rect, and we draw the text centered inside that
    ofRectangle rect(ofPoint(bodyRect.x + circleInset, y), bodyRect.getWidth(), INPUT_HEIGHT);
    ofSetColor(0);
    drawStringCentered(input.name, scene->_font, rect, false, true);

    ofPoint pt(bodyRect.x + INPUT_PADDING + CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2);
    drawOutlineCircle(
        pt, CONNECTOR_RADIUS, input.cons.empty() ? ofColor(140) : ofColor(80, 200, 80));

    y += INPUT_HEIGHT + INPUT_PADDING;
  }

  // Draw output
  if (output.type != ParamType::Void)
  {
    int y = bodyRect.y + INPUT_PADDING;
    ofRectangle strRect = scene->_font.getStringBoundingBox(output.name, 0, 0);

    // right aligned..
    int dy = (INPUT_HEIGHT - strRect.height) / 2;
    int strX = bodyRect.getRight() - circleInset - strRect.getWidth();
    scene->_font.drawString(output.name, strX, y + INPUT_HEIGHT - dy);

    ofPoint pt(bodyRect.getRight() - INPUT_PADDING - CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2);
    drawOutlineCircle(
        pt, CONNECTOR_RADIUS, output.cons.empty() ? ofColor(140) : ofColor(80, 200, 80));
  }

  if (selected)
  {
    ofNoFill();
    ofSetLineWidth(3);
    ofSetColor(219, 136, 39);
    ofDrawRectRounded(headingRect.getTopLeft(),
        bodyRect.getWidth(),
        headingRect.getHeight() + bodyRect.getHeight(),
        RECT_UPPER_ROUNDING,
        RECT_UPPER_ROUNDING,
        RECT_LOWER_ROUNDING,
        RECT_LOWER_ROUNDING);
    ofSetLineWidth(1);
    ofFill();
  }
}

//--------------------------------------------------------------
void Node::drawConnections()
{
  ofSetLineWidth(3);
  ofSetColor(100, 100, 200);
  for (const NodeConnector* con : output.cons)
  {
    ofDrawLine(output.pt, con->pt);
  }

  ofSetLineWidth(1);
}

//--------------------------------------------------------------
bool validConnection(const NodeConnector* a, const NodeConnector* b)
{
  if (a == nullptr || b == nullptr)
    return false;

  if (a == b)
    return false;

  if (a->parent == b->parent)
    return false;

  if (a->type != b->type)
    return false;

  if (a->dir == b->dir)
    return false;

  const NodeConnector* input = a->dir == NodeConnector::Dir::Input ? a : b;
  if (!input->cons.empty())
    return false;

  return true;
}

//--------------------------------------------------------------
Scene::Scene(ofxPanel* varPanel) : _varPanel(varPanel)
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
  _font.load("verdana.ttf", FONT_HEIGHT, true, true);
  _font.setLineHeight(FONT_HEIGHT);
  _font.setLetterSpacing(1.037);

  initNodes();

  return true;
}

//--------------------------------------------------------------
void Scene::draw()
{
  for (auto& node : _nodes)
  {
    node->draw();
  }

  for (auto& node : _nodes)
  {
    node->drawConnections();
  }

  if (_mode == Mode::Connecting)
  {
    ofSetLineWidth(3);
    if (_endConnector)
    {
      if (validConnection(_startConnector, _endConnector))
        ofSetColor(100, 200, 100);
      else
        ofSetColor(200, 100, 100);

      ofDrawLine(_startConnector->pt, _endConnector->pt);
    }
    else
    {
      ofSetColor(100, 100, 200);
      ofDrawLine(_startConnector->pt, ofPoint(ofGetMouseX(), ofGetMouseY()));
    }
    ofSetLineWidth(1);
  }
}

//--------------------------------------------------------------
Node* Scene::nodeAtPoint(const ofPoint& pt)
{
  // NB, only select if the point is inside the header
  for (auto& node : _nodes)
  {
    if (node->headingRect.inside(pt))
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

  if (_mode == Mode::Connecting)
  {
    _endConnector = connectorAtPoint(pt);
    return;
  }

  if (_mode == Mode::DragStart)
  {
    _lastDragPos = _dragStart = pt;
    for (auto& node : _selectedNodes)
      node->dragStart = node->bodyRect.getPosition();
    _mode = Mode::Dragging;
  }
  else if (_mode == Mode::Dragging)
  {
    ofPoint delta = pt - _lastDragPos;
    for (auto& node : _selectedNodes)
    {
      node->translate(delta);
    }

    _lastDragPos = pt;
  }
}

//--------------------------------------------------------------
NodeConnector* Scene::connectorAtPoint(const ofPoint& pt)
{
  auto& insideConnector = [&](const ofPoint& center) {
    float dist = center.squareDistance(pt);
    return dist < CONNECTOR_RADIUS * CONNECTOR_RADIUS;
  };

  for (auto& node : _nodes)
  {
    for (auto& input : node->inputs)
    {
      // skip already connected nodes
      //if (!input.cons.empty())
      //  continue;

      if (insideConnector(input.pt))
      {
        return &input;
      }
    }

    // check the output node
    if (node->output.type != ParamType::Void && insideConnector(node->output.pt))
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

  if (_mode == Mode::Create)
  {
    const NodeTemplate& t = _nodeTemplates[_createType];
    _nodes.push_back(new Node(t, pt, this));
    resetState();
    return;
  }

  // check if we clicked on any node connectors
  _startConnector = connectorAtPoint(pt);
  if (_startConnector)
  {
    _endConnector = nullptr;
    _mode = Mode::Connecting;
    return;
  }

  // if nothing was clicked, clear the selection
  auto node = nodeAtPoint(pt);
  if (!node)
  {
    clearSelection();
    return;
  }

  // if we clicked a node, select it, and toggle speculatively to drag-mode
  _mode = Mode::DragStart;

  if (!node->selected)
  {
    // ctrl allows multi-select
    if (!ofKeyControl())
    {
      clearSelection();
      initNodeParameters(node);
    }

    node->selected = true;
    _selectedNodes.push_back(node);
  }
}

//--------------------------------------------------------------
void Scene::mouseReleased(int x, int y, int button)
{
  if (_mode == Mode::Connecting)
  {
    if (validConnection(_startConnector, _endConnector))
    {
      NodeConnector* input;
      NodeConnector* output;
      if (_startConnector->dir == NodeConnector::Dir::Input)
      {
        input = _startConnector;
        output = _endConnector;
      }
      else
      {
        input = _endConnector;
        output = _startConnector;
      }

      output->cons.push_back(input);
      input->cons.push_back(output);
    }
  }

  resetState();
}

//--------------------------------------------------------------
void Scene::mouseMoved(int x, int y)
{
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
      {
        node->bodyRect.setPosition(node->dragStart);
        node->headingRect.setPosition(node->dragStart);
      }
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
  int h = 2 * INPUT_PADDING + numRows * INPUT_HEIGHT + (numRows - 1) * INPUT_PADDING;

  int strWidth = (int)ceil(_font.stringWidth(node.name));
  for (const NodeTemplate::NodeParam& p : node.inputs)
  {
    strWidth = max(strWidth, (int)ceil(_font.stringWidth(p.name)));
  }

  if (node.output != ParamType::Void)
    strWidth += (int)ceil(_font.stringWidth("out"));

  return ofRectangle(ofPoint(0, 0), max(MIN_NODE_WIDTH, strWidth), h);
}

//--------------------------------------------------------------
void Scene::initNodeParameters(Node* node)
{
  _varPanel->clear();
  char buf[256];
  sprintf(buf, "%s vars", node->name.c_str());
  _varPanel->setup(buf);

  for (Node::Param& p : node->params)
  {
    switch (p.type)
    {
      case ParamType::Void: break;
      case ParamType::Float: _varPanel->add(p.value.fValue); break;
      case ParamType::Vec2: _varPanel->add(p.value.vValue); break;
      case ParamType::Color: _varPanel->add(p.value.cValue); break;
      case ParamType::Texture: break;
      default: break;
    }
  }
}

//--------------------------------------------------------------
void ofApp::setupCreateNode(const void* sender)
{
  NodeType type = _buttonToType[sender];
  _scene->_createType = type;
  _scene->_mode = Mode::Create;
}

//--------------------------------------------------------------
ofApp::ofApp() : _scene(nullptr)
{
}

//--------------------------------------------------------------
ofApp::~ofApp()
{
  delete _scene;
}

//--------------------------------------------------------------
void ofApp::setup()
{
  ofSetVerticalSync(true);
  ofGetMainLoop()->setEscapeQuitsLoop(false);

  _scene = new Scene(&_varPanel);

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
  fnAddButton(&_genPanel, &_genButtons, "LinearGradient", NodeType::LinearGradient);
  fnAddButton(&_genPanel, &_genButtons, "RadialGradient", NodeType::RadialGradient);
  fnAddButton(&_genPanel, &_genButtons, "Sinus", NodeType::Sinus);

  _modPanel.setup("Modifiers");
  fnAddButton(&_modPanel, &_modButtons, "Modulate", NodeType::Modulate);
  fnAddButton(&_modPanel, &_modButtons, "RotateScale", NodeType::RotateScale);
  fnAddButton(&_modPanel, &_modButtons, "Distort", NodeType::Distort);
  fnAddButton(&_modPanel, &_modButtons, "ColorGradient", NodeType::ColorGradient);

  _memPanel.setup("Memory");
  fnAddButton(&_memPanel, &_memButtons, "Load", NodeType::Load);
  fnAddButton(&_memPanel, &_memButtons, "Store", NodeType::Store);

  _varPanel.setup("Vars");

  _mainPanel.setup("TextureGen");
  _mainPanel.add(&_genPanel);
  _mainPanel.add(&_modPanel);
  _mainPanel.add(&_memPanel);
  _mainPanel.add(&_varPanel);

  _scene->setup();
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

  _scene->draw();
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

  _scene->keyPressed(key);
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

  _scene->keyReleased(key);
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
  _scene->mouseMoved(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
  _scene->mouseDragged(x, y, button);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
  _scene->mousePressed(x, y, button);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
  _scene->mouseReleased(x, y, button);
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
