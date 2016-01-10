#include "ofApp.h"
#include <ofxXmlSettings.h>
#include <math.h>
#include <commdlg.h>

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

static ofApp* g_App;

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
bool showFileDialog(bool openFile, string* filename)
{
  char szFileName[MAX_PATH];
  ZeroMemory(szFileName, sizeof(szFileName));

  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFilter = "Textures (*.xml)\0*.xml\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = szFileName;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
  ofn.lpstrDefExt = "xml";

  if (openFile)
  {
    ofn.Flags |= OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameA(&ofn))
      return false;
  }
  else
  {
    if (!GetSaveFileNameA(&ofn))
      return false;
  }

  *filename = szFileName;
  return true;
}

//--------------------------------------------------------------
static void drawStringCentered(const string& str,
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
static void drawOutlineRect(
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
static void drawOutlineCircle(const ofPoint& pt, float radius, const ofColor& fill)
{
  ofSetColor(fill);
  ofDrawCircle(pt, radius);
  ofNoFill();
  ofSetColor(30);
  ofDrawCircle(pt, radius);
  ofFill();
}

//--------------------------------------------------------------
void NodeTemplate::calcTemplateRectangle(ofTrueTypeFont& font)
{
  int numRows = max(1, (int)inputs.size());
  int h = 2 * INPUT_PADDING + numRows * INPUT_HEIGHT + (numRows - 1) * INPUT_PADDING;

  int strWidth = (int)ceil(font.stringWidth(name));
  for (const NodeTemplate::NodeParam& p : inputs)
  {
    strWidth = max(strWidth, (int)ceil(font.stringWidth(p.name)));
  }

  if (output != ParamType::Void)
    strWidth += (int)ceil(font.stringWidth("out"));

  rect = ofRectangle(ofPoint(0, 0), max(MIN_NODE_WIDTH, strWidth), h);
}

//--------------------------------------------------------------
Node::Node(const NodeTemplate& t, const ofPoint& pt) : name(t.name)
{
  bodyRect = t.rect;
  bodyRect.translate(pt);

  headingRect = bodyRect;
  float h = 2 * FONT_PADDING + g_App->_font.stringHeight(name);
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
NodeConnector* Node::findConnector(const string& str)
{
  for (size_t i = 0; i < inputs.size(); ++i)
  {
    if (inputs[i].name == str)
      return &inputs[i];
  }
  return nullptr;
}

//--------------------------------------------------------------
Node::Param* Node::findParam(const string& str)
{
  for (size_t i = 0; i < params.size(); ++i)
  {
    if (params[i].name == str)
      return &params[i];
  }
  return nullptr;
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
  drawStringCentered(name, g_App->_font, headingRect, true, true);

  int circleInset = CONNECTOR_RADIUS * 2 + 2 * INPUT_PADDING;

  // Draw inputs
  int y = bodyRect.y + INPUT_PADDING;
  for (const NodeConnector& input : inputs)
  {
    // each input gets its own rect, and we draw the text centered inside that
    ofRectangle rect(ofPoint(bodyRect.x + circleInset, y), bodyRect.getWidth(), INPUT_HEIGHT);
    ofSetColor(0);
    drawStringCentered(input.name, g_App->_font, rect, false, true);

    ofPoint pt(bodyRect.x + INPUT_PADDING + CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2);
    drawOutlineCircle(
        pt, CONNECTOR_RADIUS, input.cons.empty() ? ofColor(140) : ofColor(80, 200, 80));

    y += INPUT_HEIGHT + INPUT_PADDING;
  }

  // Draw output
  if (output.type != ParamType::Void)
  {
    int y = bodyRect.y + INPUT_PADDING;
    ofRectangle strRect = g_App->_font.getStringBoundingBox(output.name, 0, 0);

    // right aligned..
    int dy = (INPUT_HEIGHT - strRect.height) / 2;
    int strX = bodyRect.getRight() - circleInset - strRect.getWidth();
    g_App->_font.drawString(output.name, strX, y + INPUT_HEIGHT - dy);

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

struct TagCreator
{
  template <typename... Tags>
  void addAttributes()
  {
    if (enter)
      s.pushTag(tag, which);
  }

  template <typename Name, typename Value, typename... Tags>
  void addAttributes(Name name, Value value, Tags... tags)
  {
    s.addAttribute(tag, name, value, which);
    addAttributes(tags...);
  }

  template <typename... Tags>
  TagCreator(ofxXmlSettings& s, const string& tag, int which, bool enter, Tags... tags)
      : tag(tag), which(which), s(s), enter(enter)
  {
    s.addTag(tag);
    addAttributes(tags...);
  }

  ~TagCreator()
  {
    if (enter)
      s.popTag();
  }

  string tag;
  int which;
  bool enter;
  ofxXmlSettings& s;
};

#define GEN_NAME2(prefix, line) prefix##line
#define GEN_NAME(prefix, line) GEN_NAME2(prefix, line)
#define MAKE_SCOPED(type) type GEN_NAME(ANON, __LINE__)
#define CREATE_TAG(tag, i, ...) TagCreator GEN_NAME(ANON, __LINE__)(s, tag, i, true, __VA_ARGS__)
#define CREATE_LOCAL_TAG(tag, i, ...) TagCreator GEN_NAME(ANON, __LINE__)(s, tag, i, false, __VA_ARGS__)

//--------------------------------------------------------------
static ParamType stringToParamType(const string& str)
{
  if (str == "float")
    return ParamType::Float;

  if (str == "color")
    return ParamType::Color;

  if (str == "texture")
    return ParamType::Texture;

  if (str == "vec2")
    return ParamType::Vec2;

  return ParamType::Void;
}

//--------------------------------------------------------------
static string paramTypeToString(ParamType type)
{
  switch (type)
  {
    case ParamType::Float: return "float";
    case ParamType::Vec2: return "vec2";
    case ParamType::Color: return "color";
    case ParamType::Texture: return "texture";
    default: return "";
  }
}

//--------------------------------------------------------------
static string paramValueToString(const Node::Param& p)
{
  switch (p.type)
  {
    case ParamType::Float: return p.value.fValue.toString();
    case ParamType::Vec2: return p.value.vValue.toString();
    case ParamType::Color: return p.value.cValue.toString();
    default: return "";
  }
}

//--------------------------------------------------------------
static void stringToParamValue(const string& str, Node::Param* p)
{
  switch (p->type)
  {
  case ParamType::Float: p->value.fValue.fromString(str); break;
  case ParamType::Vec2: p->value.vValue.fromString(str); break;
  case ParamType::Color: p->value.cValue.fromString(str); break;
  }
}

//--------------------------------------------------------------
Node* ofApp::nodeById(int id)
{
  for (Node* node : _nodes)
  {
    if (node->id == id)
      return node;
  }
  return nullptr;
}

//--------------------------------------------------------------
void ofApp::saveToFile(const string& filename)
{
  ofxXmlSettings s(filename);
  s.clear();
  {
    CREATE_TAG("Nodes", 0);
    {
      int nodeIdx = 0;
      for (Node* node : _nodes)
      {
        CREATE_TAG("Node", nodeIdx, "name", node->name, "id", node->id);

        // Only need to save top-left pos
        CREATE_LOCAL_TAG("Pos", -1, "x", node->headingRect.x, "y", node->headingRect.y);
      
        {
          CREATE_TAG("Params", 0);
          for (size_t i = 0; i < node->params.size(); ++i)
          {
            const Node::Param& p = node->params[i];
            string type = paramTypeToString(p.type);
            string value = paramValueToString(p);
            CREATE_LOCAL_TAG("Param", i, "name", p.name, "type", type, "value", value);
          }
        }

        nodeIdx++;
      }
    }
  }

  {
    CREATE_TAG("Connections", 0);
    int conIdx = 0;
    for (Node* node : _nodes)
    {
      // NB: just the outputs are saved
      for (NodeConnector* con : node->output.cons)
      {
        if (Node* p = con->parent)
        {
          CREATE_LOCAL_TAG("Connection", conIdx, "from", node->id, "to_node", p->id, "to_input", con->name);
          conIdx++;
        }
      }
    }
  }

  s.saveFile();
}

template <typename... Attrs>
void getAttrs(ofxXmlSettings& s, const string& tag, int which, vector<string>* res) {}

template <typename... Attrs>
void getAttrs(ofxXmlSettings& s, const string& tag, int which, vector<string>* res, const string& attr, Attrs... attrs)
{
  res->push_back(s.getAttribute(tag, attr, "", which));
  getAttrs(s, tag, which, res, attrs...);
}

//--------------------------------------------------------------
void ofApp::resetTexture()
{
  for (Node* node : _nodes)
    delete node;
  _nodes.clear();

  _selectedNodes.clear();
}

//--------------------------------------------------------------
void ofApp::loadFromFile(const string& filename)
{
  resetTexture();

  int maxNodeId = 0;

  ofxXmlSettings s(filename);
  if (s.tagExists("Nodes") && s.pushTag("Nodes"))
  {
    int numNodes = s.getNumTags("Node");
    for (int i = 0; i < numNodes; ++i)
    {
      string name = s.getAttribute("Node", "name", "", i);
      int id = atoi(s.getAttribute("Node", "id", "", i).c_str());
      maxNodeId = max(maxNodeId, id);

      s.pushTag("Node", i);

      float x = (float)atof(s.getAttribute("Pos", "x", "", 0).c_str());
      float y = (float)atof(s.getAttribute("Pos", "y", "", 0).c_str());

      const NodeTemplate& t = _nodeTemplates[name];
      Node* node = new Node(t, ofPoint(x, y));
      node->id = id;

      if (s.tagExists("Params") && s.pushTag("Params"))
      {
        int numParams = s.getNumTags("Param");
        for (int j = 0; j < numParams; ++j)
        {
          string name = s.getAttribute("Param", "name", "", j);
          string value = s.getAttribute("Param", "value", "", j);

          if (Node::Param* param = node->findParam(name))
          {
            stringToParamValue(value, param);
          }
          else
          {
            // error: parameter not found..
          }
        }
        s.popTag();
      }

      _nodes.push_back(node);

      s.popTag();
    }
    s.popTag();
  }

  if (s.tagExists("Connections") && s.pushTag("Connections"))
  {
    int numConnections = s.getNumTags("Connection");
    for (int i = 0; i < numConnections; ++i)
    {
      int fromId = atoi(s.getAttribute("Connection", "from", "", i).c_str());
      int toId = atoi(s.getAttribute("Connection", "to_node", "", i).c_str());
      string inputName = s.getAttribute("Connection", "to_input", "", i);

      Node* fromNode = nodeById(fromId);
      Node* toNode = nodeById(toId);
      NodeConnector* con = toNode->findConnector(inputName);

      if (fromNode && toNode && con)
      {
        fromNode->output.cons.push_back(con);
        con->cons.push_back(&fromNode->output);
      }
    }
    s.popTag();
  }

  _nextNodeId = maxNodeId + 1;

}

//--------------------------------------------------------------
void ofApp::fileMenuCallback(const void* sender)
{
  string filename;
  if (sender == _resetButton)
  {
    resetTexture();
  }
  else if (sender == _loadButton)
  {
    if (showFileDialog(true, &filename))
    {
      loadFromFile(filename);
    }
  }
  else if (sender == _saveButton)
  {
    if (showFileDialog(false, &filename))
    {
      saveToFile(filename);
    }
  }
}

//--------------------------------------------------------------
void ofApp::setupCreateNode(const void* sender)
{
  string type = _buttonToType[sender];
  _createType = type;
  _mode = Mode::Create;
}

//--------------------------------------------------------------
ofApp::ofApp()
{
  g_App = this;
}

//--------------------------------------------------------------
ofApp::~ofApp()
{
  for (Node* node : _nodes)
  {
    delete node;
  }
  _nodes.clear();
}

//--------------------------------------------------------------
void ofApp::loadTemplates()
{
  auto& fnAddButton = [this](
    ofxPanel* panel, vector<ofxMinimalButton*>* buttons, const string& type) {
    ofxMinimalButton* b = new ofxMinimalButton();
    b->setup(type);
    b->addListener(this, &ofApp::setupCreateNode);
    buttons->push_back(b);
    panel->add(b);
    _buttonToType[b] = type;
  };

  ofxXmlSettings s;
  if (s.loadFile("node_templates.xml"))
  {
    s.pushTag("NodeTemplates");
    int numCategories = s.getNumTags("Category");
    for (int i = 0; i < numCategories; ++i)
    {
      string categoryName = s.getAttribute("Category", "name", "", i);
      ofxPanel* panel = new ofxPanel();
      panel->setup(categoryName);
      _categoryPanels.push_back(panel);
      _mainPanel.add(panel);

      s.pushTag("Category", i);
      int numTemplates = s.getNumTags("NodeTemplate");
      for (int j = 0; j < numTemplates; ++j)
      {
        string templateName = s.getAttribute("NodeTemplate", "name", "", j);
        s.pushTag("NodeTemplate", j);

        fnAddButton(panel, &_categoryButtons, templateName);

        NodeTemplate& t = _nodeTemplates[templateName];
        t.name = templateName;

        if (s.tagExists("Inputs") && s.pushTag("Inputs"))
        {
          int numInputs = s.getNumTags("Input");
          for (int aa = 0; aa < numInputs; ++aa)
          {
            string inputName = s.getAttribute("Input", "name", "", aa);
            ParamType inputType = stringToParamType(s.getAttribute("Input", "type", "", aa));
            t.inputs.push_back(NodeTemplate::NodeParam{ inputName, inputType });
          }
          s.popTag();
        }

        if (s.tagExists("Params") && s.pushTag("Params"))
        {
          int numParams = s.getNumTags("Param");
          for (int aa = 0; aa < numParams; ++aa)
          {
            string paramName = s.getAttribute("Param", "name", "", aa);
            ParamType paramType = stringToParamType(s.getAttribute("Param", "type", "", aa));
            t.params.push_back(NodeTemplate::NodeParam{ paramName, paramType });
          }
          s.popTag();
        }

        t.output = stringToParamType(s.getAttribute("Output", "type", ""));
        t.calcTemplateRectangle(_font);

        s.popTag();
      }
      s.popTag();
    }
    s.popTag();
  }

}

//--------------------------------------------------------------
void ofApp::setup()
{
  ofSetVerticalSync(true);
  ofGetMainLoop()->setEscapeQuitsLoop(false);

  _font.load("verdana.ttf", FONT_HEIGHT, true, true);
  _font.setLineHeight(FONT_HEIGHT);
  _font.setLetterSpacing(1.037f);

  _mainPanel.setup("TextureGen 0.1");

  _resetButton = new ofxMinimalButton("Reset");
  _resetButton->addListener(this, &ofApp::fileMenuCallback);

  _loadButton = new ofxMinimalButton("Load");
  _loadButton->addListener(this, &ofApp::fileMenuCallback);

  _saveButton = new ofxMinimalButton("Save");
  _saveButton->addListener(this, &ofApp::fileMenuCallback);

  _mainPanel.add(_resetButton);
  _mainPanel.add(_loadButton);
  _mainPanel.add(_saveButton);

  loadTemplates();

  _mainPanel.add(&_varPanel);
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
void ofApp::keyPressed(int key)
{
  if (key == OF_KEY_SHIFT)
    g_modState |= KeyModShift;

  if (key == OF_KEY_CONTROL)
    g_modState |= KeyModCtrl;

  if (key == OF_KEY_ALT)
    g_modState |= KeyModAlt;
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

//--------------------------------------------------------------
Node* ofApp::nodeAtPoint(const ofPoint& pt)
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
void ofApp::clearSelection()
{
  for (auto& node : _selectedNodes)
    node->selected = false;
  _selectedNodes.clear();
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
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
NodeConnector* ofApp::connectorAtPoint(const ofPoint& pt)
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
void ofApp::mousePressed(int x, int y, int button)
{
  ofPoint pt(x, y);

  if (_mode == Mode::Create)
  {
    const NodeTemplate& t = _nodeTemplates[_createType];
    Node* node = new Node(t, pt);
    node->id = _nextNodeId++;
    _nodes.push_back(node);
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
void ofApp::mouseReleased(int x, int y, int button)
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
void ofApp::abortAction()
{
  // abort current action, reset draggers etc.
}

//--------------------------------------------------------------
void ofApp::resetState()
{
  _mode = Mode::Default;
}

//--------------------------------------------------------------
void ofApp::initNodeParameters(Node* node)
{
  _varPanel.clear();
  char buf[256];
  sprintf(buf, "%s vars", node->name.c_str());
  _varPanel.setup(buf);

  for (Node::Param& p : node->params)
  {
    switch (p.type)
    {
    case ParamType::Void: break;
    case ParamType::Float: _varPanel.add(p.value.fValue); break;
    case ParamType::Vec2: _varPanel.add(p.value.vValue); break;
    case ParamType::Color: _varPanel.add(p.value.cValue); break;
    case ParamType::Texture: break;
    default: break;
    }
  }
}
