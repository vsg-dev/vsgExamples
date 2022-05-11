#include <vsg/all.h>

#include <fstream>
#include <iostream>


int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    std::vector<vsg::Path> paths;
    paths.emplace_back("one");
    paths.emplace_back("one.dot");
    paths.emplace_back("one.dot.two");
    paths.emplace_back("three/");
    paths.emplace_back("four/.");
    paths.emplace_back("five/..");
    paths.emplace_back("six\\.");
    paths.emplace_back("seven\\..");
    paths.emplace_back(L"eight.dot");
    paths.emplace_back("nine/ten/eleven.dot.twelve");
    paths.emplace_back("thirteen.fourteen/fifeteen");
    paths.emplace_back("");
    paths.emplace_back(".");
    paths.emplace_back("..");
    paths.emplace_back(std::string("std.string"));
    paths.emplace_back(std::wstring(L"std.wstring"));

    for(auto& path : paths)
    {
        std::cout<<"path = "<<path<<std::endl;
        std::cout<<"    vsg::filePath(\""<<path<<"\") = "<<vsg::filePath(path)<<std::endl;
        std::cout<<"    vsg::fileExtension(\""<<path<<"\") = "<<vsg::fileExtension(path)<<std::endl;
        std::cout<<"    vsg::simpleFilename(\""<<path<<"\") = "<<vsg::simpleFilename(path)<<std::endl;
        std::cout<<"    vsg::removeExtension(\""<<path<<"\") = "<<vsg::removeExtension(path)<<std::endl;
        std::cout<<"    vsg::trailingRelativePath(\""<<path<<"\") = "<<vsg::trailingRelativePath(path)<<std::endl;

        auto combined = vsg::filePath(path) / vsg::simpleFilename(path) + vsg::fileExtension(path);
        std::cout<<"    combined = \""<<combined<<"\""<<std::endl;

        std::cout<<std::endl;
    }


    // appending with path separator
    {
        std::cout<<"\nvsg::Path::append(), /= and / operators"<<std::endl;

        vsg::Path path("first");
        path.append("second");
        std::cout<<"    path = "<<path<<std::endl;

        path = "third/";
        path /= "fourth"; // same as append
        std::cout<<"    path = "<<path<<std::endl;
        std::cout<<"    path = "<<(path / "fifth")<<std::endl;
        std::cout<<"    path = "<<(path / "fifth" / "sixth")<<std::endl;
    }

    // concat paths
    {
        std::cout<<"\nvsg::Path::concat(), += and + operators"<<std::endl;

        vsg::Path path("first");
        path.concat("second");
        std::cout<<"    path = "<<path<<std::endl;

        path = "third";
        path += "fourth"; // same as concat
        std::cout<<"    path = "<<path<<std::endl;
        std::cout<<"    path = "<<(path + "fifth")<<std::endl;
        std::cout<<"    path = "<<(path + "fifth" + "sixth")<<std::endl;
    }

    {
        //
        // test going between wstring and utf8 encoded string
        //
        std::wstring wide_string;
        for(wchar_t c = 0; c<32768; ++c)
        {
            wide_string.push_back(c);
        }

        std::cout<<"\nwide_string.size() = "<<wide_string.size()<<std::endl;

        std::string utf8;
        vsg::convert_utf(wide_string, utf8);

        std::wstring wide_copy;
        vsg::convert_utf(utf8, wide_copy);

        std::cout<<"utf8.size() = "<<utf8.size()<<std::endl;
        std::cout<<"wide_copy.size() = "<<wide_copy.size()<<std::endl;

        int result = wide_string.compare(wide_copy);
        std::cout<<"wide_string.comapare(wide_copy) = "<<result<<std::endl;

        vsg::Path path_from_string(utf8);
        vsg::Path path_from_wstring(wide_string);

        if (path_from_string == path_from_wstring)
        {
            std::cout<<"Success: both path_from_string and path_and_wstring are identical."<<std::endl;
        }
        else
        {
            std::cout<<"Problem: path_from_string and path_and_wstring are different."<<std::endl;
        }

    }

    return 0;
}
