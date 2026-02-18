#include <lib.hpp>
#include<iostream>
using namespace std;

DirWalker::ACTION replacex(DirWalker::STATUS status, File file, void* pay){
  if(!file.isDir){
    FileReader reader(file);
    cout << file.path << endl;
    FileWriter writer(reader.snapshot()); 
    writer.replaceAll("ctrl_c_ctrl_v_3000", "copyPasta").commit();
  }
  return DirWalker::CONTINUE;
};

int main(int argc, char** argv) {
    std::string path = ".";
    DirWalker walker(path);
    walker.recursive = true;
    //walker.walk(replacex);
    File file("sample12.txt");
    file.rename(file, "sample.txt");
    cout<< file.path << endl;
    return 0;
}
