#include <vsg/all.h>

#include <fstream>
#include <iostream>


class MyPath
{
public:

    MyPath(const std::wstring& str):
        _wstring(str)
    {
        std::cout<<"MyPath(wstring) "<<str.size()<<std::endl;
    }
    MyPath(const std::string& str):
        _string(str)
    {
        std::cout<<"MyPath(string) "<<str<<std::endl;
    }

    std::string _string;
    std::wstring _wstring;

//    operator const char* () { return _string.c_str(); }
    operator const wchar_t* () { return _wstring.c_str(); }
};


void print(const char* str)
{
    std::cout<<"print(char_t) "<<str<<std::endl;
}
void print(const wchar_t* wstr)
{
    std::cout<<"print(wchar_t) "<<wstr<<std::endl;
}


int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    std::cout<<"vsgpath "<<std::endl;

    auto c_string = "c_string";
    auto w_string = L"w_string";

    std::cout<<"c_string "<<typeid(*c_string).name()<<std::endl;
    std::cout<<"w_string "<<typeid(*w_string).name()<<std::endl;

    MyPath path1("string 1");

    print(path1);

    return 0;
}
