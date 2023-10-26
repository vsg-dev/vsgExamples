#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

extern vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options);


int main(int argc, char** argv)
{
    // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create();
#ifdef vsgXchange_FOUND
    options->add(vsgXchange::all::create());
#endif
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // read any command line options that the ReaderWriters support
    arguments.read(options);
    if (argc <= 1) return 0;

    auto inputFilename = arguments.value<vsg::Path>("", "-i");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    vsg::ref_ptr<vsg::ShaderSet> shaderSet = pbr_ShaderSet(options);
    options->shaderSets["pbr"] = shaderSet;

    std::cout<<"shaderSet = "<<shaderSet<<std::endl;
    if (!shaderSet)
    {
        std::cout<<"No vsg::ShaderSet to process."<<std::endl;
        return 1;
    }

    if (outputFilename)
    {
        vsg::write(shaderSet, outputFilename, options);
    }

    return 0;
}
