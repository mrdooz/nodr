#pragma once

//--------------------------------------------------------------
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

//--------------------------------------------------------------
void parseString(const string& str, float* res);
void parseString(const string& str, int* res);
void parseString(const string& str, string* res);

template <typename... Attrs>
void getAttributes(ofxXmlSettings& s, const string& tag, int which)
{
}

template <typename T, typename... Attrs>
void getAttributes(
  ofxXmlSettings& s, const string& tag, int which, const string& attr, T* value, Attrs... attrs)
{
  parseString(s.getAttribute(tag, attr, "", which), value);
  getAttributes(s, tag, which, attrs...);
}

#define GEN_NAME2(prefix, line) prefix##line
#define GEN_NAME(prefix, line) GEN_NAME2(prefix, line)
#define CREATE_TAG(tag, i, ...) TagCreator GEN_NAME(ANON, __LINE__)(s, tag, i, true, __VA_ARGS__)
#define CREATE_LOCAL_TAG(tag, i, ...) TagCreator GEN_NAME(ANON, __LINE__)(s, tag, i, false, __VA_ARGS__)
