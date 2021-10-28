#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <vector>

template<typename T>
struct allocator_adapter
{
    using value_type = T;

    allocator_adapter() noexcept
    {
        std::cout << "allocator_adapter::allocator_adapter()" << std::endl;
    }

    template<class U>
    allocator_adapter(const allocator_adapter<U>& rhs) noexcept :
        _allocator(rhs._allocator)
    {
        std::cout << "allocator_adapter::allocator_adapter(const allocator_adapter<U>& ) _allocator=" << _allocator.get() << std::endl;
    }

    explicit allocator_adapter(vsg::Allocator* allocator) noexcept :
        _allocator(allocator)
    {
        std::cout << "allocator_adapter::allocator_adapter(vsg::Allocator*) _allocator=" << _allocator.get() << std::endl;
    }

    value_type* allocate(std::size_t n)
    {
        std::cout << "allocator_adapter::allocate(" << n << ") _allocator=" << _allocator.get() << std::endl;
        const std::size_t size = n * sizeof(value_type);
        return static_cast<value_type*>(_allocator ? _allocator->allocate(size) : (::operator new(size)));
    }

    void deallocate(value_type* ptr, std::size_t n)
    {
        std::cout << "allocator_adapter::deallocate(" << n << ") _allocator=" << _allocator.get() << std::endl;
        const std::size_t size = n * sizeof(value_type);
        if (_allocator)
            _allocator->deallocate(ptr, size);
        else
            ::operator delete(ptr);
    }

    vsg::ref_ptr<vsg::Allocator> _allocator;
};

template<class T, class U>
bool operator==(const allocator_adapter<T>& lhs, const allocator_adapter<U>& rhs) noexcept
{
    return lhs._allocator == rhs._allocator;
}

template<class T, class U>
bool operator!=(const allocator_adapter<T>& lhs, const allocator_adapter<U>& rhs) noexcept
{
    return lhs._allocator != rhs._allocator;
}

int main(int /*argc*/, char** /*argv*/)
{
    {
        std::cout << std::endl;
        std::cout << "Before vsg::Allocator for std::vector" << std::endl;

        vsg::ref_ptr<vsg::Allocator> allocator(new vsg::Allocator);

        using allocator_vec3 = allocator_adapter<vsg::vec3>;
        using std_vector = std::vector<vsg::vec3>;
        using my_vector = std::vector<vsg::vec3, allocator_vec3>;
        std::cout << "\n";
        std::cout << "sizeof(std_vector) " << sizeof(std_vector) << std::endl;
        std::cout << "sizeof(my_vector) " << sizeof(my_vector) << std::endl;
        {
            std::vector<vsg::vec3> vertices(4);
            std::vector<vsg::vec3, allocator_vec3> my_vertices(16, allocator_vec3(allocator));
            std::vector<vsg::vec3, allocator_vec3> my_vertices2(36, allocator_vec3(allocator));
            std::cout << "sizeof(my_vertices2) " << sizeof(my_vertices2) << std::endl;
        }
        std::cout << std::endl;
        allocator_adapter<vsg::vec3> local_allocator(new vsg::Allocator);
        std::vector<vsg::vec3, decltype(local_allocator)> my_vertices3(72, local_allocator);
        std::cout << "before resize" << std::endl;
        my_vertices3.resize(4);
        std::cout << std::endl;
        std::cout << "before shrink_to_fit" << std::endl;
        my_vertices3.shrink_to_fit();
        std::cout << "after shrink_to_fit" << std::endl;

        std::cout << "before copy" << std::endl;
        auto copy_vertices = my_vertices3;
        std::cout << "after copy" << std::endl;
    }

    {
        std::cout << std::endl;
        std::cout << "Before vsg::Object::create() for nodes" << std::endl;
        {
            vsg::ref_ptr<vsg::Allocator> allocator(new vsg::Allocator);

#if 1
            auto group = vsg::Group::create(allocator, 4);
            auto quadgroup = vsg::QuadGroup::create(allocator);
#else
            auto group = allocator->create<vsg::Group>(4);
            auto quadgroup = allocator->create<vsg::QuadGroup>();
#endif

            group->setValue("ginger", 2.0);
            group->setValue("nutmeg", 3.0f);

            std::cout << "Before allocator = nullptr;\n"
                      << std::endl;

            allocator = nullptr;

            std::cout << "Before end of block\n"
                      << std::endl;
        }
        std::cout << "After create\n"
                  << std::endl;
    }
    return 0;
}
