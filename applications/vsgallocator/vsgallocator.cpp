#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Value.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/StateGroup.h>

#include <osg/ref_ptr>
#include <osg/Referenced>
#include <osg/Group>
#include <osg/Timer>
#include <osg/Observer>
#include <osg/UserDataContainer>

#include <iostream>
#include <vector>
#include <chrono>

struct Allocator : public vsg::Object
{
    virtual void* allocate(std::size_t n, const void* hint )
    {
        std::cout<<"Allocator::allocate(std::size_t "<<n<<", const void*"<< hint<<" )"<<std::endl;

        return ::operator new (n);
    }

    virtual void* allocate(std::size_t size)
    {
        std::cout<<"Allocator::allocate(std::size_t "<<size<<")"<<std::endl;
        return ::operator new (size);
    }


    virtual void* deallocate(void* ptr, std::size_t size=0)
    {
        std::cout<<"Allocator::deallocate("<<ptr<<"std::size_t "<<size<<")"<<std::endl;
        ::operator delete(ptr);
    }
};

template<typename T>
struct allocator_adapter
{
    using value_type = T;

    allocator_adapter() = default;
    allocator_adapter(const allocator_adapter& rhs) = default;

    allocator_adapter(Allocator* allocator) : _allocator(allocator) {}

    allocator_adapter& operator = (const allocator_adapter& rhs) = default;

    value_type* allocate( std::size_t n, const void* /*hint*/ )
    {
        const std::size_t size = n * sizeof(value_type);
        return static_cast<value_type*>(_allocator ? _allocator->allocate(size) : (::operator new (size)));
    }

    value_type* allocate( std::size_t n )
    {
        const std::size_t size = n * sizeof(value_type);
        return static_cast<value_type*>(_allocator ? _allocator->allocate(size) : (::operator new (size)));
    }

    void deallocate(value_type* ptr, std::size_t n)
    {
        const std::size_t size = n * sizeof(value_type);
        if (_allocator) _allocator->deallocate(ptr, size);
        else ::operator delete(ptr);
    }

    vsg::ref_ptr<Allocator> _allocator;
};


int main(int /*argc*/, char** /*argv*/)
{
    vsg::ref_ptr<Allocator> allocator = new Allocator;

    using allocator_vec3 = allocator_adapter<vsg::vec3>;
    using std_vector = std::vector<vsg::vec3>;
    using my_vector = std::vector<vsg::vec3, allocator_vec3>;
    std::cout<<"\n";
    std::cout<<"sizeof(std_vector) "<<sizeof(std_vector)<<std::endl;
    std::cout<<"sizeof(my_vector) "<<sizeof(my_vector)<<std::endl;
    {
    std::vector<vsg::vec3> vertices(4);
    std::vector<vsg::vec3, allocator_adapter<vsg::vec3>> my_vertices(16, allocator.get());
    std::vector<vsg::vec3, allocator_adapter<vsg::vec3>> my_vertices2(36, allocator.get());
    std::cout<<"sizeof(my_vertices2) "<<sizeof(my_vertices2)<<std::endl;
    }
    std::cout<<std::endl;
    allocator_adapter<vsg::vec3> local_allocator(new Allocator);
    std::vector<vsg::vec3, decltype(local_allocator)> my_vertices3(72,local_allocator);
    std::cout<<"before resize"<<std::endl;
    my_vertices3.resize(4);
    std::cout<<std::endl;
    std::cout<<"before shrink_to_fit"<<std::endl;
    my_vertices3.shrink_to_fit();
    std::cout<<"after shrink_to_fit"<<std::endl;

    return 0;
}
