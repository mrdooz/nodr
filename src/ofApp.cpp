#include "ofApp.h"

static const int FONT_HEIGHT = 12;
static const int FONT_PADDING = 4;
static const int HEADING_SIZE = FONT_HEIGHT + 2 * FONT_PADDING;
static const int RECT_UPPER_ROUNDING = 4;
static const int RECT_LOWER_ROUNDING = 2;
static const int INPUT_HEIGHT = 14;
static const int INPUT_PADDING = 5;

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
  for (const NodeInput& input : inputs)
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
bool Scene::setup()
{
  _verdana14.load("verdana.ttf", FONT_HEIGHT, true, true);
  _verdana14.setLineHeight(FONT_HEIGHT);
  _verdana14.setLetterSpacing(1.037);
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
  // check for overlap
  int w = 100;
  int h = 100;

  ofRectangle cand(x, y, w, h);
  for (auto& node : _nodes)
  {
    if (cand.intersects(node->rect))
    {
      // candidate overlaps with an existing rectangle, so bail
      return false;
    }
  }

  auto node = make_shared<Node>("hello", cand, &_verdana14);
  node->inputs.push_back(NodeInput("a"));
  node->inputs.push_back(NodeInput("b"));
  _nodes.push_back(node);

  if (_nodes.size() == 2)
  {
    _nodes[0]->inputs[0].node = node;
  }

  return true;
}

//--------------------------------------------------------------
shared_ptr<Node> Scene::nodeAtPoint(int x, int y)
{
  for (auto& node : _nodes)
  {
    if (node->rect.inside(x, y))
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
void Scene::mousePressed(int x, int y, int button)
{
  auto node = nodeAtPoint(x, y);
  if (!node)
  {
    clearSelection();
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
  if (_mode == Mode::Default)
  {
    tryAddNode(x, y);
  }
  else
  {
    _mode = Mode::Default;
  }
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
void ofApp::setup()
{
  ofSetVerticalSync(true);
  ofGetMainLoop()->setEscapeQuitsLoop(false);

  _genPanel.setup("Generators");
  _modPanel.setup("Modifiers");
  _memPanel.setup("Memory");

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
