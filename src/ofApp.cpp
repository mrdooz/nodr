#include "ofApp.h"
#include "xml_utils.hpp"
#include "nodr_utils.hpp"

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
static const int NUM_AUX_TEXTURES = 16;
static const ImVec2 BUTTON_SIZE(225, 20);

static const char* FILE_DLG_XML_FILTER = "Textures (*.xml)\0*.xml\0All Files (*.*)\0*.*\0";
static const char* FILE_DLG_XML_EXT = "xml";

static const char* FILE_DLG_GEN_FILTER = "Textures (*.dat)\0*.dat\0All Files (*.*)\0*.*\0";
static const char* FILE_DLG_GEN_EXT = "dat";

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
    inputs.push_back(new NodeConnector(input.name,
        input.type,
        NodeConnector::Dir::Input,
        ofPoint(bodyRect.x + INPUT_PADDING + CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2),
        this));
    y += INPUT_HEIGHT + INPUT_PADDING;
  }

  for (const NodeTemplate::NodeParam& param : t->params)
  {
    params.push_back(Node::Param{ param.name, param.type, param.bounds });
  }

  output = new NodeConnector("out",
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
    if (inputs[i]->name == str)
      return inputs[i];
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
    input->pt += delta;

  output->pt += delta;
}

//--------------------------------------------------------------
void Node::draw()
{
  // Draw body
  drawOutlineRect(bodyRect, 95, 0, RECT_LOWER_ROUNDING);

  // Draw heading
  drawOutlineRect(headingRect, 78, RECT_UPPER_ROUNDING, 0);
  ofSetColor(0);
  // for load and store nodes, include what texture they output too
  string heading = name;
  if (name == "Load" || name == "Store")
  {
    char buf[256];
    sprintf(buf, "%s [%d]", name.c_str(), params[0].value.iValue.value);
    heading = buf;
  }
  drawStringCentered(heading, g_App->_font, headingRect, true, true);

  int circleInset = CONNECTOR_RADIUS * 2 + 2 * INPUT_PADDING;

  // Draw inputs
  int y = bodyRect.y + INPUT_PADDING;
  for (const NodeConnector* input : inputs)
  {
    // each input gets its own rect, and we draw the text centered inside that
    ofRectangle rect(ofPoint(bodyRect.x + circleInset, y), bodyRect.getWidth(), INPUT_HEIGHT);
    ofSetColor(0);
    drawStringCentered(input->name, g_App->_font, rect, false, true);

    ofPoint pt(bodyRect.x + INPUT_PADDING + CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2);
    drawOutlineCircle(
        pt, CONNECTOR_RADIUS, input->cons.empty() ? ofColor(140) : ofColor(80, 200, 80));

    y += INPUT_HEIGHT + INPUT_PADDING;
  }

  // Draw output
  if (output->type != ParamType::Void)
  {
    int y = bodyRect.y + INPUT_PADDING;
    ofRectangle strRect = g_App->_font.getStringBoundingBox(output->name, 0, 0);

    // right aligned..
    int dy = (INPUT_HEIGHT - strRect.height) / 2;
    int strX = bodyRect.getRight() - circleInset - strRect.getWidth();
    g_App->_font.drawString(output->name, strX, y + INPUT_HEIGHT - dy);

    ofPoint pt(bodyRect.getRight() - INPUT_PADDING - CONNECTOR_RADIUS, y + INPUT_HEIGHT / 2);
    drawOutlineCircle(
        pt, CONNECTOR_RADIUS, output->cons.empty() ? ofColor(140) : ofColor(80, 200, 80));
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
  for (const NodeConnector* con : output->cons)
  {
    ofDrawLine(output->pt, con->pt);
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
static ParamType stringToParamType(const string& str)
{
  if (str == "bool")
    return ParamType::Bool;

  if (str == "int")
    return ParamType::Int;

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
    case ParamType::Int: return "int";
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
  ostringstream ss;
  switch (p.type)
  {
    case ParamType::Bool: ss << p.value.bValue; break;
    case ParamType::Int: ss << p.value.iValue.value; break;
    case ParamType::Float: ss << p.value.fValue.value; break;
    case ParamType::Vec2: ss << p.value.vValue.value; break;
    case ParamType::Color: ss << p.value.cValue; break;
    case ParamType::String: ss << p.value.sValue; break;
    default: return "";
  }

  return ss.str();
}

//--------------------------------------------------------------
static void stringToParamValue(const string& str, Node::Param* p)
{
  istringstream ss(str);
  switch (p->type)
  {
    case ParamType::Bool: ss >> p->value.bValue; break;
    case ParamType::Int: ss >> p->value.iValue.value; break;
    case ParamType::Float: ss >> p->value.fValue.value; break;
    case ParamType::Vec2: ss >> p->value.vValue.value; break;
    case ParamType::Color: ss >> p->value.cValue; break;
    case ParamType::String: ss >> p->value.sValue; break;
    default: break;
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
      for (NodeConnector* con : node->output->cons)
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

  clearSelection();
  _mode = Mode::Default;
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
        fromNode->output->cons.push_back(con);
        con->cons.push_back(fromNode->output);
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
          for (int paramIdx = 0; paramIdx < numParams; ++paramIdx)
          {
            string paramName = s.getAttribute("Param", "name", "", paramIdx);
            ParamType paramType = stringToParamType(s.getAttribute("Param", "type", "", paramIdx));
            NodeTemplate::NodeParam param{ paramName, paramType };

            if (s.attributeExists("Param", "minValue", paramIdx)
                && s.attributeExists("Param", "maxValue", paramIdx))
            {
              param.hasBounds = true;
              param.bounds.flags = PARAM_FLAG_HAS_MIN_MAX;
              if (paramType == ParamType::Int)
              {
                ParamInt& p = param.bounds.iValue;
                getAttributes(s, "Param", paramIdx, "minValue", &p.minValue, "maxValue", &p.maxValue);
                p.value = p.minValue;
              }
              else if (paramType == ParamType::Float)
              {
                ParamFloat& p = param.bounds.fValue;
                getAttributes(s, "Param", paramIdx, "minValue", &p.minValue, "maxValue", &p.maxValue);
                p.value = p.minValue;
              }
              else if (paramType == ParamType::Vec2)
              {
                ParamVec2& p = param.bounds.vValue;
                getAttributes(s, "Param", paramIdx, "minValue", &p.minValue, "maxValue", &p.maxValue);
                p.value = ofVec2f(p.minValue, p.minValue);
              }
            }
            t->params.push_back(param);
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

//--------------------------------------------------------------
bool ofApp::createGraph(const vector<Node*> nodes, vector<Node*>* sortedNodes)
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

  // save all the load nodes, because we need to create a relationship between the loads and the stores
  unordered_map<int, Node*> loadNodes;

  for (Node* node : nodes)
  {
    if (node->name == "Load")
    {
      loadNodes[node->params[0].value.iValue.value] = node;
    }

    graph.push_back(GraphNode{node});
  }

  for (GraphNode& g : graph)
  {
    Node* node = g.node;
    for (NodeConnector* con : node->output->cons)
    {
      g.outEdges.push_back(con->parent);
      fnGraphFindNode(con->parent)->inEdges.push_back(node);
    }

    // if this is a store node, create dependencies on the load node
    if (node->name == "Store")
    {
      int textureId = node->params[0].value.iValue.value;
      if (!loadNodes.count(textureId))
        return false;
      fnGraphFindNode(loadNodes[textureId])->inEdges.push_back(node);
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
      return false;
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

  // check that each node has its inputs filled
  for (Node* node : _nodes)
  {
    for (NodeConnector* con : node->inputs)
    {
      if (!con->parent)
      {
        return false;
      }
    }
  }

  return true;
}

//--------------------------------------------------------------
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

  vector<char> buf;
};


//--------------------------------------------------------------
bool ofApp::generateGraph(vector<char>* buf)
{
  vector<Node*> sorted;
  if (!createGraph(_nodes, &sorted))
    return false;

  // check that each node has its inputs filled
  for (Node* node : _nodes)
  {
    for (NodeConnector* con : node->inputs)
    {
      if (con->cons.empty())
      {
        printf("Node: %s missing input\n", node->name.c_str());
        return false;
      }
    }
  }

  struct VmPrg
  {
    u8 version = 1;
    u8 texturesUsed = 0;
  };

  // For each OUT, we try to grab an existing texture - Create if needed
  // Textures are references counted - Initialized to # inputs, and decremented when each block
  // has been processed. When a ref hits zero, return the texture to the pool.

  stack<u8> texturePool;
  u8 nextTextureId = NUM_AUX_TEXTURES;

  // Map to keep track of which texture id corresponds to each node's output
  unordered_map<Node*, int> nodeOutTexture;
  unordered_map<Node*, int> nodeOutRefCount;

  // Write the header
  BinaryWriter w;
  VmPrg prg;
  w.write(prg);

  u8 finalId = (u8)_nodeTemplates["Final"]->id;
  u8 loadId = (u8)_nodeTemplates["Load"]->id;
  u8 storeId = (u8)_nodeTemplates["Store"]->id;

  // create a command list for the texture
  for (Node* node : sorted)
  {
    u8 id = (u8)_nodeTemplates[node->name]->id;
    u8 outputId = id;

    // there are some special nodes:
    // final - input: normal, output: hard-coded
    // load  - input: hard-coded, output: normal
    // store - input: normal, output: hard-coded 

    // setup output texture
    u8 outputTexture;
    if (id == finalId)
    {
      // NB: both store and final are just loads, but with hard-coded outputs!
      outputId = loadId;
      outputTexture = (u8)0xff;
    }
    else if (id == storeId)
    {
      assert(node->params[0].name == "aux");
      outputId = loadId;
      outputTexture = (u8)node->params[0].value.iValue.value;
    }
    else
    {
      // create an output texture if needed
      if (texturePool.empty())
        texturePool.push(nextTextureId++);

      outputTexture = texturePool.top();
      texturePool.pop();

      // Inc the ref count on the output texture for each input node that uses it
      nodeOutRefCount[node] = (int)node->output->cons.size();
      nodeOutTexture[node] = outputTexture;
    }

    // write the operation id and output texture
    w.write(outputId);
    w.write(outputTexture);

    // setup input textures
    if (id == loadId)
    {
      assert(node->params[0].name == "aux");
      w.write((u8)1);
      w.write((u8)node->params[0].value.iValue.value);
    }
    else
    {
      w.write((u8)node->inputs.size());
      for (const NodeConnector* con : node->inputs)
      {
        u8 inputTextureId = nodeOutTexture[con->cons[0]->parent];
        w.write(inputTextureId);
      }
    }

    // load/store shouldn't have proper c-buffers
    if (outputId == loadId)
    {
      u16 cbufferSize = 0;
      w.write(cbufferSize);
    }
    else
    {
      u16 cbufferSize = 0;
      int cbufferSizePos = w.getPos();
      w.write(cbufferSize);

      static unordered_map<ParamType, int> paramSize = {
        { ParamType::Int, sizeof(int) },
        { ParamType::Float, sizeof(float) },
        { ParamType::Vec2, 2 * sizeof(float) },
        { ParamType::Color, 4 * sizeof(float) },
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

        if (param.type == ParamType::Int)
        {
          w.write(param.value.iValue.value);
        }
        else if (param.type == ParamType::Float)
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
          w.write(param.value.cValue.r);
          w.write(param.value.cValue.g);
          w.write(param.value.cValue.b);
          w.write(param.value.cValue.a);
        }
        cbufferSize += s;
        curOffset = (curOffset + s) % 4;
      }

      w.writeAt(cbufferSize, cbufferSizePos);
    }

    // dec the ref count on any used textures, and return any that have a zero count
    if (id != finalId && id != storeId)
    {
      for (NodeConnector* con : node->inputs)
      {
        for (NodeConnector* input : con->cons)
        {
          Node* node = input->parent;
          if (--nodeOutRefCount[node] == 0)
          {
            texturePool.push(nodeOutTexture[node]);
            nodeOutRefCount.erase(node);
            nodeOutTexture.erase(node);
          }
        }
      }
    }
  }

  // copy out the generated texture
  ((VmPrg*)w.buf.data())->texturesUsed = nextTextureId;
  *buf = w.buf;
  return true;
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
  if (_pipeHandle != INVALID_HANDLE_VALUE)
  {
    CloseHandle(_pipeHandle);
  }
}

//--------------------------------------------------------------
void ofApp::update()
{
}

//--------------------------------------------------------------
void ofApp::drawSidePanel()
{
  ImGui::Begin("TextureGen 0.1", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  if (ImGui::CollapsingHeader("File", NULL, true, true))
  {
    if (ImGui::Button("Reset", BUTTON_SIZE))
    {
      resetTexture();
    }

    if (ImGui::Button("Load", BUTTON_SIZE))
    {
      string filename;
      if (showFileDialog(true, FILE_DLG_XML_FILTER, FILE_DLG_XML_EXT, &filename))
      {
        loadFromFile(filename);
      }
    }

    if (ImGui::Button("Save", BUTTON_SIZE))
    {
      string filename;
      if (showFileDialog(false, FILE_DLG_XML_FILTER, FILE_DLG_XML_EXT, &filename))
      {
        saveToFile(filename);
      }
    }

    if (ImGui::Button("Generate", BUTTON_SIZE))
    {
      string filename;
      if (showFileDialog(false, FILE_DLG_GEN_FILTER, FILE_DLG_GEN_EXT, &filename))
      {
        vector<char> buf;
        generateGraph(&buf);

        FILE* f = fopen(filename.c_str(), "wb");
        if (f)
        {
          fwrite(buf.data(), 1, buf.size(), f);
          fclose(f);
        }
      }
    }
  }

  //if (ImGui::CollapsingHeader("Settings", NULL, true, true))
  //{
  //  ImGui::PushItemWidth(BUTTON_SIZE.x - 50);
  //  ImGui::SliderInt("# AUX", &_textureSettings.numAuxTextures, 4, 16);
  //  ImGui::PopItemWidth();
  //}

  ImGui::End();

  ImGui::Begin("Commands", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  for (const string& cat : { "Memory", "Generators", "Modifiers" })
  {
    if (ImGui::CollapsingHeader(cat.c_str(), NULL, true, true))
    {
      for (const NodeTemplate* t : _templatesByCategory[cat])
      {
        if (_mode == Mode::Create && _createType == t->name)
        {
          // TODO(magnus): hmm, I want something more pronunced..
          ImGui::TextUnformatted(t->name.c_str());
          //ImGui::ButtonEx(t->name.c_str(), BUTTON_SIZE, ImGuiButtonFlags_Disabled);
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
}

//--------------------------------------------------------------
void ofApp::sendTexture()
{
  vector<char> buf;
  if (generateGraph(&buf))
  {
    // try to create the pipe if it doesn't exist yet
    if (_pipeHandle == INVALID_HANDLE_VALUE)
    {
      const char* pipeName = "\\\\.\\pipe\\texturepipe";
      _pipeHandle = CreateFileA(pipeName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    }

    if (_pipeHandle != INVALID_HANDLE_VALUE)
    {
      DWORD bytesWritten = 0;
      if (!WriteFile(_pipeHandle, buf.data(), buf.size(), &bytesWritten, NULL))
      {
        _pipeHandle = INVALID_HANDLE_VALUE;
      }
    }
  }
}

//--------------------------------------------------------------
bool ofApp::drawNodeParameters()
{
  ImGui::Begin(
    "Parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

  if (!_curEditingNode)
  {
    ImGui::TextUnformatted("Nothing selected..");
    ImGui::End();
    return false;
  }

  if (_curEditingNode->params.empty())
  {
    ImGui::TextUnformatted("Node has no parameters");
    ImGui::End();
    return false;
  }

  bool updated = false;

  for (Node::Param& p : _curEditingNode->params)
  {
    const char* name = p.name.c_str();
    switch (p.type)
    {
      case ParamType::Int:
      {
        if (p.value.flags & PARAM_FLAG_HAS_MIN_MAX)
        {
          updated |= ImGui::SliderInt(
            name, &p.value.iValue.value, p.value.iValue.minValue, p.value.iValue.maxValue);
        }
        else
        {
          updated |= ImGui::InputInt(name, &p.value.iValue.value);
        }
        break;
      }

      case ParamType::Bool:
      {
        updated |= ImGui::Checkbox(p.name.c_str(), &p.value.bValue);
        break;
      }
      case ParamType::Float:
      {
        if (p.value.flags & PARAM_FLAG_HAS_MIN_MAX)
        {
          updated |= ImGui::SliderFloat(
            name, &p.value.fValue.value, p.value.fValue.minValue, p.value.fValue.maxValue);
        }
        else
        {
          updated |= ImGui::InputFloat(name, &p.value.fValue.value);
        }
        break;
      }

      case ParamType::Vec2:
      {
        if (p.value.flags & PARAM_FLAG_HAS_MIN_MAX)
        {
          updated |= ImGui::SliderFloat2(name,
            p.value.vValue.value.getPtr(),
            p.value.vValue.minValue,
            p.value.vValue.maxValue);
        }
        else
        {
          updated |= ImGui::InputFloat2(name, p.value.vValue.value.getPtr());
        }
        break;
      }
      case ParamType::Color: updated |= ImGui::ColorEdit4(p.name.c_str(), &p.value.cValue.r); break;
      case ParamType::Texture: break;
      case ParamType::String:
      {
        const int BUF_SIZE = 64;
        char buf[BUF_SIZE + 1];
        size_t len = min(BUF_SIZE, (int)p.value.sValue.size());
        memcpy(buf, p.value.sValue.c_str(), len);
        buf[len] = 0;
        if (ImGui::InputText(p.name.c_str(), buf, BUF_SIZE))
        {
          updated = true;
          p.value.sValue.assign(buf);
        }
        break;
      }
      default: break;
    }
  }

  ImGui::End();

  return updated;
}

//--------------------------------------------------------------
void ofApp::draw()
{
  ofBackgroundGradient(ofColor::white, ofColor::gray);

  _imgui.begin();

  drawSidePanel();
  if (drawNodeParameters())
  {
    sendTexture();
  }

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
      for (Node* node : _selectedNodes)
      {
        node->bodyRect.setPosition(node->dragStart);
        node->headingRect.setPosition(node->dragStart);
      }
    }
    clearSelection();
    _mode = Mode::Default;
  }

  if (key == OF_KEY_DEL)
  {
    for (Node* node : _selectedNodes)
    {
      auto it = find(_nodes.begin(), _nodes.end(), node);
      _nodes.erase(it);

      if (_curEditingNode == node)
        _curEditingNode = nullptr;

      for (NodeConnector* con : node->inputs)
        deleteConnector(con);

      deleteConnector(node->output);
      delete node;
    }

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
  for (auto& node : _nodes)
  {
    if (node->bodyRect.inside(pt) || node->headingRect.inside(pt))
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
  _curEditingNode = nullptr;
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
    for (NodeConnector* input : node->inputs)
    {
      if (insideConnector(input->pt))
      {
        return input;
      }
    }

    // check the output node
    if (node->output->type != ParamType::Void && insideConnector(node->output->pt))
    {
      return node->output;
    }
  }

  return nullptr;
}

//--------------------------------------------------------------
void ofApp::deleteConnector(NodeConnector* con)
{
  // remove the connection from each of its connections
  for (NodeConnector* other : con->cons)
  {
    other->cons.erase(
      remove_if(other->cons.begin(), other->cons.end(), [=](NodeConnector* cand) { return cand == con; }),
      other->cons.end());
  }

  // clear the connection's cons
  con->cons.clear();
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
  ofPoint pt(x, y);

  // If the mouse is over one of the menus, just ignore the mouse pressed
  if (ImGui::IsMouseHoveringAnyWindow())
    return;

  if (_mode == Mode::Create)
  {
    // Create a new node from the template, and try to send it
    const NodeTemplate* t = _nodeTemplates[_createType];
    Node* node = new Node(t, pt, _nextNodeId++);
    _curEditingNode = node;
    _nodes.push_back(node);
    resetState();
    sendTexture();
    return;
  }

  // check if we clicked on any node connectors
  NodeConnector* con = connectorAtPoint(pt);
  if (con)
  {
    // ctrl-click to remove any connections
    if (ofKeyControl())
    {
      deleteConnector(con);
    }
    else
    {
      _startConnector = con;
      _endConnector = nullptr;
      _mode = Mode::Connecting;
    }
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

      sendTexture();
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
