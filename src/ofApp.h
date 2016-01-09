#pragma once

#include "ofMain.h"
#include "ofxGui.h"

enum class NodeType
{
  Create,
  LinearGradient,
  RadialGradient,
  Sinus,

  Modulate,
  RotateScale,
  Distort,
  ColorGradient,

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

struct ParamValue
{
  ofParameter<float> fValue;
  ofParameter<ofVec2f> vValue;
  ofParameter<ofColor> cValue;
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
  DragStart,
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

struct Scene;

struct Node
{
  struct Param
  {
    Param(const string& name, ParamType type) : name(name), type(type)
    {
      switch (type)
      {
        case ParamType::Void: break;
        case ParamType::Float: value.fValue.set(name, 0, 0, 100); break;
        case ParamType::Vec2: value.vValue.set(name, ofVec2f(0,0), ofVec2f(-1, -1), ofVec2f(1, 1)); break;
        case ParamType::Color: value.cValue.set(name, ofColor(100), ofColor(0), ofColor(255)); break;
        case ParamType::Texture: break;
        default: break;
      }
    }
    string name;
    ParamType type;
    ParamValue value;
  };

  Node(const NodeTemplate& t, const ofPoint& pt, Scene* scene);
  void draw();
  void drawConnections();
  void translate(const ofPoint& delta);

  string name;
  bool selected = false;
  ofPoint dragStart;
  ofRectangle bodyRect;
  ofRectangle headingRect;
  vector<Param> params;
  vector<NodeConnector> inputs;
  // NB: a node with no output has a void tyype for its connector
  NodeConnector output;
  Scene* scene;
};

struct Scene
{
  Scene(ofxPanel* varPanel);
  ~Scene();
  bool setup();
  void draw();

  void mouseDragged(int x, int y, int button);
  void mousePressed(int x, int y, int button);
  void mouseReleased(int x, int y, int button);
  void mouseMoved(int x, int y);

  void keyPressed(int key);
  void keyReleased(int key);

  void clearSelection();

  Node* nodeAtPoint(const ofPoint& pt);
  void resetState();
  void abortAction();

  void initNodes();

  void initNodeParameters(Node* node);

  ofRectangle calcTemplateRectangle(const NodeTemplate& node);

  NodeConnector* connectorAtPoint(const ofPoint& pt);

  vector<Node*> _nodes;
  vector<Node*> _selectedNodes;

  ofTrueTypeFont _font;

  Mode _mode;

  // NB: these could be made into a union, but this seems to require everything to have default
  // ctors, which I don't really want..
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
    NodeConnector* _startConnector;
    NodeConnector* _endConnector;
  };

  ofxPanel* _varPanel;

  unordered_map<NodeType, NodeTemplate> _nodeTemplates;
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

  void setupCreateNode(const void* sender);

  ofxPanel _genPanel;
  ofxPanel _modPanel;
  ofxPanel _memPanel;
  ofxPanel _varPanel;

  ofxPanel _mainPanel;

  vector<ofxButton*> _genButtons;
  vector<ofxButton*> _modButtons;
  vector<ofxButton*> _memButtons;
  unordered_map<const void*, NodeType> _buttonToType;

  Scene* _scene = nullptr;
};
