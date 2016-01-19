#include "nodr_utils.hpp"

using namespace std;

//--------------------------------------------------------------
bool showFileDialog(bool openFile, const char* filter, const char* defaultExt, string* filename)
{
  char szFileName[MAX_PATH];
  ZeroMemory(szFileName, sizeof(szFileName));

  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  //ofn.lpstrFilter = "Textures (*.xml)\0*.xml\0All Files (*.*)\0*.*\0";
  //ofn.lpstrDefExt = "xml";
  ofn.lpstrFilter = filter;
  ofn.lpstrDefExt = defaultExt;
  ofn.lpstrFile = szFileName;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;

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
