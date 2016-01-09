#pragma once

#include "ofMain.h"
#include "ofxGui.h"

enum class NodeType
{
  Create,

  Modulate,

  Load,
  Store,
};

enum class ParamType
{
  Void,
  Float,
  Vec2,
  Color,
  Texture,
};

union ParamValue {
  ParamValue(){};
  ~ParamValue(){};
  float fValue;
  ofVec2f vValue;
  ofColor cValue;
  string tValue;
};

enum NodeCategory
{
  CategoryGen,
  CategoryMod,
  CategoryMem,
};

// Templates are the descriptions of the node types
struct NodeTemplate
{
  struct NodeParam
  {
    string name;
    ParamType type;
  };

  string name;
  vector<NodeParam> inputs;
  vector<NodeParam> params;
  ParamType output;
  ofRectangle rect;
};

enum class Mode
{
  Default,
  Create,
  Dragging,
  Connecting,
};

struct Node;
struct NodeConnector
{
  enum class Dir
  {
    Input,
    Output,
  };

  NodeConnector() : type(ParamType::Void) {}
  NodeConnector(const string& name, ParamType type, Dir dir) : name(name), type(type), dir(dir) {}
  string name;
  ParamType type;
  Dir dir;
  ofRectangle rect;
  Node* node = nullptr;
};

struct Node
{
  struct Param
  {
    string name;
    ParamType type;
    ParamValue value;
  };

  Node(const string& name, const ofRectangle& rect, ofTrueTypeFont* font)
      : name(name), rect(rect), font(font)
  {
  }

  void draw();
  string name;
  bool selected = false;
  ofPoint dragStart;
  ofRectangle rect;
  vector<Param> params;
  vector<NodeConnector> inputs;
  // NB: a node with no output has a void tyype for its connector
  NodeConnector output;
  ofTrueTypeFont* font;
};

struct Scene
{
  Scene();
  ~Scene();
  bool setup();
  void draw();
  bool tryAddNode(int x, int y);

  void mouseDragged(int x, int y, int button);
  void mousePressed(int x, int y, int button);
  void mouseReleased(int x, int y, int button);

  void keyPressed(int key);
  void keyReleased(int key);

  void clearSelection();

  Node* nodeAtPoint(const ofPoint& pt);
  void resetState();
  void abortAction();

  ofRectangle calcTemplateRectangle(const NodeTemplate& node);

  NodeConnector* connectorAtPoint(const ofPoint& pt);

  vector<Node*> _nodes;
  vector<Node*> _selectedNodes;

  ofTrueTypeFont _verdana14;

  Mode _mode;

  union {
    struct
    {
      ofPoint _dragStart;
      ofPoint _lastDragPos;
    };
    struct
    {
      NodeType _createType;
    };
    struct  
    {
      NodeConnector* _connector;
    };
  };

  unordered_map<NodeType, NodeTemplate> _nodeTemplates;
};

class ofApp : public ofBaseApp
{
public:
  ofApp() {}
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

  void setupCreateNode(const void* sender);

  ofxPanel _genPanel;
  ofxPanel _modPanel;
  ofxPanel _memPanel;

  ofxPanel _mainPanel;

  vector<ofxButton*> _genButtons;
  vector<ofxButton*> _modButtons;
  vector<ofxButton*> _memButtons;
  unordered_map<const void*, NodeType> _buttonToType;

  Scene _scene;
};
