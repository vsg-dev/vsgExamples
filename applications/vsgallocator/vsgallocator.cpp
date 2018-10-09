#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Value.h>
#include <vsg/core/Allocator.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/StateGroup.h>

#include <iostream>
#include <vector>
#include <chrono>

struct MyAllocator : public vsg::Object
{
    virtual void* allocate(std::size_t n, const void* hint )
    {
        std::cout<<"MyAllocator::allocate(std::size_t "<<n<<", const void*"<< hint<<" )"<<std::endl;

        return ::operator new (n);
    }

    virtual void* allocate(std::size_t size)
    {
        std::cout<<"MyAllocator::allocate(std::size_t "<<size<<")"<<std::endl;
        return ::operator new (size);
    }


    virtual void* deallocate(void* ptr, std::size_t size=0)
    {
        std::cout<<"MyAllocator::deallocate("<<ptr<<"std::size_t "<<size<<")"<<std::endl;
        ::operator delete(ptr);
    }
};

template<typename T>
struct allocator_adapter
{
    using value_type = T;

    allocator_adapter() = default;
    allocator_adapter(const allocator_adapter& rhs) = default;

    allocator_adapter(MyAllocator* allocator) : _allocator(allocator) {}

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

    vsg::ref_ptr<MyAllocator> _allocator;
};



template<class T>
vsg::ref_ptr<T> create(vsg::Allocator* allocator)
{
    if (allocator)
    {
        void* ptr = allocator->allocate(sizeof(T));
        T* object = new (ptr) T;
        vsg::Auxiliary* auxiliary = object->getOrCreateUniqueAuxiliary();
        auxiliary->setAllocator(allocator);
        return object;
    }
    else
    {
        return new T;
    }
}

template<class T>
vsg::ref_ptr<T> create(vsg::Auxiliary* auxiliary)
{
    if (auxiliary)
    {

        if (vsg::Allocator* allocator = auxiliary->getAllocator())
        {
            void* ptr = allocator->allocate(sizeof(T));
            T* object = new (ptr) T;
            object->setAuxiliary(auxiliary);
            return object;
        }
        else
        {
            T* object = new T;
            object->setAuxiliary(auxiliary);
            return object;
        }
    }
    else
    {
        return new T;
    }
}

int main(int /*argc*/, char** /*argv*/)
{
#if 0
    {
        vsg::ref_ptr<MyAllocator> allocator = new MyAllocator;

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
        allocator_adapter<vsg::vec3> local_allocator(new MyAllocator);
        std::vector<vsg::vec3, decltype(local_allocator)> my_vertices3(72,local_allocator);
        std::cout<<"before resize"<<std::endl;
        my_vertices3.resize(4);
        std::cout<<std::endl;
        std::cout<<"before shrink_to_fit"<<std::endl;
        my_vertices3.shrink_to_fit();
        std::cout<<"after shrink_to_fit"<<std::endl;
    }

    {
        vsg::ref_ptr<vsg::Allocator> allocator = new vsg::Allocator;
        std::cout<<std::endl;
        std::cout<<"Before create using Allocator"<<std::endl;
        {
            auto mode = create<vsg::Node>(allocator);
            auto group = create<vsg::QuadGroup>(allocator);
        }
        std::cout<<"After create"<<std::endl;
    }
#endif
    {
        vsg::ref_ptr<vsg::Auxiliary> auxiliary = new vsg::Auxiliary(new vsg::Allocator);
        std::cout<<std::endl;
        std::cout<<"Before create using Auxiliary"<<std::endl;
        {
            auto mode = create<vsg::Node>(auxiliary);
            auto group = create<vsg::QuadGroup>(auxiliary);

            group->setValue("fred", 1.0);
            std::cout<<"Before end of block\n"<<std::endl;
        }
        std::cout<<"After create\n"<<std::endl;
    }
    return 0;
}
