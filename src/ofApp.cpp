#include "ofApp.h"
#include "xml_utils.hpp"
#include <commdlg.h>
#include <math.h>
#include <ofxXmlSettings.h>

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
static const ImVec2 BUTTON_SIZE(200, 20);

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
Node::Node(const NodeTemplate* t, const ofPoint& pt, int id) : name(t->name), id(id)
{
  bodyRect = t->rect;
  bodyRect.translate(pt);

  headingRect = bodyRect;
  float h = 2 * FONT_PADDING + g_App->_font.stringHeight(name);
  headingRect.setHeight(h);
  headingRect.translateY(-h);

  int y = bodyRect.y + INPUT_PADDING;
  for (size_t i = 0; i < t->inputs.size(); ++i)
  {
    const NodeTemplate::NodeParam& input = t->inputs[i];
    inputs.push_back(NodeConnector(input.name,
        input.type,
        NodeConnector::Dir::Input,
        ofPoint(bodyRect.x + INPUT_PADDING + CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2),
        this));
    y += INPUT_HEIGHT + INPUT_PADDING;
  }

  for (const NodeTemplate::NodeParam& param : t->params)
  {
    params.push_back(Node::Param(param.name, param.type));
  }

  output = NodeConnector("out",
      t->output,
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

//--------------------------------------------------------------
template <typename T,
    typename Ret =
        conditional<is_const<T>::value, const ofAbstractParameter*, ofAbstractParameter*>::type>
Ret getParamBase(T& p)
{
  return nullptr;
  // switch (p.type)
  //{
  //  case ParamType::Bool: return &p.value.bValue;
  //  case ParamType::Float: return &p.value.fValue;
  //  case ParamType::Vec2: return &p.value.vValue;
  //  case ParamType::Color: return &p.value.cValue;
  //  case ParamType::String: return &p.value.sValue;
  //  default: return nullptr;
  //}
}

//--------------------------------------------------------------
static ParamType stringToParamType(const string& str)
{
  if (str == "bool")
    return ParamType::Bool;

  if (str == "float")
    return ParamType::Float;

  if (str == "vec2")
    return ParamType::Vec2;

  if (str == "color")
    return ParamType::Color;

  if (str == "texture")
    return ParamType::Texture;

  if (str == "string")
    return ParamType::String;

  return ParamType::Void;
}

//--------------------------------------------------------------
static string paramTypeToString(const Node::Param& p)
{
  switch (p.type)
  {
    case ParamType::Bool: return "bool";
    case ParamType::Float: return "float";
    case ParamType::Vec2: return "vec2";
    case ParamType::Color: return "color";
    case ParamType::Texture: return "texture";
    case ParamType::String: return "string";
    default: return "";
  }
}

//--------------------------------------------------------------
static string paramValueToString(const Node::Param& p)
{
  const ofAbstractParameter* ap = getParamBase(p);
  return ap ? ap->toString() : "";
}

//--------------------------------------------------------------
static void stringToParamValue(const string& str, Node::Param* p)
{
  ofAbstractParameter* ap = getParamBase(*p);
  if (ap)
    ap->fromString(str);
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
            string type = paramTypeToString(p);
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
          CREATE_LOCAL_TAG(
              "Connection", conIdx, "from", node->id, "to_node", p->id, "to_input", con->name);
          conIdx++;
        }
      }
    }
  }

  s.saveFile();
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
      string name;
      int id;
      getAttributes(s, "Node", i, "name", &name, "id", &id);
      maxNodeId = max(maxNodeId, id);

      s.pushTag("Node", i);

      float x, y;
      getAttributes(s, "Pos", 0, "x", &x, "y", &y);

      const NodeTemplate* t = _nodeTemplates[name];
      Node* node = new Node(t, ofPoint(x, y), id);

      if (s.tagExists("Params") && s.pushTag("Params"))
      {
        int numParams = s.getNumTags("Param");
        for (int j = 0; j < numParams; ++j)
        {
          string name, value;
          getAttributes(s, "Param", j, "name", &name, "value", &value);
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
      int fromId, toId;
      string inputName;
      getAttributes(s, "Connection", i, "from", &fromId, "to_node", &toId, "to_input", &inputName);
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

  for (auto& kv : _nodeTemplates)
  {
    delete kv.second;
  }
  _nodeTemplates.clear();
}

//--------------------------------------------------------------
void ofApp::loadTemplates()
{
  ofxXmlSettings s;
  if (s.loadFile("node_templates.xml"))
  {
    s.pushTag("NodeTemplates");
    int numCategories = s.getNumTags("Category");
    for (int i = 0; i < numCategories; ++i)
    {
      string categoryName = s.getAttribute("Category", "name", "", i);
      s.pushTag("Category", i);
      int numTemplates = s.getNumTags("NodeTemplate");
      for (int j = 0; j < numTemplates; ++j)
      {
        string templateName;
        int id;
        getAttributes(s, "NodeTemplate", j, "name", &templateName, "id", &id);
        s.pushTag("NodeTemplate", j);

        NodeTemplate* t = new NodeTemplate();
        _nodeTemplates[templateName] = t;
        _templatesByCategory[categoryName].push_back(t);
        t->name = templateName;
        t->id = id;

        if (s.tagExists("Inputs") && s.pushTag("Inputs"))
        {
          int numInputs = s.getNumTags("Input");
          for (int aa = 0; aa < numInputs; ++aa)
          {
            string inputName = s.getAttribute("Input", "name", "", aa);
            ParamType inputType = stringToParamType(s.getAttribute("Input", "type", "", aa));
            t->inputs.push_back(NodeTemplate::NodeParam{inputName, inputType});
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
            t->params.push_back(NodeTemplate::NodeParam{paramName, paramType});
          }
          s.popTag();
        }

        t->output = stringToParamType(s.getAttribute("Output", "type", ""));
        t->calcTemplateRectangle(_font);

        s.popTag();
      }
      s.popTag();
    }
    s.popTag();
  }
}

void createGraph(const vector<Node*> nodes, vector<Node*>* sortedNodes)
{
  // Create a graph from the nodes
  struct GraphNode
  {
    Node* node;
    vector<Node*> outEdges;
    vector<Node*> inEdges;
  };

  vector<GraphNode> graph;

  auto& fnGraphFindNode = [&](Node* node) -> GraphNode* {
    for (GraphNode& g : graph)
    {
      if (g.node == node)
        return &g;
    }
    return nullptr;
  };

  for (Node* node : nodes)
  {
    graph.push_back(GraphNode{node});
  }

  for (GraphNode& g : graph)
  {
    Node* node = g.node;
    for (NodeConnector* con : node->output.cons)
    {
      g.outEdges.push_back(con->parent);
      fnGraphFindNode(con->parent)->inEdges.push_back(node);
    }
  }

  // do a topological sort, by iteratively grabbing the node with no inputs
  while (!graph.empty())
  {
    Node* node = nullptr;
    for (auto it = graph.begin(); it != graph.end();)
    {
      if (it->inEdges.empty())
      {
        node = it->node;
        it = graph.erase(it);
        break;
      }
      else
      {
        ++it;
      }
    }

    if (!node)
    {
      // graph has cycles!
      return;
    }

    // remove occurences of node from the other node's in-edges
    for (GraphNode& g : graph)
    {
      for (auto it = g.inEdges.begin(); it != g.inEdges.end();)
      {
        if (*it == node)
        {
          it = g.inEdges.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }

    sortedNodes->push_back(node);
  }
}

typedef uint8_t u8;
typedef uint16_t u16;

struct VmPrg
{
  u8 version = 1;
  u8 texturesUsed = 0;
};

struct BinaryWriter
{
  template <typename T>
  void write(const T& v)
  {
    size_t oldPos = buf.size();
    buf.resize(buf.size() + sizeof(T));
    memcpy(buf.data() + oldPos, (const void*)&v, sizeof(T));
  }

  template <typename T>
  void writeAt(const T& v, int pos)
  {
    assert(sizeof(T) + pos <= (int)buf.size());
    memcpy(buf.data() + pos, (const void*)&v, sizeof(T));
  }

  int getPos() const { return (int)buf.size(); }

  vector<u8> buf;
};

//--------------------------------------------------------------
void ofApp::generateGraph()
{
  vector<Node*> sorted;
  createGraph(_nodes, &sorted);

  // check that each node has its inputs filled
  for (Node* node : _nodes)
  {
    for (NodeConnector& con : node->inputs)
    {
      if (!con.parent)
      {
        printf("Node: %s missing input\n", node->name.c_str());
        return;
      }
    }
  }

  // For each OUT, we try to grab an existing texture - Create if needed
  // Textures are references counted - Initialized to # inputs, and decremented when each block
  // has been processed. When a ref hits zero, return the texture to the pool.

  stack<u8> texturePool;
  u8 nextTextureId = 0;

  // Map to keep track of which texture id corresponds to each node's output
  unordered_map<Node*, int> nodeOutTexture;
  unordered_map<Node*, int> nodeOutRefCount;

  // Write the header
  BinaryWriter w;
  VmPrg prg;
  w.write(prg);

  // create a command list for the texture
  for (Node* node : sorted)
  {
    // create an output texture if needed
    if (texturePool.empty())
      texturePool.push(nextTextureId++);

    u8 outputTexture = texturePool.top();
    texturePool.pop();

    // Inc the ref count on the output texture for each input node that uses it
    nodeOutRefCount[node] = (int)node->output.cons.size();

    // write the template id
    w.write((u8)_nodeTemplates[node->name]->id);
    w.write(outputTexture);

    // Set up texture inputs
    w.write((u8)node->inputs.size());

    for (const NodeConnector& con : node->inputs)
    {
      u8 inputTextureId = nodeOutTexture[con.parent];
      w.write(inputTextureId);
    }

    u16 cbufferSize = 0;
    int cbufferSizePos = w.getPos();
    w.write(cbufferSize);

    static unordered_map<ParamType, int> paramSize = {
        {ParamType::Float, sizeof(float)},
        {ParamType::Vec2, 2 * sizeof(float)},
        {ParamType::Color, 4 * sizeof(float)},
    };

    // Write the parameters
    // NB: because these are written as-is to constant buffers, we need to take care with
    // aligning parameters on 32 bit boundaries
    int curOffset = 0;
    for (const Node::Param& param : node->params)
    {
      int s = paramSize[param.type];
      if (s == 0)
      {
        assert(false);
        // error: unknown type
      }

      if (curOffset + s > 4)
      {
        // Apply padding
        for (int i = 0; i < (curOffset + s) % 4; ++i)
          w.write(0.0f);
        curOffset = 0;
      }

      if (param.type == ParamType::Float)
      {
        w.write(param.value.fValue.value);
      }
      else if (param.type == ParamType::Vec2)
      {
        w.write(param.value.vValue.value.x);
        w.write(param.value.vValue.value.y);
      }
      else if (param.type == ParamType::Color)
      {
        w.write((float)param.value.cValue.r / 255.f);
        w.write((float)param.value.cValue.g / 255.f);
        w.write((float)param.value.cValue.b / 255.f);
        w.write((float)param.value.cValue.a / 255.f);
      }
      cbufferSize += s;
      curOffset = (curOffset + s) % 4;
    }

    w.writeAt(cbufferSize, cbufferSizePos);

    // dec the ref count on any used textures, and return any that have a zero count
    for (NodeConnector& con : node->inputs)
    {
      Node* node = con.parent;
      if (--nodeOutRefCount[node] == 0)
      {
        texturePool.push(nodeOutTexture[node]);
        nodeOutRefCount.erase(node);
      }
    }
  }

  // write # textures used
  ((VmPrg*)w.buf.data())->texturesUsed = nextTextureId;

  FILE* ff = fopen("texture.dat", "wb");
  fwrite(w.buf.data(), 1, (int)w.buf.size(), ff);
  fclose(ff);
}

//--------------------------------------------------------------
void ofApp::setup()
{
  _imgui.setup();
  ofSetVerticalSync(true);
  ofGetMainLoop()->setEscapeQuitsLoop(false);

  _font.load("verdana.ttf", FONT_HEIGHT, true, true);
  _font.setLineHeight(FONT_HEIGHT);
  _font.setLetterSpacing(1.037f);

  loadTemplates();
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
  _imgui.begin();

  ImGui::Begin("TextureGen 0.1", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  if (ImGui::Button("Reset", BUTTON_SIZE))
  {
    resetTexture();
  }

  if (ImGui::Button("Load", BUTTON_SIZE))
  {
    string filename;
    if (showFileDialog(true, &filename))
    {
      loadFromFile(filename);
    }
  }

  if (ImGui::Button("Save", BUTTON_SIZE))
  {
    string filename;
    if (showFileDialog(false, &filename))
    {
      saveToFile(filename);
    }
  }

  if (ImGui::Button("Generate", BUTTON_SIZE))
  {
    generateGraph();
  }

  for (const string& cat : {"Memory", "Generators", "Modifiers"})
  {
    if (ImGui::CollapsingHeader(cat.c_str(), NULL, true, true))
    {
      for (const NodeTemplate* t : _templatesByCategory[cat])
      {
        if (_mode == Mode::Create && _createType == t->name)
        {
          ImGui::Text(t->name.c_str());
        }
        else
        {
          if (ImGui::Button(t->name.c_str(), BUTTON_SIZE))
          {
            _mode = Mode::Create;
            _createType = t->name;
          }
        }
      }
    }
  }
  ImGui::End();

  ImGui::Begin(
      "Parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (_curEditingNode)
  {
    for (Node::Param& p : _curEditingNode->params)
    {
      const char* name = p.name.c_str();
      switch (p.type)
      {
        case ParamType::Bool: ImGui::Checkbox(p.name.c_str(), &p.value.bValue); break;
        case ParamType::Float:
        {
          if (p.value.fValue.flags & PARAM_FLAG_HAS_MIN_MAX)
          {
            ImGui::SliderFloat(
                name, &p.value.fValue.value, p.value.fValue.minValue, p.value.fValue.maxValue);
          }
          else
          {
            ImGui::InputFloat(name, &p.value.fValue.value);
          }
          break;
        }

        case ParamType::Vec2:
        {
          if (p.value.vValue.flags & PARAM_FLAG_HAS_MIN_MAX)
          {
            ImGui::SliderFloat2(name,
                p.value.vValue.value.getPtr(),
                p.value.vValue.minValue,
                p.value.vValue.maxValue);
          }
          else
          {
            ImGui::InputFloat2(name, p.value.vValue.value.getPtr());
          }
          break;
        }
        case ParamType::Color: ImGui::ColorEdit4(p.name.c_str(), &p.value.cValue.r); break;
        case ParamType::Texture: break;
        case ParamType::String:
        {
          const int BUF_SIZE = 64;
          char buf[BUF_SIZE + 1];
          size_t len = min(BUF_SIZE, (int)p.value.sValue.size());
          memcpy(buf, p.value.sValue.c_str(), len);
          buf[len] = 0;
          if (ImGui::InputText(p.name.c_str(), buf, BUF_SIZE))
            p.value.sValue.assign(buf);
          break;
        }
        default: break;
      }
    }
  }
  else
  {
    ImGui::TextUnformatted("Nothing selected..");
  }
  ImGui::End();

  ofBackgroundGradient(ofColor::white, ofColor::gray);

  //_mainPanel.draw();

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

  // ImGui::ShowTestWindow();

  _imgui.end();
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
    _curEditingNode = nullptr;
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
    const NodeTemplate* t = _nodeTemplates[_createType];
    Node* node = new Node(t, pt, _nextNodeId++);
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
      _curEditingNode = node;
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
