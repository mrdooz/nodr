#pragma once

#include "ofMain.h"
#include "ofxGui.h"

enum class Mode
{
  Default,
  Dragging,
};

struct Node;
struct NodeInput
{
  NodeInput(const string& name) : name(name) {}
  string name;
  ofPoint offset;
  shared_ptr<Node> node;
};

struct Node
{
  Node(const string& name, const ofRectangle& rect, ofTrueTypeFont* font)
      : name(name), rect(rect), font(font)
  {
  }

  void draw();
  string name;
  bool selected = false;
  ofPoint dragStart;
  ofRectangle rect;
  vector<NodeInput> inputs;
  ofTrueTypeFont* font;
};

struct Scene
{
  bool setup();
  void draw();
  bool tryAddNode(int x, int y);

  void mouseDragged(int x, int y, int button);
  void mousePressed(int x, int y, int button);
  void mouseReleased(int x, int y, int button);

  void keyPressed(int key);
  void keyReleased(int key);

  void clearSelection();

  shared_ptr<Node> nodeAtPoint(int x, int y);

  vector<shared_ptr<Node>> _nodes;
  vector<shared_ptr<Node>> _selectedNodes;

  ofTrueTypeFont _verdana14;

  Mode _mode;
  ofPoint _dragStart;
  ofPoint _lastDragPos;
};

class ofApp : public ofBaseApp
{
public:
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

  Scene _scene;
};
