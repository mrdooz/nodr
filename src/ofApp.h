#pragma once


enum class ParamType
{
  Void,
  Bool,
  Int,
  Float,
  Vec2,
  Color,
  Texture,
  String,
};

enum ParamFlag
{
  PARAM_FLAG_HAS_MIN_MAX = 0x1,
};

struct ParamInt
{
  int value = 0;
  int minValue, maxValue;
};

struct ParamFloat
{
  float value = 0;
  float minValue, maxValue;
};

struct ParamVec2
{
  ofVec2f value = ofVec2f(0,0);
  float minValue, maxValue;
};

struct ParamValue
{
  uint32_t flags = 0;
  ParamInt iValue;
  ParamFloat fValue;
  ParamVec2 vValue;
  ofColor_<float> cValue;
  bool bValue;
  string sValue;
};

// Templates are the descriptions of the node types
struct NodeTemplate
{
  struct NodeParam
  {
    NodeParam(const string& name, ParamType type) : name(name), type(type) {}
    string name;
    ParamType type;
    bool hasBounds = false;
    ParamValue bounds;
  };

  void calcTemplateRectangle(ofTrueTypeFont& font);

  string name;
  vector<NodeParam> inputs;
  vector<NodeParam> params;
  ParamType output;
  int id;
  ofRectangle rect;
};

struct Node;
struct NodeConnector
{
  enum class Dir
  {
    Input,
    Output,
  };

  NodeConnector() : parent(nullptr), type(ParamType::Void) {}
  NodeConnector(const string& name, ParamType type, Dir dir, const ofPoint& pt, Node* parent)
      : name(name), type(type), dir(dir), pt(pt), parent(parent)
  {
  }
  string name;
  ParamType type;
  Dir dir;
  ofPoint pt;
  Node* parent;
  vector<NodeConnector*> cons;
};

class ofApp;

struct TextureSettings
{
  int numAuxTextures = 8;
};

struct Node
{
  struct Param
  {
    Param(const string& name, ParamType type, const ParamValue& value) : name(name), type(type), value(value)
    {
    }
    string name;
    ParamType type;
    ParamValue value;
  };

  Node(const NodeTemplate* t, const ofPoint& pt, int it);
  void draw();
  void drawConnections();
  void translate(const ofPoint& delta);
  Param* findParam(const string& str);
  NodeConnector* findConnector(const string& str);

  string name;
  bool selected = false;
  ofPoint dragStart;
  ofRectangle bodyRect;
  ofRectangle headingRect;
  vector<Param> params;
  vector<NodeConnector*> inputs;
  // NB: a node with no output has a void type for its connector
  NodeConnector* output;
  int id;
};

class ofApp : public ofBaseApp
{
public:
  ofApp();
  ~ofApp();
  void setup();
  void update();
  void draw();

  void exit();

  void keyPressed(int key);
  void keyReleased(int key);
  void mouseMoved(int x, int y);
  void mouseDragged(int x, int y, int button);
  void mousePressed(int x, int y, int button);
  void mouseReleased(int x, int y, int button);
  void mouseEntered(int x, int y);
  void mouseExited(int x, int y);
  void windowResized(int w, int h);
  void dragEvent(ofDragInfo dragInfo);
  void gotMessage(ofMessage msg);

  void clearSelection();

  Node* nodeAtPoint(const ofPoint& pt);
  void resetState();
  void abortAction();

  void saveToFile(const string& filename);
  void loadFromFile(const string& filename);
  void loadTemplates();

  void resetTexture();

  NodeConnector* connectorAtPoint(const ofPoint& pt);
  Node* nodeById(int id);

  bool createGraph(const vector<Node*> nodes, vector<Node*>* sortedNodes);
  bool generateGraph(vector<char>* buf);

  bool drawNodeParameters();
  void drawSidePanel();

  void deleteConnector(NodeConnector* con);
  void sendTexture();

  enum class Mode
  {
    Default,
    Create,
    DragStart,
    Dragging,
    Connecting,
  };

  struct
  {
    ofPoint _dragStart;
    ofPoint _lastDragPos;
  };
  struct
  {
    string _createType;
  };
  struct
  {
    NodeConnector* _startConnector;
    NodeConnector* _endConnector;
  };

  unordered_map<string, NodeTemplate*> _nodeTemplates;
  unordered_map<string, vector<NodeTemplate*>> _templatesByCategory;

  vector<Node*> _nodes;
  vector<Node*> _selectedNodes;
  Node* _curEditingNode = nullptr;

  ofTrueTypeFont _font;

  Mode _mode = Mode::Default;

  int _nextNodeId = 1;

  ofxImGui _imgui;
  HANDLE _pipeHandle = INVALID_HANDLE_VALUE;
};
