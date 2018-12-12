#include <osg/Group>
#include <osg/NodeVisitor>
#include <osg/ArgumentParser>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

//#define INLINE_TRAVERSE

class OsgVisitor: public osg::NodeVisitor
{
public:

    OsgVisitor():
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
        numNodes(0)
    {}

    unsigned int numNodes;

    using NodeVisitor::apply;

    void apply(osg::Node& node) override
    {
        ++numNodes;
        traverse(node);
    }

};

osg::Node* createOsgQuadTree(unsigned int numLevels, unsigned int& numNodes, unsigned int& numBytes)
{
    if (numLevels==0)
    {
        numNodes += 1;
        numBytes += sizeof(osg::Node);

        return new osg::Node;
    }

    osg::Group* t = new osg::Group;

    numNodes += 1;
    numBytes += sizeof(osg::Group) + 4*sizeof(osg::ref_ptr<osg::Node>);

    --numLevels;

    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));

    return t;
}

int main(int argc, char** argv)
{
    osg::ArgumentParser arguments(&argc, argv);

    uint32_t numLevels = 11;
    uint32_t numTraversals = 10;
    if (arguments.read("-l",numLevels)) {}
    if (arguments.read("-t",numTraversals)) {}

    bool quiet = arguments.read("-q");

    std::string inputFilename, outputFilename;
    if (arguments.read("-i", inputFilename)) {}
    if (arguments.read("-o", outputFilename)) {}


    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    unsigned int numNodes = 0;
    unsigned int numBytes = 0;
    osg::ref_ptr<osg::Node> osg_root;

    if (!inputFilename.empty())
    {
        osg_root = osgDB::readNodeFile(inputFilename);
    }
    else
    {
        osg_root = createOsgQuadTree(numLevels, numNodes, numBytes);
    }

    clock::time_point after_construction = clock::now();

    unsigned int numNodesVisited = 0;

    std::cout<<"using OsgVisitor"<<std::endl;
    for(unsigned int i=0; i<numTraversals; ++i)
    {
        OsgVisitor visitor;
        osg_root->accept(visitor);
        numNodesVisited += visitor.numNodes;
    }

    clock::time_point after_traversal = clock::now();

    if (!outputFilename.empty())
    {
        osgDB::writeNodeFile(*osg_root, outputFilename);
    }

    clock::time_point after_write = clock::now();

    osg_root = 0;

    clock::time_point after_destruction = clock::now();

    if (!quiet)
    {
        std::cout<<"numNodes : "<<numNodes<<std::endl;
        std::cout<<"numBytes : "<<numBytes<<std::endl;
        std::cout<<"average node size : "<<double(numBytes)/double(numNodes)<<std::endl;
        std::cout<<"numNodesVisited : "<<numNodesVisited<<std::endl;
        if (!inputFilename.empty()) std::cout<<"read time : "<<std::chrono::duration<double>(after_construction-start).count()<<std::endl;
        else std::cout<<"construcion time : "<<std::chrono::duration<double>(after_construction-start).count()<<std::endl;
        std::cout<<"traversal time : "<<std::chrono::duration<double>(after_traversal-after_construction).count()<<std::endl;
        if (!outputFilename.empty()) std::cout<<"write time : "<<std::chrono::duration<double>(after_write-after_traversal).count()<<std::endl;
        std::cout<<"destrucion time : "<<std::chrono::duration<double>(after_destruction-after_write).count()<<std::endl;
        std::cout<<"total time : "<<std::chrono::duration<double>(after_destruction-start).count()<<std::endl;
        std::cout<<std::endl;
        std::cout<<"Nodes constructed per second : "<<double(numNodes)/std::chrono::duration<double>(after_construction-start).count()<<std::endl;
        std::cout<<"Nodes visited per second     : "<<double(numNodesVisited)/std::chrono::duration<double>(after_traversal-after_construction).count()<<std::endl;
        std::cout<<"Nodes destrctored per second : "<<double(numNodes)/std::chrono::duration<double>(after_destruction-after_traversal).count()<<std::endl;
    }

    return 0;
}
