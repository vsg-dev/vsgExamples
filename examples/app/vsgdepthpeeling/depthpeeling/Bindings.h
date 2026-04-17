#pragma once

#include <vsg/vk/Context.h>

#include <vsg/io/Options.h>
#include <vsg/utils/ShaderSet.h>

#include <vsg/state/StateCommand.h>
#include <vsg/state/PipelineLayout.h>
#include <vsg/state/BindDescriptorSet.h>

namespace vsg::oit::depthpeeling {

    class Resources;

    class DescriptorSetBinding : public Inherit<vsg::StateCommand, DescriptorSetBinding>
    {
    protected:
        vsg::observer_ptr<const Resources> _resources;

        vsg::ref_ptr<vsg::Context> _context;
        mutable vsg::Context* _compiledWithContext = nullptr;

        vsg::ref_ptr<vsg::ShaderSet> _shaderSet;
        vsg::ref_ptr<vsg::PipelineLayout> _layout;

        mutable vsg::ref_ptr<vsg::BindDescriptorSet> _binding;

    public:
        DescriptorSetBinding(Resources& r,
            vsg::ref_ptr<vsg::ShaderSet> s, vsg::ref_ptr<vsg::PipelineLayout> l);

        template<class N, class V>
        static void t_traverse(N& dsb, V& visitor)
        {
            if (dsb._layout.valid())
            {
                dsb._layout->accept(visitor);
            }
            if (dsb._binding.valid())
            {
                dsb._binding->accept(visitor);
            }
        }

        void traverse(Visitor& visitor) override 
        { 
            t_traverse(*this, visitor); 
        }
        void traverse(ConstVisitor& visitor) const override 
        { 
            t_traverse(*this, visitor); 
        }

        void dirty()
        {
            _compiledWithContext = nullptr;
        }

        void compile(Context& context) override;
        void record(CommandBuffer& commandBuffer) const override;

    protected:
        bool ensureBindingUpToDate() const
        {
            if (_compiledWithContext != nullptr || !_resources.valid()
                || !_context.valid() || !_layout.valid() || !_shaderSet.valid())
            {
                return _binding.valid();
            }

            _binding = createBinding();
            if (!_binding.valid())
            {
                return false;
            }

            _binding->compile(*_context);
            const_cast<decltype(slot)&>(slot) = _binding->slot;

            _compiledWithContext = _context.get();
            return true;
        }
        virtual vsg::ref_ptr<vsg::BindDescriptorSet> createBinding() const = 0;
    };

    class BindPeelDescriptorSet final : public Inherit<DescriptorSetBinding, BindPeelDescriptorSet>
    {
        int32_t _peelIndex;

    public:
        BindPeelDescriptorSet(Resources& r,
            vsg::ref_ptr<vsg::ShaderSet> s, vsg::ref_ptr<vsg::PipelineLayout> l, int32_t i);

    protected:
        ref_ptr<BindDescriptorSet> createBinding() const override;
    };

    class CombineDescriptorSetBinding : public Inherit<DescriptorSetBinding, CombineDescriptorSetBinding>
    {
    public:
        CombineDescriptorSetBinding(Resources& r,
            vsg::ref_ptr<vsg::ShaderSet> s, vsg::ref_ptr<vsg::PipelineLayout> l);

    protected:
        virtual ref_ptr<ImageView> getInputImageView() const = 0;
        virtual std::string inputImageViewDefine() const = 0;

        ref_ptr<BindDescriptorSet> createBinding() const override;
    };

    class BindPeelCombineDescriptorSet final : public Inherit<CombineDescriptorSetBinding, BindPeelCombineDescriptorSet>
    {
    public:
        BindPeelCombineDescriptorSet(Resources& r,
            vsg::ref_ptr<vsg::ShaderSet> s, vsg::ref_ptr<vsg::PipelineLayout> l);

    protected:
        ref_ptr<ImageView> getInputImageView() const override;
        std::string inputImageViewDefine() const override;
    };

    class BindFinalCombineDescriptorSet : public Inherit<CombineDescriptorSetBinding, BindFinalCombineDescriptorSet>
    {
    public:
        BindFinalCombineDescriptorSet(Resources& r,
            vsg::ref_ptr<vsg::ShaderSet> s, vsg::ref_ptr<vsg::PipelineLayout> l);

    protected:
        ref_ptr<ImageView> getInputImageView() const override;
        std::string inputImageViewDefine() const override;
    };

}

EVSG_type_name(vsg::oit::depthpeeling::DescriptorSetBinding);
EVSG_type_name(vsg::oit::depthpeeling::BindPeelDescriptorSet);
EVSG_type_name(vsg::oit::depthpeeling::CombineDescriptorSetBinding);
EVSG_type_name(vsg::oit::depthpeeling::BindPeelCombineDescriptorSet);
EVSG_type_name(vsg::oit::depthpeeling::BindFinalCombineDescriptorSet);
