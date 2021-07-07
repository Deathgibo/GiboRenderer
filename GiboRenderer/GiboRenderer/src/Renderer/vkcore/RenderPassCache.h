#pragma once
#include "../../pch.h"

namespace Gibo
{
	/*
	This class creates renderpasses and stores them in a vector. It handles all the memory manamgent of them. It also has a nice interface for
	you to create attachments, subpasses, and dependencies and pass them in. One function just takes attachments, another one takes in all 3.

	renderpasses consist of attachments which would be color, depth, resolve, inputattachment, and preserve. You can specify the tranisiton of layouts of these,
	the load and store operations, etc.
	With these attachments you can specify a number of subpasses, really all these contain are just what attachments you want for each subpass.
	Input attachments are specified in the actual shader and just represent the attachment coming in from the last subpass
	Finally there are dependencies, which specify dependencies between each subpass based of memory access and pipelinestage.

	Using subpasses doesn't help too much, its really meant for use on mobile where it does matter.

	colorattachments: corresponds to output location in a shader. If shader specifies location X, that references pColorAttachments[X].
		In shader: layout(location = x) vec4 outcolor;
	
	inputattachments: corresponds to input attachment index in fragment shader. If a shader declares an image variable decorated with a InputAttachmentIndex value of X, it uses pInputAttachments[X].
		In shader: layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;
	
	preserveattachments: These are just attachments that are not used during subpass, but need to be preserved.
	
	depthstencilattachment: just depth/stencil attachments that get written too, not sure if you have any shader control over this.
	
	resolveattachment: has to do with multisampling. Pretty sure it takes all the samples in the color attachment and combines them into 1. resolve the image.

	It is advisable that applications use as few render passes as possible, because changing render targets is a fundamentally expensive operation

	for layouts initial_layout is what it expects it to be, if you do undefined it just means te data can't be preserved. final_layout is what
	it will transition it to after renderpass. And te attacment layout is what it will transition it to before the renderpass.
	*/

	enum class RENDERPASSTYPE : uint32_t {COLOR, DEPTH, RESOLVE, INPUT, PRESERVE};
	
	struct RenderPassAttachment
	{
		uint32_t mattachmentnumber;
		VkFormat mformat;
		VkSampleCountFlagBits msample_count;
		VkImageLayout mcurrent_layout;
		VkImageLayout minitial_layout;
		VkImageLayout mfinal_layout;
		VkAttachmentLoadOp mloadop;
		VkAttachmentStoreOp mstoreop;
		VkAttachmentLoadOp mstencilloadop;
		VkAttachmentStoreOp mstencilstoreop;
		RENDERPASSTYPE mtype;

		RenderPassAttachment(uint32_t attachment_number, VkFormat format, VkSampleCountFlagBits sample_count, VkImageLayout current_layout, VkImageLayout initial_layout, VkImageLayout final_layout,
			VkAttachmentLoadOp loadop, VkAttachmentStoreOp storeop, VkAttachmentLoadOp stencilloadop, VkAttachmentStoreOp stencilstoreop, RENDERPASSTYPE type) : mattachmentnumber(attachment_number),
			mformat(format), msample_count(sample_count), mcurrent_layout(current_layout), minitial_layout(initial_layout), mfinal_layout(final_layout), mloadop(loadop),mstoreop(storeop),
			mstencilloadop(stencilloadop),mstencilstoreop(stencilstoreop), mtype(type) { }
	};

	struct RenderPassSubPass 
	{
		std::vector<int> input_attachment;
		std::vector<int>  color_attachment;
		std::vector<int>  depthstencil_attachment;
		std::vector<int>  resolve_attachment;
		VkPipelineBindPoint bindpoint;

		RenderPassSubPass(VkPipelineBindPoint bindpoint_, std::vector<int>  input_attachment_, std::vector<int>  color_attachment_, std::vector<int>  depthstencil_attachment_, std::vector<int>  resolve_attachment_, std::vector<int>  preserve_attachment_) :
			bindpoint(bindpoint_), input_attachment(input_attachment_), color_attachment(color_attachment_), depthstencil_attachment(depthstencil_attachment_), resolve_attachment(resolve_attachment_)
			{}
	};

	struct RenderPassDependency 
	{
		uint32_t srcsubpass;
		uint32_t dstsubpass;
		VkPipelineStageFlags srcstage;
		VkPipelineStageFlags dststage;
		VkAccessFlags srcaccess;
		VkAccessFlags dstaccess;

		RenderPassDependency(uint32_t srcsubpass_, uint32_t dstsubpass_, VkPipelineStageFlags srcstage_, VkPipelineStageFlags dststage_,VkAccessFlags srcaccess_, VkAccessFlags dstaccess_) :
			srcsubpass(srcsubpass_), dstsubpass(dstsubpass_), srcstage(srcstage_),dststage(dststage_), srcaccess(srcaccess_),dstaccess(dstaccess_) {}
	};

	class RenderPassCache
	{
	public:
		RenderPassCache(VkDevice device) : deviceref(device), mRenderPassArray(8) {  };
		~RenderPassCache() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		RenderPassCache(RenderPassCache const&) = delete;
		RenderPassCache(RenderPassCache&&) = delete;
		RenderPassCache& operator=(RenderPassCache const&) = delete;
		RenderPassCache& operator=(RenderPassCache&&) = delete;

		void Cleanup();
		void PrintDebug() const;

		VkRenderPass GetRenderPass(RenderPassAttachment* attachments, uint32_t attachment_count, VkPipelineBindPoint bindpoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
		VkRenderPass GetRenderPass(RenderPassAttachment* attachments, uint32_t attachment_count, RenderPassSubPass* subpasses, uint32_t subpass_count, RenderPassDependency* dependencies, uint32_t dependency_count);

	private:	
		std::vector<VkRenderPass> mRenderPassArray;
		VkDevice deviceref;
	};




}

