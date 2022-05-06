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
    paths.emplace_back("therteen.fourteen/fifeteen");
    paths.emplace_back("");
    paths.emplace_back(".");
    paths.emplace_back("..");

    for(auto& path : paths)
    {
        std::cout<<"path = "<<path<<std::endl;
        std::cout<<"    vsg::filePath(\""<<path<<"\") = "<<vsg::filePath(path)<<std::endl;
        std::cout<<"    vsg::fileExtension(\""<<path<<"\") = "<<vsg::fileExtension(path)<<std::endl;
        std::cout<<"    vsg::simpleFilename(\""<<path<<"\") = "<<vsg::simpleFilename(path)<<std::endl;
        std::cout<<std::endl;
    }



    return 0;
}
