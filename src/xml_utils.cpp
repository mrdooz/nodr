#include "xml_utils.hpp"

void parseString(const string& str, float* res)
{
  *res = (float)atof(str.c_str());
}

void parseString(const string& str, int* res)
{
  *res = atoi(str.c_str());
}

void parseString(const string& str, string* res)
{
  *res = str;
}
